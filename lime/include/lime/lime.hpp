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
#include <functional>
#include <string>
#include <mutex>

namespace lime {

	/** Identifies the elliptic curve used in lime, the values assigned are used in localStorage and X3DH server
	 * so do not modify it or we'll loose sync with existing DB and X3DH server
	 */
	enum class CurveId : uint8_t {
		unset=0, /**< used as default to detected incorrect behavior */
		c25519=1, /**< Curve 25519 */
		c448=2 /**< Curve 448-goldilocks */
	};

	/** Manage the encryption policy : how is the user's plaintext encrypted */
	enum class EncryptionPolicy {
		DRMessage, /**< the plaintext input is encrypted inside the Double Ratchet message (each recipient get a different encryption): not optimal for messages with numerous recipient */
		cipherMessage, /**< the plaintext input is encrypted with a random key and this random key is encrypted to each participant inside the Double Ratchet message(for a single recipient the overhead is 48 bytes) */
		optimizeUploadSize, /**< optimize upload size: encrypt in DR message if plaintext is short enougth to beat the overhead introduced by cipher message scheme, otherwise use cipher message. Selection is made on upload size only. This is the default policy used */
		optimizeGlobalBandwidth /**< optimize bandwith usage: encrypt in DR message if plaintext is short enougth to beat the overhead introduced by cipher message scheme, otherwise use cipher message. Selection is made on uploadand download (from server to recipients) sizes added. */
	};

	/**
	 * A peer device status returned after encrypt, decrypt or when directly asking for the peer device status to spot new devices and give information on our trust on this device
	 * The values explicitely mapped to specific integers(untrusted, trusted, unsafe) are stored in local storage as integer
	 * Do not modify the mapping or we will loose backward compatibility with existing databases
	 */
	enum class PeerDeviceStatus : uint8_t {
		untrusted=0, /**< we know this device but do not trust it, that information shall be displayed to the end user, a colour code shall be enough */
		trusted=1, /**< this peer device already got its public identity key validated, that information shall be displayed to the end user too */
		unsafe=2, /**< this status is a helper for the library user. It is used only by the peerDeviceStatus accessor functions */
		fail, /**< when returned by decrypt : we could not decrypt the incoming message\n
			when returned by encrypt in the peerStatus: we could not encrypt to this recipient(probably because it does not published keys on the X3DH server) */
		unknown /**< when returned after encryption or decryption, means it is the first time we communicate with this device (and thus create a DR session with it)\n
			   when returned by a get_peerDeviceStatus: this device is not in localStorage */
	};

	/** @brief The encrypt function input/output data structure
	 *
	 * give a recipient GRUU and get it back with the header which must be sent to recipient with the cipher text
	 */
	struct RecipientData {
		const std::string deviceId; /**< input: recipient deviceId (shall be GRUU) */
		lime::PeerDeviceStatus peerStatus; /**< input: if set to fail, this entry will be ignored by the encrypt function\n
						output: after encrypt calls back, it will hold the status of this peer device:\n
						     - unknown: first interaction with this device)
						     - untrusted: device is kown but we never confirmed its identity public key
						     - trusted: we already confirmed this device identity public key
						     - fail: we could not encrypt for this device, probably because it never published its keys on the X3DH server */
		std::vector<uint8_t> DRmessage; /**< output: after encrypt calls back, it will hold the Double Ratchet message targeted to the specified recipient. */
		/**
		 * recipient data are built giving a recipient id
		 * @param[in] deviceId	the recipient device Id (its GRUU)
		 */
		RecipientData(const std::string &deviceId) : deviceId{deviceId}, peerStatus{lime::PeerDeviceStatus::unknown}, DRmessage{} {};
	};

