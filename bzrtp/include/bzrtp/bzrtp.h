/*
 * Copyright (c) 2014-2019 Belledonne Communications SARL.
 *
 * This file is part of bzrtp.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef BZRTP_H
#define BZRTP_H

#include <stdint.h>
#include "bctoolbox/crypto.h" // for bctbx_rng_context_t
#include "bctoolbox/port.h" // for bctbx_mutex_t

#ifdef _MSC_VER
	#ifdef BZRTP_STATIC
		#define BZRTP_EXPORT
	#else /* BZRTP_STATIC */
		#ifdef BZRTP_EXPORTS
			#define BZRTP_EXPORT __declspec(dllexport)
		#else /* BZRTP_EXPORTS */
			#define BZRTP_EXPORT __declspec(dllimport)
		#endif /* BZRTP_EXPORTS */
	#endif /* BZRTP_STATIC */

	#ifndef BZRTP_DEPRECATED
		#define BZRTP_DEPRECATED __declspec(deprecated)
	#endif /* BZRTP_DEPRECATED */
#else /* _MSC_VER*/
	#define BZRTP_EXPORT __attribute__ ((visibility ("default")))

	#ifndef BZRTP_DEPRECATED
		#define BZRTP_DEPRECATED __attribute__ ((deprecated))
	#endif /* BZRTP_DEPRECATED */
#endif /* _MSC_VER*/

/**
 * Define different types of crypto functions */
#define ZRTP_HASH_TYPE			0x01
#define ZRTP_CIPHERBLOCK_TYPE 	0x02
#define ZRTP_AUTHTAG_TYPE		0x04
#define ZRTP_KEYAGREEMENT_TYPE	0x08
#define ZRTP_SAS_TYPE			0x10

/**
 * map the differents algorithm (some may not be available) to integer */

#define ZRTP_UNSET_ALGO			0x00

#define	ZRTP_HASH_S256			0x11
#define	ZRTP_HASH_S384			0x12
#define	ZRTP_HASH_S512			0x13
#define	ZRTP_HASH_N256			0x14
#define	ZRTP_HASH_N384			0x15

#define ZRTP_CIPHER_AES1		0x21
#define ZRTP_CIPHER_AES2		0x22
#define ZRTP_CIPHER_AES3		0x23
#define ZRTP_CIPHER_2FS1		0x24
#define ZRTP_CIPHER_2FS2		0x25
#define ZRTP_CIPHER_2FS3		0x26

#define ZRTP_AUTHTAG_HS32		0x31
#define ZRTP_AUTHTAG_HS80		0x32
#define ZRTP_AUTHTAG_SK32		0x33
#define ZRTP_AUTHTAG_SK64		0x34

/**
 * WARNING : it is very important to keep the key agreement defined in that order
 * as it is used to easily sort them from faster(DH2k) to slower(EC52)
 */
#define ZRTP_KEYAGREEMENT_DH2k	0x41
#define ZRTP_KEYAGREEMENT_X255	0x42
#define ZRTP_KEYAGREEMENT_K255	0x43
#define ZRTP_KEYAGREEMENT_EC25	0x44
#define ZRTP_KEYAGREEMENT_X448	0x45
#define ZRTP_KEYAGREEMENT_K448	0x46
#define ZRTP_KEYAGREEMENT_DH3k	0x47
#define ZRTP_KEYAGREEMENT_EC38	0x48
#define ZRTP_KEYAGREEMENT_EC52	0x49
#define ZRTP_KEYAGREEMENT_KYB1	0x4a
#define ZRTP_KEYAGREEMENT_KYB2	0x4b
#define ZRTP_KEYAGREEMENT_KYB3	0x4c
#define ZRTP_KEYAGREEMENT_HQC1	0x4d
#define ZRTP_KEYAGREEMENT_HQC2	0x4e
#define ZRTP_KEYAGREEMENT_HQC3	0x4f
#define ZRTP_KEYAGREEMENT_K255_KYB512	0x51
#define ZRTP_KEYAGREEMENT_K255_HQC128	0x52
#define ZRTP_KEYAGREEMENT_K448_KYB1024	0x53
#define ZRTP_KEYAGREEMENT_K448_HQC256	0x54
#define ZRTP_KEYAGREEMENT_K255_KYB512_HQC128	0x55
#define ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256	0x56

#define ZRTP_KEYAGREEMENT_Prsh	0x9e
#define ZRTP_KEYAGREEMENT_Mult	0x9f

#define ZRTP_SAS_B32			0xa1
#define ZRTP_SAS_B256			0xa2


/**
 * Define to give client indication on which srtp secrets are valid when given
 */
#define ZRTP_SRTP_SECRETS_FOR_SENDER	0x01
#define ZRTP_SRTP_SECRETS_FOR_RECEIVER	0x02

/**
 * brief The data structure containing the keys and algorithms to be used by srtp
 * Also stores SAS and informations about the crypto algorithms selected during ZRTP negotiation */
