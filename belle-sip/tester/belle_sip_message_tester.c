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

#include "belle-sip/belle-sip.h"
#include "belle_sip_tester.h"


static void check_uri_and_headers(belle_sip_message_t* message) {
	if (belle_sip_message_is_request(message)) {
		BC_ASSERT_TRUE(belle_sip_request_get_uri(BELLE_SIP_REQUEST(message))|| belle_sip_request_get_absolute_uri(BELLE_SIP_REQUEST(message)) );

		BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Max-Forwards"));
		BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_MAX_FORWARDS(belle_sip_message_get_header(message,"Max-Forwards")));

		BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"User-Agent"));
		BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_USER_AGENT(belle_sip_message_get_header(message,"User-Agent")));
	}
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"From"));
	BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_FROM(belle_sip_message_get_header(message,"From")));

	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"To"));
	BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_TO(belle_sip_message_get_header(message,"To")));

	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"CSeq"));
	BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_CSEQ(belle_sip_message_get_header(message,"CSeq")));

	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Via"));
	BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_VIA(belle_sip_message_get_header(message,"Via")));

	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Call-ID"));
	BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_CALL_ID(belle_sip_message_get_header(message,"Call-ID")));


	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Content-Length"));
	BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_CONTENT_LENGTH(belle_sip_message_get_header(message,"Content-Length")));





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
	BC_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Expires"));
	BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_EXPIRES(belle_sip_message_get_header(message,"Expires")));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Proxy-Authorization"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Contact"));

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
	BC_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"INVITE");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Contact"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Authorization"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Content-Type"));
	check_uri_and_headers(message);
	belle_sip_object_unref(message);
	belle_sip_free(encoded_message);
}

static void testInviteMessageWithTelUri(void) {
	const char* raw_message = "INVITE tel:11234567888;phone-context=vzims.fr SIP/2.0\r\n"\
							"Via: SIP/2.0/UDP 10.23.17.117:22600;branch=z9hG4bK-d8754z-4d7620d2feccbfac-1---d8754z-;rport=4820;received=202.165.193.129\r\n"\
							"Max-Forwards: 70\r\n"\
							"Contact: <sip:bcheong@202.165.193.129:4820>\r\n"\
							"To: <tel:+3311234567888;tot=titi>\r\n"\
							"From: tel:11234567888;tag=werwrw\r\n"\
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
	BC_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"INVITE");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Contact"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Authorization"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Content-Type"));
	check_uri_and_headers(message);
	BC_ASSERT_PTR_NOT_NULL(belle_sip_header_address_get_absolute_uri(BELLE_SIP_HEADER_ADDRESS(belle_sip_message_get_header_by_type(message,belle_sip_header_from_t))));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_header_address_get_absolute_uri(BELLE_SIP_HEADER_ADDRESS(belle_sip_message_get_header_by_type(message,belle_sip_header_to_t))));
	BC_ASSERT_STRING_EQUAL(belle_generic_uri_get_opaque_part(belle_sip_request_get_absolute_uri(request)),"11234567888;phone-context=vzims.fr");
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
	BC_ASSERT_EQUAL(belle_sip_response_get_status_code(response),401,int,"%d");
	BC_ASSERT_STRING_EQUAL(belle_sip_response_get_reason_phrase(response),"Unauthorized");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"WWW-Authenticate"));
	check_uri_and_headers(message);
	belle_sip_object_unref(message);
	belle_sip_free(encoded_message);
}

