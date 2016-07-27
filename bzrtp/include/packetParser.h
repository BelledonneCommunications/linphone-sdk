/**
 @file packetParser.h

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
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef PACKETPARSER_H
#define PACKETPARSER_H

#include <stdint.h>
#include "bzrtp/bzrtp.h"

/* header of ZRTP packet is 12 bytes : Preambule/Sequence Number + ZRTP Magic Cookie +  SSRC */
#define ZRTP_PACKET_HEADER_LENGTH	12
#define ZRTP_PACKET_CRC_LENGTH		4
#define ZRTP_PACKET_OVERHEAD		16

#define		BZRTP_PARSER_ERROR_INVALIDCRC  			0xa001
#define		BZRTP_PARSER_ERROR_INVALIDPACKET		0xa002
#define		BZRTP_PARSER_ERROR_OUTOFORDER			0xa004
#define		BZRTP_PARSER_ERROR_INVALIDMESSAGE		0xa008
#define		BZRTP_PARSER_ERROR_INVALIDCONTEXT		0xa010
#define		BZRTP_PARSER_ERROR_UNMATCHINGCONFIRMMAC	0xa020
#define		BZRTP_PARSER_ERROR_UNMATCHINGSSRC		0xa040
#define		BZRTP_PARSER_ERROR_UNMATCHINGHASHCHAIN	0xa080
#define		BZRTP_PARSER_ERROR_UNMATCHINGMAC		0xa100
#define		BZRTP_PARSER_ERROR_UNEXPECTEDMESSAGE	0xa200
#define		BZRTP_PARSER_ERROR_UNMATCHINGHVI		0xa400

#define		BZRTP_BUILDER_ERROR_INVALIDPACKET		0x5001
#define		BZRTP_BUILDER_ERROR_INVALIDMESSAGE		0x5002
#define		BZRTP_BUILDER_ERROR_INVALIDMESSAGETYPE	0x5004
#define		BZRTP_BUILDER_ERROR_UNKNOWN				0x5008
#define		BZRTP_BUILDER_ERROR_INVALIDCONTEXT		0x5010

#define		BZRTP_CREATE_ERROR_INVALIDMESSAGETYPE			0x0a01
#define		BZRTP_CREATE_ERROR_UNABLETOCREATECRYPTOCONTEXT	0x0a02
#define		BZRTP_CREATE_ERROR_INVALIDCONTEXT				0x0a04

/* map all message type to an uint8_t value */
#define		MSGTYPE_INVALID		0x00
#define		MSGTYPE_HELLO		0x01
#define		MSGTYPE_HELLOACK	0x02
#define		MSGTYPE_COMMIT		0x03
#define		MSGTYPE_DHPART1		0x04
#define		MSGTYPE_DHPART2		0x05
#define		MSGTYPE_CONFIRM1	0x06
#define		MSGTYPE_CONFIRM2	0x07
#define		MSGTYPE_CONF2ACK	0x08
#define		MSGTYPE_ERROR		0x10
#define		MSGTYPE_ERRORACK	0x11
#define		MSGTYPE_GOCLEAR		0x12
#define		MSGTYPE_CLEARACK	0x13
#define		MSGTYPE_SASRELAY	0x14
#define		MSGTYPE_RELAYACK	0x15
#define		MSGTYPE_PING		0x16
#define		MSGTYPE_PINGACK		0x17

/**
 * @brief Store all zrtpPacket informations
 * according to type a specific structure type is mapped to the void * data pointer
 */
typedef struct bzrtpPacket_struct {
	uint16_t sequenceNumber; /**< set by packet parser to enable caller to retrieve the packet sequence number. This field is not used buy the packet creator, sequence number is given as a parameter when converting the message to a packet string. Used only when parsing a string into a packet struct */
	uint32_t sourceIdentifier; /**< the SSRC of current RTP stream */
	uint8_t  messageType; /**< the ZRTP message type mapped from strings to hard defined byte */
	uint16_t messageLength; /**< the ZRTP message length in bytes - the message length indicated in the message itself is in 32 bits words. Is not the packet length(do not include packet header and CRC) */
	void *messageData; /**< a pointer to the structure containing all the message field according to message type */
	uint8_t *packetString; /**< used to stored the string version of the packet build from the message data or keep a string copy of received packets */
} bzrtpPacket_t;

