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
#include "belle_sip_tester.h"
#include <stdio.h>
#include "CUnit/Basic.h"


static void check_uri_and_headers(belle_sip_message_t* message) {
	if (belle_sip_message_is_request(message)) {
		CU_ASSERT_PTR_NOT_NULL(belle_sip_request_get_uri(BELLE_SIP_REQUEST(message)));

		CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Max-Forwards"));
		CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_MAX_FORWARDS(belle_sip_message_get_header(message,"Max-Forwards")));

		CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"User-Agent"));
		CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_USER_AGENT(belle_sip_message_get_header(message,"User-Agent")));
	}
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"From"));
	CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_FROM(belle_sip_message_get_header(message,"From")));

	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"To"));
	CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_TO(belle_sip_message_get_header(message,"To")));

	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"CSeq"));
	CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_CSEQ(belle_sip_message_get_header(message,"CSeq")));

	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Via"));
	CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_VIA(belle_sip_message_get_header(message,"Via")));

	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Call-ID"));
	CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_CALL_ID(belle_sip_message_get_header(message,"Call-ID")));


	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Content-Length"));
	CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_CONTENT_LENGTH(belle_sip_message_get_header(message,"Content-Length")));





}

static void testRegisterMessage(void) {
	const char* raw_message = "REGISTER sip:192.168.0.20 SIP/2.0\r\n"\
							"v: SIP/2.0/UDP 192.168.1.8:5062;rport;branch=z9hG4bK1439638806\r\n"\
							"f: <sip:jehan-mac@sip.linphone.org>;tag=465687829\r\n"\
							"t: <sip:jehan-mac@sip.linphone.org>\r\n"\
							"i: 1053183492\r\n"\
							"CSeq: 1 REGISTER\r\n"\
							"m: <sip:jehan-mac@192.168.1.8:5062>\r\n"\
							"Max-Forwards: 70\r\n"\
							"User-Agent: Linphone/3.3.99.10 (eXosip2/3.3.0)\r\n"\
							"Expires: 3600\r\n"\
							"Proxy-Authorization: Digest username=\"8117396\", realm=\"Realm\", nonce=\"MTMwNDAwMjIxMjA4NzVkODY4ZmZhODMzMzU4ZDJkOTA1NzM2NTQ2NDZlNmIz"\
							", uri=\"sip:linphone.net\", response=\"eed376ff7c963441255ec66594e470e7\", algorithm=MD5, cnonce=\"0a4f113b\", qop=auth, nc=00000001\r\n"\
							"l: 0\r\n\r\n";
	belle_sip_request_t* request;
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	char* encoded_message = belle_sip_object_to_string(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	message = belle_sip_message_parse(encoded_message);

	request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Expires"));
	CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_EXPIRES(belle_sip_message_get_header(message,"Expires")));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Proxy-Authorization"));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Contact"));

	check_uri_and_headers(message);
	belle_sip_free(encoded_message);
	belle_sip_object_unref(message);

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
							"c: application/sdp\r\n"\
							"Supported: replaces\r\n"\
							"Authorization: Digest username=\"003332176\", realm=\"sip.ovh.net\", nonce=\"24212965507cde726e8bc37e04686459\", uri=\"sip:sip.ovh.net\", response=\"896e786e9c0525ca3085322c7f1bce7b\", algorithm=MD5, opaque=\"241b9fb347752f2\"\r\n"\
							"User-Agent: X-Lite 4 release 4.0 stamp 58832\r\n"\
							"Content-Length: 230\r\n\r\n";
	belle_sip_request_t* request;
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	char* encoded_message = belle_sip_object_to_string(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	message = belle_sip_message_parse(encoded_message);
	request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"INVITE");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Contact"));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Authorization"));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Content-Type"));
	check_uri_and_headers(message);
	belle_sip_object_unref(message);
	belle_sip_free(encoded_message);
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
	belle_sip_response_t* response;
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	char* encoded_message = belle_sip_object_to_string(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	message = belle_sip_message_parse(encoded_message);
	response = BELLE_SIP_RESPONSE(message);
	CU_ASSERT_EQUAL(belle_sip_response_get_status_code(response),401);
	CU_ASSERT_STRING_EQUAL(belle_sip_response_get_reason_phrase(response),"Unauthorized");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"WWW-Authenticate"));
	check_uri_and_headers(message);
	belle_sip_object_unref(message);
	belle_sip_free(encoded_message);
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
	belle_sip_request_t* request;
	size_t size=0;
	size_t raw_message_size= strlen(raw_message);
	belle_sip_message_t* message = belle_sip_message_parse_raw(raw_message,raw_message_size,&size);
	CU_ASSERT_EQUAL(raw_message_size,size+9);
	request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_request_get_uri(request));
	CU_ASSERT_STRING_EQUAL(&raw_message[size],"123456789");
	belle_sip_object_unref(message);
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
	belle_sip_object_unref(message);
}

