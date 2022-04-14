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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "bzrtp/bzrtp.h"
#include "zidCache.h"
#include "bzrtpTest.h"
#include "testUtils.h"

#define MAX_PACKET_LENGTH 3000
#define MAX_QUEUE_SIZE 10
#define MAX_CRYPTO_ALG 10
#define MAX_NUM_CHANNEL ZRTP_MAX_CHANNEL_NUMBER

typedef struct packetDatas_struct {
	uint8_t packetString[MAX_PACKET_LENGTH];
	uint16_t packetLength;
} packetDatas_t;

typedef struct clientContext_struct {
	uint8_t id;
	bzrtpContext_t *bzrtpContext;
	bctbx_mutex_t zidCacheMutex;
	bzrtpSrtpSecrets_t *secrets;
	int32_t pvs;
	uint8_t haveCacheMismatch;
	uint8_t	sendExportedKey[16];
	uint8_t  recvExportedKey[16];
} clientContext_t;

typedef struct cryptoParams_struct {
	uint8_t cipher[MAX_CRYPTO_ALG] ;
	uint8_t cipherNb;
	uint8_t hash[MAX_CRYPTO_ALG] ;
	uint8_t hashNb;
	uint8_t keyAgreement[MAX_CRYPTO_ALG] ;
	uint8_t keyAgreementNb;
	uint8_t sas[MAX_CRYPTO_ALG] ;
	uint8_t sasNb;
	uint8_t authtag[MAX_CRYPTO_ALG] ;
	uint8_t authtagNb;
	uint8_t dontValidateSASflag; /**< if set to 1, SAS will not be validated even if matching peer **/
	                             /**< if set to 2, SAS will be reset even if matching peer **/
} cryptoParams_t;


/* Global vars: message queues for Alice and Bob */
static packetDatas_t aliceQueue[MAX_QUEUE_SIZE];
static packetDatas_t bobQueue[MAX_QUEUE_SIZE];
static uint8_t aliceQueueIndex = 0;
static uint8_t bobQueueIndex = 0;

/* have ids to represent Alice and Bob */
#define ALICE 0x1
#define BOB   0x2

#define ALICE_SSRC_BASE 0x12345000
#define BOB_SSRC_BASE 0x87654000