/**
 * Structure definition for all zrtp message type according to rfc section 5.2 to 5.16
 *
 */

/**
 * @brief Hello Message rfc 5.2
 */
typedef struct bzrtpHelloMessage_struct {
	uint8_t version[4]; /**< a string defining the current version, shall be 1.10 */
	uint8_t clientIdentifier[16]; /**< a string identifing the vendor and release of ZRTP software */
	uint8_t H3[32]; /**< the hash image H3 (256 bits) */
	uint8_t ZID[12]; /**< unique identifier for ZRTP endpoint (96 bits) */
	uint8_t	S; /**< The signature-capable flag. If signatures are not supported, the (S) flag MUST be set to zero (1 bit) */
	uint8_t M; /**< The MiTM flag (M) is a Boolean that is set to true if and only if this Hello message is sent from a device, usually a PBX, that has the capability to send an SASrelay message (1 bit) **/
	uint8_t P; /**< The Passive flag (P) is a Boolean normally set to false, and is set to true if and only if this Hello message is sent from a device that is configured to never send a Commit message (Section 5.4).  This would mean it cannot initiate secure sessions, but may act as a responder. (1 bit) */
	uint8_t hc; /**< hash count -zrtpPacket set to 0 means we support only HMAC-SHA256 (4 bits) */
	uint8_t supportedHash[7]; /**< list of supported hash algorithms mapped to uint8_t */
	uint8_t cc; /**< cipher count - set to 0 means we support only AES128-CFB128 (4 bits) */
	uint8_t supportedCipher[7]; /**< list of supported cipher algorithms mapped to uint8_t */
	uint8_t ac; /**< auth tag count - set to 0 mean we support only HMAC-SHA1-32 (4 bits) */
	uint8_t supportedAuthTag[7]; /**< list of supported SRTP authentication tag algorithms mapped to uint8_t */
	uint8_t kc; /**< key agreement count - set to 0 means we support only Diffie-Hellman-Merkle 3072 (4 bits) */
	uint8_t supportedKeyAgreement[7]; /**< list of supported key agreement algorithms mapped to uint8_t */
	uint8_t sc; /**< sas count - set to 0 means we support only base32 (4 bits) */
	uint8_t supportedSas[7]; /**< list of supported Sas representations (4 chars string) */
	uint8_t MAC[8]; /**< HMAC over the whole message, keyed by the hash image H2 (64 bits)*/
} bzrtpHelloMessage_t;

/**
 * @brief Hello ACK Message rfc 5.3
 * This message contains no data but only a length and message type which are stored in the bzrtpPacket_t structure
 * There the no need to define a structure type for this packet
 */

/**
 *
 * @brief Commit Message rfc 5.4
 * This message can be of 3 different types: DHM, PreShared and Multistream, some field of it may be used only by certain type of message
 * It is generated by the initiator (see section 4.2 for commit contention)
 */
typedef struct bzrtpCommitMessage_struct {
	uint8_t H2[32]; /**< the hash image H2 (256 bits) */
	uint8_t ZID[12]; /**< initiator's unique identifier for ZRTP endpoint (96 bits) */
	uint8_t hashAlgo; /**< the hash algorithm identifier rfc section 5.1.2 mapped to an integer */
	uint8_t cipherAlgo; /**< the cipher algorithm identifier rfc section 5.1.3 mapped to an integer */
	uint8_t authTagAlgo; /**< the auth tag algorithm identifier rfc section 5.1.4 mapped to an integer */
	uint8_t keyAgreementAlgo; /**< the key agreement algorithm identifier rfc section 5.1.5. It can either be a key exchange algorithm or the commit packet type in case of preShared or multistream commit message mapped to an integer */
	uint8_t sasAlgo; /**< the sas rendering algorithm identifier rfc section 5.1.6 mapped to an integer */
	uint8_t hvi[32]; /**< only for DH commit : a hash of initiator's DHPart2 and responder's Hello message rfc section 4.4.1.1 */
	uint8_t nonce[16]; /**< only for preShared or Multistream modes : a 128 bits random number generated by the initiator */
	uint8_t keyID[8]; /**< only for preShared mode : the preshared key identifier */
	uint8_t MAC[8]; /**< HMAC over the whole message, keyed by the hash image H1 (64 bits)*/
} bzrtpCommitMessage_t;


