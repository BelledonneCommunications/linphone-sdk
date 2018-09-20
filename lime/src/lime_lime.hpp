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

namespace lime {

	/** A pure abstract class, implementation used is set by curveId parameter given to insert/load_limeUser function
	* @note: never instanciate directly a Lime object, always use the Lime Factory function as Lime object MUST be held by a shared pointer */
	class LimeGeneric {

	public:
		// Encrypt/Decrypt
		/**
		 * @brief Encrypt a buffer(text or file) for a given list of recipient devices
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
		 * 	Note: all parameters are shared pointers as the process being asynchronous, the ownership will be taken internally exempting caller to manage the buffers.
		 *
		 * @param[in]		recipientUserId		the Id of intended recipient, shall be a sip:uri of user or conference, is used as associated data to ensure no-one can mess with intended recipient
		 * @param[in,out]	recipients		a list of RecipientData holding: the recipient device Id(GRUU) and an empty buffer to store the DRmessage which must then be routed to that recipient
		 * @param[in]		plainMessage		a buffer holding the message to encrypt, can be text or data.
		 * @param[in]		encryptionPolicy	select how to manage the encryption: direct use of Double Ratchet message or encrypt in the cipher message and use the DR message to share the cipher message key
		 * @param[out]		cipherMessage		points to the buffer to store the encrypted message which must be routed to all recipients(if one is produced, depends on encryption policy)
		 * @param[in]		callback		This operation contact the X3DH server and is thus asynchronous, when server responds,
		 * 					this callback will be called giving the exit status and an error message in case of failure.
		 * 					It is advised to capture a copy of cipherMessage and recipients shared_ptr in this callback so they can access
		 * 					the output of encryption as it won't be part of the callback parameters.
		*/
		virtual void encrypt(std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<RecipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback) = 0;

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
		virtual lime::PeerDeviceStatus decrypt(const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) = 0;



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
	 * @param[in]	initialOPkBatchSize		Number of OPks in the first batch uploaded to X3DH server
	 * @param[in]	X3DH_post_data			A function to communicate with x3dh key server
	 * @param[in]	callback			To provide caller the operation result
	 *
	 * @return a pointer to the LimeGeneric class allowing access to API declared in lime.hpp
	 */
	std::shared_ptr<LimeGeneric> insert_LimeUser(const std::string &dbFilename, const std::string &deviceId, const std::string &url, const lime::CurveId curve, const uint16_t OPkInitialBatchSize,
			const limeX3DHServerPostData &X3DH_post_data, const limeCallback &callback);

	/**
	 * @brief Load a local user from database
	 *
	 * @param[in]	dbFilename			path to the database to be used
	 * @param[in]	deviceId			a unique identifier to a local user, if not already present in base it will be inserted. Recommended value: device's GRUU
	 * @param[in]	X3DH_post_data			A function to communicate with x3dh key server
	 *
	 * @return	a unique pointer to the object to be used by this user for any Lime operations
	 */
	std::shared_ptr<LimeGeneric> load_LimeUser(const std::string &dbFilename, const std::string &deviceId, const limeX3DHServerPostData &X3DH_post_data);

}
#endif // lime_lime_hpp