typedef struct bzrtpSrtpSecrets_struct  {
	uint8_t *selfSrtpKey; /**< The key used by local part to encrypt */
	uint8_t selfSrtpKeyLength; /**< The length in byte of the key */
	uint8_t *selfSrtpSalt; /**< The salt used by local part to encrypt */
	uint8_t selfSrtpSaltLength; /**< The length in byte of the salt */
	uint8_t *peerSrtpKey; /**< The key used by local part to decrypt */
	uint8_t peerSrtpKeyLength; /**< The length in byte of the key */
	uint8_t *peerSrtpSalt; /**< The salt used by local part to decrypt */
	uint8_t peerSrtpSaltLength; /**< The length in byte of the salt */
	uint8_t cipherAlgo; /**< The cipher block algorithm selected durign ZRTP negotiation and used by srtp */
	uint8_t cipherKeyLength; /**< The key length in bytes for the cipher block algorithm used by srtp */
	uint8_t authTagAlgo; /**< srtp authentication tag algorithm agreed on after Hello packet exchange */
	char *sas; /**< a null terminated char containing the Short Authentication String */
	uint8_t sasLength; /**< The length of sas, including the termination character */
	uint8_t hashAlgo; /**< The hash algo selected during ZRTP negotiation */
	uint8_t keyAgreementAlgo; /**< The key agreement algo selected during ZRTP negotiation */
	uint8_t sasAlgo; /**< The SAS rendering algo selected during ZRTP negotiation */
	uint8_t cacheMismatch; /**< Flag set to 1 in case of ZRTP cache mismatch, may occurs only on first channel(the one computing SAS) */
	uint8_t auxSecretMismatch; /**< Flag set to BZRTP_AUXSECRET_MATCH, BZRTP_AUXSECRET_MISMATCH or BZRTP_AUXSECRET_UNSET, may occurs only on first channel(the one computing SAS), in case of mismatch it may be ignored and we can still validate the SAS */
	uint8_t peerAcceptGoClear; /**< Flag set to 1 in case of peer accept receiving GoClear */
} bzrtpSrtpSecrets_t;

/* define flag IDs */
#define BZRTP_IS_INITIALISED            0x00
#define BZRTP_IS_SECURE                 0x01
#define BZRTP_PEER_SUPPORT_MULTICHANNEL 0x02
#define BZRTP_SELF_ACCEPT_GOCLEAR       0x03
#define BZRTP_PEER_ACCEPT_GOCLEAR       0x04

/* define auxSecretMismatch flag codes */
#define BZRTP_AUXSECRET_MATCH		0x00
#define BZRTP_AUXSECRET_MISMATCH	0x01
#define BZRTP_AUXSECRET_UNSET		0x02

/* define message levels */
#define BZRTP_MESSAGE_ERROR     0x00
#define BZRTP_MESSAGE_WARNING	0x01
#define BZRTP_MESSAGE_LOG       0x02
#define BZRTP_MESSAGE_DEBUG     0x03

/* define message codes */
#define BZRTP_MESSAGE_CACHEMISMATCH 		0x01
#define BZRTP_MESSAGE_PEERVERSIONOBSOLETE	0x02
#define BZRTP_MESSAGE_PEERNOTBZRTP          0x03
#define BZRTP_MESSAGE_PEERREQUESTGOCLEAR    0x04
#define BZRTP_MESSAGE_PEERACKGOCLEAR		0x05

/**
 * Function pointer used by bzrtp to free memory allocated by callbacks.
**/
typedef void (*zrtpFreeBuffer_callback)(void *);
/**
 * @brief All the callback functions provided by the client needed by the ZRTP engine
 */
typedef struct bzrtpCallbacks_struct {
	/* messaging status and warnings */
	int (* bzrtp_statusMessage)(void *clientData, const uint8_t messageLevel, const uint8_t messageId, const char *messageString); /**< Sending messages to caller: error, warnings, logs, the messageString can be NULL or a NULL terminated string */
	int bzrtp_messageLevel; /**< Filter calls to this callback to levels inferiors to this setting (BZRTP_MESSAGE_ERROR, BZRTP_MESSAGE_WARNING, BZRTP_MESSAGE_LOG, BZRTP_MESSAGE_DEBUG )*/

	/* sending packets */
	int (* bzrtp_sendData)(void *clientData, const uint8_t *packetString, uint16_t packetLength); /**< Send a ZRTP packet to peer. Shall return 0 on success */

	/* dealing with SRTP session */
	int (* bzrtp_srtpSecretsAvailable)(void *clientData, const bzrtpSrtpSecrets_t *srtpSecrets, uint8_t part); /**< Send the srtp secrets to the client, for either sender, receiver or both according to the part parameter value. Client may wait for the end of ZRTP process before using it */
	int (* bzrtp_startSrtpSession)(void *clientData, const bzrtpSrtpSecrets_t *srtpSecrets, int32_t verified); /**< ZRTP process ended well, client is given the SAS and informations about the crypto algo used during ZRTP negotiation. He may start his SRTP session if not done when calling srtpSecretsAvailable */

	/* ready for exported keys */
	int (* bzrtp_contextReadyForExportedKeys)(void *clientData, int zuid, uint8_t role); /**< Tell the client that this is the time to create any exported keys, s0 is erased just after the call to this callback. Callback is given the peerZID and zuid to adress the correct node in cache and current role which is needed to set a pair of keys for IM encryption */
} bzrtpCallbacks_t;

