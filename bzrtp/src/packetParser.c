/**
 @file packetParser.c
 
 @brief functions to parse and generate a ZRTP packet 
 
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

#include <stdlib.h>
#include <string.h>
#include "typedef.h"
#include "packetParser.h"
#include "cryptoWrapper.h"
#include "cryptoUtils.h"

/* DEBUG */
#include <stdio.h>

/* minimum length of a ZRTP packet: 12 bytes header + 12 bytes message(shortest are ACK messages) + 4 bytes CRC */
#define ZRTP_MIN_PACKET_LENGTH 28

/* maximum length of a ZRTP packet: 3072 bytes get it from GNU-ZRTP CPP code */
#define ZRTP_MAX_PACKET_LENGTH 3072

/* header of ZRTP message is 12 bytes : Preambule/Message Length + Message Type(2 words) */
#define ZRTP_MESSAGE_HEADER_LENGTH 12

/* length of the non optional and fixed part of all messages, in bytes */
#define ZRTP_HELLOMESSAGE_FIXED_LENGTH			88 
#define ZRTP_HELLOACKMESSAGE_FIXED_LENGTH		12
#define ZRTP_COMMITMESSAGE_FIXED_LENGTH 		84
#define ZRTP_DHPARTMESSAGE_FIXED_LENGTH 		84
#define ZRTP_CONFIRMMESSAGE_FIXED_LENGTH		76 
#define ZRTP_CONF2ACKMESSAGE_FIXED_LENGTH 		12
#define ZRTP_ERRORMESSAGE_FIXED_LENGTH 			16
#define ZRTP_ERRORACKMESSAGE_FIXED_LENGTH 		12
#define ZRTP_GOCLEARMESSAGE_FIXED_LENGTH		20 
#define ZRTP_CLEARACKMESSAGE_FIXED_LENGTH 		12
#define ZRTP_SASRELAYMESSAGE_FIXED_LENGTH 		76
#define ZRTP_RELAYACKMESSAGE_FIXED_LENGTH 		12
#define ZRTP_PINGMESSAGE_FIXED_LENGTH 			24
#define ZRTP_PINGACKMESSAGE_FIXED_LENGTH 		36

/*** local functions prototypes ***/

/**
 * Return the variable private value length in bytes according to given key agreement algorythm
 *
 * @param[in]	keyAgreementAlgo	The key agreement algo mapped to an integer as defined in cryptoWrapper.h
 *
 * @return		the private value length in bytes
 *
 */
uint16_t computeKeyAgreementPrivateValueLength(uint8_t keyAgreementAlgo);

/**
 * @brief Retrieve the 8 char string value message type from the int32_t code
 *
 * @param[in] messageType		The messageType code
 *
 * @return	an 9 char string : 8 chars message type as specified in rfc section 5.1.1 + string terminating char
 */
uint8_t *messageTypeInttoString(uint32_t messageType);

/**
 * @brief Map the 8 char string value message type to an int32_t
 *
 * @param[in] messageTypeString		an 8 bytes string matching a zrtp message type
 *
 * @return	a 32-bits unsigned integer mapping the message type
 */
int32_t messageTypeStringtoInt(uint8_t messageTypeString[8]);

/**
 * @brief Write the message header(preambule, length, message type) into the given output buffer
 *
 * @param[out]	outputBuffer		Message starts at the begining of this buffer
 * @param[in]	messageLength		Message length in bytes! To be converted into 32bits words before being inserted in the message header
 * @param[in]	messageType			An 8 chars string for the message type (validity is not checked by this function)
 *
 */
void zrtpMessageSetHeader(uint8_t *outputBuffer, uint16_t messageLength, uint8_t messageType[8]);




/*** Public functions implementation ***/

/* First call this function to check packet validity and create the packet structure */
bzrtpPacket_t *bzrtp_packetCheck(const uint8_t * input, uint16_t inputLength, uint16_t lastValidSequenceNumber, int *exitCode) {
		bzrtpPacket_t *zrtpPacket;

	/* first check that the packet is a ZRTP one */
	/* is the length compatible with a ZRTP packet */ 
	if ((inputLength<ZRTP_MIN_PACKET_LENGTH) || (inputLength>ZRTP_MAX_PACKET_LENGTH)) {
		 *exitCode = BZRTP_PARSER_ERROR_INVALIDPACKET;
		 return NULL;
	}

	/* check ZRTP packet format from rfc section 5 
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |0 0 0 1|Not Used (set to zero) |         Sequence Number       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                 Magic Cookie 'ZRTP' (0x5a525450)              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Source Identifier                      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |           ZRTP Message (length depends on Message Type)       |
   |                            . . .                              |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          CRC (1 word)                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
	if ((input[0]>>4 != 0x01) || (input[4]!= (uint8_t)((ZRTP_MAGIC_COOKIE>>24)&0xFF)) || (input[5]!= (uint8_t)((ZRTP_MAGIC_COOKIE>>16)&0xFF)) || (input[6]!= (uint8_t)((ZRTP_MAGIC_COOKIE>>8)&0xFF)) || (input[7]!= (uint8_t)(ZRTP_MAGIC_COOKIE&0xFF))) {
		*exitCode = BZRTP_PARSER_ERROR_INVALIDPACKET;
		return NULL;
	}

	/* Check the sequence number : it must be > to the last valid one (given in parameter) to discard out of order packets
	 * TODO: what if we got a Sequence Number overflowing the 16 bits ? */
	uint16_t sequenceNumber = (((uint16_t)input[2])<<8) | ((uint16_t)input[3]);
	if (sequenceNumber <= lastValidSequenceNumber) {
		*exitCode = BZRTP_PARSER_ERROR_OUTOFORDER;
		return NULL;
	}

	/* Check the CRC : The CRC is calculated across the entire ZRTP packet, including the ZRTP header and the ZRTP message, but not including the CRC field.*/
	uint32_t packetCRC = ((((uint32_t)input[inputLength-4])<<24)&0xFF000000) | ((((uint32_t)input[inputLength-3])<<16)&0x00FF0000) | ((((uint32_t)input[inputLength-2])<<8)&0x0000FF00) | (((uint32_t)input[inputLength-1])&0x000000FF);
	if (bzrtp_CRC32((uint8_t *)input, inputLength - 4) != packetCRC) {
		*exitCode = BZRTP_PARSER_ERROR_INVALIDCRC;
		return NULL;
	}

	/* check message header : 
	 *  0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |0 1 0 1 0 0 0 0 0 1 0 1 1 0 1 0|             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            Message Type Block            (2 words)            |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
	if ((input[ZRTP_PACKET_HEADER_LENGTH]!=0x50) || (input[ZRTP_PACKET_HEADER_LENGTH+1]!=0x5a)) {
		*exitCode = BZRTP_PARSER_ERROR_INVALIDMESSAGE;
		return NULL;
	}

	/* get the length from the message: it is expressed in 32bits words, convert it to bytes (4*) */
	uint16_t messageLength = 4*(((((uint16_t)input[ZRTP_PACKET_HEADER_LENGTH+2])<<8)&0xFF00) | (((uint16_t)input[ZRTP_PACKET_HEADER_LENGTH+3])&0x00FF));

	/* get the message Type */
	uint32_t messageType = messageTypeStringtoInt((uint8_t *)(input+ZRTP_PACKET_HEADER_LENGTH+4));

	if (messageType == MSGTYPE_INVALID) {
		*exitCode = BZRTP_PARSER_ERROR_INVALIDMESSAGE;
		return NULL;
	}

	/* packet and message seems to be valid, so allocate a structure and parse it */
	zrtpPacket = (bzrtpPacket_t *)malloc(sizeof(bzrtpPacket_t));
	memset(zrtpPacket, 0, sizeof(bzrtpPacket_t));
	zrtpPacket->sequenceNumber = sequenceNumber;
	zrtpPacket->messageLength = messageLength;
	zrtpPacket->messageType = messageType;
	zrtpPacket->messageData = NULL;
	zrtpPacket->packetString = NULL;

	/* get the SSRC */
	zrtpPacket->sourceIdentifier = ((((uint32_t)input[8])<<24)&0xFF000000) | ((((uint32_t)input[9])<<16)&0x00FF0000) | ((((uint32_t)input[10])<<8)&0x0000FF00) | (((uint32_t)input[11])&0x000000FF);

	*exitCode = 0;
	return zrtpPacket;
}