static cryptoParams_t withoutX255 = {{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0};
static cryptoParams_t withX255 = {{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0};
cryptoParams_t *defaultCryptoAlgoSelection(void) {
	if (bctbx_key_agreement_algo_list()&BCTBX_ECDH_X25519) {
		return &withX255;
	}
	return &withoutX255;
}

static cryptoParams_t withoutX255noSAS = {{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,1};
static cryptoParams_t withX255noSAS = {{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,1};
cryptoParams_t *defaultCryptoAlgoSelectionNoSASValidation(void) {
	if (bctbx_key_agreement_algo_list()&BCTBX_ECDH_X25519) {
		return &withX255noSAS;
	}
	return &withoutX255noSAS;
}

static cryptoParams_t withoutX255resetSAS = {{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,2};
static cryptoParams_t withX255resetSAS = {{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,2};
cryptoParams_t *defaultCryptoAlgoSelectionResetSAS(void) {
	if (bctbx_key_agreement_algo_list()&BCTBX_ECDH_X25519) {
		return &withX255resetSAS;
	}
	return &withoutX255resetSAS;
}

/* static global settings and their reset function */
static uint64_t msSTC = 0; /* Simulation Time Coordinated start at 0 and increment it at each sleep, is in milliseconds */
static int loosePacketPercentage=0; /* simulate bd network condition: loose packet */
static uint64_t timeOutLimit=1000; /* in ms, time span given to perform the ZRTP exchange */
static float fadingLostAlice=0.0; /* try not to throw away too many packet in a row */
static float fadingLostBob=0.0; /* try not to throw away too many packet in a row */
static int continuousPacketLost=0; /* Enforce not loosing more than 10 consecutive packets so we're sure to complete the exchange */
static int totalPacketLost=0; /* for statistics */
static int totalPacketSent=0; /* for statistics */

/* when timeout is set to this specific value, negotiation is aborted but silently fails */
#define ABORT_NEGOTIATION_TIMEOUT 24
static void resetGlobalParams(void) {
	msSTC=0;
	continuousPacketLost = 0;
	totalPacketLost = 0;
	totalPacketSent = 0;
	loosePacketPercentage = 0;
	timeOutLimit = 1000;
	fadingLostBob = 0;
	fadingLostAlice = 0;
}

/* time functions, we do not run a real time scenario, go for fast test instead */
static uint64_t getSimulatedTime(void) {
	return msSTC;
}
static void STC_sleep(int ms){
	msSTC +=ms;
}

/* routing messages */
static int sendData(void *clientData, const uint8_t *packetString, uint16_t packetLength) {
	/* get the client context */
	clientContext_t *clientContext = (clientContext_t *)clientData;

	/* manage loosy network simulation */
	if (loosePacketPercentage > 0) {
		totalPacketSent++; // Stats on packets sent only when we can loose packets
		/* make sure we cannot loose 10 packets in a row from the same sender */
		if ((continuousPacketLost<10) && ((float)((rand()%100 )) < loosePacketPercentage-((clientContext->id == ALICE)?fadingLostAlice:fadingLostBob))) { /* randomly discard packets */
			//bzrtp_message("%d Loose %.8s from %s - LC %d\n", msSTC, packetString+16, (clientContext->id==ALICE?"Alice":"Bob"), totalPacketLost);

			if (clientContext->id == ALICE) {
				fadingLostAlice +=loosePacketPercentage/8;
			} else {
				fadingLostBob +=loosePacketPercentage/8;
			}
			continuousPacketLost++;
			totalPacketLost++;
			return 0;
		}
		continuousPacketLost = 0;
		if (clientContext->id == ALICE) {
			fadingLostAlice = 0;
		} else {
			fadingLostBob = 0;
		}
		//bzrtp_message("%d Keep %.8s from %s - LC %d\n", msSTC, packetString+16, (clientContext->id==ALICE?"Alice":"Bob"), totalPacketLost);
	}
	//bzrtp_message("%ld %.8s from %s\n", msSTC, packetString+16, (clientContext->id==ALICE?"Alice":"Bob"));

	/* put the message in the message correct queue */
	if (clientContext->id == ALICE) { /* message sent by Alice, so put it in Bob's queue */
		fadingLostAlice = MAX(0,fadingLostAlice-loosePacketPercentage/2);
		memcpy(bobQueue[bobQueueIndex].packetString, packetString, packetLength);
		bobQueue[bobQueueIndex++].packetLength = packetLength;
	} else { /* Bob sent the message, put it in Alice's queue */
		fadingLostBob = MAX(0,fadingLostBob-loosePacketPercentage/2);
		memcpy(bobQueue[bobQueueIndex].packetString, packetString, packetLength);
		memcpy(aliceQueue[aliceQueueIndex].packetString, packetString, packetLength);
		aliceQueue[aliceQueueIndex++].packetLength = packetLength;
	}

	return 0;
}

/* get SAS and SRTP keys */
int getSAS(void *clientData, bzrtpSrtpSecrets_t *secrets, int32_t pvs) {
	/* get the client context */
	clientContext_t *clientContext = (clientContext_t *)clientData;

	/* store the secret struct */
	clientContext->secrets = secrets;
	/* and the PVS flag */
	clientContext->pvs = pvs;

	return 0;
}

int getMessage(void *clientData, const uint8_t level, const uint8_t message, const char *messageString) {
	/* get the client context */
	clientContext_t *clientContext = (clientContext_t *)clientData;
	if (level == BZRTP_MESSAGE_ERROR && message == BZRTP_MESSAGE_CACHEMISMATCH) {
		clientContext->haveCacheMismatch = 1;
	}
	return 0;
}

int computeExportedKeys(void *clientData, int zuid, uint8_t role) {
	size_t keyLength = 16;
	/* get the client context */
	clientContext_t *clientContext = (clientContext_t *)clientData;

	/* compute 2 exported keys with label initiator and responder */
	BC_ASSERT_EQUAL(bzrtp_exportKey(clientContext->bzrtpContext,  ((role==BZRTP_ROLE_RESPONDER)?"ResponderKey":"InitiatorKey"), 12, clientContext->sendExportedKey, &keyLength), 0, int, "%x");
	BC_ASSERT_EQUAL(keyLength, 16, size_t, "%zu"); /* any hash available in the config shall be able to produce a 16 bytes key */
	keyLength = 16;
	BC_ASSERT_EQUAL(bzrtp_exportKey(clientContext->bzrtpContext,  ((role==BZRTP_ROLE_INITIATOR)?"ResponderKey":"InitiatorKey"), 12, clientContext->recvExportedKey, &keyLength), 0, int, "%x");
	BC_ASSERT_EQUAL(keyLength, 16, size_t, "%zu"); /* any hash available in the config shall be able to produce a 16 bytes key */

	return 0;
}

static int setUpClientContext(clientContext_t *clientContext, uint8_t clientID, uint32_t SSRC, void *zidCache, bctbx_mutex_t *zidCacheMutex, char *selfURI, char *peerURI, cryptoParams_t *cryptoParams) {
	int retval;
	bzrtpCallbacks_t cbs={0} ;

	/* set Id */
	clientContext->id = clientID;
	clientContext->pvs=0;
	clientContext->haveCacheMismatch=0;

	/* create zrtp context */
	clientContext->bzrtpContext = bzrtp_createBzrtpContext();
	if (clientContext->bzrtpContext==NULL) {
		bzrtp_message("ERROR: can't create bzrtp context, id client is %d", clientID);
		return -1;
	}

	/* check cache */
	if (zidCache != NULL) {
#ifdef ZIDCACHE_ENABLED
		if (zidCacheMutex == NULL) {
			zidCacheMutex = &(clientContext->zidCacheMutex);
			bctbx_mutex_init(zidCacheMutex, NULL);
		}
		retval = bzrtp_setZIDCache_lock(clientContext->bzrtpContext, zidCache, selfURI, peerURI, zidCacheMutex);
		if (retval != 0 && retval != BZRTP_CACHE_SETUP) { /* return value is BZRTP_CACHE_SETUP if the cache is populated by this call */
			bzrtp_message("ERROR: bzrtp_setZIDCache %0x, client id is %d\n", retval, clientID);
			return -2;
		}
#else
		bzrtp_message("ERROR: asking for cache but not enabled at compile time\n");
		return -2;
#endif
	}

	/* assign callbacks */
	cbs.bzrtp_sendData=sendData;
	cbs.bzrtp_startSrtpSession=(int (*)(void *,const bzrtpSrtpSecrets_t *,int32_t) )getSAS;
	cbs.bzrtp_statusMessage=getMessage;
	cbs.bzrtp_messageLevel = BZRTP_MESSAGE_ERROR;
	cbs.bzrtp_contextReadyForExportedKeys = computeExportedKeys;
	if ((retval = bzrtp_setCallbacks(clientContext->bzrtpContext, &cbs))!=0) {
		bzrtp_message("ERROR: bzrtp_setCallbacks returned %0x, client id is %d\n", retval, clientID);
		return -3;
	}

	/* set crypto params */
	if (cryptoParams != NULL) {
		bzrtp_setSupportedCryptoTypes(clientContext->bzrtpContext, ZRTP_HASH_TYPE, cryptoParams->hash, cryptoParams->hashNb);
		bzrtp_setSupportedCryptoTypes(clientContext->bzrtpContext, ZRTP_CIPHERBLOCK_TYPE, cryptoParams->cipher, cryptoParams->cipherNb);
		bzrtp_setSupportedCryptoTypes(clientContext->bzrtpContext, ZRTP_KEYAGREEMENT_TYPE, cryptoParams->keyAgreement, cryptoParams->keyAgreementNb);
		bzrtp_setSupportedCryptoTypes(clientContext->bzrtpContext, ZRTP_AUTHTAG_TYPE, cryptoParams->authtag, cryptoParams->authtagNb);
		bzrtp_setSupportedCryptoTypes(clientContext->bzrtpContext, ZRTP_SAS_TYPE, cryptoParams->sas, cryptoParams->sasNb);
	}

	/* init the first channel */
	bzrtp_initBzrtpContext(clientContext->bzrtpContext, SSRC);
	if ((retval = bzrtp_setClientData(clientContext->bzrtpContext, SSRC, (void *)clientContext))!=0) {
		bzrtp_message("ERROR: bzrtp_setClientData returned %0x, client id is %d\n", retval, clientID);
		return -4;
	}

	return 0;
}

static int addChannel(clientContext_t *clientContext, uint32_t SSRC) {
	int retval=0;

	/* add channel */
	if ((retval = bzrtp_addChannel(clientContext->bzrtpContext, SSRC))!=0) {
		bzrtp_message("ERROR: bzrtp_addChannel returned %0x, client id is %d\n", retval, clientContext->id);
		return -1;
	}

	/* associated client data(give the same than for first channel) */
	if ((retval = bzrtp_setClientData(clientContext->bzrtpContext, SSRC, (void *)clientContext))!=0) {
		bzrtp_message("ERROR: bzrtp_setClientData on secondary channel returned %0x, client id is %d\n", retval, clientContext->id);
		return -2;
	}

	/* start the channel */
	if ((retval = bzrtp_startChannelEngine(clientContext->bzrtpContext, SSRC))!=0) {
		bzrtp_message("ERROR: bzrtp_startChannelEngine on secondary channel returned %0x, client id is %d SSRC is %d\n", retval, clientContext->id,SSRC);
		return -3;
	}

	return 0;
}

/* are all a and b field the same? Check Sas(optionnaly as it is not provided for secondary channel) and srtp keys and choosen algo*/

static int compareSecrets(bzrtpSrtpSecrets_t *a, bzrtpSrtpSecrets_t* b, uint8_t mainChannel) {
	if (mainChannel==TRUE) {
		if (strcmp(a->sas,b->sas)!=0) {
			return -1;
		}
	}

	if (mainChannel == TRUE) {
		if ((a->authTagAlgo!=b->authTagAlgo)
		  || a->hashAlgo!=b->hashAlgo
		  || a->keyAgreementAlgo!=b->keyAgreementAlgo
		  || a->sasAlgo!=b->sasAlgo
		  || a->cipherAlgo!=b->cipherAlgo) {
			return -2;
		}
	} else {
		if ((a->authTagAlgo!=b->authTagAlgo)
		  || a->hashAlgo!=b->hashAlgo
		  || a->keyAgreementAlgo!=b->keyAgreementAlgo
		  || a->cipherAlgo!=b->cipherAlgo) {
			return -2;
		}
	}


	if (a->selfSrtpKeyLength==0 || b->selfSrtpKeyLength==0
	 || a->selfSrtpSaltLength==0 || b->selfSrtpSaltLength==0
	 || a->peerSrtpKeyLength==0 || b->peerSrtpKeyLength==0
	 || a->peerSrtpSaltLength==0 || b->peerSrtpSaltLength==0) {
		return -3;
	}

	if (a->selfSrtpKeyLength != b->peerSrtpKeyLength
	 || a->selfSrtpSaltLength != b->peerSrtpSaltLength
	 || a->peerSrtpKeyLength != b->selfSrtpKeyLength
	 || a->peerSrtpSaltLength != b->selfSrtpSaltLength) {
		return -4;
	}

	if (memcmp (a->selfSrtpKey, b->peerSrtpKey, b->peerSrtpKeyLength) != 0
	 || memcmp (a->selfSrtpSalt, b->peerSrtpSalt, b->peerSrtpSaltLength) != 0
	 || memcmp (a->peerSrtpKey, b->selfSrtpKey, b->selfSrtpKeyLength) != 0
	 || memcmp (a->peerSrtpSalt, b->selfSrtpSalt, b->selfSrtpSaltLength) != 0) {
		return -5;
	}

	return 0;
}

/* compare algo sets */
static int compareAlgoList(bzrtpSrtpSecrets_t *secrets, cryptoParams_t *cryptoParams) {
	if (secrets->authTagAlgo != cryptoParams->authtag[0]) return -1;
	if (secrets->hashAlgo != cryptoParams->hash[0]) return -2;
	if (secrets->cipherAlgo != cryptoParams->cipher[0]) return -3;
	if (secrets->keyAgreementAlgo != cryptoParams->keyAgreement[0]) return -4;
	if (secrets->sasAlgo != cryptoParams->sas[0]) return -5;
	return 0;
}

/* defines return values bit flags(on 16 bits, use 32 to return status for Bob(16 MSB) and Alice(16 LSB)) */
#define RET_CACHE_MISMATCH 0x0001

/*
 * Never call directly this function in tests, its purpose is to have a flexible API according to future needs
 * use a variant or create a new one, see after this function
 */
uint32_t multichannel_exchange_full_params(cryptoParams_t *aliceCryptoParams, // Alice parameter, can be NULL
										   cryptoParams_t *bobCryptoParams, // Bob parameters, can be NULL
										   cryptoParams_t *expectedCryptoParams, // Expected crypto algo used, checked only when not null
										   void *aliceCache, bctbx_mutex_t *aliceCacheMutex, char *aliceURI, // Alice cache related informations, if NULL, run cacheless
										   void *bobCache, bctbx_mutex_t *bobCacheMutex, char *bobURI, // Bob cache related informations, if NULL, run cacheless
										   uint8_t checkPVS, uint8_t expectedAlicePVS, uint8_t expectedBobPVS, // When checkPVS is TRUE, check that Alice and Bob PVS are as expected
										   size_t mtu) // if mtu is not 0, enforce mtu and check that packet size are never bigger than expected
{

	int retval,channelNumber;
	clientContext_t Alice,Bob;
	uint64_t initialTime=0;
	uint64_t lastPacketSentTime=0;
	uint32_t aliceSSRC = ALICE_SSRC_BASE;
	uint32_t bobSSRC = BOB_SSRC_BASE;
	uint32_t ret=0;

	/*** Create the main channel */
	if ((retval=setUpClientContext(&Alice, ALICE, aliceSSRC, aliceCache, aliceCacheMutex, aliceURI, bobURI, aliceCryptoParams))!=0) {
		bzrtp_message("ERROR: can't init setup client context id %d\n", ALICE);
		BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
		return retval;
	}

	if ((retval=setUpClientContext(&Bob, BOB, bobSSRC, bobCache, bobCacheMutex, bobURI, aliceURI, bobCryptoParams))!=0) {
		bzrtp_message("ERROR: can't init setup client context id %d\n", BOB);
		BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
		return retval;
	}

	/* When mtu is set, set it and check it worked */
	if (mtu>0) {
		BC_ASSERT_EQUAL(bzrtp_set_MTU(Alice.bzrtpContext, mtu), 0, int, "%d");
		BC_ASSERT_EQUAL(bzrtp_get_MTU(Alice.bzrtpContext), mtu, size_t, "%zu");
		BC_ASSERT_EQUAL(bzrtp_set_MTU(Bob.bzrtpContext, mtu), 0, int, "%d");
		BC_ASSERT_EQUAL(bzrtp_get_MTU(Bob.bzrtpContext), mtu, size_t, "%zu");
	}

	/* start the ZRTP engine(it will send a hello packet )*/
	if ((retval = bzrtp_startChannelEngine(Alice.bzrtpContext, aliceSSRC))!=0) {
		bzrtp_message("ERROR: bzrtp_startChannelEngine returned %0x, client id is %d SSRC is %d\n", retval, ALICE, aliceSSRC);
		return retval;
	}
	if ((retval = bzrtp_startChannelEngine(Bob.bzrtpContext, bobSSRC))!=0) {
		bzrtp_message("ERROR: bzrtp_startChannelEngine returned %0x, client id is %d SSRC is %d\n", retval, BOB, bobSSRC);
		return retval;
	}

	initialTime = getSimulatedTime();
	while ((bzrtp_getChannelStatus(Alice.bzrtpContext, aliceSSRC)!= BZRTP_CHANNEL_SECURE || bzrtp_getChannelStatus(Bob.bzrtpContext, bobSSRC)!= BZRTP_CHANNEL_SECURE) && (getSimulatedTime()-initialTime<timeOutLimit)){
		int i;
		/* check the message queue */
		for (i=0; i<aliceQueueIndex; i++) {
			if (mtu > 0) { // Check the packet size we received
				BC_ASSERT_LOWER(aliceQueue[i].packetLength, mtu, size_t, "%zu");
			}
			retval = bzrtp_processMessage(Alice.bzrtpContext, aliceSSRC, aliceQueue[i].packetString, aliceQueue[i].packetLength);
			//bzrtp_message("%ld Alice processed a %.8s and returns %x\n", msSTC, (aliceQueue[i].packetString)+16, retval);
			memset(aliceQueue[i].packetString, 0, MAX_PACKET_LENGTH); /* destroy the packet after sending it to the ZRTP engine */
			lastPacketSentTime=getSimulatedTime();
		}
		aliceQueueIndex = 0;

		for (i=0; i<bobQueueIndex; i++) {
			if (mtu > 0) { // Check the packet size we received
				BC_ASSERT_LOWER(bobQueue[i].packetLength, mtu, size_t, "%zu");
			}
			retval = bzrtp_processMessage(Bob.bzrtpContext, bobSSRC, bobQueue[i].packetString, bobQueue[i].packetLength);
			//bzrtp_message("%ld Bob processed a %.8s and returns %x\n",msSTC, (bobQueue[i].packetString)+16, retval);
			memset(bobQueue[i].packetString, 0, MAX_PACKET_LENGTH); /* destroy the packet after sending it to the ZRTP engine */
			lastPacketSentTime=getSimulatedTime();
		}
		bobQueueIndex = 0;

		/* send the actual time to the zrtpContext */
		retval = bzrtp_iterate(Alice.bzrtpContext, aliceSSRC, getSimulatedTime());
		retval = bzrtp_iterate(Bob.bzrtpContext, bobSSRC, getSimulatedTime());

		/* sleep for 10 ms */
		STC_sleep(10);

		/* check if we shall try to reset re-emission timers */
		if (getSimulatedTime()-lastPacketSentTime > 1250 ) { /*higher re-emission timeout is 1200ms */
			retval = bzrtp_resetRetransmissionTimer(Alice.bzrtpContext, aliceSSRC);
			retval +=bzrtp_resetRetransmissionTimer(Bob.bzrtpContext, bobSSRC);
			lastPacketSentTime=getSimulatedTime();
		}
	}

	/* when timeOutLimit is set to this specific value, our intention is to start a negotiation but not to finish it, so just return without errors */
	if (timeOutLimit == ABORT_NEGOTIATION_TIMEOUT) {
		/*** Destroy Contexts ***/
		if (aliceCache != NULL && aliceCacheMutex == NULL) { /* mutex was not provided externally, so we set up ours, destroy it */
			bctbx_mutex_destroy(&(Alice.zidCacheMutex));
		}
		while (bzrtp_destroyBzrtpContext(Alice.bzrtpContext, aliceSSRC)>0 && aliceSSRC>=ALICE_SSRC_BASE) {
			aliceSSRC--;
		}
		if (bobCache != NULL && bobCacheMutex == NULL) { /* mutex was not provided externally, so we set up ours, destroy it */
			bctbx_mutex_destroy(&(Bob.zidCacheMutex));
		}
		while (bzrtp_destroyBzrtpContext(Bob.bzrtpContext, bobSSRC)>0 && bobSSRC>=BOB_SSRC_BASE) {
			bobSSRC--;
		}

		return ret;
	}

	if ((retval=bzrtp_getChannelStatus(Alice.bzrtpContext, aliceSSRC))!=BZRTP_CHANNEL_SECURE) {
		bzrtp_message("Fail Alice on channel1 loss rate is %d", loosePacketPercentage);
		BC_ASSERT_EQUAL(retval, BZRTP_CHANNEL_SECURE, int, "%0x");
		return retval;
	}
	if ((retval=bzrtp_getChannelStatus(Bob.bzrtpContext, bobSSRC))!=BZRTP_CHANNEL_SECURE) {
		bzrtp_message("Fail Bob on channel1 loss rate is %d", loosePacketPercentage);
		BC_ASSERT_EQUAL(retval, BZRTP_CHANNEL_SECURE, int, "%0x");
		return retval;
	}

	bzrtp_message("ZRTP algo used during negotiation: Cipher: %s - KeyAgreement: %s - Hash: %s - AuthTag: %s - Sas Rendering: %s\n", bzrtp_algoToString(Alice.secrets->cipherAlgo), bzrtp_algoToString(Alice.secrets->keyAgreementAlgo), bzrtp_algoToString(Alice.secrets->hashAlgo), bzrtp_algoToString(Alice.secrets->authTagAlgo), bzrtp_algoToString(Alice.secrets->sasAlgo));

	if ((retval=compareSecrets(Alice.secrets, Bob.secrets, TRUE))!=0) {
		BC_ASSERT_EQUAL(retval, 0, int, "%d");
		if (aliceCache != NULL && bobCache != NULL) {
			bzrtp_resetSASVerified(Alice.bzrtpContext);
			bzrtp_resetSASVerified(Bob.bzrtpContext);
		}
		return retval;
	} else { /* SAS comparison is Ok, if we have a cache, confirm it */
		if (aliceCache != NULL && bobCache != NULL) {
			/* Confirm only when the cryptoParam->dontValidateSASflag is not present or set to 0 */
			if (aliceCryptoParams==NULL || aliceCryptoParams->dontValidateSASflag == 0) {
				bzrtp_SASVerified(Alice.bzrtpContext);
			} else if (aliceCryptoParams!=NULL && aliceCryptoParams->dontValidateSASflag == 2) { /* if flag is set to 2 reset the SAS */
				bzrtp_resetSASVerified(Alice.bzrtpContext);
			}
			/* Confirm only when the cryptoParam->dontValidateSASflag is not present or set to 0 */
			if (bobCryptoParams==NULL || bobCryptoParams->dontValidateSASflag == 0) {
				bzrtp_SASVerified(Bob.bzrtpContext);
			} else if (bobCryptoParams!=NULL && bobCryptoParams->dontValidateSASflag == 2) { /* if flag is set to 2 reset the SAS */
				bzrtp_resetSASVerified(Bob.bzrtpContext);
			}
			/* if flag is set to 1 just ignore the SAS validation */
		}
	}

	/* shall we check the PVS returned by the SAS callback? */
	if (checkPVS==TRUE) {
		BC_ASSERT_EQUAL(Alice.pvs, expectedAlicePVS, int, "%d");
		BC_ASSERT_EQUAL(Bob.pvs, expectedBobPVS, int, "%d");
	}

	/* if we have expected crypto param, check our result */
	if (expectedCryptoParams!=NULL) {
		BC_ASSERT_EQUAL(compareAlgoList(Alice.secrets,expectedCryptoParams), 0, int, "%d");
	}

	/* check exported keys */
	BC_ASSERT_EQUAL(memcmp(Alice.sendExportedKey, Bob.recvExportedKey, 16), 0, int, "%d");
	BC_ASSERT_EQUAL(memcmp(Alice.recvExportedKey, Bob.sendExportedKey, 16), 0, int, "%d");

	/* open as much channels as we can */
	for (channelNumber=2; channelNumber<=MAX_NUM_CHANNEL; channelNumber++) {
		/* increase SSRCs as they are used to identify a channel */
		aliceSSRC++;
		bobSSRC++;

		/* start a new channel */
		if ((retval=addChannel(&Alice, aliceSSRC))!=0) {
			bzrtp_message("ERROR: can't add a second channel to client context id %d\n", ALICE);
			BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
			return retval;
		}

		if ((retval=addChannel(&Bob, bobSSRC))!=0) {
			bzrtp_message("ERROR: can't add a second channel to client context id %d\n", ALICE);
			BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
			return retval;
		}

		initialTime = getSimulatedTime();
		while ((bzrtp_getChannelStatus(Alice.bzrtpContext, aliceSSRC)!= BZRTP_CHANNEL_SECURE || bzrtp_getChannelStatus(Bob.bzrtpContext, bobSSRC)!= BZRTP_CHANNEL_SECURE) && (getSimulatedTime()-initialTime<timeOutLimit)){
			int i;
			/* check the message queue */
			for (i=0; i<aliceQueueIndex; i++) {
				retval = bzrtp_processMessage(Alice.bzrtpContext, aliceSSRC, aliceQueue[i].packetString, aliceQueue[i].packetLength);
				//bzrtp_message("%ld Alice processed a %.8s and returns %x\n",msSTC, aliceQueue[i].packetString+16, retval);
				memset(aliceQueue[i].packetString, 0, MAX_PACKET_LENGTH); /* destroy the packet after sending it to the ZRTP engine */
				lastPacketSentTime=getSimulatedTime();
			}
			aliceQueueIndex = 0;

			for (i=0; i<bobQueueIndex; i++) {
				retval = bzrtp_processMessage(Bob.bzrtpContext, bobSSRC, bobQueue[i].packetString, bobQueue[i].packetLength);
				//bzrtp_message("%ld Bob processed a %.8s and returns %x\n",msSTC, bobQueue[i].packetString+16, retval);
				memset(bobQueue[i].packetString, 0, MAX_PACKET_LENGTH); /* destroy the packet after sending it to the ZRTP engine */
				lastPacketSentTime=getSimulatedTime();
			}
			bobQueueIndex = 0;

			/* send the actual time to the zrtpContext */
			retval = bzrtp_iterate(Alice.bzrtpContext, aliceSSRC, getSimulatedTime());
			retval = bzrtp_iterate(Bob.bzrtpContext, bobSSRC, getSimulatedTime());

			/* sleep for 10 ms */
			STC_sleep(10);

			/* check if we shall try to reset re-emission timers */
			if (getSimulatedTime()-lastPacketSentTime > 1250 ) { /*higher re-emission timeout is 1200ms */
				retval = bzrtp_resetRetransmissionTimer(Alice.bzrtpContext, aliceSSRC);
				retval += bzrtp_resetRetransmissionTimer(Bob.bzrtpContext, bobSSRC);
				lastPacketSentTime=getSimulatedTime();
			}
		}
		if ((retval=bzrtp_getChannelStatus(Alice.bzrtpContext, aliceSSRC))!=BZRTP_CHANNEL_SECURE) {
			bzrtp_message("Fail Alice on channel2 loss rate is %d", loosePacketPercentage);
			BC_ASSERT_EQUAL(retval, BZRTP_CHANNEL_SECURE, int, "%0x");
			return retval;
		}
		if ((retval=bzrtp_getChannelStatus(Bob.bzrtpContext, bobSSRC))!=BZRTP_CHANNEL_SECURE) {
			bzrtp_message("Fail Bob on channel2 loss rate is %d", loosePacketPercentage);
			BC_ASSERT_EQUAL(retval, BZRTP_CHANNEL_SECURE, int, "%0x");
			return retval;
		}
		bzrtp_message("Channel %d :ZRTP algo used during negotiation: Cipher: %s - KeyAgreement: %s - Hash: %s - AuthTag: %s - Sas Rendering: %s\n", channelNumber, bzrtp_algoToString(Alice.secrets->cipherAlgo), bzrtp_algoToString(Alice.secrets->keyAgreementAlgo), bzrtp_algoToString(Alice.secrets->hashAlgo), bzrtp_algoToString(Alice.secrets->authTagAlgo), bzrtp_algoToString(Alice.secrets->sasAlgo));
		if ((retval=compareSecrets(Alice.secrets, Bob.secrets, FALSE))!=0) {
			BC_ASSERT_EQUAL(retval, 0, int, "%d");
		}
	}

	/*** Destroy Contexts ***/
	if (aliceCache != NULL && aliceCacheMutex == NULL) { /* mutex was not provided externally, so we set up ours, destroy it */
		bctbx_mutex_destroy(&(Alice.zidCacheMutex));
	}
	while (bzrtp_destroyBzrtpContext(Alice.bzrtpContext, aliceSSRC)>0 && aliceSSRC>=ALICE_SSRC_BASE) {
		aliceSSRC--;
	}
	if (bobCache != NULL && bobCacheMutex == NULL) { /* mutex was not provided externally, so we set up ours, destroy it */
		bctbx_mutex_destroy(&(Bob.zidCacheMutex));
	}
	while (bzrtp_destroyBzrtpContext(Bob.bzrtpContext, bobSSRC)>0 && bobSSRC>=BOB_SSRC_BASE) {
		bobSSRC--;
	}

	/** Compute return value **/
	if (Alice.haveCacheMismatch==1) {
		ret |= RET_CACHE_MISMATCH;
	}
	if (Bob.haveCacheMismatch==1) {
		ret |= RET_CACHE_MISMATCH<<16;
	}

	return ret;
}

/* Variants of the exchange function with less parameter : never call directly the full params one but use one of these */
uint32_t multichannel_exchange_pvs_params(cryptoParams_t *aliceCryptoParams, cryptoParams_t *bobCryptoParams, cryptoParams_t *expectedCryptoParams, void *aliceCache, char *aliceURI, void *bobCache, char *bobURI, uint8_t checkPVS, uint8_t expectedAlicePVS, uint8_t expectedBobPVS) {
	return multichannel_exchange_full_params(aliceCryptoParams, bobCryptoParams, expectedCryptoParams, aliceCache, NULL, aliceURI, bobCache, NULL, bobURI, checkPVS, expectedAlicePVS, expectedBobPVS, 0);
}

uint32_t multichannel_exchange(cryptoParams_t *aliceCryptoParams, cryptoParams_t *bobCryptoParams, cryptoParams_t *expectedCryptoParams, void *aliceCache, char *aliceURI, void *bobCache, char *bobURI) {
	return multichannel_exchange_full_params(aliceCryptoParams, bobCryptoParams, expectedCryptoParams, aliceCache, NULL, aliceURI, bobCache, NULL, bobURI, FALSE, 0, 0, 0);
}

uint32_t multichannel_exchange_mtu(cryptoParams_t *aliceCryptoParams, cryptoParams_t *bobCryptoParams, cryptoParams_t *expectedCryptoParams, size_t mtu) {
	return multichannel_exchange_full_params(aliceCryptoParams, bobCryptoParams, expectedCryptoParams, NULL, NULL, NULL, NULL, NULL, NULL, FALSE, 0, 0, mtu);
}

uint32_t multichannel_exchange_mutex(cryptoParams_t *aliceCryptoParams, cryptoParams_t *bobCryptoParams, cryptoParams_t *expectedCryptoParams, void *aliceCache, bctbx_mutex_t *aliceCacheMutex, char *aliceURI, void *bobCache, bctbx_mutex_t *bobCacheMutex, char *bobURI) {
	return multichannel_exchange_full_params(aliceCryptoParams, bobCryptoParams, expectedCryptoParams, aliceCache, aliceCacheMutex, aliceURI, bobCache, bobCacheMutex, bobURI, FALSE, 0, 0, 0);
}


static void test_cacheless_exchange(void) {
	cryptoParams_t *pattern;

	/* Reset Global Static settings */
	resetGlobalParams();

	/* Note: common algo selection is not tested here(this is done in some cryptoUtils tests)
	here we just perform an exchange with any final configuration avalaible and check it goes well */
	cryptoParams_t patterns[] = {
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{0},0,{0},0,{0},0,{0},0,{0},0,0}, /* this pattern will end the run because cipher nb is 0 */
		};

	/* serie tested only if ECDH is available */
	cryptoParams_t ecdh_patterns[] = {
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S384},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{0},0,{0},0,{0},0,{0},0,{0},0,0}, /* this pattern will end the run because cipher nb is 0 */
	};

	/* serie tested only if OQS PQC KEM are available */
	cryptoParams_t kem_patterns[] = {
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_KYB1},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_KYB2},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_KYB3},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_SIK1},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_SIK2},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_SIK3},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},

		{{0},0,{0},0,{0},0,{0},0,{0},0,0}, /* this pattern will end the run because cipher nb is 0 */
	};

	/* serie tested only when both ECDH and OQS PQC KEM available */
	cryptoParams_t hybrid_kem_patterns[] = {
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0}, /* K255 and K448 are available only if OQS is available */
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K448},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K255_KYB512},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K255_SIK434},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K448_KYB1024},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K448_SIK751},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		
		{{0},0,{0},0,{0},0,{0},0,{0},0,0}, /* this pattern will end the run because cipher nb is 0 */
	};

	pattern = &patterns[0]; /* pattern is a pointer to current pattern */
	while (pattern->cipherNb!=0) {
		BC_ASSERT_EQUAL(multichannel_exchange(pattern, pattern, pattern, NULL, NULL, NULL, NULL), 0, int, "%x");
		pattern++; /* point to next row in the array of patterns */
	}

	/* with ECDH agreement types if available */
	if (bctbx_key_agreement_algo_list()&BCTBX_ECDH_X25519) {
		pattern = &ecdh_patterns[0]; /* pattern is a pointer to current pattern */
		while (pattern->cipherNb!=0) {
			BC_ASSERT_EQUAL(multichannel_exchange(pattern, pattern, pattern, NULL, NULL, NULL, NULL), 0, int, "%x");
			pattern++; /* point to next row in the array of patterns */
		}
	}

	/* with OQS PQC KEM agreement types if available */
	if (bctbx_key_agreement_algo_list()&BCTBX_KEM_KYBER512) {
		pattern = &kem_patterns[0]; /* pattern is a pointer to current pattern */
		while (pattern->cipherNb!=0) {
			BC_ASSERT_EQUAL(multichannel_exchange(pattern, pattern, pattern, NULL, NULL, NULL, NULL), 0, int, "%x");
			pattern++; /* point to next row in the array of patterns */
		}
	}

	/* with OQS PQC KEM agreement and ECDH types if available */
	if (bctbx_key_agreement_algo_list()&BCTBX_KEM_X25519) {
		pattern = &hybrid_kem_patterns[0]; /* pattern is a pointer to current pattern */
		while (pattern->cipherNb!=0) {
			BC_ASSERT_EQUAL(multichannel_exchange(pattern, pattern, pattern, NULL, NULL, NULL, NULL), 0, int, "%x");
			pattern++; /* point to next row in the array of patterns */
		}
	}
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), NULL, NULL, NULL, NULL), 0, int, "%x");
}