#define ZRTP_MAGIC_COOKIE 0x5a525450
#define ZRTP_VERSION	"1.10"

/* error code definition */
#define BZRTP_ERROR_INVALIDCALLBACKID				0x0001
#define	BZRTP_ERROR_CONTEXTNOTREADY					0x0002
#define BZRTP_ERROR_INVALIDCONTEXT					0x0004
#define BZRTP_ERROR_MULTICHANNELNOTSUPPORTEDBYPEER	0x0008
#define BZRTP_ERROR_UNABLETOADDCHANNEL				0x0010
#define BZRTP_ERROR_UNABLETOSTARTCHANNEL			0x0020
#define BZRTP_ERROR_OUTPUTBUFFER_LENGTH				0x0040
#define BZRTP_ERROR_HELLOHASH_MISMATCH				0x0080
#define BZRTP_ERROR_CHANNELALREADYSTARTED			0x0100
#define BZRTP_ERROR_CACHEDISABLED					0x0200
#define BZRTP_ERROR_CACHEMIGRATIONFAILED			0x0400
#define BZRTP_ERROR_CACHE_PEERNOTFOUND				0x0800
#define BZRTP_ERROR_INVALIDCLEARMAC                 0x1000
#define BZRTP_ERROR_PEERDOESNTACCEPTGOCLEAR         0x2000
#define BZRTP_ERROR_GOCLEARDISABLED					0x4000
#define BZRTP_ERROR_INVALIDARGUMENT					0x8000

/* channel status definition */
#define BZRTP_CHANNEL_NOTFOUND						0x1000
#define BZRTP_CHANNEL_INITIALISED					0x1001
#define BZRTP_CHANNEL_ONGOING						0x1002
#define BZRTP_CHANNEL_SECURE						0x1004
#define BZRTP_CHANNEL_CLEAR                         0x1010
#define BZRTP_CHANNEL_ERROR                         0x1008

/* role mapping */
#define BZRTP_ROLE_INITIATOR	0
#define	BZRTP_ROLE_RESPONDER	1

/* channel receiving GoClear message */
#define BZRTP_RECEPTION_UNKNOWN 0
#define BZRTP_RECEPTION_YES     1
#define BZRTP_RECEPTION_NO      2

/* cache related value */
#define BZRTP_CACHE_SETUP		0x2000
#define BZRTP_CACHE_UPDATE		0x2001
#define BZRTP_CACHE_DATA_NOTFOUND	0x2002
#define BZRTP_CACHE_PEER_STATUS_UNKNOWN 0x2010
#define BZRTP_CACHE_PEER_STATUS_VALID   0x2011
#define BZRTP_CACHE_PEER_STATUS_INVALID 0x2012

/* cache function error codes */
#define BZRTP_ZIDCACHE_INVALID_CONTEXT		0x2101
#define BZRTP_ZIDCACHE_INVALID_CACHE		0x2102
#define BZRTP_ZIDCACHE_UNABLETOUPDATE		0x2103
#define BZRTP_ZIDCACHE_UNABLETOREAD		0x2104
#define BZRTP_ZIDCACHE_BADINPUTDATA		0x2105
#define BZRTP_ZIDCACHE_RUNTIME_CACHELESS	0x2110

/**
 * @brief bzrtpContext_t The ZRTP engine context
 * Store current state, timers, HMAC and encryption keys
*/
typedef struct bzrtpContext_struct bzrtpContext_t;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create context structure and initialise it
 *
 * @return The ZRTP engine context data
 *                                                                        
*/
BZRTP_EXPORT bzrtpContext_t *bzrtp_createBzrtpContext(void);

/**
 * @brief Perform initialisation which can't be done without ZIDcache acces
 * - get ZID and create the first channel context
 *
 *   @param[in]		context		The context to initialise
 *   @param[in]		selfSSRC	The SSRC given to the first channel context created within the zrtpContext
 *
 *   @return 0 on success
 */
BZRTP_EXPORT int bzrtp_initBzrtpContext(bzrtpContext_t *context, uint32_t selfSSRC);

/**
 * Free memory of context structure to a channel, if all channels are freed, free the global zrtp context
 * @param[in]	context		Context hosting the channel to be destroyed.(note: the context zrtp context itself is destroyed with the last channel)
 * @param[in]	selfSSRC	The SSRC identifying the channel to be destroyed
 *                                                                           
 * @return the number of channel still active in this ZRTP context
*/
BZRTP_EXPORT int bzrtp_destroyBzrtpContext(bzrtpContext_t *context, uint32_t selfSSRC);

/**
 * @brief Allocate a function pointer to the callback function identified by his id 
 * @param[in,out]	context				The zrtp context to set the callback function
 * @param[in] 		cbs 				A structure containing all the callbacks to supply.
 *
 * @return 0 on success
 *                                                                           
*/
BZRTP_EXPORT int bzrtp_setCallbacks(bzrtpContext_t *context, const bzrtpCallbacks_t *cbs);

