/**
 @file bzrtp.c

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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "bzrtp/bzrtp.h"

#include "typedef.h"
#include "cryptoWrapper.h"
#include "cryptoUtils.h"
#include "zidCache.h"

/* buffers allocation */

/* internal functions */

/**
 * Create context structure and initialise it    
 * @return The ZRTP engine context data
 *                                                                        
*/
bzrtpContext_t *bzrtp_createBzrtpContext()
{
	int i;
	/*** create and intialise the context structure ***/
	bzrtpContext_t *context = malloc(sizeof(bzrtpContext_t));
	memset(context, 0, sizeof(bzrtpContext_t));

	/* start the random number generator */
	context->RNGContext = bzrtpCrypto_startRNG(NULL, 0); /* TODO: give a seed for the RNG? */
	/* set the DHM context to NULL, it will be created if needed when creating a DHPart packet */
	context->DHMContext = NULL;

	/* set to NULL all callbacks pointer */
	context->zrtpCallbacks.bzrtp_readCache = NULL;
	context->zrtpCallbacks.bzrtp_writeCache = NULL;
	context->zrtpCallbacks.bzrtp_setCachePosition = NULL;
	context->zrtpCallbacks.bzrtp_getCachePosition = NULL;
	
	/* initialise as initiator, switch to responder later if needed */
	context->role = INITIATOR;

	/* get the list of crypto algorithms provided by the crypto module */
	/* this list may then be updated according to users settings */
	context->hc = bzrtpCrypto_getAvailableCryptoTypes(ZRTP_HASH_TYPE, context->supportedHash);
	context->cc = bzrtpCrypto_getAvailableCryptoTypes(ZRTP_CIPHERBLOCK_TYPE, context->supportedCipher);
	context->ac = bzrtpCrypto_getAvailableCryptoTypes(ZRTP_AUTHTAG_TYPE, context->supportedAuthTag);
	context->kc = bzrtpCrypto_getAvailableCryptoTypes(ZRTP_KEYAGREEMENT_TYPE, context->supportedKeyAgreement);
	context->sc = bzrtpCrypto_getAvailableCryptoTypes(ZRTP_SAS_TYPE, context->supportedSas);

	/* reset choosen algo and their functions */
	context->hashAlgo = ZRTP_UNSET_ALGO;
	context->cipherAlgo = ZRTP_UNSET_ALGO;
	context->authTagAlgo = ZRTP_UNSET_ALGO;
	context->keyAgreementAlgo = ZRTP_UNSET_ALGO;
	context->sasAlgo = ZRTP_UNSET_ALGO;

	updateCryptoFunctionPointers(context);

	/* initialise cached secret buffer to null */
	context->cachedSecret.rs1 = NULL;
	context->cachedSecret.rs1Length = 0;
	context->cachedSecret.rs2 = NULL;
	context->cachedSecret.rs2Length = 0;
	context->cachedSecret.pbxsecret = NULL;
	context->cachedSecret.pbxsecretLength = 0;
	context->cachedSecret.auxsecret = NULL;
	context->cachedSecret.auxsecretLength = 0;

	/* create H0 (32 bytes random) and derive using implicit Hash(SHA256) H1,H2,H3 */
	bzrtpCrypto_getRandom(context->RNGContext, context->selfH[0], 32);
	bzrtpCrypto_sha256(context->selfH[0], 32, 32, context->selfH[1]);
	bzrtpCrypto_sha256(context->selfH[1], 32, 32, context->selfH[2]);
	bzrtpCrypto_sha256(context->selfH[2], 32, 32, context->selfH[3]);
	
	/* initialise the self Sequence number to a random and peer to 0
	 * (which may lead to reject the first hello packet if peer initialise his sequence number to 0) */
	bzrtpCrypto_getRandom(context->RNGContext, (uint8_t *)&(context->selfSequenceNumber), 2);
	context->selfSequenceNumber = context->selfSequenceNumber>>4; /* first 4 bits to zero in order to avoid reaching FFFF and turning back to 0 */
	context->peerSequenceNumber = 0;

	/* initialisation of packet storage */
	for (i=0; i<PACKET_STORAGE_CAPACITY; i++) {
		context->selfPackets[i] = NULL;
		context->peerPackets[i] = NULL;
	}

	/* initialise key buffers */
	context->s0 = NULL;
	context->KDFContext = NULL;
	context->KDFContextLength = 0;
	context->ZRTPSess = NULL;
	context->ZRTPSessLength = 0;
	context->mackeyi = NULL;
	context->mackeyr = NULL;
	context->zrtpkeyi = NULL;
	context->zrtpkeyr = NULL;

	return context;
}

/**
 * @brief Perform some initialisation which can't be done without some callback functions:
 * - get ZID
 *
 *   @param[in] 	context	The context to initialise
 */
void bzrtp_initBzrtpContext(bzrtpContext_t *context) {

	/* initialise ZID. Randomly generated if no ZID is found in cache */
	getSelfZID(context, context->selfZID);
}

/**
 * Free memory of context structure              
 * @param[in] context BZRtp context to be destroyed.
 *                                                                           
*/
void bzrtp_destroyBzrtpContext(bzrtpContext_t *context)
{
	/* destroy contexts */
	bzrtpCrypto_destroyRNG(context->RNGContext);
	bzrtpCrypto_DestroyDHMContext(context->DHMContext);


	/* free allocated buffers */
	free(context->cachedSecret.rs1); 
	free(context->cachedSecret.rs2);
	free(context->cachedSecret.pbxsecret);
	free(context->cachedSecret.auxsecret);

	int i;
	for (i=0; i<PACKET_STORAGE_CAPACITY; i++) {
		free(context->selfPackets[i]);
		free(context->peerPackets[i]);
	}

	free(context->s0);
	free(context->KDFContext);
	free(context->ZRTPSess);
	free(context->mackeyi);
	free(context->mackeyr);
	free(context->zrtpkeyi);
	free(context->zrtpkeyr);
	
	free(context);
	return;
}

/**
 * @brief Allocate a function pointer to the callback function identified by his id 
 * @param[in] functionPointer 	The pointer to the function to bind as callback.
 * @param[in] functionID		The ID as defined above to identify which callback to be set.
 *                                                                           
 * @return 0 on success
 *                                                                           
*/
int bzrtp_setCallback(bzrtpContext_t *context, int (*functionPointer)(), uint16_t functionID) {
	switch (functionID) {
		case ZRTP_CALLBACK_READCACHE:
			context->zrtpCallbacks.bzrtp_readCache = (int (*)(uint8_t *, uint16_t))functionPointer;
			break;
		case ZRTP_CALLBACK_WRITECACHE: 
			context->zrtpCallbacks.bzrtp_writeCache = (int (*)(uint8_t *, uint16_t))functionPointer;
			break;
		case ZRTP_CALLBACK_SETCACHEPOSITION: 
			context->zrtpCallbacks.bzrtp_setCachePosition = (int (*)(long))functionPointer;
			break;
		case ZRTP_CALLBACK_GETCACHEPOSITION: 
			context->zrtpCallbacks.bzrtp_getCachePosition = (int (*)(long *))functionPointer;
			break;
		default:
			return BZRTP_ERROR_INVALIDCALLBACKID; 
			break;
	}
	return 0;
}
