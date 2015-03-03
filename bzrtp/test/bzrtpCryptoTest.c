/**
 @file bzrtpCryptoTests.c
 @brief Test suite for the crypto wrapper.
 Test the following functions needed by ZRTP:
 - random number generation
 - HMAC-SHA256
 - AES-128 CFB mode
 - DHM2k and DHM3k
 - ZRTP Key Derivation function
 - CRC32

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
#include <math.h>
#include "CUnit/Basic.h"

#include "bzrtp/bzrtp.h"
#include "cryptoWrapper.h"
#include "cryptoUtils.h"
#include "testUtils.h"


/**
 *
 * @brief Test the Random Number Generator with a serie of 32 bits integers
 * This test doesn't really test the quality of the RNG
 * It just compute the mean and standard deviation of the generated number
 * Being a discrete uniform distribution, standard deviation is supposed to be
 * (max-min)/sqrt(12), in our case (0xFFFFFFFF - 0)/sqrt(12).
 * To get an more readable perspective, mean and sqrt are divided by 0xFFFFFFFF
 * and result are tested agains 0.5 and 1/sqrt(12)
 *
 */
#define NB_INT32_TESTED 100000
void test_RNG(void) {
	uint8_t buffer[4];
	uint32_t rnBuffer;
	long double m0=0.0,s0=0.0, m1=0.0;
	uint8_t start=0;
	int i;
	/* init the RNG */
	bzrtpRNGContext_t *context = bzrtpCrypto_startRNG((uint8_t *)"36 15 Grouiik vous souhaite une bonne journee", 46);
	if (context == NULL) {
		CU_FAIL("Cant' start RNG engine");
		return;
	}

	for (i=0; i<NB_INT32_TESTED; i++) {
		/* get a random number */
		if (bzrtpCrypto_getRandom(context, (uint8_t *)&buffer, 4) != 0) {
			CU_FAIL("Cant' get random number");
			return;		
		} else {
			/* turn the 4 bytes random into an unsigned int on 32 bits */
			rnBuffer = (((uint32_t)(buffer[0]))<<24)+(((uint32_t)(buffer[1]))<<16)+(((uint32_t)(buffer[2]))<<8)+(((uint32_t)(buffer[3])));

			/* compute mean and variance */
			if (start == 0) {
				start=1;
				m0=rnBuffer;
				s0=0;
			} else {
				m1 = m0 + (rnBuffer-m0)/i;
				s0 +=  (rnBuffer - m0)*(rnBuffer  - m1);
				m0 = m1;
			}
		}
	}

	/* mean/0xFFFFFFFF shall be around 0.5 */
	m0 = m0/(long double)0xFFFFFFFF;
	if (m0>0.49 && m0<0.51) {
		CU_PASS("RNG mean value ok");
	} else {
		CU_FAIL("RNG mean value incorrect");
	}

	/* sigma value/0xFFFFFFFF shall be around 1/sqrt(12)[0.288675]*/
	s0 = sqrt(s0/i-1)/(long double)0xFFFFFFFF;
	if (s0>0.278 && s0<0.299) {
		CU_PASS("RNG sigma value ok");
	} else {
		CU_FAIL("RNG sigma value incorrect");
	}

	printf ("%d 32 bits integers generated Mean %Lf Sigma/Mean %Lf\n", NB_INT32_TESTED, m0, s0);

	/* destroy the RNG context */
	bzrtpCrypto_destroyRNG(context);
}

/**
 *
 * Test pattern for SHA256 
 *
 */