/**
 * @brief Set the pointer allowing cache access,
 * this version of the function get a mutex to lock the cache when accessing it
 *
 * @param[in,out]	context			The ZRTP context we're dealing with
 * @param[in]		zidCache		Used by internal function to access cache: turn into a sqlite3 pointer if cache is enabled
 * @param[in]		selfURI			Local URI used for this communication, needed to perform cache operation, NULL terminated string, duplicated by this function
 * @param[in]		peerURI			Peer URI used for this communication, needed to perform cache operation, NULL terminated string, duplicated by this function
 * @param[in]		zidCacheMutex		Points to a mutex used to lock zidCache database access
 *
 * @return 0 or BZRTP_CACHE_SETUP(if cache is populated by this call) on success, error code otherwise
*/
BZRTP_EXPORT int bzrtp_setZIDCache_lock(bzrtpContext_t *context, void *zidCache, const char *selfURI, const char *peerURI, bctbx_mutex_t *zidCacheMutex);

/**
 * @brief Set the pointer allowing cache access
 *
 * @param[in,out]	context			The ZRTP context we're dealing with
 * @param[in]		zidCache		Used by internal function to access cache: turn into a sqlite3 pointer if cache is enabled
 * @param[in]   	selfURI			Local URI used for this communication, needed to perform cache operation, NULL terminated string, duplicated by this function
 * @param[in]   	peerURI			Peer URI used for this communication, needed to perform cache operation, NULL terminated string, duplicated by this function
 *
 * @return 0 or BZRTP_CACHE_SETUP(if cache is populated by this call) on success, error code otherwise
*/
BZRTP_EXPORT int bzrtp_setZIDCache(bzrtpContext_t *context, void *zidCache, const char *selfURI, const char *peerURI);

/**
 * @brief Set the client data pointer in a channel context
 * This pointer is returned to the client by the callbacks function, used to store associated contexts (RTP session)
 * @param[in,out]	zrtpContext		The ZRTP context we're dealing with
 * @param[in]		selfSSRC		The SSRC identifying the channel to be linked to the client Data
 * @param[in]		clientData		The clientData pointer, casted to a (void *)
 *
 * @return 0 on success
 *                                                                           
*/
BZRTP_EXPORT int bzrtp_setClientData(bzrtpContext_t *zrtpContext, uint32_t selfSSRC, void *clientData);

/**
 * @brief Add a channel to an existing context
 *
 * @param[in,out]	zrtpContext	The zrtp context who will get the additionnal channel
 * @param[in]		selfSSRC	The SSRC given to the channel context
 *
 * @return 0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_addChannel(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);


/**
 * @brief Start the state machine of the specified channel
 * To be able to start an addional channel, we must be in secure state
 *
 * @param[in,out]	zrtpContext			The ZRTP context hosting the channel to be started
 * @param[in]		selfSSRC			The SSRC identifying the channel to be started(will start sending Hello packets and listening for some)
 *
 * @return			0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_startChannelEngine(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);

/**
 * @brief Send the current time to a specified channel, it will check if it has to trig some timer
 *
 * @param[in,out]	zrtpContext			The ZRTP context hosting the channel
 * @param[in]		selfSSRC			The SSRC identifying the channel
 * @param[in]		timeReference		The current time in ms
 *
 * @return			0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_iterate(bzrtpContext_t *zrtpContext, uint32_t selfSSRC, uint64_t timeReference);

/**
 * @brief Process a received message
 *
 * @param[in,out]	zrtpContext				The ZRTP context we're dealing with
 * @param[in]		selfSSRC				The SSRC identifying the channel receiving the message
 * @param[in]		zrtpPacketString		The packet received
 * @param[in]		zrtpPacketStringLength	Length of the packet in bytes
 *
 * @return 	0 on success, errorcode otherwise
 */
BZRTP_EXPORT int bzrtp_processMessage(bzrtpContext_t *zrtpContext, uint32_t selfSSRC, uint8_t *zrtpPacketString, uint16_t zrtpPacketStringLength);

/**
 * @brief Called by user when the SAS has been verified
 *
 * @param[in,out]	zrtpContext				The ZRTP context we're dealing with
 */
BZRTP_EXPORT void bzrtp_SASVerified(bzrtpContext_t *zrtpContext); 

/**
 * @brief Called by user when the SAS has been set to unverified
 *
 * @param[in,out]	zrtpContext				The ZRTP context we're dealing with
 */
BZRTP_EXPORT void bzrtp_resetSASVerified(bzrtpContext_t *zrtpContext);

/**
 * @brief Reset the retransmission timer of a given channel.
 * Packets will be sent again if appropriate:
 *  - when in responder role, zrtp engine only answer to packets sent by the initiator.
 *  - if we are still in discovery phase, Hello or Commit packets will be resent.
 *
 * @param[in,out]	zrtpContext				The ZRTP context we're dealing with
 * @param[in]		selfSSRC				The SSRC identifying the channel to reset
 *
 * @return 0 on success, error code otherwise
 */
BZRTP_EXPORT int bzrtp_resetRetransmissionTimer(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);

/**
 * @brief Get the supported crypto types
 *
 * @param[in]		zrtpContext				The ZRTP context we're dealing with
 * @param[in]		algoType				mapped to defines, must be in [ZRTP_HASH_TYPE, ZRTP_CIPHERBLOCK_TYPE, ZRTP_AUTHTAG_TYPE, ZRTP_KEYAGREEMENT_TYPE or ZRTP_SAS_TYPE]
 * @param[out]		supportedTypes			mapped to uint8_t value of the 4 char strings giving the supported types as string according to rfc section 5.1.2 to 5.1.6
 *
 * @return number of supported types, 0 on error
 */
