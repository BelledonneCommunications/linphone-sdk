/*
	lime_lime.hpp
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

#ifndef lime_lime_hpp
#define lime_lime_hpp

#include <memory> // unique_ptr
#include <unordered_map>
#include <vector>
#include "belle-sip/belle-sip.h"

namespace lime {

	/* A pure abstract class, implementation used is set by curveId parameter given to insert/load_limeUser function */
	/* NOTE: never instanciate directly a Lime object, always use the Lime Factory function as Lime object MUST be held by a shared pointer */
	class LimeGeneric {

	public:
		/* Encrypt/Decrypt */
		virtual void encrypt(std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<recipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback) = 0;
		virtual bool decrypt(const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &cipherHeader, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) = 0;
		virtual void publish_user(const limeCallback &callback) = 0;
		virtual void delete_user(const limeCallback &callback) = 0;
		virtual ~LimeGeneric() {};
	};

	/* Lime Factory functions : return a pointer to the implementation using the specified elliptic curve. Two functions: one for creation, one for loading from local storage */

	/**
	 * @brief : Insert user in database and return a pointer to the control class instanciating the appropriate Lime children class
	 *	Once created a user cannot be modified, insertion of existing deviceId will raise an exception.
	 *
	 * @param[in]	dbFilename			Path to filename to use
	 * @param[in]	deviceId			User to create in DB, deviceId shall be the GRUU
	 * @param[in]	url				URL of X3DH key server to be used to publish our keys
	 * @param[in]	curve				Which curve shall we use for this account, select the implemenation to instanciate when using this user
	 * @param[in]	http_provider			An http provider used to communicate with x3dh key server
	 * @param[in]	user_authentication_callback	To complete user authentication on server: must provide user credentials
	 * @param[in]	callback			To provide caller the operation result
	 *
	 * @return a pointer to the LimeGeneric class allowing access to API declared in lime.hpp
	 */
	std::shared_ptr<LimeGeneric> insert_LimeUser(const std::string &dbFilename, const std::string &deviceId, const std::string &url, const lime::CurveId curve, belle_http_provider *http_provider,
			  const userAuthenticateCallback &user_authentication_callback, const limeCallback &callback);

	/**
	 * @brief Load a local user from database
	 *
	 * @param[in]	dbFilename			path to the database to be used
	 * @param[in]	deviceId			a unique identifier to a local user, if not already present in base it will be inserted. Recommended value: device's GRUU
	 * @param[in]	http_provider			An http provider used to access X3DH server, no scheduling is done on it internally
	 * @param[in]	user_authentication_callback	To complete user authentication on server: must provide user credentials
	 *
	 * @return	a unique pointer to the object to be used by this user for any Lime operations
	 */
	std::shared_ptr<LimeGeneric> load_LimeUser(const std::string &dbFilename, const std::string &deviceId, belle_http_provider_t *http_provider, const userAuthenticateCallback &user_authentication_callback);

}
#endif // lime_lime_hpp
