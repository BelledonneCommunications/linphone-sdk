/*
chrypto.h
Copyright (C) 2016  Belledonne Communications SARL

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
#ifndef BCTOOLBOX_CRYPTO_H
#define BCTOOLBOX_CRYPTO_H

#include <stdint.h>
/* SSL settings defines */
#define BCTOOLBOX_SSL_UNSET -1

#define BCTOOLBOX_SSL_IS_CLIENT 0
#define BCTOOLBOX_SSL_IS_SERVER 1

#define BCTOOLBOX_SSL_TRANSPORT_STREAM 0
#define BCTOOLBOX_SSL_TRANSPORT_DATAGRAM 1

#define BCTOOLBOX_SSL_VERIFY_NONE 0
#define BCTOOLBOX_SSL_VERIFY_OPTIONAL 1
#define BCTOOLBOX_SSL_VERIFY_REQUIRED 2


/* Error codes : All error codes are negative and defined  on 32 bits on format -0x7XXXXXXX
 * in order to be sure to not overlap on crypto librairy (polarssl or mbedtls for now) which are defined on 16 bits 0x[7-0]XXX */
#define BCTOOLBOX_ERROR_UNSPECIFIED_ERROR		-0x70000000
#define BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL		-0x70001000
#define BCTOOLBOX_ERROR_INVALID_BASE64_INPUT		-0x70002000

/* Public key related */
#define BCTOOLBOX_ERROR_UNABLE_TO_PARSE_KEY		-0x70010000

/* Certificate related */
#define BCTOOLBOX_ERROR_INVALID_CERTIFICATE		-0x70020000
#define BCTOOLBOX_ERROR_CERTIFICATE_GENERATION_FAIL	-0x70020001
#define BCTOOLBOX_ERROR_CERTIFICATE_WRITE_PEM		-0x70020002
#define BCTOOLBOX_ERROR_CERTIFICATE_PARSE_PEM		-0x70020003

/* SSL related */
#define BCTOOLBOX_ERROR_INVALID_SSL_CONFIG		-0x70030001
#define BCTOOLBOX_ERROR_INVALID_SSL_TRANSPORT		-0x70030002
#define BCTOOLBOX_ERROR_INVALID_SSL_ENDPOINT		-0x70030004
#define BCTOOLBOX_ERROR_INVALID_SSL_AUTHMODE		-0x70030008
#define BCTOOLBOX_ERROR_INVALID_SSL_CONTEXT		-0x70030010

#define BCTOOLBOX_ERROR_NET_WANT_READ			-0x70032000
#define BCTOOLBOX_ERROR_NET_WANT_WRITE			-0x70034000
#define BCTOOLBOX_ERROR_SSL_PEER_CLOSE_NOTIFY		-0x70038000
#define BCTOOLBOX_ERROR_NET_CONN_RESET			-0x70030000

/* certificate verification flags codes */
#define BCTOOLBOX_CERTIFICATE_VERIFY_ALL_FLAGS			0xFFFFFFFF
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_EXPIRED		0x01  /**< The certificate validity has expired. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_REVOKED		0x02  /**< The certificate has been revoked (is on a CRL). */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_CN_MISMATCH	0x04  /**< The certificate Common Name (CN) does not match with the expected CN. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_NOT_TRUSTED	0x08  /**< The certificate is not correctly signed by the trusted CA. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_NOT_TRUSTED		0x10  /**< CRL is not correctly signed by the trusted CA. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_EXPIRED		0x20  /**< CRL is expired. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_MISSING		0x40  /**< Certificate was missing. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_SKIP_VERIFY	0x80  /**< Certificate verification was skipped. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_OTHER		0x0100  /**< Other reason (can be used by verify callback) */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_FUTURE		0x0200  /**< The certificate validity starts in the future. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_FUTURE		0x0400  /**< The CRL is from the future */

int32_t bctoolbox_callback_return_remap(int ret_code);
void bctoolbox_strerror(int32_t error_code, char *buffer, size_t buffer_length);
int32_t bctoolbox_base64_encode(unsigned char *output, size_t *output_length, const unsigned char *input, size_t input_length);
int32_t bctoolbox_base64_decode(unsigned char *output, size_t *output_length, const unsigned char *input, size_t input_length);

/* Signing key */
typedef struct bctoolbox_signing_key_struct bctoolbox_signing_key_t;

bctoolbox_signing_key_t *bctoolbox_signing_key_new(void);
void  bctoolbox_signing_key_free(bctoolbox_signing_key_t *key);

char *bctoolbox_signing_key_get_pem(bctoolbox_signing_key_t *key);
int32_t bctoolbox_signing_key_parse(bctoolbox_signing_key_t *key, const char *buffer, size_t buffer_length, const unsigned char *password, size_t password_length);
int32_t bctoolbox_signing_key_parse_file(bctoolbox_signing_key_t *key, const char *path, const char *password);

/* Certificate */
typedef struct bctoolbox_x509_certificate_struct bctoolbox_x509_certificate_t;

bctoolbox_x509_certificate_t *bctoolbox_x509_certificate_new(void);
void  bctoolbox_x509_certificate_free(bctoolbox_x509_certificate_t *cert);

