/*
tester - liblinphone test suite
Copyright (C) 2013  Belledonne Communications SARL

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

#include <bctoolbox/tester.h>

#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#ifndef IN_BCUNIT_SOURCES
#include <BCUnit/Basic.h>
#include <BCUnit/Automated.h>
#include <BCUnit/MyMem.h>
#include <BCUnit/Util.h>
#else
#include "Basic.h"
#include "Automated.h"
#include "MyMem.h"
#include "Util.h"
#endif

#ifdef _WIN32
#if defined(__MINGW32__) || !defined(WINAPI_FAMILY_PARTITION) || !defined(WINAPI_PARTITION_DESKTOP)
#define BC_TESTER_WINDOWS_DESKTOP 1
#elif defined(WINAPI_FAMILY_PARTITION)
#if defined(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define BC_TESTER_WINDOWS_DESKTOP 1
#elif defined(WINAPI_PARTITION_PHONE_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
#define BC_TESTER_WINDOWS_PHONE 1
#elif defined(WINAPI_PARTITION_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#define BC_TESTER_WINDOWS_UNIVERSAL 1
#endif
#endif
#endif

#ifdef __linux
/*for monitoring total space allocated via malloc*/
#include <malloc.h>
#endif

#ifndef F_OK
#define F_OK 00 /* Visual Studio does not define F_OK */
#endif

#ifdef HAVE_BCUNIT_CUCURSES_H
#define HAVE_CU_CURSES 1
#endif

static char *bc_tester_resource_dir_prefix = NULL;
// by default writable will always write near the executable
static char *bc_tester_writable_dir_prefix = NULL;

static char *bc_current_suite_name = NULL;
static char *bc_current_test_name = NULL;

int bc_printf_verbosity_info;
int bc_printf_verbosity_error;

static test_suite_t **test_suite = NULL;
static int nb_test_suites = 0;

#ifdef HAVE_CU_CURSES
#include "BCUnit/CUCurses.h"
static unsigned char curses = 0;
#endif

char* xml_file = "BCUnitAutomated-Results.xml";
int   xml_enabled = 0;
char * suite_name = NULL;
char * test_name = NULL;
char * tag_name = NULL;
char * expected_res = NULL;
static long max_vm_kb = 0;
int run_skipped_tests = 0;

static void (*tester_printf_va)(int level, const char *format, va_list args)=NULL;

void bc_tester_printf(int level, const char *format, ...) {
	va_list args;
	va_start (args, format);
	if (tester_printf_va) {
		tester_printf_va(level, format, args);
	}else{
		vfprintf(stderr, format, args);
	}
	va_end (args);
}

int bc_tester_run_suite(test_suite_t *suite, const char *tag_name) {
	int i;
	CU_pSuite pSuite;

	if (tag_name != NULL) {
		size_t j;
		int nb_tests_for_tag = 0;
		for (i = 0; i < suite->nb_tests; i++) {
			for (j = 0; j < (sizeof(suite->tests[i].tags) / sizeof(suite->tests[i].tags[0])); j++) {
				if ((suite->tests[i].tags[j] != NULL) && (strcasecmp(tag_name, suite->tests[i].tags[j]) == 0)) {
					nb_tests_for_tag++;
				}
			}
		}
		if (nb_tests_for_tag > 0) {
			pSuite = CU_add_suite_with_setup_and_teardown(suite->name, suite->before_all, suite->after_all,
												suite->before_each, suite->after_each);
			for (i = 0; i < suite->nb_tests; i++) {
				for (j = 0; j < (sizeof(suite->tests[i].tags) / sizeof(suite->tests[i].tags[0])); j++) {
					if ((suite->tests[i].tags[j] != NULL) && (strcasecmp(tag_name, suite->tests[i].tags[j]) == 0)) {
						if (NULL == CU_add_test(pSuite, suite->tests[i].name, suite->tests[i].func)) {
							return CU_get_error();
						}
					}
				}
			}
		}
	} else {
		pSuite = CU_add_suite_with_setup_and_teardown(suite->name, suite->before_all, suite->after_all,
												suite->before_each, suite->after_each);
		for (i = 0; i < suite->nb_tests; i++) {
			size_t j;
			bool_t skip = FALSE;
			for (j = 0; j < (sizeof(suite->tests[i].tags) / sizeof(suite->tests[i].tags[0])); j++) {
				if ((suite->tests[i].tags[j] != NULL) && (strcasecmp("Skip", suite->tests[i].tags[j]) == 0) && (run_skipped_tests == 0)) {
					skip = TRUE;
				}
			}
			if (!skip) {
				if (NULL == CU_add_test(pSuite, suite->tests[i].name, suite->tests[i].func)) {
					return CU_get_error();
				}
			}
		}
	}

	return 0;
}

