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

#include "bzrtp/bzrtp.h"

#include "typedef.h"
#include "bctoolbox/crypto.h"
#include "cryptoUtils.h"
#include "zidCache.h"
#include "packetParser.h"
#include "stateMachine.h"

#define BZRTP_ERROR_INVALIDCHANNELCONTEXT 0x8001

/* local functions */
int bzrtp_initChannelContext(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, uint32_t selfSSRC);
void bzrtp_destroyChannelContext(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext);
bzrtpChannelContext_t *getChannelContext(bzrtpContext_t *zrtpContext, uint32_t selfSSRC);

/*
 * Create context structure and initialise it 
 * A channel context is created to, the selfSSRC is given to it
 *
 * @return The ZRTP engine context data
 *                                                                        
*/
bzrtpContext_t *bzrtp_createBzrtpContext(uint32_t selfSSRC) {
	int i;
	/*** create and intialise the context structure ***/
	bzrtpContext_t *context = malloc(sizeof(bzrtpContext_t));
	memset(context, 0, sizeof(bzrtpContext_t));

	/* start the random number generator */
	context->RNGContext = bctoolbox_rng_context_new(); /* TODO: give a seed for the RNG? */
	/* set the DHM context to NULL, it will be created if needed when creating a DHPart packet */
	context->DHMContext = NULL;

	/* set flags */
	context->isSecure = 0; /* start unsecure */
	context->peerSupportMultiChannel = 0; /* peer does not support Multichannel by default */

	/* set to NULL all callbacks pointer */
	context->zrtpCallbacks.bzrtp_loadCache = NULL;
	context->zrtpCallbacks.bzrtp_writeCache = NULL;
	context->zrtpCallbacks.bzrtp_sendData = NULL;
	context->zrtpCallbacks.bzrtp_srtpSecretsAvailable = NULL;
	context->zrtpCallbacks.bzrtp_startSrtpSession = NULL;
	context->zrtpCallbacks.bzrtp_contextReadyForExportedKeys = NULL;
	
	/* allocate 1 channel context, set all the others pointers to NULL */
	context->channelContext[0] = (bzrtpChannelContext_t *)malloc(sizeof(bzrtpChannelContext_t));
	memset(context->channelContext[0], 0, sizeof(bzrtpChannelContext_t));
	bzrtp_initChannelContext(context, context->channelContext[0], selfSSRC);

	for (i=1; i<ZRTP_MAX_CHANNEL_NUMBER; i++) {
		context->channelContext[i] = NULL;
	}

	/* get the list of crypto algorithms provided by the crypto module */
	/* this list may then be updated according to users settings */
	context->hc = bzrtpUtils_getAvailableCryptoTypes(ZRTP_HASH_TYPE, context->supportedHash);
	context->cc = bzrtpUtils_getAvailableCryptoTypes(ZRTP_CIPHERBLOCK_TYPE, context->supportedCipher);
	context->ac = bzrtpUtils_getAvailableCryptoTypes(ZRTP_AUTHTAG_TYPE, context->supportedAuthTag);
	context->kc = bzrtpUtils_getAvailableCryptoTypes(ZRTP_KEYAGREEMENT_TYPE, context->supportedKeyAgreement);
	context->sc = bzrtpUtils_getAvailableCryptoTypes(ZRTP_SAS_TYPE, context->supportedSas);

	/* initialise cached secret buffer to null */
#ifdef HAVE_LIBXML2
	context->cacheBuffer = NULL; /* this field is present only if cache is compiled*/
#endif
	context->cachedSecret.rs1 = NULL;
	context->cachedSecret.rs1Length = 0;
	context->cachedSecret.rs2 = NULL;
	context->cachedSecret.rs2Length = 0;
	context->cachedSecret.pbxsecret = NULL;
	context->cachedSecret.pbxsecretLength = 0;
	context->cachedSecret.auxsecret = NULL;
	context->cachedSecret.auxsecretLength = 0;
	context->cacheMismatchFlag = 0;
	
	/* initialise key buffers */
	context->ZRTPSess = NULL;
	context->ZRTPSessLength = 0;

	return context;
}