/* Call this function after the packetCheck one, to actually parse the packet : create and fill the messageData structure */
int bzrtp_packetParser(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, const uint8_t * input, uint16_t inputLength, bzrtpPacket_t *zrtpPacket) {

	int i;

	/* now allocate and fill the correct message structure according to the message type */
	/* messageContent points to the begining of the ZRTP message */
	uint8_t *messageContent = (uint8_t *)(input+ZRTP_PACKET_HEADER_LENGTH+ZRTP_MESSAGE_HEADER_LENGTH);

	switch (zrtpPacket->messageType) {
		case MSGTYPE_HELLO : 
			{
				/* allocate a Hello message structure */
				bzrtpHelloMessage_t *messageData;
				messageData = (bzrtpHelloMessage_t *)malloc(sizeof(bzrtpHelloMessage_t));

				/* fill it */
				memcpy(messageData->version, messageContent, 4);
				messageContent +=4;
				memcpy(messageData->clientIdentifier, messageContent, 16);
				messageContent +=16;
				memcpy(messageData->H3, messageContent, 32);
				messageContent +=32;
				memcpy(messageData->ZID, messageContent, 12);
				messageContent +=12;
				messageData->S = ((*messageContent)>>6)&0x01;
				messageData->M = ((*messageContent)>>5)&0x01;
				messageData->P = ((*messageContent)>>4)&0x01;
				messageContent +=1;
				messageData->hc = (*messageContent)&0x0F;
				messageContent +=1;
				messageData->cc = ((*messageContent)>>4)&0x0F;
				messageData->ac = (*messageContent)&0x0F;
				messageContent +=1;
				messageData->kc = ((*messageContent)>>4)&0x0F;
				messageData->sc = (*messageContent)&0x0F;
				messageContent +=1;

				/* Check message length according to value in hc, cc, ac, kc and sc */
				if (zrtpPacket->messageLength != ZRTP_HELLOMESSAGE_FIXED_LENGTH + 4*((uint16_t)(messageData->hc)+(uint16_t)(messageData->cc)+(uint16_t)(messageData->ac)+(uint16_t)(messageData->kc)+(uint16_t)(messageData->sc))) {
					free(messageData);
					return BZRTP_PARSER_ERROR_INVALIDMESSAGE;
				}

				/* parse the variable lenght part: algorithms types */
				for (i=0; i<messageData->hc; i++) {
					messageData->supportedHash[i] = cryptoAlgoTypeStringToInt(messageContent, ZRTP_HASH_TYPE);
					messageContent +=4;
				}
				for (i=0; i<messageData->cc; i++) {
					messageData->supportedCipher[i] = cryptoAlgoTypeStringToInt(messageContent, ZRTP_CIPHERBLOCK_TYPE);
					messageContent +=4;
				}
				for (i=0; i<messageData->ac; i++) {
					messageData->supportedAuthTag[i] = cryptoAlgoTypeStringToInt(messageContent, ZRTP_AUTHTAG_TYPE);
					messageContent +=4;
				}
				for (i=0; i<messageData->kc; i++) {
					messageData->supportedKeyAgreement[i] = cryptoAlgoTypeStringToInt(messageContent, ZRTP_KEYAGREEMENT_TYPE);
					messageContent +=4;
				}
				for (i=0; i<messageData->sc; i++) {
					messageData->supportedSas[i] = cryptoAlgoTypeStringToInt(messageContent, ZRTP_SAS_TYPE);
					messageContent +=4;
				}

				/* supported algos may be empty which means that only mandatory algo are supported.
				 * In this case we must insert them in the message Data structure */
				if (messageData->hc == 0) {
					messageData->hc = 1;
					messageData->supportedHash[0] = ZRTP_HASH_S256;
				}

				if (messageData->cc == 0) {
					messageData->cc = 1;
					messageData->supportedCipher[0] = ZRTP_CIPHER_AES1;
				}

				if (messageData->ac == 0) {
					messageData->ac = 2;
					messageData->supportedAuthTag[0] = ZRTP_AUTHTAG_HS32;
					messageData->supportedAuthTag[1] = ZRTP_AUTHTAG_HS80;
				}

				if (messageData->kc == 0) {
					messageData->kc = 1;
					messageData->supportedKeyAgreement[0] = ZRTP_KEYAGREEMENT_DH3k;
				}

				if (messageData->sc == 0) {
					messageData->sc = 1;
					messageData->supportedSas[0] = ZRTP_SAS_B32;
				}

				memcpy(messageData->MAC, messageContent, 8);
				
				/* attach the message structure to the packet one */
				zrtpPacket->messageData = (void *)messageData;

				/* the parsed Hello packet must be saved as it may be used to generate commit message or the total_hash */
				zrtpPacket->packetString = (uint8_t *)malloc(inputLength*sizeof(uint8_t));
				memcpy(zrtpPacket->packetString, input, inputLength); /* store the whole packet even if we may use the message only */
			}
			break; /* MSGTYPE_HELLO */

		case MSGTYPE_HELLOACK :
			{
				/* check message length */
				if (zrtpPacket->messageLength != ZRTP_HELLOACKMESSAGE_FIXED_LENGTH) {
					return BZRTP_PARSER_ERROR_INVALIDMESSAGE;
				}


			}
			break; /* MSGTYPE_HELLOACK */

		case MSGTYPE_COMMIT:
			{
				/* allocate a commit message structure */
				bzrtpCommitMessage_t *messageData;
				messageData = (bzrtpCommitMessage_t *)malloc(sizeof(bzrtpCommitMessage_t));

				/* fill the structure */
				memcpy(messageData->H2, messageContent, 32);
				messageContent +=32;

				/* We have now H2, check it matches the H3 we had in the hello message H3=SHA256(H2) and that the Hello message MAC is correct */
				uint8_t checkH3[32];
				uint8_t checkMAC[32];
				if (zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID] == NULL) {
					free (messageData);
					/* we have no Hello message in this channel, this commit shall never have arrived, discard it as invalid */
					return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
				}
				bzrtpHelloMessage_t *peerHelloMessageData = (bzrtpHelloMessage_t *)zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageData;
				/* Check H3 = SHA256(H2) */
				bzrtpCrypto_sha256(messageData->H2, 32, 32, checkH3);
				if (memcmp(checkH3, peerHelloMessageData->H3, 32) != 0) {
					free (messageData);
					return BZRTP_PARSER_ERROR_UNMATCHINGHASHCHAIN;
				}
				/* Check the hello MAC message. 
				 * MAC is 8 bytes long and is computed on the message(skip the ZRTP_PACKET_HEADER) and exclude the mac itself (-8 bytes from message Length) */
				bzrtpCrypto_hmacSha256(messageData->H2, 32, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength-8, 8, checkMAC);
				if (memcmp(checkMAC, peerHelloMessageData->MAC, 8) != 0) {
					free (messageData);
					return BZRTP_PARSER_ERROR_UNMATCHINGMAC;
				}

				memcpy(messageData->ZID, messageContent, 12);
				messageContent +=12;
				messageData->hashAlgo = cryptoAlgoTypeStringToInt(messageContent, ZRTP_HASH_TYPE);
				messageContent += 4;
				messageData->cipherAlgo = cryptoAlgoTypeStringToInt(messageContent, ZRTP_CIPHERBLOCK_TYPE);
				messageContent += 4;
				messageData->authTagAlgo = cryptoAlgoTypeStringToInt(messageContent, ZRTP_AUTHTAG_TYPE);
				messageContent += 4;
				messageData->keyAgreementAlgo = cryptoAlgoTypeStringToInt(messageContent, ZRTP_KEYAGREEMENT_TYPE);
				messageContent += 4;
				/* commit message length depends on the key agreement type choosen (and set in the zrtpContext->keyAgreementAlgo) */
				uint16_t variableLength = 0;
				switch(messageData->keyAgreementAlgo) {
					case ZRTP_KEYAGREEMENT_DH2k :
					case ZRTP_KEYAGREEMENT_EC25 :
					case ZRTP_KEYAGREEMENT_DH3k :
					case ZRTP_KEYAGREEMENT_EC38 :
					case ZRTP_KEYAGREEMENT_EC52 :
						variableLength = 32; /* hvi is 32 bytes length in DH Commit message format */
						break;
					case ZRTP_KEYAGREEMENT_Prsh :
						variableLength = 24; /* nonce (16 bytes) and keyID(8 bytes) are 24 bytes length in preshared Commit message format */
						break;
					case ZRTP_KEYAGREEMENT_Mult :
						variableLength = 16; /* nonce is 24 bytes length in multistream Commit message format */
						break;
					default:
						free(messageData);
						return BZRTP_PARSER_ERROR_INVALIDMESSAGE;
				}

				if (zrtpPacket->messageLength != ZRTP_COMMITMESSAGE_FIXED_LENGTH + variableLength) {
					free(messageData);
					return BZRTP_PARSER_ERROR_INVALIDMESSAGE;
				}
				messageData->sasAlgo = cryptoAlgoTypeStringToInt(messageContent, ZRTP_SAS_TYPE);
				messageContent += 4;

				/* if it is a multistream or preshared commit, get the 16 bytes nonce */
				if ((messageData->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) || (messageData->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult)) {
					memcpy(messageData->nonce, messageContent, 16);
					messageContent +=16;

					/* and the keyID for preshared commit only */
					if (messageData->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) {
						memcpy(messageData->keyID, messageContent, 8);
						messageContent +=8;
					}
				} else { /* it's a DH commit message, get the hvi */
					memcpy(messageData->hvi, messageContent, 32);
					messageContent +=32;
				}

				/* get the MAC and attach the message data to the packet structure */
				memcpy(messageData->MAC, messageContent, 8);
				zrtpPacket->messageData = (void *)messageData;

				/* the parsed commit packet must be saved as it is used to generate the total_hash */
				zrtpPacket->packetString = (uint8_t *)malloc(inputLength*sizeof(uint8_t));
				memcpy(zrtpPacket->packetString, input, inputLength); /* store the whole packet even if we may use the message only */
			}
			break; /* MSGTYPE_COMMIT */
		case MSGTYPE_DHPART1 :
		case MSGTYPE_DHPART2 :
			{
				/*check message length, depends on the selected key agreement algo set in zrtpContext */
				uint16_t pvLength = computeKeyAgreementPrivateValueLength(zrtpChannelContext->keyAgreementAlgo);
				if (pvLength == 0) {
					return BZRTP_PARSER_ERROR_INVALIDCONTEXT;
				}

				if (zrtpPacket->messageLength != ZRTP_DHPARTMESSAGE_FIXED_LENGTH+pvLength) {
					return BZRTP_PARSER_ERROR_INVALIDMESSAGE;
				}

				/* allocate a DHPart message structure and pv */
				bzrtpDHPartMessage_t *messageData;
				messageData = (bzrtpDHPartMessage_t *)malloc(sizeof(bzrtpDHPartMessage_t));
				messageData->pv = (uint8_t *)malloc(pvLength*sizeof(uint8_t));

				/* fill the structure */
				memcpy(messageData->H1, messageContent, 32);
				messageContent +=32;

				/* We have now H1, check it matches the H2 we had in the commit message H2=SHA256(H1) and that the Commit message MAC is correct */
				if ( zrtpChannelContext->role == RESPONDER) { /* do it only if we are responder (we received a commit packet) */
					uint8_t checkH2[32];
					uint8_t checkMAC[32];
					if (zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID] == NULL) {
						free (messageData);
						/* we have no Commit message in this channel, this DHPart2 shall never have arrived, discard it as invalid */
						return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
					}
					bzrtpCommitMessage_t *peerCommitMessageData = (bzrtpCommitMessage_t *)zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageData;
					/* Check H2 = SHA256(H1) */
					bzrtpCrypto_sha256(messageData->H1, 32, 32, checkH2);
					if (memcmp(checkH2, peerCommitMessageData->H2, 32) != 0) {
						free (messageData);
						return BZRTP_PARSER_ERROR_UNMATCHINGHASHCHAIN;
					}
					/* Check the Commit MAC message. 
					 * MAC is 8 bytes long and is computed on the message(skip the ZRTP_PACKET_HEADER) and exclude the mac itself (-8 bytes from message Length) */
					bzrtpCrypto_hmacSha256(messageData->H1, 32, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength-8, 8, checkMAC);
					if (memcmp(checkMAC, peerCommitMessageData->MAC, 8) != 0) {
						free (messageData);
						return BZRTP_PARSER_ERROR_UNMATCHINGMAC;
					}
				} else { /* if we are initiator(we didn't received any commit message and then no H2), we must check that H3=SHA256(SHA256(H1)) and the Hello message MAC */
					uint8_t checkH2[32];
					uint8_t checkH3[32];
					uint8_t checkMAC[32];
					if (zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID] == NULL) {
						free (messageData);
						/* we have no Hello message in this channel, this DHPart1 shall never have arrived, discard it as invalid */
						return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
					}
					bzrtpHelloMessage_t *peerHelloMessageData = (bzrtpHelloMessage_t *)zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageData;
					/* Check H3 = SHA256(SHA256(H1)) */
					bzrtpCrypto_sha256(messageData->H1, 32, 32, checkH2);
					bzrtpCrypto_sha256(checkH2, 32, 32, checkH3);
					if (memcmp(checkH3, peerHelloMessageData->H3, 32) != 0) {
						free (messageData);
						return BZRTP_PARSER_ERROR_UNMATCHINGHASHCHAIN;
					}
					/* Check the hello MAC message. 
					 * MAC is 8 bytes long and is computed on the message(skip the ZRTP_PACKET_HEADER) and exclude the mac itself (-8 bytes from message Length) */
					bzrtpCrypto_hmacSha256(checkH2, 32, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength-8, 8, checkMAC);
					if (memcmp(checkMAC, peerHelloMessageData->MAC, 8) != 0) {
						free (messageData);
						return BZRTP_PARSER_ERROR_UNMATCHINGMAC;
					}

				}

				memcpy(messageData->rs1ID, messageContent, 8);
				messageContent +=8;
				memcpy(messageData->rs2ID, messageContent, 8);
				messageContent +=8;
				memcpy(messageData->auxsecretID, messageContent, 8);
				messageContent +=8;
				memcpy(messageData->pbxsecretID, messageContent, 8);
				messageContent +=8;
				memcpy(messageData->pv, messageContent, pvLength);
				messageContent +=pvLength;
				memcpy(messageData->MAC, messageContent, 8);

				/* attach the message structure to the packet one */
				zrtpPacket->messageData = (void *)messageData;

				/* the parsed commit packet must be saved as it is used to generate the total_hash */
				zrtpPacket->packetString = (uint8_t *)malloc(inputLength*sizeof(uint8_t));
				memcpy(zrtpPacket->packetString, input, inputLength); /* store the whole packet even if we may use the message only */
			}
			break; /* MSGTYPE_DHPART1 and MSGTYPE_DHPART2 */
		case MSGTYPE_CONFIRM1:
		case MSGTYPE_CONFIRM2:
			{
				uint8_t *confirmMessageKey = NULL;
				uint8_t *confirmMessageMacKey = NULL;
				/* we shall first decrypt and validate the message, check we have the keys to do it */
				if (zrtpChannelContext->role == RESPONDER) { /* responder uses initiator's keys to decrypt */
					if ((zrtpChannelContext->zrtpkeyi == NULL) || (zrtpChannelContext->mackeyi == NULL)) {
						return BZRTP_PARSER_ERROR_INVALIDCONTEXT;
					}
					confirmMessageKey = zrtpChannelContext->zrtpkeyi;
					confirmMessageMacKey = zrtpChannelContext->mackeyi;
				}

				if (zrtpChannelContext->role == INITIATOR) { /* the iniator uses responder's keys to decrypt */
					if ((zrtpChannelContext->zrtpkeyr == NULL) || (zrtpChannelContext->mackeyr == NULL)) {
						return BZRTP_PARSER_ERROR_INVALIDCONTEXT;
					}
					confirmMessageKey = zrtpChannelContext->zrtpkeyr;
					confirmMessageMacKey = zrtpChannelContext->mackeyr;
				}
				
				/* allocate a confirm message structure */
				bzrtpConfirmMessage_t *messageData;
				messageData = (bzrtpConfirmMessage_t *)malloc(sizeof(bzrtpConfirmMessage_t));

				/* get the mac and the IV */
				memcpy(messageData->confirm_mac, messageContent, 8);
				messageContent +=8;
				memcpy(messageData->CFBIV, messageContent, 16);
				messageContent +=16;


				
				/* get the cipher text length */
				uint16_t cipherTextLength = zrtpPacket->messageLength - ZRTP_MESSAGE_HEADER_LENGTH - 24; /* confirm message is header, confirm_mac(8 bytes), CFB IV(16 bytes), encrypted part */

				/* validate the mac over the cipher text */
				uint8_t computedHmac[8];
				zrtpChannelContext->hmacFunction(confirmMessageMacKey, zrtpChannelContext->hashLength, messageContent, cipherTextLength, 8, computedHmac);
				
				if (memcmp(computedHmac, messageData->confirm_mac, 8) != 0) { /* confirm_mac doesn't match */
					free(messageData);
					return BZRTP_PARSER_ERROR_UNMATCHINGCONFIRMMAC;
				}

				/* get plain message */
				uint8_t *confirmPlainMessageBuffer = (uint8_t *)malloc(cipherTextLength*sizeof(uint8_t));
				zrtpChannelContext->cipherDecryptionFunction(confirmMessageKey, messageData->CFBIV, messageContent, cipherTextLength, confirmPlainMessageBuffer);
				uint8_t *confirmPlainMessage = confirmPlainMessageBuffer; /* point into the allocated buffer */

				/* parse it */
				memcpy(messageData->H0, confirmPlainMessage, 32);
				confirmPlainMessage +=33; /* +33 because next 8 bits are unused */

				/* Hash chain checking: if we are in multichannel or shared mode, we had not DHPart and then no H1 */
				if (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh || zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult) {
					/* compute the H1=SHA256(H0) we never received */
					uint8_t checkH1[32];
					bzrtpCrypto_sha256(messageData->H0, 32, 32, checkH1);

					/* if we are responder, we received a commit packet with H2 then check that H2=SHA256(H1) and that the commit message MAC keyed with H1 match */
					if ( zrtpChannelContext->role == RESPONDER) {
						uint8_t checkH2[32];
						uint8_t checkMAC[32];
						if (zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID] == NULL) {
							free (messageData);
							/* we have no Commit message in this channel, this Confirm2 shall never have arrived, discard it as invalid */
							return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
						}
						bzrtpCommitMessage_t *peerCommitMessageData = (bzrtpCommitMessage_t *)zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageData;
						/* Check H2 = SHA256(H1) */
						bzrtpCrypto_sha256(checkH1, 32, 32, checkH2);
						if (memcmp(checkH2, peerCommitMessageData->H2, 32) != 0) {
							free (messageData);
							return BZRTP_PARSER_ERROR_UNMATCHINGHASHCHAIN;
						}
						/* Check the Commit MAC message. 
						 * MAC is 8 bytes long and is computed on the message(skip the ZRTP_PACKET_HEADER) and exclude the mac itself (-8 bytes from message Length) */
						bzrtpCrypto_hmacSha256(checkH1, 32, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength-8, 8, checkMAC);
						if (memcmp(checkMAC, peerCommitMessageData->MAC, 8) != 0) {
							free (messageData);
							return BZRTP_PARSER_ERROR_UNMATCHINGMAC;
						}
					} else { /* if we are initiator(we didn't received any commit message and then no H2), we must check that H3=SHA256(SHA256(H1)) and the Hello message MAC */
						uint8_t checkH2[32];
						uint8_t checkH3[32];
						uint8_t checkMAC[32];
						if (zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID] == NULL) {
							free (messageData);
							/* we have no Hello message in this channel, this Confirm1 shall never have arrived, discard it as invalid */
							return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
						}
						bzrtpHelloMessage_t *peerHelloMessageData = (bzrtpHelloMessage_t *)zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageData;
						/* Check H3 = SHA256(SHA256(H1)) */
						bzrtpCrypto_sha256(checkH1, 32, 32, checkH2);
						bzrtpCrypto_sha256(checkH2, 32, 32, checkH3);
						if (memcmp(checkH3, peerHelloMessageData->H3, 32) != 0) {
							free (messageData);
							return BZRTP_PARSER_ERROR_UNMATCHINGHASHCHAIN;
						}
						/* Check the hello MAC message. 
						 * MAC is 8 bytes long and is computed on the message(skip the ZRTP_PACKET_HEADER) and exclude the mac itself (-8 bytes from message Length) */
						bzrtpCrypto_hmacSha256(checkH2, 32, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength-8, 8, checkMAC);
						if (memcmp(checkMAC, peerHelloMessageData->MAC, 8) != 0) {
							free (messageData);
							return BZRTP_PARSER_ERROR_UNMATCHINGMAC;
						}
	
					}
				} else { /* we are in DHM mode */
					/* We have now H0, check it matches the H1 we had in the DHPart message H1=SHA256(H0) and that the DHPart message MAC is correct */
					uint8_t checkH1[32];
					uint8_t checkMAC[32];
					if (zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID] == NULL) {
						free (messageData);
						/* we have no DHPART message in this channel, this confirm shall never have arrived, discard it as invalid */
						return BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE;
					}
					bzrtpDHPartMessage_t *peerDHPartMessageData = (bzrtpDHPartMessage_t *)zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageData;
					/* Check H1 = SHA256(H0) */
					bzrtpCrypto_sha256(messageData->H0, 32, 32, checkH1);
					if (memcmp(checkH1, peerDHPartMessageData->H1, 32) != 0) {
						free (messageData);
						return BZRTP_PARSER_ERROR_UNMATCHINGHASHCHAIN;
					}
					/* Check the DHPart message. 
					 * MAC is 8 bytes long and is computed on the message(skip the ZRTP_PACKET_HEADER) and exclude the mac itself (-8 bytes from message Length) */
					bzrtpCrypto_hmacSha256(messageData->H0, 32, zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpChannelContext->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength-8, 8, checkMAC);
					if (memcmp(checkMAC, peerDHPartMessageData->MAC, 8) != 0) {
						free (messageData);
						return BZRTP_PARSER_ERROR_UNMATCHINGMAC;
					}
				}

				messageData->sig_len = ((uint16_t)(confirmPlainMessage[0]&0x01))<<8 | (((uint16_t)confirmPlainMessage[1])&0x00FF);
				confirmPlainMessage += 2;
				messageData->E = ((*confirmPlainMessage)&0x08)>>3;
				messageData->V = ((*confirmPlainMessage)&0x04)>>2;
				messageData->A = ((*confirmPlainMessage)&0x02)>>1;
				messageData->D = (*confirmPlainMessage)&0x01;
				confirmPlainMessage += 1;

				messageData->cacheExpirationInterval = (((uint32_t)confirmPlainMessage[0])<<24) | (((uint32_t)confirmPlainMessage[1])<<16) | (((uint32_t)confirmPlainMessage[2])<<8) | ((uint32_t)confirmPlainMessage[3]);
				confirmPlainMessage += 4;


				/* if sig_len indicate a signature, parse it */
				if (messageData->sig_len>0) {
					memcpy(messageData->signatureBlockType, confirmPlainMessage, 4);
					confirmPlainMessage += 4;
					/* allocate memory for the signature block, sig_len is in words(32 bits) and includes the signature block type word */
					messageData->signatureBlock = (uint8_t *)malloc(4*(messageData->sig_len-1)*sizeof(uint8_t));
					memcpy(messageData->signatureBlock, confirmPlainMessage, 4*(messageData->sig_len-1));
				} else {
					messageData->signatureBlock  = NULL;
				}

				/* free plain buffer */
				free(confirmPlainMessageBuffer);

				/* attach the message structure to the packet one */
				zrtpPacket->messageData = (void *)messageData;
			}
			break; /* MSGTYPE_CONFIRM1 and MSGTYPE_CONFIRM2 */

		case MSGTYPE_CONF2ACK:
			/* nothing to do for this one */
			break; /* MSGTYPE_CONF2ACK */
		
		case MSGTYPE_PING:
			{
				/* allocate a ping message structure */
				bzrtpPingMessage_t *messageData;
				messageData = (bzrtpPingMessage_t *)malloc(sizeof(bzrtpPingMessage_t));

				/* fill the structure */
				memcpy(messageData->version, messageContent, 4);
				messageContent +=4;
				memcpy(messageData->endpointHash, messageContent, 8);

				/* attach the message structure to the packet one */
				zrtpPacket->messageData = (void *)messageData;
			}
			break; /* MSGTYPE_PING */

	}

	return 0;
}

