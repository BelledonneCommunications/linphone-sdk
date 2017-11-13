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

using namespace::std;

namespace lime {
	/****************************************************************************/
	/*                                                                          */
	/* Lime Manager API                                                         */
	/*                                                                          */
	/****************************************************************************/
	void LimeManager::create_user(const std::string &userId, const std::string &x3dhServerUrl, const lime::CurveId curve, const limeCallback &callback) {
		auto thiz = this;
		limeCallback managerCreateCallback([thiz, userId, callback](lime::callbackReturn returnCode, std::string errorMessage) {
			// first forward the callback
			callback(returnCode, errorMessage);

			// then check if it went well, if not remove the user from cache(it will trigger destruction of the lime generic object so do it last
			// as it will also destroy the instance of this callback)
			if (returnCode != lime::callbackReturn::success) {
				thiz->m_users_cache.erase(userId);
			}
		});

		m_users_cache.insert({userId, insert_LimeUser(m_db_access, userId, x3dhServerUrl, curve, m_http_provider, managerCreateCallback)});
	}

	void LimeManager::delete_user(const std::string &userId, const limeCallback &callback) {
		auto thiz = this;
		limeCallback managerDeleteCallback([thiz, userId, callback](lime::callbackReturn returnCode, std::string errorMessage) {
			// first forward the callback
			callback(returnCode, errorMessage);

			// then remove the user from cache(it will trigger destruction of the lime generic object so do it last
			// as it will also destroy the instance of this callback)
			thiz->m_users_cache.erase(userId);
		});

		// is the user load? if no we must load it to be able to delete it(generate an exception if user doesn't exists)
		auto userElem = m_users_cache.find(userId);
		std::shared_ptr<LimeGeneric> user;
		if (userElem == m_users_cache.end()) {
			user = load_LimeUser(m_db_access, userId, m_http_provider);
			m_users_cache[userId]=user; // we must load it in cache otherwise object will be destroyed before getting into callback
		} else {
			user = userElem->second;
		}

		user->delete_user(managerDeleteCallback);
	}

	void LimeManager::encrypt(const std::string &localUserId, std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<recipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback) {
		// Load user object
		auto userElem = m_users_cache.find(localUserId);
		std::shared_ptr<LimeGeneric> user;
		if (userElem == m_users_cache.end()) { // not in cache, load it from DB
			user = load_LimeUser(m_db_access, localUserId, m_http_provider);
			m_users_cache[localUserId]=user;
		} else {
			user = userElem->second;
		}

		// call the encryption function
		user->encrypt(recipientUserId, recipients, plainMessage, cipherMessage, callback);
	}

	bool LimeManager::decrypt(const std::string &localUserId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &cipherHeader, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) {
		// Load user object
		auto userElem = m_users_cache.find(localUserId);
		std::shared_ptr<LimeGeneric> user;
		if (userElem == m_users_cache.end()) { // not in cache, load it from DB
			user = load_LimeUser(m_db_access, localUserId, m_http_provider);
			m_users_cache[localUserId]=user;
		} else {
			user = userElem->second;
		}

		// call the decryption function
		return user->decrypt(recipientUserId, senderDeviceId, cipherHeader, cipherMessage, plainMessage);
	}
} // namespace lime