/**
 * @brief Perform some initialisation which can't be done without some callback functions:
 *  This function is called once per session when the first channel is created.
 *  It must be called after the cache callback function have been set
 *  - load cache
 *	- get ZID from cache or generate it
 *
 *	@param[in] 	context		The context to initialise
 */
void bzrtp_initBzrtpContext(bzrtpContext_t *context) {

	/* initialise ZID. Randomly generated if no ZID is found in cache or no cache found */
	/* This call will load the cache or create it if the cache callback functions are not null*/
	bzrtp_getSelfZID(context, context->selfZID);
}

/*
 * Free memory of context structure to a channel, if all channels are freed, free the global zrtp context
 * @param[in]	context		Context hosting the channel to be destroyed.(note: the context zrtp context itself is destroyed with the last channel)
 * @param[in]	selfSSRC	The SSRC identifying the channel to be destroyed
 *                                                                           
*/
void bzrtp_destroyBzrtpContext(bzrtpContext_t *context, uint32_t selfSSRC) {
	int i;
	int validChannelsNumber = 0;

	if (context == NULL) {
		return;
	}

	/* Find the channel to be destroyed, destroy it and check if we have anymore valid channels */
	for (i=0; i<ZRTP_MAX_CHANNEL_NUMBER; i++) {
		if (context->channelContext[i] != NULL) {
			if (context->channelContext[i]->selfSSRC == selfSSRC) {
				bzrtp_destroyChannelContext(context, context->channelContext[i]);
				context->channelContext[i] = NULL;
			} else {
				validChannelsNumber++;
			}
		}
	}

	if (validChannelsNumber>0) {
		return; /* we have more valid channels, keep the zrtp context */
	}


	/* We have no more channel, destroy the zrtp context */
	/* DHM context shall already been destroyed after s0 computation, but just in case */
	if (context->DHMContext != NULL) {
		bctoolbox_DestroyDHMContext(context->DHMContext);
		context->DHMContext = NULL;
	}

	/* Destroy keys and secrets */
	/* rs1, rs2, pbxsecret and auxsecret shall already been destroyed, just in case */
	if (context->cachedSecret.rs1!=NULL) {
		bzrtp_DestroyKey(context->cachedSecret.rs1, context->cachedSecret.rs1Length, context->RNGContext);
		free(context->cachedSecret.rs1);
		context->cachedSecret.rs1 = NULL;
	}
	if (context->cachedSecret.rs2!=NULL) {
		bzrtp_DestroyKey(context->cachedSecret.rs2, context->cachedSecret.rs2Length, context->RNGContext);
		free(context->cachedSecret.rs2);
		context->cachedSecret.rs2 = NULL;
	}
	if (context->cachedSecret.auxsecret!=NULL) {
		bzrtp_DestroyKey(context->cachedSecret.auxsecret, context->cachedSecret.auxsecretLength, context->RNGContext);
		free(context->cachedSecret.auxsecret);
		context->cachedSecret.auxsecret = NULL;
	}
	if (context->cachedSecret.pbxsecret!=NULL) {
		bzrtp_DestroyKey(context->cachedSecret.pbxsecret, context->cachedSecret.pbxsecretLength, context->RNGContext);
		free(context->cachedSecret.pbxsecret);
		context->cachedSecret.pbxsecret = NULL;
	}

	if (context->ZRTPSess!=NULL) {
		bzrtp_DestroyKey(context->ZRTPSess, context->ZRTPSessLength, context->RNGContext);
		free(context->ZRTPSess);
		context->ZRTPSess=NULL;
	}

#ifdef HAVE_LIBXML2
	xmlFreeDoc(context->cacheBuffer);
	context->cacheBuffer=NULL;
#endif
	
	/* destroy the RNG context at the end because it may be needed to destroy some keys */
	bctoolbox_rng_context_free(context->RNGContext);
	context->RNGContext = NULL;
	free(context);
	return;
}


int bzrtp_setCallbacks(bzrtpContext_t *context, const bzrtpCallbacks_t *cbs) {
	context->zrtpCallbacks=*cbs;
	return 0;
}


int bzrtp_addChannel(bzrtpContext_t *zrtpContext, uint32_t selfSSRC) {
	bzrtpChannelContext_t *zrtpChannelContext = NULL;
	int i=0;

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

	return 0;

}