static void test_loosy_network(void) {
	int i,j;
	resetGlobalParams();
	srand((unsigned int)time(NULL));

	/* run through all the configs 10 times to maximise chance to spot a random error based on a specific packet lost sequence */
	for (j=0; j<10; j++) {
		for (i=1; i<60; i+=1) {
			resetGlobalParams();
			timeOutLimit =100000; //outrageous time limit just to be sure to complete, not run in real time anyway
			loosePacketPercentage=i;
			BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), NULL, NULL, NULL, NULL), 0, int, "%x");
			bzrtp_message("Lost packets: %f pc", (100.0*totalPacketLost)/totalPacketSent);
		}
	}
}

static void test_mtu(void) {
	cryptoParams_t *pattern;

	/* Reset Global Static settings */
	resetGlobalParams();

	cryptoParams_t patterns[] = {
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},

		{{0},0,{0},0,{0},0,{0},0,{0},0,0}, /* this pattern will end the run because cipher nb is 0 */
	};

	/* serie tested only if ECDH is available */
	cryptoParams_t ecdh_patterns[] = {
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X255},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_X448},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{0},0,{0},0,{0},0,{0},0,{0},0,0}, /* this pattern will end the run because cipher nb is 0 */
	};

	/* serie tested only if OQS PQC KEM and ECDH are available */
	cryptoParams_t hybrid_kem_patterns[] = {
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K255_KYB512},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K255_SIK434},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K448_KYB1024},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K448_SIK751},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{0},0,{0},0,{0},0,{0},0,{0},0,0}, /* this pattern will end the run because cipher nb is 0 */
	};

	pattern = &patterns[0]; /* pattern is a pointer to current pattern */
	while (pattern->cipherNb!=0) {
		BC_ASSERT_EQUAL(multichannel_exchange_mtu(pattern, pattern, pattern, 800), 0, int, "%x");
		pattern++; /* point to next row in the array of patterns */
	}

	/* with ECDH agreement types if available */
	if (bctbx_key_agreement_algo_list()&BCTBX_ECDH_X25519) {
		pattern = &ecdh_patterns[0]; /* pattern is a pointer to current pattern */
		while (pattern->cipherNb!=0) {
			BC_ASSERT_EQUAL(multichannel_exchange_mtu(pattern, pattern, pattern, 800), 0, int, "%x");
			pattern++; /* point to next row in the array of patterns */
		}
	}

	/* with OQS PQC KEM agreement and ECDH types if available */
	if (bctbx_key_agreement_algo_list()&BCTBX_KEM_X25519) {
		pattern = &hybrid_kem_patterns[0]; /* pattern is a pointer to current pattern */
		while (pattern->cipherNb!=0) {
			BC_ASSERT_EQUAL(multichannel_exchange_mtu(pattern, pattern, pattern, 800), 0, int, "%x");
			pattern++; /* point to next row in the array of patterns */
		}
	}
}

