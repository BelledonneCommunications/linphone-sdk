
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

using namespace::std;

namespace lime {
	void LimeManager::load_user(std::shared_ptr<LimeGeneric> &user, const std::string &localDeviceId) {
		// Load user object
		auto userElem = m_users_cache.find(localDeviceId);
		if (userElem == m_users_cache.end()) { // not in cache, load it from DB
			user = load_LimeUser(m_db_access, localDeviceId, m_X3DH_post_data);
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
				auto localStorage = std::unique_ptr<lime::Db>(new lime::Db(thiz->m_db_access));
				localStorage->delete_LimeUser(localDeviceId);
				thiz->m_users_cache.erase(localDeviceId);
			}
		});

		m_users_cache.insert({localDeviceId, insert_LimeUser(m_db_access, localDeviceId, x3dhServerUrl, curve, OPkInitialBatchSize, m_X3DH_post_data, managerCreateCallback)});
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
		LimeManager::load_user(user, localDeviceId);

		user->delete_user(managerDeleteCallback);
	}

	void LimeManager::encrypt(const std::string &localDeviceId, std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<RecipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback, const lime::EncryptionPolicy encryptionPolicy) {
		// Load user object
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		// call the encryption function
		user->encrypt(recipientUserId, recipients, plainMessage, encryptionPolicy, cipherMessage, callback);
	}

	lime::PeerDeviceStatus LimeManager::decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) {
		// Load user object
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		// call the decryption function
		return user->decrypt(recipientUserId, senderDeviceId, DRmessage, cipherMessage, plainMessage);
	}

	// convenience definition, have a decrypt without cipherMessage input for the case we don't have it(DR message encryption policy)
	// just use an create an empty cipherMessage to be able to call Lime::decrypt which needs the cipherMessage even if empty for code simplicity
	lime::PeerDeviceStatus LimeManager::decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, std::vector<uint8_t> &plainMessage) {
		// Load user object
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		const std::vector<uint8_t> emptyCipherMessage(0);

		// call the decryption function
		return user->decrypt(recipientUserId, senderDeviceId, DRmessage, emptyCipherMessage, plainMessage);
	}


	/* This version use default settings */
	void LimeManager::update(const limeCallback &callback) {
		update(callback, lime::settings::OPk_serverLowLimit, lime::settings::OPk_batchSize);
	}
	void LimeManager::update(const limeCallback &callback, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize) {
		// open local DB
		auto localStorage = std::unique_ptr<lime::Db>(new lime::Db(m_db_access));

		/* DR sessions and old stale SPk cleaning */
		localStorage->clean_DRSessions();
		localStorage->clean_SPk();

		// get all users from localStorage
		std::vector<std::string> deviceIds{};
		localStorage->get_allLocalDevices(deviceIds);

		//This counter will trace number of callbacks, to trace how many operation did end.
		// we expect two callback per local user account: one for update SPk, one for get OPk number on server
		auto callbackCount = make_shared<size_t>(deviceIds.size()*2);
		auto globalReturnCode = make_shared<lime::CallbackReturn>(lime::CallbackReturn::success);

		// this callback will get all callbacks from update OPk and SPk on all users, when everyone is done, call the callback given to LimeManager::update
		limeCallback managerUpdateCallback([callbackCount, globalReturnCode, callback](lime::CallbackReturn returnCode, std::string errorMessage) {
			(*callbackCount)--;
			if (returnCode == lime::CallbackReturn::fail) {
				*globalReturnCode = lime::CallbackReturn::fail; // if one fail, return fail at the end of it
			}

			if (*callbackCount == 0) {
				if (callback) callback(*globalReturnCode, "");
			}
		});


		// for each user
		for (auto deviceId : deviceIds) {
			LIME_LOGI<<"Lime update user "<<deviceId;
			//load user
			std::shared_ptr<LimeGeneric> user;
			LimeManager::load_user(user, deviceId);

			// send a request to X3DH server to check how many OPk are left on server, upload more if needed
			user->update_OPk(managerUpdateCallback, OPkServerLowLimit, OPkBatchSize);

			// update the SPk(if needed)
			user->update_SPk(managerUpdateCallback);
		}
	}

	void LimeManager::get_selfIdentityKey(const std::string &localDeviceId, std::vector<uint8_t> &Ik) {
		std::shared_ptr<LimeGeneric> user;
		LimeManager::load_user(user, localDeviceId);

		user->get_Ik(Ik);
	}

	void LimeManager::set_peerDeviceStatus(const std::string &peerDeviceId, const std::vector<uint8_t> &Ik, lime::PeerDeviceStatus status) {
		// open local DB
		auto localStorage = std::unique_ptr<lime::Db>(new lime::Db(m_db_access));

		localStorage->set_peerDeviceStatus(peerDeviceId, Ik, status);
	}

	void LimeManager::set_peerDeviceStatus(const std::string &peerDeviceId, lime::PeerDeviceStatus status) {
		// open local DB
		auto localStorage = std::unique_ptr<lime::Db>(new lime::Db(m_db_access));

		localStorage->set_peerDeviceStatus(peerDeviceId, status);
	}

	lime::PeerDeviceStatus LimeManager::get_peerDeviceStatus(const std::string &peerDeviceId) {
		// open local DB
		auto localStorage = std::unique_ptr<lime::Db>(new lime::Db(m_db_access));

		return localStorage->get_peerDeviceStatus(peerDeviceId);
	}

	void LimeManager::delete_peerDevice(const std::string &peerDeviceId) {
		// loop on all local users in cache to destroy any cached session linked to that user
		for (auto userElem : m_users_cache) {
			userElem.second->delete_peerDevice(peerDeviceId);
		}

		// open local DB
		auto localStorage = std::unique_ptr<lime::Db>(new lime::Db(m_db_access));

		localStorage->delete_peerDevice(peerDeviceId);
	}


} // namespace lime
