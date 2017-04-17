/**
 @file bzrtpConfigsTests.c
 @brief Test complete ZRTP key agreement under differents configurations.

 @author Johan Pascal

 @copyright Copyright (C) 2017 Belledonne Communications, Grenoble, France

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
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "bzrtp/bzrtp.h"
#include <bctoolbox/tester.h>
#include "testUtils.h"

#define MAX_PACKET_LENGTH 1000
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
	uint8_t dontValidateSASflag; /**< if set, SAS will not be validated even if matching peer **/
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

static cryptoParams_t defaultCryptoAlgoSelection = {{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0};
static cryptoParams_t defaultCryptoAlgoSelectionNoSASValidation = {{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,1};

/* static global settings and their reset function */
static uint64_t msSTC = 0; /* Simulation Time Coordinated start at 0 and increment it at each sleep, is in milliseconds */
static int loosePacketPercentage=0; /* simulate bd network condition: loose packet */
static uint64_t timeOutLimit=1000; /* in ms, time span given to perform the ZRTP exchange */
static float fadingLostAlice=0.0; /* try not to throw away too many packet in a row */
static float fadingLostBob=0.0; /* try not to throw away too many packet in a row */
static int totalPacketLost=0; /* set a limit to the total number of packet we can loose to enforce completion of the exchange */

static void resetGlobalParams() {
	msSTC=0;
	totalPacketLost =0;
	loosePacketPercentage=0;
	timeOutLimit = 1000;
	fadingLostBob=0;
	fadingLostAlice=0;
}

/* time functions, we do not run a real time scenario, go for fast test instead */
static uint64_t getSimulatedTime() {
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
		/* make sure we cannot loose 10 packets in a row from the same sender */
		if ((totalPacketLost<10) && ((float)((rand()%100 )) < loosePacketPercentage-((clientContext->id == ALICE)?fadingLostAlice:fadingLostBob))) { /* randomly discard packets */
			//bzrtp_message("%d Loose %.8s from %s - LC %d\n", msSTC, packetString+16, (clientContext->id==ALICE?"Alice":"Bob"), totalPacketLost);

			if (clientContext->id == ALICE) {
				fadingLostAlice +=loosePacketPercentage/8;
			} else {
				fadingLostBob +=loosePacketPercentage/8;
			}
			return 0;
		}
		//bzrtp_message("%d Keep %.8s from %s - LC %d\n", msSTC, packetString+16, (clientContext->id==ALICE?"Alice":"Bob"), totalPacketLost);
	}

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
	BC_ASSERT_EQUAL(keyLength, 16, int, "%d"); /* any hash available in the config shall be able to produce a 16 bytes key */
	keyLength = 16;
	BC_ASSERT_EQUAL(bzrtp_exportKey(clientContext->bzrtpContext,  ((role==BZRTP_ROLE_INITIATOR)?"ResponderKey":"InitiatorKey"), 12, clientContext->recvExportedKey, &keyLength), 0, int, "%x");
	BC_ASSERT_EQUAL(keyLength, 16, int, "%d"); /* any hash available in the config shall be able to produce a 16 bytes key */

	return 0;
}

static int setUpClientContext(clientContext_t *clientContext, uint8_t clientID, uint32_t SSRC, void *zidCache, char *selfURI, char *peerURI, cryptoParams_t *cryptoParams) {
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
	retval = bzrtp_setZIDCache(clientContext->bzrtpContext, zidCache, selfURI, peerURI);
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

	/* start the ZRTP engine(it will send a hello packet )*/
	if ((retval = bzrtp_startChannelEngine(clientContext->bzrtpContext, SSRC))!=0) {
		bzrtp_message("ERROR: bzrtp_startChannelEngine returned %0x, client id is %d SSRC is %d\n", retval, clientID, SSRC);
		return -5;
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


uint32_t multichannel_exchange_pvs_params(cryptoParams_t *aliceCryptoParams, cryptoParams_t *bobCryptoParams, cryptoParams_t *expectedCryptoParams, void *aliceCache, char *aliceURI, void *bobCache, char *bobURI, uint8_t checkPVS, uint8_t expectedAlicePVS, uint8_t expectedBobPVS) {

	int retval,channelNumber;
	clientContext_t Alice,Bob;
	uint64_t initialTime=0;
	uint64_t lastPacketSentTime=0;
	uint32_t aliceSSRC = ALICE_SSRC_BASE;
	uint32_t bobSSRC = BOB_SSRC_BASE;
	uint32_t ret=0;

	/*** Create the main channel */
	if ((retval=setUpClientContext(&Alice, ALICE, aliceSSRC, aliceCache, aliceURI, bobURI, aliceCryptoParams))!=0) {
		bzrtp_message("ERROR: can't init setup client context id %d\n", ALICE);
		BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
		return retval;
	}

	if ((retval=setUpClientContext(&Bob, BOB, bobSSRC, bobCache, bobURI, aliceURI, bobCryptoParams))!=0) {
		bzrtp_message("ERROR: can't init setup client context id %d\n", BOB);
		BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
		return retval;
	}

	initialTime = getSimulatedTime();
	while ((bzrtp_getChannelStatus(Alice.bzrtpContext, aliceSSRC)!= BZRTP_CHANNEL_SECURE || bzrtp_getChannelStatus(Bob.bzrtpContext, bobSSRC)!= BZRTP_CHANNEL_SECURE) && (getSimulatedTime()-initialTime<timeOutLimit)){
		int i;
		/* check the message queue */
		for (i=0; i<aliceQueueIndex; i++) {
			retval = bzrtp_processMessage(Alice.bzrtpContext, aliceSSRC, aliceQueue[i].packetString, aliceQueue[i].packetLength);
			//bzrtp_message("%d Alice processed a %.8s and returns %x\n", msSTC, (aliceQueue[i].packetString)+16, retval);
			memset(aliceQueue[i].packetString, 0, MAX_PACKET_LENGTH); /* destroy the packet after sending it to the ZRTP engine */
			lastPacketSentTime=getSimulatedTime();
		}
		aliceQueueIndex = 0;

		for (i=0; i<bobQueueIndex; i++) {
			retval = bzrtp_processMessage(Bob.bzrtpContext, bobSSRC, bobQueue[i].packetString, bobQueue[i].packetLength);
			//bzrtp_message("%d Bob processed a %.8s and returns %x\n",msSTC, (bobQueue[i].packetString)+16, retval);
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

	bzrtp_message("ZRTP algo used during negotiation: Cipher: %s - KeyAgreement: %s - Hash: %s - AuthTag: %s - Sas Rendering: %s\n", bzrtp_cipher_toString(Alice.secrets->cipherAlgo), bzrtp_keyAgreement_toString(Alice.secrets->keyAgreementAlgo), bzrtp_hash_toString(Alice.secrets->hashAlgo), bzrtp_authtag_toString(Alice.secrets->authTagAlgo), bzrtp_sas_toString(Alice.secrets->sasAlgo));

	if ((retval=compareSecrets(Alice.secrets, Bob.secrets, TRUE))!=0) {
		BC_ASSERT_EQUAL(retval, 0, int, "%d");
		return retval;
	} else { /* SAS comparison is Ok, if we have a cache, confirm it */
		if (aliceCache != NULL && bobCache != NULL) {
			if (aliceCryptoParams==NULL || aliceCryptoParams->dontValidateSASflag == 0) {
				bzrtp_SASVerified(Alice.bzrtpContext);
			}
			if (bobCryptoParams==NULL || bobCryptoParams->dontValidateSASflag == 0) {
				bzrtp_SASVerified(Bob.bzrtpContext);
			}
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
				//bzrtp_message("%d Alice processed a %.8s and returns %x\n",msSTC, aliceQueue[i].packetString+16, retval);
				memset(aliceQueue[i].packetString, 0, MAX_PACKET_LENGTH); /* destroy the packet after sending it to the ZRTP engine */
				lastPacketSentTime=getSimulatedTime();
			}
			aliceQueueIndex = 0;

			for (i=0; i<bobQueueIndex; i++) {
				retval = bzrtp_processMessage(Bob.bzrtpContext, bobSSRC, bobQueue[i].packetString, bobQueue[i].packetLength);
				//bzrtp_message("%d Bob processed a %.8s and returns %x\n",msSTC, bobQueue[i].packetString+16, retval);
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
		bzrtp_message("Channel %d :ZRTP algo used during negotiation: Cipher: %s - KeyAgreement: %s - Hash: %s - AuthTag: %s - Sas Rendering: %s\n", channelNumber, bzrtp_cipher_toString(Alice.secrets->cipherAlgo), bzrtp_keyAgreement_toString(Alice.secrets->keyAgreementAlgo), bzrtp_hash_toString(Alice.secrets->hashAlgo), bzrtp_authtag_toString(Alice.secrets->authTagAlgo), bzrtp_sas_toString(Alice.secrets->sasAlgo));
		if ((retval=compareSecrets(Alice.secrets, Bob.secrets, FALSE))!=0) {
			BC_ASSERT_EQUAL(retval, 0, int, "%d");
		}
	}

	/*** Destroy Contexts ***/
	while (bzrtp_destroyBzrtpContext(Alice.bzrtpContext, aliceSSRC)>0 && aliceSSRC>=ALICE_SSRC_BASE) {
		aliceSSRC--;
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

uint32_t multichannel_exchange(cryptoParams_t *aliceCryptoParams, cryptoParams_t *bobCryptoParams, cryptoParams_t *expectedCryptoParams, void *aliceCache, char *aliceURI, void *bobCache, char *bobURI) {
	return multichannel_exchange_pvs_params(aliceCryptoParams, bobCryptoParams, expectedCryptoParams, aliceCache, aliceURI, bobCache, bobURI, FALSE, 0, 0);
}


void test_cacheless_exchange(void) {
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

		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1,0},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1,0},

		{{0},0,{0},0,{0},0,{0},0,{0},0,0}, /* this pattern will end the run because cipher nb is 0 */
		};

	pattern = &patterns[0]; /* pattern is a pointer to current pattern */

	while (pattern->cipherNb!=0) {
		BC_ASSERT_EQUAL(multichannel_exchange(pattern, pattern, pattern, NULL, NULL, NULL, NULL), 0, int, "%x");
		pattern++; /* point to next row in the array of patterns */
	}

	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, &defaultCryptoAlgoSelection, NULL, NULL, NULL, NULL), 0, int, "%x");
}

void test_loosy_network(void) {
	int i,j;
	resetGlobalParams();
	srand((unsigned int)time(NULL));

	/* run through all the configs 10 times to maximise chance to spot a random error based on a specific packet lost sequence */
	for (j=0; j<10; j++) {
		for (i=1; i<60; i+=1) {
			resetGlobalParams();
			timeOutLimit =100000; //outrageous time limit just to be sure to complete, not run in real time anyway
			loosePacketPercentage=i;
			BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, &defaultCryptoAlgoSelection, NULL, NULL, NULL, NULL), 0, int, "%x");
		}
	}
}

void test_cache_enabled_exchange(void) {
#ifdef ZIDCACHE_ENABLED
	sqlite3 *aliceDB=NULL;
	sqlite3 *bobDB=NULL;
	uint8_t selfZIDalice[12];
	uint8_t selfZIDbob[12];
	int zuidAlice=0,zuidBob=0;
	char *colNames[] = {"rs1", "rs2", "pvs"};
	uint8_t *colValuesAlice[3];
	size_t colLengthAlice[3];
	uint8_t *colValuesBob[3];
	size_t colLengthBob[3];
	int i;

	/* create tempory DB files, just try to clean them from dir before, just in case  */
	remove("tmpZIDAlice_simpleCache.sqlite");
	remove("tmpZIDBob_simpleCache.sqlite");
	bzrtptester_sqlite3_open(bc_tester_file("tmpZIDAlice_simpleCache.sqlite"), &aliceDB);
	bzrtptester_sqlite3_open(bc_tester_file("tmpZIDBob_simpleCache.sqlite"), &bobDB);

	/* make a first exchange */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, &defaultCryptoAlgoSelection, aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), 0, int, "%x");

	/* after the first exchange we shall have both pvs values at 1 and both rs1 identical and rs2 null, retrieve them from cache and check it */
	/* first get each ZIDs, note give NULL as RNG context may lead to segfault in case of error(caches were not created correctly)*/
	BC_ASSERT_EQUAL(bzrtp_getSelfZID((void *)aliceDB, "alice@sip.linphone.org", selfZIDalice, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_getSelfZID((void *)bobDB, "bob@sip.linphone.org", selfZIDbob, NULL), 0, int, "%x");
	/* then get the matching zuid in cache */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", selfZIDbob, &zuidAlice), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)bobDB, "bob@sip.linphone.org", "alice@sip.linphone.org", selfZIDalice, &zuidBob), 0, int, "%x");
	/* retrieve the values */
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3), 0, int, "%x");
	/* and compare to expected */
	/* rs1 is set and they are both the same */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	/* rs2 is unset(NULL) */
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[1], 0, int, "%d");
	BC_ASSERT_PTR_NULL(colValuesAlice[1]);
	BC_ASSERT_PTR_NULL(colValuesBob[1]);
	/* pvs is equal to 1 */
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* make a second exchange */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, &defaultCryptoAlgoSelection, aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), 0, int, "%x");
	/* read new values in cache, ZIDs and zuids must be identical, read alice first to be able to check rs2 with old rs1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3), 0, int, "%x");
	/* check what is now rs2 is the old rs1 */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthAlice[1], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[1], colValuesBob[0], 32), 0, int, "%d"); /* colValuesBob, still old values from before the second exchange */
	/* so read bob updated values and compare rs1, rs2 and check pvs is still at 1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3), 0, int, "%x");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[1], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, int, "%d");
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
	remove("tmpZIDAlice_simpleCache.sqlite");
	remove("tmpZIDBob_simpleCache.sqlite");

#endif /* ZIDCACHE_ENABLED */
}

