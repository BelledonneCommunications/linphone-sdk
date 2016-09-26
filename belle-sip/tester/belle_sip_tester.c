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
#include <belle-sip/belle-sip.h>

#include "port.h"

extern const char *test_domain;
extern const char *auth_domain;

static char* all_leaks_buffer = NULL;
static const char *belle_sip_tester_root_ca_path = NULL;
static FILE * log_file = NULL;

static belle_sip_object_pool_t *pool;

static int leaked_objects_count;

static int _belle_sip_tester_ipv6_available(void){
	struct addrinfo *ai=bctbx_ip_address_to_addrinfo(AF_INET6,SOCK_STREAM,"2a01:e00::2",53);
	if (ai){
		struct sockaddr_storage ss;
		struct addrinfo src;
		socklen_t slen=sizeof(ss);
		char localip[128];
		int port=0;
		belle_sip_get_src_addr_for(ai->ai_addr,(socklen_t)ai->ai_addrlen,(struct sockaddr*) &ss,&slen,4444);
		src.ai_addr=(struct sockaddr*) &ss;
		src.ai_addrlen=slen;
		bctbx_addrinfo_to_ip_address(&src,localip, sizeof(localip),&port);
		bctbx_freeaddrinfo(ai);
		return strcmp(localip,"::1")!=0;
	}
	return FALSE;
}

static int ipv6_available=0;

int belle_sip_tester_ipv6_available(void){
	return ipv6_available;
}

const char * belle_sip_tester_get_root_ca_path(void) {
	return belle_sip_tester_root_ca_path;
}

void belle_sip_tester_set_root_ca_path(const char *root_ca_path) {
	belle_sip_tester_root_ca_path = root_ca_path;
}


static void log_handler(int lev, const char *fmt, va_list args) {
#ifdef _WIN32
	/* We must use stdio to avoid log formatting (for autocompletion etc.) */
	vfprintf(lev == BELLE_SIP_LOG_ERROR ? stderr : stdout, fmt, args);
	fprintf(lev == BELLE_SIP_LOG_ERROR ? stderr : stdout, "\n");
#else
	va_list cap;
	va_copy(cap,args);
	vfprintf(lev == BELLE_SIP_LOG_ERROR ? stderr : stdout, fmt, cap);
	fprintf(lev == BELLE_SIP_LOG_ERROR ? stderr : stdout, "\n");
	va_end(cap);
#endif
	if (log_file){
		belle_sip_logv(BELLE_SIP_LOG_DOMAIN,lev, fmt, args);
	}
}

void belle_sip_tester_init(void(*ftester_printf)(int level, const char *fmt, va_list args)) {
	if (ftester_printf == NULL) ftester_printf = log_handler;
	bc_tester_init(ftester_printf, BELLE_SIP_LOG_MESSAGE, BELLE_SIP_LOG_ERROR, NULL);
	belle_sip_init_sockets();
	belle_sip_object_enable_marshal_check(TRUE);
	ipv6_available=_belle_sip_tester_ipv6_available();
	bc_tester_add_suite(&cast_test_suite);
	bc_tester_add_suite(&sip_uri_test_suite);
	bc_tester_add_suite(&generic_uri_test_suite);
	bc_tester_add_suite(&headers_test_suite);
	bc_tester_add_suite(&core_test_suite);
	bc_tester_add_suite(&sdp_test_suite);
	bc_tester_add_suite(&resolver_test_suite);
	bc_tester_add_suite(&message_test_suite);
	bc_tester_add_suite(&authentication_helper_test_suite);
	bc_tester_add_suite(&register_test_suite);
	bc_tester_add_suite(&dialog_test_suite);
	bc_tester_add_suite(&refresher_test_suite);
	bc_tester_add_suite(&http_test_suite);
}

void belle_sip_tester_uninit(void) {
	belle_sip_object_unref(pool);
	belle_sip_uninit_sockets();

	// show all leaks that happened during the test
	if (all_leaks_buffer) {
		bc_tester_printf(BELLE_SIP_LOG_MESSAGE, all_leaks_buffer);
		belle_sip_free(all_leaks_buffer);
	}

	bc_tester_uninit();
}

void belle_sip_tester_before_each(void) {
	belle_sip_object_enable_leak_detector(TRUE);
	leaked_objects_count = belle_sip_object_get_object_count();
}