#define NB_SHA256_TESTS 8
static const uint8_t patternSHA256Input[NB_SHA256_TESTS][70] = {
	{""},
	{"abc"},
	{"message digest"},
	{"secure hash algorithm"},
	{"SHA256 is considered to be safe"},
	{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"},
	{"For this sample, this 63-byte string will be used as input data"},
	{"This is exactly 64 bytes long, not counting the terminating byte"},
};

static const uint8_t patternSHA256Output[NB_SHA256_TESTS][32] = {
	{0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55},
	{0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad},
	{0xf7, 0x84, 0x6f, 0x55, 0xcf, 0x23, 0xe1, 0x4e, 0xeb, 0xea, 0xb5, 0xb4, 0xe1, 0x55, 0x0c, 0xad, 0x5b, 0x50, 0x9e, 0x33, 0x48, 0xfb, 0xc4, 0xef, 0xa3, 0xa1, 0x41, 0x3d, 0x39, 0x3c, 0xb6, 0x50},
	{0xf3, 0x0c, 0xeb, 0x2b, 0xb2, 0x82, 0x9e, 0x79, 0xe4, 0xca, 0x97, 0x53, 0xd3, 0x5a, 0x8e, 0xcc, 0x00, 0x26, 0x2d, 0x16, 0x4c, 0xc0, 0x77, 0x08, 0x02, 0x95, 0x38, 0x1c, 0xbd, 0x64, 0x3f, 0x0d},
	{0x68, 0x19, 0xd9, 0x15, 0xc7, 0x3f, 0x4d, 0x1e, 0x77, 0xe4, 0xe1, 0xb5, 0x2d, 0x1f, 0xa0, 0xf9, 0xcf, 0x9b, 0xea, 0xea, 0xd3, 0x93, 0x9f, 0x15, 0x87, 0x4b, 0xd9, 0x88, 0xe2, 0xa2, 0x36, 0x30},
	{0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39, 0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67, 0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1},
	{0xf0, 0x8a, 0x78, 0xcb, 0xba, 0xee, 0x08, 0x2b, 0x05, 0x2a, 0xe0, 0x70, 0x8f, 0x32, 0xfa, 0x1e, 0x50, 0xc5, 0xc4, 0x21, 0xaa, 0x77, 0x2b, 0xa5, 0xdb, 0xb4, 0x06, 0xa2, 0xea, 0x6b, 0xe3, 0x42},
	{0xab, 0x64, 0xef, 0xf7, 0xe8, 0x8e, 0x2e, 0x46, 0x16, 0x5e, 0x29, 0xf2, 0xbc, 0xe4, 0x18, 0x26, 0xbd, 0x4c, 0x7b, 0x35, 0x52, 0xf6, 0xb3, 0x82, 0xa9, 0xe7, 0xd3, 0xaf, 0x47, 0xc2, 0x45, 0xf8}
};

/**
 *
 * Tests for SHA256
 *
 */
void test_sha256(void) {
	int i;
	uint8_t outputSha256[32];

	for (i=0; i<NB_SHA256_TESTS; i++) {
		bzrtpCrypto_sha256(patternSHA256Input[i], strlen((char *)patternSHA256Input[i]), 32, outputSha256);
		CU_ASSERT_TRUE(memcmp(outputSha256, patternSHA256Output[i], 32) == 0);
	}
}

/**
 *
 * Test pattern HMAC SHA256 from rfc4231
 *
 */
static const uint8_t patternHMACSHA256[7][32] =
{
	{0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7},
	{0x5b, 0xdc, 0xc1, 0x46, 0xbf, 0x60, 0x75, 0x4e, 0x6a, 0x04, 0x24, 0x26, 0x08, 0x95, 0x75, 0xc7, 0x5a, 0x00, 0x3f, 0x08, 0x9d, 0x27, 0x39, 0x83, 0x9d, 0xec, 0x58, 0xb9, 0x64, 0xec, 0x38, 0x43},
	{0x77, 0x3e, 0xa9, 0x1e, 0x36, 0x80, 0x0e, 0x46, 0x85, 0x4d, 0xb8, 0xeb, 0xd0, 0x91, 0x81, 0xa7, 0x29, 0x59, 0x09, 0x8b, 0x3e, 0xf8, 0xc1, 0x22, 0xd9, 0x63, 0x55, 0x14, 0xce, 0xd5, 0x65, 0xfe},
	{0x82, 0x55, 0x8a, 0x38, 0x9a, 0x44, 0x3c, 0x0e, 0xa4, 0xcc, 0x81, 0x98, 0x99, 0xf2, 0x08, 0x3a, 0x85, 0xf0, 0xfa, 0xa3, 0xe5, 0x78, 0xf8, 0x07, 0x7a, 0x2e, 0x3f, 0xf4, 0x67, 0x29, 0x66, 0x5b},
	{0xa3, 0xb6, 0x16, 0x74, 0x73, 0x10, 0x0e, 0xe0, 0x6e, 0x0c, 0x79, 0x6c, 0x29, 0x55, 0x55, 0x2b, 0xa3, 0xb6, 0x16, 0x74, 0x73, 0x10, 0x0e, 0xe0, 0x6e, 0x0c, 0x79, 0x6c, 0x29, 0x55, 0x55, 0x2b},
	{0x60, 0xe4, 0x31, 0x59, 0x1e, 0xe0, 0xb6, 0x7f, 0x0d, 0x8a, 0x26, 0xaa, 0xcb, 0xf5, 0xb7, 0x7f, 0x8e, 0x0b, 0xc6, 0x21, 0x37, 0x28, 0xc5, 0x14, 0x05, 0x46, 0x04, 0x0f, 0x0e, 0xe3, 0x7f, 0x54},
	{0x9b, 0x09, 0xff, 0xa7, 0x1b, 0x94, 0x2f, 0xcb, 0x27, 0x63, 0x5f, 0xbc, 0xd5, 0xb0, 0xe9, 0x44, 0xbf, 0xdc, 0x63, 0x64, 0x4f, 0x07, 0x13, 0x93, 0x8a, 0x7f, 0x51, 0x53, 0x5c, 0x3a, 0x35, 0xe2}
};
	
/**
 *
 * Tests for HMAC SHA256 from rfc4231
 *
 */
void test_hmacSha256(void) {
	int i;
	uint8_t keyHmacSha256[140];
	uint8_t inputHmacSha256[140];
	uint8_t outputHmacSha256[32];

	/* test case 1 */
	memset(keyHmacSha256, 0x0b, 32);
	bzrtpCrypto_hmacSha256(keyHmacSha256, 20, (uint8_t*)"Hi There", 8, 32, outputHmacSha256);
	CU_ASSERT_TRUE(memcmp(outputHmacSha256, patternHMACSHA256[0], 32) == 0);

	/* test case 2 */
	bzrtpCrypto_hmacSha256((uint8_t*)"Jefe", 4, (uint8_t*)"what do ya want for nothing?", 28, 32, outputHmacSha256);
	CU_ASSERT_TRUE(memcmp(outputHmacSha256, patternHMACSHA256[1], 32) == 0);

	/* test case 3 */
	memset(keyHmacSha256, 0xaa, 20);
	memset(inputHmacSha256, 0xdd, 50);
	bzrtpCrypto_hmacSha256(keyHmacSha256, 20, inputHmacSha256, 50, 32, outputHmacSha256);
	CU_ASSERT_TRUE(memcmp(outputHmacSha256, patternHMACSHA256[2], 32) == 0);

	/* test case 4 */
	for (i=1; i<26; i++) keyHmacSha256[i-1]=i;
	memset(inputHmacSha256, 0xcd, 50);
	bzrtpCrypto_hmacSha256(keyHmacSha256, 25, inputHmacSha256, 50, 32, outputHmacSha256);
	CU_ASSERT_TRUE(memcmp(outputHmacSha256, patternHMACSHA256[3], 32) == 0);

	/* test case 5 */
	memset(keyHmacSha256, 0x0c, 20);
	bzrtpCrypto_hmacSha256(keyHmacSha256, 20, (uint8_t*)"Test With Truncation", 20, 16, outputHmacSha256);
	CU_ASSERT_TRUE(memcmp(outputHmacSha256, patternHMACSHA256[4], 16) == 0); /* this test case test only the first 16 bytes of hash result, see RFC4231 4.6 for details */

	/* test case 6 */
	memset(keyHmacSha256, 0xaa, 131);
	bzrtpCrypto_hmacSha256(keyHmacSha256, 131, (uint8_t*)"Test Using Larger Than Block-Size Key - Hash Key First", 54, 32, outputHmacSha256);
	CU_ASSERT_TRUE(memcmp(outputHmacSha256, patternHMACSHA256[5], 32) == 0);

	/* test case 7 */
	memset(keyHmacSha256, 0xaa, 131);
	bzrtpCrypto_hmacSha256(keyHmacSha256, 131, (uint8_t*)"This is a test using a larger than block-size key and a larger than block-size data. The key needs to be hashed before being used by the HMAC algorithm.", 152, 32, outputHmacSha256);
	CU_ASSERT_TRUE(memcmp(outputHmacSha256, patternHMACSHA256[6], 32) == 0);

}

/**
 *
 *	AES128-CFB128 test pattern from NIST
 *
 */
#define AES128_CFB_TEST_NB 6
static const uint8_t patternEncryptAES128CFBkey[AES128_CFB_TEST_NB][16] = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x10, 0xa5, 0x88, 0x69, 0xd7, 0x4b, 0xe5, 0xa3, 0x74, 0xcf, 0x86, 0x7c, 0xfb, 0x47, 0x38, 0x59},
	{0x47, 0xd6, 0x74, 0x2e, 0xef, 0xcc, 0x04, 0x65, 0xdc, 0x96, 0x35, 0x5e, 0x85, 0x1b, 0x64, 0xd9}
};
static const uint8_t patternEncryptAES128CFBIV[AES128_CFB_TEST_NB][16] = {
	{0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0},
	{0xf3, 0x44, 0x81, 0xec, 0x3c, 0xc6, 0x27, 0xba, 0xcd, 0x5d, 0xc3, 0xfb, 0x08, 0xf2, 0x73, 0xe6},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};