static void test_loosy_network_mtu(void) {
	if (!(bctbx_key_agreement_algo_list()&BCTBX_KEM_X25519)) {
		bctbx_warning("mtu test on loosy network skipped as we do not support key exchange requesting fragmentation");
		return;
	}

	int i,j;
	resetGlobalParams();
	srand((unsigned int)time(NULL));
	cryptoParams_t pattern = {{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_K448_KYB1024},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0};

	/* run through all the configs 10 times to maximise chance to spot a random error based on a specific packet lost sequence */
	for (j=0; j<10; j++) {
		for (i=1; i<60; i+=1) {
			resetGlobalParams();
			timeOutLimit =100000; //outrageous time limit just to be sure to complete, not run in real time anyway
			loosePacketPercentage=i;
			BC_ASSERT_EQUAL(multichannel_exchange_mtu(&pattern, &pattern, &pattern, 800), 0, int, "%x");
			bzrtp_message("Lost packets: %f pc", (100.0*totalPacketLost)/totalPacketSent);
		}
	}
}

static void test_cache_enabled_exchange(void) {
#ifdef ZIDCACHE_ENABLED
	sqlite3 *aliceDB=NULL;
	sqlite3 *bobDB=NULL;
	uint8_t selfZIDalice[12];
	uint8_t selfZIDbob[12];
	int zuidAlice=0,zuidBob=0;
	const char *colNames[] = {"rs1", "rs2", "pvs"};
	uint8_t *colValuesAlice[3];
	size_t colLengthAlice[3];
	uint8_t *colValuesBob[3];
	size_t colLengthBob[3];
	int i;
	char *aliceTesterFile = bc_tester_file("tmpZIDAlice_simpleCache.sqlite");
	char *bobTesterFile = bc_tester_file("tmpZIDBob_simpleCache.sqlite");

	resetGlobalParams();

	/* create tempory DB files, just try to clean them from dir before, just in case  */
	remove(aliceTesterFile);
	remove(bobTesterFile);
	bzrtptester_sqlite3_open(aliceTesterFile, &aliceDB);
	bzrtptester_sqlite3_open(bobTesterFile, &bobDB);

	/* make a first exchange */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), 0, int, "%x");

	/* after the first exchange we shall have both pvs values at 1 and both rs1 identical and rs2 null, retrieve them from cache and check it */
	/* first get each ZIDs, note give NULL as RNG context may lead to segfault in case of error(caches were not created correctly)*/
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock((void *)aliceDB, "alice@sip.linphone.org", selfZIDalice, NULL, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock((void *)bobDB, "bob@sip.linphone.org", selfZIDbob, NULL, NULL), 0, int, "%x");
	/* then get the matching zuid in cache */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", selfZIDbob, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidAlice, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)bobDB, "bob@sip.linphone.org", "alice@sip.linphone.org", selfZIDalice, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidBob, NULL), 0, int, "%x");
	/* retrieve the values */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3, NULL), 0, int, "%x");
	/* and compare to expected */
	/* rs1 is set and they are both the same */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	/* rs2 is unset(NULL) */
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[1], 0,  size_t, "%zu");
	BC_ASSERT_PTR_NULL(colValuesAlice[1]);
	BC_ASSERT_PTR_NULL(colValuesBob[1]);
	/* pvs is equal to 1 */
	BC_ASSERT_EQUAL(colLengthAlice[2], 1,  size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[2], 1,  size_t, "%zu");
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesAlice[i]);
		colValuesAlice[i]=NULL;
	}

	/* make a second exchange */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), 0, int, "%x");
	/* read new values in cache, ZIDs and zuids must be identical, read alice first to be able to check rs2 with old rs1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3, NULL), 0, int, "%x");
	/* check what is now rs2 is the old rs1 */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthAlice[1], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[1], colValuesBob[0], 32), 0, int, "%d"); /* colValuesBob, still old values from before the second exchange */

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesBob[i]);
		colValuesBob[i]=NULL;
	}
	/* so read bob updated values and compare rs1, rs2 and check pvs is still at 1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[1], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[1], colValuesBob[1], 32), 0, int, "%d");
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesAlice[i]);
		free(colValuesBob[i]);
	}
	sqlite3_close(aliceDB);
	sqlite3_close(bobDB);

	/* clean temporary files */
	remove(aliceTesterFile);
	remove(bobTesterFile);
	bc_free(aliceTesterFile);
	bc_free(bobTesterFile);