/*
 * @brief Start the state machine of the specified channel
 *
 * @param[in/out]	zrtpContext			The ZRTP context hosting the channel to be started
 * @param[in]		selfSSRC			The SSRC identifying the channel to be started(will start sending Hello packets and listening for some)
 *
 * @return			0 on succes, error code otherwise
 */

int bzrtp_startChannelEngine(bzrtpContext_t *zrtpContext, uint32_t selfSSRC) {
	bzrtpEvent_t initEvent;

	/* get channel context */
	bzrtpChannelContext_t *zrtpChannelContext = getChannelContext(zrtpContext, selfSSRC);

	if (zrtpChannelContext == NULL) {
		return BZRTP_ERROR_UNABLETOSTARTCHANNEL;
	}

	/* set the timer reference to 0 to force a message to be sent at first timer tick */
	zrtpContext->timeReference = 0;

	/* start the engine by setting the state to init and calling it */
	zrtpChannelContext->stateMachine = state_discovery_init;

	/* create an INIT event to call the init state function which will create a hello packet and start sending it */
	initEvent.eventType = BZRTP_EVENT_INIT;
	initEvent.bzrtpPacketString = NULL;
	initEvent.bzrtpPacketStringLength = 0;
	initEvent.zrtpContext = zrtpContext;
	initEvent.zrtpChannelContext = zrtpChannelContext;
	zrtpChannelContext->stateMachine(initEvent);

	return 0;
}

/*
 * @brief Send the current time to a specified channel, it will check if it has to trig some timer
 *
 * @param[in/out]	zrtpContext			The ZRTP context hosting the channel
 * @param[in]		selfSSRC			The SSRC identifying the channel
 * @param[in]		timeReference		The current time in ms
 *
 * @return			0 on succes, error code otherwise
 */
int bzrtp_iterate(bzrtpContext_t *zrtpContext, uint32_t selfSSRC, uint64_t timeReference) {
	/* get channel context */
	bzrtpChannelContext_t *zrtpChannelContext = getChannelContext(zrtpContext, selfSSRC);

	if (zrtpChannelContext == NULL) {
		return BZRTP_ERROR_INVALIDCONTEXT;
	}

	/* update the context time reference used when arming timers */
	zrtpContext->timeReference = timeReference;

	if (zrtpChannelContext->timer.status == BZRTP_TIMER_ON) {
		if (zrtpChannelContext->timer.firingTime<=timeReference) { /* we must trig the timer */
			bzrtpEvent_t timerEvent;

			zrtpChannelContext->timer.firingCount++;

			/* create a timer event */
			timerEvent.eventType = BZRTP_EVENT_TIMER;
			timerEvent.bzrtpPacketString = NULL;
			timerEvent.bzrtpPacketStringLength = 0;
			timerEvent.bzrtpPacket = NULL;
			timerEvent.zrtpContext = zrtpContext;
			timerEvent.zrtpChannelContext = zrtpChannelContext;

			/* send it to the state machine*/
			if (zrtpChannelContext->stateMachine != NULL) {
				return zrtpChannelContext->stateMachine(timerEvent);
			}
		}
	}

	return 0;
}

/*
 * @brief Set the client data pointer in a channel context
 * This pointer is returned to the client by the callbacks function, used to store associated contexts (RTP session)
 * @param[in/out]	zrtpContext		The ZRTP context we're dealing with
 * @param[in]		selfSSRC		The SSRC identifying the channel to be linked to the client Data
 * @param[in]		clientData		The clientData pointer, casted to a (void *)
 *
 * @return 0 on success
 *                                                                           
*/
int bzrtp_setClientData(bzrtpContext_t *zrtpContext, uint32_t selfSSRC, void *clientData) {
	/* get channel context */
	bzrtpChannelContext_t *zrtpChannelContext = getChannelContext(zrtpContext, selfSSRC);

	if (zrtpChannelContext == NULL) {
		return BZRTP_ERROR_INVALIDCONTEXT;
	}

	zrtpChannelContext->clientData = clientData;

	return 0;
}

/*
 * @brief Process a received message
 *
 * @param[in/out]	zrtpContext				The ZRTP context we're dealing with
 * @param[in]		selfSSRC				The SSRC identifying the channel receiving the message
 * @param[in]		zrtpPacketString		The packet received
 * @param[in]		zrtpPacketStringLength	Length of the packet in bytes
 *
 * @return 	0 on success, errorcode otherwise
 */
