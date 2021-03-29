/**
	@file lime_ffi.h

	@brief This header exports lime's functionality via a C89 interface.

	General rules of this API to make life easier to anyone trying to use it
	from another langage:
	- all interaction are done via pointer to opaque structure
	- use only simple types:
		- size_t for all buffer or array size
		- const char * NULL terminated strings
		- uint8_t * for binary buffers
	- no ownership of memory transfers across the API boundary.\n
	The API read data from const pointers and write output to buffers allocated by the caller.
	- when exporting a binary blob, the function takes a pointer to the output buffer and a read/write pointer to the length.\n
	If the size of the provided buffer is insufficient, an error is returned.

	@warning Check carefully the lime_ffi_decrypt function's documentation as an output buffer too small on this one mean you would loose your decrypted message forever.

	@author Johan Pascal

	@copyright	Copyright (C) 2018  Belledonne Communications SARL

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
#ifndef lime_ffi_hpp
#define lime_ffi_hpp

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

typedef struct lime_manager_struct* lime_manager_t;

typedef struct lime_ffi_data_struct* lime_ffi_data_t;

enum LIME_FFI_ERROR {
	LIME_FFI_SUCCESS = 0,
	LIME_FFI_INVALID_CURVE_ARGUMENT = -1,
	LIME_FFI_INTERNAL_ERROR = -2,
	LIME_FFI_OUTPUT_BUFFER_TOO_SMALL = -3,
	LIME_FFI_USER_NOT_FOUND = -4
};

/** Identifies the elliptic curve used in lime, the values assigned are used in localStorage and X3DH server
 * so do not modify it or we'll loose sync with existing DB and X3DH server
 */
enum lime_ffi_CurveId {
	lime_ffi_CurveId_unset = 0, /**< used as default to detected incorrect behavior */
	lime_ffi_CurveId_c25519 = 1, /**< Curve 25519 */
	lime_ffi_CurveId_c448 = 2 /**< Curve 448-goldilocks */
};

/** Manage the encryption policy : how is the user's plaintext encrypted */
enum lime_ffi_EncryptionPolicy {
	lime_ffi_EncryptionPolicy_DRMessage, /**< the plaintext input is encrypted inside the Double Ratchet message (each recipient get a different encryption): not optimal for messages with numerous recipient */
	lime_ffi_EncryptionPolicy_cipherMessage, /**< the plaintext input is encrypted with a random key and this random key is encrypted to each participant inside the Double Ratchet message(for a single recipient the overhead is 48 bytes) */
	lime_ffi_EncryptionPolicy_optimizeUploadSize, /**< optimize upload size: encrypt in DR message if plaintext is short enougth to beat the overhead introduced by cipher message scheme, otherwise use cipher message. Selection is made on upload size only. This is the default policy used */
	lime_ffi_EncryptionPolicy_optimizeGlobalBandwidth /**< optimize bandwith usage: encrypt in DR message if plaintext is short enougth to beat the overhead introduced by cipher message scheme, otherwise use cipher message. Selection is made on uploadand download (from server to recipients) sizes added. */
};

/**
 * A peer device status returned after encrypt, decrypt or when directly asking for the peer device status to spot new devices and give information on our trust on this device
 * The values explicitely mapped to specific integers(untrusted, trusted, unsafe) are stored in local storage as integer
 * Do not modify the mapping or we will loose backward compatibility with existing databases
 */
enum lime_ffi_PeerDeviceStatus {
	lime_ffi_PeerDeviceStatus_untrusted=0, /**< we know this device but do not trust it, that information shall be displayed to the end user, a colour code shall be enough */
	lime_ffi_PeerDeviceStatus_trusted=1, /**< this peer device already got its public identity key validated, that information shall be displayed to the end user too */
	lime_ffi_PeerDeviceStatus_unsafe=2, /**< this status is a helper for the library user. It is used only by the peerDeviceStatus accessor functions */
	lime_ffi_PeerDeviceStatus_fail, /**< when returned by decrypt : we could not decrypt the incoming message\n
			when returned by encrypt in the peerStatus: we could not encrypt to this recipient(probably because it does not published keys on the X3DH server) */
	lime_ffi_PeerDeviceStatus_unknown /**< when returned after encryption or decryption, means it is the first time we communicate with this device (and thus create a DR session with it)\n
			   when returned by a get_peerDeviceStatus: this device is not in localStorage */
};