static void test401ResponseWithoutResponsePhrase(void) {
	const char* raw_message = 	"SIP/2.0 401 \r\n"
								"Call-ID: 577586163\r\n"
								"CSeq: 21 REGISTER\r\n"
								"From: <sip:0033532176@sip.ovh.net>;tag=1790643209\r\n"
								"Server: Cirpack/v4.42x (gw_sip)\r\n"
								"To: <sip:0033482176@sip.ovh.net>;tag=00-08075-24212984-22e348d97\r\n"
								"Via: SIP/2.0/UDP 192.168.0.18:5062;received=81.56.113.2;rport=5062;branch=z9hG4bK1939354046\r\n"
								"WWW-Authenticate: Digest realm=\"sip.ovh.net\",\r\n   nonce=\"24212965507cde726e8bc37e04686459\",opaque=\"241b9fb347752f2\",stale=false,algorithm=MD5\r\n"
								"Content-Length: 0\r\n\r\n";
	belle_sip_response_t* response;
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	char* encoded_message = belle_sip_object_to_string(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	message = belle_sip_message_parse(encoded_message);
	response = BELLE_SIP_RESPONSE(message);
	BC_ASSERT_EQUAL(belle_sip_response_get_status_code(response),401,int,"%d");
	BC_ASSERT_PTR_NULL(belle_sip_response_get_reason_phrase(response));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"WWW-Authenticate"));
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
	BC_ASSERT_EQUAL((unsigned int)raw_message_size,(unsigned int)size+9,unsigned int,"%u");
	request = BELLE_SIP_REQUEST(message);
	BC_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_request_get_uri(request));
	BC_ASSERT_STRING_EQUAL(&raw_message[size],"123456789");
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
	BC_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_request_get_uri(request));
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
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Content-type"));
	BC_ASSERT_PTR_NOT_NULL(source);
	BC_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(source),"37.59.129.73");
	BC_ASSERT_EQUAL(belle_sip_uri_get_port(source),0,int,"%d");
	BC_ASSERT_STRING_EQUAL(belle_sip_uri_get_transport_param(source),"tcp");
	belle_sip_object_unref(message);
	belle_sip_object_unref(source);

	message = belle_sip_message_parse(invite_2);
	request = BELLE_SIP_REQUEST(message);
	source =belle_sip_request_extract_origin(request);
	BC_ASSERT_PTR_NOT_NULL(source);
	BC_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(source),"81.56.113.2");
	BC_ASSERT_EQUAL(belle_sip_uri_get_port(source),15060,int,"%d");
	BC_ASSERT_STRING_EQUAL(belle_sip_uri_get_transport_param(source),"udp");
	belle_sip_object_unref(message);
	belle_sip_object_unref(source);
}

static void test_sipfrag(void) {
	const char* raw_message = 	"SIP/2.0 100 Trying\r\n";
	belle_sip_response_t* response;
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	response = BELLE_SIP_RESPONSE(message);
	BC_ASSERT_EQUAL(belle_sip_response_get_status_code(response),100,int,"%d");
	BC_ASSERT_STRING_EQUAL(belle_sip_response_get_reason_phrase(response),"Trying");
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
	BC_ASSERT_FALSE(belle_sip_message_check_headers(message));
	belle_sip_object_unref(message);
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
	BC_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Expires"));
	BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_EXPIRES(belle_sip_message_get_header(message,"Expires")));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Proxy-Authorization"));
	BC_ASSERT_PTR_NULL(belle_sip_message_get_header(message,"Contact")); /*contact is optionnal in register*/

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
	BC_ASSERT_PTR_NULL(message);
}
#include "belle_sip_internal.h"

void channel_parser_tester_recovery_from_error_base (const char* prelude,const char* raw_message) {

	belle_sip_stack_t* stack = belle_sip_stack_new(NULL);
	belle_sip_channel_t* channel = belle_sip_stream_channel_new_client(stack
																	, NULL
																	, 45421
																	, NULL
																	, "127.0.0.1"
																	, 45421);


	belle_sip_request_t* request;
	belle_sip_message_t* message;

	if (prelude) {
		channel->input_stream.write_ptr = strcpy(channel->input_stream.write_ptr,prelude);
		channel->input_stream.write_ptr+=strlen(prelude);
		belle_sip_channel_parse_stream(channel,FALSE);
	}

	channel->input_stream.write_ptr = strcpy(channel->input_stream.write_ptr,raw_message);
	channel->input_stream.write_ptr+=strlen(raw_message);

	belle_sip_channel_parse_stream(channel,FALSE);

	BC_ASSERT_PTR_NOT_NULL(channel->incoming_messages);
	BC_ASSERT_PTR_NOT_NULL(channel->incoming_messages->data);
	message=BELLE_SIP_MESSAGE(channel->incoming_messages->data);
	BC_ASSERT_TRUE(BELLE_SIP_OBJECT_IS_INSTANCE_OF(message,belle_sip_request_t));
	request = BELLE_SIP_REQUEST(message);
	BC_ASSERT_STRING_EQUAL(belle_sip_request_get_method(request),"REGISTER");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Expires"));
	BC_ASSERT_PTR_NOT_NULL(BELLE_SIP_HEADER_EXPIRES(belle_sip_message_get_header(message,"Expires")));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Proxy-Authorization"));

	check_uri_and_headers(message);
	belle_sip_object_unref(channel);
	belle_sip_object_unref(stack);
}

