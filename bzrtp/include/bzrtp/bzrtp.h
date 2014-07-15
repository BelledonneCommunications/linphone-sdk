/**
 @file bzrtp.h

 @brief Public entry points to the ZRTP implementation

 @author Johan Pascal

 @copyright Copyright (C) 2014 Belledonne Communications, Grenoble, France
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef BZRTP_H
#define BZRTP_H

#include <stdint.h>

#ifdef _MSC_VER
#define BZRTP_EXPORT __declspec(dllexport)
#else
#define BZRTP_EXPORT __attribute__ ((visibility ("default")))
#endif

/**
 * Some defines used internally by zrtp but also needed by client to interpretate the cipher block and auth tag algorithms used by srtp */
#define ZRTP_UNSET_ALGO			0x00

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
 * Define to give client indication on which srtp secrets are valid when given
 */
#define ZRTP_SRTP_SECRETS_FOR_SENDER	0x01
#define ZRTP_SRTP_SECRETS_FOR_RECEIVER	0x02

/**
 * brief The data structure containing the keys and algorithms to be used by srtp */
typedef struct bzrtpSrtpSecrets_struct  {
	uint8_t *selfSrtpKey; /**< The key used by local part to encrypt */
	uint8_t selfSrtpKeyLength; /**< The length in byte of the key */
	uint8_t *selfSrtpSalt; /**< The salt used by local part to encrypt */
	uint8_t selfSrtpSaltLength; /**< The length in byte of the salt */
	uint8_t *peerSrtpKey; /**< The key used by local part to decrypt */
	uint8_t peerSrtpKeyLength; /**< The length in byte of the key */
	uint8_t *peerSrtpSalt; /**< The salt used by local part to decrypt */
	uint8_t peerSrtpSaltLength; /**< The length in byte of the salt */
	uint8_t cipherAlgo; /**< The cipher block algorithm used by srtp */
	uint8_t cipherKeyLength; /**< The key length in bytes for the cipher block algorithm used by srtp */
	uint8_t authTagAlgo; /**< srtp authentication tag algorithm agreed on after Hello packet exchange */
	char *sas; /**< a null terminated char containing the Short Authentication String */
	uint8_t sasLength; /**< The lenght of sas, including the termination character */
} bzrtpSrtpSecrets_t;

#define ZRTP_MAGIC_COOKIE 0x5a525450
#define ZRTP_VERSION	"1.10"
/*#define ZRTP_CLIENT_IDENTIFIER "LINPHONEZRTP0.01"*/
#define ZRTP_CLIENT_IDENTIFIER "LINPHONE-ZRTPCPP"

/* error code definition */
#define BZRTP_ERROR_INVALIDCALLBACKID				0x0001
#define	BZRTP_ERROR_CONTEXTNOTREADY					0x0002
#define BZRTP_ERROR_INVALIDCONTEXT					0x0004
#define BZRTP_ERROR_MULTICHANNELNOTSUPPORTEDBYPEER	0x0008
#define BZRTP_ERROR_UNABLETOADDCHANNEL				0x0010
#define BZRTP_ERROR_UNABLETOSTARTCHANNEL			0x0020
/**
 * @brief bzrtpContext_t The ZRTP engine context
 * Store current state, timers, HMAC and encryption keys
*/
typedef struct bzrtpContext_struct bzrtpContext_t;

/**
 * Create context structure and initialise it
 * A channel context is created when creating the zrtp context.
 *
 * @param[in]	selfSSRC	The SSRC given to the channel context created within the zrtpContext
 *
 * @return The ZRTP engine context data
 *                                                                        
*/
BZRTP_EXPORT bzrtpContext_t *bzrtp_createBzrtpContext(uint32_t selfSSRC);

/**
 * @brief Perform some initialisation which can't be done without some callback functions:
 * - get ZID
 *
 *   @param[in] 	context	The context to initialise
 */
BZRTP_EXPORT void bzrtp_initBzrtpContext(bzrtpContext_t *context); 

/**
 * Free memory of context structure to a channel, if all channels are freed, free the global zrtp context
 * @param[in]	context		Context hosting the channel to be destroyed.(note: the context zrtp context itself is destroyed with the last channel)
 * @param[in]	selfSSRC	The SSRC identifying the channel to be destroyed
 *                                                                           
*/
BZRTP_EXPORT void bzrtp_destroyBzrtpContext(bzrtpContext_t *context, uint32_t selfSSRC);

#define ZRTP_CALLBACK_LOADCACHE						0x0101
#define ZRTP_CALLBACK_WRITECACHE					0x0102
#define ZRTP_CALLBACK_SENDDATA						0x0110
#define ZRTP_CALLBACK_SRTPSECRETSAVAILABLE			0x0120
#define ZRTP_CALLBACK_STARTSRTPSESSION				0x0140
#define ZRTP_CALLBACK_CONTEXTREADYFOREXPORTEDKEYS	0x0180
/**
 * @brief Allocate a function pointer to the callback function identified by his id 
 * @param[in/out]	context				The zrtp context to set the callback function
 * @param[in] 		functionPointer 	The pointer to the function to bind as callback.
 * @param[in] 		functionID			The ID as defined above to identify which callback to be set.
 *
 * @return 0 on success
 *                                                                           
*/
BZRTP_EXPORT int bzrtp_setCallback(bzrtpContext_t *context, int (*functionPointer)(), uint16_t functionID);