static void test_extract_source(void) {
	const char * invite_1="INVITE sip:jehan@81.56.113.2:50343;transport=tcp;line=f18e0009dd6cc43 SIP/2.0\r\n"
						"Via: SIP/2.0/TCP 37.59.129.73;branch=z9hG4bK.SKvK9U327e8mU68XUv5rt144pg\r\n"
						"Via: SIP/2.0/UDP 192.168.1.12:15060;rport=15060;branch=z9hG4bK1596944937;received=81.56.113.2\r\n"
						"Record-Route: <sip:37.59.129.73;lr;transport=tcp>\r\n"
						"Record-Route: <sip:37.59.129.73;lr>\r\n"
						"Max-Forwards: 70\r\n"
						"From: <sip:jehan@sip.linphone.org>;tag=711138653\r\n"
						"To: <sip:jehan@sip.linphone.org>\r\n"
						"Call-ID: 977107319\r\n"
						"CSeq: 21 INVITE\r\n"
						"Contact: <sip:jehan@81.56.113.2:15060>\r\n"
						"Subject: Phone call\r\n"
						"User-Agent: Linphone/3.5.2 (eXosip2/3.6.0)\r\n"
						"Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO\r\n"
						"Content-Type: application/sdp\r\n"\
						"Content-Length: 230\r\n\r\n";

	const char * invite_2="INVITE sip:jehan@81.56.113.2:50343;transport=tcp;line=f18e0009dd6cc43 SIP/2.0\r\n"
						"Via: SIP/2.0/UDP 192.168.1.12:15060;rport=15060;branch=z9hG4bK1596944937;received=81.56.113.2\r\n"
						"Via: SIP/2.0/TCP 37.59.129.73;branch=z9hG4bK.SKvK9U327e8mU68XUv5rt144pg\r\n"
						"Record-Route: <sip:37.59.129.73;lr;transport=tcp>\r\n"
						"Record-Route: <sip:37.59.129.73;lr>\r\n"
						"Max-Forwards: 70\r\n"
						"From: <sip:jehan@sip.linphone.org>;tag=711138653\r\n"
						"To: <sip:jehan@sip.linphone.org>\r\n"
						"Call-ID: 977107319\r\n"
						"CSeq: 21 INVITE\r\n"
						"Contact: <sip:jehan@81.56.113.2:15060>\r\n"
						"Subject: Phone call\r\n"
						"User-Agent: Linphone/3.5.2 (eXosip2/3.6.0)\r\n"
						"Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO\r\n"
						"Content-Length: 0\r\n\r\n";

	belle_sip_message_t* message = belle_sip_message_parse(invite_1);
	belle_sip_request_t* request = BELLE_SIP_REQUEST(message);
	belle_sip_uri_t* source =belle_sip_request_extract_origin(request);
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Content-type"));
	CU_ASSERT_PTR_NOT_NULL(source);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(source),"37.59.129.73");
	CU_ASSERT_EQUAL(belle_sip_uri_get_port(source),0);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_transport_param(source),"tcp");
	belle_sip_object_unref(message);

	message = belle_sip_message_parse(invite_2);
	request = BELLE_SIP_REQUEST(message);
	source =belle_sip_request_extract_origin(request);
	CU_ASSERT_PTR_NOT_NULL(source);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(source),"81.56.113.2");
	CU_ASSERT_EQUAL(belle_sip_uri_get_port(source),15060);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_transport_param(source),"udp");
	belle_sip_object_unref(message);

}

static void test_sipfrag(void) {
	const char* raw_message = 	"SIP/2.0 100 Trying\r\n";
	belle_sip_response_t* response;
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	response = BELLE_SIP_RESPONSE(message);
	CU_ASSERT_EQUAL(belle_sip_response_get_status_code(response),100);
	CU_ASSERT_STRING_EQUAL(belle_sip_response_get_reason_phrase(response),"Trying");
	belle_sip_object_unref(message);
}

