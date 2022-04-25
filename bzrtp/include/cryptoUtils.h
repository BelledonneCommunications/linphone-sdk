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
#ifndef CRYPTOUTILS_H
#define CRYPTOUTILS_H

#include "typedef.h"
#include "packetParser.h"

#ifdef __cplusplus
extern "C"{
#endif

/** Return available crypto functions.
 * If more than 7 are available, returns the 7 first
 *
 * - Hash: HMAC-SHA256(Mandatory), HMAC-SHA384
 * - CipherBlock: AES128(Mandatory), AES256
 * - Auth Tag: HMAC-SHA132 and HMAC-SHA180 (These are mandatory for SRTP and depends on the SRTP implementation thus we can just suppose they are both available)
 * - Key Agreement: DHM3k(Mandatory), DHM2k(optional and shall not be used except on low power devices), X25519, X448
 * - Sas: base32(Mandatory), b256(pgp words)
 */
uint8_t bzrtpUtils_getAvailableCryptoTypes(uint8_t algoType, uint8_t availableTypes[7]);

/** Return available crypto functions.
 * WARNING: this function can return more than 7 items, do not use it for something else than checking the
 * values forced by bzrtp_setSupportedCryptoTypes
 */
uint8_t bzrtpUtils_getAllAvailableCryptoTypes(uint8_t algoType, uint8_t availableTypes[256]);

/**
 *
 * @brief ZRTP Key Derivation Function as in rfc 4.5.1
 *
 * KDF(KI, Label, Context, L) = HMAC(KI, i || Label ||
 *								0x00 || Context || L)
 * where
 * - i is a 32-bits integer fixed to 0x00000001
 * - L is a 32-bit big-endian positive
 *  integer, not to exceed the length in bits of the output of the HMAC.
 *  The output of the KDF is truncated to the leftmost L bits.
 *
 * @param[in]	key				The key for HMAC
 * @param[in]	keyLength		Length of the key in bytes
 * @param[in]	label			A string to be included in the hash
 * @param[in]	labelLength		Length of the label in bytes
 * @param[in]	context			a context string for the key derivation
 * @param[in]	contextLength	Length of the context string in bytes
 * @param[in]	hmacLength		The output of the KDF is the HMAC truncated to the leftmost L bytes
 * @param[in]	hmacFunction	The hashmac function to be used to compute the KDF
 * @param[out]	output			A buffer to store the hmacLength bytes of output			
 *
 * @return		0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_keyDerivationFunction(const uint8_t *key, const size_t keyLength,
		const uint8_t *label, const size_t labelLength,
		const uint8_t *context, const size_t contextLength,
		const uint8_t hmacLength,
		void (*hmacFunction)(const uint8_t *, size_t, const uint8_t *, size_t, uint8_t, uint8_t *),
		uint8_t *output);


/**
 * @brief SAS rendering from 32 bits to 4 characters
 * Function defined in rfc section 5.1.6
 *
 * @param[in]	sas		The 32 bits SAS
 * @param[out]	output	The 4 chars string to be displayed to user for vocal confirmation
 * @param[in]	outputSize	size of the ouput buffer
 *
 */
void bzrtp_base32(uint32_t sas, char *output, int outputSize);

/**
 * @brief SAS rendering from 32 bits to pgp word list
 * Function defined in rfc section 5.1.6
 *
 * @param[in]	sas		The 32 bits SAS
 * @param[out]	output		The output list. Passed in array must be at least 32 bytes
 * @param[in]	outputSize	size of the ouput buffer
 *
 */
void bzrtp_base256(uint32_t sas, char *output, int outputSize);

/**
 * @brief CRC32 as defined in RFC4960 Appendix B - Polynomial is 0x1EDC6F41
 *
 * CRC is computed in reverse bit mode (least significant bit first within each byte)
 * reversed value of polynom (0x82F63B78) was used to compute the lookup table (source
 * http://en.wikipedia.org/wiki/Cyclic_redundancy_check#Commonly_used_and_standardized_CRCs)
 *
 * @param[in]	input	input data
 * @param[in]	length	length of data in bytes
 *
 * @return		the 32 bits CRC value
 *
 */
BZRTP_EXPORT uint32_t bzrtp_CRC32(uint8_t *input, uint16_t length);

/* error code for the cryptoAlgoAgreement and function pointer update functions */
#define	ZRTP_CRYPTOAGREEMENT_INVALIDCONTEXT		0x1001
#define	ZRTP_CRYPTOAGREEMENT_INVALIDMESSAGE		0x1002
#define	ZRTP_CRYPTOAGREEMENT_INVALIDSELFALGO	0x1003
#define ZRTP_CRYPTOAGREEMENT_NOCOMMONALGOFOUND	0x1004
#define ZRTP_CRYPTOAGREEMENT_INVALIDCIPHER		0x1005
#define ZRTP_CRYPTOAGREEMENT_INVALIDHASH		0x1006
#define ZRTP_CRYPTOAGREEMENT_INVALIDAUTHTAG		0x1007
#define ZRTP_CRYPTOAGREEMENT_INVALIDSAS			0x1008
/**
 * @brief select a key agreement algorithm from the one available in context and the one provided by
 * peer in Hello Message as described in rfc section 4.1.2
 * - other algorithm are selected according to availability and selected key agreement as described in
 *   rfc section 5.1.5
 * The other algorithm choice will finally be set by the endpoint acting as initiator in the commit packet
 *
 * @param[in]		zrtpContext			The context contains the list of available algo
 * @param[out]		zrtpChannelContext	The bzrtp channel context to be updated
 * @param[in]		peerHelloMessage	The peer hello message containing his set of available algos
 *
 * return			0 on succes, error code otherwise
 *
 */
BZRTP_EXPORT int bzrtp_cryptoAlgoAgreement(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, bzrtpHelloMessage_t *peerHelloMessage);

/**
 * @brief Update context crypto function pointer according to related values of choosen algorithms fields (hashAlgo, cipherAlgo, etc..)
 *
 * @param[in,out]	zrtpChannelContext		The bzrtp channel context to be updated
 *
 * @return			0 on succes
 */
BZRTP_EXPORT int bzrtp_updateCryptoFunctionPointers(bzrtpChannelContext_t *zrtpChannelContext);

/**
 * @brief Select common algorithm from the given array where algo are represented by their 4 chars string defined in rfc section 5.1.2 to 5.1.6
 * Master array is the one given the preference order
 * All algo are designed by their uint8_t mapped values
 *
 * @param[in]	masterArray	 		The ordered available algo, result will follow this ordering
 * @param[in]	masterArrayLength	Number of valids element in the master array
 * @param[in]	slaveArray	 		The available algo, order is not taken in account
 * @param[in]	slaveArrayLength	Number of valids element in the slave array
 * @param[out]	commonArray	 		Common algorithms found, max size 7
 *
 * @return		the number of common algorithms found
 */
uint8_t bzrtp_selectCommonAlgo(uint8_t masterArray[7], uint8_t masterArrayLength, uint8_t *slaveArray, uint8_t slaveArrayLength, uint8_t commonArray[7]);

/**
 * @brief add mandatory crypto functions if they are not already included
 * - Hash function
 * - Cipher Block
 * - Auth Tag
 * - Key agreement
 * - SAS
 *
 * @param[in]		algoType		mapped to defines, must be in [ZRTP_HASH_TYPE, ZRTP_CIPHERBLOCK_TYPE, ZRTP_AUTHTAG_TYPE, ZRTP_KEYAGREEMENT_TYPE or ZRTP_SAS_TYPE]
 * @param[in,out]	algoTypes		mapped to uint8_t value of the 4 char strings giving the algo types as string according to rfc section 5.1.2 to 5.1.6
 * @param[in,out]	algoTypesCount		number of algo types
 */
BZRTP_EXPORT void bzrtp_addMandatoryCryptoTypesIfNeeded(uint8_t algoType, uint8_t algoTypes[7], uint8_t *algoTypesCount);

/**
 * @brief Map the string description of algo type to an int defined in cryptoWrapper.h
 *
 * @param[in] algoType		A 4 chars string containing the algo type as listed in rfc sections 5.1.2 to 5.1.6
 * @param[in] algoFamily	The integer mapped algo family (ZRTP_HASH_TYPE, ZRTP_CIPHERBLOCK_TYPE, ZRTP_AUTHTAG_TYPE,
 * 							ZRTP_KEYAGREEMENT_TYPE or ZRTP_SAS_TYPE)
 * @return 		The int value mapped to the algo type, ZRTP_UNSET_ALGO on error
 */
BZRTP_EXPORT uint8_t bzrtp_cryptoAlgoTypeStringToInt(uint8_t algoType[4], uint8_t algoFamily);

/**
 * @brief Unmap the string description of algo type to an int defined in cryptoWrapper.h
 *
 * @param[in] algoTypeInt	The integer algo type defined in crypoWrapper.h
 * @param[in] algoTypeString	The string code for the algorithm as defined in rfc 5.1.2 to 5.1.6
 */
BZRTP_EXPORT void bzrtp_cryptoAlgoTypeIntToString(uint8_t algoTypeInt, uint8_t algoTypeString[4]);

/**
 * @brief Destroy a key by setting it to a random number
 * Key is not freed, caller must deal with memory management.
 * Does nothing if the key pointer is NULL
 *
 * @param[in,out]	key			The key to be destroyed
 * @param[in]		keyLength	The keyLength in bytes
 * @param[in]		rngContext	The context for RNG
 */
BZRTP_EXPORT void bzrtp_DestroyKey(uint8_t *key, size_t keyLength, void *rngContext);

/**
 * Return the public value(public key or ciphertext) length in bytes according to given key agreement algorithm and packet type
 * packet type is used to determine public value type when in KEM mode:
 *     - commit holds a public key
 *     - DHPart1 holds a ciphertext
 *     - DHPart2 holds a nonce
 *
 * @param[in]	keyAgreementAlgo	The key agreement algo mapped to an integer as defined in cryptoUtils.h
 * @param[in]	messageType			MSGTYPE_COMMIT, MSGTYPE_DHPART1i or MSGTYPE_DHPART2: needed for KEM mode
 *
 * @return		the public value length in bytes
 *
 */
BZRTP_EXPORT uint16_t bzrtp_computeKeyAgreementPublicValueLength(uint8_t keyAgreementAlgo, uint8_t messageTyoe);

/**
 * Return the shared secret size in bytes according to given key agreement algorithm
 *
 * @param[in]	keyAgreementAlgo	The key agreement algo mapped to an integer as defined in cryptoUtils.h
 *
 * @return		the shared secrert length in bytes
 *
 */
uint16_t bzrtp_computeKeyAgreementSharedSecretLength(uint8_t keyAgreementAlgo, uint8_t hashLength);

/**
 * Check a given key agrement algorithm is a KEM or not
 * @param[in]	keyAgreementAlgo a key agreement algo mapped on a uint8_t
 *
 * @return TRUE if the algorithm is of type KEM, FALSE otherwise
 */
bool_t bzrtp_isKem(uint8_t keyAgreementAlgo);

/* have a C interface to the KEM defined in c++ in bctoolbox
 TODO: build bzrtp in c++ and directly use the bctoolbox interface */
/* Forward declaration of KEM context */
typedef struct bzrtp_KEMContext_struct bzrtp_KEMContext_t;

/**
 * Create the KEM context
 *
 * @param[in]	keyAgreementAlgo one of ZRTP_KEYAGREEMENT_KYB1, ZRTP_KEYAGREEMENT_KYB2, ZRTP_KEYAGREEMENT_KYB3,
 * 									ZRTP_KEYAGREEMENT_SIK1, ZRTP_KEYAGREEMENT_SIK2, ZRTP_KEYAGREEMENT_SIK3,
 *									ZRTP_KEYAGREEMENT_K255_KYB512, ZRTP_KEYAGREEMENT_K255_SIK434,
 *									ZRTP_KEYAGREEMENT_K448_KYB1024, ZRTP_KEYAGREEMENT_K448_SIK751
 *
 * @return a pointer to the created context, NULL in case of failure
 */
bzrtp_KEMContext_t *bzrtp_createKEMContext(uint8_t keyAgreementAlgo, uint8_t hashAlgo);

/**
 * Generate a key pair and store it in the context
 *
 * @return 0 on success
 */
int bzrtp_KEM_generateKeyPair(bzrtp_KEMContext_t *ctx);

/**
 * Extract the public key from context
 * @param[in]	ctx			a valid KEM context
 * @param[out]	publicKey	the key in this context. Data is copied in this buffer, caller must ensure it is the correct size
 *
 * @return 0 on success
 */
int bzrtp_KEM_getPublicKey(bzrtp_KEMContext_t *ctx, uint8_t *publicKey);

/**
 * Extract the shared secret from context
 * @param[in]	ctx				a valid KEM context
 * @param[out]	sharedSecret	the shared secret in this context. Data is copied in this buffer, caller must ensure it is the correct size
 *
 * @return 0 on success
 */
int bzrtp_KEM_getSharedSecret(bzrtp_KEMContext_t *ctx, uint8_t *sharedSecret);

/**
 * Generate and encapsulate a secret given a public key, shared secret is stored in the context
 * @param[in]	ctx				a valid KEM context
 * @param[in]	publicKey		the public key we want to encapsulate to
 * @param[out]	cipherText		the encapsulated secret
 *
 * @return 0 on success
 */
int bzrtp_KEM_encaps(bzrtp_KEMContext_t *ctx, uint8_t *publicKey, uint8_t *cipherText);

/**
 * Decapsulate a key, shared secret is stored in the context
 *
 * @param[in]	ctx				a valid KEM context holding the secret Key needed to decapsulate
 * @param[in]	cipherText		the encapsulated secret
 *
 * @return 0 on success
 */
int bzrtp_KEM_decaps(bzrtp_KEMContext_t *ctx, uint8_t *cipherText);

/**
 * Destroy a KEM context. Safely clean any secrets it may holding
 * @param[in]	ctx				a valid KEM context
 *
 * @return 0 on success
 */
int bzrtp_destroyKEMContext(bzrtp_KEMContext_t *ctx);
#ifdef __cplusplus
}
#endif

#endif /* CRYPTOUTILS_H */
