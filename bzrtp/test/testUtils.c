/**
 @file testUtils.c

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "testUtils.h"
#include "cryptoUtils.h"
int verbose = 0;

void bzrtp_message(const char *fmt, ...) {
	if (verbose) {
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

void printHex(char *title, uint8_t *data, uint32_t length) {
	if (verbose) {
		int i;
		printf ("%s : ", title);
		for (i=0; i<length; i++) {
			printf ("0x%02x, ", data[i]);
		}
		printf ("\n");
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
						cryptoAlgoTypeIntToString(messageData->supportedHash[j], algoTypeString);
						printf("%.4s, ", algoTypeString);
					}
					printf ("\ncc ");
					for (j=0; j<messageData->cc; j++) {
						cryptoAlgoTypeIntToString(messageData->supportedCipher[j], algoTypeString);
						printf("%.4s, ", algoTypeString);
					}
					printf ("\nac ");
					for (j=0; j<messageData->ac; j++) {
						cryptoAlgoTypeIntToString(messageData->supportedAuthTag[j], algoTypeString);
						printf("%.4s, ", algoTypeString);
					}
					printf ("\nkc ");
					for (j=0; j<messageData->kc; j++) {
						cryptoAlgoTypeIntToString(messageData->supportedKeyAgreement[j], algoTypeString);
						printf("%.4s, ", algoTypeString);
					}
					printf ("\nsc ");
					for (j=0; j<messageData->sc; j++) {
						cryptoAlgoTypeIntToString(messageData->supportedSas[j], algoTypeString);
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
					cryptoAlgoTypeIntToString(messageData->hashAlgo, algoTypeString);
					printf("Hash Algo: %.4s\n", algoTypeString);
					cryptoAlgoTypeIntToString(messageData->cipherAlgo, algoTypeString);
					printf("Cipher Algo: %.4s\n", algoTypeString);
					cryptoAlgoTypeIntToString(messageData->authTagAlgo, algoTypeString);
					printf("Auth tag Algo: %.4s\n", algoTypeString);
					cryptoAlgoTypeIntToString(messageData->keyAgreementAlgo, algoTypeString);
					printf("Key agreement Algo: %.4s\n", algoTypeString);
					cryptoAlgoTypeIntToString(messageData->sasAlgo, algoTypeString);
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

				cryptoAlgoTypeIntToString(channelContext->hashAlgo, buffer);
				printf("    Selected algos\n     - Hash: %.4s\n", buffer);
				cryptoAlgoTypeIntToString(channelContext->cipherAlgo, buffer);
				printf("     - cipher: %.4s\n", buffer);
				cryptoAlgoTypeIntToString(channelContext->authTagAlgo, buffer);
				printf("     - auth tag: %.4s\n", buffer);
				cryptoAlgoTypeIntToString(channelContext->keyAgreementAlgo, buffer);
				printf("     - key agreement: %.4s\n", buffer);
				cryptoAlgoTypeIntToString(channelContext->sasAlgo, buffer);
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
	}
}
