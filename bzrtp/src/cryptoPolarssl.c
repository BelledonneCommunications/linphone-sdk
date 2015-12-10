/**
 @file cryptoPolarssl.c

 @brief bind zrtpCrypto functions to their polarSSL implementation

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

/* Random number generator */
#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"

/* Hash function sha1(sha2 is different for polarssl v1.2 and v1.3 ) */
#include "polarssl/sha1.h"

/* Asymmetrics encryption */
#include "polarssl/dhm.h"

/* Symmetric encryption */
#include "polarssl/aes.h"

#include "cryptoWrapper.h"

/*** Common functions to polarSSL version 1.2 and 1.3 ***/

/** Return available crypto functions. For now we have
 *
 * - Hash: HMAC-SHA256(Mandatory)
 * - CipherBlock: AES128(Mandatory)
 * - Auth Tag: HMAC-SHA132 and HMAC-SHA180 (These are mandatory for SRTP and depends on the SRTP implementation thus we can just suppose they are both available)
 * - Key Agreement: DHM3k(Mandatory), DHM2k(optional and shall not be used except on low power devices)
 * - Sas: base32(Mandatory), b256(pgp words)
 */
uint8_t bzrtpCrypto_getAvailableCryptoTypes(uint8_t algoType, uint8_t availableTypes[7]) {

	switch(algoType) {
		case ZRTP_HASH_TYPE:
			availableTypes[0] = ZRTP_HASH_S256;
			return 1;
			break;
		case ZRTP_CIPHERBLOCK_TYPE:
			availableTypes[0] = ZRTP_CIPHER_AES1;
			availableTypes[1] = ZRTP_CIPHER_AES3;
			return 2;
			break;
		case ZRTP_AUTHTAG_TYPE:
			availableTypes[0] = ZRTP_AUTHTAG_HS32;
			availableTypes[1] = ZRTP_AUTHTAG_HS80;
			return 2;
			break;
		case ZRTP_KEYAGREEMENT_TYPE:
			availableTypes[0] = ZRTP_KEYAGREEMENT_DH3k;
			availableTypes[1] = ZRTP_KEYAGREEMENT_DH2k;
			availableTypes[2] = ZRTP_KEYAGREEMENT_Mult; /* This one shall always be at the end of the list, it is just to inform the peer ZRTP endpoint that we support the Multichannel ZRTP */
			return 3;
			break;
		case ZRTP_SAS_TYPE: /* the SAS function is implemented in cryptoUtils.c and then is not directly linked to the polarSSL crypto wrapper */
			availableTypes[0] = ZRTP_SAS_B32;
			availableTypes[1] = ZRTP_SAS_B256;
			return 2;
			break;
		default:
			return 0;
	}
}

/** Return mandatory crypto functions. For now we have
 *
 * - Hash: HMAC-SHA256
 * - CipherBlock: AES128
 * - Auth Tag: HMAC-SHA132 and HMAC-SHA180
 * - Key Agreement: DHM3k
 * - Sas: base32
 */
uint8_t bzrtpCrypto_getMandatoryCryptoTypes(uint8_t algoType, uint8_t mandatoryTypes[7]) {

	switch(algoType) {
		case ZRTP_HASH_TYPE:
			mandatoryTypes[0] = ZRTP_HASH_S256;
			return 1;
		case ZRTP_CIPHERBLOCK_TYPE:
			mandatoryTypes[0] = ZRTP_CIPHER_AES1;
			return 1;
		case ZRTP_AUTHTAG_TYPE:
			mandatoryTypes[0] = ZRTP_AUTHTAG_HS32;
			mandatoryTypes[1] = ZRTP_AUTHTAG_HS80;
			return 2;
		case ZRTP_KEYAGREEMENT_TYPE:
			mandatoryTypes[0] = ZRTP_KEYAGREEMENT_DH3k;
			mandatoryTypes[1] = ZRTP_KEYAGREEMENT_Mult; /* we must add this one if we want to be able to make multistream */
			return 2;
		case ZRTP_SAS_TYPE:
			mandatoryTypes[0] = ZRTP_SAS_B32;
			return 1;
		default:
			return 0;
	}
}

/**
 *
 * @brief Structure to store all the contexts data needed for Random Number Generation
 *
 */