void channel_parser_tester_recovery_from_error(void) {
	const char * raw_message=	"debut de stream tout pourri\r\n"
			"INVITE je_suis_une_fausse _request_uri_hihihi SIP/2.0\r\n"
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
	channel_parser_tester_recovery_from_error_base (NULL, raw_message);
}
void channel_parser_malformed_start(void) {
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
	channel_parser_tester_recovery_from_error_base (NULL, raw_message);
}

void channel_parser_truncated_start(void) {
	const char * prelude= "R";
	const char * raw_message=	"EGISTER sip:192.168.0.20 SIP/2.0\r\n"
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

	channel_parser_tester_recovery_from_error_base (prelude, raw_message);
}

void channel_parser_truncated_start_with_garbage(void) {
	const char * prelude= "truc tout pourit R";
	const char * raw_message=	"EGISTER sip:192.168.0.20 SIP/2.0\r\n"
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

	channel_parser_tester_recovery_from_error_base (prelude, raw_message);
}

static void testMalformedFrom_process_response_cb(void *user_ctx, const belle_sip_response_event_t *event){
	int status = belle_sip_response_get_status_code(belle_sip_response_event_get_response(event));

	belle_sip_message("testMalformedFrom_process_response_cb [%i]",status);

	(*(int*)user_ctx) += 1; // increment the call counter

	BC_ASSERT( status == 400 );
}

#define LISTENING_POINT_PORT 45421
#define LISTENING_POINT_HOSTPORT "127.0.0.1:45421" /* need the same port as above */

static void testMalformedFrom(void){
	belle_sip_stack_t*        stack = belle_sip_stack_new(NULL);
	belle_sip_listening_point_t* lp = belle_sip_stack_create_listening_point(stack,
																			 "127.0.0.1",
																			 LISTENING_POINT_PORT,
																			 "tcp");
	belle_sip_provider_t* provider = belle_sip_provider_new(stack,lp);
	belle_sip_listener_callbacks_t listener_cbs = {0};

	const char* raw_message = "INVITE sip:us2@172.1.1.1 SIP/2.0\r\n"
			"Via: SIP/2.0/TCP " LISTENING_POINT_HOSTPORT ";branch=z9hG4bK-edx-U_1zoIkaq72GJPqpSmDpJQ-ouBelFuLODzf9oS5J9MeFUA;rport\r\n"
			"From: c\x8e test <sip:00_12_34_56_78_90@us2>;tag=klsk+kwDc\r\n" /** 'cé test' should be enclosed in double quotes */
			"To: <sip:us2@172.1.1.1;transport=tcp>\r\n"
			"Contact: <sip:00_12_34_56_78_90@172.2.2.2>\r\n"
			"Call-ID: 2b6fb0320-1384-179494-426025-23b6b0-2e3303331@172.16.42.1\r\n"
			"Content-Type: application/sdp\r\n"
			"Content-Length: 389\r\n"
			"CSeq: 1 INVITE\r\n"
			"Allow: INVITE, ACK, BYE, CANCEL, OPTIONS, INFO, UPDATE, REGISTER, MESSAGE, REFER, SUBSCRIBE, PRACK\r\n"
			"Accept: application/sdp, application/dtmf-relay\r\n"
			"Max-Forwards: 69\r\n"
			"\r\n"
			"v=0\r\n"
			"o=- 1826 1826 IN IP4 172.16.42.1\r\n"
			"s=VeriCall Edge\r\n"
			"c=IN IP4 172.16.42.1\r\n"
			"t=0 0\r\n"
			"m=audio 20506 RTP/AVP 0 8 13 101\r\n"
			"a=rtpmap:0 PCMU/8000\r\n"
			"a=rtpmap:8 PCMA/8000\r\n"
			"a=rtpmap:13 CN/8000\r\n"
			"a=rtpmap:101 telephone-event/8000\r\n"
			"a=fmtp:101 0-15\r\n"
			"m=video 24194 RTP/AVP 105 104\r\n"
			"a=sendonly\r\n"
			"a=rtpmap:105 H264/90000\r\n"
			"a=fmtp:105 packetization-mode=0\r\n"
			"a=rtpmap:104 H263-1998/90000\r\n"
			"a=fmtp:104 CIF=1;J=1\r\n";

	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	belle_sip_listener_t* listener = NULL;

	int called_times = 0;

	listener_cbs.process_response_event = testMalformedFrom_process_response_cb;
	listener = belle_sip_listener_create_from_callbacks(&listener_cbs, &called_times);
	belle_sip_provider_add_sip_listener(provider, listener);

	belle_sip_object_ref(message);
	belle_sip_object_ref(message); /* double ref: originally the message is created with 0 refcount, and dispatch_message will unref() it.*/
	belle_sip_provider_dispatch_message(provider, message);
	// we expect the stack to send a 400 error
	belle_sip_stack_sleep(stack,1000);

	BC_ASSERT_EQUAL(called_times,1,int,"%d");
	belle_sip_provider_remove_sip_listener(provider,listener);

	belle_sip_object_unref(listener);
	belle_sip_object_unref(provider);
	belle_sip_object_unref(stack);
	belle_sip_object_unref(message);

}


