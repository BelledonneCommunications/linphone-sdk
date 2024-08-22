/*
 * Copyright (c) 2016-2020 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <bctoolbox/defs.h>
#include <bctoolbox/logging.h>
#include <bctoolbox/tester.h>
#include <bctoolbox/vfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef BCUNIT_USE_BUILD_INTERFACE
#include "Automated.h"
#include "Basic.h"
#include "MyMem.h"
#include "Util.h"
#else
#include <BCUnit/Automated.h>
#include <BCUnit/Basic.h>
#include <BCUnit/MyMem.h>
#include <BCUnit/Util.h>
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

#ifdef __linux__
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

static char *log_file_name = NULL;

static int bc_printf_verbosity_info;
static int bc_printf_verbosity_error;

static test_suite_t **test_suite = NULL;
static int nb_test_suites = 0;

#ifdef HAVE_CU_CURSES
#include "BCUnit/CUCurses.h"
static unsigned char curses = 0;
#endif

static const char default_xml_file[] = "BCUnitAutomated";
static const char *xml_file = default_xml_file;
static int xml_enabled = 0;
static bctbx_list_t *suites_to_run = NULL;
static bctbx_list_t *suites_to_exclude = NULL;
static bctbx_list_t *tags_to_run = NULL;
static char *test_name = NULL;
static char *tag_name = NULL;
static char *expected_res = NULL;
static size_t max_vm_kb = 0;
static int run_skipped_tests = 0;
static int max_parallel_suites = 0; /*if 0, but parallel is requested, an arbitrary value is determined.*/
static int run_in_parallel = 0;
static uint64_t globalTimeout = 0;
static int max_failed_tests_threshold = 0;

// To keep record of the process name who started and args
static char **origin_argv = NULL;
static int origin_argc = 0;

// Custom cli arguments handlers	for end tests implementations
static int (*verbose_arg_func)(const char *arg) = NULL;
static int (*silent_arg_func)(const char *arg) = NULL;
static int (*logfile_arg_func)(const char *arg) = NULL;

// Processing custom native events for processing events (like PeekMessage for Windows) that is not implemented by
// Linphone
static void (*process_events)(void) = NULL;

typedef struct _suite_status {
	int pid;
	int finished;
} suite_status_t;

static int bc_tester_registered_suite_count(void) {
	return CU_get_registry()->uiNumberOfSuites;
}

static CU_pSuite bc_tester_registered_suite_index(int index) {
	CU_pSuite suite = CU_get_suite_at_pos(index + 1);
	if (suite == NULL) {
		abort();
	}
	return suite;
}

void bc_tester_set_silent_func(int (*func)(const char *)) {
	if (func) {
		silent_arg_func = func;
	}
}

void bc_tester_set_verbose_func(int (*func)(const char *)) {
	if (func) {
		verbose_arg_func = func;
	}
}

void bc_tester_set_logfile_func(int (*func)(const char *)) {
	if (func) {
		logfile_arg_func = func;
	}
}

void bc_tester_set_process_events_func(void (*func)(void)) {
	process_events = func;
}

void bc_tester_process_events(void) {
	if (process_events) process_events();
}

static void (*tester_printf_va)(int level, const char *format, va_list args) = NULL;

void bc_tester_printf(int level, const char *format, ...) {
	va_list args;
	va_start(args, format);
	if (tester_printf_va) {
		tester_printf_va(level, format, args);
	} else {
		vfprintf(stderr, format, args);
	}
	va_end(args);
}

static bool_t test_enabled(const test_t *test, const bctbx_list_t *tags) {
	size_t j;
	bool_t enabled = (tags == NULL) ? TRUE : FALSE;
	for (j = 0; j < (sizeof(test->tags) / sizeof(test->tags[0])); j++) {
		if (test->tags[j] == NULL) break;
		if (strcasecmp(test->tags[j], "skip") == 0 && !run_skipped_tests) {
			// if test has the skip tag but skipped tests are not allowsed, disable it.
			enabled = FALSE;
			break;
		}
		const bctbx_list_t *it = tags;
		while (it != NULL) {
			const char *tag = bctbx_list_get_data(it);
			if (tag) {
				if (tag[0] == '!') {
					if (strcasecmp(test->tags[j], tag + 1) == 0) {
						// the test has a tag that we want to exclude.
						return FALSE;
					}
				} else {
					if (strcasecmp(test->tags[j], tag) == 0) {
						// the test has a tag that we want.
						return TRUE;
					}
				}
			}
			it = bctbx_list_next(it);
		}
	}
	return enabled;
}

int bc_tester_register_suite(test_suite_t *suite, const bctbx_list_t *tags) {
	int i;
	CU_pSuite pSuite = NULL;

	for (i = 0; i < suite->nb_tests; i++) {
		if (test_enabled(&suite->tests[i], tags)) {
			if (pSuite == NULL) {
				pSuite = CU_add_suite_with_setup_and_teardown(suite->name, suite->before_all, suite->after_all,
				                                              suite->before_each, suite->after_each);
				pSuite->pUserData = suite;
			}
			if (NULL == CU_add_test(pSuite, suite->tests[i].name, suite->tests[i].func)) {
				return CU_get_error();
			}
		}
	}
	return 0;
}

// Allows to register directly to BCUnit by name. Wrap bc_tester_register_suite
int bc_tester_register_suite_by_name(const char *suite_name) {
	int suiteIdx = -1;

	suiteIdx = bc_tester_suite_index(suite_name);
	if (suiteIdx != -1) {
		if (!CU_registry_initialized()) {
			if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();
		}
		return bc_tester_register_suite(test_suite[suiteIdx], NULL);
	}
	return -1;
}

const char *bc_tester_suite_name(int suite_index) {
	if (suite_index >= nb_test_suites) return NULL;
	return test_suite[suite_index]->name;
}

int bc_tester_suite_index(const char *suite_name) {
	int i;

	for (i = 0; i < nb_test_suites; i++) {
		if (strcasecmp(suite_name, test_suite[i]->name) == 0) {
			return i;
		}
	}

	return -1;
}

int bc_tester_test_index(test_suite_t *suite, const char *test_name) {
	int i;

	for (i = 0; i < suite->nb_tests; i++) {
		if (strcasecmp(test_name, suite->tests[i].name) == 0) {
			return i;
		}
	}

	return -1;
}

int bc_tester_nb_suites(void) {
	return nb_test_suites;
}