typedef struct polarsslRNGContext_struct {
	entropy_context entropyContext; /**< the entropy function context */
	ctr_drbg_context rngContext; /**< the random number generator context */
} polarsslRNGContext_t;

bzrtpRNGContext_t *bzrtpCrypto_startRNG(const uint8_t *entropyString, uint16_t entropyStringLength) {
	bzrtpRNGContext_t *context;

	/* create the polarssl context, it contains entropy and rng contexts */
	polarsslRNGContext_t *polarsslContext = (polarsslRNGContext_t *)malloc(sizeof(polarsslRNGContext_t));
	
	entropy_init(&(polarsslContext->entropyContext)); /* init the polarssl entropy engine */
	/* init the polarssl rng context */
	if (ctr_drbg_init(&(polarsslContext->rngContext), entropy_func, &(polarsslContext->entropyContext), (const unsigned char *)entropyString, entropyStringLength) != 0) {
		return NULL;
	}

	/* create the context and attach the polarssl one in it's unique field */
	context = (bzrtpRNGContext_t *)malloc(sizeof(bzrtpRNGContext_t));

	context->cryptoModuleData = (void *)polarsslContext;

	return context;
}

int bzrtpCrypto_getRandom(bzrtpRNGContext_t *context, uint8_t *output, size_t outputLength) {
	/* get polarssl context data */
	polarsslRNGContext_t *polarsslContext = (polarsslRNGContext_t *)context->cryptoModuleData;
	return ctr_drbg_random((void *)&(polarsslContext->rngContext), (unsigned char *)output, outputLength);
}

int bzrtpCrypto_destroyRNG(bzrtpRNGContext_t *context) {
	/* get polarssl context data */
	polarsslRNGContext_t *polarsslContext = (polarsslRNGContext_t *)context->cryptoModuleData;

	/* free the entropy context (ctr_drbg doesn't seem to need to be freed)*/
	/*entropy_free(polarsslContext->entropyContext);*/

	/* free the context structures */
	free(polarsslContext);
	free(context);

	return 0;
}

/*
 * @brief HMAC-SHA1 wrapper
 * @param[in] 	key			HMAC secret key
 * @param[in] 	keyLength	HMAC key length
 * @param[in]	input 		Input data buffer
 * @param[in]   inputLength	Input data length
 * @param[in]	hmacLength	Length of output required in bytes, HMAC output is truncated to the hmacLength left bytes. 20 bytes maximum
 * @param[out]	output		Output data buffer.
 *
 */
void bzrtpCrypto_hmacSha1(const uint8_t *key,
		size_t keyLength,
		const uint8_t *input,
		size_t inputLength,
		uint8_t hmacLength,
		uint8_t *output)
{
	uint8_t hmacOutput[20];
	sha1_hmac(key, keyLength, input, inputLength, hmacOutput);

	/* check output length, can't be>20 */
	if (hmacLength>20) {
		memcpy(output, hmacOutput, 20);
	} else {
		memcpy(output, hmacOutput, hmacLength);
	}
}

/* initialise de DHM context according to requested algorithm */
bzrtpDHMContext_t *bzrtpCrypto_CreateDHMContext(uint8_t DHMAlgo, uint8_t secretLength)
{
	dhm_context *polarsslDhmContext;

	/* create the context */
	bzrtpDHMContext_t *context = (bzrtpDHMContext_t *)malloc(sizeof(bzrtpDHMContext_t));
	memset (context, 0, sizeof(bzrtpDHMContext_t));

	/* create the polarssl context for DHM */
	polarsslDhmContext=(dhm_context *)malloc(sizeof(dhm_context));
	memset(polarsslDhmContext, 0, sizeof(dhm_context));
	context->cryptoModuleData=(void *)polarsslDhmContext;

	/* initialise pointer to NULL to ensure safe call to free() when destroying context */
	context->secret = NULL;
	context->self = NULL;
	context->key = NULL;
	context->peer = NULL;

	/* set parameters in the context */
	context->algo=DHMAlgo;
	context->secretLength = secretLength;
	switch (DHMAlgo) {
		case ZRTP_KEYAGREEMENT_DH2k:
			/* set P and G in the polarssl context */
			if ((mpi_read_string(&(polarsslDhmContext->P), 16, POLARSSL_DHM_RFC3526_MODP_2048_P) != 0) ||
			(mpi_read_string(&(polarsslDhmContext->G), 16, POLARSSL_DHM_RFC3526_MODP_2048_G) != 0)) {
				return NULL;
			}
			context->primeLength=256;
			polarsslDhmContext->len=256;
			break;
		case ZRTP_KEYAGREEMENT_DH3k:
			/* set P and G in the polarssl context */
			if ((mpi_read_string(&(polarsslDhmContext->P), 16, POLARSSL_DHM_RFC3526_MODP_3072_P) != 0) ||
			(mpi_read_string(&(polarsslDhmContext->G), 16, POLARSSL_DHM_RFC3526_MODP_3072_G) != 0)) {
				return NULL;
			}
			context->primeLength=384;
			polarsslDhmContext->len=384;
			break;
		default:
			free(context);
			return NULL;
			break;
	}

	return context;
}