char *bctoolbox_x509_certificates_chain_get_pem(bctoolbox_x509_certificate_t *cert);
int32_t bctoolbox_x509_certificate_get_info_string(char *buf, size_t size, const char *prefix, const bctoolbox_x509_certificate_t *cert);
int32_t bctoolbox_x509_certificate_parse(bctoolbox_x509_certificate_t *cert, const char *buffer, size_t buffer_length);
int32_t bctoolbox_x509_certificate_parse_file(bctoolbox_x509_certificate_t *cert, const char *path);
int32_t bctoolbox_x509_certificate_parse_path(bctoolbox_x509_certificate_t *cert, const char *path);
int32_t bctoolbox_x509_certificate_get_der_length(bctoolbox_x509_certificate_t *cert);
int32_t bctoolbox_x509_certificate_get_der(bctoolbox_x509_certificate_t *cert, unsigned char *buffer, size_t buffer_length);
int32_t bctoolbox_x509_certificate_get_subject_dn(bctoolbox_x509_certificate_t *cert, char *dn, size_t dn_length);
int32_t bctoolbox_x509_certificate_get_fingerprint(bctoolbox_x509_certificate_t *cert, char *fingerprint, size_t fingerprint_length);
int32_t bctoolbox_x509_certificate_generate_selfsigned(const char *subject, bctoolbox_x509_certificate_t *certificate, bctoolbox_signing_key_t *pkey, char *pem, size_t pem_length);
int32_t bctoolbox_x509_certificate_flags_to_string(char *buffer, size_t buffer_size, uint32_t flags);
int32_t bctoolbox_x509_certificate_set_flag(uint32_t *flags, uint32_t flags_to_set);
uint32_t bctoolbox_x509_certificate_remap_flag(uint32_t flags);
int32_t bctoolbox_x509_certificate_unset_flag(uint32_t *flags, uint32_t flags_to_unset);


/* SSL client */
typedef struct bctoolbox_ssl_context_struct bctoolbox_ssl_context_t;
typedef struct bctoolbox_ssl_config_struct bctoolbox_ssl_config_t;
bctoolbox_ssl_context_t *bctoolbox_ssl_context_new(void);
void bctoolbox_ssl_context_free(bctoolbox_ssl_context_t *ssl_ctx);
int32_t bctoolbox_ssl_context_setup(bctoolbox_ssl_context_t *ssl_ctx, bctoolbox_ssl_config_t *ssl_config);

int32_t bctoolbox_ssl_close_notify(bctoolbox_ssl_context_t *ssl_ctx);
int32_t bctoolbox_ssl_session_reset(bctoolbox_ssl_context_t *ssl_ctx);
int32_t bctoolbox_ssl_read(bctoolbox_ssl_context_t *ssl_ctx, unsigned char *buf, size_t buf_length);
int32_t bctoolbox_ssl_write(bctoolbox_ssl_context_t *ssl_ctx, const unsigned char *buf, size_t buf_length);
int32_t bctoolbox_ssl_handshake(bctoolbox_ssl_context_t *ssl_ctx);
int32_t bctoolbox_ssl_set_hs_own_cert(bctoolbox_ssl_context_t *ssl_ctx, bctoolbox_x509_certificate_t *cert, bctoolbox_signing_key_t *key);
void bctoolbox_ssl_set_io_callbacks(bctoolbox_ssl_context_t *ssl_ctx, void *callback_data,
		int(*callback_send_function)(void *, const unsigned char *, size_t), /* callbacks args are: callback data, data buffer to be send, size of data buffer */
		int(*callback_recv_function)(void *, unsigned char *, size_t)); /* args: callback data, data buffer to be read, size of data buffer */

bctoolbox_ssl_config_t *bctoolbox_ssl_config_new(void);
void bctoolbox_ssl_config_free(bctoolbox_ssl_config_t *ssl_config);
int32_t bctoolbox_ssl_config_init(bctoolbox_ssl_config_t *ssl_config);
int32_t bctoolbox_ssl_config_defaults(bctoolbox_ssl_config_t *ssl_config, int endpoint, int transport);
int32_t bctoolbox_ssl_config_set_authmode(bctoolbox_ssl_config_t *ssl_config, int authmode);
int32_t bctoolbox_ssl_config_set_rng(bctoolbox_ssl_config_t *ssl_config, int(*rng_function)(void *, unsigned char *, size_t), void *rng_context);
int32_t bctoolbox_ssl_config_set_callback_verify(bctoolbox_ssl_config_t *ssl_config, int(*callback_function)(void *, bctoolbox_x509_certificate_t *, int, uint32_t *), void *callback_data);
int32_t bctoolbox_ssl_config_set_callback_cli_cert(bctoolbox_ssl_config_t *ssl_config, int(*callback_function)(void *, bctoolbox_ssl_context_t *, unsigned char *, size_t), void *callback_data);
int32_t bctoolbox_ssl_config_set_ca_chain(bctoolbox_ssl_config_t *ssl_config, bctoolbox_x509_certificate_t *ca_chain, char *peer_cn);
#endif
