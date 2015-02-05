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
static test_suite_t **test_suite = NULL;
static int nb_test_suites = 0;
static int belle_sip_tester_use_log_file=0;

#ifdef ANDROID
const char *belle_sip_tester_writable_dir_prefix = "/data/data/org.linphone.tester/cache";
#else
const char *belle_sip_tester_writable_dir_prefix = ".";
#endif

#ifdef HAVE_CU_CURSES
	static unsigned char curses = 0;
#endif

static const char *belle_sip_tester_xml_file = "CUnitAutomated-Results.xml";
static char *belle_sip_tester_xml_tmp_file = NULL;
static unsigned char belle_sip_tester_xml_enabled = FALSE;


static void add_test_suite(test_suite_t *suite) {
	if (test_suite == NULL) {
		test_suite = (test_suite_t **)malloc(10 * sizeof(test_suite_t *));
	}
	test_suite[nb_test_suites] = suite;
	nb_test_suites++;
	if ((nb_test_suites % 10) == 0) {
		test_suite = (test_suite_t **)realloc(test_suite, (nb_test_suites + 10) * sizeof(test_suite_t *));
	}
}

static int run_test_suite(test_suite_t *suite) {
	int i;

	CU_pSuite pSuite = CU_add_suite(suite->name, suite->init_func, suite->cleanup_func);

	for (i = 0; i < suite->nb_tests; i++) {
		if (NULL == CU_add_test(pSuite, suite->tests[i].name, suite->tests[i].func)) {
			return CU_get_error();
		}
	}

	return 0;
}

static int test_suite_index(const char *suite_name) {
	int i;

	for (i = 0; i < belle_sip_tester_nb_test_suites(); i++) {
		if ((strcmp(suite_name, test_suite[i]->name) == 0) && (strlen(suite_name) == strlen(test_suite[i]->name))) {
			return i;
		}
	}

	return -1;
}

int belle_sip_tester_nb_test_suites(void) {
	return nb_test_suites;
}

int belle_sip_tester_nb_tests(const char *suite_name) {
	int i = test_suite_index(suite_name);
	if (i < 0) return 0;
	return test_suite[i]->nb_tests;
}

const char * belle_sip_tester_test_suite_name(int suite_index) {
	if (suite_index >= belle_sip_tester_nb_test_suites()) return NULL;
	return test_suite[suite_index]->name;
}

const char * belle_sip_tester_test_name(const char *suite_name, int test_index) {
	int suite_index = test_suite_index(suite_name);
	if ((suite_index < 0) || (suite_index >= belle_sip_tester_nb_test_suites())) return NULL;
	if (test_index >= test_suite[suite_index]->nb_tests) return NULL;
	return test_suite[suite_index]->tests[test_index].name;
}

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

void belle_sip_tester_init() {
	belle_sip_init_sockets();
	belle_sip_object_enable_marshal_check(TRUE);
	ipv6_available=_belle_sip_tester_ipv6_available();
	add_test_suite(&cast_test_suite);
	add_test_suite(&sip_uri_test_suite);
	add_test_suite(&generic_uri_test_suite);
	add_test_suite(&headers_test_suite);
	add_test_suite(&core_test_suite);
	add_test_suite(&sdp_test_suite);
	add_test_suite(&resolver_test_suite);
	add_test_suite(&message_test_suite);
	add_test_suite(&authentication_helper_test_suite);
	add_test_suite(&register_test_suite);
	add_test_suite(&dialog_test_suite);
	add_test_suite(&refresher_test_suite);
	add_test_suite(&http_test_suite);
}

const char * belle_sip_tester_get_root_ca_path(void) {
	return belle_sip_tester_root_ca_path;
}

void belle_sip_tester_set_root_ca_path(const char *root_ca_path) {
	belle_sip_tester_root_ca_path = root_ca_path;
}

void belle_sip_tester_uninit(void) {
	belle_sip_uninit_sockets();
	if (test_suite != NULL) {
		free(test_suite);
		test_suite = NULL;
		nb_test_suites = 0;
	}
}

/*derivated from cunit*/
static void test_complete_message_handler(const CU_pTest pTest,
												const CU_pSuite pSuite,
												const CU_pFailureRecord pFailureList) {
	int i;
	CU_pFailureRecord pFailure = pFailureList;
	if (pFailure) {
		if (belle_sip_tester_use_log_file) belle_sip_warning("Suite [%s], Test [%s] had failures:", pSuite->pName, pTest->pName);
		printf("\nSuite [%s], Test [%s] had failures:", pSuite->pName, pTest->pName);
	} else {
		if (belle_sip_tester_use_log_file) belle_sip_warning(" passed");
		printf(" passed");
	}
	  for (i = 1 ; (NULL != pFailure) ; pFailure = pFailure->pNext, i++) {
		  if (belle_sip_tester_use_log_file) belle_sip_warning("\n    %d. %s:%u  - %s", i,
			(NULL != pFailure->strFileName) ? pFailure->strFileName : "",
			pFailure->uiLineNumber,
			(NULL != pFailure->strCondition) ? pFailure->strCondition : "");
		  printf("\n    %d. %s:%u  - %s", i,
			(NULL != pFailure->strFileName) ? pFailure->strFileName : "",
			pFailure->uiLineNumber,
			(NULL != pFailure->strCondition) ? pFailure->strCondition : "");
	  }
 }


