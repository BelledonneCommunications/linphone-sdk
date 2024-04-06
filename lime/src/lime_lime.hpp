/*
	lime_lime.hpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2017  Belledonne Communications SARL

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
#include <mutex>

namespace lime {
	// forward declarations
	class DR;
	class X3DH;

	/** @brief A pure abstract class defining the API to encrypt/decrypt/manage user and its keys
	 *
	 * underlying implementation is templated to be able to use C25519 or C448, it is selected by the curveId parameter given to insert/load_limeUser function
	 * @note: never instanciate directly a Lime object, always use the Lime Factory function as Lime object MUST be held by a shared pointer
	 */
	class LimeGeneric {

	public:
		// Encrypt/Decrypt
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
		 * 	If the X3DH server can't provide keys for a peer device, its status is set to fail and its DRmessage is empty. Other devices get their encrypted message
		 * 	If no peer device could get encrypted for all of them are missing keys on the X3DH server, the callback will still be called with success exit status
		 *
		 * @note all parameters are shared pointers as the process being asynchronous, the ownership will be taken internally exempting caller to manage the buffers.
		 *
		 * @param[in]		recipientUserId		the Id of intended recipient, shall be a sip:uri of user or conference, is used as associated data to ensure no-one can mess with intended recipient
		 * @param[in,out]	recipients		a list of RecipientData holding:
		 * 						- the recipient device Id(GRUU)
		 * 						- an empty buffer to store the DRmessage which must then be routed to that recipient
		 * 						- the peer Status. If peerStatus is set to fail, this entry is ignored otherwise the peerStatus is set by the encrypt, see ::PeerDeviceStatus definition for details
		 * @param[in]		plainMessage		a buffer holding the message to encrypt, can be text or data.
		 * @param[in]		encryptionPolicy	select how to manage the encryption: direct use of Double Ratchet message or encrypt in the cipher message and use the DR message to share the cipher message key
		 * @param[out]		cipherMessage		points to the buffer to store the encrypted message which must be routed to all recipients(if one is produced, depends on encryption policy)
		 * @param[in]		callback		This operation contact the X3DH server and is thus asynchronous, when server responds,
		 * 					this callback will be called giving the exit status and an error message in case of failure.
		 * 					It is advised to capture a copy of cipherMessage and recipients shared_ptr in this callback so they can access
		 * 					the output of encryption as it won't be part of the callback parameters.
		*/
		virtual void encrypt(std::shared_ptr<const std::vector<uint8_t>> recipientUserId, std::shared_ptr<std::vector<RecipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback) = 0;

		/**
		 * @brief Decrypt the given message
		 *
		 * @param[in]	recipientUserId	the Id of intended recipient, shall be a sip:uri of user or conference, is used as associated data to ensure no-one can mess with intended recipient
		 * 				it is not necessarily the sip:uri base of the GRUU as this could be a message from alice first device intended to bob being decrypted on alice second device
		 * @param[in]	senderDeviceId	the device Id (GRUU) of the message sender
		 * @param[in]	DRmessage	the Double Ratchet message targeted to current device
		 * @param[in]	cipherMessage	part of cipher routed to all recipient devices(it may be actually empty depending on sender encryption policy and message characteristics)
		 * @param[out]	plainMessage	the output buffer
		 *
		 * @return	true if the decryption is successfull, false otherwise
		*/
		virtual lime::PeerDeviceStatus decrypt(const std::vector<uint8_t> &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) = 0;

		/**
		 * Get the lock on the Lime object ressources (mostly DR session cache and encryption queue
		 * This is a unique lock, release it by destroying the object
		 */
		virtual std::unique_lock<std::mutex> lock(void) = 0;

		/**
		 * @brief Check if we have queued encryption to process, if yes, do it
		 *
		 */
		virtual void processEncryptionQueue(void) = 0;

		/**
		 * @brief delete an entry (if found) from the DR session cache
		 *
		 * @param[in]	deviceId	the key in the DR session cache
		 */
		virtual void DRcache_delete(const std::string &deviceId) = 0;

		/**
		 * @brief insert an entry in the DR session cache
		 * if an entry with the same key already exists, do nothing
		 *
		 * @param[in]	deviceId	the key in the DR session cache
		 * @param[in]	DRsession	the DR session to insert
		 */
		virtual	void DRcache_insert(const std::string &deviceId, std::shared_ptr<DR> DRsession) = 0;

		/**
		 * @brief accessor to the internal X3DH engine
		 *
		 * @return the internal X3DH engine
		 */
		virtual std::shared_ptr<X3DH> get_X3DH(void) = 0;

		// User management
		/**
		 * @brief Publish on X3DH server the user, it is performed just after creation in local storage
		 * this  will, on success, trigger generation and sending of SPk and OPks for our new user
		 *
		 * @param[in]	callback		call when completed
		 * @param[in]	OPkInitialBatchSize	Number of OPks in the first batch uploaded to X3DH server
		*/
		virtual void publish_user(const limeCallback &callback, const uint16_t OPkInitialBatchSize) = 0;

		/**
		 * @brief Delete user from local Storage and from X3DH server
		 *
		 * @param[in]	callback		call when completed
		 */
		virtual void delete_user(const limeCallback &callback) = 0;

		/**
		 * @brief Purge cached sessions for a given peer Device (used when a peer device is being deleted)
		 *
		 * @param[in]	peerDeviceId	The peer device to remove from cache
		 */
		virtual void delete_peerDevice(const std::string &peerDeviceId) = 0;



		// User keys management
		/**
		 * @brief Check if the current SPk needs to be updated, if yes, generate a new one and publish it on server
		 *
		 * @param[in] callback 	Called with success or failure when operation is completed.
		*/
		virtual void update_SPk(const limeCallback &callback) = 0;

		/**
		 * @brief check if we shall upload more OPks on X3DH server
		 * - ask server four our keys (returns the count and all their Ids)
		 * - check if it's under the low limit, if yes, generate a batch of keys and upload them
		 *
		 * @param[in]	callback 		Called with success or failure when operation is completed.
		 * @param[in]	OPkServerLowLimit	If server holds less OPk than this limit, generate and upload a batch of OPks
		 * @param[in]	OPkBatchSize		Number of OPks in a batch uploaded to server
		*/
		virtual void update_OPk(const limeCallback &callback, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize) = 0;

		/**
		 * @brief Retrieve self public Identity key
		 *
		 * @param[out]	Ik	the public EdDSA formatted Identity key
		*/
		virtual void get_Ik(std::vector<uint8_t> &Ik) = 0;

		/**
		 * @brief Set the X3DH key server URL for this identified user
		 *
		 * @param[in]	x3dhServerUrl		The complete url(including port) of the X3DH key server
		 */
		virtual void set_x3dhServerUrl(const std::string &x3dhServerUrl) = 0;

		/**
		 * @brief Get the X3DH key server URL for this identified user
		 *
		 * @return The complete url(including port) of the X3DH key server
		 */
		virtual std::string get_x3dhServerUrl() = 0;

		/**
		 * @brief Stale all sessions between localDeviceId and peerDevice.
		 * If peerDevice keep using this session to encrypt and we decrypt with success, the session will be reactivated
		 * but to encrypt a message to this peerDevice, a new session will be created.
		 * If no session is active between the given device, this call has no effect
		 *
		 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
		 */
		virtual void stale_sessions(const std::string &peerDeviceId) = 0;

		virtual ~LimeGeneric() {};
	};

	/* Lime Factory functions : return a pointer to the implementation using the specified elliptic curve. Two functions: one for creation, one for loading from local storage */

	std::shared_ptr<LimeGeneric> insert_LimeUser(std::shared_ptr<lime::Db> localStorage, const std::string &deviceId, const std::string &url, const lime::CurveId curve, const uint16_t OPkInitialBatchSize,
			const limeX3DHServerPostData &X3DH_post_data, const limeCallback &callback);

	std::shared_ptr<LimeGeneric> load_LimeUser(std::shared_ptr<lime::Db> localStorage, const std::string &deviceId, const limeX3DHServerPostData &X3DH_post_data, const bool allStatus=false);

}
#endif // lime_lime_hpp