const char * bc_tester_suite_name(int suite_index) {
	if (suite_index >= nb_test_suites) return NULL;
	return test_suite[suite_index]->name;
}

int bc_tester_suite_index(const char *suite_name) {
	int i;

	for (i = 0; i < nb_test_suites; i++) {
		if (strcmp(suite_name, test_suite[i]->name) == 0) {
			return i;
		}
	}

	return -1;
}


int bc_tester_test_index(test_suite_t *suite, const char *test_name) {
	int i;

	for (i = 0; i < suite->nb_tests; i++) {
		if (strcmp(test_name, suite->tests[i].name) == 0) {
			return i;
		}
	}

	return -1;
}

int bc_tester_nb_suites(void) {
	return nb_test_suites;
}

const char * bc_tester_test_name(const char *suite_name, int test_index) {
	test_suite_t *suite = NULL;
	size_t j = 0;
	bool_t skip = FALSE;
	
	int suite_index = bc_tester_suite_index(suite_name);
	if ((suite_index < 0) || (suite_index >= nb_test_suites)) return NULL;
	suite = test_suite[suite_index];
	if (test_index >= suite->nb_tests) return NULL;
	
	for (j = 0; j < (sizeof(suite->tests[test_index].tags) / sizeof(suite->tests[test_index].tags[0])); j++) {
		if ((suite->tests[test_index].tags[j] != NULL) && (strcasecmp("Skip", suite->tests[test_index].tags[j]) == 0) && (run_skipped_tests == 0)) {
			skip = TRUE;
		}
	}
	if (skip) return NULL;
	
	return suite->tests[test_index].name;
}

int bc_tester_nb_tests(const char *suite_name) {
	int i = bc_tester_suite_index(suite_name);
	if (i < 0) return 0;
	return test_suite[i]->nb_tests;
}

void bc_tester_list_suites(void) {
	int j;
	for(j=0;j<nb_test_suites;j++) {
		bc_tester_printf(bc_printf_verbosity_info, "%s", bc_tester_suite_name(j));
	}
}

void bc_tester_list_tests(const char *suite_name) {
	int j;
	for( j = 0; j < bc_tester_nb_tests(suite_name); j++) {
		const char *test_name = bc_tester_test_name(suite_name, j);
		if (test_name) {
			bc_tester_printf(bc_printf_verbosity_info, "%s", test_name);
		}
	}
}

static void all_complete_message_handler(const CU_pFailureRecord pFailure) {
#ifdef HAVE_CU_GET_SUITE
	char * results = CU_get_run_results_string();
	bc_tester_printf(bc_printf_verbosity_info,"\n%s",results);
	CU_FREE(results);
#endif
}

static void suite_init_failure_message_handler(const CU_pSuite pSuite) {
	bc_tester_printf(bc_printf_verbosity_error,"Suite initialization failed for [%s]", pSuite->pName);
}

static void suite_cleanup_failure_message_handler(const CU_pSuite pSuite) {
	bc_tester_printf(bc_printf_verbosity_error,"Suite cleanup failed for [%s]", pSuite->pName);
}

#ifdef HAVE_CU_GET_SUITE
static uint64_t suite_start_time = 0;
static void suite_start_message_handler(const CU_pSuite pSuite) {
	bc_tester_printf(bc_printf_verbosity_info,"Suite [%s] started\n", pSuite->pName);
	suite_start_time = bctbx_get_cur_time_ms();
	bc_current_suite_name = pSuite->pName;
}
static void suite_complete_message_handler(const CU_pSuite pSuite, const CU_pFailureRecord pFailure) {
	bc_tester_printf(bc_printf_verbosity_info, "Suite [%s] ended in %.3f sec\n", pSuite->pName,
					 (bctbx_get_cur_time_ms() - suite_start_time) / 1000.f);
}

