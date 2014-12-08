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

#include "belle-sip/auth-helper.h"
#include "belle_sip_tester.h"
#include <stdio.h>
#include "CUnit/Basic.h"


static void test_authentication(void) {
	const char* l_raw_header = "WWW-Authenticate: Digest "
				"algorithm=MD5, realm=\"sip.linphone.org\", opaque=\"1bc7f9097684320\","
				" nonce=\"cz3h0gAAAAC06TKKAABmTz1V9OcAAAAA\"";
	char ha1[33];
	belle_sip_header_www_authenticate_t* www_authenticate=belle_sip_header_www_authenticate_parse(l_raw_header);
	belle_sip_header_authorization_t* authorization = belle_sip_auth_helper_create_authorization(www_authenticate);
	belle_sip_header_authorization_set_uri(authorization,belle_sip_uri_parse("sip:sip.linphone.org"));
	CU_ASSERT_EQUAL_FATAL(0,belle_sip_auth_helper_compute_ha1("jehan-mac","sip.linphone.org","toto",ha1));
	CU_ASSERT_EQUAL_FATAL(0,belle_sip_auth_helper_fill_authorization(authorization,"REGISTER",ha1));
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_response(authorization),"77ebf3de72e41934d806175586086508");
	belle_sip_object_unref(www_authenticate);
	belle_sip_object_unref(authorization);
}

static void test_authentication_qop_auth(void) {
	const char* l_raw_header = "WWW-Authenticate: Digest "
				"algorithm=MD5, realm=\"sip.linphone.org\", opaque=\"1bc7f9097684320\","
				" qop=\"auth,auth-int\", nonce=\"cz3h0gAAAAC06TKKAABmTz1V9OcAAAAA\"";
	char ha1[33];
	belle_sip_header_www_authenticate_t* www_authenticate=belle_sip_header_www_authenticate_parse(l_raw_header);
	belle_sip_header_authorization_t* authorization = belle_sip_auth_helper_create_authorization(www_authenticate);
	belle_sip_header_authorization_set_uri(authorization,belle_sip_uri_parse("sip:sip.linphone.org"));
	belle_sip_header_authorization_set_nonce_count(authorization,1);
	belle_sip_header_authorization_set_qop(authorization,"auth");
	belle_sip_header_authorization_set_cnonce(authorization,"8302210f"); /*for testing purpose*/
	CU_ASSERT_EQUAL_FATAL(0,belle_sip_auth_helper_compute_ha1("jehan-mac","sip.linphone.org","toto",ha1));
	CU_ASSERT_EQUAL_FATAL(0,belle_sip_auth_helper_fill_authorization(authorization,"REGISTER",ha1));
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_qop(authorization),"auth");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_response(authorization),"694dab8dfe7d50d28ba61e8c43e30666");
	CU_ASSERT_EQUAL(belle_sip_header_authorization_get_nonce_count(authorization),1);
	belle_sip_object_unref(www_authenticate);
	belle_sip_object_unref(authorization);
}

static void test_proxy_authentication(void) {
	const char* l_raw_header = "Proxy-Authenticate: Digest "
				"algorithm=MD5, realm=\"sip.linphone.org\", opaque=\"1bc7f9097684320\","
				" qop=\"auth,auth-int\", nonce=\"cz3h0gAAAAC06TKKAABmTz1V9OcAAAAA\"";
	char ha1[33];
	belle_sip_header_proxy_authenticate_t* proxy_authenticate=belle_sip_header_proxy_authenticate_parse(l_raw_header);
	belle_sip_header_proxy_authorization_t* proxy_authorization = belle_sip_auth_helper_create_proxy_authorization(proxy_authenticate);
	belle_sip_header_authorization_set_uri(BELLE_SIP_HEADER_AUTHORIZATION(proxy_authorization),belle_sip_uri_parse("sip:sip.linphone.org"));
	CU_ASSERT_EQUAL_FATAL(0,belle_sip_auth_helper_compute_ha1("jehan-mac","sip.linphone.org","toto",ha1));
	CU_ASSERT_EQUAL_FATAL(0,belle_sip_auth_helper_fill_proxy_authorization(proxy_authorization,"REGISTER",ha1));
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_response(BELLE_SIP_HEADER_AUTHORIZATION(proxy_authorization))
							,"77ebf3de72e41934d806175586086508");
	belle_sip_object_unref(proxy_authenticate);
	belle_sip_object_unref(proxy_authorization);

}

