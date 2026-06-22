/*
 * Copyright (c) 2026 Belledonne Communications SARL.
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
#include "bctoolbox/crypto.h"
#include <mbedtls/ssl.h>

struct bctbx_ssl_config_struct {
	mbedtls_ssl_config *ssl_config;         /**< actual config structure */
	uint8_t ssl_config_externally_provided; /**< a flag, on when the ssl_config was provided by callers and not created
	                                           threw the new function */
	int (*callback_cli_cert_function)(void *,
	                                  bctbx_ssl_context_t *,
	                                  const bctbx_list_t *); /**< pointer to the callback called to update client
	              certificate during handshake callback params are user_data, ssl_context, list of server certificate
	              subject alt name and CN (null terminated strings) */
	void *callback_cli_cert_data;                            /**< data passed to the client cert callback */
#ifdef HAVE_DTLS_SRTP
	mbedtls_ssl_srtp_profile
	    dtls_srtp_mbedtls_profiles[MBEDTLS_TLS_SRTP_MAX_PROFILE_LIST_LENGTH +
	                               1]; /**< list of supported DTLS-SRTP profiles, mbedtls won't hold the reference, so
	                                      we must do it for the lifetime of the config structure. (size is +1 to add the
	                                      list termination) */
#endif                                 /* HAVE_DTLS_SRTP */
	int *ciphersuites;                 /**< ciphersuites as mbedtls id's */
	bctbx_ext_signing_key_ref_t *ext_key_ref;                           /**< an external key reference */
	mbedtls_pk_context ext_key;                                         /**< the external key holder */
	size_t ext_key_size;                                                /**< the external key size in bits */
	bctbx_ssl_config_ext_sign_callback_t callback_ext_signing_function; /**< function to call for external signing */
	void *callback_ext_signing_data;
};
