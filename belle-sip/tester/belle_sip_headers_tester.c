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


#include "belle-sip/belle-sip.h"
#include <stdio.h>
#include "CUnit/Basic.h"

int init_suite1(void) {
      return 0;
}

int clean_suite1(void) {
      return 0;
}


void test_simple_header_contact(void) {

	belle_sip_header_contact_t* L_contact = belle_sip_header_contact_parse("Contact:sip:titi.com");
	belle_sip_uri_t* L_uri = belle_sip_header_address_get_uri((belle_sip_header_address_t*)L_contact);

	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");
	CU_ASSERT_PTR_NULL(belle_sip_uri_get_transport_param(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_contact));
}

void test_complex_header_contact(void) {

	belle_sip_header_contact_t* L_contact = belle_sip_header_contact_parse("Contact: \"jéremis\" <sip:sip.linphone.org>;expires=3600;q=0.7");
	belle_sip_uri_t* L_uri = belle_sip_header_address_get_uri((belle_sip_header_address_t*)L_contact);

	CU_ASSERT_PTR_NOT_NULL(L_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "sip.linphone.org");

	CU_ASSERT_STRING_EQUAL(belle_sip_header_address_get_displayname((belle_sip_header_address_t*)L_contact), "jéremis");

	CU_ASSERT_EQUAL(belle_sip_header_contact_get_expires(L_contact),3600);
	float l_qvalue = belle_sip_header_contact_get_qvalue(L_contact);
	CU_ASSERT_EQUAL(l_qvalue,0.7);

	belle_sip_object_unref(BELLE_SIP_OBJECT(L_contact));

	L_contact = belle_sip_header_contact_parse("Contact: toto <sip:titi.com>;expires=3600; q=0.7");

	CU_ASSERT_STRING_EQUAL(belle_sip_header_address_get_displayname((belle_sip_header_address_t*)L_contact), "toto");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_contact));

}

void test_simple_header_from(void) {

	belle_sip_header_from_t* L_from = belle_sip_header_from_parse("From:<sip:titi.com;transport=tcp>;tag=dlfjklcn6545614XX");
	belle_sip_uri_t* L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_from));

	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_from_get_tag(L_from),"dlfjklcn6545614XX");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_from));
}