static uint64_t test_start_time = 0;
static void test_start_message_handler(const CU_pTest pTest, const CU_pSuite pSuite) {
	bc_tester_printf(bc_printf_verbosity_info,"Suite [%s] Test [%s] started", pSuite->pName,pTest->pName);
	test_start_time = bctbx_get_cur_time_ms();
	bc_current_test_name = pTest->pName;
}

/*derivated from bcunit*/
static void test_complete_message_handler(const CU_pTest pTest, const CU_pSuite pSuite,
										  const CU_pFailureRecord pFailureList) {
	int i;
	CU_pFailureRecord pFailure = pFailureList;
	char *buffer = NULL;
	char* result = bc_sprintf("Suite [%s] Test [%s] %s in %.3f secs", pSuite->pName, pTest->pName,
			 pFailure ? "failed" : "passed", (bctbx_get_cur_time_ms() - test_start_time) / 1000.f);

	if (pFailure) {
		for (i = 1; (NULL != pFailure); pFailure = pFailure->pNext, i++) {
			buffer = bc_sprintf("%s\n    %d. %s:%u  - %s",
									result,
									i,
									(NULL != pFailure->strFileName) ? pFailure->strFileName : "",
									pFailure->uiLineNumber,
									(NULL != pFailure->strCondition) ? pFailure->strCondition : "");
			free(result);
			result = buffer;
		}
	}

	bc_tester_printf(bc_printf_verbosity_info,"%s", result);
	free(result);

	//insert empty line
	bc_tester_printf(bc_printf_verbosity_info,"");

#ifdef __linux
	/* use mallinfo() to monitor allocated space. It is linux specific but other methods don't work:
	 * setrlimit() RLIMIT_DATA doesn't count memory allocated via mmap() (which is used internally by malloc)
	 * setrlimit() RLIMIT_AS works but also counts virtual memory allocated by thread stacks, which is very big and
	 * hardly controllable.
	 * setrlimit() RLIMIT_RSS does nothing interesting on linux.
	 * getrusage() of RSS is unreliable: memory blocks can be leaked without being read or written, which would not
	 * appear in RSS.
	 * mallinfo() itself is the less worse solution. Allocated bytes are returned as 'int' so limited to 2GB
	 */
	if (max_vm_kb) {
		struct mallinfo minfo = mallinfo();
		if (minfo.uordblks > max_vm_kb * 1024) {
			bc_tester_printf(
				bc_printf_verbosity_error,
				"The program exceeded the maximum amount of memory allocatable (%i bytes), aborting now.\n",
				minfo.uordblks);
			abort();
		}
	}
#endif
}
#endif

