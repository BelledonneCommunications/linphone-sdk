/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef AUTHENTICATION_HELPER_H_
#define AUTHENTICATION_HELPER_H_

#include "belle-sip/defs.h"
#include "belle-sip/belle-sip.h"

BELLE_SIP_BEGIN_DECLS

/**
 * Create an authorization header from an www_authenticate header, all common parameters are copyed.
 * copy params: scheme, realm, nonce, algorithm, opaque
 * @param authentication source to be used as input
 * @return belle_sip_header_authorization_t*
 */
BELLESIP_EXPORT belle_sip_header_authorization_t* belle_sip_auth_helper_create_authorization(const belle_sip_header_www_authenticate_t* authentication);

/**
 * Create an http authorization header from an www_authenticate header, all common parameters are copyed.
 * copy params: scheme, realm, nonce, algorithm, opaque
 * @param authentication source to be used as input
 * @return belle_http_header_authorization_t*
 */
BELLESIP_EXPORT belle_http_header_authorization_t* belle_http_auth_helper_create_authorization(const belle_sip_header_www_authenticate_t* authentication);


/**
 * Create an proxy_authorization header from an www_authenticate header, all common parameters are copyed.
 * copy params: scheme, realm, nonce, algorithm, opaque
 * @param authentication source to be used as input
 * @return belle_sip_header_authorization_t*
 */
BELLESIP_EXPORT belle_sip_header_proxy_authorization_t* belle_sip_auth_helper_create_proxy_authorization(const belle_sip_header_proxy_authenticate_t* proxy_authentication);

/**
 * compute and set response value according to parameters
 * HA1=MD5(username:realm:passwd)
 * fills cnonce if needed (qop=auth);
 * fills qop
 *
 * @return 0 if succeed
 */
BELLESIP_EXPORT int belle_sip_auth_helper_fill_authorization(belle_sip_header_authorization_t* authorization
												,const char* method
												,const char* ha1);
/**
 * compute and set response value according to parameters
 * @return 0 if succeed
 */
BELLESIP_EXPORT int belle_sip_auth_helper_fill_proxy_authorization(belle_sip_header_proxy_authorization_t* proxy_authorization
												,const char* method
												,const char* ha1);

/*
 * compute HA1 (NULL terminated)
 * HA1=MD5(userid:realm:passwd)
 * return 0 in case of success
 * */
BELLESIP_EXPORT int belle_sip_auth_helper_compute_ha1(const char* userid,const char* realm,const char* password, char ha1[33]);
/*
 * compute HA2 (NULL terminated)
 * HA2=MD5(method:uri)
 * return 0 in case of success
 * */
BELLESIP_EXPORT int belle_sip_auth_helper_compute_ha2(const char* method,const char* uri, char ha2[33]);

/*
 * compute response(NULL terminated)
 * res=MD5(ha1:nonce:ha2)
 * return 0 in case of success
 * */
BELLESIP_EXPORT int belle_sip_auth_helper_compute_response(const char* ha1,const char* nonce, const char* ha2, char response[33]);

/*
 * compute response(NULL terminated)
 * res=MD5(HA1:nonce:nonce_count:cnonce:qop:HA2)
 * return 0 in case of success
 * */
BELLESIP_EXPORT int belle_sip_auth_helper_compute_response_qop_auth(	const char* ha1
													, const char* nonce
													, unsigned int nonce_count
													, const char* cnonce
													, const char* qop
													, const char* ha2
													, char response[33]);


/*TLS client certificate auth*/

/**
 * Set TLS certificate verification callback
 *
 * @param callback function pointer for callback, or NULL to unset
 *
 * Callback signature is:
 * int (*verify_cb_error_cb_t)(unsigned char* der, int length, int depth, int* flags);
 * der - raw certificate data, in DER format
 * length - length of certificate DER data
 * depth - position of certificate in cert chain, ending at 0 = root or top
 * flags - verification state for CURRENT certificate only
 */
BELLESIP_EXPORT int belle_sip_tls_set_verify_error_cb(void *callback);

/**
 * Format of certificate buffer
 **/
typedef enum  belle_sip_certificate_raw_format {
	BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM, /** PEM format*/
	BELLE_SIP_CERTIFICATE_RAW_FORMAT_DER /** ASN.1 raw format*/
}belle_sip_certificate_raw_format_t;