/* Create the packet string from the messageData contained into the zrtp Packet structure */
int bzrtp_packetBuild(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, bzrtpPacket_t *zrtpPacket, uint16_t sequenceNumber) {
	
	int i;
	/* checks */
	if (zrtpPacket==NULL) {
		return BZRTP_BUILDER_ERROR_INVALIDPACKET;
	}

	/* get the message type (and check it is valid) */
	uint8_t *messageTypeString = messageTypeInttoString(zrtpPacket->messageType);
	if (messageTypeString == NULL) {
		return BZRTP_BUILDER_ERROR_INVALIDMESSAGETYPE;
	}

	uint8_t *messageString = NULL; /* will point directly to the begining of the message within the packetString buffer */
	uint8_t *MACbuffer = NULL; /* if needed this will point to the beginin of the MAC in the packetString buffer */
	/*uint8_t *MACMessageData = NULL; */ /* if needed this will point to the MAC field in the message Data structure */
	uint8_t *MACkey = NULL;

	/* create first the message. Header and CRC will be added afterward */
	switch (zrtpPacket->messageType) {
		case MSGTYPE_HELLO : 
			{
				/* get the Hello message structure */
				if (zrtpPacket->messageData == NULL) {
					return BZRTP_BUILDER_ERROR_INVALIDMESSAGE;
				}
				bzrtpHelloMessage_t *messageData = (bzrtpHelloMessage_t *)zrtpPacket->messageData;

				/* compute the message length in bytes : fixed length and optionnal algorithms parts */
				zrtpPacket->messageLength = ZRTP_HELLOMESSAGE_FIXED_LENGTH + 4*((uint16_t)(messageData->hc)+(uint16_t)(messageData->cc)+(uint16_t)(messageData->ac)+(uint16_t)(messageData->kc)+(uint16_t)(messageData->sc));

				/* allocate the packetString buffer : packet is header+message+crc */
				zrtpPacket->packetString = (uint8_t *)malloc((ZRTP_PACKET_HEADER_LENGTH+zrtpPacket->messageLength+ZRTP_PACKET_CRC_LENGTH)*sizeof(uint8_t));
				/* have the messageString pointer to the begining of message(after the message header wich is computed for all messages after the switch)
				 * within the packetString buffer*/
				messageString = zrtpPacket->packetString + ZRTP_PACKET_HEADER_LENGTH + ZRTP_MESSAGE_HEADER_LENGTH;

				/* set the version (shall be 1.10), Client identifier, H3, ZID, S,M,P flags and  hc,cc,ac,kc,sc */
				memcpy(messageString, messageData->version, 4);
				messageString += 4;
				memcpy(messageString, messageData->clientIdentifier, 16);
				messageString += 16;
				memcpy(messageString, messageData->H3, 32);
				messageString += 32;
				memcpy(messageString, messageData->ZID, 12);
				messageString += 12;
				*messageString = ((((messageData->S)&0x01)<<6) | (((messageData->M)&0x01)<<5) | (((messageData->P)&0x01)<<4))&0x70;
				messageString += 1;
				*messageString = (messageData->hc)&0x0F;
				messageString += 1;
				*messageString = (((messageData->cc)<<4)&0xF0) | ((messageData->ac)&0x0F) ;
				messageString += 1;
				*messageString = (((messageData->kc)<<4)&0xF0) | ((messageData->sc)&0x0F) ;
				messageString += 1;

				/* now set optionnal supported algorithms */
				for (i=0; i<messageData->hc; i++) {
					cryptoAlgoTypeIntToString(messageData->supportedHash[i], messageString);
					messageString +=4;
				}
				for (i=0; i<messageData->cc; i++) {
					cryptoAlgoTypeIntToString(messageData->supportedCipher[i], messageString);
					messageString +=4;
				}
				for (i=0; i<messageData->ac; i++) {
					cryptoAlgoTypeIntToString(messageData->supportedAuthTag[i], messageString);
					messageString +=4;
				}
				for (i=0; i<messageData->kc; i++) {
					cryptoAlgoTypeIntToString(messageData->supportedKeyAgreement[i], messageString);
					messageString +=4;
				}
				for (i=0; i<messageData->sc; i++) {
					cryptoAlgoTypeIntToString(messageData->supportedSas[i], messageString);
					messageString +=4;
				}

				/* there is a MAC to compute, set the pointers to the key and MAC output buffer */
				MACbuffer = messageString;
				MACkey = zrtpChannelContext->selfH[2]; /* HMAC of Hello packet is keyed by H2 which have been set at context initialising */

			}
			break; /* MSGTYPE_HELLO */

		case MSGTYPE_HELLOACK : 
			{
				/* the message length is fixed */
				zrtpPacket->messageLength = ZRTP_HELLOACKMESSAGE_FIXED_LENGTH;

				/* allocate the packetString buffer : packet is header+message+crc */
				zrtpPacket->packetString = (uint8_t *)malloc((ZRTP_PACKET_HEADER_LENGTH+ZRTP_HELLOACKMESSAGE_FIXED_LENGTH+ZRTP_PACKET_CRC_LENGTH)*sizeof(uint8_t));
			}
			break; /* MSGTYPE_HELLOACK */

		case MSGTYPE_COMMIT :
			{
				/* get the Commit message structure */
				if (zrtpPacket->messageData == NULL) {
					return BZRTP_BUILDER_ERROR_INVALIDMESSAGE;
				}

				bzrtpCommitMessage_t *messageData = (bzrtpCommitMessage_t *)zrtpPacket->messageData;

				/* compute message length */
				uint16_t variableLength = 0;
				switch(messageData->keyAgreementAlgo) {
					case ZRTP_KEYAGREEMENT_DH2k :
					case ZRTP_KEYAGREEMENT_EC25 :
					case ZRTP_KEYAGREEMENT_DH3k :
					case ZRTP_KEYAGREEMENT_EC38 :
					case ZRTP_KEYAGREEMENT_EC52 :
						variableLength = 32; /* hvi is 32 bytes length in DH Commit message format */
						break;
					case ZRTP_KEYAGREEMENT_Prsh :
						variableLength = 24; /* nonce (16 bytes) and keyID(8 bytes) are 24 bytes length in preshared Commit message format */
						break;
					case ZRTP_KEYAGREEMENT_Mult :
						variableLength = 16; /* nonce is 24 bytes length in multistream Commit message format */
						break;
					default:
						return BZRTP_BUILDER_ERROR_INVALIDMESSAGE;
				}
				zrtpPacket->messageLength = ZRTP_COMMITMESSAGE_FIXED_LENGTH + variableLength;
				
				/* allocate the packetString buffer : packet is header+message+crc */
				zrtpPacket->packetString = (uint8_t *)malloc((ZRTP_PACKET_HEADER_LENGTH+zrtpPacket->messageLength+ZRTP_PACKET_CRC_LENGTH)*sizeof(uint8_t));
				/* have the messageString pointer to the begining of message(after the message header wich is computed for all messages after the switch)
				 * within the packetString buffer*/
				messageString = zrtpPacket->packetString + ZRTP_PACKET_HEADER_LENGTH + ZRTP_MESSAGE_HEADER_LENGTH;

				/* now insert the different message parts into the packetString */
				memcpy(messageString, messageData->H2, 32);
				messageString += 32;
				memcpy(messageString, messageData->ZID, 12);
				messageString += 12;
				cryptoAlgoTypeIntToString(messageData->hashAlgo, messageString);
				messageString += 4;
				cryptoAlgoTypeIntToString(messageData->cipherAlgo, messageString);
				messageString += 4;
				cryptoAlgoTypeIntToString(messageData->authTagAlgo, messageString);
				messageString += 4;
				cryptoAlgoTypeIntToString(messageData->keyAgreementAlgo, messageString);
				messageString += 4;
				cryptoAlgoTypeIntToString(messageData->sasAlgo, messageString);
				messageString += 4;

				/* if it is a multistream or preshared commit insert the 16 bytes nonce */
				if ((messageData->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) || (messageData->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult)) {
						memcpy(messageString, messageData->nonce, 16);
						messageString += 16;

					/* and the keyID for preshared commit only */
					if (messageData->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) {
						memcpy(messageString, messageData->keyID, 8);
						messageString +=8;
					}
				} else { /* it's a DH commit message, set the hvi */
					memcpy(messageString, messageData->hvi, 32);
					messageString +=32;
				}
			
				/* there is a MAC to compute, set the pointers to the key and MAC output buffer */
				MACbuffer = messageString;
				MACkey = zrtpChannelContext->selfH[1]; /* HMAC of Hello packet is keyed by H1 which have been set at context initialising */
			}
			break; /*MSGTYPE_COMMIT */

		case MSGTYPE_DHPART1 :
		case MSGTYPE_DHPART2 :
			{
				/* get the DHPart message structure */
				if (zrtpPacket->messageData == NULL) {
					return BZRTP_BUILDER_ERROR_INVALIDMESSAGE;
				}

				bzrtpDHPartMessage_t *messageData = (bzrtpDHPartMessage_t *)zrtpPacket->messageData;

				/* compute message length */
				uint16_t pvLength = computeKeyAgreementPrivateValueLength(zrtpChannelContext->keyAgreementAlgo);
				if (pvLength==0) {
					return BZRTP_BUILDER_ERROR_INVALIDCONTEXT;
				}
				zrtpPacket->messageLength = ZRTP_DHPARTMESSAGE_FIXED_LENGTH + pvLength;
				
				/* allocate the packetString buffer : packet is header+message+crc */
				zrtpPacket->packetString = (uint8_t *)malloc((ZRTP_PACKET_HEADER_LENGTH+zrtpPacket->messageLength+ZRTP_PACKET_CRC_LENGTH)*sizeof(uint8_t));
				/* have the messageString pointer to the begining of message(after the message header wich is computed for all messages after the switch)
				 * within the packetString buffer*/
				messageString = zrtpPacket->packetString + ZRTP_PACKET_HEADER_LENGTH + ZRTP_MESSAGE_HEADER_LENGTH;

				/* now insert the different message parts into the packetString */
				memcpy(messageString, messageData->H1, 32);
				messageString += 32;
				memcpy(messageString, messageData->rs1ID, 8);
				messageString += 8;
				memcpy(messageString, messageData->rs2ID, 8);
				messageString += 8;
				memcpy(messageString, messageData->auxsecretID, 8);
				messageString += 8;
				memcpy(messageString, messageData->pbxsecretID, 8);
				messageString += 8;
				memcpy(messageString, messageData->pv, pvLength);
				messageString += pvLength;

				/* there is a MAC to compute, set the pointers to the key and MAC output buffer */
				MACbuffer = messageString;
				MACkey = zrtpChannelContext->selfH[0]; /* HMAC of Hello packet is keyed by H0 which have been set at context initialising */
			}
			break; /* MSGTYPE_DHPART1 and 2 */
		
		case MSGTYPE_CONFIRM1:
		case MSGTYPE_CONFIRM2:
			{
				uint8_t *confirmMessageKey = NULL;
				uint8_t *confirmMessageMacKey = NULL;
				/* we will have to encrypt and validate the message, check we have the keys to do it */
				if (zrtpChannelContext->role == INITIATOR) {
					if ((zrtpChannelContext->zrtpkeyi == NULL) || (zrtpChannelContext->mackeyi == NULL)) {
						return BZRTP_BUILDER_ERROR_INVALIDCONTEXT;
					}
					confirmMessageKey = zrtpChannelContext->zrtpkeyi;
					confirmMessageMacKey = zrtpChannelContext->mackeyi;
				}

				if (zrtpChannelContext->role == RESPONDER) {
					if ((zrtpChannelContext->zrtpkeyr == NULL) || (zrtpChannelContext->mackeyr == NULL)) {
						return BZRTP_BUILDER_ERROR_INVALIDCONTEXT;
					}
					confirmMessageKey = zrtpChannelContext->zrtpkeyr;
					confirmMessageMacKey = zrtpChannelContext->mackeyr;
				}
				
				/* get the Confirm message structure */
				if (zrtpPacket->messageData == NULL) {
					return BZRTP_BUILDER_ERROR_INVALIDMESSAGE;
				}

				bzrtpConfirmMessage_t *messageData = (bzrtpConfirmMessage_t *)zrtpPacket->messageData;

				/* compute message length */
				zrtpPacket->messageLength = ZRTP_CONFIRMMESSAGE_FIXED_LENGTH + messageData->sig_len*4; /* sig_len is in word of 4 bytes */
				
				/* allocate the packetString buffer : packet is header+message+crc */
				zrtpPacket->packetString = (uint8_t *)malloc((ZRTP_PACKET_HEADER_LENGTH+zrtpPacket->messageLength+ZRTP_PACKET_CRC_LENGTH)*sizeof(uint8_t));
				/* have the messageString pointer to the begining of message(after the message header wich is computed for all messages after the switch)
				 * within the packetString buffer*/
				messageString = zrtpPacket->packetString + ZRTP_PACKET_HEADER_LENGTH + ZRTP_MESSAGE_HEADER_LENGTH;

				/* allocate a temporary buffer to store the plain text */
				uint16_t encryptedPartLength = zrtpPacket->messageLength - ZRTP_MESSAGE_HEADER_LENGTH - 24; /* message header, confirm_mac(8 bytes) and CFB IV(16 bytes) are not encrypted */
				uint8_t *plainMessageString = (uint8_t *)malloc(encryptedPartLength*sizeof(uint8_t)); 

				/* fill the plain message buffer with data from the message structure */
				uint16_t plainMessageStringIndex = 0;
				memcpy(plainMessageString, messageData->H0, 32);
				plainMessageStringIndex += 32;
				plainMessageString[plainMessageStringIndex++] = 0x00;
				plainMessageString[plainMessageStringIndex++] = (uint8_t)(((messageData->sig_len)>>8)&0x0001);
				plainMessageString[plainMessageStringIndex++] = (uint8_t)((messageData->sig_len)&0x00FF);
				plainMessageString[plainMessageStringIndex++] = (uint8_t)((messageData->E&0x01)<<3) | (uint8_t)((messageData->V&0x01)<<2) | (uint8_t)((messageData->A&0x01)<<1) | (uint8_t)(messageData->D&0x01) ;
				/* cache expiration in a 32 bits unsigned int */
				plainMessageString[plainMessageStringIndex++] = (uint8_t)((messageData->cacheExpirationInterval>>24)&0xFF);
				plainMessageString[plainMessageStringIndex++] = (uint8_t)((messageData->cacheExpirationInterval>>16)&0xFF);
				plainMessageString[plainMessageStringIndex++] = (uint8_t)((messageData->cacheExpirationInterval>>8)&0xFF);
				plainMessageString[plainMessageStringIndex++] = (uint8_t)((messageData->cacheExpirationInterval)&0xFF);

				if (messageData->sig_len>0) {
					memcpy(plainMessageString+plainMessageStringIndex, messageData->signatureBlockType, 4);
					plainMessageStringIndex += 4;
					/* sig_len is in 4 bytes words and include the 1 word of signature block type */
					memcpy(plainMessageString+plainMessageStringIndex, messageData->signatureBlock, (messageData->sig_len-1)*4);
				}

				/* encrypt the buffer, set the output directly in the messageString buffer at the correct position(+24 after message header) */
				zrtpChannelContext->cipherEncryptionFunction(confirmMessageKey, messageData->CFBIV, plainMessageString, encryptedPartLength, messageString+24);
				free(plainMessageString); /* free the plain message string temporary buffer */

				/* compute the mac over the encrypted part of the message and set the result in the messageString */
				zrtpChannelContext->hmacFunction(confirmMessageMacKey, zrtpChannelContext->hashLength, messageString+24, encryptedPartLength, 8, messageString);
				messageString += 8;
				/* add the CFB IV */
				memcpy(messageString, messageData->CFBIV, 16);
			}
			break; /* MSGTYPE_CONFIRM1 and MSGTYPE_CONFIRM2 */

		case MSGTYPE_CONF2ACK:
			{
				/* the message length is fixed */
				zrtpPacket->messageLength = ZRTP_CONF2ACKMESSAGE_FIXED_LENGTH;

				/* allocate the packetString buffer : packet is header+message+crc */
				zrtpPacket->packetString = (uint8_t *)malloc((ZRTP_PACKET_HEADER_LENGTH+ZRTP_CONF2ACKMESSAGE_FIXED_LENGTH+ZRTP_PACKET_CRC_LENGTH)*sizeof(uint8_t));
			}
			break; /* MSGTYPE_CONF2ACK */

		case MSGTYPE_PINGACK:
			{
				/* the message length is fixed */
				zrtpPacket->messageLength = ZRTP_PINGACKMESSAGE_FIXED_LENGTH;

				/* allocate the packetString buffer : packet is header+message+crc */
				zrtpPacket->packetString = (uint8_t *)malloc((ZRTP_PACKET_HEADER_LENGTH+ZRTP_PINGACKMESSAGE_FIXED_LENGTH+ZRTP_PACKET_CRC_LENGTH)*sizeof(uint8_t));
				messageString = zrtpPacket->packetString + ZRTP_PACKET_HEADER_LENGTH + ZRTP_MESSAGE_HEADER_LENGTH;

				/* now insert the different message parts into the packetString */
				bzrtpPingAckMessage_t *messageData = (bzrtpPingAckMessage_t *)zrtpPacket->messageData;

				memcpy(messageString, messageData->version, 4);
				messageString += 4;
				memcpy(messageString, messageData->endpointHash, 8);
				messageString += 8;
				memcpy(messageString, messageData->endpointHashReceived, 8);
				messageString += 8;
				*messageString++ = (uint8_t)((messageData->SSRC>>24)&0xFF);
				*messageString++ = (uint8_t)((messageData->SSRC>>16)&0xFF);
				*messageString++ = (uint8_t)((messageData->SSRC>>8)&0xFF);
				*messageString++ = (uint8_t)(messageData->SSRC&0xFF);
			}
			break; /* MSGTYPE_PINGACK */

	}

	/* write headers only if we have a packet string */
	if (zrtpPacket->packetString != NULL) { 
		zrtpMessageSetHeader(zrtpPacket->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpPacket->messageLength, messageTypeString);

		/* Do we have a MAC to compute on the message ? */
		if (MACbuffer != NULL) {
			/* compute the MAC(64 bits only) using the implicit HMAC function for ZRTP v1.10: HMAC-SHA256 */
			/* HMAC is computed on the whole message except the MAC itself so a length of zrtpPacket->messageLength-8 */
			bzrtpCrypto_hmacSha256(MACkey, 32, zrtpPacket->packetString+ZRTP_PACKET_HEADER_LENGTH, zrtpPacket->messageLength-8, 8, MACbuffer);
		}
	
		/* set packet header and CRC */
		/* preambule */
		zrtpPacket->packetString[0] = 0x10;
		zrtpPacket->packetString[1] = 0x00;
		/* Sequence number */
		zrtpPacket->packetString[2] = (uint8_t)((sequenceNumber>>8)&0x00FF);
		zrtpPacket->packetString[3] = (uint8_t)(sequenceNumber&0x00FF);
		/* ZRTP magic cookie */
		zrtpPacket->packetString[4] = (uint8_t)((ZRTP_MAGIC_COOKIE>>24)&0xFF);
		zrtpPacket->packetString[5] = (uint8_t)((ZRTP_MAGIC_COOKIE>>16)&0xFF);
		zrtpPacket->packetString[6] = (uint8_t)((ZRTP_MAGIC_COOKIE>>8)&0xFF);
		zrtpPacket->packetString[7] = (uint8_t)(ZRTP_MAGIC_COOKIE&0xFF);
		/* Source Identifier */
		zrtpPacket->packetString[8] = (uint8_t)(((zrtpPacket->sourceIdentifier)>>24)&0xFF);
		zrtpPacket->packetString[9] = (uint8_t)(((zrtpPacket->sourceIdentifier)>>16)&0xFF);
		zrtpPacket->packetString[10] = (uint8_t)(((zrtpPacket->sourceIdentifier)>>8)&0xFF);
		zrtpPacket->packetString[11] = (uint8_t)((zrtpPacket->sourceIdentifier)&0xFF);
		/* CRC */
		uint32_t CRC = bzrtp_CRC32(zrtpPacket->packetString, zrtpPacket->messageLength+ZRTP_PACKET_HEADER_LENGTH);
		uint8_t *CRCbuffer = (zrtpPacket->packetString)+(zrtpPacket->messageLength)+ZRTP_PACKET_HEADER_LENGTH;
		*CRCbuffer = (uint8_t)((CRC>>24)&0xFF);
		CRCbuffer++;
		*CRCbuffer = (uint8_t)((CRC>>16)&0xFF);
		CRCbuffer++;
		*CRCbuffer = (uint8_t)((CRC>>8)&0xFF);
		CRCbuffer++;
		*CRCbuffer = (uint8_t)(CRC&0xFF);
	
		return 0;
	} else { /* no packetString allocated something wen't wrong but we shall never arrive here */
		return BZRTP_BUILDER_ERROR_UNKNOWN;
	}
}

