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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "belle-sip/auth-helper.h"
#include "belle_sip_tester.h"
#include <bctoolbox/crypto.h>

static void test_authentication(void) {
	const char* l_raw_header = "WWW-Authenticate: Digest "
				"algorithm=MD5, realm=\"sip.linphone.org\", opaque=\"1bc7f9097684320\","
				" nonce=\"cz3h0gAAAAC06TKKAABmTz1V9OcAAAAA\"";
	char ha1[33];
	belle_sip_header_www_authenticate_t* www_authenticate=belle_sip_header_www_authenticate_parse(l_raw_header);
	belle_sip_header_authorization_t* authorization = belle_sip_auth_helper_create_authorization(www_authenticate);
	belle_sip_header_authorization_set_uri(authorization,belle_sip_uri_parse("sip:sip.linphone.org"));
	BC_ASSERT_EQUAL(0,belle_sip_auth_helper_compute_ha1("jehan-mac","sip.linphone.org","toto",ha1), int, "%d");
	BC_ASSERT_EQUAL(0,belle_sip_auth_helper_fill_authorization(authorization,"REGISTER",ha1), int, "%d");
	BC_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_response(authorization),"77ebf3de72e41934d806175586086508");
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
	BC_ASSERT_EQUAL(0,belle_sip_auth_helper_compute_ha1("jehan-mac","sip.linphone.org","toto",ha1), int, "%d");
	BC_ASSERT_EQUAL(0,belle_sip_auth_helper_fill_authorization(authorization,"REGISTER",ha1), int, "%d");
	BC_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_qop(authorization),"auth");
	BC_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_response(authorization),"694dab8dfe7d50d28ba61e8c43e30666");
	BC_ASSERT_EQUAL(belle_sip_header_authorization_get_nonce_count(authorization),1, int, "%d");
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
	BC_ASSERT_EQUAL(0,belle_sip_auth_helper_compute_ha1("jehan-mac","sip.linphone.org","toto",ha1), int, "%d");
	BC_ASSERT_EQUAL(0,belle_sip_auth_helper_fill_proxy_authorization(proxy_authorization,"REGISTER",ha1), int, "%d");
	BC_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_response(BELLE_SIP_HEADER_AUTHORIZATION(proxy_authorization))
							,"77ebf3de72e41934d806175586086508");
	belle_sip_object_unref(proxy_authenticate);
	belle_sip_object_unref(proxy_authorization);

}

#define TEMPORARY_CERTIFICATE_DIR "/belle_sip_tester_crt"

