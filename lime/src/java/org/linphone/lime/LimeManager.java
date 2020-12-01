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

/**
 * @brief A java wrapper around the native Lime Manager interface
 *
 * To use this wrapper you must implement the interfaces
 * - LimePostToX3DH to communicate with the X3DH Https server
 * - LimeStatusCallback to manage the lime response to asynchronous operations: create/delete users, encrypt, update
 */
public class LimeManager {
	private long nativePtr; // stores the native object pointer

	// Call the native constructor -> do it when the object is created
	protected native void initialize(String db_access, LimePostToX3DH postObj);

	// Native functions involving enumerated paremeters not public
	// Enumeration translation is done on java side
	private native int n_decrypt(String localDeviceId, String recipientUserId, String senderDeviceId, byte[] DRmessage, byte[] cipherMessage, LimeOutputBuffer plainMessage);
	private native void n_encrypt(String localDeviceId, String recipientUserId, RecipientData[] recipients, byte[] plainMessage, LimeOutputBuffer cipherMessage, LimeStatusCallback statusObj, int encryptionPolicy);

	private native void n_create_user(String localDeviceId, String serverURL, int curveId, int OPkInitialBatchSize, LimeStatusCallback statusObj);
	private native void n_update(LimeStatusCallback statusObj, int OPkServerLowLimit, int OPkBatchSize);

	private native void n_set_peerDeviceStatus_Ik(String peerDeviceId, byte[] Ik, int status);
	private native void n_set_peerDeviceStatus(String peerDeviceId, int status);

	private native int n_get_peerDeviceStatus(String peerDeviceId);


	/**
	 * @brief Lime Manager constructor
	 *
	 * @param[in]	db_access	string used to access DB: can be filename for sqlite3 or access params for mysql, directly forwarded to SOCI session opening
	 * @param[in]	postObj		An object used to send data to the X3DH server
	 */
	public LimeManager(String db_access, LimePostToX3DH postObj) {
		initialize(db_access, postObj);
	}

	/**
	 * @brief Native ressource destructor
	 * We cannot rely on finalize (deprecated since java9), it must explicitely be called before the object is destroyed
	 * by the java environment
	 */
	public native void nativeDestructor();

	/**
	 * @brief Create a user in local database and publish it on the given X3DH server
	 *
	 * 	The Lime user shall be created at the same time the account is created on the device, this function shall not be called again, attempt to re-create an already existing user will fail.
	 * 	A user is identified by its deviceId (shall be the GRUU) and must at creation select a base Elliptic curve to use, this setting cannot be changed later
	 * 	A user is published on an X3DH key server who must run using the same elliptic curve selected for this user (creation will fail otherwise), the server url cannot be changed later
	 *
	 * @param[in]	localDeviceId		Identify the local user acount to use, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
	 * @param[in]	serverURL		The complete url(including port) of the X3DH key server. It must connect using HTTPS. Example: https://sip5.linphone.org:25519
	 * @param[in]	curveId			Choice of elliptic curve to use as base for ECDH and EdDSA operation involved. Can be CurveId::c25519 or CurveId::c448.
	 * @param[in]	OPkInitialBatchSize	Number of OPks in the first batch uploaded to X3DH server
	 * @param[in]	statusObj		This operation contact the X3DH server and is thus asynchronous, when server responds,
	 * 					the statusObj.callback will be called giving the exit status and an error message in case of failure
	 * @note
	 * The OPkInitialBatchSize is optionnal, if not used, set to defaults 100
	 */
	public void create_user(String localDeviceId, String serverURL, LimeCurveId curveId, int OPkInitialBatchSize, LimeStatusCallback statusObj) throws LimeException {
		this.n_create_user(localDeviceId, serverURL, curveId.getNative(), OPkInitialBatchSize, statusObj);
	}
	/**
	 * @overload  void create_user(String localDeviceId, String serverURL, LimeCurveId curveId, LimeStatusCallback statusObj)
	 */
	public void create_user(String localDeviceId, String serverURL, LimeCurveId curveId, LimeStatusCallback statusObj) throws LimeException {
			this.n_create_user(localDeviceId, serverURL, curveId.getNative(), 100, statusObj);
	}

	/**
	 * @brief Delete a user from local database and from the X3DH server
	 *
	 * if specified localDeviceId is not found in local Storage, throw an exception
	 *
	 * @param[in]	localDeviceId	Identify the local user acount to use, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
	 * @param[in]	statusObj	This operation contact the X3DH server and is thus asynchronous, when server responds,
	 *				the statusObj.callback will be called giving the exit status and an error message in case of failure
	 */
	public native void delete_user(String localDeviceId, LimeStatusCallback statusObj) throws LimeException;