/* create a zrtpPacket and initialise it's structures */
bzrtpPacket_t *bzrtp_createZrtpPacket(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, uint32_t messageType, int *exitCode) {
	/* allocate packet */
	bzrtpPacket_t *zrtpPacket = (bzrtpPacket_t *)malloc(sizeof(bzrtpPacket_t));
	memset(zrtpPacket, 0, sizeof(bzrtpPacket_t));
	zrtpPacket->messageData = NULL;
	zrtpPacket->packetString = NULL;

	/* initialise it */
	switch(messageType) {
		case MSGTYPE_HELLO:
			{
				bzrtpHelloMessage_t *zrtpHelloMessage = (bzrtpHelloMessage_t *)malloc(sizeof(bzrtpHelloMessage_t));
				memset(zrtpHelloMessage, 0, sizeof(bzrtpHelloMessage_t));
				/* initialise some fields using zrtp context data */
				memcpy(zrtpHelloMessage->version, ZRTP_VERSION, 4);
				memcpy(zrtpHelloMessage->clientIdentifier, ZRTP_CLIENT_IDENTIFIER, 16);
				memcpy(zrtpHelloMessage->H3, zrtpChannelContext->selfH[3], 32);
				memcpy(zrtpHelloMessage->ZID, zrtpContext->selfZID, 12);
				/* set all S,M,P flags to zero as we're not able to verify signatures, we're not a PBX(TODO: implement?), we're not passive */
				zrtpHelloMessage->S = 0;
				zrtpHelloMessage->M = 0;
				zrtpHelloMessage->P = 0;

				/* get the algorithm availabilities from the context */
				zrtpHelloMessage->hc = zrtpContext->hc;
				zrtpHelloMessage->cc = zrtpContext->cc;
				zrtpHelloMessage->ac = zrtpContext->ac;
				zrtpHelloMessage->kc = zrtpContext->kc;
				zrtpHelloMessage->sc = zrtpContext->sc;
				int i;
				for (i=0; i<zrtpContext->hc; i++) {
					zrtpHelloMessage->supportedHash[i] = zrtpContext->supportedHash[i];
				}
				for (i=0; i<zrtpContext->cc; i++) {
					zrtpHelloMessage->supportedCipher[i] = zrtpContext->supportedCipher[i];
				}
				for (i=0; i<zrtpContext->ac; i++) {
					zrtpHelloMessage->supportedAuthTag[i] = zrtpContext->supportedAuthTag[i];
				}
				for (i=0; i<zrtpContext->kc; i++) {
					zrtpHelloMessage->supportedKeyAgreement[i] = zrtpContext->supportedKeyAgreement[i];
				}
				for (i=0; i<zrtpContext->sc; i++) {
					zrtpHelloMessage->supportedSas[i] = zrtpContext->supportedSas[i];
				}

				/* attach the message data to the packet */
				zrtpPacket->messageData = zrtpHelloMessage;
			}
			break; /* MSGTYPE_HELLO */

		case MSGTYPE_HELLOACK :
			{
				/* nothing to do for the Hello ACK packet as it just contains it's type */
			}
			break; /* MSGTYPE_HELLOACK */
		/* In case of DH commit, this one must be called after the DHPart build and the self DH message and peer Hello message are stored in the context */
		case MSGTYPE_COMMIT :
			{
				bzrtpCommitMessage_t *zrtpCommitMessage = (bzrtpCommitMessage_t *)malloc(sizeof(bzrtpCommitMessage_t));
				memset(zrtpCommitMessage, 0, sizeof(bzrtpCommitMessage_t));
				
				/* initialise some fields using zrtp context data */
				memcpy(zrtpCommitMessage->H2, zrtpChannelContext->selfH[2], 32);
				memcpy(zrtpCommitMessage->ZID, zrtpContext->selfZID, 12);
				zrtpCommitMessage->hashAlgo = zrtpChannelContext->hashAlgo;
				zrtpCommitMessage->cipherAlgo = zrtpChannelContext->cipherAlgo;
				zrtpCommitMessage->authTagAlgo = zrtpChannelContext->authTagAlgo;
				zrtpCommitMessage->keyAgreementAlgo = zrtpChannelContext->keyAgreementAlgo;
				zrtpCommitMessage->sasAlgo = zrtpChannelContext->sasAlgo;

				/* if it is a multistream or preshared commit create a 16 random bytes nonce */
				if ((zrtpCommitMessage->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) || (zrtpCommitMessage->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult)) {
					bzrtpCrypto_getRandom(zrtpContext->RNGContext, zrtpCommitMessage->nonce, 16);

					/* and the keyID for preshared commit only */
					if (zrtpCommitMessage->keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) {
						/* TODO at this point we must first compute the preShared key - make sure at least rs1 is present */
						/* preshared_key = hash(len(rs1) || rs1 || len(auxsecret) || auxsecret ||
                       len(pbxsecret) || pbxsecret) using the agreed hash and store it into the env */
						/* and then the keyID : MAC(preshared_key, "Prsh") truncated to 64 bits using the agreed MAC */
					}
				} else { /* it's a DH commit message, set the hvi */
					/* hvi = hash(initiator's DHPart2 message || responder's Hello message) using the agreed hash function truncated to 256 bits */
					/* create a string with the messages concatenated */
					uint16_t DHPartMessageLength = zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength;
					uint16_t HelloMessageLength = zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
					uint16_t DHPartHelloMessageStringLength = DHPartMessageLength + HelloMessageLength;

					uint8_t *DHPartHelloMessageString = (uint8_t *)malloc(DHPartHelloMessageStringLength*sizeof(uint8_t));
					
					memcpy(DHPartHelloMessageString, zrtpChannelContext->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, DHPartMessageLength);
					memcpy(DHPartHelloMessageString+DHPartMessageLength, zrtpChannelContext->peerPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, HelloMessageLength);

					zrtpChannelContext->hashFunction(DHPartHelloMessageString, DHPartHelloMessageStringLength, 32, zrtpCommitMessage->hvi);

					free(DHPartHelloMessageString);
				}

				/* attach the message data to the packet */
				zrtpPacket->messageData = zrtpCommitMessage;
			}
			break; /* MSGTYPE_COMMIT */

		/* this one is called after the exchange of Hello messages when the crypto algo agreement have been performed */
		case MSGTYPE_DHPART1 :
		case MSGTYPE_DHPART2 :
			{
				bzrtpDHPartMessage_t *zrtpDHPartMessage = (bzrtpDHPartMessage_t *)malloc(sizeof(bzrtpDHPartMessage_t));
				memset(zrtpDHPartMessage, 0, sizeof(bzrtpDHPartMessage_t));
				/* initialise some fields using zrtp context data */
				memcpy(zrtpDHPartMessage->H1, zrtpChannelContext->selfH[1], 32);
				/* get the retained secret from context, we anyway create a DHPart2 packet that we may turn into a DHPart1 packet if we end to 
				 * be the responder and not the initiator, use the initiator retained secret hashes */
				memcpy(zrtpDHPartMessage->rs1ID, zrtpContext->initiatorCachedSecretHash.rs1ID, 8);
				memcpy(zrtpDHPartMessage->rs2ID, zrtpContext->initiatorCachedSecretHash.rs2ID, 8);
				memcpy(zrtpDHPartMessage->auxsecretID, zrtpChannelContext->initiatorAuxsecretID, 8);
				memcpy(zrtpDHPartMessage->pbxsecretID, zrtpContext->initiatorCachedSecretHash.pbxsecretID, 8);
				
				/* compute the public value and insert it in the message, will then be used whatever role - initiator or responder - we assume */
				/* initialise the dhm context, secret length shall be twice the size of cipher block key length - rfc section 5.1.5 */
				uint8_t secretLength; /* is in bytes */
				switch (zrtpChannelContext->cipherAlgo) {
					case ZRTP_CIPHER_AES3:
					case ZRTP_CIPHER_2FS3:
						secretLength = 64;
						break;
					case ZRTP_CIPHER_AES2:
					case ZRTP_CIPHER_2FS2:
						secretLength = 48;
						break;
					case ZRTP_CIPHER_AES1:
					case ZRTP_CIPHER_2FS1:
					default:
						secretLength = 32;
						break;
				}
				zrtpContext->DHMContext = bzrtpCrypto_CreateDHMContext(zrtpChannelContext->keyAgreementAlgo, secretLength);
				if (zrtpContext->DHMContext == NULL) {
					free(zrtpPacket);
					free(zrtpDHPartMessage);
					*exitCode = BZRTP_CREATE_ERROR_UNABLETOCREATECRYPTOCONTEXT;
					return NULL;
				}

				/* now compute the public value */
				bzrtpCrypto_DHMCreatePublic(zrtpContext->DHMContext, (int (*)(void *, uint8_t *, size_t))bzrtpCrypto_getRandom, zrtpContext->RNGContext);
				zrtpDHPartMessage->pv = (uint8_t *)malloc((zrtpChannelContext->keyAgreementLength)*sizeof(uint8_t));
				memcpy(zrtpDHPartMessage->pv, zrtpContext->DHMContext->self, zrtpChannelContext->keyAgreementLength);

				/* attach the message data to the packet */
				zrtpPacket->messageData = zrtpDHPartMessage;
			}
			break; /* MSGTYPE_DHPART1 and MSGTYPE_DHPART2 */

		case MSGTYPE_CONFIRM1:
		case MSGTYPE_CONFIRM2:
			{
				bzrtpConfirmMessage_t *zrtpConfirmMessage = (bzrtpConfirmMessage_t *)malloc(sizeof(bzrtpConfirmMessage_t));
				memset(zrtpConfirmMessage, 0, sizeof(bzrtpConfirmMessage_t));
				/* initialise some fields using zrtp context data */
				memcpy(zrtpConfirmMessage->H0, zrtpChannelContext->selfH[0], 32);
				zrtpConfirmMessage->sig_len = 0; /* signature is not supported */
				zrtpConfirmMessage->cacheExpirationInterval = 0xFFFFFFFF; /* expiration interval is set to unlimited as recommended in rfc section 4.9 */
				zrtpConfirmMessage->E = 0; /* we are not a PBX and then will never signal an enrollment - rfc section 7.3.1 */
				zrtpConfirmMessage->V = zrtpContext->cachedSecret.previouslyVerifiedSas;
				zrtpConfirmMessage->A = 0; /* Go clear message is not supported - rfc section 4.7.2 */
				zrtpConfirmMessage->D = 0; /* The is no backdoor in our implementation of ZRTP - rfc section 11 */

				/* generate a random CFB IV */
				bzrtpCrypto_getRandom(zrtpContext->RNGContext, zrtpConfirmMessage->CFBIV, 16);

				/* attach the message data to the packet */
				zrtpPacket->messageData = zrtpConfirmMessage;
			}
			break; /* MSGTYPE_CONFIRM1 and MSGTYPE_CONFIRM2 */

		case MSGTYPE_CONF2ACK :
			{
				/* nothing to do for the conf2ACK packet as it just contains it's type */
			}
			break; /* MSGTYPE_CONF2ACK */
		case MSGTYPE_PINGACK:
			{
				/* to create a pingACK we must have a ping packet in the channel context, check it */
				bzrtpPacket_t *pingPacket = zrtpChannelContext->pingPacket;
				if (pingPacket == NULL) {
					*exitCode = BZRTP_CREATE_ERROR_INVALIDCONTEXT;
					return NULL;
				}
				bzrtpPingMessage_t *pingMessage = (bzrtpPingMessage_t *)pingPacket->messageData;

				/* create the message */
				bzrtpPingAckMessage_t *zrtpPingAckMessage = (bzrtpPingAckMessage_t *)malloc(sizeof(bzrtpPingAckMessage_t));
				memset(zrtpPingAckMessage, 0, sizeof(bzrtpPingAckMessage_t));

				/* initialise all fields using zrtp context data and the received ping message */
				memcpy(zrtpPingAckMessage->version,ZRTP_VERSION , 4); /* we support version 1.10 only, so no need to even check what was sent in the ping */
				memcpy(zrtpPingAckMessage->endpointHash, zrtpContext->selfZID, 8); /* as suggested in rfc section 5.16, use the truncated ZID as endPoint hash */
				memcpy(zrtpPingAckMessage->endpointHashReceived, pingMessage->endpointHash, 8);
				zrtpPingAckMessage->SSRC = pingPacket->sourceIdentifier;

				/* attach the message data to the packet */
				zrtpPacket->messageData = zrtpPingAckMessage;
			} /* MSGTYPE_PINGACK */
			break;

		default:
			free(zrtpPacket);
			*exitCode = BZRTP_CREATE_ERROR_INVALIDMESSAGETYPE;
			return NULL;
			break;
	}

	zrtpPacket->sequenceNumber = 0; /* this field is not used buy the packet creator, sequence number is given as a parameter when converting
	the message to a packet string(packet build). Used only when parsing a string into a packet struct */
	zrtpPacket->messageType = messageType;
	zrtpPacket->sourceIdentifier = zrtpChannelContext->selfSSRC;
	zrtpPacket->messageLength = 0; /* length will be computed at packet build */
	zrtpPacket->packetString = NULL;

	*exitCode=0;
	return zrtpPacket;
}