BZRTP_EXPORT uint8_t bzrtp_getSupportedCryptoTypes(bzrtpContext_t *zrtpContext, uint8_t algoType, uint8_t supportedTypes[7]);

/**
 * @brief set the supported crypto types. This function must be called before the context is initialised, just after creation.
 *
 * @param[in,out]	zrtpContext				The ZRTP context we're dealing with
 * @param[in]		algoType				mapped to defines, must be in [ZRTP_HASH_TYPE, ZRTP_CIPHERBLOCK_TYPE, ZRTP_AUTHTAG_TYPE, ZRTP_KEYAGREEMENT_TYPE or ZRTP_SAS_TYPE]
 * @param[in]		supportedTypes			mapped to uint8_t value of the 4 char strings giving the supported types as string according to rfc section 5.1.2 to 5.1.6
 * @param[in]		supportedTypesCount		number of supported crypto types
 *
 * @return 0 on success, error code otherwise
 */
BZRTP_EXPORT int bzrtp_setSupportedCryptoTypes(bzrtpContext_t *zrtpContext, uint8_t algoType, uint8_t supportedTypes[7], uint8_t supportedTypesCount);

/**
 * @brief Set the selfAcceptGoClear flag
 *
 * @param[in,out]   zrtpContext The ZRTP context we're dealing with
 * @param[in]       flagId      mapped to defines, must be BZRTP_SELF_ACCEPT_GOCLEAR
 * @param[in]       value       Flag value
 */
BZRTP_EXPORT int bzrtp_setFlags(bzrtpContext_t *zrtpContext, uint8_t flagId, uint8_t value);

/**
 * @brief Set the peer hello hash given by signaling to a ZRTP channel
 *
 * @param[in,out]	zrtpContext						The ZRTP context we're dealing with
 * @param[in]		selfSSRC						The SSRC identifying the channel
 * @param[in]		peerHelloHashHexString			A NULL terminated string containing the hexadecimal form of the hash received in signaling,
 * 													may contain ZRTP version as header.
 * @param[in]		peerHelloHashHexStringLength	Length of hash string (shall be at least 64 as the hash is a SHA256 so 32 bytes,
 * 													more if it contains the version header)
 *
 * @return 	0 on success, errorcode otherwise
 */
BZRTP_EXPORT int bzrtp_setPeerHelloHash(bzrtpContext_t *zrtpContext, uint32_t selfSSRC, uint8_t *peerHelloHashHexString, size_t peerHelloHashHexStringLength);

/**
 * @brief Get the self hello hash from ZRTP channel
 *
 * @param[in,out]	zrtpContext			The ZRTP context we're dealing with
 * @param[in]		selfSSRC			The SSRC identifying the channel
 * @param[out]		output				A NULL terminated string containing the hexadecimal form of the hash received in signaling,
 * 										contain ZRTP version as header. Buffer must be allocated by caller.
 * @param[in]		outputLength		Length of output buffer, shall be at least 70 : 5 chars for version, 64 for the hash itself, SHA256), NULL termination
 *
 * @return 	0 on success, errorcode otherwise
 */
BZRTP_EXPORT int bzrtp_getSelfHelloHash(bzrtpContext_t *zrtpContext, uint32_t selfSSRC, uint8_t *output, size_t outputLength);

/**
 * @brief Get the channel status
 *
 * @param[in]		zrtpContext			The ZRTP context we're dealing with
 * @param[in]		selfSSRC			The SSRC identifying the channel
 *
 * @return	BZRTP_CHANNEL_NOTFOUND 		no channel matching this SSRC doesn't exists in the zrtp context
 * 			BZRTP_CHANNEL_INITIALISED	channel initialised but not started
 * 			BZRTP_CHANNEL_ONGOING		ZRTP key exchange in ongoing
 *			BZRTP_CHANNEL_SECURE		Channel is secure
 *			BZRTP_CHANNEL_ERROR			An error occured on this channel
 */
BZRTP_EXPORT int bzrtp_getChannelStatus(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);


/**
 * @brief Set Auxiliary Secret for this channel(shall be used only on primary audio channel)
 *   The given auxSecret is appended to any aux secret found in ZIDcache
 *   This function must be called before reception of peerHello packet
 *
 * @param[in]		zrtpContext	The ZRTP context we're dealing with
 * @param[in]		auxSecret	A buffer holding the auxiliary shared secret to use (see RFC 6189 section 4.3)
 * @param[in]		auxSecretLength	lenght of the previous buffer
 *
 * @return 0 on success, error code otherwise
 *
 * @note The auxiliary shared secret mechanic is used by LIMEv2 for encryption security purposes but might be used for its original purpose in a regular
 * ZRTP session if it becomes necessary in the future, or by another encryption engine for example. In that case the API will need an adaptation work.
 */
BZRTP_EXPORT int bzrtp_setAuxiliarySharedSecret(bzrtpContext_t *zrtpContext, const uint8_t *auxSecret, size_t auxSecretLength);