void belle_sip_tester_after_each(void) {
	int leaked_objects = belle_sip_object_get_object_count() - leaked_objects_count;
	if (leaked_objects > 0) {
		char* format = belle_sip_strdup_printf("%d object%s leaked in suite [%s] test [%s], please fix that!",
										leaked_objects, leaked_objects>1?"s were":"was",
										bc_tester_current_suite_name(), bc_tester_current_test_name());
		belle_sip_object_dump_active_objects();
		belle_sip_object_flush_active_objects();
		bc_tester_printf(BELLE_SIP_LOG_MESSAGE, format);
		belle_sip_error("%s", format);

		all_leaks_buffer = all_leaks_buffer ? belle_sip_strcat_printf(all_leaks_buffer, "\n%s", format) : belle_sip_strdup_printf("\n%s", format);
	}

	// prevent any future leaks
	{
		const char **tags = bc_tester_current_test_tags();
		int leaks_expected =
			(tags && ((tags[0] && !strcmp(tags[0], "LeaksMemory")) || (tags[1] && !strcmp(tags[1], "LeaksMemory"))));
		// if the test is NOT marked as leaking memory and it actually is, we should make it fail
		if (!leaks_expected && leaked_objects > 0) {
			BC_FAIL("This test is leaking memory!");
			// and reciprocally
		} else if (leaks_expected && leaked_objects == 0) {
			BC_FAIL("This test is not leaking anymore, please remove LeaksMemory tag!");
		}
	}
}

int belle_sip_tester_set_log_file(const char *filename) {
	if (log_file) {
		fclose(log_file);
	}
	log_file = fopen(filename, "w");
	if (!log_file) {
		belle_sip_error("Cannot open file [%s] for writing logs because [%s]", filename, strerror(errno));
		return -1;
	}
	belle_sip_message("Redirecting traces to file [%s]", filename);
	belle_sip_set_log_file(log_file);
	return 0;
}


#if !defined(ANDROID) && !defined(TARGET_OS_IPHONE) && !(defined(BELLE_SIP_WINDOWS_PHONE) || defined(BELLE_SIP_WINDOWS_UNIVERSAL))

static const char* belle_sip_helper =
		"\t\t\t--verbose\n"
		"\t\t\t--silent\n"
		"\t\t\t--log-file <output log file path>\n"
		"\t\t\t--domain <test sip domain>\n"
		"\t\t\t--auth-domain <test auth domain>\n"
		"\t\t\t--root-ca <root ca file path>\n";

int main (int argc, char *argv[]) {
	int i;
	int ret;
	const char *root_ca_path = NULL;
	const char *env_domain=getenv("TEST_DOMAIN");

	belle_sip_tester_init(NULL);

#ifndef _WIN32   /*this hack doesn't work for argv[0]="c:\blablab\"*/
	// this allows to launch liblinphone_tester from outside of tester directory
	if (strstr(argv[0], ".libs")) {
		int prefix_length = strstr(argv[0], ".libs") - argv[0] + 1;
		char *prefix = belle_sip_strdup_printf("%s%.*s", argv[0][0] == '/' ? "" : "./", prefix_length, argv[0]);
		// printf("Resource prefix set to %s\n", prefix);
		bc_tester_set_resource_dir_prefix(prefix);
		bc_tester_set_writable_dir_prefix(prefix);
		belle_sip_free(prefix);
	}
#endif

	if (env_domain) {
		test_domain=env_domain;
	}

	for(i=1;i<argc;++i){
		if (strcmp(argv[i],"--verbose")==0){
			belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
			bctbx_set_log_level(BCTBX_LOG_DOMAIN,BCTBX_LOG_DEBUG);
		} else if (strcmp(argv[i],"--silent")==0){
			belle_sip_set_log_level(BELLE_SIP_LOG_FATAL);
			bctbx_set_log_level(BCTBX_LOG_DOMAIN,BCTBX_LOG_FATAL);
		} else if (strcmp(argv[i],"--log-file")==0){
			CHECK_ARG("--log-file", ++i, argc);
			if (belle_sip_tester_set_log_file(argv[i]) < 0) return -2;
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
			int ret = bc_tester_parse_args(argc, argv, i);
			if (ret>0) {
				i += ret - 1;
				continue;
			} else if (ret<0) {
				bc_tester_helper(argv[0], belle_sip_helper);
			}
			return ret;
		}
	}
	belle_sip_tester_set_root_ca_path(root_ca_path);
	pool=belle_sip_object_pool_push();

	ret = bc_tester_start(argv[0]);
	belle_sip_tester_uninit();
	return ret;
}

#endif