void bzrtp_freeZrtpPacket(bzrtpPacket_t *zrtpPacket) {
	if (zrtpPacket != NULL) {
		/* some messages have fields to be freed */
		if (zrtpPacket->messageData != NULL) {
			switch(zrtpPacket->messageType) {
				case MSGTYPE_DHPART1 :
				case MSGTYPE_DHPART2 :
					{
						bzrtpDHPartMessage_t *typedMessageData = (bzrtpDHPartMessage_t *)(zrtpPacket->messageData);
						if (typedMessageData != NULL) {
							free(typedMessageData->pv);
						}
					}
					break;
				case MSGTYPE_CONFIRM1:
				case MSGTYPE_CONFIRM2:
					{
						bzrtpConfirmMessage_t *typedMessageData = (bzrtpConfirmMessage_t *)(zrtpPacket->messageData);
						if (typedMessageData != NULL) {
							free(typedMessageData->signatureBlock);
						}
					}
					break;
			}
		}
		free(zrtpPacket->messageData);
		free(zrtpPacket->packetString);
		free(zrtpPacket);
	}
}

/**
 * @brief Modify the current sequence number of the packet in the packetString and sequenceNumber fields
 * The CRC at the end of packetString is also updated
 * 
 * param[in/out]	zrtpPacket		The zrtpPacket to modify, the packetString must have been generated by
 * 									a call to bzrtp_packetBuild on this packet
 * param[in]		sequenceNumber	The new sequence number to insert in the packetString
 * 
 * return		0 on succes, error code otherwise
 */