/**
 * @brief Get the ZRTP auxiliary shared secret mismatch status
 * @param[in]		zrtpContext			The ZRTP context we're dealing with
 * @return	BZRTP_AUXSECRET_MATCH on match, BZRTP_AUXSECRET_MISMATCH on mismatch, BZRTP_AUXSECRET_UNSET if auxiliary shared secret is unused
 */
BZRTP_EXPORT uint8_t bzrtp_getAuxiliarySharedSecretMismatch(bzrtpContext_t *zrtpContext);

/*** Cache related functions ***/
/**
 * @brief Check the given sqlite3 DB and create requested tables if needed
 * 	Also manage DB schema upgrade
 * @param[in,out]	db	Pointer to the sqlite3 db open connection
 * 				Use a void * to keep this API when building cacheless
 *
 * @return 0 on success, BZRTP_CACHE_SETUP if cache was empty, BZRTP_CACHE_UPDATE if db structure was updated, error code otherwise
 */
BZRTP_EXPORT BZRTP_DEPRECATED int bzrtp_initCache(void *db);

/**
 * @brief Check the given sqlite3 DB and create requested tables if needed
 * 	Also manage DB schema upgrade
 *
 * this version of the function gets a mutex to lock the cache when accessing it
 *
 * @param[in,out]	db	Pointer to the sqlite3 db open connection
 * 				Use a void * to keep this API when building cacheless
 * @param[in]   zidCacheMutex	Points to a mutex used to lock zidCache database access, ignored if NULL
 *
 * @return 0 on success, BZRTP_CACHE_SETUP if cache was empty, BZRTP_CACHE_UPDATE if db structure was updated, error code otherwise
 */
BZRTP_EXPORT int bzrtp_initCache_lock(void *db, bctbx_mutex_t *zidCacheMutex);

/**
 * @brief : retrieve ZID from cache
 * ZID is randomly generated if cache is empty or inexistant
 * ZID is randomly generated in case of cacheless implementation(db argument is NULL)
 *
 * @param[in,out] 	db		sqlite3 database(or NULL if we don't use cache at runtime)
 * 					Use a void * to keep this API when building cacheless
 * @param[in]		selfURI		the sip uri of local user, NULL terminated string
 * @param[out]		selfZID		the ZID, retrieved from cache or randomly generated
 * @param[in]		RNGContext	A RNG context used to generate ZID if needed
 *
 * @return		0 on success, or BZRTP_CACHE_DATA_NOTFOUND if no ZID matching the URI was found and no RNGContext is given to generate one
 */
BZRTP_EXPORT BZRTP_DEPRECATED int bzrtp_getSelfZID(void *db, const char *selfURI, uint8_t selfZID[12], bctbx_rng_context_t *RNGContext);

/**
 * @brief : retrieve ZID from cache
 * ZID is randomly generated if cache is empty or inexistant
 * ZID is randomly generated in case of cacheless implementation(db argument is NULL)
 * this version of the function gets a mutex to lock the cache when accessing it
 *
 * @param[in,out] 	db		sqlite3 database(or NULL if we don't use cache at runtime)
 * 					Use a void * to keep this API when building cacheless
 * @param[in]		selfURI		the sip uri of local user, NULL terminated string
 * @param[out]		selfZID		the ZID, retrieved from cache or randomly generated
 * @param[in]		RNGContext	A RNG context used to generate ZID if needed
 * @param[in]  		zidCacheMutex	Points to a mutex used to lock zidCache database access, ignored if NULL
 *
 * @return		0 on success, or BZRTP_CACHE_DATA_NOTFOUND if no ZID matching the URI was found and no RNGContext is given to generate one
 */
BZRTP_EXPORT int bzrtp_getSelfZID_lock(void *db, const char *selfURI, uint8_t selfZID[12], bctbx_rng_context_t *RNGContext, bctbx_mutex_t *zidCacheMutex);

/**
 * @brief Write(insert or update) data in cache, adressing it by zuid (ZID/URI binding id used in cache)
 * 		Get arrays of column names, values to be inserted, lengths of theses values
 *		All three arrays must be the same lenght: columnsCount
 * 		If the row isn't present in the given table, it will be inserted
 *
 * @param[in,out]	dbPointer	Pointer to an already opened sqlite db
 * @param[in]		zuid		The DB internal id to adress the correct row(binding between local uri and peer ZID+URI)
 * @param[in]		tableName	The name of the table to write in the db, must already exists. Null terminated string
 * @param[in]		columns		An array of null terminated strings containing the name of the columns to update
 * @param[in]		values		An array of buffers containing the values to insert/update matching the order of columns array
 * @param[in]		lengths		An array of integer containing the lengths of values array buffer matching the order of columns array
 * @param[in]		columnsCount	length common to columns,values and lengths arrays
 *
 * @return 0 on succes, error code otherwise
 */
BZRTP_EXPORT BZRTP_DEPRECATED int bzrtp_cache_write(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount);

