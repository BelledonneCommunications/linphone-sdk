/*
	lime.cpp
	Copyright (C) 2017  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define BCTBX_LOG_DOMAIN "lime"
#include <bctoolbox/logging.h>

#include "lime/lime.hpp"

#include "lime_impl.hpp"
#include "bctoolbox/exception.hh"
#include "lime_double_ratchet.hpp"
#include "lime_double_ratchet_protocol.hpp"

using namespace::std;

namespace lime {


	/****************************************************************************/
	/*                                                                          */
	/* Members helpers functions (privates)                                     */
	/*                                                                          */
	/****************************************************************************/

	/****************************************************************************/
	/*                                                                          */
	/* Constructors                                                             */
	/*                                                                          */
	/****************************************************************************/
	/**
	 * @brief Load user constructor
	 *  before calling this constructor, user existence in DB is checked and its Uid retrieved
	 *  just load it into Lime class
	 *
	 * @param[in/out]	localStorage	pointer to DB accessor
	 * @param[in]		userId		user Id(shall be GRUU), stored in the structure
	 * @param[in]		Uid		the DB internal Id for this user, speed up DB operations by holding it in DB
	 * @param[in]		url		URL of the X3DH key server used to publish our keys(retrieved from DB)
	 */
	template <typename Curve>
	Lime<Curve>::Lime(std::unique_ptr<lime::Db> &&localStorage, const std::string &userId, const std::string &url, belle_http_provider_t *http_provider, const long int Uid)
	: m_RNG{bctbx_rng_context_new()}, m_selfDeviceId{userId},
	m_Ik{}, m_Ik_loaded(false),
	m_localStorage(std::move(localStorage)), m_db_Uid{Uid},
	m_http_provider{http_provider}, m_X3DH_Server_URL{url},
	m_DR_sessions_cache{}, m_ongoing_encryption{nullptr}, m_encryption_queue{}
	{ }


	/**
	 * @brief Create user constructor
	 *  Create a user in DB, if already existing, throw exception
	 *
	 * @param[in/out]	localStorage	pointer to DB accessor
	 * @param[in]		userId		user Id(shall be GRUU), stored in the structure
	 * @param[in]		url		URL of the X3DH key server used to publish our keys
	 */
	template <typename Curve>
	Lime<Curve>::Lime(std::unique_ptr<lime::Db> &&localStorage, const std::string &userId, const std::string &url, belle_http_provider_t *http_provider)
	: m_RNG{bctbx_rng_context_new()}, m_selfDeviceId{userId},
	m_Ik{}, m_Ik_loaded(false),
	m_localStorage(std::move(localStorage)), m_db_Uid{0},
	m_http_provider{http_provider}, m_X3DH_Server_URL{url},
	m_DR_sessions_cache{}, m_ongoing_encryption{nullptr}, m_encryption_queue{}
	{
		try {
			create_user();
		} catch (...) { // if createUser throw an exception we must clean ressource allocated C style by constructor
			bctbx_rng_context_free(m_RNG);
			throw;
		}
	}

	template <typename Curve>
	Lime<Curve>::~Lime() {
		bctbx_rng_context_free(m_RNG);
	}

	/****************************************************************************/
	/*                                                                          */
	/* Public API                                                               */
	/*                                                                          */
	/****************************************************************************/
	/**
	 * @brief Publish on X3DH server the user, it is performed just after creation in local storage
	 * this  will, on success, trigger generation and sending of SPk and OPks for our new user
	 *
	 * @param[in]	callback	call when completed
	 */
	template <typename Curve>
	void Lime<Curve>::publish_user(const limeCallback &callback) {
		callbackUserData<Curve> *userData = new callbackUserData<Curve>{this->shared_from_this(), callback, true};
		get_SelfIdentityKey(); // make sure our Ik is loaded in object
		std::vector<uint8_t> X3DHmessage{};
		x3dh_protocol::buildMessage_registerUser<Curve>(X3DHmessage, m_Ik.publicKey());
		postToX3DHServer(userData, X3DHmessage);
	}

	template <typename Curve>
	void Lime<Curve>::delete_user(const limeCallback &callback) {
		// delete user from local Storage
		m_localStorage->delete_LimeUser(m_selfDeviceId);

		// delete user from server
		callbackUserData<Curve> *userData = new callbackUserData<Curve>{this->shared_from_this(), callback};
		std::vector<uint8_t> X3DHmessage{};
		x3dh_protocol::buildMessage_deleteUser<Curve>(X3DHmessage);
		postToX3DHServer(userData, X3DHmessage);
	}

	template <typename Curve>
	void Lime<Curve>::encrypt(std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<recipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback) {
		bctbx_debug("encrypt from %s to %ld recipients", m_selfDeviceId.data(), recipients->size());
		/* Check if we have all the Double Ratcher sessions ready or shall we go for an X3DH */
		std::vector<std::string> missingPeers; /* vector of userId(GRUU) which are requested to perform X3DH before the encryption can occurs */

		/* Create the appropriate recipient infos and fill it with sessions found in cache */
		std::vector<recipientInfos<Curve>> internal_recipients{};
		for (auto &recipient : *recipients) {
			auto sessionElem = m_DR_sessions_cache.find(recipient.deviceId);
			if (sessionElem != m_DR_sessions_cache.end()) { // session is in cache
				internal_recipients.emplace_back(recipient.deviceId, sessionElem->second);
			} else { // session is not in cache, just create it and the session ptr will be a nullptr
				internal_recipients.emplace_back(recipient.deviceId);
			}
		}

		/* try to load all the session that are not in cache */
		std::vector<std::string> missing_devices{};
		cache_DR_sessions(internal_recipients, missing_devices);

		/* If we are still missing session we must ask the X3DH server for key bundles */
		if (missing_devices.size()>0) {
			// create a new callbackUserData, it shall be then deleted in callback, store in all shared_ptr to input/output values needed to call this encrypt function
			auto userData = make_shared<callbackUserData<Curve>>(this->shared_from_this(), callback, recipientUserId, recipients, plainMessage, cipherMessage);
			if (m_ongoing_encryption == nullptr) { // no ongoing asynchronous encryption process it
				m_ongoing_encryption = userData;
			} else { // some one else is expecting X3DH server response, enqueue this request
				m_encryption_queue.push(userData);
				return;
			}
			// retrieve bundles from X3DH server, when they arrive, it will run the X3DH initiation and create the DR sessions
			std::vector<uint8_t> X3DHmessage{};
			x3dh_protocol::buildMessage_getPeerBundles<Curve>(X3DHmessage, missing_devices);
			// use the raw pointer to userData as it is passed to belle-sip C callbacks but store it in current object so it is not destroyed
			postToX3DHServer(userData.get(), X3DHmessage);
			// use the raw pointer to userData as it is passed to belle-sip C callbacks but store it in current object so it is not destroyed
			//X3DH_get_peerBundles(userData.get(), missing_devices);
		} else { // got everyone, encrypt
			encryptMessage(internal_recipients, *plainMessage, *recipientUserId, m_selfDeviceId, *cipherMessage);
			// move cipher headers to the input/output structure
			for (size_t i=0; i<recipients->size(); i++) {
				(*recipients)[i].cipherHeader = std::move(internal_recipients[i].cipherHeader);
			}
			if (callback) callback(lime::callbackReturn::success, "");
			// is there no one in an asynchronous encryption process and do we have something in encryption queue to process
			if (m_ongoing_encryption == nullptr && !m_encryption_queue.empty()) { // may happend when an encryption was queued but session was created by a previously queued encryption request
				auto userData = m_encryption_queue.front();
				m_encryption_queue.pop(); // remove it from queue and do it
				encrypt(userData->recipientUserId, userData->recipients, userData->plainMessage, userData->cipherMessage, userData->callback);
			}
		}
	}

	template <typename Curve>
	bool Lime<Curve>::decrypt(const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &cipherHeader, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) {
		bctbx_debug("decrypt from %s to %s", senderDeviceId.data(), recipientUserId.data());
		// do we have any session (loaded or not) matching that senderDeviceId ?
		auto sessionElem = m_DR_sessions_cache.find(senderDeviceId);
		auto db_sessionIdInCache = 0; // this would be the db_sessionId of the session stored in cache if there is one, no session has the Id 0
		if (sessionElem != m_DR_sessions_cache.end()) { // session is in cache, it is the active one, just give it a try
			db_sessionIdInCache = sessionElem->second->dbSessionId();
			std::vector<std::shared_ptr<DR<Curve>>> DRSessions{1, sessionElem->second}; // copy the session pointer into a vector as the decryot function ask for it
			if (decryptMessage<Curve>(senderDeviceId, m_selfDeviceId, recipientUserId, DRSessions, cipherHeader, cipherMessage, plainMessage) != nullptr) {
				return true; // we manage to decrypt the message with the current active session loaded in cache, nothing else to do
			} else { // remove session from cache
				// session in local storage is not modified, so it's still the active one, it will change status to stale when an other active session will be created
				m_DR_sessions_cache.erase(sessionElem);
			}
		}

		// If we are still here, no session in cache or it didn't decrypt with it. Lookup in localStorage
		std::vector<std::shared_ptr<DR<Curve>>> DRSessions{};
		// load in DRSessions all the session found in cache for this peer device, except the one with id db_sessionIdInCache(is ignored if 0) as we already tried it
		get_DRSessions(senderDeviceId, db_sessionIdInCache, DRSessions);
		auto usedDRSession = decryptMessage<Curve>(senderDeviceId, m_selfDeviceId, recipientUserId, DRSessions, cipherHeader, cipherMessage, plainMessage);
		if (usedDRSession != nullptr) { // we manage to decrypt with a session
			m_DR_sessions_cache[senderDeviceId] = std::move(usedDRSession);
			return true;
		}

		// No luck yet, is this message holds a X3DH header - if no we must give up
		std::vector<uint8_t> X3DH_initMessage{};
		if (!double_ratchet_protocol::parseMessage_get_X3DHinit<Curve>(cipherHeader, X3DH_initMessage)) {
			return false;
		}

		// parse the X3DH init message, get keys from localStorage, compute the shared secrets, create DR_Session and return a shared pointer to it
		std::shared_ptr<DR<Curve>> DRSession{X3DH_init_receiver_session(X3DH_initMessage, senderDeviceId)}; // would just throw an exception in case of failure, let it flow up
		DRSessions.clear();
		DRSessions.push_back(DRSession);

		if (decryptMessage<Curve>(senderDeviceId, m_selfDeviceId, recipientUserId, DRSessions, cipherHeader, cipherMessage, plainMessage) != 0) {
			// we manage to decrypt the message with this session, set it in cache
			m_DR_sessions_cache[senderDeviceId] = std::move(DRSession);
			return true;
		}
		return false;
	}

	/* instantiate Lime for C255 and C448 */