static const uint8_t patternEncryptAES128CFBplain[AES128_CFB_TEST_NB][16] = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};
static const uint8_t patternEncryptAES128CFBcipher[AES128_CFB_TEST_NB][16] = {
	{0x3a, 0xd7, 0x8e, 0x72, 0x6c, 0x1e, 0xc0, 0x2b, 0x7e, 0xbf, 0xe9, 0x2b, 0x23, 0xd9, 0xec, 0x34},
	{0xaa, 0xe5, 0x93, 0x9c, 0x8e, 0xfd, 0xf2, 0xf0, 0x4e, 0x60, 0xb9, 0xfe, 0x71, 0x17, 0xb2, 0xc2},
	{0x85, 0x68, 0x26, 0x17, 0x97, 0xde, 0x17, 0x6b, 0xf0, 0xb4, 0x3b, 0xec, 0xc6, 0x28, 0x5a, 0xfb},
	{0x03, 0x36, 0x76, 0x3e, 0x96, 0x6d, 0x92, 0x59, 0x5a, 0x56, 0x7c, 0xc9, 0xce, 0x53, 0x7f, 0x5e},
	{0x6d, 0x25, 0x1e, 0x69, 0x44, 0xb0, 0x51, 0xe0, 0x4e, 0xaa, 0x6f, 0xb4, 0xdb, 0xf7, 0x84, 0x65},
	{0x03, 0x06, 0x19, 0x4f, 0x66, 0x6d, 0x18, 0x36, 0x24, 0xaa, 0x23, 0x0a, 0x8b, 0x26, 0x4a, 0xe7}
};

/**
 *
 * Tests for AES-128 CFB mode encrypt and decrypt
 *
 */
void test_aes128CFB(void) {
	uint8_t output[16];
	uint8_t IV[16];
	int i;

	for (i=0; i<AES128_CFB_TEST_NB; i++) {
		memcpy(IV, patternEncryptAES128CFBIV[i], 16*sizeof(uint8_t));
		/* Encryption test */
		bzrtpCrypto_aes128CfbEncrypt(patternEncryptAES128CFBkey[i], IV, patternEncryptAES128CFBplain[i], 16, output);
		CU_ASSERT_TRUE(memcmp(output, patternEncryptAES128CFBcipher[i], 16) == 0);
		CU_ASSERT_TRUE(memcmp(IV, patternEncryptAES128CFBIV[i], 16) == 0); /* check the IV wasn't modified */
		/* Decryption test */
		bzrtpCrypto_aes128CfbDecrypt(patternEncryptAES128CFBkey[i], IV, patternEncryptAES128CFBcipher[i], 16, output);
		CU_ASSERT_TRUE(memcmp(output, patternEncryptAES128CFBplain[i], 16) == 0);
		CU_ASSERT_TRUE(memcmp(IV, patternEncryptAES128CFBIV[i], 16) == 0); /* check the IV wasn't modified */
	}
}

/**
 *
 * Tests for DHM-2048: just performe a key exchange and check that generated secrets are the same
 *
 */
void test_dhm2048(void) {
	bzrtpRNGContext_t *RNGcontext;
	bzrtpDHMContext_t *DHMaContext;
	bzrtpDHMContext_t *DHMbContext;
	bzrtpDHMContext_t *DHMcContext;

	/* start the Random Number Generator */
	RNGcontext = bzrtpCrypto_startRNG((uint8_t *)"36 15 Grouiik vous souhaite une bonne journee", 46);

	/* Create the context for Alice */
	DHMaContext = bzrtpCrypto_CreateDHMContext(ZRTP_KEYAGREEMENT_DH2k, 32);

	/* Create the public value for Alice G^Xa mod P */
	bzrtpCrypto_DHMCreatePublic(DHMaContext, (int (*)(void *, uint8_t *, size_t))bzrtpCrypto_getRandom, (void *)RNGcontext);

	/* Create the context for Bob */
	DHMbContext = bzrtpCrypto_CreateDHMContext(ZRTP_KEYAGREEMENT_DH2k, 32);

	/* Create the public value for Bob G^Xb mod P */
	bzrtpCrypto_DHMCreatePublic(DHMbContext, (int (*)(void *, uint8_t *, size_t))bzrtpCrypto_getRandom, (void *)RNGcontext);

	printf("Context created %p and %p\n", DHMaContext, DHMbContext);

	/* exchange public values */
	DHMaContext->peer = (uint8_t *)malloc(DHMaContext->primeLength*sizeof(uint8_t));
	DHMbContext->peer = (uint8_t *)malloc(DHMbContext->primeLength*sizeof(uint8_t));
	memcpy (DHMaContext->peer, DHMbContext->self, DHMaContext->primeLength*sizeof(uint8_t));
	memcpy (DHMbContext->peer, DHMaContext->self, DHMbContext->primeLength*sizeof(uint8_t));

	printf("Call compute secret\n");
	/* compute secret key */
	bzrtpCrypto_DHMComputeSecret(DHMaContext, (int (*)(void *, uint8_t *, size_t))bzrtpCrypto_getRandom, (void *)RNGcontext);
	bzrtpCrypto_DHMComputeSecret(DHMbContext, (int (*)(void *, uint8_t *, size_t))bzrtpCrypto_getRandom, (void *)RNGcontext);

	CU_ASSERT_TRUE(memcmp(DHMaContext->key, DHMbContext->key, 256) == 0); /* generated key shall be 256 bytes long */

	/* free context */
	bzrtpCrypto_DestroyDHMContext(DHMaContext);
	bzrtpCrypto_DestroyDHMContext(DHMbContext);

	/* create an unused context and destroy it to check the correct implementation of the create/destroy functions */
	DHMcContext = bzrtpCrypto_CreateDHMContext(ZRTP_KEYAGREEMENT_DH2k, 32);
	bzrtpCrypto_DestroyDHMContext(DHMcContext);

	/* destroy the RNG context */
	bzrtpCrypto_destroyRNG(RNGcontext);
}

/**
 *
 * Tests for DHM-2048: just performe a key exchange and check that generated secrets are the same
 *
 */