/** what a Lime callback could possibly say */
enum lime_ffi_CallbackReturn {
	lime_ffi_CallbackReturn_success, /**< operation completed successfully */
	lime_ffi_CallbackReturn_fail /**< operation failed, we shall have an explanation string too */
};

/** @brief The encrypt function input/output data structure
 *
 * give a recipient GRUU and get it back with the header which must be sent to recipient with the cipher text
 */
typedef struct {
	char *deviceId; /**< input: recipient deviceId (shall be GRUU) */
	enum lime_ffi_PeerDeviceStatus peerStatus; /**< output: after encrypt calls back, it will hold the status of this peer device:\n
						     - lime_ffi_PeerDeviceStatus_unknown: first interaction with this device)
						     - lime_ffi_PeerDeviceStatus_untrusted: device is kown but we never confirmed its identity public key
						     - lime_ffi_PeerDeviceStatus_trusted: we already confirmed this device identity public key
						     - lime_ffi_PeerDeviceStatus_fail: we could not encrypt for this device, probably because it never published its keys on the X3DH server */
	uint8_t *DRmessage; /**< output: after encrypt calls back, it will hold the Double Ratchet message targeted to the specified recipient. */
	size_t DRmessageSize; /**< input/output: size off the DRmessage buffer at input, size of written data as output */
} lime_ffi_RecipientData_t;

/** @brief Callback use to give a status on asynchronous operation
 *
 *	it returns a code and may return a string (could actually be empty) to detail what's happening
 *  	callback is used on every operation possibly involving a connection to X3DH server: create_user, delete_user, encrypt and update
 *
 *  @param[in]	userData	pointer to user defined data structure passed back to him
 *  @param[in]	status		success or fail
 *  @param[in]	message		in case of failure, an explanation, it may be empty
 */
typedef void (*lime_ffi_Callback)(void *userData, const enum lime_ffi_CallbackReturn status, const char *message);

/**
 * @brief Post a message to the X3DH server
 *
 * this function prototype is used to post data to the X3DH server
 * The response must be sent back to the lime engine using the lime_ffi_processX3DHServerResponse function
 *
 * @param[in]	userData	pointer given when registering the callback
 * @param[in]	limeData	pointer to an opaque lime internal structure that must then be forwarded with the server response
 * @param[in]	url		X3DH server's URL
 * @param[in]	from		User identification on X3DH server (which shall challenge for password digest, this is not however part of lime)
 * @param[in]	message		The message to post to the X3DH server
 */
typedef void (*lime_ffi_X3DHServerPostData)(void *userData, lime_ffi_data_t limeData, const char *url, const char *from,
					const uint8_t *message, const size_t message_size);



/**
 * @brief Forward X3DH server response to the lime engine
 *
 * @param[in]	limeData	A pointer to an opaque structure used internally (provided by the X3DHServerPostData function)
 * @param[in]	code		The response code given by X3DH server, connection is made through https, so we expect 200 for Ok
 * @param[in]	response	binary buffered server response
 * @param[in]	response_size	size of previous buffer
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_processX3DHServerResponse(lime_ffi_data_t limeData, const int code, const uint8_t *response, const size_t response_size);

/**
 * @brief Initialise a Lime Manager, only one per end-point is required.
 *
 * @param[out]	manager		pointer to the opaque structure used to interact with lime
 * @param[in]	db		string used to access DB (shall be filename for sqlite3), directly forwarded to SOCI session opening
 * @param[in]	X3DH_post_data	A function to send data to the X3DH server. The server response must be forwarded to lime using the lime_ffi_processX3DHServerResponse function
 * @param[in]	userData	pointer passed back to the X3DH_post_data callback
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_manager_init(lime_manager_t * const manager, const char *db, const lime_ffi_X3DHServerPostData X3DH_post_data, void *userData);

/**
 * @brief Destroy the internal structure used to interact with lime
 *
 * @param[in,out]	manager		pointer to the opaque structure used to interact with lime
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_manager_destroy(lime_manager_t manager);


/**
 * @brief Insert a lime user in local base and publish him on the X3DH server
 *
 * @param[in]	manager			pointer to the opaque structure used to interact with lime
 * @param[in]	localDeviceId		Identify the local user acount to use, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
 * @param[in]	x3dhServerUrl		The complete url(including port) of the X3DH key server. It must connect using HTTPS. Example: https://sip5.linphone.org:25519
 * @param[in]	curve			Choice of elliptic curve to use as base for ECDH and EdDSA operation involved. Can be lime_ffi_CurveId_c25519 or lime_ffi_CurveId_c448.
 * @param[in]	OPkInitialBatchSize	Number of OPks in the first batch uploaded to X3DH server
 * @param[in]	callback		This operation contact the X3DH server and is thus asynchronous, when server responds,\n
 * 					this callback will be called giving the exit status and an error message in case of failure
 * @param[in]	callbackUserData	this pointer will be forwarded to the callback as first parameter
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_create_user(lime_manager_t manager, const char *localDeviceId,
		const char *x3dhServerUrl, const enum lime_ffi_CurveId curve, const uint16_t OPkInitialBatchSize,
		const lime_ffi_Callback callback, void *callbackUserData);

/**
 * @brief Delete a user from local database and from the X3DH server
 *
 * if specified localDeviceId is not found in local Storage, return an error
 *
 * @param[in]	manager		pointer to the opaque structure used to interact with lime
 * @param[in]	localDeviceId	Identify the local user acount to use, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
 * @param[in]	callback	This operation contact the X3DH server and is thus asynchronous, when server responds,
 * 				this callback will be called giving the exit status and an error message in case of failure
 * @param[in]	callbackUserData	this pointer will be forwarded to the callback as first parameter
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_delete_user(lime_manager_t manager, const char *localDeviceId, const lime_ffi_Callback callback, void *callbackUserData);

/**
 * @brief Check if a user is present and active in local storage
 *
 * @param[in]	manager		pointer to the opaque structure used to interact with lime
 * @param[in]	localDeviceId	used to identify which local account looking up, shall be the GRUU (Null terminated string)
 *
 * @return LIME_FFI_SUCCESS if the user is active in the local storage, LIME_FFI_USER_NOT_FOUND otherwise
 */