/*static void test_fix_contact_with_received_rport() {

}*/

static void testMalformedMessage(void) {
const char * raw_message=	"INVITE sip:jehan@81.56.113.2:50343;transport=tcp;line=f18e0009dd6cc43 SIP/2.0\r\n"
							"Via: SIP/2.0/UDP 192.168.1.12:15060;rport=15060;branch=z9hG4bK1596944937;received=81.56.113.2\r\n"
							"Via: SIP/2.0/TCP 37.59.129.73;branch=z9hG4bK.SKvK9U327e8mU68XUv5rt144pg\r\n"
							"Record-Route: <sip:37.59.129.73;lr;transport=tcp>\r\n"
							"Record-Route: <sip:37.59.129.73;lr>\r\n"
							"Max-Forwards: 70\r\n"
							"From: <sip:jehan@sip.linphone.org>;tag=711138653\r\n"
							"To: <sip:jehan@sip.linphone.org>\r\n"
							"Call-ID: 977107319\r\n"
							"CSeq: 21 INVITE\r\n"
							"Contact: <sip:jehan-mac@192.168.1.8:5062>;pn-tok=/throttledthirdparty\r\n" /*Slash is not allowed for contact params*/\
							"Subject: Phone call\r\n"
							"User-Agent: Linphone/3.5.2 (eXosip2/3.6.0)\r\n"
							"Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO\r\n"
							"Content-Length: 0\r\n\r\n";

	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	CU_ASSERT_FALSE(belle_sip_message_check_headers(message));
}

static void testMalformedOptionnalHeaderInMessage(void) {
const char* raw_message = 	"REGISTER sip:192.168.0.20 SIP/2.0\r\n"\
							"Via: SIP/2.0/UDP 192.168.1.8:5062;rport;branch=z9hG4bK1439638806\r\n"\
							"From: <sip:jehan-mac@sip.linphone.org>;tag=465687829\r\n"\
							"To: <sip:jehan-mac@sip.linphone.org>\r\n"\
							"Call-ID: 1053183492\r\n"\
							"CSeq: 1 REGISTER\r\n"\
							"Contact: <sip:jehan-mac@192.168.1.8:5062>;pn-tok=/throttledthirdparty\r\n" /*Slash is not allowed for contact params*/\
							"Max-Forwards: 70\r\n"\
							"User-Agent: Linphone/3.3.99.10 (eXosip2/3.3.0)\r\n"\
							"Expires: 3600\r\n"\
							"Proxy-Authorization: Digest username=\"8117396\", realm=\"Realm\", nonce=\"MTMwNDAwMjIxMjA4NzVkODY4ZmZhODMzMzU4ZDJkOTA1NzM2NTQ2NDZlNmIz"\
							", uri=\"sip:linphone.net\", response=\"eed376ff7c963441255ec66594e470e7\", algorithm=MD5, cnonce=\"0a4f113b\", qop=auth, nc=00000001\r\n"\
							"Content-Length: 0\r\n\r\n";

	belle_sip_request_t* request;
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	char* encoded_message = belle_sip_object_to_string(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	message = belle_sip_message_parse(encoded_message);

	request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Expires"));
	CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_EXPIRES(belle_sip_message_get_header(message,"Expires")));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Proxy-Authorization"));
	CU_ASSERT_PTR_NULL(belle_sip_message_get_header(message,"Contact")); /*contact is optionnal in register*/

	check_uri_and_headers(message);
	belle_sip_free(encoded_message);
	belle_sip_object_unref(message);
}

static void testMalformedMessageWithWrongStart(void) {
	const char * raw_message=	"\r\n"
			"INVITE sip:jehan@81.56.113.2:50343;transport=tcp;line=f18e0009dd6cc43 SIP/2.0\r\n"
			"Via: SIP/2.0/UDP 192.168.1.12:15060;rport=15060;branch=z9hG4bK1596944937;received=81.56.113.2\r\n"
			"Via: SIP/2.0/TCP 37.59.129.73;branch=z9hG4bK.SKvK9U327e8mU68XUv5rt144pg\r\n"
			"Record-Route: <sip:37.59.129.73;lr;transport=tcp>\r\n"
			"Record-Route: <sip:37.59.129.73;lr>\r\n"
			"Max-Forwards: 70\r\n"
			"From: <sip:jehan@sip.linphone.org>;tag=711138653\r\n"
			"To: <sip:jehan@sip.linphone.org>\r\n"
			"Call-ID: 977107319\r\n"
			"CSeq: 21 INVITE\r\n"
			"Contact: <sip:jehan-mac@192.168.1.8:5062>\r\n"
			"Subject: Phone call\r\n"
			"User-Agent: Linphone/3.5.2 (eXosip2/3.6.0)\r\n"
			"Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO\r\n"
			"Content-Length: 0\r\n\r\n";

	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	CU_ASSERT_PTR_NULL(message);
}
#include "belle_sip_internal.h"