#define TEMPORARY_CERTIFICATE_DIR "/belle_sip_tester_crt"

static void test_generate_and_parse_certificates(void) {
/* function not available on windows yet - need to add the create and parse directory */
#ifndef WIN32
	belle_sip_certificates_chain_t *certificate, *parsed_certificate;
	belle_sip_signing_key_t *key, *parsed_key;
	unsigned char *pem_certificate, *pem_parsed_certificate, *pem_key, *pem_parsed_key;
	int ret = 0;
	char *belle_sip_certificate_temporary_dir=belle_sip_malloc(strlen(belle_sip_tester_writable_dir_prefix)+strlen(TEMPORARY_CERTIFICATE_DIR)+1);
	strcpy(belle_sip_certificate_temporary_dir, belle_sip_tester_writable_dir_prefix);
	memcpy(belle_sip_certificate_temporary_dir+strlen(belle_sip_tester_writable_dir_prefix), TEMPORARY_CERTIFICATE_DIR, strlen(TEMPORARY_CERTIFICATE_DIR)+1);

	/* create 2 certificates in ./belle_sip_crt_test directory (TODO : set the directory in a absolute path?? where?)*/
	ret = belle_sip_generate_self_signed_certificate(belle_sip_certificate_temporary_dir, "test_certificate1", &certificate, &key);
	CU_ASSERT_EQUAL_FATAL(0, ret);
	ret = belle_sip_generate_self_signed_certificate(belle_sip_certificate_temporary_dir, "test_certificate2", &certificate, &key);
	CU_ASSERT_EQUAL_FATAL(0, ret);

	/* parse directory to get the certificate2 */
	ret = belle_sip_get_certificate_and_pkey_in_dir(belle_sip_certificate_temporary_dir, "test_certificate2", &parsed_certificate, &parsed_key, BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
	belle_sip_free(belle_sip_certificate_temporary_dir);
	CU_ASSERT_EQUAL_FATAL(0, ret);

	/* get pem version of generated and parsed certificate and compare them */
	pem_certificate = belle_sip_get_certificates_pem(certificate);
	CU_ASSERT_TRUE_FATAL(pem_certificate!=NULL);
	pem_parsed_certificate = belle_sip_get_certificates_pem(parsed_certificate);
	CU_ASSERT_TRUE_FATAL(pem_parsed_certificate!=NULL);
	CU_ASSERT_STRING_EQUAL(pem_certificate, pem_parsed_certificate);

	/* get pem version of generated and parsed key and compare them */
	pem_key = belle_sip_get_key_pem(key);
	CU_ASSERT_TRUE_FATAL(pem_key!=NULL);
	pem_parsed_key = belle_sip_get_key_pem(parsed_key);
	CU_ASSERT_TRUE_FATAL(pem_parsed_key!=NULL);
	CU_ASSERT_STRING_EQUAL(pem_key, pem_parsed_key);

	belle_sip_free(pem_certificate);
	belle_sip_free(pem_parsed_certificate);
	belle_sip_free(pem_key);
	belle_sip_free(pem_parsed_key);
	belle_sip_object_unref(certificate);
	belle_sip_object_unref(parsed_certificate);
	belle_sip_object_unref(key);
	belle_sip_object_unref(parsed_key);
#endif /* WIN32 */
}

static void test_certificate_fingerprint(void) {
	unsigned char *fingerprint;
	/* parse certificate defined in belle_sip_register_tester.c */
	belle_sip_certificates_chain_t* cert = belle_sip_certificates_chain_parse(belle_sip_tester_client_cert,strlen(belle_sip_tester_client_cert),BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
	/* generate fingerprint */
	fingerprint = belle_sip_generate_certificate_fingerprint(cert);

	CU_ASSERT_STRING_EQUAL(fingerprint, belle_sip_tester_client_cert_fingerprint);

	free(fingerprint);
	belle_sip_object_unref(cert);
}

test_t authentication_helper_tests[] = {
	{ "Proxy-Authenticate", test_proxy_authentication },
	{ "WWW-Authenticate", test_authentication },
	{ "WWW-Authenticate (with qop)", test_authentication_qop_auth },
	{ "generate and parse self signed certificates", test_generate_and_parse_certificates},
	{ "generate certificate fingerprint", test_certificate_fingerprint}
};

test_suite_t authentication_helper_test_suite = {
	"Authentication helper",
	NULL,
	NULL,
	sizeof(authentication_helper_tests) / sizeof(authentication_helper_tests[0]),
	authentication_helper_tests
};