void test_dhm3072(void) {
	bzrtpRNGContext_t *RNGcontext;
	bzrtpDHMContext_t *DHMaContext;
	bzrtpDHMContext_t *DHMbContext;
	bzrtpDHMContext_t *DHMcContext;

	/* start the Random Number Generator */
	RNGcontext = bzrtpCrypto_startRNG((uint8_t *)"36 15 Grouiik vous souhaite une bonne journee", 46);

	/* Create the context for Alice */
	DHMaContext = bzrtpCrypto_CreateDHMContext(ZRTP_KEYAGREEMENT_DH3k, 32);

	/* Create the public value for Alice G^Xa mod P */
	bzrtpCrypto_DHMCreatePublic(DHMaContext, (int (*)(void *, uint8_t *, size_t))bzrtpCrypto_getRandom, (void *)RNGcontext);

	/* Create the context for Bob */
	DHMbContext = bzrtpCrypto_CreateDHMContext(ZRTP_KEYAGREEMENT_DH3k, 32);

	/* Create the public value for Bob G^Xb mod P */
	bzrtpCrypto_DHMCreatePublic(DHMbContext, (int (*)(void *, uint8_t *, size_t))bzrtpCrypto_getRandom, (void *)RNGcontext);

	/* exchange public values */
	DHMaContext->peer = (uint8_t *)malloc(DHMaContext->primeLength*sizeof(uint8_t));
	DHMbContext->peer = (uint8_t *)malloc(DHMbContext->primeLength*sizeof(uint8_t));
	memcpy (DHMaContext->peer, DHMbContext->self, DHMaContext->primeLength*sizeof(uint8_t));
	memcpy (DHMbContext->peer, DHMaContext->self, DHMbContext->primeLength*sizeof(uint8_t));

	/* compute secret key */
	bzrtpCrypto_DHMComputeSecret(DHMaContext, (int (*)(void *, uint8_t *, size_t))bzrtpCrypto_getRandom, (void *)RNGcontext);
	bzrtpCrypto_DHMComputeSecret(DHMbContext, (int (*)(void *, uint8_t *, size_t))bzrtpCrypto_getRandom, (void *)RNGcontext);

	CU_ASSERT_TRUE(memcmp(DHMaContext->key, DHMbContext->key, 384) == 0); /* generated key shall be 384 bytes long */

	/* free context */
	bzrtpCrypto_DestroyDHMContext(DHMaContext);
	bzrtpCrypto_DestroyDHMContext(DHMbContext);

	/* create an unused context and destroy it to check the correct implementation of the create/destroy functions */
	DHMcContext = bzrtpCrypto_CreateDHMContext(ZRTP_KEYAGREEMENT_DH3k, 32);
	bzrtpCrypto_DestroyDHMContext(DHMcContext);

	/* destroy the RNG context */
	bzrtpCrypto_destroyRNG(RNGcontext);
}



/**
 *
 * Patterns generated using GNU ZRTP C++
*/
#define KDF_TEST_NUMBER 9
uint8_t patternKDFLabel[KDF_TEST_NUMBER][28] = {
	"Responder SRTP master key", "Responder SRTP master salt", "SAS", "ZRTP Session Key", "retained secret", "Responder ZRTP key", "Initiator ZRTP key", "Responder HMAC key", "Initiator HMAC key"};

uint16_t patternKDFContextLength[KDF_TEST_NUMBER] = {56, 56, 56 , 56, 56, 56, 56, 56, 56};

uint16_t patternKDFHmacLength[KDF_TEST_NUMBER] = {16, 14, 32, 32, 32, 16, 16, 32, 32};

uint8_t patternKDFOutput[KDF_TEST_NUMBER][32] = {
	{0xf2, 0xcf, 0xc5, 0x72, 0x4a, 0xcf, 0xd7, 0xb5, 0x0e, 0x89, 0xbe, 0xd8, 0xa5, 0xc9, 0xf2, 0xcd},
	{0x3c, 0x8f, 0xb1, 0x3f, 0xd6, 0xca, 0x83, 0xfb, 0xd6, 0xaf, 0x3b, 0xd5, 0xab, 0x1c},
	{0xd2, 0x69, 0xb0, 0x71, 0x8a, 0xd5, 0x8f, 0x3c, 0x8e, 0x0d, 0xcf, 0x45, 0x35, 0x81, 0x1d, 0x95, 0x18, 0x47, 0x31, 0x41, 0x28, 0x31, 0x9f, 0x54, 0xc6, 0x06, 0x59, 0x5b, 0x52, 0x2c, 0x52, 0x87},
	{0xc3, 0x63, 0x39, 0x0b, 0xc9, 0xee, 0x13, 0xaa, 0x9e, 0x6a, 0x00, 0x52, 0x84, 0x09, 0xdd, 0xc9, 0x07, 0x17, 0xe0, 0x1d, 0x1a, 0xe2, 0xdc, 0xf9, 0xa7, 0x0a, 0xb9, 0x88, 0x86, 0xac, 0x69, 0x75},
	{0xa7, 0x27, 0x2d, 0xee, 0x77, 0xe7, 0x99, 0x0c, 0x24, 0xb2, 0x0b, 0x9f, 0x54, 0xba, 0x2b, 0x61, 0x7a, 0x23, 0xd8, 0xa5, 0x97, 0x65, 0x82, 0x9b, 0x78, 0x07, 0x2b, 0xc5, 0x12, 0x99, 0x01, 0x5c},
	{0x75, 0x76, 0x7d, 0x41, 0x82, 0x9c, 0xb6, 0x97, 0xc7, 0xa2, 0x0d, 0x68, 0x12, 0xea, 0xdb, 0x9a},
	{0x08, 0xa8, 0x21, 0x46, 0x0c, 0xbd, 0x6c, 0xe0, 0xf1, 0xfe, 0x3c, 0x4c, 0x70, 0x4e, 0x3b, 0xea},
	{0x21, 0x69, 0xf1, 0x5c, 0x1b, 0x85, 0xa2, 0x39, 0x77, 0x02, 0x4d, 0x5e, 0xa7, 0x1f, 0x48, 0x16, 0x23, 0xfe, 0x94, 0xdc, 0x1a, 0xab, 0xf4, 0x89, 0xb8, 0x74, 0x4d, 0x0d, 0x37, 0x3d, 0x7c, 0xab},
	{0x1d, 0x11, 0x7b, 0xef, 0xd6, 0x27, 0xcc, 0x5a, 0x80, 0xd0, 0x23, 0x6e, 0x8d, 0x49, 0x6b, 0xe4, 0x2a, 0x6a, 0x01, 0x18, 0xac, 0x41, 0x59, 0xe8, 0x61, 0xb9, 0xb1, 0x4a, 0x78, 0x4b, 0x5d, 0x3f}
};

uint8_t patternKDFContext[KDF_TEST_NUMBER][56] = {
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f}
};