static void testMalformedMandatoryField(void){
	belle_sip_stack_t*        stack = belle_sip_stack_new(NULL);
	belle_sip_listening_point_t* lp = belle_sip_stack_create_listening_point(stack,
																			 "127.0.0.1",
																			 LISTENING_POINT_PORT,
																			 "tcp");
	belle_sip_provider_t* provider = belle_sip_provider_new(stack,lp);
	belle_sip_listener_callbacks_t listener_cbs = {0};

	/* the MESSAGE message has no definition on which fields are required, which means we'll go into
	 *
	 *
	 *
	 */

	const char* raw_message = "MESSAGE sip:lollol.iphone@22.22.222.222:5861;transport=tcp SIP/2.0\r\n"
			"Via: SIP/2.0/TCP " LISTENING_POINT_HOSTPORT ";branch=z9hG4bK5eca096a;rport\r\n"
			"Max-Forwards: 70\r\n"
			"From: \"MS TFT\" <sip:lollol-labo-ms-tft1@11.11.111.111>;tag=as2413a381\r\n"
			"To: <sip:lollol-labo-iphone4s@22.22.22.222:5861;app-id=fr.lollol.phone.prod;pn-type=apple;pn-tok=azertyuiopqsdfghhjjkmlqoijfubieuzhqiluehcpoqidufqsdkjlcnuoishcvs;pn-msg-str=IM_MSG;pn-call-str=IC_MSG;pn-call-snd=ring.caf;pn-msg-snd=msg.caf;transport=tcp>;tag=\r\n"
			"Call-ID: 4070383971a9674201f463af2de1f012@11.11.111.111:5060\r\n"
			"CSeq: 103 MESSAGE\r\n"
			"User-Agent: Sip Server On Host (20130523_12h10)\r\n"
			"Content-Type: text/plain;charset=UTF-8\r\n"
			"Content-Length: 276\r\n"
			"\r\n"
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
			"<VDUCMediaConfig GId=\"1234567\" IdPosteType=\"123\"><Label>Salut Bilout TFT MS2</Label><MediaConfig GId=\"456\"><CommandCode Code=\"MediaCommand*\"><Label>Porte ouverte</Label></CommandCode><withVideo FPS=\"0.0\"/></MediaConfig></VDUCMediaConfig>\r\n"
			"\r\n";

	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	belle_sip_listener_t* listener = NULL;

	int called_times = 0;

	listener_cbs.process_response_event = testMalformedFrom_process_response_cb;
	listener = belle_sip_listener_create_from_callbacks(&listener_cbs, &called_times);

	belle_sip_provider_add_sip_listener(provider, listener);

	belle_sip_object_ref(message);
	belle_sip_object_ref(message); /* double ref: originally the message is created with 0 refcount, and dispatch_message will unref() it.*/

	belle_sip_provider_dispatch_message(provider, message);
	// we expect the stack to send a 400 error
	belle_sip_stack_sleep(stack,1000);

	BC_ASSERT_EQUAL(called_times,1,int,"%d");
	belle_sip_provider_remove_sip_listener(provider,listener);

	belle_sip_object_unref(listener);
	belle_sip_object_unref(provider);
	belle_sip_object_unref(stack);
	belle_sip_object_unref(message);

}




