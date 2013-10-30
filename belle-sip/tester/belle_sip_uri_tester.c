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


static void testSIMPLEURI(void) {
	belle_sip_uri_t* L_tmp;
	belle_sip_uri_t* L_uri = belle_sip_uri_parse("sip:sip.titi.com");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_tmp = belle_sip_uri_parse(l_raw_uri);
	L_uri = BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_uri);

	CU_ASSERT_PTR_NULL(belle_sip_uri_get_user(L_uri));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "sip.titi.com");
	CU_ASSERT_PTR_NULL(belle_sip_uri_get_transport_param(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
}

static void testCOMPLEXURI(void) {
	belle_sip_uri_t* L_tmp;
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sip:toto@titi.com:5060;transport=tcp");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_tmp = belle_sip_uri_parse(l_raw_uri);
	L_uri = BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_user(L_uri), "toto");
	CU_ASSERT_EQUAL(belle_sip_uri_get_port(L_uri), 5060);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_transport_param(L_uri), "tcp");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
}

static void testIPV6URI_base(const char* ip6) {
	belle_sip_uri_t* L_tmp;
	belle_sip_uri_t * L_uri;
	char* l_raw_uri;
	char uri[256];
	snprintf(uri,sizeof(uri),"sip:toto@[%s]:5060;transport=tcp",ip6);
	L_uri = belle_sip_uri_parse(uri);
	l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_tmp = belle_sip_uri_parse(l_raw_uri);
	L_uri = BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_user(L_uri), "toto");
	CU_ASSERT_EQUAL(belle_sip_uri_get_port(L_uri), 5060);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri),ip6);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_transport_param(L_uri), "tcp");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
}

static void testIPV6URI(void) {
	testIPV6URI_base("fe80::1");
	testIPV6URI_base("2a01:e35:1387:1020:6233:4bff:fe0b:5663");
	testIPV6URI_base("2a01:e35:1387:1020:6233::5663");
	testIPV6URI_base("::1");
}

static void testSIPSURI(void) {

	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sips:linphone.org");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_uri = belle_sip_uri_parse(l_raw_uri);
	belle_sip_free(l_raw_uri);
	CU_ASSERT_EQUAL(belle_sip_uri_is_secure(L_uri), 1);
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_uri = belle_sip_uri_parse("sip:linphone.org");
	CU_ASSERT_EQUAL(belle_sip_uri_is_secure(L_uri), 0);
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
}

static void test_ip_host(void) {
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sip:192.168.0.1");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_uri = belle_sip_uri_parse(l_raw_uri);
	belle_sip_free(l_raw_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "192.168.0.1");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
}

static void test_lr(void) {
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sip:192.168.0.1;lr");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_uri = belle_sip_uri_parse(l_raw_uri);
	belle_sip_free(l_raw_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "192.168.0.1");
	CU_ASSERT_EQUAL(belle_sip_uri_has_lr_param(L_uri), 1);
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));

}

static void test_maddr(void) {
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sip:192.168.0.1;lr;maddr=linphone.org");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_uri = belle_sip_uri_parse(l_raw_uri);
	belle_sip_free(l_raw_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_maddr_param(L_uri), "linphone.org");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));

}

static void test_user_passwd(void) {
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sip:toto:tata@bla;");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_uri = belle_sip_uri_parse(l_raw_uri);
	belle_sip_free(l_raw_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_user_password(L_uri), "tata");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));

}


static void test_uri_parameters (void) {
	char* l_raw_uri;
	belle_sip_uri_t* L_tmp;
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sip:192.168.0.1;ttl=12");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));

	L_uri = belle_sip_uri_parse("sip:maddr=@192.168.0.1;lr;maddr=192.168.0.1;user=ip;ttl=140;transport=sctp;method=INVITE;rport=5060");
	l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));

	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_tmp = belle_sip_uri_parse(l_raw_uri);
	L_uri = BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));

	belle_sip_free(l_raw_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_maddr_param(L_uri), "192.168.0.1");
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_user_param(L_uri), "ip");
	CU_ASSERT_EQUAL(belle_sip_uri_get_ttl_param(L_uri),140);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_transport_param(L_uri), "sctp");
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_method_param(L_uri), "INVITE");

	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
}