void test_simple_header_to(void) {

	belle_sip_header_to_t* L_to = belle_sip_header_to_parse("To : < sip:titi.com;transport=tcp> ; tag = dlfjklcn6545614XX");
	belle_sip_uri_t* L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_to));

	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_to_get_tag(L_to),"dlfjklcn6545614XX");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_to));
}
void test_header_via(void) {

	belle_sip_header_via_t* L_via = belle_sip_header_via_parse("Via: SIP/2.0/UDP 192.168.0.19:5062;rport;received=192.169.0.4;branch=z9hG4bK368560724");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_protocol(L_via), "SIP/2.0");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_transport(L_via), "UDP");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_host(L_via), "192.168.0.19");
	CU_ASSERT_EQUAL(belle_sip_header_via_get_port(L_via),5062);

	CU_ASSERT_TRUE(belle_sip_parameters_is_parameter(BELLE_SIP_PARAMETERS(L_via),"rport"));
	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_received(L_via),"192.169.0.4");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_branch(L_via),"z9hG4bK368560724");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_via));
}
void test_header_call_id(void) {

	belle_sip_header_call_id_t* L_call_id = belle_sip_header_call_id_parse("Call-ID: 1665237789@titi.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_call_id_get_call_id(L_call_id), "1665237789@titi.com");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_call_id));
}
void test_header_cseq(void) {

	belle_sip_header_cseq_t* L_cseq = belle_sip_header_cseq_parse("CSeq: 21 INVITE");
	CU_ASSERT_EQUAL(belle_sip_header_cseq_get_seq_number(L_cseq),21);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_cseq_get_method(L_cseq),"INVITE");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_cseq));
}
void test_header_content_type(void) {

	belle_sip_header_content_type_t* L_content_type = belle_sip_header_content_type_parse("Content-Type: text/html; charset=ISO-8859-4");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_content_type_get_type(L_content_type),"text");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_content_type_get_subtype(L_content_type),"html");
	CU_ASSERT_STRING_EQUAL(belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(L_content_type),"charset"),"ISO-8859-4");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_type));

	L_content_type = belle_sip_header_content_type_parse("Content-Type: application/sdp");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_content_type_get_type(L_content_type),"application");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_content_type_get_subtype(L_content_type),"sdp");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_type));

	L_content_type = belle_sip_header_content_type_parse("Content-Type: application/pkcs7-mime; smime-type=enveloped-data; \r\n name=smime.p7m");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_type));

}
void test_header_record_route(void) {

	belle_sip_header_record_route_t* L_record_route = belle_sip_header_record_route_parse("Record-Route: <sip:212.27.52.5:5060;transport=udp;lr>;charset=ISO-8859-4");
	belle_sip_uri_t* L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_record_route));
	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "212.27.52.5");
	CU_ASSERT_STRING_EQUAL(belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(L_record_route),"charset"),"ISO-8859-4");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_record_route));
}
void test_header_route(void) {

	belle_sip_header_route_t* L_route = belle_sip_header_route_parse("Route: <sip:212.27.52.5:5060;transport=udp;lr>;charset=ISO-8859-4");
	belle_sip_uri_t* L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_route));
	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_EQUAL(belle_sip_uri_get_port(L_uri), 5060);
	CU_ASSERT_STRING_EQUAL(belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(L_route),"charset"),"ISO-8859-4");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_route));
}
void test_header_content_length(void) {

	belle_sip_header_content_length_t* L_content_length = belle_sip_header_content_length_parse("Content-Length: 3495");
	CU_ASSERT_EQUAL(belle_sip_header_content_length_get_content_length(L_content_length), 3495);
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_length));
}
void test_header_extention(void) {
	belle_sip_header_extension_t* L_extension = belle_sip_header_extension_parse("toto: titi");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(L_extension), "titi");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_extension));
}
void test_header_authorization(void) {
	const char* l_raw_header = "Authorization: Digest username=\"0033482532176\", "\
			"realm=\"sip.ovh.net\", nonce=\"1bcdcb194b30df5f43973d4c69bdf54f\", uri=\"sip:sip.ovh.net\", response=\"eb36c8d5c8642c1c5f44ec3404613c81\","\
			"algorithm=MD5, opaque=\"1bc7f9097684320\"";
	belle_sip_header_authorization_t* L_authorization = belle_sip_header_authorization_parse(l_raw_header);
	CU_ASSERT_PTR_NOT_NULL(L_authorization);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_username(L_authorization), "0033482532176");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_realm(L_authorization), "sip.ovh.net");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_nonce(L_authorization), "1bcdcb194b30df5f43973d4c69bdf54f");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_header_authorization_get_uri(L_authorization));
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_response(L_authorization), "eb36c8d5c8642c1c5f44ec3404613c81");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_authorization));
}

int belle_sip_headers_test_suite() {
	
	   CU_pSuite pSuite = NULL;
	   /* add a suite to the registry */
	   pSuite = CU_add_suite("header_suite", init_suite1, clean_suite1);

	   /* add the tests to the suite */
	   /* NOTE - ORDER IS IMPORTANT - MUST TEST fread() AFTER fprintf() */
	   if (NULL == CU_add_test(pSuite, "test of simple contact header", test_simple_header_contact)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of complex contact header", test_complex_header_contact)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of from header", test_simple_header_from)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of to header", test_simple_header_to)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of via header", test_header_via)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of call_id header", test_header_call_id)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of cseq header", test_header_cseq)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of content type header", test_header_content_type)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of record route header", test_header_record_route)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of route header", test_header_route)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of content lenth", test_header_content_length)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of extension", test_header_extention)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of authorization", test_header_authorization)) {
	      return CU_get_error();
	   }
	   return 0;
}