int lime_ffi_is_user(lime_manager_t manager, const char *localDeviceId);

/**
 * @brief Compute the maximum buffer sizes for the encryption outputs: DRmessage and cipherMessage
 *
 * @param[in]	plainMessageSize	size of the plain message to be encrypted
 * @param[in]	curve			Choice of elliptic curve to use as base for ECDH and EdDSA operation involved. Can be lime_ffi_CurveId_c25519 or lime_ffi_CurveId_c448.
 * @param[out]	DRmessageSize		maximum size of the DRmessage produced by the encrypt function
 * @param[out]	cipherMessageSize	maximum size of the cipherMessage produced by the encrypt function
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_encryptOutBuffersMaximumSize(const size_t plainMessageSize,  const enum lime_ffi_CurveId curve, size_t *DRmessageSize, size_t *cipherMessageSize);

/**
 * @brief Encrypt a buffer (text or file) for a given list of recipient devices
 *
 * if specified localDeviceId is not found in local Storage, return an error
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
 * 	If the X3DH server can't provide keys for a peer device, its status is set to fail and its DRmessageSize is 0. Other devices get their encrypted message
 * 	If no peer device could get encrypted for all of them are missing keys on the X3DH server, the callback will be called with fail exit status
 *
 * @note all buffers are allocated by caller. If a buffer is too small to get the data, the function will return an error.
 *
 * @param[in]		manager			pointer to the opaque structure used to interact with lime
 * @param[in]		localDeviceId		used to identify which local acount to use and also as the identified source of the message, shall be the GRUU
 * @param[in]		recipientUserId		the Id of intended recipient, shall be a sip:uri of user or conference, is used as associated data to ensure no-one can mess with intended recipient
 * @param[in,out]	recipients		a list of RecipientData holding:
 *	 					- the recipient device Id(GRUU)
 * 						- an allocated buffer large enough to store the DRmessage which must then be routed to that recipient
 * 						- the size of this buffer, updated with the actual size written in it when the operation is completed(after callbackStatus is executed)
 * 						- the peer Status. If peerStatus is set to fail, this entry is ignored otherwise the peerStatus is set by the encrypt, see lime_ffi_PeerDeviceStatus definition for details
 * @param[in]		recipientsSize		how many recipients are in the recipients array
 * @param[in]		plainMessage		a buffer holding the message to encrypt, can be text or data.
 * @param[in]		plainMessageSize	size of the plainMessage buffer
 * @param[out]		cipherMessage		points to the buffer to store the encrypted message which must be routed to all recipients(if one is produced, depends on encryption policy)
 * @param[in,out]	cipherMessageSize	size of the cipherMessage buffer, is updated with the size of the actual data written in it
 * @param[in]		callback		Performing encryption may involve the X3DH server and is thus asynchronous, when the operation is completed,
 * 						this callback will be called giving the exit status and an error message in case of failure.
 * @param[in]		callbackUserData	this pointer will be forwarded to the callback as first parameter
 * @param[in]		encryptionPolicy	select how to manage the encryption: direct use of Double Ratchet message or encrypt in the cipher message and use the DR message to share the cipher message key
 * 						default is optimized output size mode.
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_encrypt(lime_manager_t manager, const char *localDeviceId,
		const char *recipientUserId, lime_ffi_RecipientData_t *const recipients, const size_t recipientsSize,
		const uint8_t *const plainMessage, const size_t plainMessageSize,
		uint8_t *const cipherMessage, size_t *cipherMessageSize,
		const lime_ffi_Callback callback, void *callbackUserData,
		enum lime_ffi_EncryptionPolicy encryptionPolicy);


/**
 * @brief Decrypt the given message
 *
 * if specified localDeviceId is not found in local Storage, return an error
 *
 * @param[in]		manager			pointer to the opaque structure used to interact with lime
 * @param[in]		localDeviceId		used to identify which local acount to use and also as the recipient device ID of the message, shall be the GRUU
 * @param[in]		recipientUserId		the Id of intended recipient, shall be a sip:uri of user or conference, is used as associated data to ensure no-one can mess with intended recipient
 * 						it is not necessarily the sip:uri base of the GRUU as this could be a message from alice first device intended to bob being decrypted on alice second device
 * @param[in]		senderDeviceId		Identify sender Device. This field shall be extracted from signaling data in transport protocol, is used to rebuild the authenticated data associated to the encrypted message
 * @param[in]		DRmessage		Double Ratchet message targeted to current device
 * @param[in]		DRmessageSize		DRmessage buffer size
 * @param[in]		cipherMessage		when present (depends on encryption policy) holds a common part of the encrypted message. Set to NULL if not present in the incoming message.
 * @param[in]		cipherMessageSize	cipherMessage buffer size(set to 0 if no cipherMessage is present in the incoming message)
 * @param[out]		plainMessage		the output buffer: its size shall be MAX(cipherMessageSize, DRmessageSize)
 * @param[in,out]	plainMessageSize	plainMessage buffer size, updated with the actual size of the data written
 *
 * @warning The plainMessage buffer must be large enough to store the decrypted message or we face the possibility to not ever be able to decrypt the message.(internal successful decryption will remove the ability to decrypt the same message again). To avoid problem the size of the plain message shall be  MAX(cipherMessageSize, DRmessageSize) as:
 * - The decrypted message is the same size of the encrypted one.
 * - The encrypted message(not alone but we can afford the temporary usage of few dozens bytes) is stored either in cipherMessage or DRmessage depends on encrypter's choice
 * - By allocating MAX(cipherMessageSize, DRmessageSize) bytes to the plainMessage buffer we ensure it can hold the decrypted message
 *
 * @return	fail if we cannot decrypt the message, unknown when it is the first message we ever receive from the sender device, untrusted for known but untrusted sender device, or trusted if it is
 */
