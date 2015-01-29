/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

#include <mediastreamer2/mediastream.h>
#if HAVE_CONFIG_H
#include <mediastreamer-config.h>
#endif

#include <stdio.h>
#include "CUnit/Basic.h"
#include "CUnit/Automated.h"
#if HAVE_CU_CURSES
#include "CUnit/CUCurses.h"
#endif
#ifdef __APPLE__
#include "TargetConditionals.h"
#endif


static test_suite_t **test_suite = NULL;
static int nb_test_suites = 0;
static const char* tester_fileroot = SOUND_FILE_PATH;
static const char* tester_writable_dir= WRITE_FILE_PATH;

static unsigned char xml = 0;
static const char *xml_file = "CUnitAutomated-Results.xml";
static char *xml_tmp_file = NULL;

#if HAVE_CU_CURSES
static unsigned char curses = 0;
#endif

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

	for (i = 0; i < mediastreamer2_tester_nb_test_suites(); i++) {
		if ((strcmp(suite_name, test_suite[i]->name) == 0) && (strlen(suite_name) == strlen(test_suite[i]->name))) {
			return i;
		}
	}

	return -1;
}

#if HAVE_CU_GET_SUITE
static void list_suite_tests(const char *suite_name) {
	int j;
	for( j = 0; j < mediastreamer2_tester_nb_tests(suite_name); j++) {
		const char *test_name = mediastreamer2_tester_test_name(suite_name, j);
		fprintf(stdout, "%s\n", test_name);
	}
}
static void list_suites() {
	int j;
	for(j = 0; j < mediastreamer2_tester_nb_test_suites(); j++) {
		const char *suite_name = mediastreamer2_tester_test_suite_name(j);
		fprintf(stdout, "%s\n", suite_name);
	}
}
#endif


const char* mediastreamer2_tester_get_file_root(){
    return tester_fileroot;
}

void mediastreamer2_tester_set_file_root(const char* fileroot){
    tester_fileroot = fileroot;
}

const char* mediastreamer2_tester_get_writable_dir(){
    return tester_writable_dir;
}
void mediastreamer2_tester_set_writable_dir(const char* writable_dir){
    tester_writable_dir = writable_dir;
}


int mediastreamer2_tester_nb_test_suites(void) {
	return nb_test_suites;
}

int mediastreamer2_tester_nb_tests(const char *suite_name) {
	int i = test_suite_index(suite_name);
	if (i < 0) return 0;
	return test_suite[i]->nb_tests;
}

const char * mediastreamer2_tester_test_suite_name(int suite_index) {
	if (suite_index >= mediastreamer2_tester_nb_test_suites()) return NULL;
	return test_suite[suite_index]->name;
}

const char * mediastreamer2_tester_test_name(const char *suite_name, int test_index) {
	int suite_index = test_suite_index(suite_name);
	if ((suite_index < 0) || (suite_index >= mediastreamer2_tester_nb_test_suites())) return NULL;
	if (test_index >= test_suite[suite_index]->nb_tests) return NULL;
	return test_suite[suite_index]->tests[test_index].name;
}

void mediastreamer2_tester_init(void) {
	int i;

	add_test_suite(&basic_audio_test_suite);
	add_test_suite(&sound_card_test_suite);
	add_test_suite(&adaptive_test_suite);
	add_test_suite(&audio_stream_test_suite);
#ifdef VIDEO_ENABLED
	add_test_suite(&video_stream_test_suite);
#endif
	add_test_suite(&framework_test_suite);
	add_test_suite(&player_test_suite);
#ifdef __ARM_NEON__
    add_test_suite(&neon_test_suite);
#endif
	for (i = 0; i < mediastreamer2_tester_nb_test_suites(); i++) {
		run_test_suite(test_suite[i]);
	}

}

void mediastreamer2_tester_uninit(void) {
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
		ms_warning("Suite [%s], Test [%s] had failures:", pSuite->pName, pTest->pName);
	} else {
		ms_warning(" passed");
	}
	for (i = 1 ; (NULL != pFailure) ; pFailure = pFailure->pNext, i++) {
		ms_warning("\n    %d. %s:%u  - %s", i,
			(NULL != pFailure->strFileName) ? pFailure->strFileName : "",
			pFailure->uiLineNumber,
			(NULL != pFailure->strCondition) ? pFailure->strCondition : "");
	}
}

static void test_all_tests_complete_message_handler(const CU_pFailureRecord pFailure) {
	ms_warning("\n\n %s",CU_get_run_results_string());
}

static void test_suite_init_failure_message_handler(const CU_pSuite pSuite) {
	ms_warning("Suite initialization failed for [%s].", pSuite->pName);
}

static void test_suite_cleanup_failure_message_handler(const CU_pSuite pSuite) {
	ms_warning("Suite cleanup failed for '%s'.", pSuite->pName);
}

static void test_start_message_handler(const CU_pTest pTest, const CU_pSuite pSuite) {
	ms_warning("Suite [%s] Test [%s]", pSuite->pName,pTest->pName);
}
static void test_suite_start_message_handler(const CU_pSuite pSuite) {
	ms_warning("Suite [%s]", pSuite->pName);
}

