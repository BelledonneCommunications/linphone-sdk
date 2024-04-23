
/*
	lime_manager.cpp
	@author Johan Pascal
	@copyright	Copyright (C) 2017  Belledonne Communications SARL

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

#include "lime_log.hpp"
#include "lime/lime.hpp"
#include "lime_lime.hpp"
#include "lime_localStorage.hpp"
#include "lime_settings.hpp"
#include <mutex>
#include "bctoolbox/exception.hh"

using namespace::std;

namespace lime {
	LimeManager::LimeManager(const std::string &db_access, const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr<std::recursive_mutex> db_mutex)
		: m_users_cache{}, m_localStorage{std::make_shared<lime::Db>(db_access, db_mutex)}, m_X3DH_post_data{X3DH_post_data} { }

	// When no mutex is provided for database access, create one
	LimeManager::LimeManager(const std::string &db_access, const limeX3DHServerPostData &X3DH_post_data)
		: m_users_cache{}, m_localStorage{std::make_shared<lime::Db>(db_access, std::make_shared<std::recursive_mutex>())}, m_X3DH_post_data{X3DH_post_data} { }

	void LimeManager::load_user(std::shared_ptr<LimeGeneric> &user, const std::string &localDeviceId, const bool allStatus) {
		// get the Lime manager lock
		std::lock_guard<std::mutex> lock(m_users_mutex);
		// Load user object
		auto userElem = m_users_cache.find(localDeviceId);
		if (userElem == m_users_cache.end()) { // not in cache, load it from DB
			user = load_LimeUser(m_localStorage, localDeviceId, m_X3DH_post_data, allStatus);
			m_users_cache[localDeviceId]=user;
		} else {
			user = userElem->second;
		}
	}

	/****************************************************************************/
	/*                                                                          */
	/* Lime Manager API                                                         */
	/*                                                                          */
	/****************************************************************************/
	void LimeManager::create_user(const std::string &localDeviceId, const std::string &x3dhServerUrl, const lime::CurveId curve, const limeCallback &callback) {
		create_user(localDeviceId, x3dhServerUrl, curve, lime::settings::OPk_initialBatchSize, callback);
	}
	void LimeManager::create_user(const std::string &localDeviceId, const std::string &x3dhServerUrl, const lime::CurveId curve, const uint16_t OPkInitialBatchSize, const limeCallback &callback) {
		auto thiz = this;
		limeCallback managerCreateCallback([thiz, localDeviceId, callback](lime::CallbackReturn returnCode, std::string errorMessage) {
			// first forward the callback
			callback(returnCode, errorMessage);

			// then check if it went well, if not delete the user from localDB
			if (returnCode != lime::CallbackReturn::success) {
				thiz->m_localStorage->delete_LimeUser(localDeviceId);

				// Failure can occur only on X3DH server response(local failure generate an exception so we would never
				// arrive in this callback)), so the lock acquired by create_user has already expired when we arrive here
				std::lock_guard<std::mutex> lock(thiz->m_users_mutex);
				thiz->m_users_cache.erase(localDeviceId);
			}
		});

		std::lock_guard<std::mutex> lock(m_users_mutex);
		m_users_cache.insert({localDeviceId, insert_LimeUser(m_localStorage, localDeviceId, x3dhServerUrl, curve, OPkInitialBatchSize, m_X3DH_post_data, managerCreateCallback)});
	}

	void LimeManager::delete_user(const std::string &localDeviceId, const limeCallback &callback) {
		auto thiz = this;
		limeCallback managerDeleteCallback([thiz, localDeviceId, callback](lime::CallbackReturn returnCode, std::string errorMessage) {
			// first forward the callback
			callback(returnCode, errorMessage);

			// then remove the user from cache(it will trigger destruction of the lime generic object so do it last
			// as it will also destroy the instance of this callback)
			thiz->m_users_cache.erase(localDeviceId);
		});

		// Load user object
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId, true); // load user even if inactive as we are deleting it anyway

		user->delete_user(managerDeleteCallback);
	}

	bool LimeManager::is_user(const std::string &localDeviceId) {
		try {
			// Load user object
			std::shared_ptr<LimeGeneric> user;
			LimeManager::load_user(user, localDeviceId);

			return true; // If we are able to load the user, it means it exists
		} catch (BctbxException const &) { // we get an exception if the user is not found
			// swallow it and return false
			return false;
		}

	}

	void LimeManager::encrypt(const std::string &localDeviceId, std::shared_ptr<const std::vector<uint8_t>> associatedData, std::shared_ptr<std::vector<RecipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback, const lime::EncryptionPolicy encryptionPolicy) {
		// Load user object
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		// call the encryption function
		user->encrypt(associatedData, recipients, plainMessage, encryptionPolicy, cipherMessage, callback);
	}
	void LimeManager::encrypt(const std::string &localDeviceId, std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<RecipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback, const lime::EncryptionPolicy encryptionPolicy) {
		auto associatedData = std::make_shared<const vector<uint8_t>>(recipientUserId->cbegin(), recipientUserId->cend());
		encrypt(localDeviceId, associatedData, recipients, plainMessage, cipherMessage, callback, encryptionPolicy);
	}

	lime::PeerDeviceStatus LimeManager::decrypt(const std::string &localDeviceId, const std::vector<uint8_t> &associatedData, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) {
		// Load user object
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		// call the decryption function
		return user->decrypt(associatedData, senderDeviceId, DRmessage, cipherMessage, plainMessage);
	}

	// convenience definition, have a decrypt without cipherMessage input for the case we don't have it(DR message encryption policy)
	// just create an empty cipherMessage to be able to call Lime::decrypt which needs the cipherMessage even if empty for code simplicity
	lime::PeerDeviceStatus LimeManager::decrypt(const std::string &localDeviceId, const std::vector<uint8_t> &associatedData, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, std::vector<uint8_t> &plainMessage) {
		// Load user object
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		const std::vector<uint8_t> emptyCipherMessage(0);

		// call the decryption function
		return user->decrypt(associatedData, senderDeviceId, DRmessage, emptyCipherMessage, plainMessage);
	}
	lime::PeerDeviceStatus LimeManager::decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) {
		std::vector<uint8_t> associatedData(recipientUserId.cbegin(), recipientUserId.cend());
		return decrypt(localDeviceId, associatedData, senderDeviceId, DRmessage, cipherMessage, plainMessage);
	}
	lime::PeerDeviceStatus LimeManager::decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, std::vector<uint8_t> &plainMessage) {
		std::vector<uint8_t> associatedData(recipientUserId.cbegin(), recipientUserId.cend());
		return decrypt(localDeviceId, associatedData, senderDeviceId, DRmessage, plainMessage);
	}


	/* This version use default settings */
	void LimeManager::update(const std::string &localDeviceId, const limeCallback &callback) {
		update(localDeviceId, callback, lime::settings::OPk_serverLowLimit, lime::settings::OPk_batchSize);
	}
	void LimeManager::update(const std::string &localDeviceId, const limeCallback &callback, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize) {
		// Check if the last update was performed more than OPk_updatePeriod seconds ago
		if (!m_localStorage->is_updateRequested(localDeviceId)) {
			if (callback) callback(lime::CallbackReturn::success, "No update needed");
			return;
		}

		LIME_LOGI<<"Update user "<<localDeviceId;

		/* DR sessions and old stale SPk cleaning - This cleaning is performed for all local users as it is easier this way */
		m_localStorage->clean_DRSessions();
		m_localStorage->clean_SPk();

		// Load user object
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		// we expect two callback: one for update SPk, one for get OPk number on server
		auto callbackCount = make_shared<size_t>(2);
		auto globalReturnCode = make_shared<lime::CallbackReturn>(lime::CallbackReturn::success);
		auto localStorage = m_localStorage;

		// this callback will get all callbacks from update OPk and SPk on all users, when everyone is done, call the callback given to LimeManager::update
		limeCallback managerUpdateCallback([callbackCount, globalReturnCode, callback, localStorage, localDeviceId](lime::CallbackReturn returnCode, std::string errorMessage) {
			(*callbackCount)--;
			if (returnCode == lime::CallbackReturn::fail) {
				*globalReturnCode = lime::CallbackReturn::fail; // if one fail, return fail at the end of it
			}

			if (*callbackCount == 0) {
				// update the timestamp
				localStorage->set_updateTs(localDeviceId);
				if (callback) callback(*globalReturnCode, "");
			}
		});

		// send a request to X3DH server to check how many OPk are left on server, upload more if needed
		user->update_OPk(managerUpdateCallback, OPkServerLowLimit, OPkBatchSize);

		// update the SPk(if needed)
		user->update_SPk(managerUpdateCallback);
	}

	void LimeManager::get_selfIdentityKey(const std::string &localDeviceId, std::vector<uint8_t> &Ik) {
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		user->get_Ik(Ik);
	}

	void LimeManager::set_peerDeviceStatus(const std::string &peerDeviceId, const std::vector<uint8_t> &Ik, lime::PeerDeviceStatus status) {
		m_localStorage->set_peerDeviceStatus(peerDeviceId, Ik, status);
	}

	void LimeManager::set_peerDeviceStatus(const std::string &peerDeviceId, lime::PeerDeviceStatus status) {
		m_localStorage->set_peerDeviceStatus(peerDeviceId, status);
	}

	lime::PeerDeviceStatus LimeManager::get_peerDeviceStatus(const std::string &peerDeviceId) {
		return m_localStorage->get_peerDeviceStatus(peerDeviceId);
	}

	lime::PeerDeviceStatus LimeManager::get_peerDeviceStatus(const std::list<std::string> &peerDeviceIds) {
		return m_localStorage->get_peerDeviceStatus(peerDeviceIds);
	}

	bool LimeManager::is_localUser(const std::string &deviceId) {
		return m_localStorage->is_localUser(deviceId);
	}

	void LimeManager::delete_peerDevice(const std::string &peerDeviceId) {
		std::lock_guard<std::mutex> lock(m_users_mutex);
		// loop on all local users in cache to destroy any cached session linked to that user
		for (auto userElem : m_users_cache) {
			userElem.second->delete_peerDevice(peerDeviceId);
		}

		m_localStorage->delete_peerDevice(peerDeviceId);
	}

	void LimeManager::stale_sessions(const std::string &localDeviceId, const std::string &peerDeviceId) {
		// load user (generate an exception if not found, let it flow up)
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		// Delete session from cache - if any
		user->delete_peerDevice(peerDeviceId);

		// stale session in DB
		user->stale_sessions(peerDeviceId);
	}

	void LimeManager::set_x3dhServerUrl(const std::string &localDeviceId, const std::string &x3dhServerUrl) {
		// load user (generate an exception if not found, let it flow up)
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		user->set_x3dhServerUrl(x3dhServerUrl);

	}

	std::string LimeManager::get_x3dhServerUrl(const std::string &localDeviceId) {
		// load user (generate an exception if not found, let it flow up)
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		return user->get_x3dhServerUrl();
	}

	bool lime_is_PQ_available(void) {
#ifdef HAVE_BCTBXPQ
		return true;
#else
		return false;
#endif
	};

} // namespace lime
