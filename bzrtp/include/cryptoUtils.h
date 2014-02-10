/**
 @file cryptoUtils.h
 
 @brief Prototypes for security related functions
 - Key Derivation Function
 - CRC
 - Base32
 - Key agreement algorithm negociation

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
#ifndef CRYPTOUTILS_H
#define CRYPTOUTILS_H

#include "packetParser.h"
#include "typedef.h"

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
int bzrtp_keyDerivationFunction(uint8_t *key, uint16_t keyLength,
		uint8_t *label, uint16_t labelLength,
		uint8_t *context, uint16_t contextLength,
		uint16_t hmacLength,
		void (*hmacFunction)(uint8_t *, uint8_t, uint8_t *, uint32_t, uint8_t, uint8_t *),
		uint8_t *output);


/**
 * @brief SAS rendering from 32 bits to 4 characters
 * Function defined in rfc section 5.1.6
 *
 * @param[in]	sas		The 32 bits SAS
 * @param[out]	output	The 4 chars string to be displayed to user for vocal confirmation
 *
 */
void bzrtp_base32(uint32_t sas, char output[4]);

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
uint32_t bzrtp_CRC32(uint8_t *input, uint16_t length); 

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
 * @param[in/out]	zrtpContext			The context contains the list of available algo and is set with the selected ones
 * @param[in]		peerHelloMessage	The peer hello message containing his set of available algos
 *
 * return			0 on succes, error code otherwise
 *
 */
int crypoAlgoAgreement(bzrtpContext_t *zrtpContext, bzrtpHelloMessage_t *peerHelloMessage);

/**
 * @brief Update context crypto function pointer according to related values of choosen algorithms fields (hashAlgo, cipherAlgo, etc..)
 *
 * @param[in/out]	context		The bzrtp context to be updated
 *
 * @return			0 on succes
 */
int updateCryptoFunctionPointers(bzrtpContext_t *context);

/**
 * @brief Map the string description of algo type to an int defined in cryptoWrapper.h
 *
 * @param[in] algoType		A 4 chars string containing the algo type as listed in rfc sections 5.1.2 to 5.1.6
 * @param[in] algoFamily	The integer mapped algo family (ZRTP_HASH_TYPE, ZRTP_CIPHERBLOCK_TYPE, ZRTP_AUTHTAG_TYPE,
 * 							ZRTP_KEYAGREEMENT_TYPE or ZRTP_SAS_TYPE)
 * @return 		The int value mapped to the algo type, ZRTP_UNSET_ALGO on error
 */
uint8_t cryptoAlgoTypeStringToInt(uint8_t algoType[4], uint8_t algoFamily);

/**
 * @brief Unmap the string description of algo type to an int defined in cryptoWrapper.h
 *
 * @param[in] algoTypeInt	The integer algo type defined in crypoWrapper.h
 * @param[in] algoFamily	The string code for the algorithm as defined in rfc 5.1.2 to 5.1.6
 */
void cryptoAlgoTypeIntToString(uint8_t algoTypeInt, uint8_t algoTypeString[4]);

/**
 * @brief Destroy a key by setting it to a random number
 * Key is not freed, caller must deal with memory management
 *
 * @param[in/out]	key			The key to be destroyed
 * @param[in]		keyLength	The keyLength in bytes
 * @param[in]		rngContext	The context for RNG
 */
void bzrtp_DestroyKey(uint8_t *key, uint8_t keyLength, void *rngContext);

#endif /* CRYPTOUTILS_H */