int bc_tester_run_tests(const char *suite_name, const char *test_name, const char *tag_name) {
	int i;

	/* initialize the BCUnit test registry */
	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	for (i = 0; i < nb_test_suites; i++) {
		bc_tester_run_suite(test_suite[i], tag_name);
	}
#ifdef HAVE_CU_GET_SUITE
	CU_set_suite_start_handler(suite_start_message_handler);
	CU_set_suite_complete_handler(suite_complete_message_handler);
	CU_set_test_start_handler(test_start_message_handler);
	CU_set_test_complete_handler(test_complete_message_handler);
#endif
	CU_set_all_test_complete_handler(all_complete_message_handler);
	CU_set_suite_init_failure_handler(suite_init_failure_message_handler);
	CU_set_suite_cleanup_failure_handler(suite_cleanup_failure_message_handler);

	if (xml_enabled == 1) {
		CU_automated_run_tests();
	} else {

#ifndef HAVE_CU_GET_SUITE
		if( suite_name ){
			bc_tester_printf(bc_printf_verbosity_info, "Tester compiled without CU_get_suite() function, running all tests instead of suite '%s'", suite_name);
		}
#else
		if (suite_name){
			CU_pSuite suite;
			suite=CU_get_suite(suite_name);
			if (!suite) {
				if (tag_name != NULL) {
					bc_tester_printf(bc_printf_verbosity_error, "Could not find suite '%s' or this suite has no tests with tag '%s'. Available suites are:", suite_name, tag_name);
				} else {
					bc_tester_printf(bc_printf_verbosity_error, "Could not find suite '%s'. Available suites are:", suite_name);
				}
				bc_tester_list_suites();
				return -1;
			} else if (test_name) {
				CU_pTest test=CU_get_test_by_name(test_name, suite);
				if (!test) {
					if (tag_name != NULL) {
						bc_tester_printf(bc_printf_verbosity_error, "Could not find test '%s' in suite '%s' or this test is not tagged '%s'. Available tests are:", test_name, suite_name, tag_name);
					} else {
						bc_tester_printf(bc_printf_verbosity_error, "Could not find test '%s' in suite '%s'. Available tests are:", test_name, suite_name);
					}
					// do not use suite_name here, since this method is case sensitive
					bc_tester_list_tests(suite->pName);
					return -2;
				} else {
					CU_ErrorCode err= CU_run_test(suite, test);
					if (err != CUE_SUCCESS) bc_tester_printf(bc_printf_verbosity_error, "CU_basic_run_test error %d", err);
				}
			} else {
				CU_run_suite(suite);
			}
		}
		else
#endif
		{
#ifdef HAVE_CU_CURSES
			if (curses) {
			/* Run tests using the BCUnit curses interface */
				CU_curses_run_tests();
			}
			else
#endif
			{
				/* Run all tests using the BCUnit Basic interface */
				CU_run_all_tests();
			}
		}
	}
#ifdef __linux
	bc_tester_printf(bc_printf_verbosity_info, "Still %i kilobytes allocated when all tests are finished.",
					 mallinfo().uordblks / 1024);
#endif

	return CU_get_number_of_tests_failed()!=0;

}

#if !defined(BC_TESTER_WINDOWS_PHONE) && !defined(BC_TESTER_WINDOWS_UNIVERSAL) && !defined(__QNX__) && !defined(ANDROID) && !defined(IOS)
static int file_exists(const char* root_path) {
	char * res_path = bc_sprintf("%s/%s", root_path, expected_res);
	int err = bctbx_file_exist(res_path);
	free(res_path);
	return err == 0;
}
#endif

static void detect_res_prefix(const char* prog) {
	char* progpath = NULL;
	char* progname = NULL;
	FILE* writable_file = NULL;


	if (prog != NULL) {
		char *ptr;
		progpath = strdup(prog);
		if (strchr(prog, '/') != NULL) {
			progpath[strrchr(prog, '/') - prog + 1] = '\0';
		} else if (strchr(prog, '\\') != NULL) {
			progpath[strrchr(prog, '\\') - prog + 1] = '\0';
		}
		ptr = strrchr(prog, '/');
		if (ptr == NULL) {
			ptr = strrchr(prog, '\\');
		}
		if (ptr != NULL) {
			progname = strdup(ptr + 1);
		}
	}
#if !defined(BC_TESTER_WINDOWS_PHONE) && !defined(BC_TESTER_WINDOWS_UNIVERSAL) && !defined(__QNX__) && !defined(ANDROID) && !defined(IOS)
	{
		char* prefix = NULL;
		char *installed_resources_path = NULL;

		if ((progname != NULL) && (progpath != NULL)) {
			installed_resources_path = bc_sprintf("%s../share/%s", progpath, progname);
		}

		if (file_exists(".")) {
			prefix = strdup(".");
		} else if (file_exists("..")) {
			prefix = strdup("..");
		} else if ((installed_resources_path != NULL) && file_exists(installed_resources_path)) {
			prefix = strdup(installed_resources_path);
		} else if (progpath) {
			//for autotools, binary is in .libs/ subdirectory
			char * progpath2 = bc_sprintf("%s/../", progpath);
			if (file_exists(progpath)) {
				prefix = strdup(progpath);
			} else if (file_exists(progpath2)) {
				prefix = strdup(progpath2);
			}
			free(progpath2);
		}
		if (installed_resources_path != NULL) free(installed_resources_path);

		if (bc_tester_resource_dir_prefix != NULL && !file_exists(bc_tester_resource_dir_prefix)) {
			bc_tester_printf(bc_printf_verbosity_error, "Invalid provided resource directory: could not find expected resources '%s' in '%s'.", expected_res, bc_tester_resource_dir_prefix);
			free(bc_tester_resource_dir_prefix);
			bc_tester_resource_dir_prefix = NULL;
		}

		if (prefix != NULL) {
			if (bc_tester_resource_dir_prefix == NULL) {
				bc_tester_printf(bc_printf_verbosity_error, "Resource directory set to %s", prefix);
				bc_tester_set_resource_dir_prefix(prefix);
			}

			if (bc_tester_writable_dir_prefix == NULL) {
				bc_tester_printf(bc_printf_verbosity_error, "Writable directory set to %s", prefix);
				bc_tester_set_writable_dir_prefix(prefix);
			}
			free(prefix);
		}
	}
#endif

	// check that we can write in writable directory
	if (bc_tester_writable_dir_prefix != NULL) {
		char * writable_file_path = bc_sprintf("%s/%s", bc_tester_writable_dir_prefix, ".bc_tester_utils.tmp");
		writable_file = fopen(writable_file_path, "w");
		if (writable_file) {
			fclose(writable_file);
		}
		free(writable_file_path);
	}
	if (bc_tester_resource_dir_prefix == NULL || writable_file == NULL) {
		if (bc_tester_resource_dir_prefix == NULL) {
			bc_tester_printf(bc_printf_verbosity_error, "Could not find resource directory '%s' in '%s'! Please try again using option --resource-dir.", expected_res, progpath);
		}
		if (writable_file == NULL) {
			bc_tester_printf(bc_printf_verbosity_error, "Failed to write file in %s. Please try again using option --writable-dir.", bc_tester_writable_dir_prefix);
		}
		abort();
	}

	if (progpath != NULL) {
		free(progpath);
	}
	if(progname) {
		free(progname);
	}
}