int bzrtp_packetUpdateSequenceNumber(bzrtpPacket_t *zrtpPacket, uint16_t sequenceNumber) {
	if (zrtpPacket == NULL) {
		return BZRTP_BUILDER_ERROR_INVALIDPACKET;
	}

	if (zrtpPacket->packetString == NULL) {
		return BZRTP_BUILDER_ERROR_INVALIDPACKET;
	}
	/* update the sequence number field (even if it is probably useless as this function is called just before sending the DHPart2 packet only)*/
	zrtpPacket->sequenceNumber = sequenceNumber;

	/* update hte sequence number in the packetString */
	*(zrtpPacket->packetString+2)= (uint8_t)((sequenceNumber>>8)&0x00FF);
	*(zrtpPacket->packetString+3)= (uint8_t)(sequenceNumber&0x00FF);


	/* update the CRC */
	uint32_t CRC = bzrtp_CRC32(zrtpPacket->packetString, zrtpPacket->messageLength+ZRTP_PACKET_HEADER_LENGTH);
	uint8_t *CRCbuffer = (zrtpPacket->packetString)+(zrtpPacket->messageLength)+ZRTP_PACKET_HEADER_LENGTH;
	*CRCbuffer = (uint8_t)((CRC>>24)&0xFF);
	CRCbuffer++;
	*CRCbuffer = (uint8_t)((CRC>>16)&0xFF);
	CRCbuffer++;
	*CRCbuffer = (uint8_t)((CRC>>8)&0xFF);
	CRCbuffer++;
	*CRCbuffer = (uint8_t)(CRC&0xFF);

	return 0;
}