static void testRFC2543_base(char* branch) {
	belle_sip_server_transaction_t *tr;
	const char* raw_message_base = 	"INVITE sip:me@127.0.0.1 SIP/2.0\r\n"
			"Via: SIP/2.0/UDP 192.168.1.12:15060;%srport=15060;received=81.56.113.2\r\n"
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

	char raw_message[2048];
	belle_sip_request_t* request;
	belle_sip_stack_t *stack=belle_sip_stack_new(NULL);
	belle_sip_provider_t *prov=belle_sip_provider_new(stack,NULL);
	belle_sip_message_t* message;

	snprintf(raw_message,sizeof(raw_message),raw_message_base,branch);

	message = belle_sip_message_parse(raw_message);
	belle_sip_object_ref(message);
	belle_sip_object_ref(message); /*yes double ref: originally the message is created with 0 refcount, and dispatch_message will unref() it.*/
	belle_sip_provider_dispatch_message(prov,message);
	request = BELLE_SIP_REQUEST(message);

	BC_ASSERT_PTR_NOT_NULL(request);
	tr=belle_sip_provider_create_server_transaction(prov,request);
	BC_ASSERT_PTR_NOT_NULL(belle_sip_provider_find_matching_server_transaction(prov,request)); /*make sure branch id is properly set*/
	BC_ASSERT_PTR_NOT_NULL(tr);
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
	belle_sip_object_unref(message);
}
static void testRFC2543Compat(void) {
	testRFC2543_base("");
}
static void testRFC2543CompatWithBranch(void) {
	testRFC2543_base("branch=blablabla;");
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
	BC_ASSERT_PTR_NOT_NULL(request);
	BC_ASSERT_PTR_NOT_NULL(raw_header=belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"header1"));
	if (raw_header) {
		BC_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(raw_header)),"blabla");
	}
	BC_ASSERT_PTR_NOT_NULL(raw_header=belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"header2"));
	if (raw_header) {
		BC_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(raw_header)),"blue;weftutu=bla");
	}
	BC_ASSERT_PTR_NOT_NULL(raw_header=belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"To"));
	if (raw_header) {
		BC_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(raw_header)),"sip:toto@titi.com");
	}
	BC_ASSERT_STRING_NOT_EQUAL(belle_sip_header_get_unparsed_value(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"From")),"toto");
	BC_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Record-Route"));
	BC_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Accept"));
	BC_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Accept-Encoding"));
	BC_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Accept-Language"));
	BC_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Allow"));
	BC_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Contact"));
	BC_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Organization"));
	BC_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"Supported"));
	BC_ASSERT_PTR_NULL(belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),"User-Agent"));


	belle_sip_object_unref(request);
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);

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
	BC_ASSERT_PTR_NOT_NULL(request);
	t=belle_sip_provider_create_client_transaction(prov,request);
	BC_ASSERT_NOT_EQUAL(belle_sip_client_transaction_send_request(t),0,int,"%d");
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
}

static void testGenericMessage(void) {
	const char* raw_message = 	"SIP/2.0 180 Ringing\r\n"
			"Via: SIP/2.0/UDP 192.168.1.73:5060;branch=z9hG4bK.hhdJx4~kD;rport\r\n"
			"Record-Route: <sip:91.121.209.194;lr>\r\n"
			"Record-Route: <sip:siproxd@192.168.1.254:5060;lr>\r\n"
			"From: <sip:granny2@sip.linphone.org>;tag=5DuaoDRru\r\n"
			"To: <sip:chmac@sip.linphone.org>;tag=PelIhu0\r\n"
			"Call-ID: e-2Q~fxwNs\r\n"
			"CSeq: 21 INVITE\r\n"
			"user-agent: Linphone/3.6.99 (belle-sip/1.2.4)\r\n"
			"supported: replaces\r\n"
			"supported: outbound\r\n"
			"Content-Length: 0\r\n"
			"\r\n";

	belle_sip_response_t* response;
	belle_sip_message_t* message = belle_sip_message_parse(raw_message);
	char* encoded_message = belle_sip_object_to_string(BELLE_SIP_OBJECT(message));
	belle_sip_object_unref(BELLE_SIP_OBJECT(message));
	message = belle_sip_message_parse(encoded_message);
	response = BELLE_SIP_RESPONSE(message);
	BC_ASSERT_EQUAL(belle_sip_response_get_status_code(response),180,int,"%d");
/*	BC_ASSERT_STRING_EQUAL(belle_sip_response_get_reason_phrase(response),"Unauthorized");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"WWW-Authenticate"));
	check_uri_and_headers(message);*/
	belle_sip_object_unref(message);
	belle_sip_free(encoded_message);
}