void bc_tester_init(void (*ftester_printf)(int level, const char *format, va_list args), int iverbosity_info, int iverbosity_error, const char* aexpected_res) {
	tester_printf_va = ftester_printf;
	bc_printf_verbosity_error = iverbosity_error;
	bc_printf_verbosity_info = iverbosity_info;
	if (!bc_tester_writable_dir_prefix) {
		bc_tester_writable_dir_prefix = strdup(".");
	}
	if (aexpected_res) {
		expected_res = strdup(aexpected_res);
	}
}

void bc_tester_set_max_vm(long amax_vm_kb) {
#ifdef __linux
	max_vm_kb = amax_vm_kb;
	bc_tester_printf(bc_printf_verbosity_info, "Maximum virtual memory space set to %li kilo bytes", max_vm_kb);
#else
	bc_tester_printf(bc_printf_verbosity_error, "Maximum virtual memory space setting is only implemented on Linux.");
#endif
}

void bc_tester_helper(const char *name, const char* additionnal_helper) {
	bc_tester_printf(bc_printf_verbosity_info,
					 "%s --help\n"
#ifdef HAVE_CU_CURSES
					 "\t\t\t--curses\n"
#endif
					 "\t\t\t--list-suites\n"
					 "\t\t\t--list-tests <suite>\n"
					 "\t\t\t--suite <suite name>\n"
					 "\t\t\t--test <test name>\n"
					 "\t\t\t--tag <tag name> (execute all tests with the given tag)\n"
					 "\t\t\t--all (execute all tests, even the ones with the Skip flag)\n"
					 "\t\t\t--resource-dir <folder path> (directory where tester resource are located)\n"
					 "\t\t\t--writable-dir <folder path> (directory where temporary files should be created)\n"
					 "\t\t\t--xml\n"
					 "\t\t\t--xml-file <xml file name>\n"
					 "\t\t\t--max-alloc <size in ko> (maximum amount of memory obtained via malloc allocator)\n"
					 "And additionally:\n"
					 "%s",
					 name,
					 additionnal_helper);
}

