/*
	lime.hpp
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
#ifndef lime_hpp
#define lime_hpp

#include <memory> // unique_ptr
#include <unordered_map>
#include <vector>
#include "belle-sip/belle-sip.h"

namespace lime {

	/* This enum identifies the elliptic curve used in lime, the values assigned are used in localStorage and X3DH server
	 * so do not modify it or we'll loose sync with existing DB and X3DH server */
	enum class CurveId : uint8_t {unset=0, c25519=1, c448=2};

	/* Struct used to manage recipient list for encrypt function input: give a recipient GRUU and get it back with the header which must be sent to recipient with the cipher text*/
	struct recipientData {
		std::string deviceId; // recipient deviceId (shall be GRUU)
		std::vector<uint8_t> cipherHeader; // after encrypt calls back, it will hold the header targeted to the specified recipient. This header may contain an X3DH init message.
		recipientData(std::string deviceId) : deviceId{deviceId}, cipherHeader{} {};
	};

	/* Enum of what a Lime callback could possibly say */
	enum class callbackReturn : uint8_t {success, fail};
	// a callback function must return a code and may return a string(could actually be empty) to detail what's happening
	// callback is used on every operation possibly involving a connection to X3DH server: create_user, delete_user, encrypt
	using limeCallback = std::function<void(lime::callbackReturn, std::string)>;


	/* Forward declare the class managing one lime user*/
	class LimeGeneric;

	/* class to manage and cache Lime objects(its one per user), then adressed using their userId (GRUU) */
	class LimeManager {
		private :
			std::unordered_map<std::string, std::shared_ptr<LimeGeneric>> m_users_cache; // cache of already opened Lime Session, identified by user Id (GRUU)
			std::string m_db_access; // DB access information forwarded to SOCI to correctly access database
			belle_http_provider_t *m_http_provider;
		public :
			void create_user(const std::string &userId, const std::string &x3dhServerUrl, const lime::CurveId curve, const limeCallback &callback);
			void delete_user(const std::string &userId, const limeCallback &callback);
			void encrypt(const std::string &localUserId, std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<recipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback);
			bool decrypt(const std::string &localUserId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &cipherHeader, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage);
			LimeManager() = delete; // no manager without Database and http provider
			LimeManager(const LimeManager&) = delete; // no copy constructor
			LimeManager operator=(const LimeManager &) = delete; // nor copy operator
			/**
			 * @brief Lime Manager constructor
			 *
			 * @param[in]	db_access	string used to access DB: can be filename for sqlite3 or access params for mysql, directly forwarded to SOCI session opening
			 * @param[in]	http_provider	An http provider used to access X3DH server, no scheduling is done on it internally
			 */
			LimeManager(const std::string &db_access, belle_http_provider_t *http_provider)
				: m_users_cache{}, m_db_access{db_access}, m_http_provider{http_provider} {};

			~LimeManager() = default;
	};
} //namespace lime
#endif /* lime_hpp */