static void test_headers(void) {
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sip:192.168.0.1?toto=titi");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_uri = belle_sip_uri_parse(l_raw_uri);
	belle_sip_free(l_raw_uri);
	CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_uri_get_header(L_uri,"toto"));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_header(L_uri,"toto"), "titi");

	CU_ASSERT_PTR_NULL(belle_sip_uri_get_header(L_uri,"bla"));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_uri = belle_sip_uri_parse("sip:192.168.0.1?toto=titi&header2=popo&header3=");
	l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_uri = belle_sip_uri_parse(l_raw_uri);
	belle_sip_free(l_raw_uri);

	CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_uri_get_header(L_uri,"toto"));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_header(L_uri,"header2"), "popo");
	belle_sip_object_unref(L_uri);
}
static void test_escaped_headers(void) {
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sip:toto@sip.linhone.org?User-to-User=323a313030363a3230385a48363039313941364b4342463845495936%3Bencoding%3Dhex");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_uri = belle_sip_uri_parse(l_raw_uri);
	belle_sip_free(l_raw_uri);
	CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_uri_get_header(L_uri,"User-to-User"));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_header(L_uri,"User-to-User"), "323a313030363a3230385a48363039313941364b4342463845495936;encoding=hex");

	belle_sip_object_unref(L_uri);
}

static void testSIMPLEURI_error(void) {
	belle_sip_uri_t* L_uri = belle_sip_uri_parse("siptcom");
	CU_ASSERT_PTR_NULL(L_uri);

}

static void test_escaped_username(void) {
	belle_sip_uri_t* L_tmp;
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sip:toto%40linphone.org@titi.com");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_tmp = belle_sip_uri_parse(l_raw_uri);
	L_uri = BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_user(L_uri), "toto@linphone.org");
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
}

static void test_escaped_passwd(void) {
	belle_sip_uri_t* L_tmp;
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sips:%22jehan%22%20%3cjehan%40sip2.linphone.org:544%3e@sip.linphone.org");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_tmp = belle_sip_uri_parse(l_raw_uri);
	L_uri = BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_user(L_uri), "\"jehan\" <jehan@sip2.linphone.org");
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "sip.linphone.org");
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_user_password(L_uri), "544>");

	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));

}


static void test_escaped_parameter(void) {
	belle_sip_uri_t* L_tmp;
	belle_sip_uri_t *  L_uri = belle_sip_uri_parse("sip:toto@titi.com;pa%3Dram=aa%40bb;o%40");
	char* l_raw_uri = belle_sip_object_to_string(BELLE_SIP_OBJECT(L_uri));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
	L_tmp = belle_sip_uri_parse(l_raw_uri);
	L_uri = BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(L_tmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_tmp));
	belle_sip_free(l_raw_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(L_uri), "pa=ram"), "aa@bb");
	CU_ASSERT_TRUE(belle_sip_parameters_has_parameter(BELLE_SIP_PARAMETERS(L_uri), "o@"));
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");
	belle_sip_object_unref(BELLE_SIP_OBJECT(L_uri));
}