int bzrtp_processMessage(bzrtpContext_t *zrtpContext, uint32_t selfSSRC, uint8_t *zrtpPacketString, uint16_t zrtpPacketStringLength) {
	int retval;
	bzrtpPacket_t *zrtpPacket;
	bzrtpEvent_t event;

	/* get channel context */
	bzrtpChannelContext_t *zrtpChannelContext = getChannelContext(zrtpContext, selfSSRC);

	if (zrtpChannelContext == NULL) {
		return BZRTP_ERROR_INVALIDCONTEXT;
	}

	/* check the context is initialised (we may receive packets before initialisation is complete i.e. between channel initialisation and channel start) */
	if (zrtpChannelContext->stateMachine == NULL) {
		return BZRTP_ERROR_INVALIDCONTEXT; /* drop the message */
	}

	/* first check the packet */
	zrtpPacket = bzrtp_packetCheck(zrtpPacketString, zrtpPacketStringLength, zrtpChannelContext->peerSequenceNumber, &retval);
	if (retval != 0) {
		/*TODO: check the returned error code and do something or silent drop? */
		return retval;
	}

	/* TODO: Intercept error and ping zrtp packets */
	/* if we have a ping packet, just answer with a ping ACK and do not forward to the state machine */
	if (zrtpPacket->messageType == MSGTYPE_PING) {
		bzrtpPacket_t *pingAckPacket = NULL;

		bzrtp_packetParser(zrtpContext, zrtpChannelContext, zrtpPacketString, zrtpPacketStringLength, zrtpPacket);
		/* store ping packet in the channel context as packet creator will need it to create the pingACK */
		zrtpChannelContext->pingPacket = zrtpPacket;
		/* create the pingAck packet */
		pingAckPacket = bzrtp_createZrtpPacket(zrtpContext, zrtpChannelContext, MSGTYPE_PINGACK, &retval);
		if (retval == 0) {
			retval = bzrtp_packetBuild(zrtpContext, zrtpChannelContext, pingAckPacket, zrtpChannelContext->selfSequenceNumber);
			if (retval==0 && zrtpContext->zrtpCallbacks.bzrtp_sendData!=NULL) { /* send the packet */
				zrtpContext->zrtpCallbacks.bzrtp_sendData(zrtpChannelContext->clientData, pingAckPacket->packetString, pingAckPacket->messageLength+ZRTP_PACKET_OVERHEAD);
				zrtpChannelContext->selfSequenceNumber++;
			}
		}

		/* free packets and reset channel context storage */
		bzrtp_freeZrtpPacket(zrtpPacket);
		bzrtp_freeZrtpPacket(pingAckPacket);
		zrtpChannelContext->pingPacket = NULL;
		
		return retval;
	}

	/* build a packet event of it and send it to the state machine */
	event.eventType = BZRTP_EVENT_MESSAGE;
	event.bzrtpPacketString = zrtpPacketString;
	event.bzrtpPacketStringLength = zrtpPacketStringLength;
	event.bzrtpPacket = zrtpPacket;
	event.zrtpContext = zrtpContext;
	event.zrtpChannelContext = zrtpChannelContext;

	retval = zrtpChannelContext->stateMachine(event);
	return  retval;
}

/*
 * @brief Return the status of current channel, 1 if SRTP secrets have been computed and confirmed, 0 otherwise
 * 
 * @param[in]		zrtpContext			The ZRTP context hosting the channel
 * @param[in]		selfSSRC			The SSRC identifying the channel
 *
 * @return			0 if this channel is not ready to secure SRTP communication, 1 if it is ready
 */
int bzrtp_isSecure(bzrtpContext_t *zrtpContext, uint32_t selfSSRC) {
	
	/* get channel context */
	bzrtpChannelContext_t *zrtpChannelContext = getChannelContext(zrtpContext, selfSSRC);

	if (zrtpChannelContext == NULL) {
		return 0; /* can't find the channel, return it as non secure */
	}

	return zrtpChannelContext->isSecure;
}


/*
 * @brief Called by user when the SAS has been verified
 * update the cache(if any) to set the previously verified flag
 *
 * @param[in/out]	zrtpContext				The ZRTP context we're dealing with
 */
