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

/** Return available crypto functions. For now we have
 *
 * - Hash: HMAC-SHA256(Mandatory)
 * - CipherBlock: AES128(Mandatory)
 * - Auth Tag: HMAC-SHA132 and HMAC-SHA180 (These are mandatory for SRTP and depends on the SRTP implementation thus we can just suppose they are both available)
 * - Key Agreement: DHM3k(Mandatory), DHM2k(optional and shall not be used except on low power devices)
 * - Sas: base32(Mandatory), b256(pgp words)
 */
uint8_t bzrtpUtils_getAvailableCryptoTypes(uint8_t algoType, uint8_t availableTypes[7]);

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
		const uint16_t hmacLength,
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
uint8_t selectCommonAlgo(uint8_t masterArray[7], uint8_t masterArrayLength, uint8_t slaveArray[7], uint8_t slaveArrayLength, uint8_t commonArray[7]);

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
BZRTP_EXPORT void bzrtp_DestroyKey(uint8_t *key, uint8_t keyLength, void *rngContext);

/**
 * @brief Convert an hexadecimal string into the corresponding byte buffer
 *
 * @param[out]	outputBytes			The output bytes buffer, must have a length of half the input string buffer
 * @param[in]	inputString			The input string buffer, must be hexadecimal(it is not checked by function, any non hexa char is converted to 0)
 * @param[in]	inputStringLength	The length in chars of the string buffer, output is half this length
 */
void bzrtp_strToUint8(uint8_t *outputBytes, uint8_t *inputString, uint16_t inputStringLength);

/**
 * @brief Convert a byte buffer into the corresponding hexadecimal string
 *
 * @param[out]	outputString		The output string buffer, must have a length of twice the input bytes buffer
 * @param[in]	inputBytes			The input bytes buffer
 * @param[in]	inputBytesLength	The length in bytes buffer, output is twice this length
 */
void bzrtp_int8ToStr(uint8_t *outputString, uint8_t *inputBytes, uint16_t inputBytesLength);

/**
 * @brief	convert an hexa char [0-9a-fA-F] into the corresponding unsigned integer value
 * Any invalid char will be converted to zero without any warning
 *
 * @param[in]	inputChar	a char which shall be in range [0-9a-fA-F]
 *
 * @return		the unsigned integer value in range [0-15]
 */
uint8_t bzrtp_charToByte(uint8_t inputChar);

/**
 * @brief	convert a byte which value is in range [0-15] into an hexa char [0-9a-fA-F]
 *
 * @param[in]	inputByte	an integer which shall be in range [0-15]
 *
 * @return		the hexa char [0-9a-f] corresponding to the input
 */
uint8_t bzrtp_byteToChar(uint8_t inputByte);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTOUTILS_H */