#else /* ZIDCACHE_ENABLED */
	bzrtp_message("Test skipped as ZID cache is disabled\n");
#endif /* ZIDCACHE_ENABLED */
}

/* first perform an exchange to establish a correct shared cache, then modify one of them and perform an other exchange to check we have a cache mismatch warning */
static void test_cache_mismatch_exchange(void) {
#ifdef ZIDCACHE_ENABLED
	sqlite3 *aliceDB=NULL;
	sqlite3 *bobDB=NULL;
	uint8_t selfZIDalice[12];
	uint8_t selfZIDbob[12];
	int zuidAlice=0,zuidBob=0;
	const char *colNames[] = {"rs1", "rs2", "pvs"};
	uint8_t *colValuesAlice[3];
	size_t colLengthAlice[3];
	uint8_t *colValuesBob[3];
	size_t colLengthBob[3];
	int i;
	char *aliceTesterFile = bc_tester_file("tmpZIDAlice_cacheMismatch.sqlite");
	char *bobTesterFile = bc_tester_file("tmpZIDBob_cacheMismatch.sqlite");

	resetGlobalParams();

	/* create tempory DB files, just try to clean them from dir before, just in case  */
	remove(aliceTesterFile);
	remove(bobTesterFile);
	bzrtptester_sqlite3_open(aliceTesterFile, &aliceDB);
	bzrtptester_sqlite3_open(bobTesterFile, &bobDB);

	/* make a first exchange */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), 0, int, "%x");

	/* after the first exchange we shall have both pvs values at 1 and both rs1 identical and rs2 null, retrieve them from cache and check it */
	/* first get each ZIDs, note give NULL as RNG context may lead to segfault in case of error(caches were not created correctly)*/
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock((void *)aliceDB, "alice@sip.linphone.org", selfZIDalice, NULL, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock((void *)bobDB, "bob@sip.linphone.org", selfZIDbob, NULL, NULL), 0, int, "%x");
	/* then get the matching zuid in cache */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", selfZIDbob, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidAlice, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)bobDB, "bob@sip.linphone.org", "alice@sip.linphone.org", selfZIDalice, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidBob, NULL), 0, int, "%x");
	/* retrieve the values */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3, NULL), 0, int, "%x");
	/* and compare to expected */
	/* rs1 is set and they are both the same */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	/* rs2 is unset(NULL) */
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[1], 0, size_t, "%zu");
	BC_ASSERT_PTR_NULL(colValuesAlice[1]);
	BC_ASSERT_PTR_NULL(colValuesBob[1]);
	/* pvs is equal to 1 */
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* Modify Alice cache rs1 first byte value, it will cause a cache mismatch at next exchange */
	colValuesAlice[0][0] += 1;
	BC_ASSERT_EQUAL(bzrtp_cache_write_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 1, NULL), 0, int, "%x");

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesAlice[i]);
		colValuesAlice[i]=NULL;
		free(colValuesBob[i]);
		colValuesBob[i]=NULL;
	}

	/* make a third exchange : we have a cache mismatch(on Bob side only), wich means rs1 will not be backed up in rs2 which shall be NULL again */
	/* make a second exchange : we have a cache mismatch(both on Bob and Alice side), wich means rs1 will not be backed up in rs2 which shall be NULL again */
	/* rs1 will be in sync has the SAS comparison will succeed and pvs will be set to 1*/
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), RET_CACHE_MISMATCH<<16|RET_CACHE_MISMATCH, int, "%x");

	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3, NULL), 0, int, "%x");

	BC_ASSERT_EQUAL(colLengthAlice[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[1], 0, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	BC_ASSERT_PTR_NULL(colValuesAlice[1]);
	BC_ASSERT_PTR_NULL(colValuesBob[1]);
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* Delete Alice cache rs1 first byte value, it will cause a cache mismatch at next exchange but only on Bob's side as Alice will not expect any valid cache */
	free(colValuesAlice[0]);
	colValuesAlice[0] = NULL;
	colLengthAlice[0] = 0;
	colValuesAlice[2][0] = 0; /* reset pvs to 0 */
	BC_ASSERT_EQUAL(bzrtp_cache_write_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3, NULL), 0, int, "%x");

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesAlice[i]);
		colValuesAlice[i]=NULL;
		free(colValuesBob[i]);
		colValuesBob[i]=NULL;
	}

	/* make a third exchange : we have a cache mismatch(on Bob side only), wich means rs1 will not be backed up in rs2 which shall be NULL again */
	/* rs1 will be in sync has the SAS comparison will succeed and pvs will be set to 1*/
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), RET_CACHE_MISMATCH<<16, int, "%x");

	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3, NULL), 0, int, "%x");

	BC_ASSERT_EQUAL(colLengthAlice[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[1], 0, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, size_t, "%zu");
	BC_ASSERT_PTR_NULL(colValuesAlice[1]);
	BC_ASSERT_PTR_NULL(colValuesBob[1]);
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesAlice[i]);
		free(colValuesBob[i]);
	}
	sqlite3_close(aliceDB);
	sqlite3_close(bobDB);

	/* clean temporary files */
	remove(aliceTesterFile);
	remove(bobTesterFile);
	bc_free(aliceTesterFile);
	bc_free(bobTesterFile);
#else /* ZIDCACHE_ENABLED */
	bzrtp_message("Test skipped as ZID cache is disabled\n");
#endif /* ZIDCACHE_ENABLED */
}