/* first perform an exchange to establish a correct shared cache, then modify one of them and perform an other exchange to check we have a cache mismatch warning */
void test_cache_mismatch_exchange(void) {
#ifdef ZIDCACHE_ENABLED
	sqlite3 *aliceDB=NULL;
	sqlite3 *bobDB=NULL;
	uint8_t selfZIDalice[12];
	uint8_t selfZIDbob[12];
	int zuidAlice=0,zuidBob=0;
	char *colNames[] = {"rs1", "rs2", "pvs"};
	uint8_t *colValuesAlice[3];
	size_t colLengthAlice[3];
	uint8_t *colValuesBob[3];
	size_t colLengthBob[3];
	int i;

	/* create tempory DB files, just try to clean them from dir before, just in case  */
	remove("tmpZIDAlice_cacheMismtach.sqlite");
	remove("tmpZIDBob_cacheMismatch.sqlite");
	bzrtptester_sqlite3_open(bc_tester_file("tmpZIDAlice_cacheMismatch.sqlite"), &aliceDB);
	bzrtptester_sqlite3_open(bc_tester_file("tmpZIDBob_cacheMismatch.sqlite"), &bobDB);

	/* make a first exchange */
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, &defaultCryptoAlgoSelection, aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), 0, int, "%x");

	/* after the first exchange we shall have both pvs values at 1 and both rs1 identical and rs2 null, retrieve them from cache and check it */
	/* first get each ZIDs, note give NULL as RNG context may lead to segfault in case of error(caches were not created correctly)*/
	BC_ASSERT_EQUAL(bzrtp_getSelfZID((void *)aliceDB, "alice@sip.linphone.org", selfZIDalice, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_getSelfZID((void *)bobDB, "bob@sip.linphone.org", selfZIDbob, NULL), 0, int, "%x");
	/* then get the matching zuid in cache */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", selfZIDbob, &zuidAlice), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)bobDB, "bob@sip.linphone.org", "alice@sip.linphone.org", selfZIDalice, &zuidBob), 0, int, "%x");
	/* retrieve the values */
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3), 0, int, "%x");
	/* and compare to expected */
	/* rs1 is set and they are both the same */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	/* rs2 is unset(NULL) */
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[1], 0, int, "%d");
	BC_ASSERT_PTR_NULL(colValuesAlice[1]);
	BC_ASSERT_PTR_NULL(colValuesBob[1]);
	/* pvs is equal to 1 */
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* Modify Alice cache rs1 first byte value, it will cause a cache mismatch at next exchange */
	colValuesAlice[0][0] += 1;
	BC_ASSERT_EQUAL(bzrtp_cache_write((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 1), 0, int, "%x");

	/* make a second exchange : we have a cache mismatch(both on Bob and Alice side), wich means rs1 will not be backed up in rs2 which shall be NULL again */
	/* rs1 will be in sync has the SAS comparison will succeed and pvs will be set to 1*/
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, &defaultCryptoAlgoSelection, aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), RET_CACHE_MISMATCH<<16|RET_CACHE_MISMATCH, int, "%x");

	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3), 0, int, "%x");

	BC_ASSERT_EQUAL(colLengthAlice[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, int, "%d");
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[1], 0, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, int, "%d");
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
	BC_ASSERT_EQUAL(bzrtp_cache_write((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3), 0, int, "%x");

	/* make a third exchange : we have a cache mismatch(on Bob side only), wich means rs1 will not be backed up in rs2 which shall be NULL again */
	/* rs1 will be in sync has the SAS comparison will succeed and pvs will be set to 1*/
	BC_ASSERT_EQUAL(multichannel_exchange(NULL, NULL, &defaultCryptoAlgoSelection, aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org"), RET_CACHE_MISMATCH<<16, int, "%x");

	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3), 0, int, "%x");

	BC_ASSERT_EQUAL(colLengthAlice[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, int, "%d");
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[1], 0, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
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
	remove("tmpZIDAlice_cacheMismatch.sqlite");
	remove("tmpZIDBob_cacheMismatch.sqlite");

#endif /* ZIDCACHE_ENABLED */
}

void test_cache_sas_not_confirmed(void) {
#ifdef ZIDCACHE_ENABLED
	sqlite3 *aliceDB=NULL;
	sqlite3 *bobDB=NULL;
	uint8_t selfZIDalice[12];
	uint8_t selfZIDbob[12];
	int zuidAlice=0,zuidBob=0;
	char *colNames[] = {"rs1", "rs2", "pvs"};
	uint8_t *colValuesAlice[3];
	size_t colLengthAlice[3];
	uint8_t *colValuesBob[3];
	size_t colLengthBob[3];
	int i;

	/* create tempory DB files, just try to clean them from dir before, just in case  */
	remove("tmpZIDAlice_simpleCache.sqlite");
	remove("tmpZIDBob_simpleCache.sqlite");
	bzrtptester_sqlite3_open(bc_tester_file("tmpZIDAlice_simpleCache.sqlite"), &aliceDB);
	bzrtptester_sqlite3_open(bc_tester_file("tmpZIDBob_simpleCache.sqlite"), &bobDB);

	/* make a first exchange, Alice is instructed to not validate the SAS */
	BC_ASSERT_EQUAL(multichannel_exchange_pvs_params(&defaultCryptoAlgoSelectionNoSASValidation, &defaultCryptoAlgoSelection, &defaultCryptoAlgoSelection, aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org", TRUE, 0, 0), 0, int, "%x");

	/* after the first exchange we shall have alice pvs at 0 and bob at 1 and both rs1 identical and rs2 null, retrieve them from cache and check it */
	/* first get each ZIDs, note give NULL as RNG context may lead to segfault in case of error(caches were not created correctly)*/
	BC_ASSERT_EQUAL(bzrtp_getSelfZID((void *)aliceDB, "alice@sip.linphone.org", selfZIDalice, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_getSelfZID((void *)bobDB, "bob@sip.linphone.org", selfZIDbob, NULL), 0, int, "%x");
	/* then get the matching zuid in cache */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", selfZIDbob, &zuidAlice), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)bobDB, "bob@sip.linphone.org", "alice@sip.linphone.org", selfZIDalice, &zuidBob), 0, int, "%x");
	/* retrieve the values */
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3), 0, int, "%x");
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3), 0, int, "%x");
	/* and compare to expected */
	/* rs1 is set and they are both the same */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	/* rs2 is unset(NULL) */
	BC_ASSERT_EQUAL(colLengthAlice[1], 0, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[1], 0, int, "%d");
	BC_ASSERT_PTR_NULL(colValuesAlice[1]);
	BC_ASSERT_PTR_NULL(colValuesBob[1]);
	/* pvs is equal to 0 for Alice(actually NULL, so length is 0 and has no value which is considered 0 by the getPeerSecrets function) and 1 for Bob */
	BC_ASSERT_EQUAL(colLengthAlice[2], 0, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* make a second exchange, the PVS flag returned by both side shall be 0 as Alice did not validate hers on previous exchange */
	/* but let them both validate this one */
	BC_ASSERT_EQUAL(multichannel_exchange_pvs_params(NULL, NULL, &defaultCryptoAlgoSelection, aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org", TRUE, 0, 0), 0, int, "%x");
	/* read new values in cache, ZIDs and zuids must be identical, read alice first to be able to check rs2 with old rs1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3), 0, int, "%x");
	/* check what is now rs2 is the old rs1 */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthAlice[1], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[1], colValuesBob[0], 32), 0, int, "%d"); /* colValuesBob, still old values from before the second exchange */
	/* so read bob updated values and compare rs1, rs2 and check pvs is at 1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3), 0, int, "%x");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[1], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[0], colValuesBob[0], 32), 0, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[1], colValuesBob[1], 32), 0, int, "%d");
	BC_ASSERT_EQUAL(*colValuesAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(*colValuesBob[2], 1, int, "%d");

	/* make a third exchange, the PVS flag returned by both side shall be 1 */
	BC_ASSERT_EQUAL(multichannel_exchange_pvs_params(NULL, NULL, &defaultCryptoAlgoSelection, aliceDB, "alice@sip.linphone.org", bobDB, "bob@sip.linphone.org", TRUE, 1, 1), 0, int, "%x");
	/* read new values in cache, ZIDs and zuids must be identical, read alice first to be able to check rs2 with old rs1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)aliceDB, zuidAlice, "zrtp", colNames, colValuesAlice, colLengthAlice, 3), 0, int, "%x");
	/* check what is now rs2 is the old rs1 */
	BC_ASSERT_EQUAL(colLengthAlice[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthAlice[1], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthAlice[2], 1, int, "%d");
	BC_ASSERT_EQUAL(memcmp(colValuesAlice[1], colValuesBob[0], 32), 0, int, "%d"); /* colValuesBob, still old values from before the second exchange */
	/* so read bob updated values and compare rs1, rs2 and check pvs is still at 1 */
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)bobDB, zuidBob, "zrtp", colNames, colValuesBob, colLengthBob, 3), 0, int, "%x");
	BC_ASSERT_EQUAL(colLengthBob[0], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[1], 32, int, "%d");
	BC_ASSERT_EQUAL(colLengthBob[2], 1, int, "%d");
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
	remove("tmpZIDAlice_simpleCache.sqlite");
	remove("tmpZIDBob_simpleCache.sqlite");

#endif /* ZIDCACHE_ENABLED */
}