#ifdef EC25519_ENABLED
	template class Lime<C255>;
#endif

#ifdef EC448_ENABLED
	template class Lime<C448>;
#endif

	/****************************************************************************/
	/*                                                                          */
	/* Factory functions and Delete user                                        */
	/*                                                                          */
	/****************************************************************************/
	/**
	 * @brief : Insert user in database and return a pointer to the control class instanciating the appropriate Lime children class
	 m*	Once created a user cannot be modified, insertion of existing userId will raise an exception.
	 *
	 * @param[in]	dbFilename	Path to filename to use
	 * @param[in]	userId		User to create in DB, userId shall be the GRUU
	 * @param[in]	url		URL of X3DH key server to be used to publish our keys
	 * @param[in]	curve		Which curve shall we use for this account, select the implemenation to instanciate when using this user
	 * @param[in]	http_provider	An http provider used to communicate with x3dh key server
	 *
	 * @return a pointer to the LimeGeneric class allowing access to API declared in lime.hpp
	 */
	std::shared_ptr<LimeGeneric> insert_LimeUser(const std::string &dbFilename, const std::string &userId, const std::string &url, const lime::CurveId curve, belle_http_provider *http_provider,
			 const limeCallback &callback) {
		bctbx_message("Create Lime user %s", userId.data());
		/* first check the requested curve is instanciable and return an exception if not */
#ifndef EC25519_ENABLED
		if (curve == lime::CurveId::c25519) {
			throw BCTBX_EXCEPTION << "Lime User creation asking to use Curve 25519 but it's not supported - change lib lime compile option to enable it";
		}
#endif
#ifndef EC448_ENABLED
		if (curve == lime::CurveId::c448) {
			throw BCTBX_EXCEPTION << "Lime User creation asking to use Curve 448 but it's not supported - change lib lime compile option to enable it";
		}
#endif

		/* open DB */
		auto localStorage = std::make_unique<lime::Db>(dbFilename); // create as unique ptr, ownership is then passed to the Lime structure when instanciated

		try {
			//instanciate the correct Lime object
			switch (curve) {
				case lime::CurveId::c25519 :
#ifdef EC25519_ENABLED
				{
					/* constructor will insert user in Db, if already present, raise an exception*/
					auto lime_ptr = std::make_shared<Lime<C255>>(std::move(localStorage), userId, url, http_provider);
					lime_ptr->publish_user(callback);
					return lime_ptr;
				}
#endif
				break;

				case lime::CurveId::c448 :
#ifdef EC448_ENABLED
				{
					auto lime_ptr = std::make_shared<Lime<C448>>(std::move(localStorage), userId, url, http_provider);
					lime_ptr->publish_user(callback);
					return lime_ptr;
				}
#endif
				break;

				case lime::CurveId::unset :
				default: // asking for an unsupported type
					throw BCTBX_EXCEPTION << "Cannot create lime user "<<userId;//<<". Unsupported curve (id <<"static_cast<uint8_t>(curve)") requested";
				break;
			}
		} catch (BctbxException &e) {
			throw; // just forward the exceptions raised by constructor
		}
		return nullptr;
	};

	/**
	 * @brief : Load user from database and return a pointer to the control class instanciating the appropriate Lime children class
	 *	Fail to find the user will raise an exception
	 *
	 * @param[in]	dbFilename	Path to filename to use
	 * @param[in]	userId		User to create in DB, userId shall be the GRUU
	 * @param[in]	http_provider	An http provider used to communicate with x3dh key server
	 *
	 * @return a pointer to the LimeGeneric class allowing access to API declared in lime.hpp
	 */
	std::shared_ptr<LimeGeneric> load_LimeUser(const std::string &dbFilename, const std::string &userId, belle_http_provider *http_provider) {

		/* open DB and load user */
		auto localStorage = std::make_unique<lime::Db>(dbFilename); // create as unique ptr, ownership is then passed to the Lime structure when instanciated
		auto curve = CurveId::unset;
		long int Uid=0;
		std::string x3dh_server_url;

		localStorage->load_LimeUser(userId, Uid, curve, x3dh_server_url); // this one will throw an exception if user is not found, just let it rise
		bctbx_message("Load Lime user %s", userId.data());

		/* check the curve id retrieved from DB is instanciable and return an exception if not */
#ifndef EC25519_ENABLED
		if (curve == lime::CurveId::c25519) {
			throw BCTBX_EXCEPTION << "Lime load User "<<userId<<" requests usage of Curve 25519 but it's not supported - change lib lime compile option to enable it";
		}
#endif
#ifndef EC448_ENABLED
		if (curve == lime::CurveId::c448) {
			throw BCTBX_EXCEPTION << "Lime load User "<<userId<<" requests usage of Curve 448 but it's not supported - change lib lime compile option to enable it";
		}
#endif


		try {
			switch (curve) {
				case lime::CurveId::c25519 :
#ifdef EC25519_ENABLED
					return std::make_shared<Lime<C255>>(std::move(localStorage), userId, x3dh_server_url, http_provider, Uid);
#endif
				break;

				case lime::CurveId::c448 :
#ifdef EC448_ENABLED

					return std::make_shared<Lime<C448>>(std::move(localStorage), userId, x3dh_server_url, http_provider, Uid);
#endif
				break;

				case lime::CurveId::unset :
				default: // asking for an unsupported type
					throw BCTBX_EXCEPTION << "Cannot create load user "<<userId;//<<". Unsupported curve (id <<"static_cast<uint8_t>(curve)") requested";
				break;
			}
		} catch (BctbxException &e) {
			throw;
		}
		return nullptr;
	};
} //namespace lime
