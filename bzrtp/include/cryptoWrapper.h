/**
 @file cryptoWrapper.h

 @brief This file contains all functions prototypes to the needed cryptographic operations:
 - SHA256
 - HMAC-SHA256
 - AES128-CFB128
 - Diffie-Hellman-Merkle 2048 and 3072 bits
 - Random number generator

 This functions are implemented by an external lib(polarssl but it can be other) and the functions
 wrapper depends on the external lib API.

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
#ifndef CRYPTOWRAPPER_H
#define CRYPTOWRAPPER_H

#include <stdint.h>

/* define different types of crypto functions */
#define ZRTP_HASH_TYPE				0x01
#define ZRTP_CIPHERBLOCK_TYPE 		0x02
#define ZRTP_AUTHTAG_TYPE			0x04
#define ZRTP_KEYAGREEMENT_TYPE		0x08
#define ZRTP_SAS_TYPE				0x10

/* map the differents algorithm (some may not be available) to integer */
#define ZRTP_UNSET_ALGO			0x00

#define	ZRTP_HASH_S256			0x11
#define	ZRTP_HASH_S384			0x12
#define	ZRTP_HASH_N256			0x13
#define	ZRTP_HASH_N384			0x14

#define ZRTP_CIPHER_AES1		0x21
#define ZRTP_CIPHER_AES2		0x22
#define ZRTP_CIPHER_AES3		0x23
#define ZRTP_CIPHER_2FS1		0x24
#define ZRTP_CIPHER_2FS2		0x25
#define ZRTP_CIPHER_2FS3		0x26

#define ZRTP_AUTHTAG_HS32		0x31
#define ZRTP_AUTHTAG_HS80		0x32
#define ZRTP_AUTHTAG_SK32		0x33
#define ZRTP_AUTHTAG_SK64		0x34

/* WARNING : it is very important to keep the key agreement defined in that order
 * as it is used to easily sort them from faster(DH2k) to slower(EC52)
 */
#define ZRTP_KEYAGREEMENT_DH2k	0x41
#define ZRTP_KEYAGREEMENT_EC25	0x42
#define ZRTP_KEYAGREEMENT_DH3k	0x43
#define ZRTP_KEYAGREEMENT_EC38	0x44
#define ZRTP_KEYAGREEMENT_EC52	0x45

#define ZRTP_KEYAGREEMENT_Prsh	0x46
#define ZRTP_KEYAGREEMENT_Mult	0x47

#define ZRTP_SAS_B32			0x51
#define ZRTP_SAS_B256			0x52

/**
 * @brief Get the available crypto functions
 * - Hash function
 * - Cipher Block
 * - Auth Tag (this depends on SRTP implementation, thus returns only mandatory types: "HS32" and "HS80" HMAC-SHA1)
 * - Key agreement
 * - SAS
 *
 * @param[in]	algoType		mapped to defines, must be in [ZRTP_HASH_TYPE, ZRTP_CIPHERBLOCK_TYPE, ZRTP_AUTHTAG_TYPE, ZRTP_KEYAGREEMENT_TYPE or ZRTP_SAS_TYPE]
 * @param[out]	availableTypes	mapped to uint8_t value of the 4 char strings giving the available types as string according to rfc section 5.1.2 to 5.1.6
 *
 * @return		number of available types, 0 on error
 */
uint8_t bzrtpCrypto_getAvailableCryptoTypes(uint8_t algoType, uint8_t availableTypes[7]);

/**
 *
 * @brief The context structure to the Random Number Generator
 * actually just holds a pointer to the crypto module implementation context
 *
 */
typedef struct bzrtpRNGContext_struct {
	void *cryptoModuleData; /**< a context needed by the actual implementation */
} bzrtpRNGContext_t;

/**
 * 
 * @brief Initialise the random number generator
 *
 * @param[in] 	entropyString			Any string providing more entropy to the RNG
 * @param[in] 	entropyStringLength	Length of previous parameter
 * @return		The context initialised. NULL on error
 *
 */
bzrtpRNGContext_t *bzrtpCrypto_startRNG(const uint8_t *entropyString, uint16_t entropyStringLength);

/**
 *
 * @brief Generate a random number
 * @param[in]	context			The RNG context
 * @param[out]	output			The random generated
 * @param[in]	outputLength	The length(in bytes) of random number to be generated
 * @return		0 on success.
 *
 */
int bzrtpCrypto_getRandom(bzrtpRNGContext_t *context, uint8_t *output, int16_t outputLength);


/**
 *
 * @brief Destroy the RNG context
 *
 * @param[in]	context	The context to be destroyed
 * @return		0 on success.	
 *
 */
int bzrtpCrypto_destroyRNG(bzrtpRNGContext_t *context);

/**
 * @brief HMAC-SHA256 wrapper
 * @param[in] 	key			HMAC secret key
 * @param[in] 	keyLength	HMAC key length in bytes
 * @param[in]	input 		Input data buffer
 * @param[in]   inputLength	Input data length in bytes
 * @param[in]	hmacLength	Lenght of output required in bytes, HMAC output is truncated to the hmacLenght left bytes. 32 bytes maximum
 * @param[out]	output		Output data buffer.
 *
 */
void bzrtpCrypto_hmacSha256(const uint8_t *key,
		uint8_t keyLength,
		const uint8_t *input,
		uint32_t inputLength,
		uint8_t hmacLength,
		uint8_t *output);

