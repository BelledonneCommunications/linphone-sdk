/*
	auth_helper.c belle-sip - SIP (RFC3261) library.
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


#include "belle-sip/auth-helper.h"
#include "belle_sip_internal.h"
#include <string.h>
#include "bctoolbox/crypto.h"

#ifndef BELLE_SIP_CNONCE_LENGTH
#define BELLE_SIP_CNONCE_LENGTH 16
#endif

#define CHECK_IS_PRESENT(obj,header_name,name) \
	if (!belle_sip_header_##header_name##_get_##name(obj)) {\
		 belle_sip_error("parameter ["#name"]not found for header ["#header_name"]");\
		 return-1;\
	}

static void belle_sip_auth_helper_clone_authorization(belle_sip_header_authorization_t* authorization, const belle_sip_header_www_authenticate_t* authentication) {
	CLONE_STRING_GENERIC(belle_sip_header_www_authenticate,belle_sip_header_authorization,scheme,authorization,authentication)
	CLONE_STRING_GENERIC(belle_sip_header_www_authenticate,belle_sip_header_authorization,realm,authorization,authentication)
	CLONE_STRING_GENERIC(belle_sip_header_www_authenticate,belle_sip_header_authorization,nonce,authorization,authentication)
	CLONE_STRING_GENERIC(belle_sip_header_www_authenticate,belle_sip_header_authorization,algorithm,authorization,authentication)
	CLONE_STRING_GENERIC(belle_sip_header_www_authenticate,belle_sip_header_authorization,opaque,authorization,authentication)
}

static void belle_sip_auth_helper_clone_www_authenticate(belle_sip_header_www_authenticate_t* authentication, const belle_sip_header_authorization_t* authorization) {
	CLONE_STRING_GENERIC(belle_sip_header_authorization,belle_sip_header_www_authenticate,scheme, authentication, authorization)
	CLONE_STRING_GENERIC(belle_sip_header_authorization,belle_sip_header_www_authenticate,realm, authentication, authorization)
	CLONE_STRING_GENERIC(belle_sip_header_authorization,belle_sip_header_www_authenticate,nonce, authentication, authorization)
	CLONE_STRING_GENERIC(belle_sip_header_authorization,belle_sip_header_www_authenticate,algorithm,authentication ,authorization)
	CLONE_STRING_GENERIC(belle_sip_header_authorization,belle_sip_header_www_authenticate,opaque,authentication, authorization)
}

belle_sip_header_authorization_t* belle_sip_auth_helper_create_authorization(const belle_sip_header_www_authenticate_t* authentication) {
	belle_sip_header_authorization_t* authorization = belle_sip_header_authorization_new();
	belle_sip_auth_helper_clone_authorization(authorization,authentication);
	return authorization;
}

belle_sip_header_www_authenticate_t* belle_sip_auth_helper_create_www_authenticate(const belle_sip_header_authorization_t* authorization) {
	belle_sip_header_www_authenticate_t* www_authenticate = belle_sip_header_www_authenticate_new();
	belle_sip_auth_helper_clone_www_authenticate(www_authenticate, authorization);
	return www_authenticate;
}

belle_http_header_authorization_t* belle_http_auth_helper_create_authorization(const belle_sip_header_www_authenticate_t* authentication) {
	belle_http_header_authorization_t* authorization = belle_http_header_authorization_new();
	belle_sip_auth_helper_clone_authorization(BELLE_SIP_HEADER_AUTHORIZATION(authorization),authentication);
	return authorization;
}

belle_sip_header_proxy_authorization_t* belle_sip_auth_helper_create_proxy_authorization(const belle_sip_header_proxy_authenticate_t* proxy_authentication){
	belle_sip_header_proxy_authorization_t* authorization = belle_sip_header_proxy_authorization_new();
	belle_sip_auth_helper_clone_authorization(BELLE_SIP_HEADER_AUTHORIZATION(authorization),BELLE_SIP_HEADER_WWW_AUTHENTICATE(proxy_authentication));
	return authorization;
}

static void belle_sip_auth_choose_method(const char *algo, const char *ask, uint8_t *out, size_t size) {
	if ((algo == NULL) || (!strcmp(algo, "MD5"))) {
		// By default, using MD5 when algorithm is NULL
		bctbx_md5((const uint8_t *)ask, strlen(ask), out);
	} else if (!strcmp(algo, "SHA-256")) {
		bctbx_sha256((const uint8_t *)ask, strlen(ask), (uint8_t)size, out);
	}
}

int belle_sip_auth_define_size(const char *algo) {
	if ((algo == NULL) || (!strcmp(algo, "MD5"))) {
		return 33;
	} else if (!strcmp(algo, "SHA-256")) {
		return 65;
	} else {
		return 0;
	}
}

int belle_sip_auth_helper_compute_ha1_for_algorithm(const char *userid, const char *realm, const char *password, char *ha1, size_t size, const char *algo) {
	size_t compared_size;
	compared_size = belle_sip_auth_define_size(algo);
	if (compared_size != size) {
		belle_sip_error("belle_sip_fill_authorization_header, size of ha1 must be 33 when MD5 or 65 when SHA-256 ");
		return -1;
	}
	size_t length_byte = (size - 1) / 2;
	uint8_t out[MAX_LENGTH_BYTE];
	size_t di;
	char *ask;
	if (!userid) {
		belle_sip_error("belle_sip_fill_authorization_header, username not found ");
		return -1;
	}
	if (!password) {
		belle_sip_error("belle_sip_fill_authorization_header, password not found ");
		return -1;
	}
	if (!realm) {
		belle_sip_error("belle_sip_fill_authorization_header, password not found ");
		return -1;
	}

	ask = bctbx_strdup_printf("%s:%s:%s", userid, realm, password);
	belle_sip_auth_choose_method(algo, ask, out, length_byte);
	for (di = 0; di < length_byte; ++di)
		sprintf(ha1 + di * 2, "%02x", out[di]);
	ha1[length_byte * 2] = '\0';
	bctbx_free(ask);
	return 0;
}

int belle_sip_auth_helper_compute_ha1(const char *userid, const char *realm, const char *password, char ha1[33]) {
	belle_sip_auth_helper_compute_ha1_for_algorithm(userid, realm, password, ha1, 33, "MD5");
	return 0;
}

int belle_sip_auth_helper_compute_ha2_for_algorithm(const char *method, const char *uri, char *ha2, size_t size, const char *algo) {
	size_t compared_size;
	compared_size = belle_sip_auth_define_size(algo);
	if (compared_size != size) {
		belle_sip_error("belle_sip_fill_authorization_header, size of ha1 must be 33 when MD5 or 65 when SHA-256 ");
		return -1;
	}
	size_t length_byte = (size - 1) / 2;
	uint8_t out[MAX_LENGTH_BYTE];
	size_t di;
	char *ask;
	ha2[length_byte * 2] = '\0';
	/*HA2=MD5(method:uri)*/

	ask = bctbx_strdup_printf("%s:%s", method, uri);
	belle_sip_auth_choose_method(algo, ask, out, length_byte);
	for (di = 0; di < length_byte; ++di)
		sprintf(ha2 + di * 2, "%02x", out[di]);
	bctbx_free(ask);
	return 0;
}