int bc_tester_parse_args(int argc, char **argv, int argid)
{
	int i = argid;

	if (strcmp(argv[i],"--help")==0){
		return -1;
	} else if (strcmp(argv[i],"--test")==0){
		CHECK_ARG("--test", ++i, argc);
		test_name=argv[i];
	} else if (strcmp(argv[i],"--suite")==0){
		CHECK_ARG("--suite", ++i, argc);
		suite_name=argv[i];
	} else if (strcmp(argv[i], "--tag") == 0) {
		CHECK_ARG("--tag", ++i, argc);
		tag_name = argv[i];
	} else if (strcmp(argv[i], "--all") == 0) {
		run_skipped_tests = 1;
	} else if (strcmp(argv[i],"--list-suites")==0){
		bc_tester_list_suites();
		return 0;
	} else if (strcmp(argv[i],"--list-tests")==0){
		CHECK_ARG("--list-tests", ++i, argc);
		suite_name = argv[i];
		bc_tester_list_tests(suite_name);
		return 0;
	} else if (strcmp(argv[i], "--xml-file") == 0){
		CHECK_ARG("--xml-file", ++i, argc);
		xml_file = argv[i];
		xml_enabled = 1;
	} else if (strcmp(argv[i], "--xml") == 0){
		xml_enabled = 1;
	} else if (strcmp(argv[i], "--max-alloc") == 0) {
		CHECK_ARG("--max-alloc", ++i, argc);
		max_vm_kb = atol(argv[i]);
	} else if (strcmp(argv[i], "--resource-dir") == 0) {
		CHECK_ARG("--resource-dir", ++i, argc);
		bc_tester_resource_dir_prefix = strdup(argv[i]);
	} else if (strcmp(argv[i], "--writable-dir") == 0) {
		CHECK_ARG("--writable-dir", ++i, argc);
		bc_tester_writable_dir_prefix = strdup(argv[i]);
	} else {
		bc_tester_printf(bc_printf_verbosity_error, "Unknown option \"%s\"", argv[i]);
		return -1;
	}

	if( xml_enabled && (suite_name || test_name) ){
		bc_tester_printf(bc_printf_verbosity_error, "Cannot use both XML and specific test suite");
		return -1;
	}

	/* returns number of arguments read + 1 */
	return i - argid + 1;
}

int bc_tester_start(const char* prog_name) {
	int ret;

	if (expected_res)
		detect_res_prefix(prog_name);

	if (max_vm_kb)
		bc_tester_set_max_vm(max_vm_kb);

	if( xml_enabled ){
		char * xml_tmp_file = bc_sprintf("%s.tmp", xml_file);
		CU_set_output_filename(xml_tmp_file);
		CU_automated_enable_junit_xml(TRUE); /* this requires 3.0.1 because previous versions crash automated.c */
		free(xml_tmp_file);
	}

	ret = bc_tester_run_tests(suite_name, test_name, tag_name);

	return ret;
}
void bc_tester_add_suite(test_suite_t *suite) {
	if (test_suite == NULL) {
		test_suite = (test_suite_t **)malloc(10 * sizeof(test_suite_t *));
	}
	test_suite[nb_test_suites] = suite;
	nb_test_suites++;
	if ((nb_test_suites % 10) == 0) {
		test_suite = (test_suite_t **)realloc(test_suite, (nb_test_suites + 10) * sizeof(test_suite_t *));
	}
}

void bc_tester_uninit(void) {
	/* Redisplay list of failed tests on end */
	/*BUG: do not display list of failures on mingw, it crashes mysteriously*/
#if !defined(_WIN32) && !defined(_MSC_VER)
	/* Redisplay list of failed tests on end */
	if (CU_get_number_of_failure_records()){
		CU_basic_show_failures(CU_get_failure_list());
	}
#endif
	CU_cleanup_registry();
	/*add missing final newline*/
	bc_tester_printf(bc_printf_verbosity_info,"");

	if (xml_enabled) {
		/*create real xml file only if tester did not crash*/
		char * xml_tmp_file = bc_sprintf("%s.tmp-Results.xml", xml_file);
		bc_tester_printf(bc_printf_verbosity_info, "Tests ended, renaming temporary result file %s to %s", xml_tmp_file, xml_file);
		if (rename(xml_tmp_file, xml_file) != 0) {
			bc_tester_printf(bc_printf_verbosity_error, "Failed to rename XML file: %s", strerror(errno));
		}
		free(xml_tmp_file);
	}

	if (test_suite != NULL) {
		free(test_suite);
		test_suite = NULL;
		nb_test_suites = 0;
	}

	if (bc_tester_resource_dir_prefix != NULL) {
		free(bc_tester_resource_dir_prefix);
		bc_tester_resource_dir_prefix = NULL;
	}
	if (bc_tester_writable_dir_prefix != NULL) {
		free(bc_tester_writable_dir_prefix);
		bc_tester_writable_dir_prefix = NULL;
	}
}

