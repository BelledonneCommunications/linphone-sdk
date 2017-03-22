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

static cryptoParams_t defaultCryptoAlgoSelection = {{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1};

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
int getSAS(void *clientData, bzrtpSrtpSecrets_t *secrets, int32_t part) {
	/* get the client context */
	clientContext_t *clientContext = (clientContext_t *)clientData;

	/* store the secret struct */
	clientContext->secrets = secrets;

	return 0;
}

static int setUpClientContext(clientContext_t *clientContext, uint8_t clientID, uint32_t SSRC, char *ZIDCache_filename, cryptoParams_t *cryptoParams) {
	int retval;
	bzrtpCallbacks_t cbs={0} ;

	/* set Id */
	clientContext->id = clientID;

	/* create zrtp context */
	clientContext->bzrtpContext = bzrtp_createBzrtpContext();
	if (clientContext->bzrtpContext==NULL) {
		bzrtp_message("ERROR: can't create bzrtp context, id client is %d", clientID);
		return -1;
	}

	/* check cache */
	if (ZIDCache_filename != NULL) {
		bzrtp_message("ERROR: asking for cache but not implemented yet\n");
		return -2;
	}

	/* assign callbacks */
	cbs.bzrtp_sendData=sendData;
	cbs.bzrtp_startSrtpSession=(int (*)(void *,const bzrtpSrtpSecrets_t *,int32_t) )getSAS;
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

void multichannel_exchange(cryptoParams_t *aliceCryptoParams, cryptoParams_t *bobCryptoParams, cryptoParams_t *expectedCryptoParams) {

	int retval,channelNumber;
	clientContext_t Alice,Bob;
	uint64_t initialTime=0;
	uint64_t lastPacketSentTime=0;
	uint32_t aliceSSRC = ALICE_SSRC_BASE;
	uint32_t bobSSRC = BOB_SSRC_BASE;

	/*** Create the main channel */
	if ((retval=setUpClientContext(&Alice, ALICE, aliceSSRC, NULL, aliceCryptoParams))!=0) {
		bzrtp_message("ERROR: can't init setup client context id %d\n", ALICE);
		BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
		return;
	}

	if ((retval=setUpClientContext(&Bob, BOB, bobSSRC, NULL, bobCryptoParams))!=0) {
		bzrtp_message("ERROR: can't init setup client context id %d\n", BOB);
		BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
		return;
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
		bzrtp_iterate(Alice.bzrtpContext, aliceSSRC, getSimulatedTime());
		bzrtp_iterate(Bob.bzrtpContext, bobSSRC, getSimulatedTime());

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
		return;
	}
	if ((retval=bzrtp_getChannelStatus(Bob.bzrtpContext, bobSSRC))!=BZRTP_CHANNEL_SECURE) {
		bzrtp_message("Fail Bob on channel1 loss rate is %d", loosePacketPercentage);
		BC_ASSERT_EQUAL(retval, BZRTP_CHANNEL_SECURE, int, "%0x");
		return;
	}

	bzrtp_message("ZRTP algo used during negotiation: Cipher: %s - KeyAgreement: %s - Hash: %s - AuthTag: %s - Sas Rendering: %s\n", bzrtp_cipher_toString(Alice.secrets->cipherAlgo), bzrtp_keyAgreement_toString(Alice.secrets->keyAgreementAlgo), bzrtp_hash_toString(Alice.secrets->hashAlgo), bzrtp_authtag_toString(Alice.secrets->authTagAlgo), bzrtp_sas_toString(Alice.secrets->sasAlgo));

	if ((retval=compareSecrets(Alice.secrets, Bob.secrets, TRUE))!=0) {
		BC_ASSERT_EQUAL(retval, 0, int, "%d");
	}

	if (expectedCryptoParams!=NULL) {
		BC_ASSERT_EQUAL(compareAlgoList(Alice.secrets,expectedCryptoParams), 0, int, "%d");
	}

	for (channelNumber=2; channelNumber<=MAX_NUM_CHANNEL; channelNumber++) {
		/* increase SSRCs as they are used to identify a channel */
		aliceSSRC++;
		bobSSRC++;

		/* start a new channel */
		if ((retval=addChannel(&Alice, aliceSSRC))!=0) {
			bzrtp_message("ERROR: can't add a second channel to client context id %d\n", ALICE);
			BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
			return;
		}

		if ((retval=addChannel(&Bob, bobSSRC))!=0) {
			bzrtp_message("ERROR: can't add a second channel to client context id %d\n", ALICE);
			BC_ASSERT_EQUAL(retval, 0, uint32_t, "0x%08x");
			return;
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
			bzrtp_iterate(Alice.bzrtpContext, aliceSSRC, getSimulatedTime());
			bzrtp_iterate(Bob.bzrtpContext, bobSSRC, getSimulatedTime());

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
			return;
		}
		if ((retval=bzrtp_getChannelStatus(Bob.bzrtpContext, bobSSRC))!=BZRTP_CHANNEL_SECURE) {
			bzrtp_message("Fail Bob on channel2 loss rate is %d", loosePacketPercentage);
			BC_ASSERT_EQUAL(retval, BZRTP_CHANNEL_SECURE, int, "%0x");
			return;
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

}

void test_cacheless_exchange(void) {
	cryptoParams_t *pattern;

	/* Reset Global Static settings */
	resetGlobalParams();

	/* Note: common algo selection is not tested here(this is done in some cryptoUtils tests)
	here we just perform an exchange with any final configuration avalaible and check it goes well */
	cryptoParams_t patterns[] = {
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1},

		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1},
		{{ZRTP_CIPHER_AES1},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH3k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1},

		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS32},1},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B32},1,{ZRTP_AUTHTAG_HS80},1},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS32},1},
		{{ZRTP_CIPHER_AES3},1,{ZRTP_HASH_S256},1,{ZRTP_KEYAGREEMENT_DH2k},1,{ZRTP_SAS_B256},1,{ZRTP_AUTHTAG_HS80},1},

		{{0},0,{0},0,{0},0,{0},0,{0},0}, /* this pattern will end the run because cipher nb is 0 */
		};

	pattern = &patterns[0]; /* pattern is a pointer to current pattern */

	while (pattern->cipherNb!=0) {
		multichannel_exchange(pattern, pattern, pattern);
		pattern++; /* point to next row in the array of patterns */
	}

	multichannel_exchange(NULL, NULL, &defaultCryptoAlgoSelection);
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
			multichannel_exchange(NULL,NULL,&defaultCryptoAlgoSelection);
		}
	}
}