/**
 *
 * @brief DHPart Message rfc 5.5 and rfc 5.6
 * DHPart1 and DHPart2 message have the same structure
 * DHPart1 is generated by the responder, and DHPart2 by the initiator
 */
typedef struct bzrtpDHPartMessage_struct {
	uint8_t H1[32]; /**< the hash image H1 (256 bits) */
	uint8_t rs1ID[8]; /**< hash of the retained secret 1 (64 bits) */
	uint8_t rs2ID[8]; /**< hash of the retained secret 2 (64 bits) */
	uint8_t auxsecretID[8]; /**< hash of the auxiliary shared secret (64 bits) */
	uint8_t pbxsecretID[8]; /**< hash of the trusted MiTM PBX shared secret pbxsecret, defined in section 7.3.1 (64 bits) */
	uint8_t *pv; /* Key exchange public value (length depends on key agreement type) */
	uint8_t MAC[8]; /**< HMAC over the whole message, keyed by the hash image H1 (64 bits)*/
} bzrtpDHPartMessage_t;

/**
 *
 * @brief Confirm Message rfc 5.7
 * Confirm1 and Confirm2 messages have the same structure
 * Confirm1 is generated by the responder and Confirm2 by the initiator
 * Part of the message is encrypted using the negotiated block cipher for media encryption. Keys ares zrtpkeyr for responder and zrtpkeyi for initiator
 */
typedef struct bzrtpConfirmMessage_struct {
	uint8_t confirm_mac[8]; /**< a MAC computed over the encrypted part of the message (64 bits) */
	uint8_t CFBIV[16]; /**< The CFB Initialization Vector is a 128-bit random nonce (128 bits) */
	uint8_t H0[32]; /**< the hash image H0 - Encrypted - (256 bits) */
	uint16_t sig_len; /**< The SAS signature length.  If no SAS signature (described in Section 7.2) is present, all bits are set to zero.  The signature length is in words and includes the signature type block.  If the calculated signature octet count is not a multiple of 4, zeros are added to pad it out to a word boundary.  If no signature is present, the overall length of the Confirm1 or Confirm2 message will be set to 19 words - Encrypted - (9 bits) */
	uint8_t E; /**< The PBX Enrollment flag (E) is a Boolean bit defined in Section 7.3.1 - Encrypted - (1 bit) */
	uint8_t V; /**< The SAS Verified flag (V) is a Boolean bit defined in Section 7.1. - Encrypted - (1 bit) */
	uint8_t A; /**< The Allow Clear flag (A) is a Boolean bit defined in Section 4.7.2 - Encrypted - (1 bit) */
	uint8_t D; /**< The Disclosure Flag (D) is a Boolean bit defined in Section 11. - Encrypted - (1 bit) */
	uint32_t cacheExpirationInterval; /**< The cache expiration interval is defined in Section 4.9 - Encrypted - (32 bits) */
	uint8_t signatureBlockType[4]; /**< Optionnal signature type : "PGP " or "X509" string - Encrypted - (32 bits) */
	uint8_t *signatureBlock; /**< Optionnal signature block as decribded in section 7.2 - Encrypted - (variable length) */
	
} bzrtpConfirmMessage_t;

/**
 * @brief Conf2 ACK Message rfc 5.8
 * This message contains no data but only a length and message type which are stored in the bzrtpPacket_t structure
 * There the no need to define a structure type for this packet
 */

/**
 * @brief Error Message rfc section 5.9
 * The Error message is sent to terminate an in-process ZRTP key agreement exchange due to an error.
 * There is no need to define a structure for this packet as it contains length and message type which are stored
 * in the bzrtpPacket_t structure and a 32 bits integer error code only
 */

/**
 * @brief Error ACK Message rfc 5.10
 * This message contains no data but only a length and message type which are stored in the bzrtpPacket_t structure
 * There the no need to define a structure type for this packet
 */

/**
 * @brief GoClear Message rfc 5.11
 * Support for the GoClear message is OPTIONAL in the protocol, and it is sent to switch from SRTP to RTP.
 */