enum lime_ffi_PeerDeviceStatus lime_ffi_decrypt(lime_manager_t manager, const char *localDeviceId,
		const char *recipientUserId, const char *senderDeviceId,
		const uint8_t *const DRmessage, const size_t DRmessageSize,
		const uint8_t *const cipherMessage, const size_t cipherMessageSize,
		uint8_t *const plainMessage, size_t *plainMessageSize);

/**
 * @brief retrieve self Identity Key, an EdDSA formatted public key
 *
 * if specified localDeviceId is not found in local Storage, return an error
 *
 * @param[in]		manager		pointer to the opaque structure used to interact with lime
 * @param[in]		localDeviceId	used to identify which local account we're dealing with, shall be the GRUU
 * @param[out]		Ik		the EdDSA public identity key, formatted as in RFC8032
 * @param[in,out]	IkSize		size of the previous buffer, updated with the size of data actually written
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_get_selfIdentityKey(lime_manager_t manager, const char *localDeviceId, uint8_t *const Ik, size_t *IkSize);

/**
 * @brief set the peer device status flag in local storage: unsafe, trusted or untrusted.
 *
 * @param[in]	manager		pointer to the opaque structure used to interact with lime
 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
 * @param[in]	Ik		the EdDSA peer public identity key, formatted as in RFC8032 (optionnal, needed only if status is trusted)
 * @param[in]	IkSize		size of the previous buffer
 * @param[in]	status		value of flag to set: accepted values are trusted, untrusted, unsafe
 *
 * return an error if given key doesn't match the one present in local storage(if we have a key)
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
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_set_peerDeviceStatus(lime_manager_t manager, const char *peerDeviceId, const uint8_t *const Ik, const size_t IkSize, enum lime_ffi_PeerDeviceStatus status);

/**
 * @brief get the status of a peer device: unknown, untrusted, trusted, unsafe
 *
 * @param[in]	manager		pointer to the opaque structure used to interact with lime
 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
 *
 * @return unknown if the device is not in localStorage, untrusted, trusted or unsafe according to the stored value of peer device status flag otherwise
 */
