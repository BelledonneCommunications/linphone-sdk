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
#include "belle_sip_internal.h"
#include <stdio.h>
#include "CUnit/Basic.h"

int init_suite1(void) {
      return 0;
}

int clean_suite1(void) {
      return 0;
}


void test_simple_header_contact(void) {
	belle_sip_header_contact_t* L_tmp;
	belle_sip_uri_t* L_uri;
	belle_sip_header_contact_t* L_contact = belle_sip_header_contact_parse("Contact:sip:titi.com");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_contact));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_contact));
	L_tmp = belle_sip_header_contact_parse(l_raw_header);
	L_contact = BELLE_SIP_HEADER_CONTACT(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);

	L_uri = belle_sip_header_address_get_uri((belle_sip_header_address_t*)L_contact);

	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");
	CU_ASSERT_PTR_NULL(belle_sip_uri_get_transport_param(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_contact));
}

void test_complex_header_contact(void) {
	belle_sip_header_contact_t* L_contact;
	belle_sip_uri_t* L_uri;
	belle_sip_header_contact_t* L_tmp = belle_sip_header_contact_parse("Contact: \"jéremis\" <sip:sip.linphone.org>;expires=3600;q=0.7, sip:titi.com");
	belle_sip_header_t* l_next;
	belle_sip_header_contact_t* L_next_contact;
	char* l_raw_header;
	float l_qvalue;

	L_contact = BELLE_SIP_HEADER_CONTACT(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));

	L_uri = belle_sip_header_address_get_uri((belle_sip_header_address_t*)L_contact);

	CU_ASSERT_PTR_NOT_NULL(L_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "sip.linphone.org");

	CU_ASSERT_STRING_EQUAL(belle_sip_header_address_get_displayname((belle_sip_header_address_t*)L_contact), "jéremis");

	CU_ASSERT_EQUAL(belle_sip_header_contact_get_expires(L_contact),3600);
	l_qvalue = belle_sip_header_contact_get_qvalue(L_contact);
	CU_ASSERT_EQUAL(l_qvalue,0.7f);

	l_next = belle_sip_header_get_next(BELLE_SIP_HEADER(L_contact));
	L_next_contact = BELLE_SIP_HEADER_CONTACT(l_next);
	CU_ASSERT_PTR_NOT_NULL(L_next_contact);
	CU_ASSERT_PTR_NOT_NULL( belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_contact)));

	belle_sip_object_unref(BELLE_SIP_OBJECT(L_contact));

	L_contact = belle_sip_header_contact_parse("Contact: toto <sip:titi.com>;expires=3600; q=0.7");
	l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_contact));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_contact));
	L_contact = belle_sip_header_contact_parse(l_raw_header);
	belle_sip_free(l_raw_header);

	CU_ASSERT_STRING_EQUAL(belle_sip_header_address_get_displayname((belle_sip_header_address_t*)L_contact), "toto");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_contact));

}

void test_simple_header_from(void) {
	belle_sip_header_from_t* L_tmp;
	belle_sip_uri_t* L_uri;
	belle_sip_header_from_t* L_from = belle_sip_header_from_parse("From:<sip:titi.com;transport=tcp>;tag=dlfjklcn6545614XX");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_from));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_from));
	L_tmp = belle_sip_header_from_parse(l_raw_header);
	L_from = BELLE_SIP_HEADER_FROM(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);

	L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_from));

	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_from_get_tag(L_from),"dlfjklcn6545614XX");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_from));

	/*test factory*/
	L_from = belle_sip_header_from_create2("super <sip:titi.com>","12345-abc");
	CU_ASSERT_PTR_NOT_NULL_FATAL(L_from);
	L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_from));
	CU_ASSERT_PTR_NOT_NULL_FATAL(L_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_from_get_tag(L_from),"12345-abc");
	belle_sip_object_unref(L_from);
}