/* generate the random secret and compute the public value */
void bzrtpCrypto_DHMCreatePublic(bzrtpDHMContext_t *context, int (*rngFunction)(void *, uint8_t *, size_t), void *rngContext) {
	/* get the polarssl context */
	dhm_context *polarsslContext = (dhm_context *)context->cryptoModuleData;

	/* allocate output buffer */
	context->self = (uint8_t *)malloc(context->primeLength*sizeof(uint8_t));

	dhm_make_public(polarsslContext, context->secretLength, context->self, context->primeLength, (int (*)(void *, unsigned char *, size_t))rngFunction, rngContext);

}

/* clean DHM context */
void bzrtpCrypto_DestroyDHMContext(bzrtpDHMContext_t *context) {
	if (context!= NULL) {
		free(context->secret);
		free(context->self);
		free(context->key);
		free(context->peer);

		dhm_free((dhm_context *)context->cryptoModuleData);
		free(context->cryptoModuleData);

		free(context);
	}
}

/*
 * @brief Wrapper for AES-128 in CFB128 mode encryption
 * Both key and IV must be 16 bytes long, IV is not updated
 *
 * @param[in]	key			encryption key, 128 bits long
 * @param[in]	IV			Initialisation vector, 128 bits long, is not modified by this function.
 * @param[in]	input		Input data buffer
 * @param[in]	inputLength	Input data length
 * @param[out]	output		Output data buffer
 *
 */
void bzrtpCrypto_aes128CfbEncrypt(const uint8_t key[16],
		const uint8_t IV[16],
		const uint8_t *input,
		size_t inputLength,
		uint8_t *output)
{
	uint8_t IVbuffer[16];
	size_t iv_offset=0; /* is not used by us but needed and updated by polarssl */
	aes_context context;
	memset (&context, 0, sizeof(aes_context));

	/* make a local copy of IV which is modified by the polar ssl AES-CFB function */
	memcpy(IVbuffer, IV, 16*sizeof(uint8_t));

	/* initialise the aes context and key */
	aes_setkey_enc(&context, key, 128);

	/* encrypt */
	aes_crypt_cfb128 (&context, AES_ENCRYPT, inputLength, &iv_offset, IVbuffer, input, output);
}

/*
 * @brief Wrapper for AES-128 in CFB128 mode decryption
 * Both key and IV must be 16 bytes long, IV is not updated
 *
 * @param[in]	key			decryption key, 128 bits long
 * @param[in]	IV			Initialisation vector, 128 bits long, is not modified by this function.
 * @param[in]	input		Input data buffer
 * @param[in]	inputLength	Input data length
 * @param[out]	output		Output data buffer
 *
 */
void bzrtpCrypto_aes128CfbDecrypt(const uint8_t key[16],
		const uint8_t IV[16],
		const uint8_t *input,
		size_t inputLength,
		uint8_t *output)
{
	uint8_t IVbuffer[16];
	size_t iv_offset=0; /* is not used by us but needed and updated by polarssl */
	aes_context context;
	memset (&context, 0, sizeof(aes_context));

	/* make a local copy of IV which is modified by the polar ssl AES-CFB function */
	memcpy(IVbuffer, IV, 16*sizeof(uint8_t));

	/* initialise the aes context and key - use the aes_setkey_enc function as requested by the documentation of aes_crypt_cfb128 function */
	aes_setkey_enc(&context, key, 128);

	/* encrypt */
	aes_crypt_cfb128 (&context, AES_DECRYPT, inputLength, &iv_offset, IVbuffer, input, output);
}

