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
#include "openssl/ssl.h"

#define BCTBX_KEYMGMT_PARAM_SSL_CONFIG "user-data-bctbx-ssl-config"

struct bctbx_ssl_config_struct {
	const SSL_METHOD *ssl_method; /**< current tls method used by ssl_ctx (cannot retrieve it from the SSL_CTX object
	                                 before OpenSSL 3.0 so have to store it here..)*/
	SSL_CTX *ssl_ctx;             /**< actual config structure */
	int ssl_verification_mode;    /**< BCTBX_SSL_VERIFY_NONE, BCTBX_SSL_VERIFY_OPTIONAL, BCTBX_SSL_VERIFY_REQUIRED */
	uint8_t ssl_config_externally_provided; /**< a flag, on when the ssl_config was provided by callers and not created
	                                           through bctbx_ssl_config_new() function */
	bctbx_ext_signing_key_ref_t *ext_key_ref;                           /**< an external key reference */
	size_t ext_key_size;                                                /**< the external key size in bits */
	int ext_key_type;                                                   /**< is it a EC or RSA key? */
	bctbx_ssl_config_ext_sign_callback_t callback_ext_signing_function; /**< function to call for external signing */
	void *callback_ext_signing_data;
};