/**
 * @brief Write(insert or update) data in cache, adressing it by zuid (ZID/URI binding id used in cache)
 * 		Get arrays of column names, values to be inserted, lengths of theses values
 *		All three arrays must be the same lenght: columnsCount
 * 		If the row isn't present in the given table, it will be inserted
 * this version of the function gets a mutex to lock the cache when accessing it
 *
 * @param[in,out]	dbPointer	Pointer to an already opened sqlite db
 * @param[in]		zuid		The DB internal id to adress the correct row(binding between local uri and peer ZID+URI)
 * @param[in]		tableName	The name of the table to write in the db, must already exists. Null terminated string
 * @param[in]		columns		An array of null terminated strings containing the name of the columns to update
 * @param[in]		values		An array of buffers containing the values to insert/update matching the order of columns array
 * @param[in]		lengths		An array of integer containing the lengths of values array buffer matching the order of columns array
 * @param[in]		columnsCount	length common to columns,values and lengths arrays
 * @param[in]		zidCacheMutex	Points to a mutex used to lock zidCache database access, ignored if NULL
 *
 * @return 0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_cache_write_lock(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount, bctbx_mutex_t *zidCacheMutex);

/**
 * @brief Read data from specified table/columns from cache adressing it by zuid (ZID/URI binding id used in cache)
 * 		Get arrays of column names, values to be read, and the number of colums to be read
 *		Produce an array of values(uint8_t arrays) and a array of corresponding lengths
 *		Values memory is allocated by this function and must be freed by caller
 *
 * @param[in,out]	dbPointer	Pointer to an already opened sqlite db
 * @param[in]		zuid		The DB internal id to adress the correct row(binding between local uri and peer ZID+URI)
 * @param[in]		tableName	The name of the table to read in the db. Null terminated string
 * @param[in]		columns		An array of null terminated strings containing the name of the columns to read, the array's length  is columnsCount
 * @param[out]		values		An array of uint8_t pointers, each one will be allocated to the read value and they must be freed by caller
 * @param[out]		lengths		An array of integer containing the lengths of values array buffer read
 * @param[in]		columnsCount	length common to columns,values and lengths arrays
 *
 * @return 0 on succes, error code otherwise
 */
BZRTP_EXPORT BZRTP_DEPRECATED int bzrtp_cache_read(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount);

/**
 * @brief Read data from specified table/columns from cache adressing it by zuid (ZID/URI binding id used in cache)
 * 		Get arrays of column names, values to be read, and the number of colums to be read
 *		Produce an array of values(uint8_t arrays) and a array of corresponding lengths
 *		Values memory is allocated by this function and must be freed by caller
 * this version of the function gets a mutex to lock the cache when accessing it
 *
 * @param[in,out]	dbPointer	Pointer to an already opened sqlite db
 * @param[in]		zuid		The DB internal id to adress the correct row(binding between local uri and peer ZID+URI)
 * @param[in]		tableName	The name of the table to read in the db. Null terminated string
 * @param[in]		columns		An array of null terminated strings containing the name of the columns to read, the array's length  is columnsCount
 * @param[out]		values		An array of uint8_t pointers, each one will be allocated to the read value and they must be freed by caller
 * @param[out]		lengths		An array of integer containing the lengths of values array buffer read
 * @param[in]		columnsCount	length common to columns,values and lengths arrays
 * @param[in]		zidCacheMutex	Points to a mutex used to lock zidCache database access, ignored if NULL
 *
 * @return 0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_cache_read_lock(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount, bctbx_mutex_t *zidCacheMutex);

/**
 * @brief Perform migration from xml version to sqlite3 version of cache
 *	Warning: new version of cache associate a ZID to each local URI, the old one did not
 *		the migration function will associate any data in the cache to the sip URI given in parameter which shall be the default URI
 * @param[in]		cacheXmlPtr	a pointer to an xmlDocPtr structure containing the old cache to be migrated
 * @param[in,out]	cacheSqlite	a pointer to an sqlite3 structure containing a cache initialised using bzrtp_cache_init function
 * @param[in]		selfURI		default sip URI for this end point, NULL terminated char
 *
 * @return	0 on success, BZRTP_ERROR_CACHEDISABLED when bzrtp was not compiled with cache enabled, BZRTP_ERROR_CACHEMIGRATIONFAILED on error during migration
 */
BZRTP_EXPORT int bzrtp_cache_migration(void *cacheXmlPtr, void *cacheSqlite, const char *selfURI);