/**
 * Parse a buffer containing either a certificate chain order in PEM format or a single DER cert
 * @param buff raw buffer
 * @param size buffer size
 * @param format either PEM or DER
 * @return  belle_sip_certificates_chain_t or NULL if cannot be decoded
 */
BELLESIP_EXPORT belle_sip_certificates_chain_t* belle_sip_certificates_chain_parse(const char* buff, size_t size,belle_sip_certificate_raw_format_t format);

/**
 * Parse a buffer containing either a private or public rsa key in PEM format
 * @param buff raw buffer
 * @param size buffer size
 * @param passwd password (optionnal)
 * @return list of belle_sip_signing_key_t or NULL if cannot be decoded
 */
BELLESIP_EXPORT belle_sip_signing_key_t* belle_sip_signing_key_parse(const char* buff, size_t size,const char* passwd);

/**
 * Parse a pather containing either a certificate chain order in PEM format or a single DER cert
 * @param path file
 * @param format either PEM or DER
 * @return  belle_sip_certificates_chain_t or NUL if cannot be decoded
 */
BELLESIP_EXPORT belle_sip_certificates_chain_t* belle_sip_certificates_chain_parse_file(const char* path, belle_sip_certificate_raw_format_t format);

/**
 * Parse a directory for *.pem file containing a certificate and private key in PEM format or a single DER cert with subject CNAME as given
 *
 * @param[in]	path			directory to parse
 * @param[in]	subject			subject CNAME to look for
 * @param[out]	certificate		result certificate, NULL if not found. Is allocated by this function, caller must do a belle_sip_object_unref on it after use
 * @param[out]	pkey			result private key, NULL if not found. Is allocated by this function, caller must do a belle_sip_object_unref on it after use
 * @param[in]	format			either PEM or DER
 * @return  0 if we found a certificate and key matching given subject common name
 */
BELLESIP_EXPORT int belle_sip_get_certificate_and_pkey_in_dir(const char *path, const char *subject, belle_sip_certificates_chain_t **certificate, belle_sip_signing_key_t **pkey, belle_sip_certificate_raw_format_t format);

/**
 * Generate a self signed certificate and key and save them in a file if a path is given, file will be <subject>.pem
 *
 * @param[in]	path		If not NULL a file will be written in the given directory. filename is <subject>.pem
 * @param[in]	subject		used in the CN= field of issuer and subject name
 * @param[out]	certificate	the generated certificate. Must be destroyed using belle_sip_certificates_chain_destroy
 * @param[out]	key			the generated key. Must be destroyed using belle_sip_signing_key_destroy
 * @return 0 on success
 */
BELLESIP_EXPORT int belle_sip_generate_self_signed_certificate(const char* path, const char *subject, belle_sip_certificates_chain_t **certificate, belle_sip_signing_key_t **pkey);

/**
 * Convert a certificate into a its PEM format string
 *
 * @param[in]	cert	The certificate to be converted into PEM format string
 * @return	the PEM representation of certificate. Buffer is allocated by this function and must be freed by caller
 */
BELLESIP_EXPORT char *belle_sip_certificates_chain_get_pem(belle_sip_certificates_chain_t *cert);

/**
 * Convert a key into a its PEM format string
 *
 * @param[in]	key		The key to be converted into PEM format string
 * @return	the PEM representation of key. Buffer is allocated by this function and must be freed by caller
 */
BELLESIP_EXPORT char *belle_sip_signing_key_get_pem(belle_sip_signing_key_t *key);

/**
 * Generate a certificate fingerprint as described in RFC4572
 * Note: only SHA1 signing algo is supported for now
 *
 * @param[in]	certificate		The certificate used to generate the fingerprint
 * @return		The generated fingerprint formatted according to RFC4572 section 5. Is a null terminated string, must be freed by caller
 */
BELLESIP_EXPORT char *belle_sip_certificates_chain_get_fingerprint(belle_sip_certificates_chain_t *certificate);

/**
 * Parse a pather containing either a private or public rsa key
 * @param path file
 * @param passwd password (optionnal)
 * @return list of belle_sip_signing_key_t or NUL iff cannot be decoded
 */