void test_header_from_with_paramless_address_spec(void) {
	belle_sip_header_from_t* L_from = belle_sip_header_from_parse("From: sip:bob@titi.com;tag=dlfjklcn6545614XX");
	CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_header_from_get_tag(L_from));
	CU_ASSERT_STRING_EQUAL(belle_sip_header_from_get_tag(L_from),"dlfjklcn6545614XX");
	belle_sip_object_unref(L_from);
}
void test_header_to_with_paramless_address_spec(void) {
	belle_sip_header_to_t* L_to = belle_sip_header_to_parse("To: sip:bob@titi.com;tag=dlfjklcn6545614XX");
	CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_header_to_get_tag(L_to));
	CU_ASSERT_STRING_EQUAL(belle_sip_header_to_get_tag(L_to),"dlfjklcn6545614XX");
	belle_sip_object_unref(L_to);
}
void test_header_contact_with_paramless_address_spec(void) {
	belle_sip_header_contact_t* L_contact = belle_sip_header_contact_parse("Contact: sip:bob@titi.com;expires=60");
	CU_ASSERT_EQUAL(belle_sip_header_contact_get_expires(L_contact),60);
	belle_sip_object_unref(L_contact);
}

void test_simple_header_to(void) {
	belle_sip_uri_t* L_uri;
	belle_sip_header_to_t* L_to = belle_sip_header_to_parse("To : < sip:titi.com;transport=tcp> ; tag = dlfjklcn6545614XX");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_to));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_to));
	L_to = belle_sip_header_to_parse(l_raw_header);
	belle_sip_free(l_raw_header);

	L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_to));

	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_to_get_tag(L_to),"dlfjklcn6545614XX");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_to));
	/*test factory*/
	L_to = belle_sip_header_to_create2("\"super man\" <sip:titi.com>","12345-abc");
	CU_ASSERT_PTR_NOT_NULL_FATAL(L_to);
	L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_to));
	CU_ASSERT_PTR_NOT_NULL_FATAL(L_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_address_get_displayname(BELLE_SIP_HEADER_ADDRESS(L_to)), "super man");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_to_get_tag(L_to),"12345-abc");
	belle_sip_object_unref(L_to);

}
void test_header_via(void) {
	belle_sip_header_via_t* L_tmp;
	belle_sip_header_t* l_next;
	belle_sip_header_via_t* L_next_via;
	belle_sip_header_via_t* L_via = belle_sip_header_via_parse("Via: SIP/2.0/UDP [::1]:5062;rport;received=192.169.0.4;branch=z9hG4bK368560724");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_via));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_via));
	L_tmp = belle_sip_header_via_parse(l_raw_header);
	L_via = BELLE_SIP_HEADER_VIA(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);

	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_protocol(L_via), "SIP/2.0");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_transport(L_via), "UDP");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_host(L_via), "::1");
	CU_ASSERT_EQUAL(belle_sip_header_via_get_port(L_via),5062);

	CU_ASSERT_TRUE(belle_sip_parameters_is_parameter(BELLE_SIP_PARAMETERS(L_via),"rport"));
	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_received(L_via),"192.169.0.4");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_branch(L_via),"z9hG4bK368560724");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_via));

	L_via = belle_sip_header_via_parse("Via: SIP/2.0/UDP 192.168.0.19:5062;rport;received=192.169.0.4;branch=z9hG4bK368560724, SIP/2.0/UDP 192.168.0.19:5062");

	l_next = belle_sip_header_get_next(BELLE_SIP_HEADER(L_via));
	L_next_via = BELLE_SIP_HEADER_VIA(l_next);
	CU_ASSERT_PTR_NOT_NULL(L_next_via);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_via_get_host(L_next_via),"192.168.0.19");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_via));

}
void test_header_call_id(void) {
	belle_sip_header_call_id_t* L_tmp;
	belle_sip_header_call_id_t* L_call_id = belle_sip_header_call_id_parse("Call-ID: 1665237789@titi.com");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_call_id));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_call_id));
	L_tmp= belle_sip_header_call_id_parse(l_raw_header);
	L_call_id = BELLE_SIP_HEADER_CALL_ID(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);

	CU_ASSERT_STRING_EQUAL(belle_sip_header_call_id_get_call_id(L_call_id), "1665237789@titi.com");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_call_id));
}
void test_header_cseq(void) {
	belle_sip_header_cseq_t* L_tmp;
	belle_sip_header_cseq_t* L_cseq = belle_sip_header_cseq_parse("CSeq: 21 INVITE");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_cseq));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_cseq));
	L_tmp = belle_sip_header_cseq_parse(l_raw_header);
	L_cseq = BELLE_SIP_HEADER_CSEQ(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);

	CU_ASSERT_EQUAL(belle_sip_header_cseq_get_seq_number(L_cseq),21);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_cseq_get_method(L_cseq),"INVITE");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_cseq));

	/*test factory*/
	L_cseq = belle_sip_header_cseq_create(1,"INFO");
	CU_ASSERT_EQUAL(belle_sip_header_cseq_get_seq_number(L_cseq),1);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_cseq_get_method(L_cseq),"INFO");
	belle_sip_object_unref(L_cseq);
}
void test_header_content_type(void) {
	belle_sip_header_content_type_t* L_tmp;
	belle_sip_header_content_type_t* L_content_type = belle_sip_header_content_type_parse("Content-Type: text/html; charset=ISO-8859-4");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_content_type));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_type));
	L_tmp = belle_sip_header_content_type_parse(l_raw_header);
	L_content_type = BELLE_SIP_HEADER_CONTENT_TYPE(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);

	CU_ASSERT_STRING_EQUAL(belle_sip_header_content_type_get_type(L_content_type),"text");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_content_type_get_subtype(L_content_type),"html");
	CU_ASSERT_STRING_EQUAL(belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(L_content_type),"charset"),"ISO-8859-4");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_type));

	L_content_type = belle_sip_header_content_type_parse("Content-Type: application/sdp");
	l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_content_type));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_type));
	L_content_type = belle_sip_header_content_type_parse(l_raw_header);
	belle_sip_free(l_raw_header);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_content_type_get_type(L_content_type),"application");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_content_type_get_subtype(L_content_type),"sdp");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_type));

	L_content_type = belle_sip_header_content_type_parse("Content-Type: application/pkcs7-mime; smime-type=enveloped-data; \r\n name=smime.p7m");
	l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_content_type));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_type));
	L_content_type = belle_sip_header_content_type_parse(l_raw_header);
	belle_sip_free(l_raw_header);
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_type));

}
void test_header_record_route(void) {
	belle_sip_uri_t* L_uri;
	belle_sip_header_t* l_next;
	belle_sip_header_record_route_t* L_next_route;
	belle_sip_header_record_route_t* L_record_route = belle_sip_header_record_route_parse("Record-Route: <sip:212.27.52.5:5060;transport=udp;lr>;charset=ISO-8859-4");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_record_route));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_record_route));
	L_record_route = belle_sip_header_record_route_parse(l_raw_header);
	belle_sip_free(l_raw_header);
	L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_record_route));
	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "212.27.52.5");
	CU_ASSERT_STRING_EQUAL(belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(L_record_route),"charset"),"ISO-8859-4");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_record_route));

	L_record_route = belle_sip_header_record_route_parse("Record-Route: <sip:212.27.52.5:5060;transport=udp;lr>;charset=ISO-8859-4, <sip:212.27.52.5:5060;transport=udp;lr>");
	l_next = belle_sip_header_get_next(BELLE_SIP_HEADER(L_record_route));
	L_next_route = BELLE_SIP_HEADER_RECORD_ROUTE(l_next);
	CU_ASSERT_PTR_NOT_NULL(L_next_route);
	CU_ASSERT_PTR_NOT_NULL( belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_next_route)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_record_route));
}
void test_header_route(void) {
	belle_sip_header_route_t* L_route;
	belle_sip_uri_t* L_uri;
	belle_sip_header_t* l_next;
	belle_sip_header_route_t* L_next_route;
	belle_sip_header_address_t* address = belle_sip_header_address_parse("<sip:212.27.52.5:5060;transport=udp;lr>");
	char* l_raw_header;
	CU_ASSERT_PTR_NOT_NULL_FATAL(address);
	L_route = belle_sip_header_route_create(address);
	CU_ASSERT_PTR_NOT_NULL_FATAL(L_route);
	l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_route));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_route));
	L_route = belle_sip_header_route_parse(l_raw_header);
	belle_sip_free(l_raw_header);
	L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_route));
	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_EQUAL(belle_sip_uri_get_port(L_uri), 5060);

	belle_sip_object_unref(BELLE_SIP_OBJECT(L_route));

	L_route = belle_sip_header_route_parse("Route: <sip:212.27.52.5:5060;transport=udp;lr>;charset=ISO-8859-4, <sip:titi.com>");
	l_next = belle_sip_header_get_next(BELLE_SIP_HEADER(L_route));
	L_next_route = BELLE_SIP_HEADER_ROUTE(l_next);
	CU_ASSERT_PTR_NOT_NULL(L_next_route);
	CU_ASSERT_PTR_NOT_NULL( belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_next_route)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_route));
}
void test_header_service_route(void) {
	belle_sip_header_service_route_t* L_service_route = belle_sip_header_service_route_parse("Service-Route: <sip:orig@scscf.ims.linphone.com:6060;lr>");
	belle_sip_uri_t* L_uri;
	char* l_raw_header;
	CU_ASSERT_PTR_NOT_NULL_FATAL(L_service_route);
	l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_service_route));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_service_route));
	L_service_route = belle_sip_header_service_route_parse(l_raw_header);
	belle_sip_free(l_raw_header);
	L_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(L_service_route));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_EQUAL(belle_sip_uri_get_port(L_uri), 6060);
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_service_route));
}
void test_header_content_length(void) {
	belle_sip_header_content_length_t* L_tmp;
	belle_sip_header_content_length_t* L_content_length = belle_sip_header_content_length_parse("Content-Length: 3495");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_content_length));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_length));
	L_tmp = belle_sip_header_content_length_parse(l_raw_header);
	L_content_length = BELLE_SIP_HEADER_CONTENT_LENGTH(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);
	CU_ASSERT_EQUAL(belle_sip_header_content_length_get_content_length(L_content_length), 3495);
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_content_length));
}
void test_header_extention(void) {
	belle_sip_header_extension_t* L_tmp;
	belle_sip_header_extension_t* L_extension = belle_sip_header_extension_parse("toto: titi");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_extension));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_extension));
	L_tmp = belle_sip_header_extension_parse(l_raw_header);
	L_extension = BELLE_SIP_HEADER_EXTENSION(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);

	CU_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(L_extension), "titi");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_extension));
}
void test_header_authorization(void) {
	const char* l_header = "Authorization: Digest username=\"0033482532176\", "\
			"realm=\"sip.ovh.net\", nonce=\"1bcdcb194b30df5f43973d4c69bdf54f\", uri=\"sip:sip.ovh.net\", response=\"eb36c8d5c8642c1c5f44ec3404613c81\","\
			"algorithm=MD5, opaque=\"1bc7f9097684320\","
			"\r\n qop=auth, nc=00000001,cnonce=\"0a4f113b\", blabla=\"toto\"";
	belle_sip_header_authorization_t* L_tmp;
	belle_sip_header_authorization_t* L_authorization = belle_sip_header_authorization_parse(l_header);
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_authorization));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_authorization));
	L_tmp = belle_sip_header_authorization_parse(l_raw_header);
	L_authorization = BELLE_SIP_HEADER_AUTHORIZATION(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);

	CU_ASSERT_PTR_NOT_NULL_FATAL(L_authorization);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_scheme(L_authorization), "Digest");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_username(L_authorization), "0033482532176");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_realm(L_authorization), "sip.ovh.net");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_nonce(L_authorization), "1bcdcb194b30df5f43973d4c69bdf54f");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_header_authorization_get_uri(L_authorization));
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_response(L_authorization), "eb36c8d5c8642c1c5f44ec3404613c81");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_algorithm(L_authorization), "MD5");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_opaque(L_authorization), "1bc7f9097684320");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_qop(L_authorization), "auth");

	CU_ASSERT_EQUAL(belle_sip_header_authorization_get_nonce_count(L_authorization), 1);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_cnonce(L_authorization), "0a4f113b");
	CU_ASSERT_STRING_EQUAL(belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(L_authorization), "blabla"),"\"toto\"");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_authorization));
}
void test_header_proxy_authorization(void) {
	const char* l_header = "Proxy-Authorization: Digest username=\"Alice\""
			", realm=\"atlanta.com\", nonce=\"c60f3082ee1212b402a21831ae\""
			", response=\"245f23415f11432b3434341c022\"";
	belle_sip_header_proxy_authorization_t* L_authorization = belle_sip_header_proxy_authorization_parse(l_header);
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_authorization));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_authorization));
	L_authorization = belle_sip_header_proxy_authorization_parse(l_raw_header);
	belle_sip_free(l_raw_header);
	CU_ASSERT_PTR_NOT_NULL(L_authorization);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_username(BELLE_SIP_HEADER_AUTHORIZATION(L_authorization)), "Alice");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_realm(BELLE_SIP_HEADER_AUTHORIZATION(L_authorization)), "atlanta.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_authorization_get_nonce(BELLE_SIP_HEADER_AUTHORIZATION(L_authorization)), "c60f3082ee1212b402a21831ae");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_authorization));

}
static void check_header_authenticate(belle_sip_header_www_authenticate_t* authenticate) {
	belle_sip_list_t* qop;
	CU_ASSERT_PTR_NOT_NULL(authenticate);
	CU_ASSERT_STRING_EQUAL(belle_sip_header_www_authenticate_get_realm(authenticate), "atlanta.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_www_authenticate_get_domain(authenticate), "sip:boxesbybob.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_www_authenticate_get_nonce(authenticate), "c60f3082ee1212b402a21831ae");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_www_authenticate_get_algorithm(authenticate), "MD5");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_www_authenticate_get_opaque(authenticate), "1bc7f9097684320");

	CU_ASSERT_PTR_NOT_NULL_FATAL(qop=belle_sip_header_www_authenticate_get_qop(authenticate));
	CU_ASSERT_STRING_EQUAL((const char*)qop->data, "auth");
	CU_ASSERT_PTR_NOT_NULL_FATAL(qop=qop->next);
	CU_ASSERT_STRING_EQUAL((const char*)qop->data, "auth-int");

	CU_ASSERT_STRING_EQUAL(belle_sip_header_www_authenticate_get_qop(authenticate)->data, "auth");
	CU_ASSERT_STRING_EQUAL(belle_sip_header_www_authenticate_get_scheme(authenticate), "Digest");
	CU_ASSERT_EQUAL(belle_sip_header_www_authenticate_is_stale(authenticate),1);
	belle_sip_object_unref(BELLE_SIP_OBJECT(authenticate));
}
void test_header_www_authenticate(void) {
	const char* l_header = "WWW-Authenticate: Digest "
			"algorithm=MD5, realm=\"atlanta.com\", opaque=\"1bc7f9097684320\","
			" qop=\"auth,auth-int\", nonce=\"c60f3082ee1212b402a21831ae\", stale=true, domain=\"sip:boxesbybob.com\"";
	belle_sip_header_www_authenticate_t* L_tmp;
	belle_sip_header_www_authenticate_t* l_authenticate = belle_sip_header_www_authenticate_parse(l_header);


	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(l_authenticate));
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_authenticate));
	L_tmp = belle_sip_header_www_authenticate_parse(l_raw_header);
	l_authenticate = BELLE_SIP_HEADER_WWW_AUTHENTICATE(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);
	check_header_authenticate(l_authenticate);


}
void test_header_proxy_authenticate(void) {
	const char* l_header = "Proxy-Authenticate: Digest "
			"algorithm=MD5, realm=\"atlanta.com\", opaque=\"1bc7f9097684320\","
			" qop=\"auth,auth-int\", nonce=\"c60f3082ee1212b402a21831ae\", stale=true, domain=\"sip:boxesbybob.com\"";
	belle_sip_header_proxy_authenticate_t* L_tmp;
	belle_sip_header_proxy_authenticate_t* L_proxy_authorization = belle_sip_header_proxy_authenticate_parse(l_header);
	//belle_sip_list_t* qop;

	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_proxy_authorization));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_proxy_authorization));
	L_tmp = belle_sip_header_proxy_authenticate_parse(l_raw_header);
	L_proxy_authorization = BELLE_SIP_HEADER_PROXY_AUTHENTICATE(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);
	check_header_authenticate(BELLE_SIP_HEADER_WWW_AUTHENTICATE(L_proxy_authorization));
}