/*** Local functions implementation ***/

uint8_t *messageTypeInttoString(uint32_t messageType) {

	switch(messageType) {
		case MSGTYPE_HELLO : 
			return (uint8_t *)"Hello   ";
			break;
		case MSGTYPE_HELLOACK : 
			return (uint8_t *)"HelloACK";
			break;
		case MSGTYPE_COMMIT : 
			return (uint8_t *)"Commit  ";
			break;
		case MSGTYPE_DHPART1 : 
			return (uint8_t *)"DHPart1 ";
			break;
		case MSGTYPE_DHPART2 : 
			return (uint8_t *)"DHPart2 ";
			break;
		case MSGTYPE_CONFIRM1 : 
			return (uint8_t *)"Confirm1";
			break;
		case MSGTYPE_CONFIRM2 : 
			return (uint8_t *)"Confirm2";
			break;
		case MSGTYPE_CONF2ACK : 
			return (uint8_t *)"Conf2ACK";
			break;
		case MSGTYPE_ERROR : 
			return (uint8_t *)"Error   ";
			break;
		case MSGTYPE_ERRORACK : 
			return (uint8_t *)"ErrorACK";
			break;
		case MSGTYPE_GOCLEAR : 
			return (uint8_t *)"GoClear ";
			break;
		case MSGTYPE_CLEARACK : 
			return (uint8_t *)"ClearACK";
			break;
		case MSGTYPE_SASRELAY : 
			return (uint8_t *)"SASrelay";
			break;
		case MSGTYPE_RELAYACK : 
			return (uint8_t *)"RelayACK";
			break;
		case MSGTYPE_PING : 
			return (uint8_t *)"Ping    ";
			break;
		case MSGTYPE_PINGACK : 
			return (uint8_t *)"PingACK ";
			break;
	}
	return NULL;
}