void bzrtp_SASVerified(bzrtpContext_t *zrtpContext) {
	if (zrtpContext != NULL) {
		uint8_t pvsFlag = 1;

		/* check if we must update the cache(delayed until sas verified in case of cache mismatch) */
		if (zrtpContext->cacheMismatchFlag  == 1) {
			zrtpContext->cacheMismatchFlag  = 0;
			bzrtp_updateCachedSecrets(zrtpContext, zrtpContext->channelContext[0]); /* channel[0] is the only one in DHM mode, so the only one able to have a cache mismatch */
		}
		bzrtp_writePeerNode(zrtpContext, zrtpContext->peerZID, (uint8_t *)"pvs", 3, &pvsFlag, 1, BZRTP_CACHE_TAGISBYTE|BZRTP_CACHE_NOMULTIPLETAGS, BZRTP_CACHE_LOADFILE|BZRTP_CACHE_WRITEFILE);
	}
}

/*
 * @brief Called by user when the SAS has been set to unverified
 * update the cache(if any) to unset the previously verified flag
 *
 * @param[in/out]	zrtpContext				The ZRTP context we're dealing with
 */
void bzrtp_resetSASVerified(bzrtpContext_t *zrtpContext) {
	if (zrtpContext != NULL) {
		uint8_t pvsFlag = 0;
		bzrtp_writePeerNode(zrtpContext, zrtpContext->peerZID, (uint8_t *)"pvs", 3, &pvsFlag, 1, BZRTP_CACHE_TAGISBYTE|BZRTP_CACHE_NOMULTIPLETAGS, BZRTP_CACHE_LOADFILE|BZRTP_CACHE_WRITEFILE);
	}
}

/*
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
 * @param[in]		useKDF				A flag, if set to 0, write data as it is provided, if set to 1, write KDF(s0, "tagContent", KDF_Context, negotiated hash length)
 * @param[in]		fileFlag			Flag, if LOADFILE bit is set, reload the cache buffer from file before updating.
 * 										if WRITEFILE bit is set, update the cache file
 *
 * @return	0 on success, errorcode otherwise
 */
int bzrtp_addCustomDataInCache(bzrtpContext_t *zrtpContext, uint8_t peerZID[12], uint8_t *tagName, uint16_t tagNameLength, uint8_t *tagContent, uint16_t tagContentLength, uint8_t derivedDataLength, uint8_t useKDF, uint8_t fileFlag) {
	/* check we have a valid context, a cache access callback function and a valid channelContext[0] */
	if (zrtpContext == NULL || zrtpContext->zrtpCallbacks.bzrtp_loadCache == NULL || zrtpContext->channelContext[0]==NULL) {
		return BZRTP_ERROR_INVALIDCONTEXT;
	}

	if (useKDF ==  BZRTP_CUSTOMCACHE_PLAINDATA) { /* write content as provided : content is a string and multiple tag is allowed as we are writing the peer URI(To be modified if needed) */
		return bzrtp_writePeerNode(zrtpContext, peerZID, tagName, tagNameLength, tagContent, tagContentLength, BZRTP_CACHE_TAGISSTRING|BZRTP_CACHE_ALLOWMULTIPLETAGS, fileFlag);
	} else { /* we must derive the content using the key derivation function */
		uint8_t derivedContent[32];
		
		/* check we have s0 and KDFContext in channel[0] */
		bzrtpChannelContext_t *zrtpChannelContext = zrtpContext->channelContext[0];
		if (zrtpChannelContext->s0 == NULL || zrtpChannelContext->KDFContext == NULL) {
			return BZRTP_ERROR_INVALIDCONTEXT;
		}
		/* We derive a maximum of 32 bytes for a 256 bit key */
		if (derivedDataLength>32) { 
			derivedDataLength = 32;
		}
		bzrtp_keyDerivationFunction(zrtpChannelContext->s0, zrtpChannelContext->hashLength, tagContent, tagContentLength, zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, derivedDataLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))zrtpChannelContext->hmacFunction, derivedContent);

		/* if derivedDataLength is 4 it means we are writing the session Index, mask the first bit of first byte(MSB) to 0 in order to avoid any counter loop */
		if (derivedDataLength == 4) {
			derivedContent[0] &=0x7F;
		}
		/* write it to cache, do not allow multiple tags */
		return bzrtp_writePeerNode(zrtpContext, peerZID, tagName, tagNameLength, derivedContent, derivedDataLength, BZRTP_CACHE_TAGISBYTE|BZRTP_CACHE_NOMULTIPLETAGS, fileFlag);
	}
}