void test_zrtpKDF(void) {

	int i;
	uint8_t keyKDF[32] = {0x33, 0xe6, 0x6c, 0x01, 0xca, 0x6f, 0xe6, 0x4f, 0xb7, 0x6f, 0xfd, 0xe3, 0x1c, 0xab, 0xc0, 0xfb, 0xad, 0x3d, 0x31, 0x02, 0x67, 0x6b, 0x0c, 0x09, 0x0f, 0xc9, 0x96, 0x38, 0x1e, 0x0a, 0x8c, 0x2f};
	uint8_t output[32];

	for (i=0; i<KDF_TEST_NUMBER; i++) {
		bzrtp_keyDerivationFunction(keyKDF, 32,
			patternKDFLabel[i], strlen((char *)patternKDFLabel[i]),
			patternKDFContext[i], patternKDFContextLength[i],
			patternKDFHmacLength[i],
			(void (*)(uint8_t *, uint8_t, uint8_t *, uint32_t, uint8_t, uint8_t *))bzrtpCrypto_hmacSha256,
			output);

		CU_ASSERT_TRUE(memcmp(output, patternKDFOutput[i], patternKDFHmacLength[i]) == 0);
	}

}

#define CRC_TEST_NUMBER 3
uint16_t patternCRCLength[CRC_TEST_NUMBER] = {128, 24, 480};

uint8_t patterCRCinput[CRC_TEST_NUMBER][480] = {
 {0x10, 0x00, 0x18, 0xca, 0x5a, 0x52, 0x54, 0x50, 0x63, 0x67, 0x4b, 0xfb, 0x50, 0x5a, 0x00, 0x1d, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x20, 0x20, 0x31, 0x2e, 0x31, 0x30, 0x4c, 0x49, 0x4e, 0x50, 0x48, 0x4f, 0x4e, 0x45, 0x2d, 0x5a, 0x52, 0x54, 0x50, 0x43, 0x50, 0x50, 0x5d, 0x9c, 0x1a, 0x31, 0xfa, 0x1d, 0xe7, 0xfd, 0x0a, 0x90, 0x33, 0x92, 0x3a, 0x7b, 0xe5, 0xfb, 0xf9, 0xa3, 0xd3, 0x23, 0x28, 0x6e, 0xfc, 0x72, 0x49, 0xeb, 0x7b, 0x71, 0x3e, 0x06, 0x7c, 0xc9, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x00, 0x01, 0x12, 0x21, 0x53, 0x32, 0x35, 0x36, 0x41, 0x45, 0x53, 0x31, 0x48, 0x53, 0x33, 0x32, 0x48, 0x53, 0x38, 0x30, 0x44, 0x48, 0x33, 0x6b, 0x4d, 0x75, 0x6c, 0x74, 0x42, 0x33, 0x32, 0x20, 0x80, 0x47, 0x37, 0x13, 0xdc, 0xd6, 0xef, 0x54},
 {0x10, 0x00, 0x18, 0xcd, 0x5a, 0x52, 0x54, 0x50, 0x63, 0x67, 0x4b, 0xfb, 0x50, 0x5a, 0x00, 0x03, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x41, 0x43, 0x4b}, 
 {0x10, 0x00, 0x18, 0xcf, 0x5a, 0x52, 0x54, 0x50, 0x63, 0x67, 0x4b, 0xfb, 0x50, 0x5a, 0x00, 0x75, 0x44, 0x48, 0x50, 0x61, 0x72, 0x74, 0x31, 0x20, 0xda, 0x39, 0xde, 0xa2, 0x99, 0xf2, 0x9e, 0x17, 0x7b, 0xd2, 0x5e, 0x16, 0x21, 0xfb, 0xf3, 0xe7, 0x8f, 0xa7, 0xdf, 0x0d, 0x18, 0x79, 0x86, 0x45, 0x2d, 0x9c, 0xf2, 0x16, 0x7f, 0x09, 0x98, 0x11, 0x3a, 0xd1, 0xeb, 0xf1, 0x85, 0xa7, 0x2a, 0x85, 0xfe, 0x41, 0x5f, 0x50, 0x80, 0x5a, 0xaa, 0x39, 0x27, 0x89, 0xc0, 0x58, 0x68, 0x1c, 0x56, 0xae, 0x36, 0xa6, 0xee, 0x54, 0x06, 0x2c, 0x06, 0x76, 0x7c, 0x8d, 0xe0, 0xf7, 0x66, 0x82, 0x85, 0xb6, 0x3a, 0x2e, 0xd1, 0x98, 0x9d, 0xd9, 0xf1, 0x89, 0x67, 0xac, 0xe2, 0xe6, 0x54, 0x50, 0x7e, 0x24, 0xdd, 0x9b, 0xa3, 0x0d, 0xbf, 0xe8, 0x65, 0xd3, 0xe3, 0x4b, 0x76, 0x2b, 0xec, 0x28, 0x42, 0xf7, 0x09, 0x35, 0x79, 0x42, 0xb5, 0x25, 0x57, 0x2b, 0x53, 0x71, 0xe1, 0xca, 0x40, 0x25, 0xa9, 0x98, 0x21, 0x3b, 0xfb, 0xbd, 0x14, 0xf7, 0xb9, 0xbb, 0x5c, 0x51, 0xca, 0xdc, 0xe2, 0xe6, 0xf7, 0x19, 0x69, 0xe4, 0x48, 0x93, 0x73, 0x06, 0x17, 0x0a, 0x04, 0x6a, 0x50, 0xa1, 0x51, 0xd5, 0x73, 0xf6, 0xae, 0xbb, 0xaa, 0xb5, 0xd1, 0x40, 0x5a, 0x8e, 0x28, 0x15, 0x02, 0xff, 0xd6, 0xa2, 0x0f, 0x64, 0x8e, 0xed, 0x74, 0x7f, 0x4d, 0xdc, 0x93, 0x0e, 0x87, 0xfe, 0x9e, 0x31, 0x86, 0xe3, 0x30, 0x88, 0x67, 0x1f, 0x68, 0x61, 0xb7, 0x26, 0x34, 0x14, 0xb1, 0xf5, 0x97, 0x99, 0x53, 0xeb, 0xf1, 0x55, 0x1b, 0xf4, 0xa6, 0xe9, 0x21, 0xdc, 0x2b, 0xc4, 0x1c, 0x26, 0x7e, 0x86, 0x11, 0x80, 0xe3, 0xa2, 0xd8, 0x2a, 0x64, 0x4f, 0x1b, 0x45, 0xf8, 0x90, 0x5a, 0x30, 0x2a, 0xc8, 0x13, 0xb9, 0xd7, 0x2b, 0x32, 0x56, 0xc9, 0xea, 0x9f, 0xef, 0xbb, 0xf7, 0xc3, 0x43, 0x9e, 0x27, 0x12, 0xbd, 0xec, 0x1e, 0x61, 0x1d, 0xc6, 0x58, 0xf3, 0x77, 0x2a, 0x27, 0x9a, 0xb7, 0xf1, 0x1e, 0x15, 0x2b, 0x0d, 0x67, 0x06, 0x07, 0x65, 0x6d, 0x89, 0x60, 0x29, 0xa1, 0x49, 0x25, 0xd4, 0x50, 0x6e, 0xfe, 0xe2, 0xa8, 0x8e, 0x59, 0xf9, 0xb3, 0x5f, 0x98, 0x0d, 0x29, 0x87, 0xd3, 0x87, 0x45, 0x35, 0xa4, 0x3f, 0x87, 0x5f, 0x6f, 0xd0, 0x4a, 0xd2, 0xf9, 0x37, 0x68, 0x7e, 0xc6, 0xaa, 0x57, 0x3e, 0x71, 0xaf, 0xb4, 0x48, 0xea, 0x0e, 0x7e, 0x46, 0xf9, 0xdb, 0xd3, 0xb9, 0x26, 0xc4, 0xe2, 0x50, 0x34, 0x87, 0x0a, 0xb1, 0x0d, 0x92, 0x01, 0x0a, 0xb1, 0xb0, 0xd5, 0xd9, 0xb7, 0x74, 0x01, 0x2e, 0x0f, 0xab, 0xc8, 0x4a, 0x57, 0x0e, 0x94, 0x2c, 0x2d, 0x32, 0x7f, 0x67, 0x89, 0xa2, 0xc5, 0xa2, 0xb3, 0xd3, 0x68, 0x76, 0x3d, 0xe8, 0x96, 0x34, 0xa9, 0x96, 0x4f, 0x66, 0xc0, 0x10, 0x3c, 0xf6, 0x4d, 0xaf, 0xc8, 0xd3, 0xbd, 0x38, 0x6f, 0x52, 0x0f, 0x1b, 0xa9, 0x0e, 0x92, 0x46, 0x0d, 0xba, 0x40, 0x1a, 0xec, 0x68, 0x80, 0xec, 0xa0, 0xfe, 0x1a, 0x9b, 0xa5, 0xd8, 0x54, 0x85, 0x15, 0x9e, 0x64, 0x1c, 0x5e, 0xd8, 0x97, 0xc2, 0x4c, 0x4f, 0xf7, 0xe9, 0x09, 0x3c, 0x6d, 0xc3, 0x8c, 0x1b, 0x36, 0xd0, 0x56, 0xd2, 0x77, 0xcd, 0xaa, 0xcf, 0x5c, 0xff, 0xdf, 0x23, 0x93, 0xb5, 0xdc, 0x08, 0x95, 0x97, 0xdb, 0x68, 0x2d, 0x4b, 0x01, 0x89, 0xb2, 0x8b, 0xd0, 0x25, 0x19, 0xab, 0xbf, 0x49, 0xcf, 0xa5, 0x27} 
};