/*
 * @brief Map the 8 char string value message type to an int32_t
 *
 * @param[in] messageTypeString		an 8 bytes string matching a zrtp message type
 *
 * @return	a 32-bits unsigned integer mapping the message type
 */
int32_t messageTypeStringtoInt(uint8_t messageTypeString[8]) {
	if (memcmp(messageTypeString, "Hello   ", 8) == 0) {
		return MSGTYPE_HELLO;
	} else if (memcmp(messageTypeString, "HelloACK", 8) == 0) {
		return MSGTYPE_HELLOACK;
	} else if (memcmp(messageTypeString, "Commit  ", 8) == 0) {
		return MSGTYPE_COMMIT;
	} else if (memcmp(messageTypeString, "DHPart1 ", 8) == 0) {
		return MSGTYPE_DHPART1;
	} else if (memcmp(messageTypeString, "DHPart2 ", 8) == 0) {
		return MSGTYPE_DHPART2;
	} else if (memcmp(messageTypeString, "Confirm1", 8) == 0) {
		return MSGTYPE_CONFIRM1;
	} else if (memcmp(messageTypeString, "Confirm2", 8) == 0) {
		return MSGTYPE_CONFIRM2;
	} else if (memcmp(messageTypeString, "Conf2ACK", 8) == 0) {
		return MSGTYPE_CONF2ACK;
	} else if (memcmp(messageTypeString, "Error   ", 8) == 0) {
		return MSGTYPE_ERROR;
	} else if (memcmp(messageTypeString, "ErrorACK", 8) == 0) {
		return MSGTYPE_ERRORACK;
	} else if (memcmp(messageTypeString, "GoClear ", 8) == 0) {
		return MSGTYPE_GOCLEAR;
	} else if (memcmp(messageTypeString, "ClearACK", 8) == 0) {
		return MSGTYPE_CLEARACK;
	} else if (memcmp(messageTypeString, "SASrelay", 8) == 0) {
		return MSGTYPE_SASRELAY;
	} else if (memcmp(messageTypeString, "RelayACK", 8) == 0) {
		return MSGTYPE_RELAYACK;
	} else if (memcmp(messageTypeString, "Ping    ", 8) == 0) {
		return MSGTYPE_PING;
	} else if (memcmp(messageTypeString, "PingACK ", 8) == 0) {
		return MSGTYPE_PINGACK;
	} else {
		return MSGTYPE_INVALID;
	}
}

/*
 * @brief Write the message header(preambule, length, message type) into the given output buffer
 *
 * @param[out]	outputBuffer		Message starts at the begining of this buffer
 * @param[in]	messageLength		Message length in bytes! To be converted into 32bits words before being inserted in the message header
 * @param[in]	messageType			An 8 chars string for the message type (validity is not checked by this function)
 *
 */
void zrtpMessageSetHeader(uint8_t *outputBuffer, uint16_t messageLength, uint8_t messageType[8]) {
	/* insert the preambule */
	outputBuffer[0] = 0x50;
	outputBuffer[1] = 0x5a;

	/* then the length in 32 bits words (param is in bytes, so >> 2) */
	outputBuffer[2] = (uint8_t)((messageLength>>10)&0x00FF);
	outputBuffer[3] = (uint8_t)((messageLength>>2)&0x00FF);

	/* the message type */
	memcpy(outputBuffer+4, messageType, 8);
}

/*
 * Return the variable private value length in bytes according to given key agreement algorythm
 *
 * @param[in]	keyAgreementAlgo	The key agreement algo mapped to an integer as defined in cryptoWrapper.h
 *
 * @return		the private value length in bytes
 *
 */
uint16_t computeKeyAgreementPrivateValueLength(uint8_t keyAgreementAlgo) {

	uint16_t pvLength = 0;
	switch (keyAgreementAlgo) {
		case ZRTP_KEYAGREEMENT_DH3k	:
			pvLength = 384;
			break;
		case ZRTP_KEYAGREEMENT_DH2k :
			pvLength = 256;
			break;
		case ZRTP_KEYAGREEMENT_EC25	:
			pvLength = 64;
			break;
		case ZRTP_KEYAGREEMENT_EC38	:
			pvLength = 96;
			break;
		case ZRTP_KEYAGREEMENT_EC52 :
			pvLength = 132;
			break;
		default :
			pvLength = 0;
			break;
	}

	return pvLength;
}
