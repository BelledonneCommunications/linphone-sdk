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
typedef struct bzrtpContext_struct bzrtpContext;
#include <stdint.h>

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
} bzrtpSrtpSecrets_t;

#define ZRTP_MAGIC_COOKIE 0x5a525450
#define ZRTP_VERSION	"1.10"
/*#define ZRTP_CLIENT_IDENTIFIER "LINPHONEZRTP0.01"*/
#define ZRTP_CLIENT_IDENTIFIER "LINPHONE-ZRTPCPP"

/* error code definition */
#define BZRTP_ERROR_INVALIDCALLBACKID	0x0001
/**
 * @brief bzrtpContext_t The ZRTP engine context
 * Store current state, timers, HMAC and encryption keys
*/
typedef struct bzrtpContext_struct bzrtpContext_t;

/**
 * Create context structure and initialise it    
 * @return The ZRTP engine context data
 *                                                                        
*/
__attribute__ ((visibility ("default"))) bzrtpContext_t *bzrtp_createBzrtpContext();

/**
 * @brief Perform some initialisation which can't be done without some callback functions:
 * - get ZID
 *
 *   @param[in] 	context	The context to initialise
 */
__attribute__ ((visibility ("default"))) void bzrtp_initBzrtpContext(bzrtpContext_t *context); 

/**
 * Free memory of context structure              
 * @param[in] context BZRtp context to be destroyed.
 *                                                                           
*/
__attribute__ ((visibility ("default"))) void bzrtp_destroyBzrtpContext(bzrtpContext_t *context);

#define ZRTP_CALLBACK_READCACHE					0x0101
#define ZRTP_CALLBACK_WRITECACHE				0x0102
#define ZRTP_CALLBACK_SETCACHEPOSITION			0x0104
#define ZRTP_CALLBACK_GETCACHEPOSITION			0x0108
/**
 * @brief Allocate a function pointer to the callback function identified by his id 
 * @param[in] functionPointer 	The pointer to the function to bind as callback.
 * @param[in] functionID		The ID as defined above to identify which callback to be set.
 *
 * @return 0 on success
 *                                                                           
*/
__attribute__ ((visibility ("default"))) int bzrtp_setCallback(bzrtpContext_t *context, int (*functionPointer)(), uint16_t functionID);

#endif /* ifndef BZRTP_H */