static void test_cache_sas_not_confirmed(void) {
#ifdef ZIDCACHE_ENABLED
	sqlite3 *aliceDB=NULL;
	sqlite3 *bobDB=NULL;
	uint8_t selfZIDalice[12];
	uint8_t selfZIDbob[12];
	int zuidAlice=0,zuidBob=0;
	const char *colNames[] = {"rs1", "rs2", "pvs"};
	uint8_t *colValuesAlice[3];
	size_t colLengthAlice[3];
	uint8_t *colValuesBob[3];
	size_t colLengthBob[3];
	int i;
	char *aliceTesterFile = bc_tester_file("tmpZIDAlice_cacheSASNotConfirmed.sqlite");
	char *bobTesterFile = bc_tester_file("tmpZIDBob_cacheSasNotConfirmed.sqlite");

	resetGlobalParams();

	/* init columns values pointers */
	for (i=0; i<3; i++) {
		colValuesAlice[i] = NULL;
		colValuesBob[i] = NULL;
	}

	/* create tempory DB files, just try to clean them from dir before, just in case  */
	remove(aliceTesterFile);
	remove(bobTesterFile);
	bzrtptester_sqlite3_open(aliceTesterFile, &aliceDB);
	bzrtptester_sqlite3_open(bobTesterFile, &bobDB);

	/* make a first exchange, Alice is instructed to not validate the SAS */
	BC_ASSERT_EQUAL(multichannel_exchange_pvs_params(defaultCryptoAlgoSelectionNoSASValidation(), defaultCryptoAlgoSelection(), defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org", TRUE, 0, 0), 0, int, "%x");

	/* after the first exchange we shall have alice pvs at 0 and bob at 1 and both rs1 identical and rs2 null, retrieve them from cache and check it */
	/* first get each ZIDs, note give NULL as RNG context may lead to segfault in case of error(caches were not created correctly)*/
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock((void *)aliceDB, "alice@sip.linphone.org", selfZIDalice, NULL, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock((void *)bobDB, "bob@sip.linphone.org", selfZIDbob, NULL, NULL), 0, int, "%x");
	/* then get the matching zuid in cache */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", selfZIDbob, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidAlice, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)bobDB, "bob@sip.linphone.org", "alice@sip.linphone.org", selfZIDalice, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidBob, NULL), 0, int, "%x");
	/* retrieve the values */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3, NULL), 0, int, "%x");
	/* and compare to expected */
	/* rs1 is set and they are both the same */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	/* rs2 is unset(NULL) */
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[1], 0, size_t, "%zu");
	BC_ASSERT_PTR_NULL(colValuesAlice[1]);
	BC_ASSERT_PTR_NULL(colValuesBob[1]);
	/* pvs is equal to 0 for Alice(actually NULL, so length is 0 and has no value which is considered 0 by the getPeerSecrets function) and 1 for Bob */
	BC_ASSERT_EQUAL(colLengthAlice[2], 0, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesAlice[i]);
		colValuesAlice[i] = NULL;
	}

	/* make a second exchange, the PVS flag returned by both side shall be 0 as Alice did not validate hers on previous exchange */
	/* but let them both validate this one */
	BC_ASSERT_EQUAL(multichannel_exchange_pvs_params(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org", TRUE, 0, 0), 0, int, "%x");
	/* read new values in cache, ZIDs and zuids must be identical, read alice first to be able to check rs2 with old rs1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3, NULL), 0, int, "%x");
	/* check what is now rs2 is the old rs1 */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthAlice[1], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[1], colValuesBob[0], 32), 0, int, "%d"); /* colValuesBob, still old values from before the second exchange */

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesBob[i]);
		colValuesBob[i] = NULL;
	}

	/* so read bob updated values and compare rs1, rs2 and check pvs is at 1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[1], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[1], colValuesBob[1], 32), 0, int, "%d");
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesAlice[i]);
		colValuesAlice[i] = NULL;
	}

	/* make a third exchange, the PVS flag returned by both side shall be 1 */
	BC_ASSERT_EQUAL(multichannel_exchange_pvs_params(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org", TRUE, 1, 1), 0, int, "%x");
	/* read new values in cache, ZIDs and zuids must be identical, read alice first to be able to check rs2 with old rs1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3, NULL), 0, int, "%x");
	/* check what is now rs2 is the old rs1 */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthAlice[1], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[1], colValuesBob[0], 32), 0, int, "%d"); /* colValuesBob, still old values from before the second exchange */

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesBob[i]);
		colValuesBob[i] = NULL;
	}
	/* so read bob updated values and compare rs1, rs2 and check pvs is at 1 */
	/* so read bob updated values and compare rs1, rs2 and check pvs is still at 1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[1], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[1], colValuesBob[1], 32), 0, int, "%d");
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesAlice[i]);
		free(colValuesBob[i]);
	}
	sqlite3_close(aliceDB);
	sqlite3_close(bobDB);

	/* clean temporary files */
	remove(aliceTesterFile);
	remove(bobTesterFile);
	bc_free(aliceTesterFile);
	bc_free(bobTesterFile);
#else /* ZIDCACHE_ENABLED */
	bzrtp_message("Test skipped as ZID cache is disabled\n");
#endif /* ZIDCACHE_ENABLED */
}

static int test_auxiliary_secret_params(uint8_t *aliceAuxSecret, size_t aliceAuxSecretLength, uint8_t *bobAuxSecret, size_t bobAuxSecretLength, uint8_t aliceExpectedAuxSecretMismatch, uint8_t bobExpectedAuxSecretMismatch, uint8_t badTimingFlag) {
	int retval;
	clientContext_t Alice,Bob;
	uint64_t initialTime=0;
	uint64_t lastPacketSentTime=0;
	uint32_t aliceSSRC = ALICE_SSRC_BASE;
	uint32_t bobSSRC = BOB_SSRC_BASE;
	uint8_t setAuxSecretFlag=0; // switch to 1 once we've set the aux secret

	/*** Create the main channel */
	if ((retval=setUpClientContext(&Alice, ALICE, aliceSSRC, NULL, NULL, NULL, NULL, NULL))!=0) {
		bzrtp_message("ERROR: can't init setup client context id %d\n", ALICE);
		BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
		return -1;
	}

	if ((retval=setUpClientContext(&Bob, BOB, bobSSRC, NULL, NULL, NULL, NULL, NULL))!=0) {
		bzrtp_message("ERROR: can't init setup client context id %d\n", BOB);
		BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
		return -1;
	}

	/*** Setup a transient auxiliary secret ***/
	if (badTimingFlag==0) {
		setAuxSecretFlag=1;
		if (aliceAuxSecret != NULL) {
			if ((retval = bzrtp_setAuxiliarySharedSecret(Alice.bzrtpContext, aliceAuxSecret, aliceAuxSecretLength))!=0) {
				bzrtp_message("ERROR: can't set Auxiliary shared secret. id is %d\n", ALICE);
				BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
				return -1;
			}
		}

		if (bobAuxSecret != NULL) {
			if ((retval = bzrtp_setAuxiliarySharedSecret(Bob.bzrtpContext, bobAuxSecret, bobAuxSecretLength))!=0) {
				bzrtp_message("ERROR: can't set Auxiliary shared secret. id is %d\n", BOB);
				BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
				return -1;
			}
		}
	}

	/* start the ZRTP engine(it will send a hello packet )*/
	if ((retval = bzrtp_startChannelEngine(Alice.bzrtpContext, aliceSSRC))!=0) {
		bzrtp_message("ERROR: bzrtp_startChannelEngine returned %0x, client id is %d SSRC is %d\n", retval, ALICE, aliceSSRC);
		BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
		return -1;
	}
	if ((retval = bzrtp_startChannelEngine(Bob.bzrtpContext, bobSSRC))!=0) {
		bzrtp_message("ERROR: bzrtp_startChannelEngine returned %0x, client id is %d SSRC is %d\n", retval, BOB, bobSSRC);
		BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
		return -1;
	}

	initialTime = getSimulatedTime();

	while ((bzrtp_getChannelStatus(Alice.bzrtpContext, aliceSSRC)!= BZRTP_CHANNEL_SECURE || bzrtp_getChannelStatus(Bob.bzrtpContext, bobSSRC)!= BZRTP_CHANNEL_SECURE) && (getSimulatedTime()-initialTime<timeOutLimit)){
		int i;
		/* check the message queue */
		for (i=0; i<aliceQueueIndex; i++) {
			retval = bzrtp_processMessage(Alice.bzrtpContext, aliceSSRC, aliceQueue[i].packetString, aliceQueue[i].packetLength);
			//bzrtp_message("%ld Alice processed a %.8s and returns %x\n", msSTC, (aliceQueue[i].packetString)+16, retval);
			memset(aliceQueue[i].packetString, 0, MAX_PACKET_LENGTH); /* destroy the packet after sending it to the ZRTP engine */
			lastPacketSentTime=getSimulatedTime();
		}
		aliceQueueIndex = 0;

		for (i=0; i<bobQueueIndex; i++) {
			retval = bzrtp_processMessage(Bob.bzrtpContext, bobSSRC, bobQueue[i].packetString, bobQueue[i].packetLength);
			//bzrtp_message("%ld Bob processed a %.8s and returns %x\n",msSTC, (bobQueue[i].packetString)+16, retval);
			memset(bobQueue[i].packetString, 0, MAX_PACKET_LENGTH); /* destroy the packet after sending it to the ZRTP engine */
			lastPacketSentTime=getSimulatedTime();
		}
		bobQueueIndex = 0;


		/* send the actual time to the zrtpContext */
		retval = bzrtp_iterate(Alice.bzrtpContext, aliceSSRC, getSimulatedTime());
		retval = bzrtp_iterate(Bob.bzrtpContext, bobSSRC, getSimulatedTime());

		/* sleep for 10 ms */
		STC_sleep(10);

		/* check if we shall try to reset re-emission timers */
		if (getSimulatedTime()-lastPacketSentTime > 1250 ) { /*higher re-emission timeout is 1200ms */
			retval = bzrtp_resetRetransmissionTimer(Alice.bzrtpContext, aliceSSRC);
			retval +=bzrtp_resetRetransmissionTimer(Bob.bzrtpContext, bobSSRC);
			lastPacketSentTime=getSimulatedTime();

		}

		if (badTimingFlag!=0 && setAuxSecretFlag < 2) { /* after the HelloPacket exchange has occurs, insert the auxSecret if we have the badTiming flag on */
			setAuxSecretFlag ++;
			if (setAuxSecretFlag == 2) { // first time we process a clock tick will be sending Hello Message, at the second one we will already have processed them and it will be too late
				if (aliceAuxSecret != NULL) {
					BC_ASSERT_NOT_EQUAL(bzrtp_setAuxiliarySharedSecret(Alice.bzrtpContext, aliceAuxSecret, aliceAuxSecretLength), 0, int, "%d"); // we expect this insert to be rejected
				}

				if (bobAuxSecret != NULL) {
					BC_ASSERT_NOT_EQUAL(bzrtp_setAuxiliarySharedSecret(Bob.bzrtpContext, bobAuxSecret, bobAuxSecretLength), 0, int, "%d"); // we expect this insert to be rejected
				}
			}
	}

	}
	if ((retval=bzrtp_getChannelStatus(Alice.bzrtpContext, aliceSSRC))!=BZRTP_CHANNEL_SECURE) {
		bzrtp_message("Fail Alice on channel1 loss rate is %d", loosePacketPercentage);
		BC_ASSERT_EQUAL(retval, BZRTP_CHANNEL_SECURE, int, "%0x");
		return -1;
	}
	if ((retval=bzrtp_getChannelStatus(Bob.bzrtpContext, bobSSRC))!=BZRTP_CHANNEL_SECURE) {
		bzrtp_message("Fail Bob on channel1 loss rate is %d", loosePacketPercentage);
		BC_ASSERT_EQUAL(retval, BZRTP_CHANNEL_SECURE, int, "%0x");
		return -1;
	}

	bzrtp_message("ZRTP algo used during negotiation: Cipher: %s - KeyAgreement: %s - Hash: %s - AuthTag: %s - Sas Rendering: %s\n", bzrtp_algoToString(Alice.secrets->cipherAlgo), bzrtp_algoToString(Alice.secrets->keyAgreementAlgo), bzrtp_algoToString(Alice.secrets->hashAlgo), bzrtp_algoToString(Alice.secrets->authTagAlgo), bzrtp_algoToString(Alice.secrets->sasAlgo));

	if ((retval=compareSecrets(Alice.secrets, Bob.secrets, TRUE))!=0) {
		BC_ASSERT_EQUAL(retval, 0, int, "%d");
		return -1;
	}

	// check aux secrets mismatch flag, they must be in sync
	if (Alice.secrets->auxSecretMismatch != Bob.secrets->auxSecretMismatch) {
		// if one is unset(AuxSecret is null so flag is at unset) then other can be unset(caught by previous if) or mismatch(this one)
		if (!( 	(Alice.secrets->auxSecretMismatch == BZRTP_AUXSECRET_UNSET
				&& aliceAuxSecret == NULL
				&& Bob.secrets->auxSecretMismatch == BZRTP_AUXSECRET_MISMATCH)
			|| (Bob.secrets->auxSecretMismatch == BZRTP_AUXSECRET_UNSET
				&& bobAuxSecret == NULL
				&& Alice.secrets->auxSecretMismatch == BZRTP_AUXSECRET_MISMATCH)))
		{
			BC_FAIL("computed auxSecretMismatch flags differ from Alice to Bob");
			return -1;
		}
	}

	// Do we have the expected mismatch on aux secret
	BC_ASSERT_EQUAL(Alice.secrets->auxSecretMismatch, aliceExpectedAuxSecretMismatch, uint8_t, "%d");
	BC_ASSERT_EQUAL(Bob.secrets->auxSecretMismatch, bobExpectedAuxSecretMismatch, uint8_t, "%d");

	/*** Destroy Contexts ***/
	while (bzrtp_destroyBzrtpContext(Alice.bzrtpContext, aliceSSRC)>0 && aliceSSRC>=ALICE_SSRC_BASE) {
		aliceSSRC--;
	}
	while (bzrtp_destroyBzrtpContext(Bob.bzrtpContext, bobSSRC)>0 && bobSSRC>=BOB_SSRC_BASE) {
		bobSSRC--;
	}

	return 0;
}