int mediastreamer2_tester_run_tests(const char *suite_name, const char *test_name) {
	int ret;

	CU_set_test_start_handler(test_start_message_handler);
	CU_set_test_complete_handler(test_complete_message_handler);
	CU_set_all_test_complete_handler(test_all_tests_complete_message_handler);
	CU_set_suite_init_failure_handler(test_suite_init_failure_message_handler);
	CU_set_suite_cleanup_failure_handler(test_suite_cleanup_failure_message_handler);
	CU_set_suite_start_handler(test_suite_start_message_handler);

	if( xml ){
		xml_tmp_file = ms_strdup_printf("%s.tmp", xml_file);
		CU_set_output_filename(xml_tmp_file);
		CU_automated_run_tests();
	} else {
#if HAVE_CU_GET_SUITE
		if (suite_name){
			CU_pSuite suite;
			CU_basic_set_mode(CU_BRM_VERBOSE);
			suite=CU_get_suite(suite_name);
			if (!suite) {
				fprintf(stderr, "Could not find suite '%s'. Available suites are:\n", suite_name);
				list_suites();
			} else if (test_name) {
				CU_pTest test=CU_get_test_by_name(test_name, suite);
				if (!test) {
					fprintf(stderr, "Could not find test '%s' in suite '%s'. Available tests are:\n", test_name, suite_name);
					// do not use suite_name here, since this method is case sentisitive
					list_suite_tests(suite->pName);
				} else {
					CU_ErrorCode err= CU_basic_run_test(suite, test);
					if (err != CUE_SUCCESS) fprintf(stderr, "CU_basic_run_test error=%d\n", err);
				}
			} else {
				CU_basic_run_suite(suite);
			}
		} else
#endif
		{
#if HAVE_CU_CURSES
			if (curses) {
				/* Run tests using the CUnit curses interface */
				CU_curses_run_tests();
			}
			else
#endif
			{
				/* Run all tests using the CUnit Basic interface */
				CU_basic_set_mode(CU_BRM_VERBOSE);
				CU_basic_run_tests();
			}
		}
	}
	/* Redisplay list of failed tests on end */
	if (CU_get_number_of_failure_records()){
		CU_basic_show_failures(CU_get_failure_list());
		printf("\n");
	}

	ret=CU_get_number_of_tests_failed()!=0;
	return ret;
}

void helper(const char *name) {
	fprintf(stderr, "%s \t--help\n"
#ifndef _WIN32
		"\t\t\t--verbose\n"
#endif
		"\t\t\t--silent\n"
#if HAVE_CU_GET_SUITE
		"\t\t\t--list-suites\n"
		"\t\t\t--list-tests <suite>\n"
		"\t\t\t--suite <suite name>\n"
		"\t\t\t--test <test name>\n"
#endif
#if HAVE_CU_CURSES
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
#if TARGET_OS_MAC || TARGET_OS_IPHONE
int apple_main (int argc, char *argv[]) {
#else
int main (int argc, char *argv[]) {
#endif
	int i;
	int ret;
	const char *suite_name = NULL;
	const char *test_name = NULL;
	unsigned char verbose = FALSE;

	/* initialize the CUnit test registry */
	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	mediastreamer2_tester_init();

	for(i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--help") == 0) {
			helper(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "--verbose") == 0) {
			verbose = TRUE;
		} else if (strcmp(argv[i], "--silent") == 0) {
			verbose = FALSE;
		} else if (strcmp(argv[i], "--xml-file") == 0){
			CHECK_ARG("--xml-file", ++i, argc);
			xml_file = argv[i];
			xml = 1;
		} else if (strcmp(argv[i], "--xml") == 0){
			xml = 1;
		}
#if HAVE_CU_GET_SUITE
		else if (strcmp(argv[i], "--test")==0) {
			CHECK_ARG("--test", ++i, argc);
			test_name = argv[i];
		} else if (strcmp(argv[i], "--suite") == 0) {
			CHECK_ARG("--suite", ++i, argc);
			suite_name = argv[i];
		}  else if (strcmp(argv[i],"--list-suites")==0){
			list_suites();
			return 0;
		} else if (strcmp(argv[i],"--list-tests")==0){
			CHECK_ARG("--list-tests", ++i, argc);
			suite_name = argv[i];
			if (CU_get_suite(suite_name)==NULL){
				fprintf(stderr, "Could not find suite '%s'. Available suites are:\n", suite_name);
				list_suites();
				return -1;
			}else{
				list_suite_tests(suite_name);
				return 0;
			}
		}
#endif
#if HAVE_CU_CURSES
		else if (strcmp(argv[i], "--curses") == 0) {
			curses = 1;
		}
#endif
		else {
			fprintf(stderr, "Unknown option \"%s\"\n", argv[i]); \

			helper(argv[0]);
			return -1;
		}
	}


	if (verbose) {
		putenv("MEDIASTREAMER_DEBUG=1");
	} else {
		putenv("MEDIASTREAMER_DEBUG=0");
	}

#ifdef HAVE_CU_CURSES
	if( xml && curses ){
		printf("Cannot use both xml and curses\n");
		return -1;
	}
#endif

	if( xml && (suite_name || test_name) ){
		printf("Cannot use both xml and specific test suite\n");
		return -1;
	}

	ret = mediastreamer2_tester_run_tests(suite_name, test_name);
	CU_cleanup_registry();
	mediastreamer2_tester_uninit();

	if ( xml ) {
		/*create real xml file only if tester did not crash*/
		ms_strcat_printf(xml_tmp_file, "-Results.xml");
		rename(xml_tmp_file, xml_file);
		ms_free(xml_tmp_file);
	}

	return ret;
}
#endif
