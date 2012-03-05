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
#include <stdio.h>
#include "CUnit/Basic.h"
#include <belle-sip/belle-sip.h>

extern int belle_sip_uri_test_suite ();
extern int belle_sip_headers_test_suite ();
extern int belle_sip_message_test_suite ();
extern int belle_sdp_test_suite();
extern int belle_sip_authentication_helper_suite ();
extern int belle_sip_cast_test_suite();
extern int belle_sip_register_test_suite();

int main (int argc, char *argv[]) {

	if (argc>1){
		if (strcmp(argv[1],"--help")==0){
				fprintf(stderr,"%s \t--help\n\t\t\t--verbose",argv[0]);
				return 0;
		}else if (strcmp(argv[1],"--verbose")==0){
			belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
		}
	}
	
	/* initialize the CUnit test registry */
	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	belle_sip_uri_test_suite();

	belle_sip_headers_test_suite ();

	belle_sip_message_test_suite();

	belle_sdp_test_suite();
	
	belle_sip_cast_test_suite();

	belle_sip_authentication_helper_suite();

	belle_sip_register_test_suite();
	/* Run all tests using the CUnit Basic interface */
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();
	return CU_get_error();

}
