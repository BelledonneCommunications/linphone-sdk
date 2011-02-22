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
	CU_ASSERT_PTR_NOT_NULL(belle_sip_request_get_uri(BELLE_SIP_REQUEST(message)));

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

	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Contact"));
	BELLE_SIP_HEADER_CONTACT(belle_sip_message_get_header(message,"Contact"));
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
							"Content-Length: 0\r\n\r\n";
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	belle_sip_request_t* request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Expires"));
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
							"User-Agent: X-Lite 4 release 4.0 stamp 58832\r\n"\
							"Content-Length: 230\r\n\r\n";
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	belle_sip_request_t* request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"INVITE");
	check_uri_and_headers(message);
}
static void testByeMessage(void) {
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


	   return CU_get_error();
}