	/** what a Lime callback could possibly say */
	enum class CallbackReturn : uint8_t {
		success, /**< operation completed successfully */
		fail /**< operation failed, we shall have an explanation string too */
	};
	/** @brief Callback use to give a status on asynchronous operation
	 *
	 *	it returns a code and may return a string (could actually be empty) to detail what's happening
	 *  	callback is used on every operation possibly involving a connection to X3DH server: create_user, delete_user, encrypt and update
	 *  @param[in]	status	success or fail
	 *  @param[in]	message	in case of failure, an explanation, it may be empty
	 */
	using limeCallback = std::function<void(const lime::CallbackReturn status, const std::string message)>;

	/* X3DH server communication : these functions prototypes are used to post data and get response from/to the X3DH server */
	/**
	 * @brief Get the response from server. The external service providing secure communication to the X3DH server shall forward to lime library the server's response
	 *
	 * @param[in]	responseCode	Lime expects communication with server to be over HTTPS, this shall be the response code. Lime expects 200 for successfull response from server.
	 * 				Any other response code is treated as an error and response ignored(but it is still usefull to forward it in order to perform internal cleaning)
	 * @param[in]	responseBody	The actual response from X3DH server
	 */
	using limeX3DHServerResponseProcess = std::function<void(int responseCode, const std::vector<uint8_t> &responseBody)>;

	/**
	 * @brief Post a message to the X3DH server
	 *
	 * @param[in]	url			X3DH server's URL
	 * @param[in]	from			User identification on X3DH server (which shall challenge for password digest, this is not however part of lime)
	 * @param[in]	message			The message to post to the X3DH server
	 * @param[in]	responseProcess		Function to be called with server's response
	 */
	using limeX3DHServerPostData = std::function<void(const std::string &url, const std::string &from, const std::vector<uint8_t> &message, const limeX3DHServerResponseProcess &reponseProcess)>;

	/* Forward declare the class managing one lime user*/
	class LimeGeneric;

	/** @brief Manage several Lime objects(one is needed for each local user).
	 *
	 * 	LimeManager is mostly a cache of Lime users, any command get as first parameter the device Id (Lime manage devices only, the link user(sip:uri)<->device(GRUU) is provided by upper level)
	 *	All interactions should take place through the LimeManager object, any endpoint shall have only one LimeManager object instanciated
	 */

	class LimeManager {
		private :
			std::unordered_map<std::string, std::shared_ptr<LimeGeneric>> m_users_cache; // cache of already opened Lime Session, identified by user Id (GRUU)
			std::mutex m_users_mutex; // m_users_cache mutex
			std::string m_db_access; // DB access information forwarded to SOCI to correctly access database
			std::shared_ptr<std::recursive_mutex> m_db_mutex; // database access mutex
			limeX3DHServerPostData m_X3DH_post_data; // send data to the X3DH key server
			void load_user(std::shared_ptr<LimeGeneric> &user, const std::string &localDeviceId, const bool allStatus=false); // helper function, get from m_users_cache of local Storage the requested Lime object

		public :

			/**
			 * @brief Create a user in local database and publish it on the given X3DH server
			 *
			 * 	The Lime user shall be created at the same time the account is created on the device, this function shall not be called again, attempt to re-create an already existing user will fail.
			 * 	A user is identified by its deviceId (shall be the GRUU) and must at creation select a base Elliptic curve to use, this setting cannot be changed later
			 * 	A user is published on an X3DH key server who must run using the same elliptic curve selected for this user (creation will fail otherwise), the server url cannot be changed later
			 *
			 * @param[in]	localDeviceId		Identify the local user account, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
			 * @param[in]	x3dhServerUrl		The complete url(including port) of the X3DH key server. It must connect using HTTPS. Example: https://sip5.linphone.org:25519
			 * @param[in]	curve			Choice of elliptic curve used as base for ECDH and EdDSA operation involved. Can be CurveId::c25519 or CurveId::c448.
			 * @param[in]	OPkInitialBatchSize	Number of OPks in the first batch uploaded to X3DH server
			 * @param[in]	callback		This operation contact the X3DH server and is thus asynchronous, when server responds,
			 * 					this callback will be called giving the exit status and an error message in case of failure
			 * @note
			 * The OPkInitialBatchSize is optionnal, if not used, set to defaults defined in lime::settings
			 * (not done with param default value as the lime::settings shall not be available in public include)
			 */
			void create_user(const std::string &localDeviceId, const std::string &x3dhServerUrl, const lime::CurveId curve, const uint16_t OPkInitialBatchSize, const limeCallback &callback);
			/**
			 * @overload void create_user(const std::string &localDeviceId, const std::string &x3dhServerUrl, const lime::CurveId curve, const limeCallback &callback)
			 */
			void create_user(const std::string &localDeviceId, const std::string &x3dhServerUrl, const lime::CurveId curve, const limeCallback &callback);

