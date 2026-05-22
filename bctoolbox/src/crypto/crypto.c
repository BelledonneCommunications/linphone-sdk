/*
 * Copyright (c) 2016-2020 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <bctoolbox/crypto.h>

struct bctbx_crypto_provider_struct {
	/* Stable identity for config/resolver decisions. Future dispatch hooks can be attached here. */
	const char *name;
	const char *class_name;
	bctbx_type_implementation_t implementation;
	unsigned int capability_flags;
};

#define BCTBX_CRYPTO_PROVIDER_CAP_TLS 0x01
#define BCTBX_CRYPTO_PROVIDER_CAP_X509 0x02

static const bctbx_crypto_provider_t mbedtls_crypto_provider = {
    "mbedtls", "MbedTlsCryptoProvider", BCTBX_MBEDTLS, BCTBX_CRYPTO_PROVIDER_CAP_TLS | BCTBX_CRYPTO_PROVIDER_CAP_X509};
static const bctbx_crypto_provider_t openssl_crypto_provider = {
    "openssl", "OpenSslCryptoProvider", BCTBX_OPENSSL, BCTBX_CRYPTO_PROVIDER_CAP_TLS | BCTBX_CRYPTO_PROVIDER_CAP_X509};

static int bctbx_crypto_provider_is_compiled_in(bctbx_type_implementation_t implementation) {
	switch (implementation) {
#ifdef HAVE_MBEDTLS
		case BCTBX_MBEDTLS:
		case BCTBX_MBEDTLS2:
			return 1;
#endif
#ifdef HAVE_OPENSSL
		case BCTBX_OPENSSL:
			return 1;
#endif
		default:
			return 0;
	}
}

static int bctbx_crypto_provider_implementation_matches(bctbx_type_implementation_t requested_implementation,
                                                        bctbx_type_implementation_t implementation) {
	if (requested_implementation == implementation) {
		return 1;
	}
	if ((requested_implementation == BCTBX_MBEDTLS && implementation == BCTBX_MBEDTLS2) ||
	    (requested_implementation == BCTBX_MBEDTLS2 && implementation == BCTBX_MBEDTLS)) {
		return 1;
	}
	return 0;
}

/**
 * List of available key agreement algorithm
 */
uint32_t bctbx_key_agreement_algo_list(void) {
	uint32_t ret = BCTBX_DHM_2048 | BCTBX_DHM_3072; /* provided by mbedtls */
#ifdef HAVE_DECAF
	/* decaf always provide X448 and X25519 */
	ret |= BCTBX_ECDH_X25519 | BCTBX_ECDH_X448;
#endif /* HAVE_DECAF */
	return ret;
}

const bctbx_crypto_provider_t *bctbx_crypto_provider_get_default(void) {
#ifdef HAVE_MBEDTLS
	return &mbedtls_crypto_provider;
#elif defined(HAVE_OPENSSL)
	return &openssl_crypto_provider;
#else
	return NULL;
#endif
}

const bctbx_crypto_provider_t *bctbx_crypto_provider_get_by_name(const char *name) {
	if (name == NULL || name[0] == '\0') {
		return NULL;
	}
	if (strcmp(name, mbedtls_crypto_provider.name) == 0) {
		return &mbedtls_crypto_provider;
	}
	if (strcmp(name, openssl_crypto_provider.name) == 0) {
		return &openssl_crypto_provider;
	}
	return NULL;
}

int32_t bctbx_crypto_provider_resolve(const char *name, const bctbx_crypto_provider_t **provider) {
	const bctbx_crypto_provider_t *resolved_provider;

	if (provider == NULL) {
		return BCTBX_ERROR_INVALID_INPUT_DATA;
	}
	*provider = NULL;

	if (name == NULL || name[0] == '\0') {
		return BCTBX_ERROR_INVALID_CRYPTO_PROVIDER;
	}

	resolved_provider = bctbx_crypto_provider_get_by_name(name);
	if (resolved_provider == NULL) {
		return BCTBX_ERROR_INVALID_CRYPTO_PROVIDER;
	}
	if (!bctbx_crypto_provider_is_compiled_in(resolved_provider->implementation)) {
		return BCTBX_ERROR_UNAVAILABLE_CRYPTO_PROVIDER;
	}

	*provider = resolved_provider;
	return 0;
}

int32_t bctbx_crypto_provider_resolve_for_implementation(const char *name,
                                                         bctbx_type_implementation_t implementation,
                                                         const bctbx_crypto_provider_t **provider) {
	int32_t ret = bctbx_crypto_provider_resolve(name, provider);

	if (ret != 0) {
		return ret;
	}
	if (!bctbx_crypto_provider_implementation_matches((*provider)->implementation, implementation)) {
		*provider = NULL;
		return BCTBX_ERROR_UNAVAILABLE_CRYPTO_PROVIDER;
	}
	return 0;
}