static void bc_tester_set_dir_prefix(char **prefix, const char *name) {
	if (*prefix != NULL) free(*prefix);
	*prefix = strdup(name);
}

const char * bc_tester_get_resource_dir_prefix(void) {
	return bc_tester_resource_dir_prefix;
}

void bc_tester_set_resource_dir_prefix(const char *name) {
	bc_tester_set_dir_prefix(&bc_tester_resource_dir_prefix, name);
}

const char * bc_tester_get_writable_dir_prefix(void) {
	return bc_tester_writable_dir_prefix;
}

void bc_tester_set_writable_dir_prefix(const char *name) {
	bc_tester_set_dir_prefix(&bc_tester_writable_dir_prefix, name);
}

static char * bc_tester_path(const char *prefix, const char *name) {
	if (name) {
		return bc_sprintf("%s/%s", prefix, name);
	} else {
		return NULL;
	}
}

char * bc_tester_res(const char *name) {
	return bc_tester_path(bc_tester_resource_dir_prefix, name);
}

char * bc_tester_file(const char *name) {
	return bc_tester_path(bc_tester_writable_dir_prefix, name);
}

char* bc_sprintfva(const char* format, va_list args) {
	/* Guess we need no more than 100 bytes. */
	int n, size = 200;
	char *p,*np;
#ifndef _WIN32
	va_list cap;/*copy of our argument list: a va_list cannot be re-used (SIGSEGV on linux 64 bits)*/
#endif
	if ((p = malloc(size)) == NULL)
		return NULL;
	while (1)
	{
		/* Try to print in the allocated space. */
#ifndef _WIN32
		va_copy(cap,args);
		n = vsnprintf (p, size, format, cap);
		va_end(cap);
#else
		/*this works on 32 bits, luckily*/
		n = vsnprintf (p, size, format, args);
#endif
		/* If that worked, return the string. */
		if (n > -1 && n < size)
			return p;
		//bc_tester_printf(bc_printf_verbosity_error, "Reallocing space.");
		/* Else try again with more space. */
		if (n > -1)	/* glibc 2.1 */
			size = n + 1;	/* precisely what is needed */
		else		/* glibc 2.0 */
			size *= 2;	/* twice the old size */
		if ((np = realloc (p, size)) == NULL)
		{
			free(p);
			return NULL;
		}
		else
		{
			p = np;
		}
	}
}

char* bc_sprintf(const char* format, ...) {
	va_list args;
	char* res;
	va_start(args, format);
	res = bc_sprintfva(format, args);
	va_end (args);
	return res;
}

void bc_free(void *ptr) {
	free(ptr);
}

const char * bc_tester_current_suite_name(void) {
	return bc_current_suite_name;
}

const char * bc_tester_current_test_name(void) {
	return bc_current_test_name;
}

const char ** bc_tester_current_test_tags(void) {
	if (bc_current_suite_name && bc_current_test_name) {
		int suite_index = bc_tester_suite_index(bc_current_suite_name);
		int test_index = bc_tester_test_index(test_suite[suite_index], bc_current_test_name);
		return test_suite[suite_index]->tests[test_index].tags;
	}
	return NULL;
}

unsigned int bc_get_number_of_failures(void) {
	return CU_get_number_of_failures();
}

void bc_set_trace_handler(void(*handler)(int, const char*, va_list)) {
#ifdef HAVE_CU_SET_TRACE_HANDLER
	CU_set_trace_handler(handler);
#else
	bc_tester_printf(bc_printf_verbosity_error, "CU_set_trace_handler not implemented");
#endif
}

int bc_assert(const char* file, int line, int predicate, const char* format) {
	if (!predicate) bc_tester_printf(bc_printf_verbosity_info, format, NULL);
	return CU_assertImplementation(predicate, line, format, file, "", FALSE);
}
