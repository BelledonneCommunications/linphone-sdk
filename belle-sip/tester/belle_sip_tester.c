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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "belle_sip_tester.h"

#include <belle-sip/utils.h>

#include <stdio.h>
#include "CUnit/Basic.h"
#include "CUnit/MyMem.h"
#include "CUnit/Automated.h"
#ifdef HAVE_CU_CURSES
#include "CUnit/CUCurses.h"
#endif
#include <belle-sip/belle-sip.h>

#include "port.h"

extern const char *test_domain;
extern const char *auth_domain;

static const char *belle_sip_tester_root_ca_path = NULL;
static FILE * log_file = NULL;
static belle_sip_log_function_t belle_sip_log_handler;

static belle_sip_object_pool_t *pool;

static int _belle_sip_tester_ipv6_available(void){
	struct addrinfo *ai=belle_sip_ip_address_to_addrinfo(AF_INET6,"2a01:e00::2",53);
	if (ai){
		struct sockaddr_storage ss;
		struct addrinfo src;
		socklen_t slen=sizeof(ss);
		char localip[128];
		int port=0;
		belle_sip_get_src_addr_for(ai->ai_addr,ai->ai_addrlen,(struct sockaddr*) &ss,&slen,4444);
		src.ai_addr=(struct sockaddr*) &ss;
		src.ai_addrlen=slen;
		belle_sip_addrinfo_to_ip(&src,localip, sizeof(localip),&port);
		belle_sip_freeaddrinfo(ai);
		return strcmp(localip,"::1")!=0;
	}
	return FALSE;
}

static int ipv6_available=0;

int belle_sip_tester_ipv6_available(void){
	return ipv6_available;
}

static void log_handler(int lev, const char *fmt, va_list args) {
	belle_sip_set_log_file(stderr);
	belle_sip_log_handler(lev, fmt, args);
	if (log_file){
		belle_sip_set_log_file(log_file);
		belle_sip_log_handler(lev, fmt, args);
	}
}

void belle_sip_tester_init() {
	belle_sip_log_handler = belle_sip_get_log_handler();
	belle_sip_set_log_handler(belle_sip_logv_out);

	tester_init(log_handler);
	belle_sip_init_sockets();
	belle_sip_object_enable_marshal_check(TRUE);
	ipv6_available=_belle_sip_tester_ipv6_available();
	tester_add_suite(&cast_test_suite);
	tester_add_suite(&sip_uri_test_suite);
	tester_add_suite(&generic_uri_test_suite);
	tester_add_suite(&headers_test_suite);
	tester_add_suite(&core_test_suite);
	tester_add_suite(&sdp_test_suite);
	tester_add_suite(&resolver_test_suite);
	tester_add_suite(&message_test_suite);
	tester_add_suite(&authentication_helper_test_suite);
	tester_add_suite(&register_test_suite);
	tester_add_suite(&dialog_test_suite);
	tester_add_suite(&refresher_test_suite);
	tester_add_suite(&http_test_suite);
}

const char * belle_sip_tester_get_root_ca_path(void) {
	return belle_sip_tester_root_ca_path;
}

void belle_sip_tester_set_root_ca_path(const char *root_ca_path) {
	belle_sip_tester_root_ca_path = root_ca_path;
}

void belle_sip_tester_uninit(void) {
	belle_sip_object_unref(pool);
	belle_sip_uninit_sockets();
	tester_uninit();
}

static const char* belle_sip_helper =
		"\t\t\t--verbose\n"
		"\t\t\t--silent\n"
		"\t\t\t--log-file <output log file path>\n"
		"\t\t\t--domain <test sip domain>\n"
		"\t\t\t--auth-domain <test auth domain>\n"
		"\t\t\t--root-ca <root ca file path>\n";


#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
int main (int argc, char *argv[]) {
	int i;
	const char *root_ca_path = NULL;
	const char *env_domain=getenv("TEST_DOMAIN");

	belle_sip_tester_init();

	if (env_domain) {
		test_domain=env_domain;
	}

	for(i=1;i<argc;++i){
		if (strcmp(argv[i],"--verbose")==0){
			belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
		} else if (strcmp(argv[i],"--silent")==0){
			belle_sip_set_log_level(BELLE_SIP_LOG_FATAL);
		} else if (strcmp(argv[i],"--log-file")==0){
			CHECK_ARG("--log-file", ++i, argc);
			log_file=fopen(argv[i],"w");
			if (!log_file) {
				belle_sip_error("Cannot open file [%s] for writing logs because [%s]",argv[i],strerror(errno));
				return -2;
			} else {
				belle_sip_message("Redirecting traces to file [%s]",argv[i]);
			}
		} else if (strcmp(argv[i],"--domain")==0){
			CHECK_ARG("--domain", ++i, argc);
			test_domain=argv[i];
		}else if (strcmp(argv[i],"--auth-domain")==0){
			CHECK_ARG("--auth-domain", ++i, argc);
			auth_domain=argv[i];
		} else if (strcmp(argv[i], "--root-ca") == 0) {
			CHECK_ARG("--root-ca", ++i, argc);
			root_ca_path = argv[i];
		}else {
			int ret = tester_parse_args(argc, argv, i);
			if (ret>0) {
				i += ret;
			} else {
				tester_helper(argv[0], belle_sip_helper);
				return ret;
			}
		}
	}
	belle_sip_tester_set_root_ca_path(root_ca_path);
	pool=belle_sip_object_pool_push();

	return tester_start();
}
#endif