static void test_auxiliary_secret(void) {
	uint8_t secret1[] = {0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x9a, 0x00, 0xff};
	uint8_t secret2[] = {0xfe, 0xed, 0xdc, 0xcb, 0xba, 0xa9, 0x98, 0x87, 0x76, 0x65, 0x54, 0x43};

	resetGlobalParams();

	// matching cases (expect mismatch flag to be 0)
	BC_ASSERT_EQUAL(test_auxiliary_secret_params(secret1, sizeof(secret1), secret1, sizeof(secret1), BZRTP_AUXSECRET_MATCH, BZRTP_AUXSECRET_MATCH, 0), 0, int, "%d");
	BC_ASSERT_EQUAL(test_auxiliary_secret_params(secret2, sizeof(secret2), secret2, sizeof(secret2), BZRTP_AUXSECRET_MATCH, BZRTP_AUXSECRET_MATCH, 0), 0, int, "%d");

	// mismatching cases (expect mismatch flag to be 1)
	// different secrets
	BC_ASSERT_EQUAL(test_auxiliary_secret_params(secret1, sizeof(secret1), secret2, sizeof(secret2), BZRTP_AUXSECRET_MISMATCH, BZRTP_AUXSECRET_MISMATCH, 0), 0, int, "%d");
	// only one side has a secret
	BC_ASSERT_EQUAL(test_auxiliary_secret_params(secret1, sizeof(secret1), NULL, 0, BZRTP_AUXSECRET_MISMATCH, BZRTP_AUXSECRET_UNSET, 0), 0, int, "%d");
	// no one has a secret
	BC_ASSERT_EQUAL(test_auxiliary_secret_params(NULL, 0, NULL, 0, BZRTP_AUXSECRET_UNSET, BZRTP_AUXSECRET_UNSET, 0), 0, int, "%d");
	// same secret but one is one byte shorter
	BC_ASSERT_EQUAL(test_auxiliary_secret_params(secret1, sizeof(secret1)-1, secret1, sizeof(secret1), BZRTP_AUXSECRET_MISMATCH, BZRTP_AUXSECRET_MISMATCH, 0), 0, int, "%d");

	// matching secret, but inserted to late(last param is a flag to do that) so we expect unset
	BC_ASSERT_EQUAL(test_auxiliary_secret_params(secret1, sizeof(secret1), secret1, sizeof(secret1), BZRTP_AUXSECRET_UNSET, BZRTP_AUXSECRET_UNSET, 1), 0, int, "%d");
};

/**
 * scenario:
 *  - create new users with empty zid cache
 *  - start a firt exchange but abort it before conclusion
 *  - make a successive exchange going correctly to the end
 */
static void test_abort_retry(void) {
#ifdef ZIDCACHE_ENABLED
	sqlite3 *aliceDB=NULL;
	sqlite3 *bobDB=NULL;
	uint8_t selfZIDalice[12];
	uint8_t selfZIDbob[12];
	int zuidAlice=0,zuidBob=0;
	const char *colNames[] = {"rs1", "rs2", "pvs"};
	uint8_t *colValuesAlice[3];
	size_t colLengthAlice[3];
	uint8_t *colValuesBob[3];
	size_t colLengthBob[3];
	int i;
	char *aliceTesterFile = bc_tester_file("tmpZIDAlice_abortRetry.sqlite");
	char *bobTesterFile = bc_tester_file("tmpZIDBob_abortRetry.sqlite");


	resetGlobalParams();

	/* create tempory DB files, just try to clean them from dir before, just in case  */
	remove(aliceTesterFile);
	remove(bobTesterFile);
	bzrtptester_sqlite3_open(aliceTesterFile, &aliceDB);
	bzrtptester_sqlite3_open(bobTesterFile, &bobDB);

	/* make a first exchange but abort it */
	timeOutLimit = ABORT_NEGOTIATION_TIMEOUT; /* set timeout to ABORT_NEGOTIATION_TIMEOUT aborts an ongoing negotiation without errors */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), 0, int, "%x");

	/* after the first exchange we shall have only self ZID, peer ZID must not be inserted in cache */
	/* first get each ZIDs, note give NULL as RNG context may lead to segfault in case of error(caches were not created correctly)*/
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock((void *)aliceDB, "alice@sip.linphone.org", selfZIDalice, NULL, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock((void *)bobDB, "bob@sip.linphone.org", selfZIDbob, NULL, NULL), 0, int, "%x");
	/* try to get the matching zuid in cache: it shall not be there as the negotiation didn't completed */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", selfZIDbob, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidAlice, NULL), BZRTP_ERROR_CACHE_PEERNOTFOUND, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)bobDB, "bob@sip.linphone.org", "alice@sip.linphone.org", selfZIDalice, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidBob, NULL), BZRTP_ERROR_CACHE_PEERNOTFOUND, int, "%x");

	/* make a second exchange */
	resetGlobalParams(); /* this one goes to the end of it */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), 0, int, "%x");

	/* after the exchange we shall have both pvs values at 1 and both rs1 identical and rs2 null, retrieve them from cache and check it */
	/* first get each ZIDs, note give NULL as RNG context may lead to segfault in case of error(caches were not created correctly)*/
	/* get the matching zuid in cache */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", selfZIDbob, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidAlice, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)bobDB, "bob@sip.linphone.org", "alice@sip.linphone.org", selfZIDalice, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidBob, NULL), 0, int, "%x");

	if (zuidAlice==0 || zuidBob==0) {//abort if we didn't retrieve valid zuid values, keep tmp sqlite files for inspection
		sqlite3_close(aliceDB);
		sqlite3_close(bobDB);

		return;
	}

	/* retrieve the values */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3, NULL), 0, int, "%x");
	/* and compare to expected */
	/* rs1 is set and they are both the same */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	/* rs2 is unset(NULL) */
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[1], 0, size_t, "%zu");
	BC_ASSERT_PTR_NULL(colValuesAlice[1]);
	BC_ASSERT_PTR_NULL(colValuesBob[1]);
	/* pvs is equal to 1 */
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, size_t, "%zu");
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesBob[i]);
		colValuesBob[i]=NULL;
	}

	/* free buffers */
	for (i=0; i<3; i++) {
		free(colValuesAlice[i]);
		free(colValuesBob[i]);
	}
	sqlite3_close(aliceDB);
	sqlite3_close(bobDB);

	/* clean temporary files */
	remove(aliceTesterFile);
	remove(bobTesterFile);
	bc_free(aliceTesterFile);
	bc_free(bobTesterFile);
#else /* ZIDCACHE_ENABLED */
	bzrtp_message("Test skipped as ZID cache is disabled\n");
#endif /* ZIDCACHE_ENABLED */
}