			/**
			 * @brief Delete a user from local database and from the X3DH server
			 *
			 * if specified localDeviceId is not found in local Storage, throw an exception
			 *
			 * @param[in]	localDeviceId	Identify the local user acount to use, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
			 * @param[in]	callback	This operation contact the X3DH server and is thus asynchronous, when server responds,
			 * 				this callback will be called giving the exit status and an error message in case of failure
			 */
			void delete_user(const std::string &localDeviceId, const limeCallback &callback);

			/**
			 * @brief Check if a user is present and active in local storage
			 *
			 * @param[in]	localDeviceId	used to identify which local account looking up, shall be the GRUU
			 *
			 * @return true if the user is active in the local storage, false otherwise
			 */
			bool is_user(const std::string &localDeviceId);

			/**
			 * @brief Encrypt a buffer (text or file) for a given list of recipient devices
			 *
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
			 * 	If the X3DH server can't provide keys for a peer device, its status is set to fail and its DRmessage is empty. Other devices get their encrypted message
			 * 	If no peer device could get encrypted for all of them are missing keys on the X3DH server, the callback will be called with fail exit status
			 *
			 * @note nearly all parameters are shared pointers as the process being asynchronous, the ownership will be taken internally exempting caller to manage the buffers.
			 *
			 * @param[in]		localDeviceId	used to identify which local acount to use and also as the identified source of the message, shall be the GRUU
			 * @param[in]		recipientUserId	the Id of intended recipient, shall be a sip:uri of user or conference, is used as associated data to ensure no-one can mess with intended recipient
			 * @param[in,out]	recipients	a list of RecipientData holding:
			 * 					- the recipient device Id(GRUU)
			 * 					- an empty buffer to store the DRmessage which must then be routed to that recipient
			 * 					- the peer Status. If peerStatus is set to fail, this entry is ignored otherwise the peerStatus is set by the encrypt, see ::PeerDeviceStatus definition for details
			 * @param[in]		plainMessage	a buffer holding the message to encrypt, can be text or data.
			 * @param[out]		cipherMessage	points to the buffer to store the encrypted message which must be routed to all recipients(if one is produced, depends on encryption policy)
			 * @param[in]		callback	Performing encryption may involve the X3DH server and is thus asynchronous, when the operation is completed,
			 * 					this callback will be called giving the exit status and an error message in case of failure.
			 * 					It is advised to capture a copy of cipherMessage and recipients shared_ptr in this callback so they can access
			 * 					the output of encryption as it won't be part of the callback parameters.
			 * @param[in]		encryptionPolicy	select how to manage the encryption: direct use of Double Ratchet message or encrypt in the cipher message and use the DR message to share the cipher message key
			 * 						default is optimized upload size mode.
			 */
			void encrypt(const std::string &localDeviceId, std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<RecipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback, lime::EncryptionPolicy encryptionPolicy=lime::EncryptionPolicy::optimizeUploadSize);

