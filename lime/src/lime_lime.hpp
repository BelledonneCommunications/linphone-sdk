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
		virtual void delete_user(const limeCallback &callback) = 0;
		virtual ~LimeGeneric() {};
	};

	/* Lime Factory functions : return a pointer to the implementation using the specified elliptic curve. Two functions: one for creation, one for loading from local storage */

	/**
	 * @brief Create a local lime user and insert all its needed data in a DB, it will trigger identity key creation and communication of it, SPKs and OPKs to key server
	 *
	 * @param[in]	dbFilename	path to the database to be used
	 * @param[in]	userId		a unique identifier to a local user, if not already present in base it will be inserted. Recommended value: device's GRUU
	 * @param[in]	keyServer	URL of X3DH key server(WARNING : matching between elliptic curve usage of all clients on the same server is responsability of clients)
	 * @param[in]	curve		select which Elliptic curve to base X3DH and Double ratchet on: Curve25519 or Curve448,
	 *				this is set once at user creation and can't be modified, it must reflect key server preference.
	 * @return	a unique pointer to the object to be used by this user for any Lime operations
	 */
	std::shared_ptr<LimeGeneric> insert_LimeUser(const std::string &dbFilename, const std::string &userId, const std::string &url, const lime::CurveId curve,
							belle_http_provider_t *http_provider, const limeCallback &callback);

	/**
	 * @brief Load a local user from database
	 *
	 * @param[in]	dbFilename	path to the database to be used
	 * @param[in]	userId		a unique identifier to a local user, if not already present in base it will be inserted. Recommended value: device's GRUU
	 *
	 * @return	a unique pointer to the object to be used by this user for any Lime operations
	 */
	std::shared_ptr<LimeGeneric> load_LimeUser(const std::string &dbFilename, const std::string &userId, belle_http_provider_t *http_provider);

}
#endif // lime_lime_hpp