static void test_active_flag(void) {
#ifdef ZIDCACHE_ENABLED
	sqlite3 *aliceDB=NULL;
	sqlite3 *bob1DB=NULL;
	sqlite3 *bob2DB=NULL;
	sqlite3 *bob3DB=NULL;
	sqlite3 *claire1DB=NULL;
	sqlite3 *claire2DB=NULL;
	char *aliceTesterFile = bc_tester_file("tmpZIDAlice_activeFlag.sqlite");
	char *bob1TesterFile = bc_tester_file("tmpZIDBob1_activeFlag.sqlite");
	char *bob2TesterFile = bc_tester_file("tmpZIDBob2_activeFlag.sqlite");
	char *bob3TesterFile = bc_tester_file("tmpZIDBob3_activeFlag.sqlite");
	char *claire1TesterFile = bc_tester_file("tmpZIDClaire1_activeFlag.sqlite");
	char *claire2TesterFile = bc_tester_file("tmpZIDClaire2_activeFlag.sqlite");

	resetGlobalParams();

	/* create tempory DB files, just try to clean them from dir before, just in case  */
	remove(aliceTesterFile);
	remove(bob1TesterFile);
	remove(bob2TesterFile);
	remove(bob3TesterFile);
	remove(claire1TesterFile);
	remove(claire2TesterFile);
	bzrtptester_sqlite3_open(aliceTesterFile, &aliceDB);
	bzrtptester_sqlite3_open(bob1TesterFile, &bob1DB);
	bzrtptester_sqlite3_open(bob2TesterFile, &bob2DB);
	bzrtptester_sqlite3_open(bob3TesterFile, &bob3DB);
	bzrtptester_sqlite3_open(claire1TesterFile, &claire1DB);
	bzrtptester_sqlite3_open(claire2TesterFile, &claire2DB);

	/* make a first exchange alice <-> bob1, validate the SAS */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bob1DB, "bob@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall be valid(bob1 is active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_VALID, int, "%x");

	/* make an exchange, alice <-> bob2, alice is instructed to not validate the SAS nor invalidate it */
	BC_ASSERT_EQUAL(multichannel_exchange(defaultCryptoAlgoSelectionNoSASValidation(), defaultCryptoAlgoSelection(), defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bob2DB, "bob@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall be unknown(as it is the first exchange with bob2 which is now active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_UNKNOWN, int, "%x");

	/* make an exchange, alice <-> bob1, alice is instructed to not validate the SAS nor invalidate it */
	BC_ASSERT_EQUAL(multichannel_exchange(defaultCryptoAlgoSelectionNoSASValidation(), defaultCryptoAlgoSelection(), defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bob1DB, "bob@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall be valid(bob1 is now active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_VALID, int, "%x");

	/* make an exchange, alice <-> bob1, alice is instructed to reset the SAS */
	BC_ASSERT_EQUAL(multichannel_exchange(defaultCryptoAlgoSelectionResetSAS(), defaultCryptoAlgoSelection(), defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bob1DB, "bob@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall be invalid(bob1 is still active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_INVALID, int, "%x");

	/* make an exchange, alice <-> bob2, alice is instructed to not validate the SAS nor invalidate it */
	BC_ASSERT_EQUAL(multichannel_exchange(defaultCryptoAlgoSelectionNoSASValidation(), defaultCryptoAlgoSelection(), defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bob2DB, "bob@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall be unknown(as it is the first exchange with bob2 which is now active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_UNKNOWN, int, "%x");

	/* make an exchange alice <-> bob2, validate the SAS */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bob2DB, "bob@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall be valid (bob2 is now active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_VALID, int, "%x");

	/* make an exchange, alice <-> bob1, alice is instructed to not validate the SAS nor invalidate it */
	BC_ASSERT_EQUAL(multichannel_exchange(defaultCryptoAlgoSelectionNoSASValidation(), defaultCryptoAlgoSelection(), defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bob1DB, "bob@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall be invalid(bob1 is now active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_INVALID, int, "%x");

	/* make an exchange, alice <-> bob3, alice is instructed to not validate the SAS nor invalidate it */
	BC_ASSERT_EQUAL(multichannel_exchange(defaultCryptoAlgoSelectionNoSASValidation(), defaultCryptoAlgoSelection(), defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bob3DB, "bob@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall be unknown(as it is the first exchange with bob3 which is now active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_UNKNOWN, int, "%x");


	/* introducing Claire */
	/* ask alice what is the pvs status of the active claire uri: claire@sip.linphone.org, it shall still be unknown as alice never heard about clairee yet */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "claire@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_UNKNOWN, int, "%x");

	/* make an exchange, alice <-> claire1, alice is instructed to reset the SAS */
	BC_ASSERT_EQUAL(multichannel_exchange(defaultCryptoAlgoSelectionResetSAS(), defaultCryptoAlgoSelection(), defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", claire1DB, "claire@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall still be unknown(bob3 is still active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_UNKNOWN, int, "%x");
	/* ask alice what is the pvs status of the active claire uri: claire@sip.linphone.org, it shall still be invalid */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "claire@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_INVALID, int, "%x");

	/* make a first exchange alice <-> claire2, validate the SAS */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", claire2DB, "claire@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall still be unknown(bob3 is still active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_UNKNOWN, int, "%x");
	/* ask alice what is the pvs status of the active claire uri: claire@sip.linphone.org, it shall still be valid */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "claire@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_VALID, int, "%x");

	/* make an exchange, alice <-> bob1, alice is instructed to not validate the SAS nor invalidate it */
	BC_ASSERT_EQUAL(multichannel_exchange(defaultCryptoAlgoSelectionNoSASValidation(), defaultCryptoAlgoSelection(), defaultCryptoAlgoSelection(), aliceDB, "alice@sip.linphone.org", bob1DB, "bob@sip.linphone.org"), 0, int, "%x");
	/* ask alice what is the pvs status of the active bob uri: bob@sip.linphone.org, it shall be invalid(bob1 is now active) */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "bob@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_INVALID, int, "%x");
	/* ask alice what is the pvs status of the active claire uri: claire@sip.linphone.org, it shall still be valid */
	BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(aliceDB, "claire@sip.linphone.org", NULL), BZRTP_CACHE_PEER_STATUS_VALID, int, "%x");

	sqlite3_close(aliceDB);
	sqlite3_close(bob1DB);
	sqlite3_close(bob2DB);
	sqlite3_close(bob3DB);
	sqlite3_close(claire1DB);
	sqlite3_close(claire2DB);

	/* clean temporary files */
	remove(aliceTesterFile);
	remove(bob1TesterFile);
	remove(bob2TesterFile);
	remove(bob3TesterFile);
	remove(claire1TesterFile);
	remove(claire2TesterFile);
	bc_free(aliceTesterFile);
	bc_free(bob1TesterFile);
	bc_free(bob2TesterFile);
	bc_free(bob3TesterFile);
	bc_free(claire1TesterFile);
	bc_free(claire2TesterFile);
#else /* ZIDCACHE_ENABLED */
	bzrtp_message("Test skipped as ZID cache is disabled\n");
#endif /* ZIDCACHE_ENABLED */
}



/*
 * Scenario:
 * - one thread runs exchanges
 * - one thread requests peerStatus
 */
#ifdef ZIDCACHE_ENABLED
struct thread_argument {
	sqlite3 *db;
	bctbx_mutex_t *dbMutex;
	char *peerUri;
	int expectedStatus;
	uint64_t timeout;
};


static void *test_cache_concurrent_access_getPeerStatus(void *arg) {
	struct thread_argument *param = arg;

	while (getSimulatedTime()<param->timeout) {
		BC_ASSERT_EQUAL(bzrtp_cache_getPeerStatus_lock(param->db, param->peerUri, param->dbMutex), param->expectedStatus, int, "%x");
		bctbx_sleep_ms(10);
	}
	return NULL;
}
#endif /* ZIDCACHE_ENABLED */

static void test_cache_concurrent_access(void) {
#ifdef ZIDCACHE_ENABLED
	sqlite3 *aliceDB=NULL;
	sqlite3 *bobDB=NULL;
	sqlite3 *aliceDB2=NULL;
	sqlite3 *bobDB2=NULL;
	uint64_t timeout = 20000; // run a 20s simulation
	bctbx_mutex_t aliceMutex, bobMutex;
	struct thread_argument aliceParams,bobParams;
	bctbx_thread_t aliceThreadId, bobThreadId;
	char *aliceTesterFile = bc_tester_file("tmpZIDAlice_concurrentAccess.sqlite");
	char *bobTesterFile = bc_tester_file("tmpZIDBob1_concurrentAccess.sqlite");
	void *res;

	resetGlobalParams();

	/* create tempory DB files, just try to clean them from dir before, just in case  */
	remove(aliceTesterFile);
	remove(bobTesterFile);
	/* open 2 connections on each file */
	bzrtptester_sqlite3_open(aliceTesterFile, &aliceDB);
	bzrtptester_sqlite3_open(aliceTesterFile, &aliceDB2);
	bzrtptester_sqlite3_open(bobTesterFile, &bobDB);
	bzrtptester_sqlite3_open(bobTesterFile, &bobDB2);

	/* init mutex */
	bctbx_mutex_init(&aliceMutex, NULL);
	bctbx_mutex_init(&bobMutex, NULL);

	/* set alice parameter to start a thread checking for bob status */
	aliceParams.db = aliceDB2;
	aliceParams.dbMutex = &aliceMutex;
	//aliceParams.dbMutex = NULL;
	aliceParams.peerUri = "bob@sip.linphone.org";
	aliceParams.expectedStatus = BZRTP_CACHE_PEER_STATUS_VALID;
	aliceParams.timeout = timeout;

	/* set alice parameter to start a thread checking for bob status */
	bobParams.db = bobDB2;
	bobParams.dbMutex = &bobMutex;
	//bobParams.dbMutex = NULL;
	bobParams.peerUri = "alice@sip.linphone.org";
	bobParams.expectedStatus = BZRTP_CACHE_PEER_STATUS_VALID;
	bobParams.timeout = timeout;

	/* make a first exchange alice <-> bob, validate the SAS */
	BC_ASSERT_EQUAL(multichannel_exchange_mutex(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, &aliceMutex, "alice@sip.linphone.org", bobDB, &bobMutex, "bob@sip.linphone.org"), 0, int, "%x");

	/* launch get_peerStatus thread */
	bctbx_thread_create(&bobThreadId, NULL, &test_cache_concurrent_access_getPeerStatus, &bobParams);
	bctbx_thread_create(&aliceThreadId, NULL, &test_cache_concurrent_access_getPeerStatus, &aliceParams);

	/* run more exchanges */
	while (getSimulatedTime()<timeout) {
		BC_ASSERT_EQUAL(multichannel_exchange_mutex(NULL, NULL, defaultCryptoAlgoSelection(), aliceDB, &aliceMutex, "alice@sip.linphone.org", bobDB, &bobMutex, "bob@sip.linphone.org"), 0, int, "%x");
	}

	bctbx_thread_join(aliceThreadId, &res);
	bctbx_thread_join(bobThreadId, &res);

	bctbx_mutex_destroy(&aliceMutex);
	bctbx_mutex_destroy(&bobMutex);

	sqlite3_close(aliceDB);
	sqlite3_close(bobDB);
	sqlite3_close(aliceDB2);
	sqlite3_close(bobDB2);

	/* clean temporary files */
	remove(aliceTesterFile);
	remove(bobTesterFile);
	bc_free(aliceTesterFile);
	bc_free(bobTesterFile);
#else /* ZIDCACHE_ENABLED */
	bzrtp_message("Test skipped as ZID cache is disabled\n");
#endif /* ZIDCACHE_ENABLED */
}


static test_t key_exchange_tests[] = {
	TEST_NO_TAG("Cacheless multi channel", test_cacheless_exchange),
	TEST_NO_TAG("Packet Fragmentation", test_mtu),
	TEST_NO_TAG("Packet Fragmentation over loosy network", test_loosy_network_mtu),
	TEST_NO_TAG("Cached Simple", test_cache_enabled_exchange),
	TEST_NO_TAG("Cached mismatch", test_cache_mismatch_exchange),
	TEST_NO_TAG("Loosy network", test_loosy_network),
	TEST_NO_TAG("Cached PVS", test_cache_sas_not_confirmed),
	TEST_NO_TAG("Auxiliary Secret", test_auxiliary_secret),
	TEST_NO_TAG("Abort and retry", test_abort_retry),
	TEST_NO_TAG("Active flag", test_active_flag),
	TEST_NO_TAG("Cache concurrent access", test_cache_concurrent_access),
};

test_suite_t key_exchange_test_suite = {
	"Key exchange",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(key_exchange_tests) / sizeof(key_exchange_tests[0]),
	key_exchange_tests
};
