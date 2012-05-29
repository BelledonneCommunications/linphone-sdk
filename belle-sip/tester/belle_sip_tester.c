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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "CUnit/Basic.h"
#include <belle-sip/belle-sip.h>

extern const char *test_domain;
extern int belle_sip_uri_test_suite ();
extern int belle_sip_headers_test_suite ();
extern int belle_sip_message_test_suite ();
extern int belle_sdp_test_suite();
extern int belle_sip_authentication_helper_suite ();
extern int belle_sip_cast_test_suite();
extern int belle_sip_register_test_suite();
extern int belle_sip_dialog_test_suite();

int main (int argc, char *argv[]) {
	int i;
	char *suite_name=NULL;
	const char *env_domain=getenv("TEST_DOMAIN");
	if (env_domain)
		test_domain=env_domain;
	
	for(i=1;i<argc;++i){
		if (strcmp(argv[i],"--help")==0){
				fprintf(stderr,"%s \t--help\n\t\t\t--verbose",argv[0]);
				return 0;
		}else if (strcmp(argv[i],"--verbose")==0){
			belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
		}else if (strcmp(argv[i],"--domain")==0){
			i++;
			test_domain=argv[i];
		}else if (strcmp(argv[i],"--suite")==0){
			i++;
			suite_name=argv[i];
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

	belle_sip_dialog_test_suite ();

	/* Run all tests using the CUnit Basic interface */
	CU_basic_set_mode(CU_BRM_VERBOSE);


	if (suite_name){
#ifdef HAVE_CU_GET_SUITE
		CU_pSuite suite;
		suite=CU_get_suite(suite_name);
		CU_basic_run_suite(suite);
#else
	fprintf(stderr,"Your CUnit version does not support suite selection.\n");
#endif
	} else
		CU_basic_run_tests();

	CU_cleanup_registry();
	return CU_get_error();

}