void channel_parser_tester_recovery_from_error () {
	belle_sip_stack_t* stack = belle_sip_stack_new(NULL);
	belle_sip_channel_t* channel = belle_sip_stream_channel_new_client(stack
																	, NULL
																	, 45421
																	, NULL
																	, "127.0.0.1"
																	, 45421);

	const char * raw_message=	"debut de stream tout pourri\r\n"
			"INVITE je_suis_une_fausse_request_uri_hihihi SIP/2.0\r\n"
			"Via: SIP/2.0/UDP 192.168.1.12:15060;rport=15060;branch=z9hG4bK1596944937;received=81.56.113.2\r\n"
			"Via: SIP/2.0/TCP 37.59.129.73;branch=z9hG4bK.SKvK9U327e8mU68XUv5rt144pg\r\n"
			"Record-Route: <sip:37.59.129.73;lr;transport=tcp>\r\n"
			"Record-Route: <sip:37.59.129.73;lr>\r\n"
			"Max-Forwards: 70\r\n"
			"From: <sip:jehan@sip.linphone.org>;tag=711138653\r\n"
			"To: <sip:jehan@sip.linphone.org>\r\n"
			"Call-ID: 977107319\r\n"
			"CSeq: 21 INVITE\r\n"
			"Contact: <sip:jehan-mac@192.168.1.8:5062>\r\n"
			"Subject: Phone call\r\n"
			"User-Agent: Linphone/3.5.2 (eXosip2/3.6.0)\r\n"
			"Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO\r\n"
			"Content-Length: 0\r\n"
			"\r\n"
			"REGISTER sip:192.168.0.20 SIP/2.0\r\n"\
			"Via: SIP/2.0/UDP 192.168.1.8:5062;rport;branch=z9hG4bK1439638806\r\n"\
			"From: <sip:jehan-mac@sip.linphone.org>;tag=465687829\r\n"\
			"To: <sip:jehan-mac@sip.linphone.org>\r\n"\
			"Call-ID: 1053183492\r\n"\
			"CSeq: 1 REGISTER\r\n"\
			"Contact: <sip:jehan-mac@192.168.1.8:5062>\r\n" \
			"Max-Forwards: 70\r\n"\
			"User-Agent: Linphone/3.3.99.10 (eXosip2/3.3.0)\r\n"\
			"Expires: 3600\r\n"\
			"Proxy-Authorization: Digest username=\"8117396\", realm=\"Realm\", nonce=\"MTMwNDAwMjIxMjA4NzVkODY4ZmZhODMzMzU4ZDJkOTA1NzM2NTQ2NDZlNmIz"\
			", uri=\"sip:linphone.net\", response=\"eed376ff7c963441255ec66594e470e7\", algorithm=MD5, cnonce=\"0a4f113b\", qop=auth, nc=00000001\r\n"\
			"Content-Length: 0\r\n"
			"\r\n";
	belle_sip_request_t* request;
	belle_sip_message_t* message;
	channel->input_stream.write_ptr = strcpy(channel->input_stream.write_ptr,raw_message);
	channel->input_stream.write_ptr+=strlen(raw_message);

	belle_sip_channel_parse_stream(channel);

	CU_ASSERT_PTR_NOT_NULL(channel->incoming_messages);
	CU_ASSERT_PTR_NOT_NULL(channel->incoming_messages->data);
	message=BELLE_SIP_MESSAGE(channel->incoming_messages->data);
	CU_ASSERT_TRUE(BELLE_SIP_OBJECT_IS_INSTANCE_OF(message,belle_sip_request_t));
	request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Expires"));
	CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_EXPIRES(belle_sip_message_get_header(message,"Expires")));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Proxy-Authorization"));

	check_uri_and_headers(message);

	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(stack);

}