/*
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

int bzrtp_resetRetransmissionTimer(bzrtpContext_t *zrtpContext, uint32_t selfSSRC) {
	/* get channel context */
	bzrtpChannelContext_t *zrtpChannelContext = getChannelContext(zrtpContext, selfSSRC);

	if (zrtpChannelContext == NULL) {
		return BZRTP_ERROR_INVALIDCONTEXT;
	}
	/* reset timer only when not in secure mode yet and for initiator(engine start as initiator so if we call this function in discovery phase, it will reset the timer */
	if ((zrtpContext->isSecure == 0) && (zrtpChannelContext->role == INITIATOR)) {
		zrtpChannelContext->timer.status = BZRTP_TIMER_ON;
		zrtpChannelContext->timer.firingTime = 0; /* be sure it will trigger at next call to bzrtp_iterate*/
		zrtpChannelContext->timer.firingCount = -1; /* -1 to count the initial packet and then retransmit the regular number of packets */
		/* reset timerStep to the base value */
		if ((zrtpChannelContext->timer.timerStep % NON_HELLO_BASE_RETRANSMISSION_STEP) == 0) {
			zrtpChannelContext->timer.timerStep = NON_HELLO_BASE_RETRANSMISSION_STEP;
		} else {
			zrtpChannelContext->timer.timerStep = HELLO_BASE_RETRANSMISSION_STEP;
		}
	}

	return 0;
}

/* Local functions implementation */

/**
 * @brief Look in the given ZRTP context for a channel referenced with given SSRC
 *
 * @param[in]	zrtpContext	The zrtp context which shall contain the channel context we are looking for
 * @param[in]	selfSSRC	The SSRC identifying the channel context
 *
 * @return 		a pointer to the channel context, NULL if the context is invalid or channel not found
 */
bzrtpChannelContext_t *getChannelContext(bzrtpContext_t *zrtpContext, uint32_t selfSSRC) {
	int i;

	if (zrtpContext==NULL) {
		return NULL;
	}
	
	for (i=0; i<ZRTP_MAX_CHANNEL_NUMBER; i++) {
		if (zrtpContext->channelContext[i]!=NULL) {
			if (zrtpContext->channelContext[i]->selfSSRC == selfSSRC) {
				return zrtpContext->channelContext[i];
			}
		}
	}

	return NULL; /* found no channel with this SSRC */
}
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

	zrtpChannelContext->clientData = NULL;

	/* the state machine is not started at the creation of the channel but on explicit call to the start function */
	zrtpChannelContext->stateMachine = NULL;

	/* timer is off */
	zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;
	zrtpChannelContext->timer.timerStep = HELLO_BASE_RETRANSMISSION_STEP; /* we must initialise the timeStep just in case the resettimer function is called between init and start */

	zrtpChannelContext->selfSSRC = selfSSRC;

	/* flags */
	zrtpChannelContext->isSecure = 0;

	/* initialise as initiator, switch to responder later if needed */
	zrtpChannelContext->role = INITIATOR;

	/* create H0 (32 bytes random) and derive using implicit Hash(SHA256) H1,H2,H3 */
	bctoolbox_rng_get(zrtpContext->RNGContext, zrtpChannelContext->selfH[0], 32);
	bctoolbox_sha256(zrtpChannelContext->selfH[0], 32, 32, zrtpChannelContext->selfH[1]);
	bctoolbox_sha256(zrtpChannelContext->selfH[1], 32, 32, zrtpChannelContext->selfH[2]);
	bctoolbox_sha256(zrtpChannelContext->selfH[2], 32, 32, zrtpChannelContext->selfH[3]);

	/* initialisation of packet storage */
	for (i=0; i<PACKET_STORAGE_CAPACITY; i++) {
		zrtpChannelContext->selfPackets[i] = NULL;
		zrtpChannelContext->peerPackets[i] = NULL;
	}

	/* initialise the self Sequence number to a random and peer to 0 */
	bctoolbox_rng_get(zrtpContext->RNGContext, (uint8_t *)&(zrtpChannelContext->selfSequenceNumber), 2);
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

	/* initialise srtpSecrets structure */
	zrtpChannelContext->srtpSecrets.selfSrtpKey = NULL;
	zrtpChannelContext->srtpSecrets.selfSrtpSalt = NULL;
	zrtpChannelContext->srtpSecrets.peerSrtpKey = NULL;
	zrtpChannelContext->srtpSecrets.peerSrtpSalt = NULL;
	zrtpChannelContext->srtpSecrets.selfSrtpKeyLength = 0;
	zrtpChannelContext->srtpSecrets.selfSrtpSaltLength = 0;
	zrtpChannelContext->srtpSecrets.peerSrtpKeyLength = 0;
	zrtpChannelContext->srtpSecrets.peerSrtpSaltLength = 0;
	zrtpChannelContext->srtpSecrets.cipherAlgo = ZRTP_UNSET_ALGO;
	zrtpChannelContext->srtpSecrets.cipherKeyLength = 0;
	zrtpChannelContext->srtpSecrets.authTagAlgo = ZRTP_UNSET_ALGO;
	zrtpChannelContext->srtpSecrets.sas = NULL;
	zrtpChannelContext->srtpSecrets.sasLength = 0;

	return 0;
}

