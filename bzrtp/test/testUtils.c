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
#include <string.h>

#include "testUtils.h"
#include "cryptoUtils.h"

#include "bzrtpTest.h"

#ifndef _WIN32
#if !defined(__QNXNTO__) && !(defined(__ANDROID__) && defined(__LP64__))
#include <ctype.h>
#include <string.h>
#endif
#else
#include <Windows.h>
#endif


#ifdef __APPLE__
#include "TargetConditionals.h"
#endif


int verbose = 0;

///* trace functions: bzrtp algo code to string */
//const char *bzrtp_hash_toString(uint8_t hashAlgo) {
//	switch(hashAlgo) {
//		case(ZRTP_UNSET_ALGO): return "unset";
//		case(ZRTP_HASH_S256): return "SHA-256";
//		case(ZRTP_HASH_S384): return "SHA-384";
//		case(ZRTP_HASH_N256): return "SHA3-256";
//		case(ZRTP_HASH_N384): return "SHA3-384";
//		default: return "Unknown Algo";
//	}
//}

//const char *bzrtp_keyAgreement_toString(uint8_t keyAgreementAlgo) {
//	switch(keyAgreementAlgo) {
//		case(ZRTP_UNSET_ALGO): return "unset";
//		case(ZRTP_KEYAGREEMENT_DH2k): return "DHM-2048";
//		case(ZRTP_KEYAGREEMENT_EC25): return "ECDH-256";
//		case(ZRTP_KEYAGREEMENT_DH3k): return "DHM-3072";
//		case(ZRTP_KEYAGREEMENT_EC38): return "ECDH-384";
//		case(ZRTP_KEYAGREEMENT_EC52): return "ECDH-521";
//		case(ZRTP_KEYAGREEMENT_X255): return "X25519";
//		case(ZRTP_KEYAGREEMENT_X448): return "X448";
//		case(ZRTP_KEYAGREEMENT_K255): return "K25519";
//		case(ZRTP_KEYAGREEMENT_K448): return "K448";
//		case(ZRTP_KEYAGREEMENT_Prsh): return "PreShared";
//		case(ZRTP_KEYAGREEMENT_Mult): return "MultiStream";
//		case(ZRTP_KEYAGREEMENT_KYB1): return "Kyber-512";
//		case(ZRTP_KEYAGREEMENT_KYB2): return "Kyber-768";
//		case(ZRTP_KEYAGREEMENT_KYB3): return "Kyber-1024";
//		case(ZRTP_KEYAGREEMENT_SIK1): return "Sike-434";
//		case(ZRTP_KEYAGREEMENT_SIK2): return "Sike-610";
//		case(ZRTP_KEYAGREEMENT_SIK3): return "Sike-751";
//		case(ZRTP_KEYAGREEMENT_K255_KYB512): return "K25519-Kyber512";
//		case(ZRTP_KEYAGREEMENT_K255_SIK434): return "K25519-Sike434";
//		case(ZRTP_KEYAGREEMENT_K448_KYB1024): return "K448-Kyber1024";
//		case(ZRTP_KEYAGREEMENT_K448_SIK751): return "K448-Sike751";
//		default: return "Unknown Algo";
//	}
//}

//const char *bzrtp_cipher_toString(uint8_t cipherAlgo) {
//	switch(cipherAlgo) {
//		case(ZRTP_UNSET_ALGO): return "unset";
//		case(ZRTP_CIPHER_AES1): return "AES-128";
//		case(ZRTP_CIPHER_AES2): return "AES-192";
//		case(ZRTP_CIPHER_AES3): return "AES-256";
//		case(ZRTP_CIPHER_2FS1): return "TwoFish-128";
//		case(ZRTP_CIPHER_2FS2): return "TwoFish-192";
//		case(ZRTP_CIPHER_2FS3): return "TwoFish-256";
//		default: return "Unknown Algo";
//	}
//}

//const char *bzrtp_authtag_toString(uint8_t authtagAlgo) {
//	switch(authtagAlgo) {
//		case(ZRTP_UNSET_ALGO): return "unset";
//		case(ZRTP_AUTHTAG_HS32): return "HMAC-SHA1-32";
//		case(ZRTP_AUTHTAG_HS80): return "HMAC-SHA1-80";
//		case(ZRTP_AUTHTAG_SK32): return "Skein-32";
//		case(ZRTP_AUTHTAG_SK64): return "Skein-64";
//		default: return "Unknown Algo";
//	}
//}