void test_header_max_forwards(void) {
	const char* l_header = "Max-Forwards: 6";
	belle_sip_header_max_forwards_t* L_tmp;
	belle_sip_header_max_forwards_t* L_max_forwards = belle_sip_header_max_forwards_parse(l_header);
	char* l_raw_header;
	belle_sip_header_max_forwards_decrement_max_forwards(L_max_forwards);
	l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_max_forwards));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_max_forwards));
	L_tmp = belle_sip_header_max_forwards_parse(l_raw_header);
	L_max_forwards = BELLE_SIP_HEADER_MAX_FORWARDS(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));

	belle_sip_free(l_raw_header);
	CU_ASSERT_PTR_NOT_NULL(L_max_forwards);
	CU_ASSERT_EQUAL(belle_sip_header_max_forwards_get_max_forwards(L_max_forwards), 5);

	belle_sip_object_unref(BELLE_SIP_OBJECT(L_max_forwards));

}
void test_header_user_agent(void) {
	const char* l_header = "User-Agent: Linphone/3.4.99.1 (eXosip2/3.3.0)";
	const char* values[] ={"Linphone/3.4.99.1"
				,"(eXosip2/3.3.0)"};
	belle_sip_list_t* products;
	belle_sip_header_user_agent_t* L_tmp;
	belle_sip_header_user_agent_t* L_user_agent = belle_sip_header_user_agent_parse(l_header);
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_user_agent));
	int i=0;
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_user_agent));
	L_tmp = belle_sip_header_user_agent_parse(l_raw_header);
	L_user_agent = BELLE_SIP_HEADER_USER_AGENT(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));

	belle_sip_free(l_raw_header);

	products = belle_sip_header_user_agent_get_products(L_user_agent);

	for(i=0;i<2;i++){
		CU_ASSERT_PTR_NOT_NULL(products);
		CU_ASSERT_STRING_EQUAL((const char *)(products->data),values[i]);
		products=products->next;
	}

	belle_sip_object_unref(BELLE_SIP_OBJECT(L_user_agent));

}
void test_header_expires(void) {
	belle_sip_header_expires_t* L_tmp;
	belle_sip_header_expires_t* L_expires = belle_sip_header_expires_parse("Expires: 3600");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_expires));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_expires));
	L_tmp= belle_sip_header_expires_parse(l_raw_header);
	L_expires = BELLE_SIP_HEADER_EXPIRES(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);
	CU_ASSERT_EQUAL(belle_sip_header_expires_get_expires(L_expires), 3600);
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_expires));
	/*test factory*/
	L_expires = belle_sip_header_expires_create(600);
	CU_ASSERT_EQUAL(belle_sip_header_expires_get_expires(L_expires), 600);
	belle_sip_object_unref(L_expires);
}
void test_header_allow(void) {
	belle_sip_header_allow_t* L_tmp;
	belle_sip_header_allow_t* L_allow = belle_sip_header_allow_parse("Allow:INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_allow));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_allow));
	L_tmp = belle_sip_header_allow_parse(l_raw_header);
	L_allow = BELLE_SIP_HEADER_ALLOW(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);

	CU_ASSERT_STRING_EQUAL(belle_sip_header_allow_get_method(L_allow), "INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_allow));
}

static void test_header_address_with_error(void) {
	belle_sip_header_address_t* laddress = belle_sip_header_address_parse("sip:liblinphone_tester@=auth1.example.org");
	CU_ASSERT_PTR_NULL(laddress);
}

static void test_header_address(void) {
	belle_sip_uri_t* L_uri;
	belle_sip_header_address_t* laddress = belle_sip_header_address_parse("\"toto\" <sip:liblinphone_tester@81.56.11.2:5060>");
	CU_ASSERT_PTR_NOT_NULL_FATAL(laddress);
	CU_ASSERT_STRING_EQUAL("toto",belle_sip_header_address_get_displayname(laddress))
	L_uri = belle_sip_header_address_get_uri(laddress);

	CU_ASSERT_PTR_NOT_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "81.56.11.2");
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_user(L_uri), "liblinphone_tester");
	CU_ASSERT_EQUAL(belle_sip_uri_get_port(L_uri), 5060);
	belle_sip_object_unref(BELLE_SIP_OBJECT(laddress));
}
void test_header_subscription_state(void) {
	belle_sip_header_subscription_state_t* L_tmp;
	belle_sip_header_subscription_state_t* L_subscription_state = belle_sip_header_subscription_state_parse("Subscription-State: terminated;expires=600");
	char* l_raw_header = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_subscription_state));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_subscription_state));
	L_tmp = belle_sip_header_subscription_state_parse(l_raw_header);
	L_subscription_state = BELLE_SIP_HEADER_SUBSCRIPTION_STATE(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_header);

	CU_ASSERT_STRING_EQUAL(belle_sip_header_subscription_state_get_state(L_subscription_state), "terminated");
	CU_ASSERT_EQUAL(belle_sip_header_subscription_state_get_expires(L_subscription_state), 600);
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_subscription_state));
}