uint32_t patternCRCoutput[CRC_TEST_NUMBER] = {0x5065ab04, 0x9c9edccd, 0xda5c92ea};


/**
 *
 * Pattern generated using GNU-ZRTP C++
 *
 */
void test_CRC32(void) {
	int i;
	for (i=0; i<CRC_TEST_NUMBER; i++) {
		CU_ASSERT_TRUE(bzrtp_CRC32(patterCRCinput[i], patternCRCLength[i]) == patternCRCoutput[i]);
	}
}

static void setHelloMessageAlgo(bzrtpHelloMessage_t *helloMessage, uint8_t algoType, uint8_t types[7], const uint8_t typesCount) {
	switch(algoType) {
		case ZRTP_HASH_TYPE:
			memcpy(helloMessage->supportedHash, types, typesCount);
			helloMessage->hc = typesCount;
			break;
		case ZRTP_CIPHERBLOCK_TYPE:
			memcpy(helloMessage->supportedCipher, types, typesCount);
			helloMessage->cc = typesCount;
			break;
		case ZRTP_AUTHTAG_TYPE:
			memcpy(helloMessage->supportedAuthTag, types, typesCount);
			helloMessage->ac = typesCount;
			break;
		case ZRTP_KEYAGREEMENT_TYPE:
			memcpy(helloMessage->supportedKeyAgreement, types, typesCount);
			helloMessage->kc = typesCount;
			break;
		case ZRTP_SAS_TYPE:
			memcpy(helloMessage->supportedSas, types, typesCount);
			helloMessage->sc = typesCount;
			break;
	}
}

static int compareAllAlgoTypes(bzrtpChannelContext_t *zrtpChannelContext, uint8_t expectedHashAlgo, uint8_t expectedCipherAlgo, uint8_t expectedAuthTagAlgo, uint8_t expectedKeyAgreementAlgo, uint8_t expectedSasAlgo) {
	return
			zrtpChannelContext->hashAlgo         == expectedHashAlgo &&
			zrtpChannelContext->cipherAlgo       == expectedCipherAlgo &&
			zrtpChannelContext->authTagAlgo      == expectedAuthTagAlgo &&
			zrtpChannelContext->keyAgreementAlgo == expectedKeyAgreementAlgo &&
			zrtpChannelContext->sasAlgo          == expectedSasAlgo;
}

static int compareAllAlgoTypesWithExpectedChangedOnly(bzrtpChannelContext_t *zrtpChannelContext, uint8_t expectedAlgoType, uint8_t expectedType) {
	switch(expectedAlgoType) {
		case ZRTP_HASH_TYPE:
			return compareAllAlgoTypes(zrtpChannelContext, expectedType,   ZRTP_CIPHER_AES1, ZRTP_AUTHTAG_HS32, ZRTP_KEYAGREEMENT_DH3k, ZRTP_SAS_B32);
		case ZRTP_CIPHERBLOCK_TYPE:
			return compareAllAlgoTypes(zrtpChannelContext, ZRTP_HASH_S256, expectedType,     ZRTP_AUTHTAG_HS32, ZRTP_KEYAGREEMENT_DH3k, ZRTP_SAS_B32);
		case ZRTP_AUTHTAG_TYPE:
			return compareAllAlgoTypes(zrtpChannelContext, ZRTP_HASH_S256, ZRTP_CIPHER_AES1, expectedType,      ZRTP_KEYAGREEMENT_DH3k, ZRTP_SAS_B32);
		case ZRTP_KEYAGREEMENT_TYPE:
			return compareAllAlgoTypes(zrtpChannelContext, ZRTP_HASH_S256, ZRTP_CIPHER_AES1, ZRTP_AUTHTAG_HS32, expectedType,           ZRTP_SAS_B32);
		case ZRTP_SAS_TYPE:
			return compareAllAlgoTypes(zrtpChannelContext, ZRTP_HASH_S256, ZRTP_CIPHER_AES1, ZRTP_AUTHTAG_HS32, ZRTP_KEYAGREEMENT_DH3k, expectedType);
		default:
			return compareAllAlgoTypes(zrtpChannelContext, ZRTP_HASH_S256, ZRTP_CIPHER_AES1, ZRTP_AUTHTAG_HS32, ZRTP_KEYAGREEMENT_DH3k, ZRTP_SAS_B32);
	}
}

