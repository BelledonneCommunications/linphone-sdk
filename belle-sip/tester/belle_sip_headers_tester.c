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

	belle_sip_header_contact_t* L_contact = belle_sip_header_contact_parse("Contact: \"jéremis\" <sip:titi.com>;expires=3600;q=0.7");
	belle_sip_uri_t* L_uri = belle_sip_header_address_get_uri((belle_sip_header_address_t*)L_contact);

	CU_ASSERT_PTR_NOT_NULL(L_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");

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
void test_header_callid(void) {

	belle_sip_header_callid_t* L_callid = belle_sip_header_callid_parse("Call-ID: 1665237789@titi.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_callid_get_callid(L_callid), "1665237789@titi.com");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_callid));
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
	   if (NULL == CU_add_test(pSuite, "test of callid header", test_header_callid)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of cseq header", test_header_cseq)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of content type header", test_header_content_type)) {
	      return CU_get_error();
	   }
	   return 0;
}