static void testHttpGet(void)  {
	const char* raw_message = 	"GET /index.php HTTP/1.1\r\n"
							 	"User-Agent: Wget/1.14 (darwin11.4.2)\r\n"
								"Accept: */*\r\n"
								"Host: www.linphone.org\r\n"
								"Connection: Keep-Alive\r\n"
								"\r\n";
	char* marshaled_msg;
	belle_sip_message_t* msg = belle_sip_message_parse(raw_message);
	belle_http_request_t* http_request;
	belle_generic_uri_t* uri;
	belle_sip_header_extension_t* host_header;
	belle_sip_object_t* tmp;

	if (!BC_ASSERT_PTR_NOT_NULL(msg)) return;

	marshaled_msg=belle_sip_object_to_string(BELLE_SIP_OBJECT(msg));
	belle_sip_object_unref(msg);
	msg = belle_sip_message_parse(marshaled_msg);
	belle_sip_free(marshaled_msg);
	tmp=belle_sip_object_clone(BELLE_SIP_OBJECT(msg));
	belle_sip_object_unref(msg);
	msg=BELLE_SIP_MESSAGE(tmp);

	BC_ASSERT_TRUE(BELLE_SIP_IS_INSTANCE_OF(msg,belle_http_request_t));
	http_request=BELLE_HTTP_REQUEST(msg);
	if (!BC_ASSERT_PTR_NOT_NULL(uri=belle_http_request_get_uri(http_request))) return;

	BC_ASSERT_STRING_EQUAL(belle_generic_uri_get_path(uri),"/index.php");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(msg,"User-Agent"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(msg,"Accept"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(msg,"Connection"));
	BC_ASSERT_PTR_NOT_NULL(host_header=BELLE_SIP_HEADER_EXTENSION(belle_sip_message_get_header(msg,"Host")));
	BC_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(host_header),"www.linphone.org");
	belle_sip_object_unref(msg);
}

static void testHttp200Ok(void)  {
	const char* raw_message = 	"HTTP/1.1 200 OK\r\n"
								"Date: Tue, 07 Jan 2014 09:28:43 GMT\r\n"
								"Server: Apache\r\n"
								"Last-Modified: Tue, 18 Aug 1998 20:19:11 GMT\r\n"
								"ETag: \"8982a60-14a17-335b3dcdcadc0\"\r\n"
								"Accept-Ranges: bytes\r\n"
								"Vary: Accept-Encoding\r\n"
								"Content-Encoding: gzip\r\n"
								"Content-Length: 6\r\n"
								"Keep-Alive: timeout=15, max=100\r\n"
								"Connection: Keep-Alive\r\n"
								"Content-Type: text/plain\r\n"
								"\r\n"
								"blabla";

	char* marshaled_msg;
	belle_sip_message_t* msg = belle_sip_message_parse(raw_message);
	belle_http_response_t* http_response;
	belle_sip_header_extension_t* host_header;
	belle_sip_object_t* tmp;

	if (!BC_ASSERT_PTR_NOT_NULL(msg)) return;

	marshaled_msg=belle_sip_object_to_string(BELLE_SIP_OBJECT(msg));
	belle_sip_object_unref(msg);
	msg = belle_sip_message_parse(marshaled_msg);
	belle_sip_free(marshaled_msg);
	tmp=belle_sip_object_clone(BELLE_SIP_OBJECT(msg));
	belle_sip_object_unref(msg);
	msg=BELLE_SIP_MESSAGE(tmp);

	BC_ASSERT_TRUE(BELLE_SIP_IS_INSTANCE_OF(msg,belle_http_response_t));
	http_response=BELLE_HTTP_RESPONSE(msg);

	BC_ASSERT_EQUAL(belle_http_response_get_status_code(http_response),200,int,"%d");
	BC_ASSERT_STRING_EQUAL(belle_http_response_get_reason_phrase(http_response),"OK");

	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(msg,"Date"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(msg,"ETag"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(msg,"Connection"));
	BC_ASSERT_PTR_NOT_NULL(host_header=BELLE_SIP_HEADER_EXTENSION(belle_sip_message_get_header(msg,"Server")));
	BC_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(host_header),"Apache");
	belle_sip_object_unref(msg);
}


