/*
	LimeManager.java
	@author Johan Pascal
	@copyright	Copyright (C) 2019  Belledonne Communications SARL

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
package org.linphone.lime;

public class LimeManager {

	public LimeManager(String db_access) {
	    initialize(db_access);
	}

	private long peer;
	protected native void initialize(String db_access);
	protected native void finalize() throws Throwable;

	// Native functions involving enumerated paremeters not public
	// Enumeration translation is done on java side
	private native int n_decrypt(String localDeviceId, String recipientUserId, String senderDeviceId, byte[] DRmessage, byte[] cipherMessage, LimeOutputBuffer plainMessage);
	private native void n_encrypt(String localDeviceId, String recipientUserId, RecipientData[] recipients, byte[] plainMessage, LimeOutputBuffer cipherMessage, LimeStatusCallback statusObj, int encryptionPolicy);

	private native void n_create_user(String localDeviceId, String serverURL, int curveId, int OPkInitialBatchSize, LimeStatusCallback statusObj);
	private native void n_update(LimeStatusCallback statusObj, int OPkServerLowLimit, int OPkBatchSize);


	/* default value for OPkInitialBatchSize is 100 */
	public void create_user(String localDeviceId, String serverURL, LimeCurveId curveId, LimeStatusCallback statusObj) {
		this.n_create_user(localDeviceId, serverURL, curveId.getNative(), 100, statusObj);
	}
	public void create_user(String localDeviceId, String serverURL, LimeCurveId curveId, int OPkInitialBatchSize, LimeStatusCallback statusObj) {
		this.n_create_user(localDeviceId, serverURL, curveId.getNative(), OPkInitialBatchSize, statusObj);
	}

	public native void delete_user(String localDeviceId, LimeStatusCallback statusObj);

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
	 * 	If no peer device could get encrypted for all of them are missing keys on the X3DH server, the callback will still be called with success exit status
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
	 * 						default is optimized output size mode.
	 */
	public void encrypt(String localDeviceId, String recipientUserId, RecipientData[] recipients, byte[] plainMessage, LimeOutputBuffer cipherMessage, LimeStatusCallback statusObj, LimeEncryptionPolicy encryptionPolicy) {
		this.n_encrypt(localDeviceId, recipientUserId, recipients, plainMessage, cipherMessage, statusObj, encryptionPolicy.getNative());
	}
	/**
	* @overload encrypt(String localDeviceId, String recipientUserId, RecipientData[] recipients, byte[] plainMessage, LimeOutputBuffer cipherMessage, LimeStatusCallback statusObj
	* convenience form using LimeEncryptionPolicy.OPTIMIZEUPLOADSIZE as default policy
	*/
	public void encrypt(String localDeviceId, String recipientUserId, RecipientData[] recipients, byte[] plainMessage, LimeOutputBuffer cipherMessage, LimeStatusCallback statusObj) {
		this.n_encrypt(localDeviceId, recipientUserId, recipients, plainMessage, cipherMessage, statusObj, LimeEncryptionPolicy.OPTIMIZEUPLOADSIZE.getNative());
	}

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
	 * @return	LimePeerDeviceStatus.FAIL if we cannot decrypt the message, LimePeerDeviceStatus.UNKNOWN when it is the first message we ever receive from the sender device, LimePeerDeviceStatus.UNTRUSTED for known but untrusted sender device, or LimePeerDeviceStatus.TRUSTED if it is
	 */
	public LimePeerDeviceStatus decrypt(String localDeviceId, String recipientUserId, String senderDeviceId, byte[] DRmessage, byte[] cipherMessage, LimeOutputBuffer plainMessage) {
		int native_status = this.n_decrypt(localDeviceId, recipientUserId, senderDeviceId, DRmessage, cipherMessage, plainMessage);
		return LimePeerDeviceStatus.fromNative(native_status);
	}
	/**
	* @overload decrypt(String localDeviceId, String recipientUserId, String senderDeviceId, byte[] DRmessage, LimeOutputBuffer plainMessage)
	* convenience form to be called when no cipher message is received
	*/
	public LimePeerDeviceStatus decrypt(String localDeviceId, String recipientUserId, String senderDeviceId, byte[] DRmessage, LimeOutputBuffer plainMessage) {
		// can't easily overload the native function and the native decrypt support empty cipherMessage buffer
		byte[] dummyCipherMessage = new byte[0];
		int native_status = this.n_decrypt(localDeviceId, recipientUserId, senderDeviceId, DRmessage, dummyCipherMessage, plainMessage);
		return LimePeerDeviceStatus.fromNative(native_status);
	}

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
	public void update(LimeStatusCallback statusObj, int OPkServerLowLimit, int OPkBatchSize) {
		this.n_update(statusObj, OPkServerLowLimit, OPkBatchSize);
	}
	/**
	 * @overload update(LimeStatusCallback statusObj)
	 * convenience form using default server limit(100) and batch size(25)
	 */
	public void update(LimeStatusCallback statusObj) {
		this.n_update(statusObj, 100, 25);
	}
}