//const char *bzrtp_sas_toString(uint8_t sasAlgo) {
//	switch(sasAlgo) {
//		case(ZRTP_UNSET_ALGO): return "unset";
//		case(ZRTP_SAS_B32): return "Base32";
//		case(ZRTP_SAS_B256): return "PGP-WordList";
//		default: return "Unknown Algo";
//		}
//}

void printHex(char *title, uint8_t *data, uint32_t length) {
	if (verbose) {
		uint32_t i;
		printf ("%s : ", title);
		for (i=0; i<length; i++) {
			printf ("0x%02x, ", data[i]);
		}
		printf ("\n");
		fflush(NULL);
	}
}

void packetDump(bzrtpPacket_t *zrtpPacket, uint8_t addRawMessage) {
	if (verbose) {
		int j;
		printf ("SSRC %x - Message Length: %d - ", zrtpPacket->sourceIdentifier, zrtpPacket->messageLength);
		if (zrtpPacket->packetString!=NULL) { /* paquet has been built and we need to get his sequence number in the packetString */
			printf ("Sequence number: %02x%02x", *(zrtpPacket->packetString+2), *(zrtpPacket->packetString+3));
		} else { /* packet has been parsed, so get his sequence number in the structure */
			printf ("Sequence number: %04x", zrtpPacket->sequenceNumber);
		}
		switch (zrtpPacket->messageType) {
			case MSGTYPE_HELLO :
				{
					bzrtpHelloMessage_t *messageData;
					uint8_t algoTypeString[4];

					printf(" - Message Type : Hello\n");
					messageData = (bzrtpHelloMessage_t *)zrtpPacket->messageData;
					printf ("Version %.4s\nIdentifier %.16s\n", messageData->version, messageData->clientIdentifier);
					printHex ("H3", messageData->H3, 32);
					printHex ("ZID", messageData->ZID, 12);
					printf ("S : %d - M : %d - P : %d\nhc : %x - cc : %x - ac : %x - kc : %x - sc : %x\n", messageData->S, messageData->M, messageData->P, messageData->hc, messageData->cc, messageData->ac, messageData->kc, messageData->sc);
					printf ("hc ");
					for (j=0; j<messageData->hc; j++) {
						bzrtp_cryptoAlgoTypeIntToString(messageData->supportedHash[j], algoTypeString);
						printf("%.4s, ", algoTypeString);
					}
					printf ("\ncc ");
					for (j=0; j<messageData->cc; j++) {
						bzrtp_cryptoAlgoTypeIntToString(messageData->supportedCipher[j], algoTypeString);
						printf("%.4s, ", algoTypeString);
					}
					printf ("\nac ");
					for (j=0; j<messageData->ac; j++) {
						bzrtp_cryptoAlgoTypeIntToString(messageData->supportedAuthTag[j], algoTypeString);
						printf("%.4s, ", algoTypeString);
					}
					printf ("\nkc ");
					for (j=0; j<messageData->kc; j++) {
						bzrtp_cryptoAlgoTypeIntToString(messageData->supportedKeyAgreement[j], algoTypeString);
						printf("%.4s, ", algoTypeString);
					}
					printf ("\nsc ");
					for (j=0; j<messageData->sc; j++) {
						bzrtp_cryptoAlgoTypeIntToString(messageData->supportedSas[j], algoTypeString);
						printf("%.4s, ", algoTypeString);
					}
					printHex("\nMAC", messageData->MAC, 8);
				}

				break; /* MSGTYPE_HELLO */

			case MSGTYPE_HELLOACK :
				{
					printf(" - Message Type : Hello ACK\n");
				}
				break;

			case MSGTYPE_COMMIT:
				{
					uint8_t algoTypeString[4];
					bzrtpCommitMessage_t *messageData;

					printf(" - Message Type : Commit\n");
					messageData = (bzrtpCommitMessage_t *)zrtpPacket->messageData;
					printHex("H2", messageData->H2, 32);
					printHex("ZID", messageData->ZID, 12);
					bzrtp_cryptoAlgoTypeIntToString(messageData->hashAlgo, algoTypeString);
					printf("Hash Algo: %.4s\n", algoTypeString);
					bzrtp_cryptoAlgoTypeIntToString(messageData->cipherAlgo, algoTypeString);
					printf("Cipher Algo: %.4s\n", algoTypeString);
					bzrtp_cryptoAlgoTypeIntToString(messageData->authTagAlgo, algoTypeString);
					printf("Auth tag Algo: %.4s\n", algoTypeString);
					bzrtp_cryptoAlgoTypeIntToString(messageData->keyAgreementAlgo, algoTypeString);
					printf("Key agreement Algo: %.4s\n", algoTypeString);
					bzrtp_cryptoAlgoTypeIntToString(messageData->sasAlgo, algoTypeString);
					printf("Sas Algo: %.4s\n", algoTypeString);
					/* if it is a multistream or preshared commit, get the 16 bytes nonce */
					if ((messageData->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) || (messageData->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult)) {
						printHex("Nonce", messageData->nonce, 16);

						/* and the keyID for preshared commit only */
						if (messageData->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) {
							printHex("KeyId", messageData->keyID, 8);
						}
					} else { /* it's a DH commit message, get the hvi */
						printHex("hvi", messageData->hvi, 32);
					}

					printHex("\nMAC", messageData->MAC, 8);
				}
				break;
			case MSGTYPE_DHPART1:
			case MSGTYPE_DHPART2:
				{
					bzrtpDHPartMessage_t *messageData;

					if (zrtpPacket->messageType == MSGTYPE_DHPART1) {
						printf(" - Message Type : DHPart1\n");
					} else {
						printf(" - Message Type : DHPart2\n");
					}
					messageData = (bzrtpDHPartMessage_t *)zrtpPacket->messageData;
					printHex ("H1", messageData->H1, 32);
					printHex ("rs1ID", messageData->rs1ID, 8);
					printHex ("rs2ID", messageData->rs2ID, 8);
					printHex ("auxsecretID", messageData->auxsecretID, 8);
					printHex ("pbxsecretID", messageData->pbxsecretID, 8);
					printHex ("rs1ID", messageData->rs1ID, 8);
					printf("PV length is %d\n", (zrtpPacket->messageLength-84));
					printHex ("PV", messageData->pv, (zrtpPacket->messageLength-84)); /* length of fixed part of the message is 84, rest is the variable length PV */
					printHex("MAC", messageData->MAC, 8);
					
				}
				break;
			case MSGTYPE_CONFIRM1:
			case MSGTYPE_CONFIRM2:
				{
					bzrtpConfirmMessage_t *messageData;

					if (zrtpPacket->messageType == MSGTYPE_CONFIRM1) {
						printf(" - Message Type : Confirm1\n");
					} else {
						printf(" - Message Type : Confirm2\n");
					}
					messageData = (bzrtpConfirmMessage_t *)zrtpPacket->messageData;
					printHex("H0", messageData->H0, 32);
					printf("sig_len %d\n", messageData->sig_len);
					printf("E %d V %d A %d D %d\n", messageData->E,  messageData->V, messageData->A, messageData->D);
					printf("Cache expiration Interval %08x\n", messageData->cacheExpirationInterval);
				}
				break;
			case MSGTYPE_CONF2ACK:
				{
					printf(" - Message Type: Conf2ACK\n");
				}
		}

		if (addRawMessage) {
			printHex("Data", zrtpPacket->packetString, zrtpPacket->messageLength+16);
		}
		fflush(NULL);
	}
}

