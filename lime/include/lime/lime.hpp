/*
	lime.hpp
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
#ifndef lime_hpp
#define lime_hpp

#include <memory> //smart ptrs
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
		bool identityVerified; // after encrypt calls back, it will hold the status of this peer device: identity verified or not
		std::vector<uint8_t> cipherHeader; // after encrypt calls back, it will hold the header targeted to the specified recipient. This header may contain an X3DH init message.
		recipientData(std::string deviceId) : deviceId{deviceId}, identityVerified{false}, cipherHeader{} {};
	};

	/* Enum of what a Lime callback could possibly say */
	enum class callbackReturn : uint8_t {success, fail};
	// a callback function must return a code and may return a string(could actually be empty) to detail what's happening
	// callback is used on every operation possibly involving a connection to X3DH server: create_user, delete_user, encrypt
	using limeCallback = std::function<void(lime::callbackReturn, std::string)>;
	// the user authenticate callback will pass a sip_auth_event to be completed with user's password
	using userAuthenticateCallback = std::function<void(belle_sip_auth_event_t *)>;


	/* Forward declare the class managing one lime user*/
	class LimeGeneric;

	/* Manage Lime objects(it's one per user), then adressed using their device Id (GRUU) */
	class LimeManager {
		private :
			std::unordered_map<std::string, std::shared_ptr<LimeGeneric>> m_users_cache; // cache of already opened Lime Session, identified by user Id (GRUU)
			std::string m_db_access; // DB access information forwarded to SOCI to correctly access database
			belle_http_provider_t *m_http_provider; // used to access the X3DH key server
			userAuthenticateCallback m_user_auth; // called to complete user authentication on server
			void load_user(std::shared_ptr<LimeGeneric> &user, const std::string &localDeviceId); // helper function, get from m_users_cache of local Storage the requested Lime object

		public :
			/* LimeManager is mostly a cache of Lime users, any command get as first parameter the device Id (Lime manage devices only, the link user(sip:uri)<->device(GRUU) is provided by upper level) */

			/**
			 * @brief Create a user in local database and publish it on the given X3DH server
			 * 	The Lime user shall be created at the same time the account is created on the device, this function shall not be called again, attempt to re-create an already existing user will fail.
			 * 	A user is identified by its deviceId(shall be the GRUU) and must at creation select a base Elliptic curve to use, this setting cannot be changed later
			 * 	A user is published on an X3DH key server who must run using the same elliptic curve selected for this user (creation will fail otherwise), the server url cannot be changed later
			 *
			 * @param[in]	localDeviceId		Identify the local user acount to use, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
			 * @param[in]	x3dhServerUrl		The complete url(including port) of the X3DH key server. It must connect using HTTPS. Example: https://sip5.linphone.org:25519
			 * @param[in]	curve			Choice of elliptic curve to use as base for ECDH and EdDSA operation involved. Can be CurveId::c25519 or CurveId::c448.
			 * @param[in]	initialOPkBatchSize	Number of OPks in the first batch uploaded to X3DH server
			 * @param[in]	callback		This operation contact the X3DH server and is thus asynchronous, when server responds,
			 * 					this callback will be called giving the exit status and an error message in case of failure
			 * The initialOPkBatchSize is optionnal, if not used, set to defaults defined in lime::settings
			 * (not done with param default value as the lime::settings shall not be available in public include)
			 */
			void create_user(const std::string &localDeviceId, const std::string &x3dhServerUrl, const lime::CurveId curve, const uint16_t initialOPkBatchSize, const limeCallback &callback);
			void create_user(const std::string &localDeviceId, const std::string &x3dhServerUrl, const lime::CurveId curve, const limeCallback &callback);

			/**
			 * @brief Delete a user from local database and from the X3DH server
			 * if specified localDeviceId is not found in local Storage, throw an exception
			 *
			 * @param[in]	localDeviceId	Identify the local user acount to use, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
			 * @param[in]	callback	This operation contact the X3DH server and is thus asynchronous, when server responds,
			 * 				this callback will be called giving the exit status and an error message in case of failure
			 */
			void delete_user(const std::string &localDeviceId, const limeCallback &callback);

			/**
			 * @brief Encrypt a buffer(text or file) for a given list of recipient devices
			 * if specified localDeviceId is not found in local Storage, throw an exception
			 *
			 * 	Clarification on recipients:
			 *
			 * 	recipients information needed are a list of the device Id and one userId. The device Id shall be their GRUU while the userId is a sip:uri.
			 *
			 * 	recipient User Id is used to identify the actual intended recipient. Example: alice have two devices and is signed up on a conference having
			 * 	bob and claire as other members. The recipientUserId will be the conference sip:uri and device list will include:
			 * 		 - alice other device
			 * 		 - bob devices
			 * 		 - claire devices
			 * 	If Alice write to Bob only, the recipientUserId will be bob sip:uri and recipient devices list :
			 * 		 - alice other device
			 * 		 - bob devices
			 *
			 * 	In all cases, the identified source of the message will be the localDeviceId
			 *
			 * 	Note: nearly all parameters are shared pointers as the process being asynchronous, the ownership will be taken internally exempting caller to manage the buffers.
			 *
			 * @param[in]		localDeviceId	used to identify which local acount to use and also as the identified source of the message, shall be the GRUU
			 * @param[in]		recipientUserId	the Id of intended recipient, shall be a sip:uri of user or conference, is used as associated data to ensure no-one can mess with intended recipient
			 * @param[in/out]	recipients	a list of recipientData holding: the recipient device Id(GRUU) and an empty buffer to store the cipherHeader which must then be routed to that recipient
			 * @param[in]		plainMessage	a buffer holding the message to encrypt, can be text or data.
			 * @param[out]		cipherMessage	points to the buffer to store the encrypted message which must be routed to all recipients
			 * @param[in]		callback	This operation contact the X3DH server and is thus asynchronous, when server responds,
			 * 					this callback will be called giving the exit status and an error message in case of failure.
			 * 					It is advised to capture a copy of cipherMessage and recipients shared_ptr in this callback so they can access
			 * 					the output of encryption as it won't be part of the callback parameters.
			 */
			void encrypt(const std::string &localDeviceId, std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<recipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback);

			/**
			 * @brief Decrypt the given message
			 * if specified localDeviceId is not found in local Storage, throw an exception
			 *
			 * @param[in]		localDeviceId	used to identify which local acount to use and also as the recipient device ID of the message, shall be the GRUU
			 * @param[in]		recipientUserId	the Id of intended recipient, shall be a sip:uri of user or conference, is used as associated data to ensure no-one can mess with intended recipient
			 * 					it is not necessarily the sip:uri base of the GRUU as this could be a message from alice first device intended to bob being decrypted on alice second device
			 * @param[in]		cipherHeader	the part of cipher which is targeted to current device
			 * @param[in]		cipherMessage	part of cipher routed to all recipient devices
			 * @param[out]		plainMessage	the output buffer
			 *
			 * @return	true if the decryption is successfull, false otherwise
			 */
			bool decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &cipherHeader, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage);

			/**
			 * @brief Update: shall be called once a day at least, performs checks, updates and cleaning operations
			 *  - check if we shall update a new SPk to X3DH server(SPk lifetime is set in settings)
			 *  - check if we need to upload OPks to X3DH server
			 *  - remove old SPks, clean double ratchet sessions (remove staled, clean their stored keys for skipped messages)
			 *
			 *  Is performed for all users founds in local storage
			 *
			 * @param[in]	callback		This operation may contact the X3DH server and is thus asynchronous, when server responds,
			 * 					this callback will be called giving the exit status and an error message in case of failure.
			 * @param[in]	OPkServerLowLimit	If server holds less OPk than this limit, generate and upload a batch of OPks
			 * @param[in]	OPkBatchSize		Number of OPks in a batch uploaded to server
			 *
			 * The last two parameters are optionnal, if not used, set to defaults defined in lime::settings
			 * (not done with param default value as the lime::settings shall not be available in public include)
			 */
			void update(const limeCallback &callback);
			void update(const limeCallback &callback, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize);

			/**
			 * @brief retrieve self Identity Key, an EdDSA formatted public key
			 * if specified localDeviceId is not found in local Storage, throw an exception
			 *
			 *
			 * @param[in]	localDeviceId	used to identify which local account we're dealing with, shall be the GRUU
			 * @param[out]	Ik		the EdDSA public identity key, formatted as in RFC8032
			 */
			void get_selfIdentityKey(const std::string &localDeviceId, std::vector<uint8_t> &Ik);

			/**
			 * @brief set the identity verified flag for peer device
			 *
			 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
			 * @param[in]	Ik		the EdDSA peer public identity key, formatted as in RFC8032
			 * @param[in]	status		value of flag to set
			 *
			 * throw an exception if given key doesn't match the one present in local storage
			 * if peer Device is not present in local storage and status is true, it is added, if status is false, it is just ignored
			 */
			void set_peerIdentityVerifiedStatus(const std::string &peerDeviceId, const std::vector<uint8_t> &Ik, bool status);

			/**
			 * @brief get the identity verified flag for peer device
			 *
			 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
			 *
			 * @return the stored Verified status, false if peer Device is not present in local Storage
			 */
			bool get_peerIdentityVerifiedStatus(const std::string &peerDeviceId);

			LimeManager() = delete; // no manager without Database and http provider
			LimeManager(const LimeManager&) = delete; // no copy constructor
			LimeManager operator=(const LimeManager &) = delete; // nor copy operator

			/**
			 * @brief Lime Manager constructor
			 *
			 * @param[in]	db_access			string used to access DB: can be filename for sqlite3 or access params for mysql, directly forwarded to SOCI session opening
			 * @param[in]	http_provider			An http provider used to access X3DH server, no scheduling is done on it internally
			 * @param[in]	user_authentication_callback	To complete user authentication on server: must provide user credentials
			 */
			LimeManager(const std::string &db_access, belle_http_provider_t *http_provider, const userAuthenticateCallback &user_authentication_callback)
				: m_users_cache{}, m_db_access{db_access}, m_http_provider{http_provider}, m_user_auth{user_authentication_callback} {};

			~LimeManager() = default;
	};
} //namespace lime
#endif /* lime_hpp */