BELLESIP_EXPORT belle_sip_signing_key_t* belle_sip_signing_key_parse_file(const char* path, const char* passwd);

#define BELLE_TLS_VERIFY_NONE		(0)
#define BELLE_TLS_VERIFY_CN_MISMATCH (1)
#define BELLE_TLS_VERIFY_ANY_REASON (0xff)
/* Set of functions deprecated on 2016/02/02 use the belle_tls_crypto_config_XXX ones */
BELLESIP_DEPRECATED BELLESIP_EXPORT belle_tls_verify_policy_t *belle_tls_verify_policy_new(void);
BELLESIP_DEPRECATED BELLESIP_EXPORT int belle_tls_verify_policy_set_root_ca(belle_tls_verify_policy_t *obj, const char *path);
BELLESIP_DEPRECATED BELLESIP_EXPORT void belle_tls_verify_policy_set_exceptions(belle_tls_verify_policy_t *obj, int flags);
BELLESIP_DEPRECATED BELLESIP_EXPORT unsigned int belle_tls_verify_policy_get_exceptions(const belle_tls_verify_policy_t *obj);


/**
 * Create a new crypto configuration object
 * The crypto configuration may be passed to a http provider or a listening point using the appropriate methods
 * It can be used to provide :
 * - a path to the trusted root certificates
 * - a way to override certificate verification exceptions
 * - a ssl configuration structure provided directly to the underlying crypto library (mbedtls 2 or above),
 * @return an empty belle_tls_crypto_config object, trusted certificate path is initialised to the default system path without any warranty
 */
BELLESIP_EXPORT belle_tls_crypto_config_t *belle_tls_crypto_config_new(void);

/**
 * Set the path to the trusted certificate chain
 * @param[in/out]	obj		The crypto configuration object to set
 * @param[in]		path	The path to the trusted certificate chain file(NULL terminated string)
 *
 * @return 0 on success
 */
BELLESIP_EXPORT int belle_tls_crypto_config_set_root_ca(belle_tls_crypto_config_t *obj, const char *path);

/**
 * Set the content of the trusted certificate chain
 * @param[in/out]	obj		The crypto configuration object to set
 * @param[in]		data	The content to the trusted certificate chain data(NULL terminated string)
 *
 * @return 0 on success
 */
BELLESIP_EXPORT int belle_tls_crypto_config_set_root_ca_data(belle_tls_crypto_config_t *obj, const char *data);

/**
 * Set the exception flags to manage exception overriding during peer certificate verification
 * @param[in/out]	obj		The crypto configuration object to set
 * @param[in]		flags	Flags value to set:
 * 							BELLE_TLS_VERIFY_NONE to raise and error on any exception
 * 							BELLE_TLS_VERIFY_CN_MISMATCH to ignore Common Name mismatch
 * 							BELLE_TLS_VERIFY_ANY_REASON to ignore any exception
 *
 * @return 0 on success
 */
BELLESIP_EXPORT void belle_tls_crypto_config_set_verify_exceptions(belle_tls_crypto_config_t *obj, int flags);

/**
 * Get the exception flags used to manage exception overriding during peer certificate verification
 * @param[in]i		obj		The crypto configuration object to set
 * @return			Possible flags value :
 * 							BELLE_TLS_VERIFY_NONE to raise and error on any exception
 * 							BELLE_TLS_VERIFY_CN_MISMATCH to ignore Common Name mismatch
 * 							BELLE_TLS_VERIFY_ANY_REASON to ignore any exception
 *
 */
BELLESIP_EXPORT unsigned int belle_tls_crypto_config_get_verify_exceptions(const belle_tls_crypto_config_t *obj);

/**
 * Set the pointer to an externally provided ssl configuration for the crypto library
 * @param[in/out]	obj			The crypto configuration object to set
 * @param[in]		ssl_config	A pointer to an opaque structure which will be provided directly to the crypto library used in bctoolbox. Use with extra care.
 * 								This ssl_config structure is responsability of the caller and will not be freed at the connection's end.
 */
BELLESIP_EXPORT void belle_tls_crypto_config_set_ssl_config(belle_tls_crypto_config_t *obj, void *ssl_config);

BELLE_SIP_END_DECLS

#endif /* AUTHENTICATION_HELPER_H_ */