/**
 * @brief SHA256 wrapper
 * @param[in]	input 		Input data buffer
 * @param[in]   inputLength	Input data length in bytes
 * @param[in]	hmacLength	Lenght of output required in bytes, SHA256 output is truncated to the hashLenght left bytes. 32 bytes maximum
 * @param[out]	output		Output data buffer.
 *
 */
void bzrtpCrypto_sha256(const uint8_t *input,
		uint32_t inputLength,
		uint8_t hashLength,
		uint8_t *output);

/**
 * @brief HMAC-SHA1 wrapper
 * @param[in] 	key			HMAC secret key
 * @param[in] 	keyLength	HMAC key length
 * @param[in]	input 		Input data buffer
 * @param[in]   inputLength	Input data length
 * @param[in]	hmacLength	Lenght of output required in bytes, HMAC output is truncated to the hmacLenght left bytes. 32 bytes maximum
 * @param[out]	output		Output data buffer
 *
 */
void bzrtpCrypto_hmacSha1(const uint8_t *key,
		uint8_t keyLength,
		const uint8_t *input,
		uint32_t inputLength,
		uint8_t hmacLength,
		uint8_t *output);

/**
 * @brief Wrapper for AES-128 in CFB128 mode encryption
 * Both key and IV must be 16 bytes long
 *
 * @param[in]	key			encryption key, 128 bits long
 * @param[in]	IV			Initialisation vector, 128 bits long, is not modified by this function.
 * @param[in]	input		Input data buffer
 * @param[in]	inputLength	Input data length
 * @param[out]	output		Output data buffer
 *
 */
void bzrtpCrypto_aes128CfbEncrypt(const uint8_t *key,
		const uint8_t *IV,
		const uint8_t *input,
		uint16_t inputLength,
		uint8_t *output);

/**
 * @brief Wrapper for AES-128 in CFB128 mode decryption
 * Both key and IV must be 16 bytes long
 *
 * @param[in]	key			decryption key, 128 bits long
 * @param[in]	IV			Initialisation vector, 128 bits long, is not modified by this function.
 * @param[in]	input		Input data buffer
 * @param[in]	inputLength	Input data length
 * @param[out]	output		Output data buffer
 *
 */
void bzrtpCrypto_aes128CfbDecrypt(const uint8_t *key,
		const uint8_t *IV,
		const uint8_t *input,
		uint16_t inputLength,
		uint8_t *output);

/**
 * @brief Context for the Diffie-Hellman-Merkle key exchange
 *	ZRTP specifies the use of RFC3526 values for G and P so we do not need to store them in this context
 */
typedef struct bzrtpDHMContext_struct {
	uint8_t algo; /**< Algorithm used for the key exchange mapped to an int */
	uint16_t primeLength; /**< Prime number length in bytes(256 or 384)*/
	uint8_t *secret; /**< the random secret (X), this field may not be used if the crypto module implementation already store this value in his context */
	uint8_t secretLength; /**< in bytes. Shall be twice the AES key length used (rfc 5.1.5) */
	uint8_t *key; /**< the key exchanged (G^Y)^X mod P */
	uint8_t *self; /**< this side of the public exchange G^X mod P */
	uint8_t *peer; /**< the other side of the public exchange G^Y mod P */
	void *cryptoModuleData; /**< a context needed by the crypto implementation */
}bzrtpDHMContext_t;

/**
 *
 * @brief Create a context for the DHM key exchange
 * 	This function will also instantiate the context needed by the actual implementation of the crypto module
 *
 * @param[in] DHMAlgo		The algorithm type(ZRTP_KEYAGREEMENT_DH2k or ZRTP_KEYAGREEMENT_DH3k)
 * @param[in] secretLength	The length in byte of the random secret(X). Shall be twice the AES key length used(rfc 5.1.5)
 *
 * @return The initialised context for the DHM calculation(must then be freed calling the destroyDHMContext function), NULL on error
 *
 */
bzrtpDHMContext_t *bzrtpCrypto_CreateDHMContext(uint8_t DHMAlgo, uint8_t secretLength);

/**
 *
 * @brief Generate the private secret X and compute the public value G^X mod P
 * G, P and X length have been set by previous call to DHM_CreateDHMContext
 *
 * @param[in/out] 	context		DHM context, will store the public value in ->self after this call
 * @param[in] 		rngFunction	pointer to a random number generator used to create the secret X
 * @param[in]		rngContext	pointer to the rng context if neeeded
 *
 */
void bzrtpCrypto_DHMCreatePublic(bzrtpDHMContext_t *context, int (*rngFunction)(void *, uint8_t *, uint16_t), void *rngContext);

/**
 *
 * @brief Compute the secret key G^X^Y mod p
 * G^X mod P has been computed in previous call to DHMCreatePublic
 * G^Y mod P must have been set in context->peer
 *
 * @param[in/out] 	context		Read the public values from context, export the key to context->key
 * @param[in]		rngFunction	Pointer to a random number generation function, used for blinding countermeasure, may be NULL	
 * @param[in]		rngContext	Pointer to the RNG function context
 *
 */
void bzrtpCrypto_DHMComputeSecret(bzrtpDHMContext_t *context, int (*rngFunction)(void *, uint8_t *, uint16_t), void *rngContext);

/**
 *
 * @brief Clean DHM context 
 *
 * @param	context	The context to deallocate
 *
 */
void bzrtpCrypto_DestroyDHMContext(bzrtpDHMContext_t *context);

#endif /* ifndef CRYPTOWRAPPER_H */
