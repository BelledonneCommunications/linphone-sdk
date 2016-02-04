/*
crypto.h
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
#define BCTOOLBOX_ERROR_UNSPECIFIED_ERROR			-0x70000000
#define BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL		-0x70001000
#define BCTOOLBOX_ERROR_INVALID_BASE64_INPUT		-0x70002000
#define BCTOOLBOX_ERROR_INVALID_INPUT_DATA			-0x70004000
#define BCTOOLBOX_ERROR_UNAVAILABLE_FUNCTION		-0x70008000

/* Public key related */
#define BCTOOLBOX_ERROR_UNABLE_TO_PARSE_KEY		-0x70010000

/* Certificate related */
#define BCTOOLBOX_ERROR_INVALID_CERTIFICATE			-0x70020000
#define BCTOOLBOX_ERROR_CERTIFICATE_GENERATION_FAIL	-0x70020001
#define BCTOOLBOX_ERROR_CERTIFICATE_WRITE_PEM		-0x70020002
#define BCTOOLBOX_ERROR_CERTIFICATE_PARSE_PEM		-0x70020004
#define BCTOOLBOX_ERROR_UNSUPPORTED_HASH_FUNCTION	-0x70020008

/* SSL related */
#define BCTOOLBOX_ERROR_INVALID_SSL_CONFIG		-0x70030001
#define BCTOOLBOX_ERROR_INVALID_SSL_TRANSPORT	-0x70030002
#define BCTOOLBOX_ERROR_INVALID_SSL_ENDPOINT	-0x70030004
#define BCTOOLBOX_ERROR_INVALID_SSL_AUTHMODE	-0x70030008
#define BCTOOLBOX_ERROR_INVALID_SSL_CONTEXT		-0x70030010

#define BCTOOLBOX_ERROR_NET_WANT_READ			-0x70032000
#define BCTOOLBOX_ERROR_NET_WANT_WRITE			-0x70034000
#define BCTOOLBOX_ERROR_SSL_PEER_CLOSE_NOTIFY	-0x70038000
#define BCTOOLBOX_ERROR_NET_CONN_RESET			-0x70030000

/* certificate verification flags codes */
#define BCTOOLBOX_CERTIFICATE_VERIFY_ALL_FLAGS				0xFFFFFFFF
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_EXPIRED		0x01  /**< The certificate validity has expired. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_REVOKED		0x02  /**< The certificate has been revoked (is on a CRL). */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_CN_MISMATCH	0x04  /**< The certificate Common Name (CN) does not match with the expected CN. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_NOT_TRUSTED	0x08  /**< The certificate is not correctly signed by the trusted CA. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_MISSING		0x10  /**< Certificate was missing. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_SKIP_VERIFY	0x20  /**< Certificate verification was skipped. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_OTHER			0x0100  /**< Other reason (can be used by verify callback) */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_FUTURE			0x0200  /**< The certificate validity starts in the future. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_KEY_USAGE      0x0400  /**< Usage does not match the keyUsage extension. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_EXT_KEY_USAGE  0x0800  /**< Usage does not match the extendedKeyUsage extension. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_NS_CERT_TYPE   0x1000  /**< Usage does not match the nsCertType extension. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_BAD_MD         0x2000  /**< The certificate is signed with an unacceptable hash. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_BAD_PK         0x4000  /**< The certificate is signed with an unacceptable PK alg (eg RSA vs ECDSA). */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_BAD_KEY        0x8000  /**< The certificate is signed with an unacceptable key (eg bad curve, RSA too short). */

#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_FUTURE			0x10000  /**< The CRL is from the future */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_NOT_TRUSTED		0x20000  /**< CRL is not correctly signed by the trusted CA. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_EXPIRED			0x40000  /**< CRL is expired. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_BAD_MD          0x80000  /**< The CRL is signed with an unacceptable hash. */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_BAD_PK          0x100000  /**< The CRL is signed with an unacceptable PK alg (eg RSA vs ECDSA). */
#define BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_BAD_KEY         0x200000  /**< The CRL is signed with an unacceptable key (eg bad curve, RSA too short). */


/* Hash functions type */
typedef enum bctoolbox_md_type {
	BCTOOLBOX_MD_UNDEFINED,
	BCTOOLBOX_MD_SHA1,
	BCTOOLBOX_MD_SHA224,
	BCTOOLBOX_MD_SHA256,
	BCTOOLBOX_MD_SHA384,
	BCTOOLBOX_MD_SHA512} bctoolbox_md_type_t;