int32_t bctbx_crypto_provider_is_available(const bctbx_crypto_provider_t *provider) {
	if (provider == NULL) {
		return 0;
	}
	return bctbx_crypto_provider_is_compiled_in(provider->implementation);
}

const char *bctbx_crypto_provider_get_name(const bctbx_crypto_provider_t *provider) {
	return provider ? provider->name : NULL;
}

const char *bctbx_crypto_provider_get_class_name(const bctbx_crypto_provider_t *provider) {
	return provider ? provider->class_name : NULL;
}

bctbx_type_implementation_t bctbx_crypto_provider_get_implementation_type(const bctbx_crypto_provider_t *provider) {
	return provider ? provider->implementation : bctbx_ssl_get_implementation_type();
}

/*****************************************************************************/
/***** AES GCM encrypt/decrypt chunk by chunk, needed for file encryption ****/
/*****************************************************************************/
/**
 * @brief encrypt the file in input buffer for linphone encrypted file transfer
 *
 * @param[in/out]	cryptoContext	a context already initialized using bctbx_aes_gcm_context_new
 * @param[in]		key		encryption key
 * @param[in]		length	buffer size
 * @param[in]		plain	buffer holding the input data
 * @param[out]		cipher	buffer to store the output data
 */
int bctbx_aes_gcm_encryptFile(void **cryptoContext, unsigned char *key, size_t length, char *plain, char *cipher) {
	bctbx_aes_gcm_context_t *gcmContext;

	if (*cryptoContext == NULL && key == NULL) return -1; // we need the key, at least at first call

	if (*cryptoContext == NULL) { /* first call to the function, allocate a crypto context and initialise it */
		/* key contains 192bits of key || 64 bits of Initialisation Vector, no additional data */
		gcmContext = bctbx_aes_gcm_context_new(key, 24, NULL, 0, key + 24, 8, BCTBX_GCM_ENCRYPT);
		if (gcmContext == NULL) {
			return -1;
		}
		*cryptoContext = gcmContext;
	} else { /* this is not the first call, get the context */
		gcmContext = (bctbx_aes_gcm_context_t *)*cryptoContext;
	}

	if (plain != NULL) {
		bctbx_aes_gcm_process_chunk(gcmContext, (const uint8_t *)plain, length, (uint8_t *)cipher);
	} else { /* plain is NULL, finish the stream, if cipher is not null, generate a tag in it */
		if (cipher != NULL && length > 0) {
			bctbx_aes_gcm_finish(gcmContext, (uint8_t *)cipher, length);
		} else {
			bctbx_aes_gcm_finish(gcmContext, NULL, 0);
		}
		*cryptoContext = NULL;
	}

	return 0;
}

/**
 * @brief decrypt the file in input buffer for linphone encrypted file transfer
 *
 * @param[in/out]	cryptoContext	a context already initialized using bctbx_aes_gcm_context_new
 * @param[in]		key		encryption key
 * @param[in]		length	buffer size
 * @param[out]		plain	buffer holding the output data
 * @param[int]		cipher	buffer to store the input data
 */
int bctbx_aes_gcm_decryptFile(void **cryptoContext, unsigned char *key, size_t length, char *plain, char *cipher) {
	bctbx_aes_gcm_context_t *gcmContext;
	int ret = -1;

	if (*cryptoContext == NULL && key == NULL) return ret; // we need the key, at least at first call

	if (*cryptoContext == NULL) { /* first call to the function, allocate a crypto context and initialise it */

		/* key contains 192bits of key || 64 bits of Initialisation Vector, no additional data */
		gcmContext = bctbx_aes_gcm_context_new(key, 24, NULL, 0, key + 24, 8, BCTBX_GCM_DECRYPT);
		if (gcmContext == NULL) {
			return ret;
		}
		*cryptoContext = gcmContext;
	} else { /* this is not the first call, get the context */
		gcmContext = (bctbx_aes_gcm_context_t *)*cryptoContext;
	}

	if (cipher != NULL) {
		ret = bctbx_aes_gcm_process_chunk(gcmContext, (const uint8_t *)cipher, length, (uint8_t *)plain);
	} else { /* cipher is NULL, finish the stream, if plain is not null and we have a length, compute the authentication
		        tag*/
		if (plain != NULL && length > 0) {
			ret = bctbx_aes_gcm_finish(gcmContext, (uint8_t *)plain, length);
		} else {
			ret = bctbx_aes_gcm_finish(gcmContext, NULL, 0);
		}
		*cryptoContext = NULL;
	}

	return ret;
}
