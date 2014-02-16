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
#include "packetParser.h"

#define BZRTP_ERROR_INVALIDCHANNELCONTEXT 0x8001
/* buffers allocation */

/* local functions */
int bzrtp_initChannelContext(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, uint32_t selfSSRC);
void bzrtp_destroyChannelContext(bzrtpContext_t *zrtpContext, uint8_t channelIndex);

/*
 * Create context structure and initialise it 
 * A channel context is created to, the selfSSRC is given to it
 *
 * @return The ZRTP engine context data
 *                                                                        
*/
bzrtpContext_t *bzrtp_createBzrtpContext(uint32_t selfSSRC)
{
	int i;
	/*** create and intialise the context structure ***/
	bzrtpContext_t *context = malloc(sizeof(bzrtpContext_t));
	memset(context, 0, sizeof(bzrtpContext_t));

	/* start the random number generator */
	context->RNGContext = bzrtpCrypto_startRNG(NULL, 0); /* TODO: give a seed for the RNG? */
	/* set the DHM context to NULL, it will be created if needed when creating a DHPart packet */
	context->DHMContext = NULL;

	/* set flags */
	context->isSecure = 0; /* start unsecure */
	context->peerSupportMultiChannel = 0; /* peer does not support Multichannel by default */

	/* set to NULL all callbacks pointer */
	context->zrtpCallbacks.bzrtp_readCache = NULL;
	context->zrtpCallbacks.bzrtp_writeCache = NULL;
	context->zrtpCallbacks.bzrtp_setCachePosition = NULL;
	context->zrtpCallbacks.bzrtp_getCachePosition = NULL;
	
	/* allocate 1 channel context, set all the others pointers to NULL */
	context->channelContext[0] = (bzrtpChannelContext_t *)malloc(sizeof(bzrtpChannelContext_t));
	memset(context->channelContext[0], 0, sizeof(bzrtpChannelContext_t));
	bzrtp_initChannelContext(context, context->channelContext[0], selfSSRC);

	for (i=1; i<ZRTP_MAX_CHANNEL_NUMBER; i++) {
		context->channelContext[i] = NULL;
	}

	/* get the list of crypto algorithms provided by the crypto module */
	/* this list may then be updated according to users settings */
	context->hc = bzrtpCrypto_getAvailableCryptoTypes(ZRTP_HASH_TYPE, context->supportedHash);
	context->cc = bzrtpCrypto_getAvailableCryptoTypes(ZRTP_CIPHERBLOCK_TYPE, context->supportedCipher);
	context->ac = bzrtpCrypto_getAvailableCryptoTypes(ZRTP_AUTHTAG_TYPE, context->supportedAuthTag);
	context->kc = bzrtpCrypto_getAvailableCryptoTypes(ZRTP_KEYAGREEMENT_TYPE, context->supportedKeyAgreement);
	context->sc = bzrtpCrypto_getAvailableCryptoTypes(ZRTP_SAS_TYPE, context->supportedSas);

	/* initialise cached secret buffer to null */
	context->cachedSecret.rs1 = NULL;
	context->cachedSecret.rs1Length = 0;
	context->cachedSecret.rs2 = NULL;
	context->cachedSecret.rs2Length = 0;
	context->cachedSecret.pbxsecret = NULL;
	context->cachedSecret.pbxsecretLength = 0;
	context->cachedSecret.auxsecret = NULL;
	context->cachedSecret.auxsecretLength = 0;

	
	/* initialise key buffers */
	context->ZRTPSess = NULL;
	context->ZRTPSessLength = 0;

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
	int i;
	/* destroy contexts */
	bzrtpCrypto_DestroyDHMContext(context->DHMContext);
	context->DHMContext = NULL;


	/* destroy channel contexts */
	for (i=0; i<ZRTP_MAX_CHANNEL_NUMBER; i++) {
		bzrtp_destroyChannelContext(context, i);
		context->channelContext[i] = NULL;
	}

	/* free allocated buffers */
	free(context->cachedSecret.rs1); 
	free(context->cachedSecret.rs2);
	free(context->cachedSecret.pbxsecret);
	free(context->cachedSecret.auxsecret);
	free(context->ZRTPSess);

	context->cachedSecret.rs1=NULL; 
	context->cachedSecret.rs2=NULL;
	context->cachedSecret.pbxsecret=NULL;
	context->cachedSecret.auxsecret=NULL;
	context->ZRTPSess=NULL;

	
	/* destroy the RNG context at the end because it may be needed to destroy some keys */
	bzrtpCrypto_destroyRNG(context->RNGContext);
	context->RNGContext = NULL;
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


int bzrtp_addChannel(bzrtpContext_t *zrtpContext, uint32_t selfSSRC) {
	/* is zrtp context valid */
	if (zrtpContext==NULL) {
		return BZRTP_ERROR_INVALIDCONTEXT;
	}

	/* is ZRTP context able to add a channel (means channel 0 has already performed the secrets generation) */
	if (zrtpContext->isSecure == 0) {
		return BZRTP_ERROR_CONTEXTNOTREADY;
	}

	/* check the peer support Multichannel(shall be set in the first Hello message received) */
	if (zrtpContext->peerSupportMultiChannel == 0) {
		return BZRTP_ERROR_MULTICHANNELNOTSUPPORTEDBYPEER;
	}

	/* get the first free channel context from ZRTP context and create a channel context */
	bzrtpChannelContext_t *zrtpChannelContext = NULL;
	int i=0;

	while(i<ZRTP_MAX_CHANNEL_NUMBER && zrtpChannelContext==NULL) {
		if (zrtpContext->channelContext[i] == NULL) {
			int retval;
			zrtpChannelContext = (bzrtpChannelContext_t *)malloc(sizeof(bzrtpChannelContext_t));
			memset(zrtpChannelContext, 0, sizeof(bzrtpChannelContext_t));
			retval = bzrtp_initChannelContext(zrtpContext, zrtpChannelContext, selfSSRC);
			if (retval != 0) {
				free(zrtpChannelContext);
				return retval;
			}
		} else {
			i++;
		}
	}

	if (zrtpChannelContext == NULL) {
		return BZRTP_ERROR_UNABLETOADDCHANNEL;
	}

	/* attach the created channel to the ZRTP context */
	zrtpContext->channelContext[i] = zrtpChannelContext;
	printf ("Added channel index %d\n", i);

	return 0;

}
/* Local functions implementation */
/**
 * @brief Initialise the context of a channel
 * Initialise some vectors
 * 
 * @param[in] 		zrtpContext			The zrtpContext hosting this channel, needed to acces the RNG
 * @param[out]		zrtpChanneContext	The channel context to be initialised
 * @param[in]		selfSSRC			The SSRC allocated to this channel
 *
 * @return	0 on success, error code otherwise
 */
int bzrtp_initChannelContext(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, uint32_t selfSSRC) {
	int i;
	if (zrtpChannelContext == NULL) {
		return BZRTP_ERROR_INVALIDCHANNELCONTEXT;
	}

	/* the state machine is not started at the creation of the channel but on explicit call to the start function */
	zrtpChannelContext->stateMachine = NULL;

	zrtpChannelContext->selfSSRC = selfSSRC;

	/* initialise as initiator, switch to responder later if needed */
	zrtpChannelContext->role = INITIATOR;

	/* create H0 (32 bytes random) and derive using implicit Hash(SHA256) H1,H2,H3 */
	bzrtpCrypto_getRandom(zrtpContext->RNGContext, zrtpChannelContext->selfH[0], 32);
	bzrtpCrypto_sha256(zrtpChannelContext->selfH[0], 32, 32, zrtpChannelContext->selfH[1]);
	bzrtpCrypto_sha256(zrtpChannelContext->selfH[1], 32, 32, zrtpChannelContext->selfH[2]);
	bzrtpCrypto_sha256(zrtpChannelContext->selfH[2], 32, 32, zrtpChannelContext->selfH[3]);

	/* initialisation of packet storage */
	for (i=0; i<PACKET_STORAGE_CAPACITY; i++) {
		zrtpChannelContext->selfPackets[i] = NULL;
		zrtpChannelContext->peerPackets[i] = NULL;
	}

	/* initialise the self Sequence number to a random and peer to 0 */
	bzrtpCrypto_getRandom(zrtpContext->RNGContext, (uint8_t *)&(zrtpChannelContext->selfSequenceNumber), 2);
	zrtpChannelContext->selfSequenceNumber &= 0x0FFF; /* first 4 bits to zero in order to avoid reaching FFFF and turning back to 0 */
	zrtpChannelContext->selfSequenceNumber++; /* be sure it is not initialised to 0 */
	zrtpChannelContext->peerSequenceNumber = 0;

	/* reset choosen algo and their functions */
	zrtpChannelContext->hashAlgo = ZRTP_UNSET_ALGO;
	zrtpChannelContext->cipherAlgo = ZRTP_UNSET_ALGO;
	zrtpChannelContext->authTagAlgo = ZRTP_UNSET_ALGO;
	zrtpChannelContext->keyAgreementAlgo = ZRTP_UNSET_ALGO;
	zrtpChannelContext->sasAlgo = ZRTP_UNSET_ALGO;

	updateCryptoFunctionPointers(zrtpChannelContext);

	/* initialise key buffers */
	zrtpChannelContext->s0 = NULL;
	zrtpChannelContext->KDFContext = NULL;
	zrtpChannelContext->KDFContextLength = 0;
	zrtpChannelContext->mackeyi = NULL;
	zrtpChannelContext->mackeyr = NULL;
	zrtpChannelContext->zrtpkeyi = NULL;
	zrtpChannelContext->zrtpkeyr = NULL;

	return 0;
}

/**
 * @brief Destroy the context of a channel
 * Free allocated buffers, destroy keys
 * 
 * @param[in] 		zrtpContext			The zrtpContext hosting this channel
 * @param[in/out]	channelIndex		The index of the channel to be freed in the ZRTP context channel array
 */
void bzrtp_destroyChannelContext(bzrtpContext_t *zrtpContext, uint8_t channelIndex) {
	int i;
	
	if (zrtpContext == NULL) {
		return;
	}

	bzrtpChannelContext_t *zrtpChannelContext = zrtpContext->channelContext[channelIndex];

	/* check there is something to be freed */
	if (zrtpChannelContext == NULL) {
		return;
	}

	/* destroy and free the key buffers */
	bzrtp_DestroyKey(zrtpChannelContext->s0, zrtpChannelContext->hashLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->mackeyi, zrtpChannelContext->hashLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->mackeyr, zrtpChannelContext->hashLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->zrtpkeyi, zrtpChannelContext->cipherKeyLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->zrtpkeyr, zrtpChannelContext->cipherKeyLength, zrtpContext->RNGContext);

	free(zrtpChannelContext->s0);
	free(zrtpChannelContext->KDFContext);
	free(zrtpChannelContext->mackeyi);
	free(zrtpChannelContext->mackeyr);
	free(zrtpChannelContext->zrtpkeyi);
	free(zrtpChannelContext->zrtpkeyr);

	zrtpChannelContext->s0=NULL;
	zrtpChannelContext->KDFContext=NULL;
	zrtpChannelContext->mackeyi=NULL;
	zrtpChannelContext->mackeyr=NULL;
	zrtpChannelContext->zrtpkeyi=NULL;
	zrtpChannelContext->zrtpkeyr=NULL;

	/* free the allocated buffers */
	for (i=0; i<PACKET_STORAGE_CAPACITY; i++) {
		free(zrtpChannelContext->selfPackets[i]);
		free(zrtpChannelContext->peerPackets[i]);
		zrtpChannelContext->selfPackets[i] = NULL;
		zrtpChannelContext->peerPackets[i] = NULL;
	}
}


/* here are the state functions for the state machine */
/**
 * brief This is the initial state
 * On first call, we will create the Hello message and start sending it until we receive an helloACK and a hello message from peer
 * if we receive an Hello message*/
void state_discovery(bzrtp_event_t event) {
};