/* Dtls srtp protection profile */
typedef enum bctoolbox_srtp_profile {
	BCTOOLBOX_SRTP_UNDEFINED,
	BCTOOLBOX_SRTP_AES128_CM_HMAC_SHA1_80,
	BCTOOLBOX_SRTP_AES128_CM_HMAC_SHA1_32,
	BCTOOLBOX_SRTP_NULL_HMAC_SHA1_80,
	BCTOOLBOX_SRTP_NULL_HMAC_SHA1_32
} bctoolbox_dtls_srtp_profile_t;

/*****************************************************************************/
/****** Utils                                                           ******/
/*****************************************************************************/
/**
 * @brief Return a string translation of an error code
 * PolarSSL and mbedTLS error codes are on 16 bits always negatives, and these are forwarded to the crypto library error to string translation
 * Specific bctoolbox error code are on 32 bits, all in the form -0x7XXX XXXX
 * Output string is truncated if the buffer is too small and always include a null termination char
 *
 * @param[in]		error_code		The error code
 * @param[in/out]	buffer			Buffer to place error string representation
 * @param[in]		buffer_length	Size of the buffer in bytes.
 */
void bctoolbox_strerror(int32_t error_code, char *buffer, size_t buffer_length);

/**
 * @brief Encode a buffer into base64 format
 * @param[out]		output			base64 encoded buffer
 * @param[in/out]	output_length	output buffer max size and actual size of buffer after encoding
 * @param[in]		input			source plain buffer
 * @param[in]		input_length	Length in bytes of plain buffer to be encoded
 *
 * @return 0 if success or BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL if the output buffer cannot contain the encoded data
 */
int32_t bctoolbox_base64_encode(unsigned char *output, size_t *output_length, const unsigned char *input, size_t input_length);

/**
 * @brief Decode a base64 formatted buffer.
 * @param[out]		output			plain buffer
 * @param[in/out]	output_length	output buffer max size and actual size of buffer after decoding
 * @param[in]		input			source base64 encoded buffer
 * @param[in]		input_length	Length in bytes of base64 buffer to be decoded
 *
 * @return 0 if success, BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL if the output buffer cannot contain the decoded data or BCTOOLBOX_ERROR_INVALID_BASE64_INPUT if encoded buffer was incorrect base64 data
 */
int32_t bctoolbox_base64_decode(unsigned char *output, size_t *output_length, const unsigned char *input, size_t input_length);

/*****************************************************************************/
/****** Random Number Generation                                        ******/
/*****************************************************************************/
/** @brief An opaque structure used to store RNG context
 * Instanciate pointers only and allocate them using the bctoolbox_rng_context_new() function
 */
typedef struct bctoolbox_rng_context_struct bctoolbox_rng_context_t;

/**
 * @brief Create and initialise the Random Number Generator context
 * @return a pointer to the RNG context
 */
bctoolbox_rng_context_t *bctoolbox_rng_context_new(void);

/**
 * @brief Get some random material
 *
 * @param[in/out]	context			The RNG context to be used
 * @param[out]		output			A destination buffer for the random material generated
 * @param[in]		output_length	Size in bytes of the output buffer and requested random material
 *
 * @return 0 on success
 */
int32_t bctoolbox_rng_get(bctoolbox_rng_context_t *context, unsigned char*output, size_t output_length);

/**
 * @brief Clear the RNG context and free internal buffer
 *
 * @param[in]	context		The RNG context to clear
 */
void bctoolbox_rng_context_free(bctoolbox_rng_context_t *context);

/*****************************************************************************/
/***** Signing key                                                       *****/
/*****************************************************************************/
/** @brief An opaque structure used to store the signing key context
 * Instanciate pointers only and allocate them using the bctoolbox_signing_key_new() function
 */
typedef struct bctoolbox_signing_key_struct bctoolbox_signing_key_t;

/**
 * @brief Create and initialise a signing key context
 * @return a pointer to the signing key context
 */
bctoolbox_signing_key_t *bctoolbox_signing_key_new(void);

/**
 * @brief Clear the signing key context and free internal buffer
 *
 * @param[in]	key		The signing key context to clear
 */
void  bctoolbox_signing_key_free(bctoolbox_signing_key_t *key);

/**
 * @brief	Write the key in a buffer as a PEM string
 *
 * @param[in]	key		The signing key to be extracted in PEM format
 *
 * @return a pointer to a null terminated string containing the key in PEM format. This buffer must then be freed by caller. NULL on failure.
 */
char *bctoolbox_signing_key_get_pem(bctoolbox_signing_key_t *key);

/**
 * @brief Parse signing key in PEM format from a null terminated string buffer
 *
 * @param[in/out]	key				An already initialised signing key context
 * @param[in]		buffer			The input buffer containing a PEM format key in a null terminated string
 * @param[in]		buffer_length	The length of input buffer, including the NULL termination char
 * @param[in]		password		Password for decryption(may be NULL)
 * @param[in]		passzord_length	size of password
 *
 * @return 0 on success
 */
