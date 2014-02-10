/**
 @file typedef.h

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
#ifndef TYPEDEF_H
#define TYPEDEF_H

/* aux secret may rarely be used define his maximum length in bytes */
#define MAX_AUX_SECRET_LENGTH	64
/* the context will store some of the sent or received packets */
#define PACKET_STORAGE_CAPACITY 3

#define HELLO_MESSAGE_STORE_ID 0
#define	COMMIT_MESSAGE_STORE_ID 1
#define DHPART2_MESSAGE_STORE_ID 2

/* role mapping */
#define INITIATOR	0
#define	RESPONDER	1

#include <stdint.h>
#include <stdio.h>
#include "cryptoWrapper.h"
#include "packetParser.h"

/**
 * @brief A set of cached secrets retrieved from the cache as defined
 */
typedef struct cachedSecrets_struct {
	uint8_t *rs1; /**< retained secret 1 */
	uint8_t rs1Length; /**< retained secret 1 length in bytes */
	uint8_t *rs2; /**< retained secret 2 */
	uint8_t rs2Length; /**< retained secret 2 length in bytes */
	uint8_t *auxsecret; /**< auxiliary secret */
	uint8_t auxsecretLength; /**< auxiliary secret length in bytes */
	uint8_t *pbxsecret; /**< PBX secret */
	uint8_t pbxsecretLength; /**< PBX secret length in bytes */
} cachedSecrets_t;

/**
 * @brief The hash of cached secret truncated to the 64 leftmost bits
 */
typedef struct cachedSecretsHash_struct {
	uint8_t rs1ID[8]; /**< retained secret 1 Hash */
	uint8_t rs2ID[8]; /**< retained secret 2 Hash */
	uint8_t auxsecretID[8]; /**< auxiliary secret Hash */
	uint8_t pbxsecretID[8]; /**< pbx secret Hash */
} cachedSecretsHash_t;

/**
 * @brief All the callback functions provided by the client needed by the ZRTP engine
 */
typedef struct zrtpCallbacks_struct {
	int (* bzrtp_readCache)(uint8_t *output, uint16_t size); /**< Cache related function : read size bytes from cache, shall return the number of bytes read */
	int (* bzrtp_writeCache)(uint8_t *input, uint16_t size); /**< Cache related function : write size bytes to cache */
	int (* bzrtp_setCachePosition)(long position); /**< Cache related function : set cache position in cache file, rewind when passing 0 */
	int (* bzrtp_getCachePosition)(long *position); /**< Cache related function : get the current cache position in cache file */
} zrtpCallbacks_t;