static void test_all_tests_complete_message_handler(const CU_pFailureRecord pFailure) {
  char *result_string;
  if (belle_sip_tester_use_log_file) belle_sip_warning("\n\n %s",CU_get_run_results_string());
  result_string = CU_get_run_results_string();
  if (result_string != NULL) {
	  printf("\n\n %s",result_string);
	  CU_FREE(result_string);
  }
}

static void test_suite_init_failure_message_handler(const CU_pSuite pSuite) {
	if (belle_sip_tester_use_log_file) belle_sip_warning("Suite initialization failed for [%s].", pSuite->pName);
	printf("Suite initialization failed for [%s].", pSuite->pName);
}

static void test_suite_cleanup_failure_message_handler(const CU_pSuite pSuite) {
	if (belle_sip_tester_use_log_file) belle_sip_warning("Suite cleanup failed for '%s'.", pSuite->pName);
	printf("Suite cleanup failed for [%s].", pSuite->pName);
}

static void test_start_message_handler(const CU_pTest pTest, const CU_pSuite pSuite) {
	if (belle_sip_tester_use_log_file) belle_sip_warning("Suite [%s] Test [%s]", pSuite->pName,pTest->pName);
	printf("\nSuite [%s] Test [%s]", pSuite->pName,pTest->pName);
	fflush(stdout);
}
static void test_suite_start_message_handler(const CU_pSuite pSuite) {
	if (belle_sip_tester_use_log_file) belle_sip_warning("Suite [%s]", pSuite->pName);
	printf("\nSuite [%s]", pSuite->pName);
	fflush(stdout);
}
int belle_sip_tester_run_tests(const char *suite_name, const char *test_name) {
	int i,ret;
	belle_sip_object_pool_t *pool;

	/* initialize the CUnit test registry */
	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	for (i = 0; i < belle_sip_tester_nb_test_suites(); i++) {
		run_test_suite(test_suite[i]);
	}
	pool=belle_sip_object_pool_push();
	CU_set_test_start_handler(test_start_message_handler);
	CU_set_test_complete_handler(test_complete_message_handler);
	CU_set_all_test_complete_handler(test_all_tests_complete_message_handler);
	CU_set_suite_init_failure_handler(test_suite_init_failure_message_handler);
	CU_set_suite_cleanup_failure_handler(test_suite_cleanup_failure_message_handler);
	CU_set_suite_start_handler(test_suite_start_message_handler);

	if(belle_sip_tester_xml_enabled) {
		belle_sip_tester_xml_tmp_file = belle_sip_strdup_printf("%s.tmp", belle_sip_tester_xml_file);
		CU_set_output_filename(belle_sip_tester_xml_tmp_file);
		CU_automated_run_tests();
	} else {
#ifdef HAVE_CU_GET_SUITE
		if (suite_name){
			CU_pSuite suite;
			suite=CU_get_suite(suite_name);
			if (suite==NULL){
				fprintf(stderr,"There is no suite named '%s'",suite_name);
				exit(-1);
			}
			if (test_name) {
				CU_pTest test=CU_get_test_by_name(test_name, suite);
				if (test==NULL){
					fprintf(stderr,"There is no test named '%s'",test_name);
					exit(-1);
				}
				CU_run_test(suite, test);
			} else
				CU_run_suite(suite);
		} else
#endif
		{
#ifdef HAVE_CU_CURSES
			if (curses) {
				/* Run tests using the CUnit curses interface */
				CU_curses_run_tests();
			}
			else
#endif
			{
				CU_run_all_tests();
			}
		}
	}

	belle_sip_object_unref(pool);
	ret=CU_get_number_of_tests_failed()!=0;
	CU_cleanup_registry();
	return ret;
}


void helper(const char *name) {
	fprintf(stderr,"%s \t--help\n"
		"\t\t\t--verbose\n"
		"\t\t\t--domain <test sip domain>\n"
		"\t\t\t--auth-domain <test auth domain>\n"
		"\t\t\t--root-ca <root ca file path>\n"
		"\t\t\t--log-file <output log file path>\n"
#ifdef HAVE_CU_GET_SUITE
		"\t\t\t--list-suites\n"
		"\t\t\t--list-tests <suite>\n"
		"\t\t\t--suite <suite name>\n"
		"\t\t\t--test <test name>\n"
#endif
#ifdef HAVE_CU_CURSES
		"\t\t\t--curses\n"
#endif
		"\t\t\t--xml\n"
		"\t\t\t--xml-file <xml file prefix (will be suffixed by '-Results.xml')>\n"
		, name);
}