			/**
			 * @brief Decrypt the given message
			 *
			 * if specified localDeviceId is not found in local Storage, throw an exception
			 *
			 * @param[in]		localDeviceId	used to identify which local acount to use and also as the recipient device ID of the message, shall be the GRUU
			 * @param[in]		recipientUserId	the Id of intended recipient, shall be a sip:uri of user or conference, is used as associated data to ensure no-one can mess with intended recipient
			 * 					it is not necessarily the sip:uri base of the GRUU as this could be a message from alice first device intended to bob being decrypted on alice second device
			 * @param[in]		senderDeviceId	Identify sender Device. This field shall be extracted from signaling data in transport protocol, is used to rebuild the authenticated data associated to the encrypted message
			 * @param[in]		DRmessage	Double Ratchet message targeted to current device
			 * @param[in]		cipherMessage	when present (depends on encryption policy) holds a common part of the encrypted message. Can be ignored or set to empty vector if not present in the incoming message.
			 * @param[out]		plainMessage	the output buffer
			 *
			 * @return	fail if we cannot decrypt the message, unknown when it is the first message we ever receive from the sender device, untrusted for known but untrusted sender device, or trusted if it is
			 */
			lime::PeerDeviceStatus decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage);
			/**
			 * @overload decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, std::vector<uint8_t> &plainMessage)
			 * convenience form to be called when no cipher message is received
			 */
			lime::PeerDeviceStatus decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, std::vector<uint8_t> &plainMessage);

			/**
			 * @brief Update: shall be called once a day at least, performs checks, updates and cleaning operations
			 *
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
			 * @note
			 * The last two parameters are optional, if not used, set to defaults defined in lime::settings
			 * (not done with param default value as the lime::settings shall not be available in public include)
			 */
			void update(const limeCallback &callback, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize);
			/**
			 * @overload void update(const limeCallback &callback)
			 */
			void update(const limeCallback &callback);

			/**
			 * @brief retrieve self Identity Key, an EdDSA formatted public key
			 *
			 * if specified localDeviceId is not found in local Storage, throw an exception
			 *
			 *
			 * @param[in]	localDeviceId	used to identify which local account we're dealing with, shall be the GRUU
			 * @param[out]	Ik		the EdDSA public identity key, formatted as in RFC8032
			 */
			void get_selfIdentityKey(const std::string &localDeviceId, std::vector<uint8_t> &Ik);

			/**
			 * @brief set the peer device status flag in local storage: unsafe, trusted or untrusted.
			 *
			 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
			 * @param[in]	Ik		the EdDSA peer public identity key, formatted as in RFC8032
			 * @param[in]	status		value of flag to set: accepted values are trusted, untrusted, unsafe
			 *
			 * throw an exception if given key doesn't match the one present in local storage
			 * if the status flag value is unexpected (not one of trusted, untrusted, unsafe), ignore the call
			 * if the status flag is unsafe or untrusted, ignore the value of Ik and call the version of this function without it
			 *
			 * if peer Device is not present in local storage and status is trusted or unsafe, it is added, if status is untrusted, it is just ignored
			 *
			 * General algorithm followed by the set_peerDeviceStatus functions
			 * - Status is valid? (not one of trusted, untrusted, unsafe)? No: return
			 * - status is trusted
			 *       - We have Ik? -> No: return
			 *       - Device is already in storage but Ik differs from the given one : exception
			 *       - Insert/update in local storage
			 * - status is untrusted
			 *       - Ik is ignored
			 *       - Device already in storage? No: return
			 *       - Device already in storage but current status is unsafe? Yes: return
			 *       - update in local storage
			 * -status is unsafe
			 *       - ignore Ik
			 *       - insert/update the status. If inserted, insert an invalid Ik
			 */
			void set_peerDeviceStatus(const std::string &peerDeviceId, const std::vector<uint8_t> &Ik, lime::PeerDeviceStatus status);

			/**
			 * @brief set the peer device status flag in local storage: unsafe or untrusted.
			 *
			 * This variation allows to set a peer Device status to unsafe or untrusted only whithout providing its identity key Ik
			 *
			 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
			 * @param[in]	status		value of flag to set: accepted values are untrusted or unsafe
			 *
			 * if the status flag value is unexpected (not one of untrusted, unsafe), ignore the call
			 *
			 * if peer Device is not present in local storage, it is inserted if status is unsafe and call is ignored if status is untrusted
			 * if the status is untrusted but the current status in local storage is unsafe, ignore the call
			 * Any call to the other form of the function with a status to unsafe or untrusted is rerouted to this function
			 */
			void set_peerDeviceStatus(const std::string &peerDeviceId, lime::PeerDeviceStatus status);

			/**
			 * @brief get the status of a peer device: unknown, untrusted, trusted, unsafe
			 *
			 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
			 *
			 * @return unknown if the device is not in localStorage, untrusted, trusted or unsafe according to the stored value of peer device status flag otherwise
			 */
			lime::PeerDeviceStatus get_peerDeviceStatus(const std::string &peerDeviceId);

			/**
			 * @brief checks if a device iD exists in the local users
			 *
			 * @param[in]	deviceId	The device Id to check
			 *
			 * @return true if the device Id exists in the local users table, false otherwise
			 */
			bool is_localUser(const std::string &deviceId);

			/**
			 * @brief delete a peerDevice from local storage
			 *
			 * @param[in]	peerDeviceId	The device Id to be removed from local storage, shall be its GRUU
			 *
			 * Call is silently ignored if the device is not found in local storage
			 */
			void delete_peerDevice(const std::string &peerDeviceId);

			/**
			 * @brief Stale all sessions between localDeviceId and peerDevice.
			 * If peerDevice keep using this session to encrypt and we decrypt with success, the session will be reactivated
			 * but to encrypt a message to this peerDevice, a new session will be created.
			 * If no session is active between the given device, this call has no effect
			 *
			 * @param[in]	localDeviceId	Identify the local user account, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
			 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
			 */
			void stale_sessions(const std::string &localDeviceId, const std::string &peerDeviceId);

			/**
			 * @brief Set the X3DH key server URL for this identified user
			 *
			 * @param[in]	localDeviceId		Identify the local user account, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
			 * @param[in]	x3dhServerUrl		The complete url(including port) of the X3DH key server. It must connect using HTTPS. Example: https://sip5.linphone.org:25519
			 *
			 * Throw an exception if the user is unknow or inactive
			 */
			void set_x3dhServerUrl(const std::string &localDeviceId, const std::string &x3dhServerUrl);

			/**
			 * @brief Get the X3DH key server URL for this identified user
			 *
			 * @param[in]	localDeviceId		Identify the local user account, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
			 *
			 * @return The complete url(including port) of the X3DH key server.
			 *
			 * Throw an exception if the user is unknow or inactive
			 */
			std::string get_x3dhServerUrl(const std::string &localDeviceId);

			LimeManager() = delete; // no manager without Database and http provider
			LimeManager(const LimeManager&) = delete; // no copy constructor
			LimeManager operator=(const LimeManager &) = delete; // nor copy operator

			/**
			 * @brief Lime Manager constructor
			 *
			 * @param[in]	db_access	string used to access DB: can be filename for sqlite3 or access params for mysql, directly forwarded to SOCI session opening
			 * @param[in]	X3DH_post_data	A function to send data to the X3DH server, parameters includes a callback to transfer back the server response
			 * @param[in]	db_mutex	a mutex used to lock database access. Is optionnal: if not given, the manager will produce one internally
			 */
			LimeManager(const std::string &db_access, const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr<std::recursive_mutex> db_mutex);
			/**
			 * @overload LimeManager(const std::string &db_access, const limeX3DHServerPostData &X3DH_post_data)
			 */
			LimeManager(const std::string &db_access, const limeX3DHServerPostData &X3DH_post_data);

			~LimeManager() = default;
	};
} //namespace lime
#endif /* lime_hpp */