void channel_parser_http_response(void) {
	belle_sip_stack_t* stack = belle_sip_stack_new(NULL);
	belle_sip_channel_t* channel = belle_sip_stream_channel_new_client(stack
																	, NULL
																	, 45421
																	, NULL
																	, "127.0.0.1"
																	, 45421);

	const char * raw_message=	"HTTP/1.1 200 OK\r\n"
								"Cache-Control: private\r\n"
								"Date: Tue, 07 Jan 2014 13:51:57 GMT\r\n"
								"Content-Type: text/html; charset=utf-8\r\n"
								"Server: Microsoft-IIS/6.0\r\n"
								"X-Powered-By: ASP.NET\r\n"
								"Content-Encoding: gzip\r\n"
								"Vary: Accept-Encoding\r\n"
								"Transfer-Encoding: chunked\r\n"
								"\r\n"
								"<html></html>\r\n\r\n";
	belle_http_response_t* response;
	belle_sip_message_t* message;
	channel->input_stream.write_ptr = strcpy(channel->input_stream.write_ptr,raw_message);
	channel->input_stream.write_ptr+=strlen(raw_message);

	belle_sip_channel_parse_stream(channel,TRUE);

	BC_ASSERT_PTR_NOT_NULL(channel->incoming_messages);
	BC_ASSERT_PTR_NOT_NULL(channel->incoming_messages->data);
	message=BELLE_SIP_MESSAGE(channel->incoming_messages->data);
	BC_ASSERT_TRUE(BELLE_SIP_OBJECT_IS_INSTANCE_OF(message,belle_http_response_t));
	response = BELLE_HTTP_RESPONSE(message);
	BC_ASSERT_STRING_EQUAL(belle_http_response_get_reason_phrase(response),"OK");
	BC_ASSERT_EQUAL(belle_http_response_get_status_code(response),200,int,"%d");
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Cache-Control"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header(message,"Vary"));

	belle_sip_object_unref(channel);
	belle_sip_object_unref(stack);
}

void testGetBody(void) {
	const char* raw_message = "INVITE sip:us2@172.1.1.1 SIP/2.0\r\n"
				"Via: SIP/2.0/TCP " LISTENING_POINT_HOSTPORT ";branch=z9hG4bK-edx-U_1zoIkaq72GJPqpSmDpJQ-ouBelFuLODzf9oS5J9MeFUA;rport\r\n"
				"From: test <sip:00_12_34_56_78_90@us2>;tag=klsk+kwDc\r\n"
				"To: <sip:us2@172.1.1.1;transport=tcp>\r\n"
				"Contact: <sip:00_12_34_56_78_90@172.2.2.2>\r\n"
				"Call-ID: 2b6fb0320-1384-179494-426025-23b6b0-2e3303331@172.16.42.1\r\n"
				"Content-Type: application/sdp\r\n"
				"Content-Length: 389\r\n"
				"CSeq: 1 INVITE\r\n"
				"Allow: INVITE, ACK, BYE, CANCEL, OPTIONS, INFO, UPDATE, REGISTER, MESSAGE, REFER, SUBSCRIBE, PRACK\r\n"
				"Accept: application/sdp, application/dtmf-relay\r\n"
				"Max-Forwards: 69\r\n"
				"\r\n"
				"v=0\r\n"
				"o=- 1826 1826 IN IP4 172.16.42.1\r\n"
				"s=VeriCall Edge\r\n"
				"c=IN IP4 172.16.42.1\r\n"
				"t=0 0\r\n"
				"m=audio 20506 RTP/AVP 0 8 13 101\r\n"
				"a=rtpmap:0 PCMU/8000\r\n"
				"a=rtpmap:8 PCMA/8000\r\n"
				"a=rtpmap:13 CN/8000\r\n"
				"a=rtpmap:101 telephone-event/8000\r\n"
				"a=fmtp:101 0-15\r\n"
				"m=video 24194 RTP/AVP 105 104\r\n"
				"a=sendonly\r\n"
				"a=rtpmap:105 H264/90000\r\n"
				"a=fmtp:105 packetization-mode=0\r\n"
				"a=rtpmap:104 H263-1998/90000\r\n"
				"a=fmtp:104 CIF=1;J=1\r\n"
				"nimportequoi a la fin";
	belle_sip_stack_t* stack = belle_sip_stack_new(NULL);
	belle_sip_channel_t* channel = belle_sip_stream_channel_new_client(stack
																	, NULL
																	, LISTENING_POINT_PORT
																	, NULL
																	, "127.0.0.1"
																	, LISTENING_POINT_PORT);


	belle_sip_message_t* message;
	belle_sip_header_content_length_t *ctlt;

	channel->input_stream.write_ptr = strcpy(channel->input_stream.write_ptr,raw_message);
	channel->input_stream.write_ptr+=strlen(raw_message);

	belle_sip_channel_parse_stream(channel,FALSE);

	BC_ASSERT_PTR_NOT_NULL(channel->incoming_messages);
	BC_ASSERT_PTR_NOT_NULL(channel->incoming_messages->data);
	message=BELLE_SIP_MESSAGE(channel->incoming_messages->data);

	ctlt = belle_sip_message_get_header_by_type(message,belle_sip_header_content_length_t);
	BC_ASSERT_PTR_NOT_NULL(ctlt);
	BC_ASSERT_EQUAL((unsigned int)belle_sip_header_content_length_get_content_length(ctlt),(unsigned int)strlen(belle_sip_message_get_body(message)),unsigned int,"%u");
	BC_ASSERT_EQUAL((unsigned int)belle_sip_header_content_length_get_content_length(ctlt),(unsigned int)belle_sip_message_get_body_size(message),unsigned int,"%u");
	belle_sip_object_unref(channel);
	belle_sip_object_unref(stack);
}