int belle_sip_auth_helper_compute_ha2(const char *method, const char *uri, char ha2[33]) {
	belle_sip_auth_helper_compute_ha2_for_algorithm(method, uri, ha2, 33, "MD5");
	return 0;
}

int belle_sip_auth_helper_compute_response_for_algorithm(const char *ha1, const char *nonce, const char *ha2, char *response, size_t size, const char *algo) {
	size_t compared_size;
	compared_size = belle_sip_auth_define_size(algo);
	if (compared_size != size) {
		belle_sip_error("belle_sip_fill_authorization_header, size of ha1 must be 33 when MD5 or 65 when SHA-256 ");
		return -1;
	}
	size_t length_byte = (size - 1) / 2;
	uint8_t out[MAX_LENGTH_BYTE];
	size_t di;
	char *ask;
	response[length_byte * 2] = '\0';

	ask = bctbx_strdup_printf("%s:%s:%s", ha1, nonce, ha2);
	belle_sip_auth_choose_method(algo, ask, out, length_byte);
	/*copy values*/
	for (di = 0; di < length_byte; ++di)
		sprintf(response + di * 2, "%02x", out[di]);
	bctbx_free(ask);
	return 0;

}

int belle_sip_auth_helper_compute_response(const char *ha1, const char *nonce, const char *ha2, char response[33]) {
	belle_sip_auth_helper_compute_response_for_algorithm(ha1, nonce, ha2, response, 33, "MD5");
	return 0;
}

int belle_sip_auth_helper_compute_response_qop_auth_for_algorithm(const char* ha1
													, const char* nonce
													, unsigned int nonce_count
													, const char* cnonce
													, const char* qop
													, const char* ha2
                                                    , char *response
                                                    , size_t size, const char* algo) {
	size_t compared_size;
	compared_size = belle_sip_auth_define_size(algo);
	if (compared_size != size) {
		belle_sip_error("belle_sip_fill_authorization_header, size of ha1 must be 33 when MD5 or 65 when SHA-256 ");
		return -1;
	}
	size_t length_byte = (size - 1) / 2;
	uint8_t out[MAX_LENGTH_BYTE];
	size_t di;
	char *ask;
	char nounce_count_as_string[9];

	response[length_byte * 2] = '\0';

	snprintf(nounce_count_as_string, sizeof(nounce_count_as_string), "%08x", nonce_count);
	/*response=MD5(HA1:nonce:nonce_count:cnonce:qop:HA2)*/

	ask = bctbx_strdup_printf("%s:%s:%s:%s:%s:%s", ha1, nonce, nounce_count_as_string, cnonce, qop, ha2);
	belle_sip_auth_choose_method(algo, ask, out, length_byte);
	/*copy values*/
	for (di = 0; di < length_byte; ++di)
		sprintf(response + di * 2, "%02x", out[di]);
	bctbx_free(ask);
	return 0;
}