void channel_parser_malformed_start () {
	belle_sip_stack_t* stack = belle_sip_stack_new(NULL);
	belle_sip_channel_t* channel = belle_sip_stream_channel_new_client(stack
																	, NULL
																	, 45421
																	, NULL
																	, "127.0.0.1"
																	, 45421);

	const char * raw_message=	"debut de stream tout pourri\r\n"
			"REGISTER sip:192.168.0.20 SIP/2.0\r\n"
			"Via: SIP/2.0/UDP 192.168.1.8:5062;rport;branch=z9hG4bK1439638806\r\n"
			"From: <sip:jehan-mac@sip.linphone.org>;tag=465687829\r\n"
			"To: <sip:jehan-mac@sip.linphone.org>\r\n"
			"Call-ID: 1053183492\r\n"
			"CSeq: 1 REGISTER\r\n"
			"Contact: <sip:jehan-mac@192.168.1.8:5062>\r\n"
			"Max-Forwards: 70\r\n"
			"User-Agent: Linphone/3.3.99.10 (eXosip2/3.3.0)\r\n"
			"Expires: 3600\r\n"
			"Proxy-Authorization: Digest username=\"8117396\", realm=\"Realm\", nonce=\"MTMwNDAwMjIxMjA4NzVkODY4ZmZhODMzMzU4ZDJkOTA1NzM2NTQ2NDZlNmIz"
			", uri=\"sip:linphone.net\", response=\"eed376ff7c963441255ec66594e470e7\", algorithm=MD5, cnonce=\"0a4f113b\", qop=auth, nc=00000001\r\n"
			"Content-Length: 0\r\n"
			"\r\n";
	belle_sip_request_t* request;
	belle_sip_message_t* message;
	channel->input_stream.write_ptr = strcpy(channel->input_stream.write_ptr,raw_message);
	channel->input_stream.write_ptr+=strlen(raw_message);

	belle_sip_channel_parse_stream(channel);

	CU_ASSERT_PTR_NOT_NULL(channel->incoming_messages);
	CU_ASSERT_PTR_NOT_NULL(channel->incoming_messages->data);
	message=BELLE_SIP_MESSAGE(channel->incoming_messages->data);
	CU_ASSERT_TRUE(BELLE_SIP_OBJECT_IS_INSTANCE_OF(message,belle_sip_request_t));
	request = BELLE_SIP_REQUEST(message);
	CU_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Expires"));
	CU_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_EXPIRES(belle_sip_message_get_header(message,"Expires")));
	CU_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Proxy-Authorization"));


	check_uri_and_headers(message);

	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(stack);
}


static void testRFC2543Compat(void) {
	belle_sip_server_transaction_t *tr;
	const char* raw_message = 	"INVITE sip:me@127.0.0.1 SIP/2.0\r\n"
			"Via: SIP/2.0/UDP 192.168.1.12:15060;rport=15060;received=81.56.113.2\r\n"
			"Record-Route: <sip:37.59.129.73;lr;transport=tcp>\r\n"
			"Record-Route: <sip:37.59.129.73;lr>\r\n"
			"Max-Forwards: 70\r\n"
			"From: <sip:jehan@sip.linphone.org>;tag=711138653\r\n"
			"To: <sip:jehan@sip.linphone.org>\r\n"
			"Call-ID: 977107319\r\n"
			"CSeq: 21 INVITE\r\n"
			"Contact: <sip:jehan-mac@192.168.1.8:5062>\r\n"
			"Subject: Phone call\r\n"
			"User-Agent: Linphone/3.5.2 (eXosip2/3.6.0)\r\n"
			"Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO\r\n"
			"Content-Length: 0\r\n"
			"Extended: \r\n" /*fixme lexer*/
			"\r\n";

	belle_sip_request_t* request;
	belle_sip_stack_t *stack=belle_sip_stack_new(NULL);
	belle_sip_provider_t *prov=belle_sip_provider_new(stack,NULL);
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	
	belle_sip_object_ref(message);
	belle_sip_object_ref(message); /*yes double ref: originally the message is created with 0 refcount, and dispatch_message will unref() it.*/
	belle_sip_provider_dispatch_message(prov,message);
	request = BELLE_SIP_REQUEST(message);
	
	CU_ASSERT_PTR_NOT_NULL(request);
	tr=belle_sip_provider_create_server_transaction(prov,request);
	CU_ASSERT_PTR_NOT_NULL(tr);
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
	belle_sip_object_unref(message);
}