static void testHop(void){
	belle_sip_uri_t * uri = belle_sip_uri_parse("sip:sip.linphone.org;maddr=[2001:41d0:8:6e48::]");
	belle_sip_hop_t *hop = belle_sip_hop_new_from_uri(uri);
	BC_ASSERT_STRING_EQUAL(hop->host, "2001:41d0:8:6e48::");
	belle_sip_object_unref(uri);
	belle_sip_object_unref(hop);
}


/* NOTE - ORDER IS IMPORTANT - MUST TEST fread() AFTER fprintf() */
test_t message_tests[] = {
	TEST_NO_TAG("REGISTER", testRegisterMessage),
	TEST_NO_TAG("INVITE", testInviteMessage),
	TEST_NO_TAG("INVITE with tel uri", testInviteMessageWithTelUri),
	TEST_NO_TAG("Option message", testOptionMessage),
	TEST_NO_TAG("REGISTER (Raw)", testRegisterRaw),
	TEST_NO_TAG("401 Response", test401Response),
	TEST_NO_TAG("Response without response phrase",test401ResponseWithoutResponsePhrase),
	TEST_NO_TAG("Origin extraction", test_extract_source),
	TEST_NO_TAG("SIP frag", test_sipfrag),
	TEST_NO_TAG("Malformed invite", testMalformedMessage),
	TEST_NO_TAG("Malformed from", testMalformedFrom),
	TEST_NO_TAG("Malformed mandatory field", testMalformedMandatoryField),
	TEST_NO_TAG("Malformed invite with bad begin", testMalformedMessageWithWrongStart),
	TEST_NO_TAG("Malformed register", testMalformedOptionnalHeaderInMessage),
	TEST_NO_TAG("Channel parser error recovery", channel_parser_tester_recovery_from_error),
	TEST_NO_TAG("Channel parser malformed start", channel_parser_malformed_start),
	TEST_NO_TAG("Channel parser truncated start", channel_parser_truncated_start),
	TEST_NO_TAG("Channel parser truncated start with garbage",channel_parser_truncated_start_with_garbage),
	TEST_ONE_TAG("RFC2543 compatibility", testRFC2543Compat, "LeaksMemory"),
	TEST_ONE_TAG("RFC2543 compatibility with branch id",testRFC2543CompatWithBranch, "LeaksMemory"),
	TEST_NO_TAG("Uri headers in sip INVITE",testUriHeadersInInvite),
	TEST_NO_TAG("Uris components in request",testUrisComponentsForRequest),
	TEST_NO_TAG("Generic message test",testGenericMessage),
	TEST_NO_TAG("HTTP get",testHttpGet),
	TEST_NO_TAG("HTTP 200 Ok",testHttp200Ok),
	TEST_NO_TAG("Channel parser for HTTP reponse",channel_parser_http_response),
	TEST_NO_TAG("Get body size",testGetBody),
	TEST_NO_TAG("Create hop from uri", testHop)
};

test_suite_t message_test_suite = {"Message", NULL, NULL, belle_sip_tester_before_each, belle_sip_tester_after_each,
								   sizeof(message_tests) / sizeof(message_tests[0]), message_tests};
