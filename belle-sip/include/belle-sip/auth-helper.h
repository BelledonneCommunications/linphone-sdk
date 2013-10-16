/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
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
 * Format of certificate buffer
 * */
typedef enum  belle_sip_certificate_raw_format {
	BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM, /** PEM format*/
	BELLE_SIP_CERTIFICATE_RAW_FORMAT_DER /** ASN.1 raw format*/
}belle_sip_certificate_raw_format_t;
/**
 * Parse a buffer containing either a certificate chain order in PEM format or a single DER cert
 * @param buff raw buffer
 * @param size buffer size
 * @param format either PEM or DER
 * @return  belle_sip_certificates_chain_t or NUL if cannot be decoded
 */

BELLESIP_EXPORT belle_sip_certificates_chain_t* belle_sip_certificates_chain_parse(const char* buff, size_t size,belle_sip_certificate_raw_format_t format);
/**
 * Parse a buffer containing either a private or public rsa key
 * @param buff raw buffer
 * @param size buffer size
 * @param passwd password (optionnal)
 * @return list of belle_sip_signing_key_t or NUL iff cannot be decoded
 */

BELLESIP_EXPORT belle_sip_signing_key_t* belle_sip_signing_key_parse(const char* buff, size_t size,const char* passwd);

BELLE_SIP_END_DECLS

#endif /* AUTHENTICATION_HELPER_H_ */