static int testAlgoType(uint8_t algoType, uint8_t *packetTypes, uint8_t packetTypesCount, uint8_t *contextTypes, uint8_t contextTypesCount, uint8_t expectedType) {
	int retval;

	bzrtpContext_t *zrtpContext = bzrtp_createBzrtpContext(0x12345678);
	bzrtpPacket_t *helloPacket = NULL;
	if (contextTypes != NULL) {
		bzrtp_setSupportedCryptoTypes(zrtpContext, algoType, contextTypes, contextTypesCount);
	}

	helloPacket = bzrtp_createZrtpPacket(zrtpContext, zrtpContext->channelContext[0], MSGTYPE_HELLO, &retval);
	if (packetTypes != NULL) {
		bzrtpHelloMessage_t *helloMessage = (bzrtpHelloMessage_t *)helloPacket->messageData;
		setHelloMessageAlgo(helloMessage, algoType, packetTypes, packetTypesCount);
	}

	CU_ASSERT_FALSE(crypoAlgoAgreement(zrtpContext, zrtpContext->channelContext[0], helloPacket->messageData));
	retval = compareAllAlgoTypesWithExpectedChangedOnly(zrtpContext->channelContext[0], algoType, expectedType);

	bzrtp_freeZrtpPacket(helloPacket);
	bzrtp_destroyBzrtpContext(zrtpContext, 0x12345678);
	return retval;
}

static int testAlgoTypeWithPacket(uint8_t algoType, uint8_t *types, uint8_t typesCount, uint8_t expectedType) {
	return testAlgoType(algoType, types, typesCount, NULL, 0, expectedType);
}

static int testAlgoTypeWithContext(uint8_t algoType, uint8_t *types, uint8_t typesCount, uint8_t expectedType) {
	return testAlgoType(algoType, NULL, 0, types, typesCount, expectedType);
}

struct st_algo_type_with_packet {
	uint8_t types[7];
	uint8_t typesCount;
	uint8_t expectedType;
};

struct st_algo_type_with_context {
	uint8_t types[7];
	uint8_t typesCount;
	uint8_t expectedType;
};

struct st_algo_type {
	uint8_t packetTypes[7];
	uint8_t packetTypesCount;
	uint8_t contextTypes[7];
	uint8_t contextTypesCount;
	uint8_t expectedType;
};

