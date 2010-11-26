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
	belle_sip_header_contact_delete(L_contact);
}

void test_complex_header_contact_with(void) {

	belle_sip_header_contact_t* L_contact = belle_sip_header_contact_parse("Contact: \"jéremis\" <sip:titi.com>;expires=3600; q=0.7");
	belle_sip_uri_t* L_uri = belle_sip_header_address_get_uri((belle_sip_header_address_t*)L_contact);

	CU_ASSERT_PTR_NOT_NULL(L_uri);
	CU_ASSERT_STRING_EQUAL(belle_sip_uri_get_host(L_uri), "titi.com");

	CU_ASSERT_STRING_EQUAL(belle_sip_header_address_get_displayname((belle_sip_header_address_t*)L_contact), "jéremis");

	CU_ASSERT_EQUAL(belle_sip_header_contact_get_expires(L_contact),3600);
	CU_ASSERT_EQUAL(belle_sip_header_contact_get_qvalue(L_contact),0.7);

	belle_sip_header_contact_delete(L_contact);
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

}