/**
 * @brief Set the client data pointer in a channel context
 * This pointer is returned to the client by the callbacks function, used to store associated contexts (RTP session)
 * @param[in/out]	zrtpContext		The ZRTP context we're dealing with
 * @param[in]		selfSSRC		The SSRC identifying the channel to be linked to the client Data
 * @param[in]		clientData		The clientData pointer, casted to a (void *)
 *
 * @return 0 on success
 *                                                                           
*/
BZRTP_EXPORT int bzrtp_setClientData(bzrtpContext_t *zrtpContext, uint32_t selfSSRC, void *clientData);

/**
 * @brief Add a channel to an existing context, this can be done only if the first channel has concluded a DH key agreement
 *
 * @param[in/out]	zrtpContext	The zrtp context who will get the additionnal channel. Must be in secure state.
 * @param[in]		selfSSRC	The SSRC given to the channel context
 *
 * @return 0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_addChannel(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);


/**
 * @brief Start the state machine of the specified channel
 *
 * @param[in/out]	zrtpContext			The ZRTP context hosting the channel to be started
 * @param[in]		selfSSRC			The SSRC identifying the channel to be started(will start sending Hello packets and listening for some)
 *
 * @return			0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_startChannelEngine(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);

/**
 * @brief Send the current time to a specified channel, it will check if it has to trig some timer
 *
 * @param[in/out]	zrtpContext			The ZRTP context hosting the channel
 * @param[in]		selfSSRC			The SSRC identifying the channel
 * @param[in]		timeReference		The current time in ms
 *
 * @return			0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_iterate(bzrtpContext_t *zrtpContext, uint32_t selfSSRC, uint64_t timeReference);


/**
 * @brief Return the status of current channel, 1 if SRTP secrets have been computed and confirmed, 0 otherwise
 * 
 * @param[in]		zrtpContext			The ZRTP context hosting the channel
 * @param[in]		selfSSRC			The SSRC identifying the channel
 *
 * @return			0 if this channel is not ready to secure SRTP communication, 1 if it is ready
 */
BZRTP_EXPORT int bzrtp_isSecure(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);


/**
 * @brief Process a received message
 *
 * @param[in/out]	zrtpContext				The ZRTP context we're dealing with
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
 * @param[in/out]	zrtpContext				The ZRTP context we're dealing with
 */
BZRTP_EXPORT void bzrtp_SASVerified(bzrtpContext_t *zrtpContext); 

/**
 * @brief Called by user when the SAS has been set to unverified
 *
 * @param[in/out]	zrtpContext				The ZRTP context we're dealing with
 */
BZRTP_EXPORT void bzrtp_resetSASVerified(bzrtpContext_t *zrtpContext);

/**
 * @brief Reset the retransmission timer of a given channel.
 * Packets will be sent again if appropriate:
 *  - when in responder role, zrtp engine only answer to packets sent by the initiator.
 *  - if we are still in discovery phase, Hello or Commit packets will be resent.
 *
 * @param[in/out]	zrtpContext				The ZRTP context we're dealing with
 * @param[in]		selfSSRC				The SSRC identifying the channel to reset
 *
 * return 0 on success, error code otherwise
 */
BZRTP_EXPORT int bzrtp_resetRetransmissionTimer(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);

#define BZRTP_CUSTOMCACHE_USEKDF 	1
#define BZRTP_CUSTOMCACHE_PLAINDATA 0

#define BZRTP_CACHE_LOADFILE		0x01
#define BZRTP_CACHE_DONTLOADFILE	0x00
#define BZRTP_CACHE_WRITEFILE		0x10
#define BZRTP_CACHE_DONTWRITEFILE	0x00

/* role mapping */
#define INITIATOR	0
#define	RESPONDER	1
/**
 * @brief Allow client to write data in cache in the current <peer> tag.
 * Data can be written directly or ciphered using the ZRTP Key Derivation Function and current s0.
 * If useKDF flag is set but no s0 is available, nothing is written in cache and an error is returned
 *
 * @param[in/out]	zrtpContext			The ZRTP context we're dealing with
 * @param[in]		peerZID				The ZID identifying the peer node we want to write into
 * @param[in]		tagName				The name of the tag to be written
 * @param[in]		tagNameLength		The length in bytes of the tagName
 * @param[in]		tagContent			The content of the tag to be written(a string, if KDF is used the result will be turned into an hexa string)
 * @param[in]		tagContentLength	The length in bytes of tagContent
 * @param[in]		derivedDataLength	Used only in KDF mode, length in bytes of the derived data to use (max 32)
 * @param[in]		useKDF				A flag, if set to 0, write data as it is provided, if set to 1, write KDF(s0, "tagContent", KDF_Context, negotiated hash lenght)
 * @param[in]		fileFlag			Flag, if LOADFILE bit is set, reload the cache buffer from file before updating.
 * 										if WRITEFILE bit is set, update the cache file
 *
 * @return	0 on success, errorcode otherwise
 */
BZRTP_EXPORT int bzrtp_addCustomDataInCache(bzrtpContext_t *zrtpContext, uint8_t peerZID[12], uint8_t *tagName, uint16_t tagNameLength, uint8_t *tagContent, uint16_t tagContentLength, uint8_t derivedDataLength, uint8_t useKDF, uint8_t fileFlag);

#endif /* ifndef BZRTP_H */