void test_algoAgreement(void) {
	struct st_algo_type_with_packet agreement_types_with_packet[] = {
		{ {ZRTP_KEYAGREEMENT_DH2k}, 1, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_DH3k}, 1, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k}, 2, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_EC52}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_DH3k}, 3, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52}, 4, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type_with_packet *agreement_type_with_packet;
	struct st_algo_type_with_context agreement_types_with_context[] = {
		{ {ZRTP_KEYAGREEMENT_DH2k}, 1, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_DH3k}, 1, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_EC25}, 1, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k}, 2, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_EC52}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25}, 2, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_DH3k}, 3, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52}, 4, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type_with_context *agreement_type_with_context;
	struct st_algo_type agreement_types[] = {
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52}, 4, {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52}, 4, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type *agreement_type;
	struct st_algo_type_with_packet cipher_types_with_packet[] = {
		{ {ZRTP_CIPHER_AES1}, 1, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES3}, 1, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 3, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES3}, 6, ZRTP_CIPHER_AES1 },
		{ {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type_with_packet *cipher_type_with_packet;
	struct st_algo_type_with_context cipher_types_with_context[] = {
		{ {ZRTP_CIPHER_AES1}, 1, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES3}, 1, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS1}, 1, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 3, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1}, 3, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES3}, 6, ZRTP_CIPHER_AES1 },
		{ {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type_with_context *cipher_type_with_context;
	struct st_algo_type cipher_types[] = {
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES3}, 6, ZRTP_CIPHER_AES1 },
		{ {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type *cipher_type;

	/* check defaults */
	CU_TEST(testAlgoTypeWithPacket(ZRTP_UNSET_ALGO, NULL, 0, ZRTP_KEYAGREEMENT_DH3k));

	/* key agreement type */
	agreement_type_with_packet = &agreement_types_with_packet[0];
	while (agreement_type_with_packet->typesCount > 0) {
		CU_TEST(testAlgoTypeWithPacket(ZRTP_KEYAGREEMENT_TYPE, agreement_type_with_packet->types, agreement_type_with_packet->typesCount, agreement_type_with_packet->expectedType));
	}
	agreement_type_with_context = &agreement_types_with_context[0];
	while (agreement_type_with_context->typesCount > 0) {
		CU_TEST(testAlgoTypeWithContext(ZRTP_KEYAGREEMENT_TYPE, agreement_type_with_context->types, agreement_type_with_context->typesCount, agreement_type_with_context->expectedType));
	}
	agreement_type = &agreement_types[0];
	while (agreement_type->packetTypesCount > 0) {
		CU_TEST(testAlgoType(ZRTP_KEYAGREEMENT_TYPE, agreement_type->packetTypes, agreement_type->packetTypesCount, agreement_type->contextTypes, agreement_type->contextTypesCount, agreement_type->expectedType));
	}

	/* cipher type */
	cipher_type_with_packet = &cipher_types_with_packet[0];
	while (cipher_type_with_packet->typesCount > 0) {
		CU_TEST(testAlgoTypeWithPacket(ZRTP_CIPHERBLOCK_TYPE, cipher_type_with_packet->types, cipher_type_with_packet->typesCount, cipher_type_with_packet->expectedType));
	}
	cipher_type_with_context = &cipher_types_with_context[0];
	while (cipher_type_with_context->typesCount > 0) {
		CU_TEST(testAlgoTypeWithContext(ZRTP_CIPHERBLOCK_TYPE, cipher_type_with_context->types, cipher_type_with_context->typesCount, cipher_type_with_context->expectedType));
	}
	cipher_type = &cipher_types[0];
	while (cipher_type->packetTypesCount > 0) {
		CU_TEST(testAlgoType(ZRTP_CIPHERBLOCK_TYPE, cipher_type->packetTypes, cipher_type->packetTypesCount, cipher_type->contextTypes, cipher_type->contextTypesCount, cipher_type->expectedType));
	}
}

static int compareAlgoTypes(uint8_t actualTypes[7], uint8_t actualTypesCount, uint8_t expectedTypes[7], uint8_t expectedTypesCount) {
	int i;

	if (actualTypesCount != expectedTypesCount) {
		return 0;
	}

	for (i=0; i<actualTypesCount; i++) {
		if (actualTypes[i] != expectedTypes[i]) {
			return 0;
		}
	}

	return 1;
}

static int testAlgoSetterGetter(uint8_t algoType, uint8_t *contextTypes, uint8_t contextTypesCount, uint8_t *expectedTypes, uint8_t expectedTypesCount) {
	int retval;
	uint8_t compareTypes[8];
	uint8_t compareTypesCount;

	bzrtpContext_t *zrtpContext = bzrtp_createBzrtpContext(0x12345678);
	bzrtp_setSupportedCryptoTypes(zrtpContext, algoType, contextTypes, contextTypesCount);
	compareTypesCount = bzrtp_getSupportedCryptoTypes(zrtpContext, algoType, compareTypes);
	retval = compareAlgoTypes(compareTypes, compareTypesCount, expectedTypes, expectedTypesCount);
	bzrtp_destroyBzrtpContext(zrtpContext, 0x12345678);
	return retval;
}

struct st_algo_setter_getter {
	uint8_t contextTypes[8];
	uint8_t contextTypesCount;
	uint8_t expectedTypes[8];
	uint8_t expectedTypesCount;
};

void test_algoSetterGetter(void) {
	struct st_algo_setter_getter agreement_types[] = {
		{ {ZRTP_KEYAGREEMENT_DH2k}, 1, {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2 },
		{ {ZRTP_KEYAGREEMENT_DH3k}, 1, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ {ZRTP_KEYAGREEMENT_EC25}, 1, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ {ZRTP_KEYAGREEMENT_EC38}, 1, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ {ZRTP_KEYAGREEMENT_EC52}, 1, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ {ZRTP_KEYAGREEMENT_Prsh}, 1, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ {ZRTP_KEYAGREEMENT_Mult}, 1, {ZRTP_KEYAGREEMENT_Mult, ZRTP_KEYAGREEMENT_DH3k}, 2 },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2 },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2, {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2 },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k}, 2, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_EC25}, 2, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k}, 2, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52}, 2, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2 },
		{ {ZRTP_KEYAGREEMENT_Prsh, ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC38, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 6, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2 },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2 },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k}, 8, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, }
	};
	struct st_algo_setter_getter *agreement_type;
	struct st_algo_setter_getter cipher_types[] = {
		{ {ZRTP_CIPHER_AES1}, 1, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_AES2}, 1, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_AES3}, 1, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2 },
		{ {ZRTP_CIPHER_2FS1}, 1, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_2FS2}, 1, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_2FS3}, 1, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES3}, 2, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2, {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES2}, 2, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_2FS3}, 2, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_AES2, ZRTP_CIPHER_2FS3}, 2, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2 },
		{ {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, }
	};
	struct st_algo_setter_getter *cipher_type;

	/* key agreement type */
	agreement_type = &agreement_types[0];
	while (agreement_type->contextTypesCount > 0) {
		CU_TEST(testAlgoSetterGetter(ZRTP_KEYAGREEMENT_TYPE, agreement_type->contextTypes, agreement_type->contextTypesCount, agreement_type->expectedTypes, agreement_type->expectedTypesCount));
	}

	/* cipher type */
	cipher_type = &cipher_types[0];
	while (cipher_type->contextTypesCount > 0) {
		CU_TEST(testAlgoSetterGetter(ZRTP_CIPHERBLOCK_TYPE, cipher_type->contextTypes, cipher_type->contextTypesCount, cipher_type->expectedTypes, cipher_type->expectedTypesCount));
	}
}

#define ZRTP_AUTHTAG_FAKE_1		0x41
#define ZRTP_AUTHTAG_FAKE_2		0x42
#define ZRTP_AUTHTAG_FAKE_3		0x43
#define ZRTP_AUTHTAG_FAKE_4		0x44
#define ZRTP_AUTHTAG_FAKE_5		0x45

static int testAddMandatoryCryptoTypesIfNeeded(uint8_t algoType, uint8_t *algoTypes, uint8_t algoTypesCount, uint8_t *expectedTypes, uint8_t expectedTypesCount) {
	addMandatoryCryptoTypesIfNeeded(algoType, algoTypes, &algoTypesCount);
	return compareAlgoTypes(algoTypes, algoTypesCount, expectedTypes, expectedTypesCount);
}

struct st_add_crypto {
	uint8_t algoType;
	uint8_t algoTypes[7];
	uint8_t algoTypesCount;
	uint8_t expectedTypes[7];
	uint8_t expectedTypesCount;
};

void test_addMandatoryCryptoTypesIfNeeded(void) {
	struct st_add_crypto crypto_types[] = {
		/* mandatory types */
		{ ZRTP_HASH_TYPE, {ZRTP_UNSET_ALGO}, 0, {ZRTP_HASH_S256}, 1 },
		{ ZRTP_CIPHERBLOCK_TYPE, {ZRTP_UNSET_ALGO}, 0, {ZRTP_CIPHER_AES1}, 1 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_UNSET_ALGO}, 0, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 2 },
		{ ZRTP_KEYAGREEMENT_TYPE, {ZRTP_UNSET_ALGO}, 0, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ ZRTP_SAS_TYPE, {ZRTP_UNSET_ALGO}, 0, {ZRTP_SAS_B32}, 1 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 2, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 2 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32}, 2, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32}, 2 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK64}, 2, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_HS32}, 3 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64}, 2, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 4 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3}, 5, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS80}, 6, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32}, 7 },
		/* overrride */
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4, ZRTP_AUTHTAG_FAKE_5}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4, ZRTP_AUTHTAG_HS80}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32}, 7 },
		/* uniqueness */
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64}, 4, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 4 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS32}, 4, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 2 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 4, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 2 },
		{ 0, {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, }
	};
	struct st_add_crypto *crypto_type;

	crypto_type = &crypto_types[0];
	while (crypto_type->algoTypesCount > 0) {
		CU_TEST(testAddMandatoryCryptoTypesIfNeeded(crypto_type->algoType, crypto_type->algoTypes, crypto_type->algoTypesCount, crypto_type->expectedTypes, crypto_type->expectedTypesCount));
	}
}