#define CHECK_ARG(argument, index, argc) \
	if (index >= argc) { \
		fprintf(stderr, "Missing argument for \"%s\"\n", argument); \
		return -1; \
	}

#ifndef WINAPI_FAMILY_PHONE_APP

int main (int argc, char *argv[]) {
	int i;
	int ret;
	const char *root_ca_path = NULL;
	const char *suite_name=NULL;
	const char *test_name=NULL;
	const char *env_domain=getenv("TEST_DOMAIN");
	FILE* log_file=NULL;

	belle_sip_tester_init();

	if (env_domain)
		test_domain=env_domain;

	for(i=1;i<argc;++i){
		if (strcmp(argv[i],"--help")==0){
			helper(argv[0]);
			return 0;
		}else if (strcmp(argv[i],"--verbose")==0){
			belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
		}else if (strcmp(argv[i],"--log-file")==0){
			CHECK_ARG("--log-file", ++i, argc);
			log_file=fopen(argv[i],"w");
			if (!log_file) {
				belle_sip_fatal("Cannot open file [%s] for writting logs because [%s]",argv[i],strerror(errno));
			} else {
				belle_sip_tester_use_log_file=1;
				printf ("Redirecting traces to file [%s]",argv[i]);
				belle_sip_set_log_file(log_file);
			}
		}else if (strcmp(argv[i],"--domain")==0){
			CHECK_ARG("--domain", ++i, argc);
			test_domain=argv[i];
		}else if (strcmp(argv[i],"--auth-domain")==0){
			CHECK_ARG("--auth-domain", ++i, argc);
			auth_domain=argv[i];
		} else if (strcmp(argv[i], "--root-ca") == 0) {
			CHECK_ARG("--root-ca", ++i, argc);
			root_ca_path = argv[i];
		} else if (strcmp(argv[i], "--xml-file") == 0){
			CHECK_ARG("--xml-file", ++i, argc);
			belle_sip_tester_xml_file = argv[i];
			belle_sip_tester_xml_enabled = 1;
		} else if (strcmp(argv[i], "--xml") == 0){
			belle_sip_tester_xml_enabled = 1;
		}
#ifdef HAVE_CU_GET_SUITE
		else if (strcmp(argv[i],"--list-suites")==0){
			int j;
			for(j = 0; j < belle_sip_tester_nb_test_suites(); j++) {
				suite_name = belle_sip_tester_test_suite_name(j);
				fprintf(stdout, "%s\n", suite_name);
			}
			return 0;
		} else if (strcmp(argv[i],"--list-tests")==0){
			int j;
			CHECK_ARG("--list-tests", ++i, argc);
			suite_name = argv[i];
			for(j = 0; j < belle_sip_tester_nb_tests(suite_name);j++) {
				test_name = belle_sip_tester_test_name(suite_name, j);
				fprintf(stdout, "%s\n", test_name);
			}
			return 0;
		} else if (strcmp(argv[i],"--test")==0){
			CHECK_ARG("--test", ++i, argc);
			test_name=argv[i];
		}else if (strcmp(argv[i],"--suite")==0){
			CHECK_ARG("--suite", ++i, argc);
			suite_name=argv[i];
		}
#endif
#ifdef HAVE_CU_CURSES
		else if (strcmp(argv[i], "--curses") == 0) {
			curses = 1;
		}
#endif
		else {
			helper(argv[0]);
			return -1;
		}
	}

#ifdef HAVE_CU_CURSES
	if( belle_sip_tester_xml_enabled && curses ){
		printf("Cannot use both xml and curses\n");
		return -1;
	}
#endif

	if( belle_sip_tester_xml_enabled && (suite_name || test_name) ){
		printf("Cannot use both xml and specific test suite\n");
		return -1;
	}

	belle_sip_tester_set_root_ca_path(root_ca_path);
	ret = belle_sip_tester_run_tests(suite_name, test_name);
	belle_sip_tester_uninit();
	if (log_file) fclose(log_file);

	if ( belle_sip_tester_xml_enabled ) {
		/*create real xml file only if tester did not crash*/
		char * real_name = belle_sip_strdup_printf("%s%s", belle_sip_tester_xml_tmp_file, "-Results.xml");
		rename(real_name, belle_sip_tester_xml_file);
		belle_sip_free(real_name);
		belle_sip_free(belle_sip_tester_xml_tmp_file);
	}

	return ret;
}
#endif
