/*
	lime_manager.cpp
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
#include "lime_lime.hpp"
#include "lime_localStorage.hpp"

using namespace::std;

namespace lime {
	/****************************************************************************/
	/*                                                                          */
	/* Lime Manager API                                                         */
	/*                                                                          */
	/****************************************************************************/
	void LimeManager::create_user(const std::string &localDeviceId, const std::string &x3dhServerUrl, const lime::CurveId curve, const limeCallback &callback) {
		auto thiz = this;
		limeCallback managerCreateCallback([thiz, localDeviceId, callback](lime::callbackReturn returnCode, std::string errorMessage) {
			// first forward the callback
			callback(returnCode, errorMessage);

			// then check if it went well, if not delete the user from localDB
			if (returnCode != lime::callbackReturn::success) {
				auto localStorage = std::unique_ptr<lime::Db>(new lime::Db(thiz->m_db_access));
				localStorage->delete_LimeUser(localDeviceId);
				thiz->m_users_cache.erase(localDeviceId);
			}
		});

		m_users_cache.insert({localDeviceId, insert_LimeUser(m_db_access, localDeviceId, x3dhServerUrl, curve, m_http_provider, m_user_auth, managerCreateCallback)});
	}

	void LimeManager::delete_user(const std::string &localDeviceId, const limeCallback &callback) {
		auto thiz = this;
		limeCallback managerDeleteCallback([thiz, localDeviceId, callback](lime::callbackReturn returnCode, std::string errorMessage) {
			// first forward the callback
			callback(returnCode, errorMessage);

			// then remove the user from cache(it will trigger destruction of the lime generic object so do it last
			// as it will also destroy the instance of this callback)
			thiz->m_users_cache.erase(localDeviceId);
		});

		// is the user load? if no we must load it to be able to delete it(generate an exception if user doesn't exists)
		auto userElem = m_users_cache.find(localDeviceId);
		std::shared_ptr<LimeGeneric> user;
		if (userElem == m_users_cache.end()) {
			user = load_LimeUser(m_db_access, localDeviceId, m_http_provider, m_user_auth);
			m_users_cache[localDeviceId]=user; // we must load it in cache otherwise object will be destroyed before getting into callback
		} else {
			user = userElem->second;
		}

		user->delete_user(managerDeleteCallback);
	}

	void LimeManager::encrypt(const std::string &localDeviceId, std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<recipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback) {
		// Load user object
		auto userElem = m_users_cache.find(localDeviceId);
		std::shared_ptr<LimeGeneric> user;
		if (userElem == m_users_cache.end()) { // not in cache, load it from DB
			user = load_LimeUser(m_db_access, localDeviceId, m_http_provider, m_user_auth);
			m_users_cache[localDeviceId]=user;
		} else {
			user = userElem->second;
		}

		// call the encryption function
		user->encrypt(recipientUserId, recipients, plainMessage, cipherMessage, callback);
	}

	bool LimeManager::decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &cipherHeader, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) {
		// Load user object
		auto userElem = m_users_cache.find(localDeviceId);
		std::shared_ptr<LimeGeneric> user;
		if (userElem == m_users_cache.end()) { // not in cache, load it from DB
			user = load_LimeUser(m_db_access, localDeviceId, m_http_provider, m_user_auth);
			m_users_cache[localDeviceId]=user;
		} else {
			user = userElem->second;
		}

		// call the decryption function
		return user->decrypt(recipientUserId, senderDeviceId, cipherHeader, cipherMessage, plainMessage);
	}

	void LimeManager::update(const limeCallback &callback) {
		// open local DB
		auto localStorage = std::unique_ptr<lime::Db>(new lime::Db(m_db_access));

		/* DR sessions and old stale SPk cleaning */
		localStorage->clean_DRSessions();
		localStorage->clean_SPk();

		// get all users from localStorage
		std::vector<std::string> deviceIds{};
		localStorage->get_allLocalDevices(deviceIds);

		//This counter will trace number of callbacks, to trace how many operation did end,
		auto callbackCounts = make_shared<size_t>(0);
		// we expect two callback per local user account: one for update SPk, one for get OPk number on server
		size_t expectedCallbacks = deviceIds.size();
		auto globalReturnCode = make_shared<lime::callbackReturn>(lime::callbackReturn::success);

		limeCallback managerUpdateCallback([callbackCounts, expectedCallbacks, globalReturnCode, callback](lime::callbackReturn returnCode, std::string errorMessage) {
			(*callbackCounts)++;
			if (returnCode == lime::callbackReturn::fail) {
				*globalReturnCode = lime::callbackReturn::fail; // if one fail, return fail at the end of it
			}

			if (*callbackCounts == expectedCallbacks) {
				if (callback) callback(*globalReturnCode, "");
			}
		});


		// for each user
		for (auto deviceId : deviceIds) {
			BCTBX_SLOGI<<"Lime update user "<<deviceId;
			//load user
			auto userElem = m_users_cache.find(deviceId);
			std::shared_ptr<LimeGeneric> user;
			if (userElem == m_users_cache.end()) { // not in cache, load it from DB
				user = load_LimeUser(m_db_access, deviceId, m_http_provider, m_user_auth);
				m_users_cache[deviceId]=user;
			} else {
				user = userElem->second;
			}

			// send a request to X3DH server to check how many OPk are left on server


			// update the SPk(if needed)
			user->update_SPk(managerUpdateCallback);
		}


		/* SPk check */

		//if (callback) callback(lime::callbackReturn::success, "");
	}
} // namespace lime