/*
 * @brief Wrapper for AES-256 in CFB128 mode encryption
 * The key must be 32 bytes long and the IV must be 16 bytes long, IV is not updated
 *
 * @param[in]	key			encryption key, 256 bits long
 * @param[in]	IV			Initialisation vector, 128 bits long, is not modified by this function.
 * @param[in]	input		Input data buffer
 * @param[in]	inputLength	Input data length
 * @param[out]	output		Output data buffer
 *
 */
void bzrtpCrypto_aes256CfbEncrypt(const uint8_t key[32],
		const uint8_t IV[16],
		const uint8_t *input,
		size_t inputLength,
		uint8_t *output)
{
	uint8_t IVbuffer[16];
	size_t iv_offset=0;
	aes_context context;

	memcpy(IVbuffer, IV, 16*sizeof(uint8_t));
	memset (&context, 0, sizeof(aes_context));
	aes_setkey_enc(&context, key, 256);

	/* encrypt */
	aes_crypt_cfb128 (&context, AES_ENCRYPT, inputLength, &iv_offset, IVbuffer, input, output);
}

/*
 * @brief Wrapper for AES-256 in CFB128 mode decryption
 * The key must be 32 bytes long and the IV must be 16 bytes long, IV is not updated
 *
 * @param[in]	key			decryption key, 256 bits long
 * @param[in]	IV			Initialisation vector, 128 bits long, is not modified by this function.
 * @param[in]	input		Input data buffer
 * @param[in]	inputLength	Input data length
 * @param[out]	output		Output data buffer
 *
 */
void bzrtpCrypto_aes256CfbDecrypt(const uint8_t key[32],
		const uint8_t IV[16],
		const uint8_t *input,
		size_t inputLength,
		uint8_t *output)
{
	uint8_t IVbuffer[16];
	size_t iv_offset=0;
	aes_context context;

	memcpy(IVbuffer, IV, 16*sizeof(uint8_t));
	memset (&context, 0, sizeof(aes_context));
	aes_setkey_enc(&context, key, 256);

	/* decrypt */
	aes_crypt_cfb128 (&context, AES_DECRYPT, inputLength, &iv_offset, IVbuffer, input, output);
}

/*** End of code common to polarSSL version 1.2 and 1.3 ***/

/* check polarssl version */
#include <polarssl/version.h>
#if POLARSSL_VERSION_NUMBER >= 0x01030000 /* for Polarssl version 1.3 */

/* Hashs */
#include "polarssl/sha256.h"

/*
 * @brief SHA256 wrapper
 * @param[in]	input 		Input data buffer
 * @param[in]   inputLength	Input data length in bytes
 * @param[in]	hmacLength	Length of output required in bytes, HMAC output is truncated to the hmacLength left bytes. 32 bytes maximum
 * @param[out]	output		Output data buffer.
 *
 */
void bzrtpCrypto_sha256(const uint8_t *input,
		size_t inputLength,
		uint8_t hashLength,
		uint8_t *output)
{
	uint8_t hashOutput[32];
	sha256(input, inputLength, hashOutput, 0); /* last param to zero to select SHA256 and not SHA224 */

	/* check output length, can't be>32 */
	if (hashLength>32) {
		memcpy(output, hashOutput, 32);
	} else {
		memcpy(output, hashOutput, hashLength);
	}
}

/*
 * HMAC-SHA-256 wrapper
 * @param[in] 	key			HMAC secret key
 * @param[in] 	keyLength	HMAC key length
 * @param[in]	input 		Input data buffer
 * @param[in]   inputLength	Input data length
 * @param[in]	hmacLength	Length of output required in bytes, HMAC output is truncated to the hmacLength left bytes. 32 bytes maximum
 * @param[out]	output		Output data buffer.
 *
 */