const char *bc_tester_test_name(const char *suite_name, int test_index) {
	test_suite_t *suite = NULL;
	size_t j = 0;
	bool_t skip = FALSE;

	int suite_index = bc_tester_suite_index(suite_name);
	if ((suite_index < 0) || (suite_index >= nb_test_suites)) return NULL;
	suite = test_suite[suite_index];
	if (test_index >= suite->nb_tests) return NULL;

	for (j = 0; j < (sizeof(suite->tests[test_index].tags) / sizeof(suite->tests[test_index].tags[0])); j++) {
		if ((suite->tests[test_index].tags[j] != NULL) && (strcasecmp("Skip", suite->tests[test_index].tags[j]) == 0) &&
		    (run_skipped_tests == 0)) {
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
	for (j = 0; j < nb_test_suites; j++) {
		bc_tester_printf(bc_printf_verbosity_info, "%s", bc_tester_suite_name(j));
	}
}

void bc_tester_list_tests(const char *suite_name) {
	int j;
	for (j = 0; j < bc_tester_nb_tests(suite_name); j++) {
		const char *test_name = bc_tester_test_name(suite_name, j);
		if (test_name) {
			bc_tester_printf(bc_printf_verbosity_info, "%s", test_name);
		}
	}
}

char *bc_tester_get_failed_asserts(void) {
	int i;
	CU_pFailureRecord pFailure = NULL;
	char *buffer = "", *tmp;

	pFailure = CU_get_failure_list();

	if (pFailure) {
		for (i = 1; (NULL != pFailure); pFailure = pFailure->pNext, i++) {
			tmp = bc_sprintf("%s\n    %d. %s:%u  - %s", buffer, i,
			                 (NULL != pFailure->strFileName) ? pFailure->strFileName : "", pFailure->uiLineNumber,
			                 (NULL != pFailure->strCondition) ? pFailure->strCondition : "");
			if (i != 1) {
				bctbx_free(buffer);
			}
			buffer = tmp;
		}
	}
	return buffer;
}

static void write_suite_result_file(char *suite_name, char *results_string) {
	bctbx_vfs_file_t *bctbx_file;
	char *suite_name_wo_spaces, *file_name;

	suite_name_wo_spaces = bctbx_replace(bctbx_strdup(suite_name), ' ', '_');
	file_name = bc_sprintf("%s.result", suite_name_wo_spaces);
	bctbx_file = bctbx_file_open(bctbx_vfs_get_default(), file_name, "w+");
	if (bctbx_file) {
		bctbx_file_truncate(bctbx_file, 0);
		bctbx_file_fprintf(bctbx_file, 0, results_string);
		bctbx_file_close(bctbx_file);
	}
	bctbx_free(suite_name_wo_spaces);
	bctbx_free(file_name);
}

static void merge_and_print_results_files(void) {
	bctbx_vfs_file_t *bctbx_file;
	int i;
	ssize_t file_size, read_bytes;
	char *buffer, *tmp;
	char *suite_name_wo_spaces, *file_name;
	char *results = NULL;

	for (i = 0; i < bc_tester_registered_suite_count(); i++) {
		CU_pSuite suite = CU_get_suite_at_pos(i + 1);
		const char *suite_name = suite->pName;
		suite_name_wo_spaces = bctbx_replace(bctbx_strdup(suite_name), ' ', '_');
		file_name = bc_sprintf("%s.result", suite_name_wo_spaces);
		bctbx_file = bctbx_file_open2(bctbx_vfs_get_default(), file_name, O_RDONLY);

		if (bctbx_file) {
			file_size = (ssize_t)bctbx_file_size(bctbx_file);
			if (file_size > 0) {
				buffer = malloc(file_size + 1);
				read_bytes = bctbx_file_read(bctbx_file, (void *)buffer, file_size, 0);
				if (read_bytes == file_size) {
					buffer[read_bytes] = '\0';

					if (results == NULL) {
						results = bctbx_concat("Suite '", suite_name, "' results:\n", buffer, NULL);
					} else {
						tmp = bctbx_concat(results, "\nSuite '", suite_name, "' results:\n", buffer, NULL);
						bctbx_free(results);
						results = tmp;
					}
				} else {
					bc_tester_printf(bc_printf_verbosity_error, "Failed to read suite results file '%s'", file_name);
				}
				bctbx_free(buffer);
			} else {
				bc_tester_printf(bc_printf_verbosity_error, "Empty suite results file '%s'", file_name);
			}
			remove(file_name);
			bctbx_file_close(bctbx_file);
		} else {
			bc_tester_printf(bc_printf_verbosity_error, "Failed to open suite results file '%s'", file_name);
			// Assume suite crash and report it.
			if (results == NULL) {
				results = bctbx_concat("Suite '", suite_name, "' results: CRASH\n", NULL);
			} else {
				tmp = bctbx_concat(results, "\nSuite '", suite_name, "' results: CRASH\n", NULL);
				bctbx_free(results);
				results = tmp;
			}
		}
		bctbx_free(suite_name_wo_spaces);
		bctbx_free(file_name);
	}
	if (results) {
		bc_tester_printf(bc_printf_verbosity_info, "Tests suites results: \n%s", results);
		bctbx_free(results);
	}
}

static void all_complete_message_handler(BCTBX_UNUSED(const CU_pFailureRecord pFailure)) {
#ifdef HAVE_CU_GET_SUITE
	if (run_in_parallel != 0) {
		if (bc_current_suite_name) {
			char *results = CU_get_run_results_string();
			write_suite_result_file(bc_current_suite_name, results);
			CU_FREE(results);
		} else {
			merge_and_print_results_files();
		}
	} else {
		char *results = CU_get_run_results_string();
		bc_tester_printf(bc_printf_verbosity_info, "\n%s", results);
		CU_FREE(results);
	}
#endif
}

static void suite_init_failure_message_handler(const CU_pSuite pSuite) {
	bc_tester_printf(bc_printf_verbosity_error, "Suite initialization failed for [%s]", pSuite->pName);
}

static void suite_cleanup_failure_message_handler(const CU_pSuite pSuite) {
	bc_tester_printf(bc_printf_verbosity_error, "Suite cleanup failed for [%s]", pSuite->pName);
}

#ifdef HAVE_CU_GET_SUITE
static uint64_t suite_start_time = 0;
static void suite_start_message_handler(const CU_pSuite pSuite) {
	bc_tester_printf(bc_printf_verbosity_info, "Suite [%s] started\n", pSuite->pName);
	suite_start_time = bctbx_get_cur_time_ms();
	bc_current_suite_name = pSuite->pName;
}
static void suite_complete_message_handler(const CU_pSuite pSuite, BCTBX_UNUSED(const CU_pFailureRecord pFailure)) {
	bc_tester_printf(bc_printf_verbosity_info, "Suite [%s] ended in %.3f sec\n", pSuite->pName,
	                 (bctbx_get_cur_time_ms() - suite_start_time) / 1000.f);
}

static uint64_t test_start_time = 0;
static void test_start_message_handler(const CU_pTest pTest, const CU_pSuite pSuite) {
	bc_tester_printf(bc_printf_verbosity_info, "Suite [%s] Test [%s] started", pSuite->pName, pTest->pName);
	test_start_time = bctbx_get_cur_time_ms();
	bc_current_test_name = pTest->pName;
}

/*derivated from bcunit*/
static void
test_complete_message_handler(const CU_pTest pTest, const CU_pSuite pSuite, const CU_pFailureRecord pFailureList) {
	int i;
	CU_pFailureRecord pFailure = pFailureList;
	char *buffer = NULL;
	char *result = bc_sprintf("Suite [%s] Test [%s] %s in %.3f secs", pSuite->pName, pTest->pName,
	                          pFailure ? "failed" : "passed", (bctbx_get_cur_time_ms() - test_start_time) / 1000.f);

	if (pFailure) {
		for (i = 1; (NULL != pFailure); pFailure = pFailure->pNext, i++) {
			buffer = bc_sprintf("%s\n    %d. %s:%u  - %s", result, i,
			                    (NULL != pFailure->strFileName) ? pFailure->strFileName : "", pFailure->uiLineNumber,
			                    (NULL != pFailure->strCondition) ? pFailure->strCondition : "");
			bctbx_free(result);
			result = buffer;
		}
	}

	bc_tester_printf(bc_printf_verbosity_info, "%s", result);
	bctbx_free(result);

	// insert empty line
	bc_tester_printf(bc_printf_verbosity_info, "");

#ifdef __linux__
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
// Needed to avoid warning: 'mallinfo' is deprecated
#if defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 33)
		struct mallinfo2 minfo = mallinfo2();
#else
		struct mallinfo minfo = mallinfo();
#endif
#else
		struct mallinfo minfo = mallinfo();
#endif

		if ((size_t)minfo.uordblks > max_vm_kb * 1024) {
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

//
static char *get_logfile_name(const char *base_name, const char *suite_name) {
	if (suite_name) {
		char *name_wo_spaces = bctbx_replace(bctbx_strdup(suite_name), ' ', '_');
		char *ret = bc_sprintf("%s_%s.log", base_name, name_wo_spaces);
		bctbx_free(name_wo_spaces);
		return ret;
	} else {
		return strdup(base_name);
	}
}

// Get the junit xml file name for a specific suite or the global one
// If passed to BCUnit, it will append "-Results.xml" to the name
// Use `suffix` to match	the resulting name if needed
static char *get_junit_xml_file_name(const char *suite_name, const char *suffix) {
	char *xml_tmp_file;

	if (suite_name) {
		unsigned int suiteIdx = CU_get_suite_pos_by_name(suite_name) - 1;
		if (suffix) {
			xml_tmp_file = bc_sprintf("%s_%d%s", xml_file, suiteIdx, suffix);
		} else {
			xml_tmp_file = bc_sprintf("%s_%d", xml_file, suiteIdx);
		}
	} else {
		if (suffix) {
			xml_tmp_file = bc_sprintf("%s%s", xml_file, suffix);
		} else {
			xml_tmp_file = bc_sprintf("%s", xml_file);
		}
	}
	return xml_tmp_file;
}

// In case tests are started in parallel.
// Merge partial JUnit suites reports into the final XML file
static void merge_junit_xml_files(const char *dst_file_name) {
	char **suite_junit_xml_results;
	char *file_name;
	bctbx_vfs_file_t *bctbx_file;
	ssize_t read_bytes = 0, file_size = 0, offset = 0;
	int i;
	int effective_suite_count = bc_tester_registered_suite_count();

	suite_junit_xml_results = malloc(sizeof(char *) * effective_suite_count);
	for (i = 0; i < effective_suite_count; i++) {
		file_name = get_junit_xml_file_name(bc_tester_registered_suite_index(i)->pName, "-Results.xml");
		bctbx_file = bctbx_file_open2(bctbx_vfs_get_default(), file_name, O_RDONLY);
		if (bctbx_file != NULL) {
			file_size = (ssize_t)bctbx_file_size(bctbx_file);
			suite_junit_xml_results[i] = malloc(file_size + 1);
			read_bytes = bctbx_file_read(bctbx_file, (void *)suite_junit_xml_results[i], file_size, 0);
			if (read_bytes == file_size) {
				suite_junit_xml_results[i][file_size] = '\0';
				// Also remove the file
				bctbx_file_close(bctbx_file);
				remove(file_name);
			} else {
				bc_tester_printf(bc_printf_verbosity_error, "Could not read JUnit XML file '%s' to merge", file_name);
				bctbx_free(suite_junit_xml_results[i]);
				suite_junit_xml_results[i] = NULL;
				bctbx_file_close(bctbx_file);
			}
		} else {
			bc_tester_printf(bc_printf_verbosity_error, "Could not open JUnit XML file '%s' to merge", file_name);
			suite_junit_xml_results[i] = NULL;
			bctbx_file_close(bctbx_file);
		}
		bctbx_free(file_name);
	}
	// Empty the destination file
	bctbx_file = bctbx_file_open(bctbx_vfs_get_default(), dst_file_name, "w+");
	if (bctbx_file) {
		bctbx_file_truncate(bctbx_file, 0);
		offset = bctbx_file_fprintf(bctbx_file, 0, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuites>\n");
		for (i = 0; i < effective_suite_count; i++) {
			if (suite_junit_xml_results[i] != NULL) {
				offset += bctbx_file_fprintf(bctbx_file, (off_t)offset, "%s", suite_junit_xml_results[i]);
				bctbx_free(suite_junit_xml_results[i]);
			}
		}
		bctbx_file_fprintf(bctbx_file, (off_t)offset, "</testsuites>\n");
		bctbx_file_close(bctbx_file);
	}
	bctbx_free(suite_junit_xml_results);
}

// In case tests are started in parallel AND --log-file was given
// Merge	individual suite log file into a global one
static void merge_log_files(const char *base_logfile_name) {
	bctbx_vfs_file_t *dst_file;
	bctbx_vfs_file_t *bctbx_file;
	void *buf;
	ssize_t offset = 0, file_size = 0, read_bytes = 0;
	int i;
	int effective_suite_count = bc_tester_registered_suite_count();

	dst_file = bctbx_file_open(bctbx_vfs_get_default(), base_logfile_name, "w+");
	if (!dst_file) {
		bc_tester_printf(bc_printf_verbosity_error, "Failed to create target log file '%s'", base_logfile_name);
		return;
	}
	for (i = 0; i < effective_suite_count; ++i) {
		char *suite_logfile_name = get_logfile_name(log_file_name, bc_tester_registered_suite_index(i)->pName);
		bctbx_file = bctbx_file_open2(bctbx_vfs_get_default(), suite_logfile_name, O_RDONLY);

		if (!bctbx_file) {
			bc_tester_printf(bc_printf_verbosity_error, "Could not open log file '%s' to merge into '%s'",
			                 suite_logfile_name, base_logfile_name);
			continue;
		}
		file_size = (ssize_t)bctbx_file_size(bctbx_file);
		buf = malloc(file_size);
		read_bytes = bctbx_file_read(bctbx_file, buf, file_size, 0);
		if (read_bytes == file_size) {
			ssize_t written = bctbx_file_write(dst_file, buf, (size_t)file_size, (off_t)offset);
			if (written == (ssize_t)BCTBX_VFS_ERROR) {
				bc_tester_printf(bc_printf_verbosity_error, "Could not write logs of suite %s into log file '%s'",
				                 suite_logfile_name, base_logfile_name);
			} else offset += written;
		} else {
			bc_tester_printf(bc_printf_verbosity_error, "Could not read log file '%s' to merge into '%s'",
			                 suite_logfile_name, base_logfile_name);
		}
		bctbx_file_close(bctbx_file);
		bctbx_free(suite_logfile_name);
		bctbx_free(buf);
	}
	bctbx_file_close(dst_file);
}

// Number of test suites to run concurrently
int bc_tester_get_max_parallel_processes(void) {
	if (max_parallel_suites == 0) return (nb_test_suites / 2) + 1;
	return max_parallel_suites;
}

void bc_tester_set_max_parallel_suites(int nb_suites) {
	max_parallel_suites = nb_suites;
}

void bc_tester_set_max_failed_tests_threshold(int threshold) {
	max_failed_tests_threshold = threshold;
}

void bc_tester_set_global_timeout(int seconds) {
	globalTimeout = seconds;
}

#ifdef _WIN32

#if 0
static void kill_sub_processes(int *pids) {
	// TODO: Windows support
}
#endif

#else

#if 0
// If there was an error, kill zombies
static void kill_sub_processes(int *pids) {
	int i;
	for (i = 0; i < nb_test_suites; ++i) {
		if (pids[i] > 0) {
			kill(pids[i], SIGTERM);
		}
	}
}
#endif

#endif

// Start test subprocess for the given suite
static void prepare_child_arguments(const char *suite_name, const char **argv) {
	int argc = 0;
	int i;

	argv[argc++] = origin_argv[0];
	for (i = 1; origin_argv[i]; ++i) {
		if (strcmp(origin_argv[i], "--verbose") == 0) {
			argv[argc++] = origin_argv[i];
		} else if (strcmp(origin_argv[i], "--silent") == 0) {
			argv[argc++] = origin_argv[i];
		} else if (strcmp(origin_argv[i], "--log-file") == 0) {
			// Create a specific log file for this suite
			argv[argc++] = origin_argv[i++];
			argv[argc++] = get_logfile_name(log_file_name, suite_name);
		} else if (strcmp(origin_argv[i], "--xml-file") == 0) {
			argv[argc++] = origin_argv[i++];
			argv[argc++] = origin_argv[i];
		} else if (strcmp(origin_argv[i], "--parallel") == 0) {
			argv[argc++] = origin_argv[i];
		} else if (strcmp(origin_argv[i], "--suite") == 0) {
			++i;
			// eliminate --suite
		} else if (strcmp(origin_argv[i], "--exclude-suite") == 0) {
			++i;
			// eliminate --exclude-suite
		} else {
			// Keep unknown parameters
			argv[argc++] = origin_argv[i];
		}
	}
	argv[argc++] = "--xml";
	argv[argc++] = "--xml-file";
	argv[argc++] = get_junit_xml_file_name(suite_name, NULL);
	argv[argc++] = "--suite";
	argv[argc++] = suite_name;
	argv[argc] = NULL;
}

#ifdef _WIN32

// Start test subprocess for the given suite
static int start_sub_process(const char *suite_name, PROCESS_INFORMATION *pi) {
	int i;
	char **argv = (char **)malloc(sizeof(char *) * (origin_argc + 10)); // Assume safey 10 more parameters
	char *commandLine = NULL, *commandLineTemp = NULL;

	prepare_child_arguments(suite_name, (const char **)argv);
	commandLine = bctbx_strdup(argv[0]);

	/* Serialize the command line with its arguments */
	for (i = 0; argv[i] != NULL; ++i) {
		commandLineTemp = bctbx_strdup_printf("%s %s", commandLine, argv[i]);
		bctbx_free(commandLine);
		commandLine = commandLineTemp;
		commandLineTemp = NULL;
	}

	// Start the child process.
	STARTUPINFOA si;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(pi, sizeof(*pi));
	if (!CreateProcessA(NULL,        // No module name (use command line)
	                    commandLine, // Command line
	                    NULL,        // Process handle not inheritable
	                    NULL,        // Thread handle not inheritable
	                    FALSE,       // Set handle inheritance to FALSE
	                    0,           // CREATE_NEW_CONSOLE,              // No creation flags
	                    NULL,        // Use parent's environment block
	                    NULL,        // Use parent's starting directory
	                    &si,         // Pointer to STARTUPINFO structure
	                    pi)          // Pointer to PROCESS_INFORMATION structure
	) {
		printf("CreateProcess failed (%d).\n", GetLastError());
		return GetLastError();
	}
	bctbx_free(commandLine);
	free(argv);
	return 0;
}

#else

static int start_sub_process(const char *suite_name) {
	const char *argv[origin_argc + 10]; // Assume safey 10 more parameters
	prepare_child_arguments(suite_name, argv);
	return execv(argv[0], (char **)argv);
}

#endif

// For parallel tests only - handle anormally exited test suites
// Remove previously generated XML suite file if exited anormally (could cause unusable final JUnit XML)
// And mark all tests for the suite as failed
static int handle_sub_process_error(int pid, suite_status_t *suitesPids) {
	int failed_tests = 0;
	int i;
	int effective_suite_count = bc_tester_registered_suite_count();
	for (i = 0; i < effective_suite_count; ++i) {
		if (suitesPids[i].pid == pid) {
			ssize_t offset;
			CU_pSuite suite = bc_tester_registered_suite_index(i);
			char *suite_file_name = get_junit_xml_file_name(suite->pName, "-Results.xml");
			bctbx_vfs_file_t *bctbx_file = bctbx_file_open(bctbx_vfs_get_default(), suite_file_name, "w+");
			bctbx_file_truncate(bctbx_file, 0);

			offset = bctbx_file_fprintf(
			    bctbx_file, 0,
			    "\n<testsuite name=\"%s\" tests=\"%d\" time=\"0\" failures=\"%d\" errors=\"0\" skipped=\"0\">\n",
			    suite->pName, suite->uiNumberOfTests, suite->uiNumberOfTests);
			failed_tests = suite->uiNumberOfTests;
			for (unsigned int j = 0; j < suite->uiNumberOfTests; ++j) {
				offset += bctbx_file_fprintf(bctbx_file, (off_t)offset, "\t<testcase classname=\"%s\" name=\"%s\">\n",
				                             suite->pName, CU_get_test_at_pos(suite, j + 1)->pName);
				offset += bctbx_file_fprintf(bctbx_file, (off_t)offset,
				                             "\t\t<failure message=\"\" type=\"Failure\">\n\t\tGlobal suite failure\n");
				offset += bctbx_file_fprintf(bctbx_file, (off_t)offset, "\t\t</failure>\n\t</testcase>\n");
			}
			bctbx_file_fprintf(bctbx_file, (off_t)offset, "\n</testsuite>\n");
			bc_tester_printf(bc_printf_verbosity_info, "Suite '%s' ended in error. Marking all tests as failed",
			                 suite->pName);
			bctbx_file_close(bctbx_file);
			bctbx_free(suite_file_name);
			break;
		}
	}
	return failed_tests;
}

static test_suite_t *lookup_suite_from_pid(int pid, suite_status_t *suitesStatus, int *index) {
	int i;
	int effective_suite_count = bc_tester_registered_suite_count();
	for (i = 0; i < effective_suite_count; ++i) {
		if (suitesStatus[i].pid == pid) {
			CU_pSuite suite = bc_tester_registered_suite_index(i);
			if (index) *index = i;
			if (suite) return (test_suite_t *)suite->pUserData;
		}
	}
	return NULL;
}

static int test_suite_cpu_weight(const test_suite_t *ts) {
	int maxProcess = bc_tester_get_max_parallel_processes();
	int cpu_weight = ts->cpu_weight == 0 ? 1 : ts->cpu_weight;
	if (cpu_weight > maxProcess) {
		bc_tester_printf(
		    bc_printf_verbosity_error,
		    "Suite [%s] has a declared cpu weight [%i] greater than the maximum number of parallel processes. "
		    "cpu_weight is ignored.",
		    ts->name, cpu_weight);
		cpu_weight = 1;
	}
	return cpu_weight;
}

#ifdef _WIN32
int bc_tester_run_parallel(void) {
	int ret = 0; // Global return status;
	int effective_suite_count = bc_tester_registered_suite_count();
	if (effective_suite_count > 0) {
		PROCESS_INFORMATION *suitesPids = malloc(effective_suite_count * sizeof(PROCESS_INFORMATION));
		int *processed = malloc(effective_suite_count * sizeof(int));
		uint64_t time_start = bctbx_get_cur_time_ms(), elapsed = time_start, print_timer = time_start;

		// Assume there is a problem if a suite is still running 60mn after the start of the tester. TODO make timeout
		// a cli parameter ?
		uint64_t timeout = 0;
		if (globalTimeout <= 0) {
			globalTimeout = 60 * 60;
		}
		timeout = time_start + (globalTimeout * 1000);

		int maxProcess = bc_tester_get_max_parallel_processes();
		int nextSuite = 0;     // Next suite id to be exec'd
		int runningSuites = 0; // Number of currently running suites
		int testsFinished = 0;

		memset(suitesPids, 0, effective_suite_count * sizeof(PROCESS_INFORMATION));
		memset(processed, 0, effective_suite_count * sizeof(int));
		do {
			if (nextSuite < effective_suite_count && runningSuites < maxProcess) {
				PROCESS_INFORMATION pi;
				if (start_sub_process(bc_tester_registered_suite_index(nextSuite)->pName, &pi) != 0) {
					bc_tester_printf(bc_printf_verbosity_error, "Error while starting suite sub-process. Aborting.");
					return -1;
				}
				suitesPids[nextSuite] = pi;
				runningSuites++;
				nextSuite++;
			} else {
				for (int i = 0; i < nextSuite; ++i) {
					if (processed[i] == 0) {
						int returnCode = WaitForSingleObject(suitesPids[i].hProcess, 1000);
						if ((returnCode != WAIT_TIMEOUT)) {
							--runningSuites;
							++testsFinished;
							bc_tester_printf(bc_printf_verbosity_error,
							                 "Suite sub process (ID %d) terminated with return code %d.", i,
							                 returnCode);
							processed[i] = 1;
							ret += returnCode;
						}
					}
				}
			}
			bctbx_sleep_ms(50);
			if (elapsed - print_timer > 10000) { // print message only every ~10s...
				bc_tester_printf(
				    bc_printf_verbosity_error,
				    "Waiting for test suites to finish... Total Suites(%d). Suites running(%d), Finished(%d)",
				    nb_test_suites, runningSuites, testsFinished);
				print_timer = bctbx_get_cur_time_ms();
			}
			elapsed = bctbx_get_cur_time_ms();
		} while (testsFinished < nb_test_suites);
		// This code has been commented out in order to display failing test of timed out suites in the allure report
		//		} while (testsFinished < nb_test_suites && elapsed < timeout);

		if (elapsed >= timeout) {
			bc_tester_printf(bc_printf_verbosity_error,
			                 "*** Test suite took too much time. Please check errors or split longest test suites to "
			                 "benefit from parallel execution. ***");
			ret = -1;
		}
		for (int i = 0; i < nextSuite; ++i) {
			CloseHandle(suitesPids[i].hProcess);
			CloseHandle(suitesPids[i].hThread);
		}
		bc_tester_printf(bc_printf_verbosity_info, "All suites ended.");

		if (ret != -1) {
			int seconds = (int)(elapsed - time_start) / 1000;
			all_complete_message_handler(NULL);
			bc_tester_printf(bc_printf_verbosity_info, "Full parallel run completed in %2i mn %2i s.\n", seconds / 60,
			                 seconds % 60);
		}
		free(suitesPids);
		free(processed);
	}
	return ret == -1 ? ret : (ret > max_failed_tests_threshold);
}
#else

int bc_tester_run_parallel(void) {
	int effective_suite_count = bc_tester_registered_suite_count();
	suite_status_t suitesPids[effective_suite_count];
	uint64_t time_start = bctbx_get_cur_time_ms(), elapsed = time_start, print_timer = time_start;
	uint64_t timeout = 0;

	memset(&suitesPids, 0, sizeof(suite_status_t) * effective_suite_count);

	if (globalTimeout <= 0) {
		globalTimeout = 60 * 60;
	}
	timeout = time_start + (globalTimeout * 1000);

	int maxProcess = bc_tester_get_max_parallel_processes();
	int nextSuite = 0;     // Next suite id to be exec'd
	int runningSuites = 0; // Number of currently running suites
	int testsFinished = 0;
	int ret = 0; // Global return status;
	int suitesCpuWeight = 0;
	int timeoutElapsed = 0;

	do {
		if (nextSuite < effective_suite_count) {
			CU_Suite *launchedSuite = bc_tester_registered_suite_index(nextSuite);
			test_suite_t *bcLaunchedSuite = (test_suite_t *)launchedSuite->pUserData;
			int cpu_weight = test_suite_cpu_weight(bcLaunchedSuite);

			if (suitesCpuWeight + cpu_weight <= maxProcess) {
				int pid = fork();

				if (pid == -1) {
					bc_tester_printf(bc_printf_verbosity_error,
					                 "Error during fork() while starting child process. Aborting.");
					return -1;
				} else if (pid > 0) {
					suitesPids[nextSuite].pid = pid;
					runningSuites++;
					suitesCpuWeight += cpu_weight;
					nextSuite++;
				} else {
					if (start_sub_process(launchedSuite->pName) == -1) {
						bc_tester_printf(bc_printf_verbosity_error,
						                 "Error while starting suite sub-process. Aborting.");
						return -1;
					}
				}
			}
		}
		if (runningSuites > 0) {
			int wstatus, childPid, childRet;
			if ((childPid = waitpid(-1, &wstatus, WNOHANG)) == -1) {
				bc_tester_printf(bc_printf_verbosity_error,
				                 "Error during waitpid() while waiting for child process. Aborting.");
				return -1;
			}
			if (childPid != 0) {
				if (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) {
					int index = 0;
					test_suite_t *finishedSuite = lookup_suite_from_pid(childPid, suitesPids, &index);
					suitesCpuWeight -= test_suite_cpu_weight(finishedSuite);
					suitesPids[index].finished = 1; /* reset when suite is finished */
					--runningSuites;
					++testsFinished;
				}
				/*
				 * Handle test suite errors, which can be:
				 * - a crash (WIFSIGNALED(wstatus))
				 * - a -1 return value (typically when sanitizer detects an error)
				 * In these two cases, the whole suite is marked as failed (all tests failed).
				 */
				if (WIFSIGNALED(wstatus) || (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 255)) {
					bc_tester_printf(bc_printf_verbosity_error,
					                 "Suite sub process (pid %d) crashed or exited with (-1) return code.", childPid);
					childRet = handle_sub_process_error(childPid, suitesPids);
				} else {
					/* Here the sub-process returned the number of failed tests. */
					childRet = WEXITSTATUS(wstatus);
				}
				ret += childRet;
			}
		}

		bctbx_sleep_ms(50);

		if (elapsed - print_timer > 10000) { // print message only every ~10s...
			if (!timeoutElapsed && (elapsed >= timeout)) {
				int i;
				timeoutElapsed = TRUE;
				for (i = 0; i < effective_suite_count; ++i) {
					if (suitesPids[i].finished == 0) {
						CU_pSuite unfinishedSuite = bc_tester_registered_suite_index(i);
						bc_tester_printf(bc_printf_verbosity_error, "Suite [%s] is still running after timeout.",
						                 unfinishedSuite->pName);
					}
				}
			}
			bc_tester_printf(bc_printf_verbosity_error,
			                 "Waiting for test suites to finish... Total Suites(%d). Suites running(%d), Finished(%d), "
			                 "Timeout reached(%s)",
			                 effective_suite_count, runningSuites, testsFinished, timeoutElapsed ? "yes" : "no");
			print_timer = bctbx_get_cur_time_ms();
		}
		elapsed = bctbx_get_cur_time_ms();
	} while (testsFinished < effective_suite_count);
	// Despite the timeout, we let all suites finish normally.

	if (elapsed >= timeout) {
		// But if timeout is reached, the overall result is that the tests have failed.
		bc_tester_printf(bc_printf_verbosity_error, "*** Test suite took too much time. Please check errors or split "
		                                            "longest test suites to benefit from parallel execution. ***");
		ret = -1;
	}
	bc_tester_printf(bc_printf_verbosity_info, "All suites ended.");

	if (ret != -1) {
		int seconds = (int)(elapsed - time_start) / 1000;
		all_complete_message_handler(NULL);
		bc_tester_printf(bc_printf_verbosity_info, "Full parallel run completed in %2i mn %2i s.\n", seconds / 60,
		                 seconds % 60);
		return ret > max_failed_tests_threshold;
	}
	return -1;
}

#endif

int bc_tester_run_tests(const char *suite_name, const char *test_name, const char *tag_name) {
	int ret = 0;

	if ((ret = bc_tester_register_suites()) != 0) {
		return ret;
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

	if (xml_enabled == 1 && !test_name) {
		char *xml_file_name;                 //, *xml_file_name_tmp;
		CU_automated_enable_junit_xml(TRUE); /* this requires 3.0.1 because previous versions crash automated.c */

		if (run_in_parallel != 0) {
			if (suite_name) {
				// Sub-process started by parent in bc_tester_run_parallel
				CU_automated_enable_partial_junit(TRUE);
				xml_file_name = get_junit_xml_file_name(NULL, NULL);
				CU_set_output_filename(xml_file_name);
				bctbx_free(xml_file_name);
				CU_automated_run_tests();
			} else { // Starting registered suites in parallel
				ret = bc_tester_run_parallel();
				xml_file_name = get_junit_xml_file_name(NULL, "-Results.xml");
				merge_junit_xml_files(xml_file_name);
				bctbx_free(xml_file_name);
				if (log_file_name) {
					merge_log_files(log_file_name);
				}
				return ret;
			}
		} else { // Classic, non-parallel run
			xml_file_name = get_junit_xml_file_name(NULL, NULL);
			CU_set_output_filename(xml_file_name);
			CU_automated_run_tests();
			bctbx_free(xml_file_name);
		}
		goto end;
	}

#ifndef HAVE_CU_GET_SUITE
	if (suite_name) {
		bc_tester_printf(bc_printf_verbosity_info,
		                 "Tester compiled without CU_get_suite() function, running all tests instead of suite '%s'",
		                 suite_name);
	}
#else
	if (suite_name) {
		CU_pSuite suite;
		suite = CU_get_suite(suite_name);
		if (!suite) {
			if (tag_name != NULL) {
				bc_tester_printf(
				    bc_printf_verbosity_error,
				    "Could not find suite '%s' or this suite has no tests with tag '%s'. Available suites are:",
				    suite_name, tag_name);
			} else {
				bc_tester_printf(bc_printf_verbosity_error,
				                 "Could not find suite '%s'. Available suites are:", suite_name);
			}
			bc_tester_list_suites();
			return -1;
		} else if (test_name) {
			CU_pTest test = CU_get_test_by_name(test_name, suite);
			if (!test) {
				if (tag_name != NULL) {
					bc_tester_printf(
					    bc_printf_verbosity_error,
					    "Could not find test '%s' in suite '%s' or this test is not tagged '%s'. Available tests are:",
					    test_name, suite_name, tag_name);
				} else {
					bc_tester_printf(bc_printf_verbosity_error,
					                 "Could not find test '%s' in suite '%s'. Available tests are:", test_name,
					                 suite_name);
				}
				// do not use suite_name here, since this method is case sensitive
				bc_tester_list_tests(suite->pName);
				return -2;
			} else {
				CU_ErrorCode err = CU_run_test(suite, test);
				if (err != CUE_SUCCESS) bc_tester_printf(bc_printf_verbosity_error, "CU_basic_run_test error %d", err);
			}
		} else {
			CU_run_suite(suite);
		}
	} else
#endif
	{
#ifdef HAVE_CU_CURSES
		if (curses) {
			/* Run tests using the BCUnit curses interface */
			CU_curses_run_tests();
		} else
#endif
		{
			/* Run all tests using the BCUnit Basic interface */
			CU_run_all_tests();
		}
	}
end:
#ifdef __linux__
#if defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 33)
	bc_tester_printf(bc_printf_verbosity_info, "Still %i kilobytes allocated when all tests are finished.",
	                 mallinfo2().uordblks / 1024);
#else
	bc_tester_printf(bc_printf_verbosity_info, "Still %i kilobytes allocated when all tests are finished.",
	                 mallinfo().uordblks / 1024);
#endif
#else
	bc_tester_printf(bc_printf_verbosity_info, "Still %i kilobytes allocated when all tests are finished.",
	                 mallinfo().uordblks / 1024);
#endif
#endif
	if (run_in_parallel) {
		// We are a child process, return the number of test failed.
		int failed_tests = CU_get_number_of_tests_failed();
		bc_tester_printf(bc_printf_verbosity_info, "Suite  %s finished with %i failed tests.", suite_name,
		                 failed_tests);
		if (failed_tests >= 255) {
			bc_tester_printf(bc_printf_verbosity_error,
			                 "The number of tests exceeded 255, the maximum value for an exit status !");
			failed_tests = 255;
		}
		return failed_tests;
	}
	/* Otherwise it is serialized execution.*/
	return (int)CU_get_number_of_tests_failed() > (int)max_failed_tests_threshold;
}

#if !defined(BC_TESTER_WINDOWS_PHONE) && !defined(BC_TESTER_WINDOWS_UNIVERSAL) && !defined(__QNX__) &&                 \
    !defined(__ANDROID__) && !defined(IOS)
static int file_exists(const char *root_path) {
	char *res_path = bc_sprintf("%s/%s", root_path, expected_res);
	int err = bctbx_file_exist(res_path);
	bctbx_free(res_path);
	return err == 0;
}
#endif

static void detect_res_prefix(const char *prog) {
	char *progpath = NULL;
	char *progname = NULL;
	FILE *writable_file = NULL;

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
#ifdef BC_TESTER_WINDOWS_DESKTOP
			char *exe = strstr(prog, ".exe");
			if (exe != NULL) exe[0] = '\0';
#endif
			progname = strdup(ptr + 1);
		}
	}
#if !defined(BC_TESTER_WINDOWS_PHONE) && !defined(BC_TESTER_WINDOWS_UNIVERSAL) && !defined(__QNX__) &&                 \
    !defined(__ANDROID__) && !defined(IOS)
	{
		char *prefix = NULL;
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
			// for autotools, binary is in .libs/ subdirectory
			char *progpath2 = bc_sprintf("%s/../", progpath);
			if (file_exists(progpath)) {
				prefix = strdup(progpath);
			} else if (file_exists(progpath2)) {
				prefix = strdup(progpath2);
			}
			bctbx_free(progpath2);
		}
		if (installed_resources_path != NULL) bctbx_free(installed_resources_path);

		if (bc_tester_resource_dir_prefix != NULL && !file_exists(bc_tester_resource_dir_prefix)) {
			bc_tester_printf(bc_printf_verbosity_error,
			                 "Invalid provided resource directory: could not find expected resources '%s' in '%s'.",
			                 expected_res, bc_tester_resource_dir_prefix);
			bctbx_free(bc_tester_resource_dir_prefix);
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
			bctbx_free(prefix);
		}
	}
#endif

	// check that we can write in writable directory
	if (bc_tester_writable_dir_prefix != NULL) {
		char *writable_file_path = bc_sprintf("%s/%s", bc_tester_writable_dir_prefix, ".bc_tester_utils.tmp");
		writable_file = fopen(writable_file_path, "w");
		if (writable_file) {
			fclose(writable_file);
			unlink(writable_file_path);
		}
		bctbx_free(writable_file_path);
	}
	if (bc_tester_resource_dir_prefix == NULL || writable_file == NULL) {
		if (bc_tester_resource_dir_prefix == NULL) {
			bc_tester_printf(
			    bc_printf_verbosity_error,
			    "Could not find resource directory '%s' in '%s'! Please try again using option --resource-dir.",
			    expected_res, progpath);
		}
		if (writable_file == NULL) {
			bc_tester_printf(bc_printf_verbosity_error,
			                 "Failed to write file in %s. Please try again using option --writable-dir.",
			                 bc_tester_writable_dir_prefix);
		}
		abort();
	}

	if (progpath != NULL) {
		bctbx_free(progpath);
	}
	if (progname) {
		bctbx_free(progname);
	}
}

// Default function for the `--verbose`cli option
int bc_tester_verbose_handler(BCTBX_UNUSED(const char *arg)) {
	bctbx_set_log_level(BCTBX_LOG_DOMAIN, BCTBX_LOG_DEBUG);
	return 0;
}

// Default function for the `--silent` cli option
int bc_tester_silent_handler(BCTBX_UNUSED(const char *arg)) {
	bctbx_set_log_level(BCTBX_LOG_DOMAIN, BCTBX_LOG_FATAL);
	return 0;
}

// Default function for the `--log-file`cli option
int bc_tester_logfile_handler(const char *arg) {
	int res = 0;
	char *dir = bctbx_dirname(arg);
	char *base = bctbx_basename(arg);
	bctbx_log_handler_t *filehandler = bctbx_create_file_log_handler(0, dir, base);
	if (filehandler == NULL) {
		res = -1;
		goto end;
	}
	bctbx_add_log_handler(filehandler);

end:
	bctbx_free(dir);
	bctbx_free(base);
	return res;
}

void bc_tester_init(void (*ftester_printf)(int level, const char *format, va_list args),
                    int iverbosity_info,
                    int iverbosity_error,
                    const char *aexpected_res) {
	// Set default cli arguments handlers for --silent, --verbose, --log-file if undefined
	if (silent_arg_func == NULL) {
		silent_arg_func = bc_tester_silent_handler;
	}
	if (verbose_arg_func == NULL) {
		verbose_arg_func = bc_tester_verbose_handler;
	}
	if (logfile_arg_func == NULL) {
		logfile_arg_func = bc_tester_logfile_handler;
	}
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

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif // _MSC_VER
void bc_tester_set_max_vm(size_t amax_vm_kb) {
#ifdef __linux__
	max_vm_kb = (size_t)amax_vm_kb;
	bc_tester_printf(bc_printf_verbosity_info, "Maximum virtual memory space set to %li kilo bytes", max_vm_kb);
#else
	bc_tester_printf(bc_printf_verbosity_error, "Maximum virtual memory space setting is only implemented on Linux.");
#endif
}
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif // _MSC_VER

void bc_tester_helper(const char *name, const char *additionnal_helper) {
	bc_tester_printf(bc_printf_verbosity_info,
	                 "%s --help\n"
#ifdef HAVE_CU_CURSES
	                 "\t\t\t--curses\n"
#endif
	                 "\t\t\t--verbose\n"
	                 "\t\t\t--silent\n"
	                 "\t\t\t--log-file <output log file path>\n"
	                 "\t\t\t--list-suites\n"
	                 "\t\t\t--list-tests <suite>\n"
	                 "\t\t\t--suite <suite name> (this option can be set several times)\n"
	                 "\t\t\t--exclude-suite <suite name> (this option can be set several times)\n"
	                 "\t\t\t--test <test name>\n"
	                 "\t\t\t--tag <tag name> (execute all tests with the given tag - use '!' before tag name to "
	                 "exclude test with given tag)\n"
	                 "\t\t\t--all (execute all tests, even the ones with the Skip flag)\n"
	                 "\t\t\t--resource-dir <folder path> (directory where tester resource are located)\n"
	                 "\t\t\t--writable-dir <folder path> (directory where temporary files should be created)\n"
	                 "\t\t\t--xml\n"
	                 "\t\t\t--xml-file <xml file name>\n"
	                 "\t\t\t--max-alloc <size in ko> (maximum amount of memory obtained via malloc allocator)\n"
	                 "\t\t\t--max-alloc <size in ko> (maximum amount of memory obtained via malloc allocator)\n"
	                 "\t\t\t--parallel (Execute tests concurrently and with JUnit report)\n"
	                 "\t\t\t--parallel-max (Number Max of parallel processes)\n"
	                 "\t\t\t--timeout <timeout in minutes> (sets the global timeout when used alongside to the "
	                 "parallel option, the default value is 60)\n"
	                 "And additionally:\n"
	                 "%s",
	                 name, additionnal_helper);
}

int bc_tester_parse_args(int argc, char **argv, int argid) {
	int ret = 0;
	int i = argid;

	if (strcmp(argv[i], "--help") == 0) {
		return -1;
	} else if (strcmp(argv[i], "--log-file") == 0) {
		CHECK_ARG("--log-file", ++i, argc);
		ret = logfile_arg_func(argv[i]);
		if (ret < 0) return ret;
		log_file_name = argv[i];
	} else if (strcmp(argv[i], "--silent") == 0) {
		ret = silent_arg_func(argv[i]);
		if (ret < 0) return ret;
	} else if (strcmp(argv[i], "--verbose") == 0) {
		ret = verbose_arg_func(argv[i]);
		if (ret < 0) return ret;
	} else if (strcmp(argv[i], "--test") == 0) {
		CHECK_ARG("--test", ++i, argc);
		test_name = argv[i];
	} else if (strcmp(argv[i], "--suite") == 0) {
		CHECK_ARG("--suite", ++i, argc);
		suites_to_run = bctbx_list_append(suites_to_run, argv[i]);
	} else if (strcmp(argv[i], "--exclude-suite") == 0) {
		CHECK_ARG("--exclude-suite", ++i, argc);
		suites_to_exclude = bctbx_list_append(suites_to_exclude, argv[i]);
	} else if (strcmp(argv[i], "--tag") == 0) {
		CHECK_ARG("--tag", ++i, argc);
		tags_to_run = bctbx_list_append(tags_to_run, argv[i]);
	} else if (strcmp(argv[i], "--all") == 0) {
		run_skipped_tests = 1;
	} else if (strcmp(argv[i], "--list-suites") == 0) {
		bc_tester_list_suites();
		return 0;
	} else if (strcmp(argv[i], "--list-tests") == 0) {
		CHECK_ARG("--list-tests", ++i, argc);
		const char *suite_name = argv[i];
		bc_tester_list_tests(suite_name);
		return 0;
	} else if (strcmp(argv[i], "--xml-file") == 0) {
		CHECK_ARG("--xml-file", ++i, argc);
		xml_file = argv[i];
		xml_enabled = 1;
	} else if (strcmp(argv[i], "--xml") == 0) {
		xml_enabled = 1;
	} else if (strcmp(argv[i], "--parallel") == 0) {
		// Keep record of cli args for subprocesses
		if (!origin_argv) {
			origin_argv = argv;
			origin_argc = argc;
		}
		// Defaults to JUnit report if parallel is enabled
		xml_enabled = 1;
		run_in_parallel = 1;
	} else if (strcmp(argv[i], "--parallel-max") == 0) {
		CHECK_ARG("--parallel-max", ++i, argc);
		bc_tester_set_max_parallel_suites(atoi(argv[i]));
	} else if (strcmp(argv[i], "--timeout") == 0) {
		CHECK_ARG("--timeout", ++i, argc);
		globalTimeout = atoi(argv[i]) * 60;
	} else if (strcmp(argv[i], "--max-alloc") == 0) {
		CHECK_ARG("--max-alloc", ++i, argc);
		max_vm_kb = atol(argv[i]);
	} else if (strcmp(argv[i], "--resource-dir") == 0) {
		CHECK_ARG("--resource-dir", ++i, argc);
		if (bc_tester_resource_dir_prefix) bctbx_free(bc_tester_resource_dir_prefix);
		bc_tester_resource_dir_prefix = strdup(argv[i]);
	} else if (strcmp(argv[i], "--writable-dir") == 0) {
		CHECK_ARG("--writable-dir", ++i, argc);
		if (bc_tester_writable_dir_prefix) bctbx_free(bc_tester_writable_dir_prefix);
		bc_tester_writable_dir_prefix = strdup(argv[i]);
	} else if (strcmp(argv[i], "--child") == 0) { // Switch off this parameter as it is used for external processing
	} else {
		bc_tester_printf(bc_printf_verbosity_error, "Unknown option \"%s\"", argv[i]);
		return -1;
	}
	/* returns number of arguments read + 1 */
	return i - argid + 1;
}

// Init BCUnit and register suites and/or tests into internal BCUnit registry before actual BCUnit test launch
int bc_tester_register_suites(void) {
	// Assume everything is already setup if BCUnit registry exists
	if (CU_registry_initialized()) {
		return 0;
	}
	if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();

	if (suites_to_run && suites_to_exclude) {
		bc_tester_printf(bc_printf_verbosity_error,
		                 "You cannot supply --suite and --exclude-suite options simultaenously.");
		return -1;
	}

	if (suites_to_run != NULL) {
		const bctbx_list_t *elem;
		for (elem = suites_to_run; elem != NULL; elem = elem->next) {
			const char *suite_name = (const char *)elem->data;
			int suiteIdx = bc_tester_suite_index(suite_name);
			if (suiteIdx == -1) {
				bc_tester_printf(bc_printf_verbosity_error,
				                 "Suite with name \"%s\" not found. Available suites are: ", suite_name);
				bc_tester_list_suites();
				return -1;
			}
			bc_tester_register_suite(test_suite[suiteIdx], tags_to_run);
		}
	} else {
		int i;
		for (i = 0; i < nb_test_suites; i++) {
			const bctbx_list_t *elem;
			bool_t excluded = FALSE;
			for (elem = suites_to_exclude; elem != NULL; elem = elem->next) {
				const char *suite_name = (const char *)elem->data;
				if (strcasecmp(suite_name, test_suite[i]->name) == 0) {
					excluded = TRUE;
				}
			}
			if (!excluded) bc_tester_register_suite(test_suite[i], tags_to_run);
		}
	}
	return 0;
}

int bc_tester_start(const char *prog_name) {
	int ret;
	const char *suite_name = NULL;

	if (expected_res) detect_res_prefix(prog_name);

	if (max_vm_kb) bc_tester_set_max_vm(max_vm_kb);

	if (bctbx_list_size(suites_to_run) == 1) {
		suite_name = (const char *)suites_to_run->data;
	} else if (test_name != NULL) {
		bc_tester_printf(bc_printf_verbosity_error, "--test requires a unique --suite option to be given. ");
		return -1;
	}

	ret = bc_tester_run_tests(suite_name, test_name, tag_name);

	return ret;
}

void bc_tester_add_suite(test_suite_t *suite) {
	if (test_suite == NULL) {
		test_suite = (test_suite_t **)malloc(10 * sizeof(test_suite_t *));
	}
	int insert_index = 0;
	while (insert_index < nb_test_suites && test_suite[insert_index]->average_time >= suite->average_time)
		++insert_index;                  // if suite->average_time == 0 => the new suite will be at the end.
	if (insert_index < nb_test_suites) { // Need insertion
		for (int move_index = nb_test_suites; move_index >= insert_index + 1; --move_index)
			test_suite[move_index] = test_suite[move_index - 1];
	}
	test_suite[insert_index] = suite;
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
	if (CU_get_number_of_failure_records()) {
		CU_basic_show_failures(CU_get_failure_list());
	}
#endif
	CU_cleanup_registry();

	/*add missing final newline*/
	bc_tester_printf(bc_printf_verbosity_info, "");

	if (test_suite != NULL) {
		bctbx_free(test_suite);
		test_suite = NULL;
		nb_test_suites = 0;
	}

	if (bc_tester_resource_dir_prefix != NULL) {
		bctbx_free(bc_tester_resource_dir_prefix);
		bc_tester_resource_dir_prefix = NULL;
	}
	if (bc_tester_writable_dir_prefix != NULL) {
		bctbx_free(bc_tester_writable_dir_prefix);
		bc_tester_writable_dir_prefix = NULL;
	}
	if (suites_to_run) {
		bctbx_list_free(suites_to_run);
		suites_to_run = NULL;
	}
	if (suites_to_exclude) {
		bctbx_list_free(suites_to_exclude);
		suites_to_exclude = NULL;
	}
}

static void bc_tester_set_dir_prefix(char **prefix, const char *name) {
	if (*prefix != NULL) bctbx_free(*prefix);
	*prefix = strdup(name);
}

const char *bc_tester_get_resource_dir_prefix(void) {
	return bc_tester_resource_dir_prefix;
}

void bc_tester_set_resource_dir_prefix(const char *name) {
	bc_tester_set_dir_prefix(&bc_tester_resource_dir_prefix, name);
}

const char *bc_tester_get_writable_dir_prefix(void) {
	return bc_tester_writable_dir_prefix;
}

void bc_tester_set_writable_dir_prefix(const char *name) {
	bc_tester_set_dir_prefix(&bc_tester_writable_dir_prefix, name);
}

static char *bc_tester_path(const char *prefix, const char *name) {
	if (name) {
		return bc_sprintf("%s/%s", prefix, name);
	} else {
		return NULL;
	}
}

char *bc_tester_res(const char *name) {
	return bc_tester_path(bc_tester_resource_dir_prefix, name);
}

char *bc_tester_file(const char *name) {
	return bc_tester_path(bc_tester_writable_dir_prefix, name);
}

char *bc_sprintfva(const char *format, va_list args) {
	/* Guess we need no more than 100 bytes. */
	int n, size = 200;
	char *p, *np;
#ifndef _WIN32
	va_list cap; /*copy of our argument list: a va_list cannot be re-used (SIGSEGV on linux 64 bits)*/
#endif
	if ((p = malloc(size)) == NULL) return NULL;
	while (1) {
		/* Try to print in the allocated space. */
#ifndef _WIN32
		va_copy(cap, args);
		n = vsnprintf(p, size, format, cap);
		va_end(cap);
#else
		/*this works on 32 bits, luckily*/
		n = vsnprintf(p, size, format, args);
#endif
		/* If that worked, return the string. */
		if (n > -1 && n < size) return p;
		// bc_tester_printf(bc_printf_verbosity_error, "Reallocing space.");
		/* Else try again with more space. */
		if (n > -1)                     /* glibc 2.1 */
			size = n + 1;               /* precisely what is needed */
		else /* glibc 2.0 */ size *= 2; /* twice the old size */
		if ((np = realloc(p, size)) == NULL) {
			bctbx_free(p);
			return NULL;
		} else {
			p = np;
		}
	}
}

char *bc_sprintf(const char *format, ...) {
	va_list args;
	char *res;
	va_start(args, format);
	res = bc_sprintfva(format, args);
	va_end(args);
	return res;
}

void bc_free(void *ptr) {
	free(ptr);
}

const char *bc_tester_current_suite_name(void) {
	return bc_current_suite_name;
}

const char *bc_tester_current_test_name(void) {
	return bc_current_test_name;
}

const char **bc_tester_current_test_tags(void) {
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

void bc_set_trace_handler(void (*handler)(int, const char *, va_list)) {
#ifdef HAVE_CU_SET_TRACE_HANDLER
	CU_set_trace_handler(handler);
#else
	bc_tester_printf(bc_printf_verbosity_error, "CU_set_trace_handler not implemented");
#endif
}

int bc_assert(const char *file, int line, int predicate, const char *format) {
	if (!predicate) bc_tester_printf(bc_printf_verbosity_info, format, NULL);
	return CU_assertImplementation(predicate, line, format, file, "", FALSE);
}
