/*
	belle-sip - SIP (RFC3261) library.
	Copyright (C) 2010-2013  Belledonne Communications SARL

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

#ifndef _BELLE_SIP_TESTER_H
#define _BELLE_SIP_TESTER_H


#include "common/tester_utils.h"


#ifdef __cplusplus
extern "C" {
#endif

extern test_suite_t cast_test_suite;
extern test_suite_t generic_uri_test_suite;
extern test_suite_t sip_uri_test_suite;
extern test_suite_t headers_test_suite;
extern test_suite_t core_test_suite;
extern test_suite_t sdp_test_suite;
extern test_suite_t resolver_test_suite;
extern test_suite_t message_test_suite;
extern test_suite_t authentication_helper_test_suite;
extern test_suite_t register_test_suite;
extern test_suite_t dialog_test_suite;
extern test_suite_t refresher_test_suite;
extern test_suite_t http_test_suite;

extern int belle_sip_tester_ipv6_available(void);
extern void belle_sip_tester_init(void);
extern void belle_sip_tester_uninit(void);
extern const char * belle_sip_tester_get_root_ca_path(void);
extern void belle_sip_tester_set_root_ca_path(const char *root_ca_path);
extern int belle_sip_tester_run_tests(const char *suite_name, const char *test_name);

extern const char* belle_sip_tester_client_cert;
extern const char* belle_sip_tester_client_cert_fingerprint;
extern const char* belle_sip_tester_private_key;
extern const char* belle_sip_tester_private_key_passwd;

#ifdef __cplusplus
};
#endif


#endif /* _BELLE_SIP_TESTER_H */