int belle_sip_auth_helper_compute_response_qop_auth(const char* ha1
                                                    , const char* nonce
                                                    , unsigned int nonce_count
                                                    , const char* cnonce
                                                    , const char* qop
                                                    , const char* ha2, char response[33]) {
	belle_sip_auth_helper_compute_response_qop_auth_for_algorithm(ha1, nonce, nonce_count, cnonce, qop, ha2, response, 33, "MD5");
	return 0;
}

int belle_sip_auth_helper_fill_authorization(belle_sip_header_authorization_t* authorization
											,const char* method
											,const char* ha1) {
	const char *algo = belle_sip_header_authorization_get_algorithm(authorization);
	size_t size = belle_sip_auth_define_size(algo);
	if (!size) {
		belle_sip_error("Algorithm [%s] is not correct ", algo);
		return -1;
	}
	int auth_mode=0;
	char* uri;
	char ha2[MAX_RESPONSE_SIZE];
	char response[MAX_RESPONSE_SIZE];
	char cnonce[BELLE_SIP_CNONCE_LENGTH + 1];

	response[size-1]=ha2[size-1]='\0';

	if (belle_sip_header_authorization_get_scheme(authorization) != NULL &&
		strcmp("Digest",belle_sip_header_authorization_get_scheme(authorization))!=0) {
		belle_sip_error("belle_sip_fill_authorization_header, unsupported schema [%s]"
						,belle_sip_header_authorization_get_scheme(authorization));
		return -1;
	}
	if (belle_sip_header_authorization_get_qop(authorization)
		&& !(auth_mode=strcmp("auth",belle_sip_header_authorization_get_qop(authorization))==0)) {
		belle_sip_error("belle_sip_fill_authorization_header, unsupported qop [%s], use auth or nothing instead"
								,belle_sip_header_authorization_get_qop(authorization));
		return -1;
	}
	CHECK_IS_PRESENT(authorization,authorization,realm)
	CHECK_IS_PRESENT(authorization,authorization,nonce)
	if (BELLE_SIP_IS_INSTANCE_OF(authorization,belle_http_header_authorization_t)) {
		/*http case*/
		if (!belle_http_header_authorization_get_uri(BELLE_HTTP_HEADER_AUTHORIZATION(authorization))) {
			 belle_sip_error("parameter uri not found for http header authorization");
			 return-1;
		}
	} else {
		CHECK_IS_PRESENT(authorization,authorization,uri)
	}
	if (auth_mode) {
		CHECK_IS_PRESENT(authorization,authorization,nonce_count)
		if (!belle_sip_header_authorization_get_cnonce(authorization)) {
			belle_sip_header_authorization_set_cnonce(authorization, belle_sip_random_token((cnonce), sizeof(cnonce)));
		}
	}
	if (!method) {
		 belle_sip_error("belle_sip_fill_authorization_header, method not found ");
		 return -1;
	}

	if (BELLE_SIP_IS_INSTANCE_OF(authorization,belle_http_header_authorization_t)) {
			/*http case*/
		uri=belle_generic_uri_to_string(belle_http_header_authorization_get_uri(BELLE_HTTP_HEADER_AUTHORIZATION(authorization)));
	} else {
		uri=belle_sip_uri_to_string(belle_sip_header_authorization_get_uri(authorization));
	}

	belle_sip_auth_helper_compute_ha2_for_algorithm(method,uri,ha2,size,algo);
	belle_sip_free(uri);
	if (auth_mode) {
		/*response=MD5(HA1:nonce:nonce_count:cnonce:qop:HA2)*/
		belle_sip_auth_helper_compute_response_qop_auth_for_algorithm(ha1
														,belle_sip_header_authorization_get_nonce(authorization)
														,belle_sip_header_authorization_get_nonce_count(authorization)
														,belle_sip_header_authorization_get_cnonce(authorization)
														,belle_sip_header_authorization_get_qop(authorization)
														,ha2
														,response
                                                        ,size
                                                        ,algo);
	}  else {
		/*response=MD5(ha1:nonce:ha2)*/
		belle_sip_auth_helper_compute_response_for_algorithm(ha1,belle_sip_header_authorization_get_nonce(authorization),ha2,response,size,algo);
	}
	belle_sip_header_authorization_set_response(authorization,(const char*)response);
	return 0;
}


int belle_sip_auth_helper_fill_proxy_authorization(belle_sip_header_proxy_authorization_t* proxy_authorization
												,const char* method
												,const char* ha1) {
	return belle_sip_auth_helper_fill_authorization(BELLE_SIP_HEADER_AUTHORIZATION(proxy_authorization)
													,method, ha1);


}