	/**
	 * @brief Check if a user is present and active in local storage
	 *
	 * @param[in]	localDeviceId	Identify the local user acount to use, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
	 *
	 * @return true if the user is active in the local storage, false otherwise
	 */
	public native boolean is_user(String localDeviceId) throws LimeException;

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
	 * 					- the peer Status. If peerStatus is set to fail, this entry is ignored otherwise the peerStatus is set by the encrypt, see LimePeerDeviceStatus definition for details
	 * @param[in]		plainMessage	a buffer holding the message to encrypt, can be text or data.
	 * @param[out]		cipherMessage	points to the buffer to store the encrypted message which must be routed to all recipients(if one is produced, depends on encryption policy)
	 * @param[in]		statusObj	Performing encryption may involve the X3DH server and is thus asynchronous, when the operation is completed,
	 * 					this statusObj.callback will be called giving the exit status and an error message in case of failure.
	 * 					It is advised to store a reference to cipherMessage and recipients in this object so they can access
	 * 					the output of encryption as it won't be part of the callback parameters.
	 * @param[in]		encryptionPolicy	select how to manage the encryption: direct use of Double Ratchet message or encrypt in the cipher message and use the DR message to share the cipher message key
	 * 						default is optimized output size mode.
	 */
	public void encrypt(String localDeviceId, String recipientUserId, RecipientData[] recipients, byte[] plainMessage, LimeOutputBuffer cipherMessage, LimeStatusCallback statusObj, LimeEncryptionPolicy encryptionPolicy) {
		this.n_encrypt(localDeviceId, recipientUserId, recipients, plainMessage, cipherMessage, statusObj, encryptionPolicy.getNative());
	}
	/**
	* @overload encrypt(String localDeviceId, String recipientUserId, RecipientData[] recipients, byte[] plainMessage, LimeOutputBuffer cipherMessage, LimeStatusCallback statusObj)
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
	 * @param[in]	statusObj		This operation may contact the X3DH server and is thus asynchronous, when server responds,
	 * 					this statusObj.callback will be called giving the exit status and an error message in case of failure.
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

	/**
	 * @brief retrieve self Identity Key, an EdDSA formatted public key
	 *
	 * if specified localDeviceId is not found in local Storage, throw an exception
	 *
	 * @param[in]	localDeviceId	used to identify which local account we're dealing with, shall be the GRUU
	 * @param[out]	Ik		the EdDSA public identity key, formatted as in RFC8032
	 */
	public native void get_selfIdentityKey(String localDeviceId, LimeOutputBuffer Ik) throws LimeException;

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
	public void set_peerDeviceStatus(String peerDeviceId, byte[] Ik, LimePeerDeviceStatus status) throws LimeException{
		this.n_set_peerDeviceStatus_Ik(peerDeviceId, Ik, status.getNative());
	}

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
	public void set_peerDeviceStatus(String peerDeviceId, LimePeerDeviceStatus status) {
		this.n_set_peerDeviceStatus(peerDeviceId, status.getNative());
	}

	/**
	 * @brief get the status of a peer device: unknown, untrusted, trusted, unsafe
	 *
	 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
	 *
	 * @return unknown if the device is not in localStorage, untrusted, trusted or unsafe according to the stored value of peer device status flag otherwise
	 */
	public LimePeerDeviceStatus get_peerDeviceStatus(String peerDeviceId) {
		return LimePeerDeviceStatus.fromNative(this.n_get_peerDeviceStatus(peerDeviceId));
	}

	/**
	 * @brief delete a peerDevice from local storage
	 *
	 * @param[in]	peerDeviceId	The device Id to be removed from local storage, shall be its GRUU
	 *
	 * Call is silently ignored if the device is not found in local storage
	 */
	public native void delete_peerDevice(String peerDeviceId);

	/**
	 * @brief Set the X3DH key server URL for this identified user
	 * if specified localDeviceId is not found in local Storage, throw an exception
	 *
	 * @param[in]	localDeviceId		Identify the local user acount to use, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
	 * @param[in]	serverURL		The complete url(including port) of the X3DH key server. It must connect using HTTPS. Example: https://sip5.linphone.org:25519
	 */
	public native void set_x3dhServerUrl(String localDeviceId, String serverURL) throws LimeException;

	/**
	 * @brief Stale all sessions between localDeviceId and peerDevice.
	 * If peerDevice keep using this session to encrypt and we decrypt with success, the session will be reactivated
	 * but to encrypt a message to this peerDevice, a new session will be created.
	 * If no session is active between the given device, this call has no effect
	 *
	 * @param[in]	localDeviceId	Identify the local user account, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
	 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
	 */
	public native void stale_sessions(String localDeviceId, String peerDeviceId) throws LimeException;


	/**
	 * @brief Get the X3DH key server URL for this identified user
	 * if specified localDeviceId is not found in local Storage, throw an exception
	 *
	 * @param[in]	localDeviceId		Identify the local user acount to use, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
	 *
	 * @return	serverURL		The complete url(including port) of the X3DH key server.
	 */
	public native String get_x3dhServerUrl(String localDeviceId) throws LimeException;

	/**
	 * @brief native function to process the X3DH server response
	 *
	 * This function must be called by the response handler to forward the response (and http answer code)
	 *
	 * @param[in]	ptr		a native object pointer passed to the postToX3DH method
	 * @param[in]	responseCode	the HTTP server response code
	 * @param[in]	response	the binary X3DH server response
	 *
	 */
	public static native void process_X3DHresponse(long ptr, int responseCode, byte[] response);
}