static void test_uri_equals(void) {
	belle_sip_uri_t* a;
	belle_sip_uri_t* b;
/*
	 *    The URIs within each of the following sets are equivalent:

	   sip:%61lice@atlanta.com;transport=TCP
	   sip:alice@AtLanTa.CoM;Transport=tcp
*/
	a = belle_sip_uri_parse("sip:%61lice@atlanta.com;transport=TCP");
	CU_ASSERT_PTR_NOT_NULL_FATAL(a);
	b = belle_sip_uri_parse("sip:alice@AtLanTa.CoM;Transport=tcp");
	CU_ASSERT_PTR_NOT_NULL_FATAL(b);
	CU_ASSERT_TRUE(belle_sip_uri_equals(a,b));
	belle_sip_object_unref(a);
	belle_sip_object_unref(b);
	/*
	   sip:carol@chicago.com
	   sip:carol@chicago.com;newparam=5
	   sip:carol@chicago.com;security=on
*/
	a = belle_sip_uri_parse("sip:carol@chicago.com");
	CU_ASSERT_PTR_NOT_NULL_FATAL(a);
	b = belle_sip_uri_parse("sip:carol@chicago.com;newparam=5");
	CU_ASSERT_PTR_NOT_NULL_FATAL(b);
	CU_ASSERT_TRUE(belle_sip_uri_equals(a,b));
	belle_sip_object_unref(a);
	belle_sip_object_unref(b);
/*

	   sip:biloxi.com;transport=tcp;method=REGISTER?to=sip:bob%40biloxi.com
	   sip:biloxi.com;method=REGISTER;transport=tcp?to=sip:bob%40biloxi.com
*/
	a = belle_sip_uri_parse("sip:biloxi.com;transport=tcp;method=REGISTER?to=sip:bob%40biloxi.com");
	CU_ASSERT_PTR_NOT_NULL_FATAL(a);
	b = belle_sip_uri_parse("sip:biloxi.com;method=REGISTER;transport=tcp?to=sip:bob%40biloxi.com");
	CU_ASSERT_PTR_NOT_NULL_FATAL(b);
	CU_ASSERT_TRUE(belle_sip_uri_equals(a,b));
	belle_sip_object_unref(a);
	belle_sip_object_unref(b);
	/*
	sip:alice@atlanta.com?subject=project%20x&priority=urgent
	   sip:alice@atlanta.com?priority=urgent&subject=project%20x

	   The URIs within each of the following sets are not equivalent:

	   SIP:ALICE@AtLanTa.CoM;Transport=udp             (different usernames)
	   sip:alice@AtLanTa.CoM;Transport=UDP
*/
	a = belle_sip_uri_parse("sip:ALICE@AtLanTa.CoM;Transport=udp");
	CU_ASSERT_PTR_NOT_NULL_FATAL(a);
	b = belle_sip_uri_parse("sip:alice@AtLanTa.CoM;Transport=UDP");
	CU_ASSERT_PTR_NOT_NULL_FATAL(b);
	CU_ASSERT_FALSE(belle_sip_uri_equals(a,b));
	belle_sip_object_unref(a);
	belle_sip_object_unref(b);
	/*
	   sip:bob@biloxi.com                   (can resolve to different ports)
	   sip:bob@biloxi.com:5060
*/
	a = belle_sip_uri_parse("sip:ALICE@AtLanTa.CoM;Transport=udp");
	CU_ASSERT_PTR_NOT_NULL_FATAL(a);
	b = belle_sip_uri_parse("sip:alice@AtLanTa.CoM;Transport=UDP");
	CU_ASSERT_PTR_NOT_NULL_FATAL(b);
	CU_ASSERT_FALSE(belle_sip_uri_equals(a,b));
	belle_sip_object_unref(a);
	belle_sip_object_unref(b);
	/*
	sip:bob@biloxi.com              (can resolve to different transports)
	   sip:bob@biloxi.com;transport=udp
*/
	a = belle_sip_uri_parse("sip:bob@biloxi.com");
	CU_ASSERT_PTR_NOT_NULL_FATAL(a);
	b = belle_sip_uri_parse("sip:bob@biloxi.com;transport=udp");
	CU_ASSERT_PTR_NOT_NULL_FATAL(b);
	CU_ASSERT_FALSE(belle_sip_uri_equals(a,b));
	belle_sip_object_unref(a);
	belle_sip_object_unref(b);
/*	   sip:bob@biloxi.com     (can resolve to different port and transports)
	   sip:bob@biloxi.com:6000;transport=tcp
*/
	a = belle_sip_uri_parse("sip:bob@biloxi.com");
	CU_ASSERT_PTR_NOT_NULL_FATAL(a);
	b = belle_sip_uri_parse("sip:bob@biloxi.com:6000;transport=tcp");
	CU_ASSERT_PTR_NOT_NULL_FATAL(b);
	CU_ASSERT_FALSE(belle_sip_uri_equals(a,b));
	belle_sip_object_unref(a);
	belle_sip_object_unref(b);

	/*	   sip:carol@chicago.com                    (different header component)
	   sip:carol@chicago.com?Subject=next%20meeting

	   sip:bob@phone21.boxesbybob.com   (even though that's what
	   sip:bob@192.0.2.4                 phone21.boxesbybob.com resolves to)

	   Note that equality is not transitive:

	      o  sip:carol@chicago.com and sip:carol@chicago.com;security=on are
	         equivalent

	      o  sip:carol@chicago.com and sip:carol@chicago.com;security=off
	         are equivalent

	      o  sip:carol@chicago.com;security=on and
	         sip:carol@chicago.com;security=off are not equivalent
	Rosenberg, et. al.          Standards Track                   [Page 155]

	RFC 3261            SIP: Session Initiation Protocol           June 2002

	 */


}