int32_t bctoolbox_signing_key_parse(bctoolbox_signing_key_t *key, const char *buffer, size_t buffer_length, const unsigned char *password, size_t password_length);

/**
 * @brief Parse signing key from a file
 *
 * @param[in/out]	key				An already initialised signing key context
 * @param[in]		path			filename to read the key from
 * @param[in]		password		Password for decryption(may be NULL)
 *
 * @return 0 on success
 */
int32_t bctoolbox_signing_key_parse_file(bctoolbox_signing_key_t *key, const char *path, const char *password);

/*****************************************************************************/
/***** X509 Certificate                                                  *****/
/*****************************************************************************/
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
int32_t bctoolbox_x509_certificate_get_fingerprint(const bctoolbox_x509_certificate_t *cert, char *fingerprint, size_t fingerprint_length, bctoolbox_md_type_t hash_algorithm);
int32_t bctoolbox_x509_certificate_get_signature_hash_function(const bctoolbox_x509_certificate_t *certificate, bctoolbox_md_type_t *hash_algorithm);
int32_t bctoolbox_x509_certificate_generate_selfsigned(const char *subject, bctoolbox_x509_certificate_t *certificate, bctoolbox_signing_key_t *pkey, char *pem, size_t pem_length);
int32_t bctoolbox_x509_certificate_flags_to_string(char *buffer, size_t buffer_size, uint32_t flags);
int32_t bctoolbox_x509_certificate_set_flag(uint32_t *flags, uint32_t flags_to_set);
uint32_t bctoolbox_x509_certificate_remap_flag(uint32_t flags);
int32_t bctoolbox_x509_certificate_unset_flag(uint32_t *flags, uint32_t flags_to_unset);


/*****************************************************************************/
/***** SSL                                                               *****/
/*****************************************************************************/
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
const bctoolbox_x509_certificate_t *bctoolbox_ssl_get_peer_certificate(bctoolbox_ssl_context_t *ssl_ctx);

bctoolbox_ssl_config_t *bctoolbox_ssl_config_new(void);
int32_t bctoolbox_ssl_config_set_crypto_library_config(bctoolbox_ssl_config_t *ssl_config, void *internal_config);
void bctoolbox_ssl_config_free(bctoolbox_ssl_config_t *ssl_config);
int32_t bctoolbox_ssl_config_defaults(bctoolbox_ssl_config_t *ssl_config, int endpoint, int transport);
int32_t bctoolbox_ssl_config_set_endpoint(bctoolbox_ssl_config_t *ssl_config, int endpoint);
int32_t bctoolbox_ssl_config_set_transport (bctoolbox_ssl_config_t *ssl_config, int transport);
int32_t bctoolbox_ssl_config_set_authmode(bctoolbox_ssl_config_t *ssl_config, int authmode);
int32_t bctoolbox_ssl_config_set_rng(bctoolbox_ssl_config_t *ssl_config, int(*rng_function)(void *, unsigned char *, size_t), void *rng_context);
int32_t bctoolbox_ssl_config_set_callback_verify(bctoolbox_ssl_config_t *ssl_config, int(*callback_function)(void *, bctoolbox_x509_certificate_t *, int, uint32_t *), void *callback_data);
int32_t bctoolbox_ssl_config_set_callback_cli_cert(bctoolbox_ssl_config_t *ssl_config, int(*callback_function)(void *, bctoolbox_ssl_context_t *, unsigned char *, size_t), void *callback_data);
int32_t bctoolbox_ssl_config_set_ca_chain(bctoolbox_ssl_config_t *ssl_config, bctoolbox_x509_certificate_t *ca_chain, char *peer_cn);
int32_t bctoolbox_ssl_config_set_own_cert(bctoolbox_ssl_config_t *ssl_config, bctoolbox_x509_certificate_t *cert, bctoolbox_signing_key_t *key);

/***** DTLS-SRTP functions *****/
bctoolbox_dtls_srtp_profile_t bctoolbox_ssl_get_dtls_srtp_protection_profile(bctoolbox_ssl_context_t *ssl_ctx);
int32_t bctoolbox_ssl_config_set_dtls_srtp_protection_profiles(bctoolbox_ssl_config_t *ssl_config, const bctoolbox_dtls_srtp_profile_t *profiles, size_t profiles_number);
int32_t bctoolbox_ssl_get_dtls_srtp_key_material(bctoolbox_ssl_context_t *ssl_ctx, char *output, size_t *output_length);
uint8_t bctoolbox_dtls_srtp_supported(void);
#endif