typedef struct bzrtpGoClearMessage_struct {
	uint8_t clear_mac[8]; /**< The clear_mac is used to authenticate the GoClear message so that bogus GoClear messages introduced by an attacker can be detected and discarded. (64 bits) */
} bzrtpGoClearMessage_t;

/**
 *
 * @brief Clear ACK Message rfc 5.12
 * This message contains no data but only a length and message type which are stored in the bzrtpPacket_t structure
 * There the no need to define a structure type for this packet
 */

/**
 * @brief SASRelay Message rfc 5.13
 * The SASrelay message is sent by a trusted MiTM, most often a PBX.  It is not sent as a response to a packet, but is sent as a self-initiated packet by the trusted MiTM (Section 7.3).  It can only be sent after the rest of the ZRTP key negotiations have completed, after the Confirm messages and their ACKs.  It can only be sent after the trusted MiTM has finished key negotiations with the other party, because it is the other party's SAS that is being relayed.  It is sent with retry logic until a RelayACK message (Section 5.14) is received or the retry schedule has been exhausted. Part of the message is encrypted using the negotiated block cipher for media encryption.
 * Depending on whether the trusted MiTM had taken the role of the initiator or the responder during the ZRTP key negotiation, the
 * SASrelay message is encrypted with zrtpkeyi or zrtpkeyr.
 */
typedef struct bzrtpSASRelayMessage_struct {
	uint8_t MAC[8]; /**< a MAC computed over the encrypted part of the message (64 bits) */
	uint8_t CFBIV[16]; /**< The CFB Initialization Vector is a 128-bit random nonce (128 bits) */
	uint16_t sig_len; /**< The SAS signature length.  The trusted MiTM MAY compute a digital signature on the SAS hash, as described in Section 7.2, using a persistent signing key owned by the trusted MiTM.  If no SAS signature is present, all bits are set to zero.  The signature length is in words and includes the signature type block.  If the calculated signature octet count is not a multiple of 4, zeros are added to pad it out to a word boundary.  If no signature block is present, the overall length of the SASrelay message will be set to 19 words.*/
	uint8_t V; /**< The SAS Verified flag (V) is a Boolean bit defined in Section 7.1. - Encrypted - (1 bit) */
	uint8_t A; /**< The Allow Clear flag (A) is a Boolean bit defined in Section 4.7.2 - Encrypted - (1 bit) */
	uint8_t D; /**< The Disclosure Flag (D) is a Boolean bit defined in Section 11. - Encrypted - (1 bit) */
	uint8_t renderingScheme[4]; /**< the SAS rendering scheme for the relayed sashash, which will be the same rendering scheme used by the other party on the other side of the trusted MiTM. - Encrypted - (32 bits) */ 
	uint8_t relayedSasHash[32];	/**< the sashash relayed from the other party.  The first 32-bit word of the sashash contains the sasvalue, which may be rendered to the user using the specified SAS rendering scheme.  If this SASrelay message is being sent to a ZRTP client that does not trust this MiTM, the sashash will be ignored by the recipient and should be set to zeros by the PBX. - Encrypted - (256 bits) */
	uint8_t signatureBlockType; /**< Optionnal signature type : "PGP " or "X509" string - Encrypted - (32 bits) */
	uint8_t *signatureBlock; /**< Optionnal signature block as decribded in section 7.2 - Encrypted - (variable length) */

} bzrtpSASRelayMessage_t;

/**
 * @brief Relay ACK Message rfc 5.14
 * This message contains no data but only a length and message type which are stored in the bzrtpPacket_t structure
 * There the no need to define a structure type for this packet
 */

/**
 * @brief Ping Message
 * The Ping and PingACK messages are unrelated to the rest of the ZRTP protocol.  No ZRTP endpoint is required to generate a Ping message, but every ZRTP endpoint MUST respond to a Ping message with a PingACK message.
 */
typedef struct bzrtpPingMessage_struct {
	uint8_t version[4]; /**< a string defining the current version, shall be 1.10 (32 bits) */
	uint8_t endpointHash[8]; /**< see section 5.16 for the endpointHash definition (64 bits) */
} bzrtpPingMessage_t;

/**
 *
 * @brief PingAck Message
 * The Ping and PingACK messages are unrelated to the rest of the ZRTP protocol.  No ZRTP endpoint is required to generate a Ping message, but every ZRTP endpoint MUST respond to a Ping message with a PingACK message.
 */