/*
 * From 19.1.1 SIP and SIPS URI Components
 * 									   				dialog
										reg./redir. Contact/
			default  Req.-URI  To  From  Contact   R-R/Route  external
user          --          o      o    o       o          o         o
password      --          o      o    o       o          o         o
host          --          m      m    m       m          m         m
port          (1)         o      -    -       o          o         o
user-param    ip          o      o    o       o          o         o
method        INVITE      -      -    -       -          -         o
maddr-param   --          o      -    -       o          o         o
ttl-param     1           o      -    -       o          -         o
transp.-param (2)         o      -    -       o          o         o
lr-param      --          o      -    -       -          o         o
other-param   --          o      o    o       o          o         o
headers       --          -      -    -       o          -         o*/
void testUriComponentsChecker() {
	belle_sip_uri_t* uri = belle_sip_uri_parse("sip:hostonly");
	CU_ASSERT_TRUE(belle_sip_uri_check_components_from_request_uri(uri));
	belle_sip_object_unref(uri);

	{
	belle_sip_header_from_t*	header = belle_sip_header_from_parse("From: sip:linphone.org:5061");
	CU_ASSERT_FALSE(belle_sip_uri_check_components_from_context(belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(header)),NULL,"From"));
	belle_sip_object_unref(header);
	}
	{
	belle_sip_header_to_t*	header = belle_sip_header_to_parse("To: sip:linphone.org?header=interdit");
	CU_ASSERT_FALSE(belle_sip_uri_check_components_from_context(belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(header)),NULL,"To"));
	belle_sip_object_unref(header);
	}
	{
	belle_sip_header_contact_t*	header = belle_sip_header_contact_parse("Contact: <sip:linphone.org;lr>");
	CU_ASSERT_FALSE(belle_sip_uri_check_components_from_context(belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(header)),"REGISTER","Contact"));
	CU_ASSERT_TRUE(belle_sip_uri_check_components_from_context(belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(header)),NULL,"Contact"));
	belle_sip_object_unref(header);
	}
	{
	belle_sip_header_record_route_t*	header = belle_sip_header_record_route_parse("Record-Route: <sip:linphone.org;ttl=interdit>");
	CU_ASSERT_FALSE(belle_sip_uri_check_components_from_context(belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(header)),NULL,"Record-Route"));
	belle_sip_object_unref(header);
	}
	{
	belle_sip_uri_t* uri = belle_sip_uri_parse("sip:linphone.org:5061?header=toto");
	CU_ASSERT_TRUE(belle_sip_uri_check_components_from_context(uri,NULL,"Any"));
	belle_sip_object_unref(uri);
	}
}

test_t uri_tests[] = {
	{ "Simple URI", testSIMPLEURI },
	{ "Complex URI", testCOMPLEXURI },
	{ "Escaped username", test_escaped_username },
	{ "Escaped parameter", test_escaped_parameter },
	{ "Escaped passwd", test_escaped_passwd},
	{ "User passwd", test_user_passwd},
	{ "IP host", test_ip_host },
	{ "lr", test_lr },
	{ "maddr", test_maddr },
	{ "headers", test_headers },
	{ "Escaped headers", test_escaped_headers},
	{ "URI parameters", test_uri_parameters },
	{ "SIPS URI", testSIPSURI },
	{ "URI equals", test_uri_equals },
	{ "Simple URI error", testSIMPLEURI_error },
	{ "IPv6 URI", testIPV6URI },
	{ "URI components", testUriComponentsChecker }
};

test_suite_t uri_test_suite = {
	"URI",
	NULL,
	NULL,
	sizeof(uri_tests) / sizeof(uri_tests[0]),
	uri_tests
};