static void testUriHeadersInInvite(void)  {
	belle_sip_request_t* request;
	belle_sip_stack_t *stack=belle_sip_stack_new(NULL);
	belle_sip_provider_t *prov=belle_sip_provider_new(stack,NULL);
	const char* raw_uri="sip:toto@titi.com"
						"?header1=blabla"
						"&header2=blue%3Bweftutu%3Dbla"
						"&From=toto"
						"&To=sip%3Atoto%40titi.com"
						"&Call-ID=asdads"
						"&CSeq=asdasd"
						"&Via=asdasd"
						"&Accept=adsad"
						"&Accept-Encoding=adsad"
						"&Accept-Language=adsad"
						"&Allow=adsad"
						"&Record-Route=adsad"
						"&Contact=adsad"
						"&Organization=adsad"
						"&Supported=adsad"
						"&User-Agent=adsad";

	belle_sip_header_t* raw_header;
	request=belle_sip_request_create(	belle_sip_uri_parse(raw_uri)
										,"INVITE"
										,belle_sip_provider_create_call_id(prov)
										,belle_sip_header_cseq_create(20,"INVITE")
										,belle_sip_header_from_create2("sip:toto@titi.com","4654")
										,NULL
										,belle_sip_header_via_new()
										,70);
	CU_ASSERT_PTR_NOT_NULL(request);
	CU_ASSERT_PTR_NOT_NULL(raw_header=belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"header1"));
	if (raw_header) {
		CU_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(raw_header)),"blabla");
	}
	CU_ASSERT_PTR_NOT_NULL(raw_header=belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"header2"));
	if (raw_header) {
		CU_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(raw_header)),"blue;weftutu=bla");
	}
	CU_ASSERT_PTR_NOT_NULL(raw_header=belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"To"));
	if (raw_header) {
		CU_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(raw_header)),"sip:toto@titi.com");
	}
	CU_ASSERT_STRING_NOT_EQUAL(belle_sip_header_get_unparsed_value(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"From")),"toto");
	CU_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Record-Route"));
	CU_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Accept"));
	CU_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Accept-Encoding"));
	CU_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Accept-Language"));
	CU_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Allow"));
	CU_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Contact"));
	CU_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Organization"));
	CU_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Supported"));
	CU_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"User-Agent"));


	belle_sip_object_unref(request);

}
static void testUrisComponentsForRequest(void)  {
	belle_sip_request_t* request;
	belle_sip_stack_t *stack=belle_sip_stack_new(NULL);
	belle_sip_provider_t *prov=belle_sip_provider_new(stack,NULL);
	belle_sip_client_transaction_t* t;
	const char* raw_uri="sip:toto@titi.com?header1=blabla";

	request=belle_sip_request_create(	belle_sip_uri_parse(raw_uri)
										,"INVITE"
										,belle_sip_provider_create_call_id(prov)
										,belle_sip_header_cseq_create(20,"INVITE")
										,belle_sip_header_from_create2("sip:toto@titi.com","4654")
										,belle_sip_header_to_parse("To: sip:titi@titi.com:5061")
										,belle_sip_header_via_new()
										,70);
	CU_ASSERT_PTR_NOT_NULL(request);
	t=belle_sip_provider_create_client_transaction(prov,request);
	CU_ASSERT_NOT_EQUAL(belle_sip_client_transaction_send_request(t),0);
}

/* NOTE - ORDER IS IMPORTANT - MUST TEST fread() AFTER fprintf() */
test_t message_tests[] = {
	{ "REGISTER", testRegisterMessage },
	{ "INVITE", testInviteMessage },
	{ "Option message", testOptionMessage },
	{ "REGISTER (Raw)", testRegisterRaw },
	{ "401 Response", test401Response },
	{ "Origin extraction", test_extract_source },
	{ "SIP frag", test_sipfrag },
	{ "Malformed invite", testMalformedMessage },
	{ "Malformed invite with bad begin", testMalformedMessageWithWrongStart },
	{ "Malformed register", testMalformedOptionnalHeaderInMessage },
	{ "Channel parser error recovery", channel_parser_tester_recovery_from_error},
	{ "Channel parser malformed start", channel_parser_malformed_start},
	{ "RFC2543 compatibility", testRFC2543Compat},
	{ "Uri headers in sip INVITE",testUriHeadersInInvite},
	{ "Uris components in request",testUrisComponentsForRequest}
};

test_suite_t message_test_suite = {
	"Message",
	NULL,
	NULL,
	sizeof(message_tests) / sizeof(message_tests[0]),
	message_tests
};