int belle_sip_headers_test_suite() {
	
	   CU_pSuite pSuite = NULL;
	   /* add a suite to the registry */
	   pSuite = CU_add_suite("Headers", init_suite1, clean_suite1);

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
	   if (NULL == CU_add_test(pSuite, "test of content length", test_header_content_length)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of extension", test_header_extention)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of authorization", test_header_authorization)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of proxy authorization", test_header_proxy_authorization)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of www authenticate", test_header_www_authenticate)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of proxy authenticate", test_header_proxy_authenticate)) {
	   	  return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of max forwards", test_header_max_forwards)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of  user agent", test_header_user_agent)) {
	   	  return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of  expires", test_header_expires)) {
	   	  return CU_get_error();
	   	}
	   if (NULL == CU_add_test(pSuite, "test of  allow", test_header_allow)) {
	   	  return CU_get_error();
	   	}
	   if (NULL == CU_add_test(pSuite, "test header address with error",test_header_address_with_error )) {
	   	  return CU_get_error();
	   	}
	   if (NULL == CU_add_test(pSuite, "test-header-address",test_header_address )) {
	   	  return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test_header_subscription_state",test_header_subscription_state )) {
	   	  return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test_header_service_route",test_header_service_route )) {
	   	  return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test_header_from_with_paramless_address_spec",test_header_from_with_paramless_address_spec )) {
	   	  return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test_header_to_with_paramless_address_spec",test_header_to_with_paramless_address_spec )) {
	   	  return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test_header_contact_with_paramless_address_spec",test_header_contact_with_paramless_address_spec )) {
	   	  return CU_get_error();
	   }
	   return 0;
}