/**
 * @brief Destroy the context of a channel
 * Free allocated buffers, destroy keys
 * 
 * @param[in] 		zrtpContext			The zrtpContext hosting this channel, needed to acces the RNG
 * @param[in] 		zrtpChannelContext	The channel context to be destroyed
 */
void bzrtp_destroyChannelContext(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext) {
	int i;

	/* check there is something to be freed */
	if (zrtpChannelContext == NULL) {
		return;
	}

	/* reset state Machine */
	zrtpChannelContext->stateMachine = NULL;

	/* set timer off */
	zrtpChannelContext->timer.status = BZRTP_TIMER_OFF;

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
		bzrtp_freeZrtpPacket(zrtpChannelContext->selfPackets[i]);
		bzrtp_freeZrtpPacket(zrtpChannelContext->peerPackets[i]);
		zrtpChannelContext->selfPackets[i] = NULL;
		zrtpChannelContext->peerPackets[i] = NULL;
	}

	/* destroy and free the srtp and sas struture */
	bzrtp_DestroyKey(zrtpChannelContext->srtpSecrets.selfSrtpKey, zrtpChannelContext->srtpSecrets.selfSrtpKeyLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->srtpSecrets.selfSrtpSalt, zrtpChannelContext->srtpSecrets.selfSrtpSaltLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->srtpSecrets.peerSrtpKey, zrtpChannelContext->srtpSecrets.peerSrtpKeyLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->srtpSecrets.peerSrtpSalt, zrtpChannelContext->srtpSecrets.peerSrtpSaltLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey((uint8_t *)zrtpChannelContext->srtpSecrets.sas, zrtpChannelContext->srtpSecrets.sasLength, zrtpContext->RNGContext);

	free(zrtpChannelContext->srtpSecrets.selfSrtpKey);
	free(zrtpChannelContext->srtpSecrets.selfSrtpSalt);
	free(zrtpChannelContext->srtpSecrets.peerSrtpKey);
	free(zrtpChannelContext->srtpSecrets.peerSrtpSalt);
	free(zrtpChannelContext->srtpSecrets.sas);

	/* free the channel context */
	free(zrtpChannelContext);
}

static uint8_t copyCryptoTypes(uint8_t destination[7], uint8_t source[7], uint8_t size)
{
	int i;

	for (i=0; i<size; i++) {
		destination[i] = source[i];
	}
	return size;
}

/**
 * @brief Get the supported crypto types
 *
 * @param[int]		zrtpContext				The ZRTP context we're dealing with
 * @param[in]		algoType				mapped to defines, must be in [ZRTP_HASH_TYPE, ZRTP_CIPHERBLOCK_TYPE, ZRTP_AUTHTAG_TYPE, ZRTP_KEYAGREEMENT_TYPE or ZRTP_SAS_TYPE]
 * @param[out]		supportedTypes			mapped to uint8_t value of the 4 char strings giving the supported types as string according to rfc section 5.1.2 to 5.1.6
 *
 * @return number of supported types, 0 on error
 */