void bzrtpCrypto_hmacSha256(const uint8_t *key,
		size_t keyLength,
		const uint8_t *input,
		size_t inputLength,
		uint8_t hmacLength,
		uint8_t *output)
{
	uint8_t hmacOutput[32];
	sha256_hmac(key, keyLength, input, inputLength, hmacOutput, 0); /* last param to zero to select SHA256 and not SHA224 */

	/* check output length, can't be>32 */
	if (hmacLength>32) {
		memcpy(output, hmacOutput, 32);
	} else {
		memcpy(output, hmacOutput, hmacLength);
	}
}

/* compute secret - the ->peer field of context must have been set before calling this function */
void bzrtpCrypto_DHMComputeSecret(bzrtpDHMContext_t *context, int (*rngFunction)(void *, uint8_t *, size_t), void *rngContext) {
	size_t keyLength;

	/* import the peer public value G^Y mod P in the polar ssl context */
	dhm_read_public((dhm_context *)(context->cryptoModuleData), context->peer, context->primeLength);

	/* compute the secret key */
	keyLength = context->primeLength; /* undocumented but this value seems to be in/out, so we must set it to the expected key length */
	context->key = (uint8_t *)malloc(keyLength*sizeof(uint8_t)); /* allocate key buffer */
	dhm_calc_secret((dhm_context *)(context->cryptoModuleData), context->key, &keyLength, (int (*)(void *, unsigned char *, size_t))rngFunction, rngContext);
}



#else /* POLAR SSL VERSION 1.2 */

/* Hashs */
#include "polarssl/sha2.h"

/*
 * @brief SHA256 wrapper
 * @param[in]	input 		Input data buffer
 * @param[in]   inputLength	Input data length in bytes
 * @param[in]	hmacLength	Length of output required in bytes, HMAC output is truncated to the hmacLength left bytes. 32 bytes maximum
 * @param[out]	output		Output data buffer.
 *
 */
void bzrtpCrypto_sha256(const uint8_t *input,
		size_t inputLength,
		uint8_t hashLength,
		uint8_t *output)
{
	uint8_t hashOutput[32];
	sha2(input, inputLength, hashOutput, 0); /* last param to zero to select SHA256 and not SHA224 */

	/* check output length, can't be>32 */
	if (hashLength>32) {
		memcpy(output, hashOutput, 32);
	} else {
		memcpy(output, hashOutput, hashLength);
	}
}

/*
 * HMAC-SHA-256 wrapper
 * @param[in] 	key			HMAC secret key
 * @param[in] 	keyLength	HMAC key length
 * @param[in]	input 		Input data buffer
 * @param[in]   inputLength	Input data length
 * @param[in]	hmacLength	Length of output required in bytes, HMAC output is truncated to the hmacLength left bytes. 32 bytes maximum
 * @param[out]	output		Output data buffer.
 *
 */
void bzrtpCrypto_hmacSha256(const uint8_t *key,
		size_t keyLength,
		const uint8_t *input,
		size_t inputLength,
		uint8_t hmacLength,
		uint8_t *output)
{
	uint8_t hmacOutput[32];
	sha2_hmac(key, keyLength, input, inputLength, hmacOutput, 0); /* last param to zero to select SHA256 and not SHA224 */

	/* check output length, can't be>32 */
	if (hmacLength>32) {
		memcpy(output, hmacOutput, 32);
	} else {
		memcpy(output, hmacOutput, hmacLength);
	}
}

/* compute secret - the ->peer field of context must have been set before calling this function */
void bzrtpCrypto_DHMComputeSecret(bzrtpDHMContext_t *context, int (*rngFunction)(void *, uint8_t *, size_t), void *rngContext) {
	size_t keyLength;
	/* import the peer public value G^Y mod P in the polar ssl context */
	dhm_read_public((dhm_context *)(context->cryptoModuleData), context->peer, context->primeLength);

	/* compute the secret key */
	keyLength = context->primeLength; /* undocumented but this value seems to be in/out, so we must set it to the expected key length */
	context->key = (uint8_t *)malloc(keyLength*sizeof(uint8_t)); /* allocate key buffer */
	dhm_calc_secret((dhm_context *)(context->cryptoModuleData), context->key, &keyLength);
}

#endif /* POLARSSL Version 1.2 */
