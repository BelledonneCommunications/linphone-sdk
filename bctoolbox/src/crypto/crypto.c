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

#include <bctoolbox/crypto.h>


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
		gcmContext = bctbx_aes_gcm_context_new(key, 24, NULL, 0, key+24, 8, BCTBX_GCM_ENCRYPT);
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

	if (*cryptoContext == NULL && key == NULL) return -1; // we need the key, at least at first call

	if (*cryptoContext == NULL) { /* first call to the function, allocate a crypto context and initialise it */

		/* key contains 192bits of key || 64 bits of Initialisation Vector, no additional data */
		gcmContext = bctbx_aes_gcm_context_new(key, 24, NULL, 0, key+24, 8, BCTBX_GCM_DECRYPT);
		if (gcmContext == NULL) {
			return -1;
		}
		*cryptoContext = gcmContext;
	} else { /* this is not the first call, get the context */
		gcmContext = (bctbx_aes_gcm_context_t *)*cryptoContext;
	}

	if (cipher != NULL) {
		bctbx_aes_gcm_process_chunk(gcmContext, (const uint8_t *)cipher, length, (uint8_t *)plain);
	} else { /* cipher is NULL, finish the stream, if plain is not null and we have a length, compute the authentication tag*/
		if (plain != NULL && length > 0) {
			bctbx_aes_gcm_finish(gcmContext, (uint8_t *)plain, length);
		} else {
			bctbx_aes_gcm_finish(gcmContext, NULL, 0);
		}
		*cryptoContext = NULL;
	}

	return 0;
}