/**
 * @struct bzrtpContext_t structure of the ZRTP engine
 * Store current state, timers, HMAC and encryption keys
*/
typedef struct bzrtpContext_struct {
	/* contexts */
	bzrtpRNGContext_t *RNGContext; /**< context for random number generation */
	bzrtpDHMContext_t *DHMContext; /**< context for the Diffie-Hellman-Merkle operations */

	/* role : can be INITIATOR or RESPONDER, is set to INITIATOR at creation, may switch to responder later */ 
	uint8_t role;
	/* callbacks */
	zrtpCallbacks_t zrtpCallbacks; /**< structure holding all the pointers to callbacks functions needed by the ZRTP engine. Functions are set by client using the bzrtp_setCallback function */

	/* Hash chains, self is generated at context init */
	uint8_t selfH[4][32]; /**< Store self 256 bits Hash images H0-H3 used to generate messages MAC */
	uint8_t peerH[4][32]; /**< Store peer 256 bits Hash images H0-H3 used to check messages authenticity */

	/* List of available algorithms, initialised with algo implemented in cryptoWrapper but can be then be modified according to user settings */
	uint8_t hc; /**< hash count -zrtpPacket set to 0 means we support only HMAC-SHA256 (4 bits) */
	uint8_t supportedHash[7]; /**< list of supported hash algorithms mapped to uint8_t */
	uint8_t cc; /**< cipher count - set to 0 means we support only AES128-CFB128 (4 bits) */
	uint8_t supportedCipher[7]; /**< list of supported cipher algorithms mapped to uint8_t */
	uint8_t ac; /**< auth tag count - set to 0 mean we support only HMAC-SHA1-32 (4 bits) */
	uint8_t supportedAuthTag[7]; /**< list of supported SRTP authentication tag algorithms mapped to uint8_t */
	uint8_t kc; /**< key agreement count - set to 0 means we support only Diffie-Hellman-Merkle 3072 (4 bits) */
	uint8_t supportedKeyAgreement[7]; /**< list of supported key agreement algorithms mapped to uint8_t */
	uint8_t sc; /**< sas count - set to 0 means we support only base32 (4 bits) */
	uint8_t supportedSas[7]; /**< list of supported Sas representations mapped to uint8_t */
	
	/* algorithm agreed after Hello message exchange(use mapping define in cryptoWrapper.h) and the function pointer to use them */
	uint8_t hashAlgo; /**< hash algorithm agreed on after Hello packet exchange, stored using integer mapping defined in cryptoWrapper.h */
	uint8_t hashLength; /**< the length in bytes of a hash generated with the agreed hash algo */
	uint8_t cipherAlgo; /**< cipher algorithm agreed on after Hello packet exchange, stored using integer mapping defined in cryptoWrapper.h */
	uint8_t cipherKeyLength; /**< the length in bytes of the key needed by the agreed cipher block algo */
	uint8_t authTagAlgo; /**< srtp authentication tag algorithm agreed on after Hello packet exchange, stored using integer mapping defined in cryptoWrapper.h */
	uint8_t keyAgreementAlgo; /**< key agreement algorithm agreed on after Hello packet exchange, stored using integer mapping defined in cryptoWrapper.h */
	uint16_t keyAgreementLength; /**< the length in byte of the secret generated by the agreed key exchange algo */
	uint8_t sasAlgo; /**< sas rendering algorithm agreed on after Hello packet exchange, stored using integer mapping defined in cryptoWrapper.h */

	void (*hmacFunction)(const uint8_t *key, uint8_t keyLength, const uint8_t *input, uint32_t inputLength, uint8_t hmacLength, uint8_t *output);
	void (*hashFunction)(const uint8_t *input, uint32_t inputLength, uint8_t hashLength, uint8_t *output);
	void (*cipherEncryptionFunction)(const uint8_t *key, const uint8_t *IV, const uint8_t *input, uint16_t inputLength, uint8_t *output);
	void (*cipherDecryptionFunction)(const uint8_t *key, const uint8_t *IV, const uint8_t *input, uint16_t inputLength, uint8_t *output);

	void (*sasFunction)(uint32_t sas, char output[4]);

	/* ZIDs and cache */
	uint8_t selfZID[12]; /**< The ZRTP Identifier of this ZRTP end point - a random if running cache less */
	uint8_t peerZID[12]; /**< The ZRTP Identifier of the peer ZRTP end point - given by the Hello packet */
	cachedSecrets_t cachedSecret; /**< the local cached secrets */
	cachedSecretsHash_t initiatorCachedSecretHash; /**< The hash of cached secret from initiator side, computed as described in rfc section 4.3.1 */
	cachedSecretsHash_t responderCachedSecretHash; /**< The hash of cached secret from responder side, computed as described in rfc section 4.3.1 */

	/* sequence number: self and peer */
	uint16_t selfSequenceNumber; /**< Sequence number of the next packet to be sent */
	uint16_t peerSequenceNumber; /**< Sequence number of the last valid received packet */

	/* packet storage : shall store some sent and received packets */
	bzrtpPacket_t *selfPackets[PACKET_STORAGE_CAPACITY]; /**< Hello and DHPart packet locally generated */
	bzrtpPacket_t *peerPackets[PACKET_STORAGE_CAPACITY]; /**< Hello and DHPart packet received from peer */

	/* keys */
	uint8_t *s0; /**< the s0 as describred rfc section 4.4 - have a length of hashLength */
	uint8_t *KDFContext; /**< defined in rfc section 4.4 */
	uint16_t KDFContextLength; /**< lenght of the KDF context, is 24 + output length of the selected hash algo */
	uint8_t *ZRTPSess; /**< ZRTP session key as described in rfc section 4.5.2 */
	uint8_t	ZRTPSessLength; /**< length of ZRTP session key depends on agreed hash algorithm */
	uint8_t *mackeyi; /**< the initiator mackey as defined in rfc section 4.5.3 - have a length of hashLength */
	uint8_t *mackeyr; /**< the responder mackey as defined in rfc section 4.5.3 - have a length of hashLength*/
	uint8_t *zrtpkeyi; /**< the initiator mackey as defined in rfc section 4.5.3 - have a length of cipherKeyLength */
	uint8_t *zrtpkeyr; /**< the responder mackey as defined in rfc section 4.5.3 - have a length of cipherKeyLength*/

	bzrtpSrtpSecrets_t srtpSecrets; /**< the secrets keys and salt needed by SRTP */
	


	
} bzrtpContext_t;

#endif /* ifndef TYPEDEF_H */
