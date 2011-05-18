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

static int init_suite_message(void) {
      return 0;
}

static int clean_suite_message(void) {
      return 0;
}


static void check_uri_and_headers(belle_sip_message_t* message) {
	if (belle_sip_message_is_request(message)) {
		CU_ASSERT_PTR_NOT_NULL(belle_sip_request_get_uri(BELLE_SIP_REQUEST(message)));
	}
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"From"));
	BELLE_SIP_HEADER_FROM(belle_sip_message_get_header(message,"From"));

	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"To"));
	BELLE_SIP_HEADER_TO(belle_sip_message_get_header(message,"To"));

	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"CSeq"));
	BELLE_SIP_HEADER_CSEQ(belle_sip_message_get_header(message,"CSeq"));

	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Via"));
	BELLE_SIP_HEADER_VIA(belle_sip_message_get_header(message,"Via"));

	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Call-ID"));
	BELLE_SIP_HEADER_CALL_ID(belle_sip_message_get_header(message,"Call-ID"));


	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Content-Length"));
	BELLE_SIP_HEADER_CONTENT_LENGTH(belle_sip_message_get_header(message,"Content-Length"));


}

static void testRegisterMessage(void) {
	const char* raw_message = "REGISTER sip:192.168.0.20 SIP/2.0\r\n"\
							"Via: SIP/2.0/UDP 192.168.1.8:5062;rport;branch=z9hG4bK1439638806\r\n"\
							"From: <sip:jehan-mac@sip.linphone.org>;tag=465687829\r\n"\
							"To: <sip:jehan-mac@sip.linphone.org>\r\n"\
							"Call-ID: 1053183492\r\n"\
							"CSeq: 1 REGISTER\r\n"\
							"Contact: <sip:jehan-mac@192.168.1.8:5062>\r\n"\
							"Max-Forwards: 70\r\n"\
							"User-Agent: Linphone/3.3.99.10 (eXosip2/3.3.0)\r\n"\
							"Expires: 3600\r\n"\
							"Proxy-Authorization: Digest username=\"8117396\", realm=\"Realm\", nonce=\"MTMwNDAwMjIxMjA4NzVkODY4ZmZhODMzMzU4ZDJkOTA1NzM2NTQ2NDZlNmIz"\
							", uri=\"sip:linphone.net\", response=\"eed376ff7c963441255ec66594e470e7\", algorithm=MD5, cnonce=\"0a4f113b\", qop=auth, nc=00000001\r\n"\
							"Content-Length: 0\r\n\r\n";
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	char* encoded_message = belle_sip_object_to_string(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	message = belle_sip_message_parse(encoded_message);

	belle_sip_request_t* request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Expires"));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Proxy-Authorization"));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Contact"));

	check_uri_and_headers(message);

}
static void testInviteMessage(void) {
	const char* raw_message = "INVITE sip:becheong@sip.linphone.org SIP/2.0\r\n"\
							"Via: SIP/2.0/UDP 10.23.17.117:22600;branch=z9hG4bK-d8754z-4d7620d2feccbfac-1---d8754z-;rport=4820;received=202.165.193.129\r\n"\
							"Max-Forwards: 70\r\n"\
							"Contact: <sip:bcheong@202.165.193.129:4820>\r\n"\
							"To: \"becheong\" <sip:becheong@sip.linphone.org>\r\n"\
							"From: \"Benjamin Cheong\" <sip:bcheong@sip.linphone.org>;tag=7326e5f6\r\n"\
							"Call-ID: Y2NlNzg0ODc0ZGIxODU1MWI5MzhkNDVkNDZhOTQ4YWU.\r\n"\
							"CSeq: 1 INVITE\r\n"\
							"Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO\r\n"\
							"Content-Type: application/sdp\r\n"\
							"Supported: replaces\r\n"\
							"Authorization: Digest username=\"003332176\", realm=\"sip.ovh.net\", nonce=\"24212965507cde726e8bc37e04686459\", uri=\"sip:sip.ovh.net\", response=\"896e786e9c0525ca3085322c7f1bce7b\", algorithm=MD5, opaque=\"241b9fb347752f2\"\r\n"\
							"User-Agent: X-Lite 4 release 4.0 stamp 58832\r\n"\
							"Content-Length: 230\r\n\r\n";
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	char* encoded_message = belle_sip_object_to_string(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	message = belle_sip_message_parse(encoded_message);
	belle_sip_request_t* request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"INVITE");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Contact"));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Authorization"));
	check_uri_and_headers(message);
}
static void test401Response(void) {
	const char* raw_message = 	"SIP/2.0 401 Unauthorized\r\n"
								"Call-ID: 577586163\r\n"
								"CSeq: 21 REGISTER\r\n"
								"From: <sip:0033532176@sip.ovh.net>;tag=1790643209\r\n"
								"Server: Cirpack/v4.42x (gw_sip)\r\n"
								"To: <sip:0033482176@sip.ovh.net>;tag=00-08075-24212984-22e348d97\r\n"
								"Via: SIP/2.0/UDP 192.168.0.18:5062;received=81.56.113.2;rport=5062;branch=z9hG4bK1939354046\r\n"
								"WWW-Authenticate: Digest realm=\"sip.ovh.net\",nonce=\"24212965507cde726e8bc37e04686459\",opaque=\"241b9fb347752f2\",stale=false,algorithm=MD5\r\n"
								"Content-Length: 0\r\n\r\n";
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	char* encoded_message = belle_sip_object_to_string(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	message = belle_sip_message_parse(encoded_message);
	belle_sip_response_t* response = BELLE_SIP_RESPONSE(message);
	CU_ASSERT_EQUAL(belle_sip_response_get_status_code(response),401);
	CU_ASSERT_STRING_EQUAL(belle_sip_response_get_reason_phrase(response),"Unauthorized");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"WWW-Authenticate"));
	check_uri_and_headers(message);
}
static void testRegisterRaw(void) {
	const char* raw_message = "REGISTER sip:192.168.0.20 SIP/2.0\r\n"\
							"Via: SIP/2.0/UDP 192.168.1.8:5062;rport;branch=z9hG4bK1439638806\r\n"\
							"From: <sip:jehan-mac@sip.linphone.org>;tag=465687829\r\n"\
							"To: <sip:jehan-mac@sip.linphone.org>\r\n"\
							"Call-ID: 1053183492\r\n"\
							"CSeq: 1 REGISTER\r\n"\
							"Contact: <sip:jehan-mac@192.168.1.8:5062>\r\n"\
							"Max-Forwards: 70\r\n"\
							"User-Agent: Linphone/3.3.99.10 (eXosip2/3.3.0)\r\n"\
							"Expires: 3600\r\n"\
							"Content-Length: 0\r\n\r\n123456789";
	size_t size=0;
	size_t raw_message_size= strlen(raw_message);
	belle_sip_message_t* message = belle_sip_message_parse_raw(raw_message,raw_message_size,&size);
	CU_ASSERT_EQUAL(raw_message_size,size+9);
	belle_sip_request_t* request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_request_get_uri(request));
	CU_ASSERT_STRING_EQUAL(belle_sip_message_get_body(request),"123456789");

}
static void testOptionMessage(void) {
	const char* raw_message = "REGISTER sip:192.168.0.20 SIP/2.0\r\n"\
							"Via: SIP/2.0/UDP 192.168.1.8:5062;rport;branch=z9hG4bK1439638806\r\n"\
							"From: <sip:jehan-mac@sip.linphone.org>;tag=465687829\r\n"\
							"To: <sip:jehan-mac@sip.linphone.org>\r\n"\
							"Call-ID: 1053183492\r\n"\
							"CSeq: 1 REGISTER\r\n"\
							"Contact: <sip:jehan-mac@192.168.1.8:5062>\r\n"\
							"Max-Forwards: 70\r\n"\
							"User-Agent: Linphone/3.3.99.10 (eXosip2/3.3.0)\r\n"\
							"Expires: 3600\r\n"\
							"Content-Length: 0\r\n\r\n";
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	belle_sip_request_t* request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_request_get_uri(request));
}


int belle_sip_message_test_suite () {

	   CU_pSuite pSuite = NULL;


	   /* add a suite to the registry */
	   pSuite = CU_add_suite("message suite", init_suite_message, clean_suite_message);
	   if (NULL == pSuite) {
	      return CU_get_error();
	   }

	   /* add the tests to the suite */
	   /* NOTE - ORDER IS IMPORTANT - MUST TEST fread() AFTER fprintf() */
	   if (NULL == CU_add_test(pSuite, "test of register message", testRegisterMessage)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of invite message", testInviteMessage)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of register raw message", testRegisterRaw)) {
	      return CU_get_error();
	   }
	   if (NULL == CU_add_test(pSuite, "test of 401 response", test401Response)) {
	      return CU_get_error();
	   }



	   return CU_get_error();
}