typedef struct bzrtpPingAckMessage_struct {
	uint8_t version[4]; /**< a string defining the current version, shall be 1.10 (32 bits) */
	uint8_t endpointHash[8]; /**< see section 5.16 for the endpointHash definition (64 bits) */
	uint8_t endpointHashReceived[8]; /**< the endpoint hash received in the ping Message we're acknowledging (64 bits) */
	uint32_t SSRC; /**< the SSRC received in the ping packet we're acknowledging (32 bits) */
} bzrtpPingAckMessage_t;


/** 
 * @brief Parse a string which shall be a valid ZRTP packet
 * Check validity and allocate the bzrtpPacket structure but do not parse the message except for type and length.
 * messageData structure field is not allocated by this function (use then bzrtp_packetParse for that).
 * The packet check and actual message parsing are split in two functions to avoid useless parsing when message is
 * to be discarded as the check will give message type (in case of message repetition for example)
 *
 * @param[in]	input						The string buffer storing the complete ZRTP packet
 * @param[in]	inputLength					Input length in bytes
 * @param[in]	lastValidSequenceNumber		If the sequence number of this packet is smaller than this param, packet will be discarded
 *											and an error code returned
 * @param[out]	exitCode					0 on success, error code otherwise
 *
 * @return		The create bzrtpPacket structure(to be freed using bzrtp_freeZrtpPacket). NULL on error
 */
bzrtpPacket_t *bzrtp_packetCheck(const uint8_t * input, uint16_t inputLength, uint16_t lastValidSequenceNumber, int *exitCode);


/**
 * @brief Parse the packet to extract the message and allocate the matching message structure if needed
 *
 * @param[in]		zrtpContext			The current ZRTP context, some parameters(key agreement algorithm) may be needed to parse packet.
 * @param[in]		zrtpChannelContext	The channel context this packet is intended to(channel context and packet must match peer SSRC).
 * @param[in]		input				The string buffer storing the complete ZRTP packet
 * @param[in]		inputLength			Input length in bytes
 * @param[in]		zrtpPacket			The zrtpPacket structure allocated by previous call to bzrtpPacketCheck
 *
 * @return 	0 on sucess, error code otherwise
 */
int bzrtp_packetParser(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, const uint8_t * input, uint16_t inputLength, bzrtpPacket_t *zrtpPacket); 


/**
 * @brief Create an empty packet and allocate the messageData according to requested packetType
 *
 * @param[in]		zrtpContext			The current ZRTP context, some data (H chain or others, may be needed to create messages)
 * @param[in]		zrtpChannelContext	The channel context this packet is intended to
 * @param[in]		messageType			The 32bit integer mapped to the message type to be created
 * @param[out]		exitCode			0 on success, error code otherwise
 *
 * @return		An empty packet initialised to get data for the requested paquet tyep. NULL on error
 */ 
bzrtpPacket_t *bzrtp_createZrtpPacket(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, uint32_t messageType, int *exitCode);


/**
 * @brief Create a ZRTP packet string from the ZRTP packet values present in the structure
 * messageType, messageData and sourceIdentifier in zrtpPacket must have been correctly set before calling this function
 *
 * @param[in]		zrtpContext				A zrtp context where to find H0-H3 to compute MAC requested by some paquets or encryption's key for commit/SASRelay packet
 * @param[in]		zrtpChannelContext		The channel context this packet is intended to
 * @param[in/out]	zrtpPacket				The zrtpPacket structure containing the message Data structure, output is stored in ->packetString
 * @param[in]		sequenceNumber			Sequence number of this packet
 *
 * @return			0 on success, error code otherwise
 *
 */
int bzrtp_packetBuild(bzrtpContext_t *zrtpContext,  bzrtpChannelContext_t *zrtpChannelContext, bzrtpPacket_t *zrtpPacket, uint16_t sequenceNumber);


/**
 * @brief Deallocate zrtp Packet
 *
 * @param[in] zrtpPacket	The packet to be freed
 *
 */
void bzrtp_freeZrtpPacket(bzrtpPacket_t *zrtpPacket);

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
int bzrtp_packetUpdateSequenceNumber(bzrtpPacket_t *zrtpPacket, uint16_t sequenceNumber);
#endif /* PACKETPARSER_H */