enum lime_ffi_PeerDeviceStatus lime_ffi_get_peerDeviceStatus(lime_manager_t manager, const char *peerDeviceId);


/**
 * @brief delete a peerDevice from local storage
 *
 * @param[in]	manager		pointer to the opaque structure used to interact with lime
 * @param[in]	peerDeviceId	The device Id to be removed from local storage, shall be its GRUU
 *
 * Call is silently ignored if the device is not found in local storage
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_delete_peerDevice(lime_manager_t manager, const char *peerDeviceId);

/**
 * @brief Stale all sessions between localDeviceId and peerDevice.
 * If peerDevice keep using this session to encrypt and we decrypt with success, the session will be reactivated
 * but to encrypt a message to this peerDevice, a new session will be created.
 * If no session is active between the given device, this call has no effect
 *
 * @param[in]	manager		pointer to the opaque structure used to interact with lime
 * @param[in]	localDeviceId	Identify the local user account, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
 * @param[in]	peerDeviceId	The device Id of peer, shall be its GRUU
 */
int lime_ffi_stale_sessions(lime_manager_t manager,  const char *localDeviceId, const char *peerDeviceId);
/**
 * @brief Update: shall be called once a day at least, performs checks, updates and cleaning operations
 *
 *  - check if we shall update a new SPk to X3DH server(SPk lifetime is set in settings)
 *  - check if we need to upload OPks to X3DH server
 *  - remove old SPks, clean double ratchet sessions (remove staled, clean their stored keys for skipped messages)
 *
 *  Is performed for all users founds in local storage
 *
 * @param[in]	manager		pointer to the opaque structure used to interact with lime
 * @param[in]	callback		Performing encryption may involve the X3DH server and is thus asynchronous, when the operation is completed,
 * 					this callback will be called giving the exit status and an error message in case of failure.
 * @param[in]	callbackUserData	this pointer will be forwarded to the callback as first parameter
 * @param[in]	OPkServerLowLimit	If server holds less OPk than this limit, generate and upload a batch of OPks
 * @param[in]	OPkBatchSize		Number of OPks in a batch uploaded to server
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_update(lime_manager_t manager, const lime_ffi_Callback callback, void *callbackUserData, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize);

/**
 * @brief Set the X3DH key server URL for this identified user
 *
 * @param[in]	manager			pointer to the opaque structure used to interact with lime
 * @param[in]	localDeviceId		Identify the local user account, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU
 * @param[in]	x3dhServerUrl		The complete url(including port) of the X3DH key server. It must connect using HTTPS. Example: https://sip5.linphone.org:25519
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_set_x3dhServerUrl(lime_manager_t manager, const char *localDeviceId, const char *x3dhServerUrl);

/**
 * @brief Get the X3DH key server URL for this identified user
 *
 * @param[in]		manager			pointer to the opaque structure used to interact with lime
 * @param[in]		localDeviceId		Identify the local user account, it must be unique and is also be used as Id on the X3DH key server, it shall be the GRUU, in a NULL terminated string
 * @param[in]		x3dhServerUrl		The complete url(including port) of the X3DH key server in a NULL terminated string
 * @param[in,out]	x3dhServerUrlSize	Size of the previous buffer, is updated with actual size of data written(without the '\0', would give the same result as strlen.)
 *
 * @return LIME_FFI_SUCCESS or a negative error code
 */
int lime_ffi_get_x3dhServerUrl(lime_manager_t manager, const char *localDeviceId, char *x3dhServerUrl, size_t *x3dhServerUrlSize);

#ifdef __cplusplus
}
#endif

#endif /* lime__ffi_hpp */