uint8_t bzrtp_getSupportedCryptoTypes(bzrtpContext_t *zrtpContext, uint8_t algoType, uint8_t supportedTypes[7])
{
	if (zrtpContext==NULL) {
		return 0;
	}

	switch(algoType) {
		case ZRTP_HASH_TYPE:
			return copyCryptoTypes(supportedTypes, zrtpContext->supportedHash, zrtpContext->hc);
		case ZRTP_CIPHERBLOCK_TYPE:
			return copyCryptoTypes(supportedTypes, zrtpContext->supportedCipher, zrtpContext->cc);
		case ZRTP_AUTHTAG_TYPE:
			return copyCryptoTypes(supportedTypes, zrtpContext->supportedAuthTag, zrtpContext->ac);
		case ZRTP_KEYAGREEMENT_TYPE:
			return copyCryptoTypes(supportedTypes, zrtpContext->supportedKeyAgreement, zrtpContext->kc);
		case ZRTP_SAS_TYPE:
			return copyCryptoTypes(supportedTypes, zrtpContext->supportedSas, zrtpContext->sc);
		default:
			return 0;
	}
}

/**
 * @brief set the supported crypto types
 *
 * @param[int/out]	zrtpContext				The ZRTP context we're dealing with
 * @param[in]		algoType				mapped to defines, must be in [ZRTP_HASH_TYPE, ZRTP_CIPHERBLOCK_TYPE, ZRTP_AUTHTAG_TYPE, ZRTP_KEYAGREEMENT_TYPE or ZRTP_SAS_TYPE]
 * @param[in]		supportedTypes			mapped to uint8_t value of the 4 char strings giving the supported types as string according to rfc section 5.1.2 to 5.1.6
 * @param[in]		supportedTypesCount		number of supported crypto types
 */
void bzrtp_setSupportedCryptoTypes(bzrtpContext_t *zrtpContext, uint8_t algoType, uint8_t supportedTypes[7], uint8_t supportedTypesCount)
{
	uint8_t implementedTypes[7];
	uint8_t implementedTypesCount;

	if (zrtpContext==NULL) {
		return;
	}

	implementedTypesCount = bzrtpUtils_getAvailableCryptoTypes(algoType, implementedTypes);

	switch(algoType) {
		case ZRTP_HASH_TYPE:
			zrtpContext->hc = selectCommonAlgo(supportedTypes, supportedTypesCount, implementedTypes, implementedTypesCount, zrtpContext->supportedHash);
			addMandatoryCryptoTypesIfNeeded(algoType, zrtpContext->supportedHash, &zrtpContext->hc);
			break;
		case ZRTP_CIPHERBLOCK_TYPE:
			zrtpContext->cc = selectCommonAlgo(supportedTypes, supportedTypesCount, implementedTypes, implementedTypesCount, zrtpContext->supportedCipher);
			addMandatoryCryptoTypesIfNeeded(algoType, zrtpContext->supportedCipher, &zrtpContext->cc);
			break;
		case ZRTP_AUTHTAG_TYPE:
			zrtpContext->ac = selectCommonAlgo(supportedTypes, supportedTypesCount, implementedTypes, implementedTypesCount, zrtpContext->supportedAuthTag);
			addMandatoryCryptoTypesIfNeeded(algoType, zrtpContext->supportedAuthTag, &zrtpContext->ac);
			break;
		case ZRTP_KEYAGREEMENT_TYPE:
			zrtpContext->kc = selectCommonAlgo(supportedTypes, supportedTypesCount, implementedTypes, implementedTypesCount, zrtpContext->supportedKeyAgreement);
			addMandatoryCryptoTypesIfNeeded(algoType, zrtpContext->supportedKeyAgreement, &zrtpContext->kc);
			break;
		case ZRTP_SAS_TYPE:
			zrtpContext->sc = selectCommonAlgo(supportedTypes, supportedTypesCount, implementedTypes, implementedTypesCount, zrtpContext->supportedSas);
			addMandatoryCryptoTypesIfNeeded(algoType, zrtpContext->supportedSas, &zrtpContext->sc);
			break;
	}
}