void dumpContext(char *title, bzrtpContext_t *zrtpContext) {
	if (verbose) {
		uint8_t buffer[4];
		int i,j;
		printf("%s context is :\n", title);
		printHex("selfZID", zrtpContext->selfZID, 12);
		printHex("peerZID", zrtpContext->peerZID, 12);

		for (i=0; i<ZRTP_MAX_CHANNEL_NUMBER; i++) {
			if (zrtpContext->channelContext[i] != NULL) {
				bzrtpChannelContext_t *channelContext = zrtpContext->channelContext[i];
				printf("Channel %i\n  self: %08x\n", i, channelContext->selfSSRC);
				printf ("    selfH: ");
				for (j=0; j<4; j++) {
					printHex("      ", channelContext->selfH[j], 32);
				}
				printf ("    peerH: ");
				for (j=0; j<4; j++) {
					printHex("      ", channelContext->peerH[j], 32);
				}		

				bzrtp_cryptoAlgoTypeIntToString(channelContext->hashAlgo, buffer);
				printf("    Selected algos\n     - Hash: %.4s\n", buffer);
				bzrtp_cryptoAlgoTypeIntToString(channelContext->cipherAlgo, buffer);
				printf("     - cipher: %.4s\n", buffer);
				bzrtp_cryptoAlgoTypeIntToString(channelContext->authTagAlgo, buffer);
				printf("     - auth tag: %.4s\n", buffer);
				bzrtp_cryptoAlgoTypeIntToString(channelContext->keyAgreementAlgo, buffer);
				printf("     - key agreement: %.4s\n", buffer);
				bzrtp_cryptoAlgoTypeIntToString(channelContext->sasAlgo, buffer);
				printf("     - sas: %.4s\n", buffer);
				printHex("    initiator auxID", channelContext->initiatorAuxsecretID, 8);
				printHex("    responder auxID", channelContext->responderAuxsecretID, 8);
				if (channelContext->s0 != NULL) {
					printHex("    s0", channelContext->s0, channelContext->hashLength);
				}
				if(channelContext->srtpSecrets.sas != NULL) {
					printf("    sas : %.4s\n", channelContext->srtpSecrets.sas);
				}
				if (channelContext->srtpSecrets.selfSrtpKey != NULL) {
					printHex("    selfsrtp key", channelContext->srtpSecrets.selfSrtpKey, channelContext->srtpSecrets.selfSrtpKeyLength);
					printHex("    selfsrtp salt", channelContext->srtpSecrets.selfSrtpSalt, channelContext->srtpSecrets.selfSrtpSaltLength);
					printHex("    peersrtp key", channelContext->srtpSecrets.peerSrtpKey, channelContext->srtpSecrets.peerSrtpKeyLength);
					printHex("    peersrtp salt", channelContext->srtpSecrets.peerSrtpSalt, channelContext->srtpSecrets.peerSrtpSaltLength);
				}
				if (channelContext->mackeyi!=NULL) {
					printHex("    mackeyi", channelContext->mackeyi, channelContext->hashLength);
				}
				if (channelContext->mackeyr!=NULL) {
					printHex("    mackeyr", channelContext->mackeyr, channelContext->hashLength);
				}
				if (channelContext->zrtpkeyi!=NULL) {
					printHex("    zrtpkeyi", channelContext->zrtpkeyi, channelContext->cipherKeyLength);
				}
				if (channelContext->zrtpkeyr!=NULL) {
					printHex("    zrtpkeyr", channelContext->zrtpkeyr, channelContext->cipherKeyLength);
				}
			}
		}

		printf("Initiator Shared Secrets :\n");
		printHex("rs1ID", zrtpContext->initiatorCachedSecretHash.rs1ID, 8);
		printHex("rs2ID", zrtpContext->initiatorCachedSecretHash.rs2ID, 8);
		printHex("pbxID", zrtpContext->initiatorCachedSecretHash.pbxsecretID, 8);

		printf("Responder Shared Secrets :\n");
		printHex("rs1ID", zrtpContext->responderCachedSecretHash.rs1ID, 8);
		printHex("rs2ID", zrtpContext->responderCachedSecretHash.rs2ID, 8);
		printHex("pbxID", zrtpContext->responderCachedSecretHash.pbxsecretID, 8);
		fflush(NULL);
	}
}


#ifdef ZIDCACHE_ENABLED

#define MAX_PATH_SIZE 1024

int bzrtptester_sqlite3_open(const char *db_file, sqlite3 **db) {
	char* errmsg = NULL;
	int ret;
	int flags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;

#if TARGET_OS_IPHONE
	/* the secured filesystem of the iPHone doesn't allow writing while the app is in background mode, which is problematic.
	 * We workaround by asking that the open is made with no protection*/
	flags |= SQLITE_OPEN_FILEPROTECTION_NONE;
#endif

	ret = sqlite3_open_v2(db_file, db, flags, NULL);

	if (ret != SQLITE_OK) return ret;
	// Some platforms do not provide a way to create temporary files which are needed
	// for transactions... so we work in memory only
	// see http ://www.sqlite.org/compile.html#temp_store
	ret = sqlite3_exec(*db, "PRAGMA temp_store=MEMORY", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		sqlite3_free(errmsg);
	}

	return ret;
}
#endif /* ZIDCACHE_ENABLED */