static void test_generate_and_parse_certificates(void) {
	belle_sip_certificates_chain_t *certificate, *parsed_certificate;
	belle_sip_signing_key_t *key, *parsed_key;
	char *pem_certificate, *pem_parsed_certificate, *pem_key, *pem_parsed_key;
	int ret = 0;
	char *belle_sip_certificate_temporary_dir = bc_tester_file(TEMPORARY_CERTIFICATE_DIR);

	/* create 2 certificates in the temporary certificate directory (TODO : set the directory in a absolute path?? where?)*/
	ret = belle_sip_generate_self_signed_certificate(belle_sip_certificate_temporary_dir, "test_certificate1", &certificate, &key);
	if (ret == BCTBX_ERROR_UNAVAILABLE_FUNCTION) {
		belle_sip_warning("Test skipped, self signed certificate generation not available.");
		return;
	}
	BC_ASSERT_EQUAL(0, ret, int, "%d");
	if (ret == 0) {
		belle_sip_object_unref(certificate);
		belle_sip_object_unref(key);
	}
	ret = belle_sip_generate_self_signed_certificate(belle_sip_certificate_temporary_dir, "test_certificate2", &certificate, &key);
	BC_ASSERT_EQUAL(0, ret, int, "%d");

	/* parse directory to get the certificate2 */
	ret = belle_sip_get_certificate_and_pkey_in_dir(belle_sip_certificate_temporary_dir, "test_certificate2", &parsed_certificate, &parsed_key, BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
	free(belle_sip_certificate_temporary_dir);
	BC_ASSERT_EQUAL(0, ret, int, "%d");

	/* get pem version of generated and parsed certificate and compare them */
	pem_certificate = belle_sip_certificates_chain_get_pem(certificate);
	pem_parsed_certificate = belle_sip_certificates_chain_get_pem(parsed_certificate);
	if (BC_ASSERT_PTR_NOT_NULL(pem_certificate) && BC_ASSERT_PTR_NOT_NULL(pem_parsed_certificate)) {
		BC_ASSERT_STRING_EQUAL(pem_certificate, pem_parsed_certificate);
	}

	/* get pem version of generated and parsed key and compare them */
	pem_key = belle_sip_signing_key_get_pem(key);
	pem_parsed_key = belle_sip_signing_key_get_pem(parsed_key);
	if (BC_ASSERT_PTR_NOT_NULL(pem_key) && BC_ASSERT_PTR_NOT_NULL(pem_parsed_key)) {
		BC_ASSERT_STRING_EQUAL(pem_key, pem_parsed_key);
	}

	belle_sip_free(pem_certificate);
	belle_sip_free(pem_parsed_certificate);
	belle_sip_free(pem_key);
	belle_sip_free(pem_parsed_key);
	belle_sip_object_unref(certificate);
	belle_sip_object_unref(parsed_certificate);
	belle_sip_object_unref(key);
	belle_sip_object_unref(parsed_key);
}


const char* belle_sip_tester_fingerprint256_cert = /*for URI:sip:tester@client.example.org*/
		"-----BEGIN CERTIFICATE-----\n"
		"MIIDtTCCAh2gAwIBAgIBATANBgkqhkiG9w0BAQsFADAcMRowGAYDVQQDExF0ZXN0\n"
		"X2NlcnRpZmljYXRlMTAeFw0wMTAxMDEwMDAwMDBaFw0zMDAxMDEwMDAwMDBaMBwx\n"
		"GjAYBgNVBAMTEXRlc3RfY2VydGlmaWNhdGUxMIIBojANBgkqhkiG9w0BAQEFAAOC\n"
		"AY8AMIIBigKCAYEAoI6Dpdyc8ARM9KTIkuagImUgpybuWrKayPfrAeUE/gnyd8bO\n"
		"Bf7CkGdpHv82c1BdUxE5Z1j19TMR0MHCtFD5z0PWtW3erWQqUdxdFYIUknIi5ObU\n"
		"AlXgqAIYLCSMaGWzmavdsC95HfHiuPC+YTLwr1vhNC6IWCSKt9N7xek/InY73cBh\n"
		"pNw/kJOB/AzB9r40uxcye6+6Hp3dAd2YOGOiuKlAFBlAeq/T70VKBvdw/D8QFi5Z\n"
		"BJ2+xX9jQBshzHi9JdMS6ZhLdtjBHwi37k1l1KyRh+qVTbze5pN7YCRmj8Q4dS0S\n"
		"3ozV27AXM60kXbX4+PWQG9nuL/PO2NxTx0olIaTkzjM+roxWE6srhAEQ+aXn3tCq\n"
		"bHND6AN2Yjm/mzQI2ig143gHraLRaHx+uTtRonMeWMvTeUlX/BwUoffjppmWqICd\n"
		"OiBFNXOpp3hlzZDdoEhwKgIVMu3WbEsOTG7uphkUGZo/VaTVW0zvYAS2JXC/0s/S\n"
		"85dB5M3Y9l/8v0T7AgMBAAGjAjAAMA0GCSqGSIb3DQEBCwUAA4IBgQBm5N00W7+G\n"
		"ygF6OUM3143N5B/41vTk5FDZ/iU/UJaPSLBM/aZhA2FjoTswjpFfY8V6IkALrtUH\n"
		"20FVip3lguMc7md9L9qMRVYj/2H94A2Bg/zx+PlhJNI0bshITzS6pHgM2qKk+KRB\n"
		"yZaHQTa8DjRCYuAp1roh4NKNDa16WdY4Dk5ncRORqzcxczBJ2LSbq4b78pdEl/iL\n"
		"nHOoFOSmiQQ2ui7H89bSUxRmVJFiNfPlTeYUKjc753LJCuri30rQVnHE+HMBmE5y\n"
		"sM6FiGawJxUKAcS0zuKeroHNXLzL0qIGgeLkoPb267se0tCAcJZImiqyK0y1cuHw\n"
		"o9BZ5t/I6UvTJLE9+p+wG7nR8TdszaZ+bLzSdHWDRPS2Ux4J+Ux3dnIAH/ZcD5CD\n"
		"/mj4F12yW0ZNukFVkptneS6ab1lQb3PT7tzkuzKud00QNHswZLbORQrXnvuk5LrR\n"
		"V7PbeVUz1FxaOjFwHXkkvFqrbwRdBc7GVqQZDVV40WVvciGGcBhemqc=\n"
		"-----END CERTIFICATE-----";

/* fingerprint of certificate generated using openssl x509 -fingerprint -sha256 */
const char* belle_sip_tester_fingerprint256_cert_fingerprint =
		"SHA-256 A0:98:2D:3E:68:F3:14:8D:ED:50:40:DB:ED:A4:28:BC:1E:1A:6A:05:59:9E:69:3F:02:E2:F8:22:BF:4C:92:14";

static void test_certificate_fingerprint(void) {
	char *fingerprint;
	belle_sip_certificates_chain_t *cert;

	/* check underlying bctoolbox function availability */
	if (bctbx_x509_certificate_get_fingerprint(NULL, NULL, 0, 0) == BCTBX_ERROR_UNAVAILABLE_FUNCTION) {
		belle_sip_warning("Test skipped, certificate fingerprint generation not available.");
		return;
	}
	/* parse certificate defined in belle_sip_register_tester.c */
	cert = belle_sip_certificates_chain_parse(belle_sip_tester_client_cert,strlen(belle_sip_tester_client_cert),BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
	/* generate fingerprint */
	fingerprint = belle_sip_certificates_chain_get_fingerprint(cert);

	BC_ASSERT_PTR_NOT_NULL(fingerprint);
	if (fingerprint) {
		BC_ASSERT_STRING_EQUAL(fingerprint, belle_sip_tester_client_cert_fingerprint);
	}
	belle_sip_free(fingerprint);
	belle_sip_object_unref(cert);

	/* parse certificate defined above, signing algo is sha256 */
	cert = belle_sip_certificates_chain_parse(belle_sip_tester_fingerprint256_cert,strlen(belle_sip_tester_fingerprint256_cert),BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
	/* generate fingerprint */
	fingerprint = belle_sip_certificates_chain_get_fingerprint(cert);

	BC_ASSERT_PTR_NOT_NULL(fingerprint);
	if (fingerprint) {
		BC_ASSERT_STRING_EQUAL(fingerprint, belle_sip_tester_fingerprint256_cert_fingerprint);
	}

	belle_sip_free(fingerprint);
	belle_sip_object_unref(cert);
}

test_t authentication_helper_tests[] = {
	TEST_NO_TAG("Proxy-Authenticate", test_proxy_authentication),
	TEST_NO_TAG("WWW-Authenticate", test_authentication),
	TEST_NO_TAG("WWW-Authenticate (with qop)", test_authentication_qop_auth),
	TEST_NO_TAG("generate and parse self signed certificates", test_generate_and_parse_certificates),
	TEST_NO_TAG("generate certificate fingerprint", test_certificate_fingerprint)
};

test_suite_t authentication_helper_test_suite = {
	"Authentication helper", NULL, NULL, belle_sip_tester_before_each, belle_sip_tester_after_each,
	sizeof(authentication_helper_tests) / sizeof(authentication_helper_tests[0]), authentication_helper_tests};