/*
 * @brief  Allow client to compute an exported according to RFC section 4.5.2
 *		Check the context is ready(we already have a master exported key and KDF context)
 * 		and run KDF(master exported key, "Label", KDF_Context, negotiated hash Length)
 *
 * @param[in]		zrtpContext		The ZRTP context we're dealing with
 * @param[in]		label			Label used in the KDF
 * @param[in]		labelLength		Length of previous buffer
 * @param[out]		derivedKey		Buffer to store the derived key
 * @param[in,out]	derivedKeyLength	Length of previous buffer(updated to fit actual length of data produced if too long)
 *
 * @return 0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_exportKey(bzrtpContext_t *zrtpContext, char *label, size_t labelLength, uint8_t *derivedKey, size_t *derivedKeyLength);

/**
 * @brief Retrieve from bzrtp cache the trust status(based on the previously verified flag) of a peer URI
 *
 * This function will return the SAS validation status of the active device
 * associated to the given peerURI.
 *
 * Important note about the active device:
 * - any ZRTP exchange with a peer device will set it to be the active one for its sip:uri
 * - the concept of active device is shared between local accounts if there are several of them, it means that :
 *       - if you have several local users on your device, each of them may have an entry in the ZRTP cache with a particular peer sip:uri (if they ever got in contact with it) but only one of this entry is set to active
 *       - this function will return the status associated to the last updated entry without any consideration for the local users it is associated with
 * - any call where the SAS was neither accepted or rejected will not update the trust status but will set as active device for the peer sip:uri the one involved in the call
 *
 * This function is intended for use in a mono-device environment.
 *
 * @param[in]	dbPointer	Pointer to an already opened sqlite db
 * @param[in]	peerURI		The peer sip:uri we're interested in
 * @param[in]	zidCacheMutex	Points to a mutex used to lock zidCache database access, ignored if NULL
 *
 * @return one of:
 *  - BZRTP_CACHE_PEER_STATUS_UNKNOWN : this uri is not present in cache OR during calls with the active device, SAS never was validated or rejected
 *  	Note: once the SAS has been validated or rejected, the status will never return to UNKNOWN(unless you delete your cache)
 *  - BZRTP_CACHE_PEER_STATUS_VALID : the active device status is set to valid
 *  - BZRTP_CACHE_PEER_STATUS_INVALID : the active peer device status is set to invalid
 *
 */
BZRTP_EXPORT int bzrtp_cache_getPeerStatus_lock(void *dbPointer, const char *peerURI, bctbx_mutex_t *zidCacheMutex);

/**
 * @brief	Retrieve the name of the algo in string
 *
 * @param[in]	algo	Id of the algo
 *
 * @return	The of the algo in string
 */
BZRTP_EXPORT const char *bzrtp_algoToString(uint8_t algo);

/**
 * @brief set the maximum size of a ZRTP packet generated locally
 * MTU must be at least 600 bytes to avoid useless fragmentation of small packets
 *
 * @param[in]		zrtpContext		The ZRTP context we're dealing with
 * @param[in]		mtu 			The size in bytes of the maximum allowed for a ZRTP packet. If this parameter is less than 600, the actual MTU is set to 600
 *
 * @return 0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_set_MTU(bzrtpContext_t *zrtpContext, size_t mtu);

/**
 * @brief get the maximum size of a ZRTP packet generated locally
 *
 * @param[in]		zrtpContext		The ZRTP context we're dealing with
 *
 * @return the maximum size in bytes of a ZRTP packet generated locally
 */
BZRTP_EXPORT size_t bzrtp_get_MTU(bzrtpContext_t *zrtpContext);


/**
 * @brief Retrieve the list of available key agreements algorithms
 *
 * @param[in/out] algos		an array containing the list of available algorithms mapped on uint8
 * 				as defined in this header(ZRTP_KEYAGREEMENT_<ID>). Caller is responsible for the array allocation
 *
 * @return The number of available key agreement algorithms
 */
BZRTP_EXPORT uint8_t bzrtp_available_key_agreement(uint8_t availableTypes[256]);


/**
 * @brief check is Post Quantum algorithms are available
 * @return TRUE when PQ key exchange algorithms are available, FALSE otherwise
 */
BZRTP_EXPORT bool_t bzrtp_is_PQ_available(void);

/**
 * @brief Create a GoClear event and send it to the state machine
 * The user is in secure state.
 * He decided to change his encryption mode by clicking on a button for example.
 * The end point continues to send SRTP packets.
 * On ClearACK reception the end point deletes all key materials.
 *
 * @param[in] context	The ZRTP context we're dealing with
 * @param[in] selfSSRC	The SSRC identifying the channel
 *
 * @return one of :
 *  - BZRTP_ERROR_INVALIDCONTEXT : The context is invalid or the channel is not in secure state
 *  - Return value of the state machine
 */
BZRTP_EXPORT int bzrtp_sendGoClear(bzrtpContext_t *context, uint32_t selfSSRC);

/**
 * @brief Create a acceptGoClear event and send it to the state machine
 * The user received a valid GoClear packet and sent a ClearACK message (so the end point stops to send SRTP packets).
 * He agrees to change the encryption mode by clicking on a button for example.
 * The the sending of RTP packets may begin.
 *
 * @param[in] context	The ZRTP context we're dealing with
 * @param[in] selfSSRC	The SSRC identifying the channel
 *
 * @return one of :
 *  - BZRTP_ERROR_INVALIDCONTEXT : The context is invalid
 *  - Return value of the state machine
 */
BZRTP_EXPORT int bzrtp_confirmGoClear(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);

/**
 * @brief Create a BackToSecure event and send it to the state machine
 * The user has a clear channel.
 * He decided to resume the secure mode by clicking on a button for example.
 *
 * @param zrtpContext	The ZRTP context we're dealing with
 * @param selfSSRC		The SSRC identifying the channel
 *
 * @return
 *	- BZRTP_ERROR_INVALIDCONTEXT : The context is invalid
 *	- Return value of the state machine
 */
BZRTP_EXPORT int bzrtp_backToSecureMode(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);

#ifdef __cplusplus
}
#endif

#endif /* ifndef BZRTP_H */
