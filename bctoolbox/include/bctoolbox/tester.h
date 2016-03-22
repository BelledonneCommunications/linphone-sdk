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

#ifndef BCTOOLBOX_TESTER_H
#define BCTOOLBOX_TESTER_H

#if defined(_MSC_VER)
#define BCTOOLBOX_PUBLIC	__declspec(dllexport)
#else
#define BCTOOLBOX_PUBLIC
#endif

#ifndef TRUE
	#define TRUE 1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#ifndef snprintf
#define snprintf _snprintf
#define strcasecmp _stricmp
#endif
#ifndef strdup
#define strdup _strdup
#endif
#endif

typedef void (*test_function_t)(void);
/** Function used in all suites - it is invoked before all and each tests and also after each and all tests
  * @return 0 means success, otherwise it's an error
**/
typedef int (*pre_post_function_t)(void);
// typedef int (*test_suite_function_t)(const char *name);

typedef struct {
	const char *name;
	test_function_t func;
	const char *tags[2];
} test_t;

#define TEST_NO_TAG(name, func) \
	{ name, func, { NULL, NULL } }
#define TEST_ONE_TAG(name, func, tag) \
	{ name, func, { tag, NULL } }
#define TEST_TWO_TAGS(name, func, tag1, tag2) \
	{ name, func, { tag1, tag2 } }

typedef struct {
	const char *name; /*suite name*/
	pre_post_function_t before_all; /*function invoked before running the suite. If not returning 0, suite is not launched. */
	pre_post_function_t after_all; /*function invoked at the end of the suite, even if some tests failed. */
	test_function_t before_each;   /*function invoked before each test within this suite. */
	pre_post_function_t after_each;	/*function invoked after each test within this suite, even if it failed. */
	int nb_tests;				   /* number of tests */
	test_t *tests;				   /* tests within this suite */
} test_suite_t;

#ifdef __cplusplus
extern "C" {
#endif


#define CHECK_ARG(argument, index, argc)                      \
if(index >= argc) {                                           \
fprintf(stderr, "Missing argument for \"%s\"\n", argument);   \
return -1;                                                    \
}                                                             \

BCTOOLBOX_PUBLIC void bc_tester_init(void (*ftester_printf)(int level, const char *fmt, va_list args)
									, int verbosity_info, int verbosity_error, const char* expected_res);
BCTOOLBOX_PUBLIC void bc_tester_helper(const char *name, const char* additionnal_helper);
BCTOOLBOX_PUBLIC int bc_tester_parse_args(int argc, char** argv, int argid);
BCTOOLBOX_PUBLIC int bc_tester_start(const char* prog_name);
BCTOOLBOX_PUBLIC void bc_tester_add_suite(test_suite_t *suite);
BCTOOLBOX_PUBLIC void bc_tester_uninit(void);
BCTOOLBOX_PUBLIC void bc_tester_printf(int level, const char *fmt, ...);
BCTOOLBOX_PUBLIC const char * bc_tester_get_resource_dir_prefix(void);
BCTOOLBOX_PUBLIC void bc_tester_set_resource_dir_prefix(const char *name);
BCTOOLBOX_PUBLIC const char * bc_tester_get_writable_dir_prefix(void);
BCTOOLBOX_PUBLIC void bc_tester_set_writable_dir_prefix(const char *name);

BCTOOLBOX_PUBLIC int bc_tester_nb_suites(void);
BCTOOLBOX_PUBLIC int bc_tester_nb_tests(const char* name);
BCTOOLBOX_PUBLIC void bc_tester_list_suites(void);
BCTOOLBOX_PUBLIC void bc_tester_list_tests(const char *suite_name);
BCTOOLBOX_PUBLIC const char * bc_tester_suite_name(int suite_index);
BCTOOLBOX_PUBLIC const char * bc_tester_test_name(const char *suite_name, int test_index);
BCTOOLBOX_PUBLIC int bc_tester_run_suite(test_suite_t *suite, const char *tag_name);
BCTOOLBOX_PUBLIC int bc_tester_run_tests(const char *suite_name, const char *test_name, const char *tag_name);
BCTOOLBOX_PUBLIC int bc_tester_suite_index(const char *suite_name);
BCTOOLBOX_PUBLIC const char * bc_tester_current_suite_name(void);
BCTOOLBOX_PUBLIC const char * bc_tester_current_test_name(void);
BCTOOLBOX_PUBLIC const char ** bc_tester_current_test_tags(void);

BCTOOLBOX_PUBLIC char* bc_sprintfva(const char* format, va_list args);
BCTOOLBOX_PUBLIC char* bc_sprintf(const char* format, ...);
BCTOOLBOX_PUBLIC void bc_free(void *ptr);

BCTOOLBOX_PUBLIC unsigned int bc_get_number_of_failures(void);
BCTOOLBOX_PUBLIC void bc_set_trace_handler(void(*handler)(int, const char*, va_list));
/**
 * Get full path to the given resource
 *
 * @param name relative resource path
 * @return path to the resource. Must be freed by caller.
*/
BCTOOLBOX_PUBLIC char * bc_tester_res(const char *name);

/**
* Get full path to the given writable_file
*
* @param name relative writable file path
* @return path to the writable file. Must be freed by caller.
*/
BCTOOLBOX_PUBLIC char * bc_tester_file(const char *name);


/*Redefine the CU_... macros WITHOUT final ';' semicolon, to allow IF conditions and smarter error message */
extern int CU_assertImplementation(int bValue,
                                      unsigned int uiLine,
                                      const char *strCondition,
                                      const char *strFile,
                                      const char *strFunction,
                                      int bFatal);

BCTOOLBOX_PUBLIC int bc_assert(const char* file, int line, int predicate, const char* format, int fatal);

#define _BC_ASSERT_PRED(name, pred, actual, expected, type, fatal, ...) \
	do { \
		char format[4096] = {0}; \
		type cactual = (actual); \
		type cexpected = (expected); \
		snprintf(format, 4096, name "(" #actual ", " #expected ") - " __VA_ARGS__); \
		bc_assert(__FILE__, __LINE__, pred, format, fatal); \
	} while (0)

#define BC_PASS(msg) bc_assert(__FILE__, __LINE__, TRUE, "BC_PASS(" #msg ").", FALSE)
#define BC_FAIL(msg) bc_assert(__FILE__, __LINE__, FALSE, "BC_FAIL(" #msg ").", FALSE)
#define BC_ASSERT(value) bc_assert(__FILE__, __LINE__, (value), #value, FALSE)
#define BC_ASSERT_FATAL(value) bc_assert(__FILE__, __LINE__, (value), #value, TRUE)
#define BC_TEST(value) bc_assert(__FILE__, __LINE__, (value), #value, FALSE)
#define BC_TEST_FATAL(value) bc_assert(__FILE__, __LINE__, (value), #value, TRUE)
#define BC_ASSERT_TRUE(value) bc_assert(__FILE__, __LINE__, (value), ("BC_ASSERT_TRUE(" #value ")"), FALSE)
#define BC_ASSERT_TRUE_FATAL(value) bc_assert(__FILE__, __LINE__, (value), ("BC_ASSERT_TRUE_FATAL(" #value ")"), TRUE)
#define BC_ASSERT_FALSE(value) bc_assert(__FILE__, __LINE__, !(value), ("BC_ASSERT_FALSE(" #value ")"), FALSE)
#define BC_ASSERT_FALSE_FATAL(value) bc_assert(__FILE__, __LINE__, !(value), ("BC_ASSERT_FALSE_FATAL(" #value ")"), TRUE)
#define BC_ASSERT_EQUAL(actual, expected, type, type_format) _BC_ASSERT_PRED("BC_ASSERT_EQUAL", ((cactual) == (cexpected)), actual, expected, type, FALSE, "Expected " type_format " but was " type_format ".", cexpected, cactual)
#define BC_ASSERT_EQUAL_FATAL(actual, expected, type, type_format) _BC_ASSERT_PRED("BC_ASSERT_EQUAL_FATAL", ((cactual) == (cexpected)), actual, expected, type, TRUE, "Expected " type_format " but was " type_format ".", cexpected, cactual)
#define BC_ASSERT_NOT_EQUAL(actual, expected, type, type_format) _BC_ASSERT_PRED("BC_ASSERT_NOT_EQUAL", ((cactual) != (cexpected)), actual, expected, type, FALSE, "Expected NOT " type_format " but it was.", cexpected)
#define BC_ASSERT_NOT_EQUAL_FATAL(actual, expected, type, type_format) _BC_ASSERT_PRED("BC_ASSERT_NOT_EQUAL_FATAL", ((cactual) != (cexpected)), actual, expected, type, TRUE, "Expected NOT " type_format " but it was.", cexpected)
#define BC_ASSERT_PTR_EQUAL(actual, expected) _BC_ASSERT_PRED("BC_ASSERT_PTR_EQUAL", ((cactual) == (cexpected)), (const void*)(actual), (const void*)(expected), const void*, FALSE, "Expected %p but was %p.", cexpected, cactual)
#define BC_ASSERT_PTR_EQUAL_FATAL(actual, expected) _BC_ASSERT_PRED("BC_ASSERT_PTR_EQUAL_FATAL", ((cactual) == (cexpected)), (const void*)(actual), (const void*)(expected), const void*, TRUE, "Expected %p but was %p.", cexpected, cactual)
#define BC_ASSERT_PTR_NOT_EQUAL(actual, expected) _BC_ASSERT_PRED("BC_ASSERT_PTR_NOT_EQUAL", ((cactual) != (cexpected)), (const void*)(actual), (const void*)(expected), const void*, FALSE, "Expected NOT %p but it was.", cexpected)
#define BC_ASSERT_PTR_NOT_EQUAL_FATAL(actual, expected) _BC_ASSERT_PRED("BC_ASSERT_PTR_NOT_EQUAL_FATAL", ((cactual) != (cexpected)), (const void*)(actual), (const void*)(expected), const void*, TRUE, "Expected NOT %p but it was.", cexpected)
#define BC_ASSERT_PTR_NULL(value) _BC_ASSERT_PRED("BC_ASSERT_PTR_NULL", ((cactual) == (cexpected)), (const void*)(value), NULL, const void*, FALSE, "Expected NULL but was %p.", cactual)
#define BC_ASSERT_PTR_NULL_FATAL(value) _BC_ASSERT_PRED("BC_ASSERT_PTR_NULL_FATAL", ((cactual) == (cexpected)), (const void*)(value), NULL, const void*, TRUE, "Expected NULL but was %p.", cactual)
#define BC_ASSERT_PTR_NOT_NULL(value) _BC_ASSERT_PRED("BC_ASSERT_PTR_NOT_NULL", ((cactual) != (cexpected)), (const void*)(value), NULL, const void*, FALSE, "Expected NOT NULL but it was.")
#define BC_ASSERT_PTR_NOT_NULL_FATAL(value) _BC_ASSERT_PRED("BC_ASSERT_PTR_NOT_NULL_FATAL", ((cactual) != (cexpected)), (const void*)(value), NULL, const void*, TRUE, "Expected NOT NULL but it was.")
#define BC_ASSERT_STRING_EQUAL(actual, expected) _BC_ASSERT_PRED("BC_ASSERT_STRING_EQUAL", !(strcmp((const char*)(cactual), (const char*)(cexpected))), actual, expected, const char*, FALSE, "Expected %s but was %s.", cexpected, cactual)
#define BC_ASSERT_STRING_EQUAL_FATAL(actual, expected) _BC_ASSERT_PRED("BC_ASSERT_STRING_EQUAL_FATAL", !(strcmp((const char*)(cactual), (const char*)(cexpected))), actual, expected, const char*, TRUE, "Expected %s but was %s.", cexpected, cactual)
#define BC_ASSERT_STRING_NOT_EQUAL(actual, expected) _BC_ASSERT_PRED("BC_ASSERT_STRING_NOT_EQUAL", (strcmp((const char*)(cactual), (const char*)(cexpected))), actual, expected, const char*, FALSE, "Expected NOT %s but it was.", cexpected)
#define BC_ASSERT_STRING_NOT_EQUAL_FATAL(actual, expected) _BC_ASSERT_PRED("BC_ASSERT_STRING_NOT_EQUAL_FATAL", (strcmp((const char*)(cactual), (const char*)(cexpected))), actual, expected, const char*, TRUE, "Expected NOT %s but it was.", cexpected)
#define BC_ASSERT_NSTRING_EQUAL(actual, expected, count) _BC_ASSERT_PRED("BC_ASSERT_NSTRING_EQUAL", !(strncmp((const char*)(cactual), (const char*)(cexpected), (size_t)(count))), actual, expected, const char*, FALSE, "Expected %*s but was %*s.", (int)(count), cexpected, (int)(count), cactual)
#define BC_ASSERT_NSTRING_EQUAL_FATAL(actual, expected, count) _BC_ASSERT_PRED("BC_ASSERT_NSTRING_EQUAL_FATAL", !(strncmp((const char*)(cactual), (const char*)(cexpected), (size_t)(count))), actual, expected, const char*, TRUE, "Expected %*s but was %*s.", (int)count, cexpected, (int)count, cactual)
#define BC_ASSERT_NSTRING_NOT_EQUAL(actual, expected, count) _BC_ASSERT_PRED("BC_ASSERT_NSTRING_NOT_EQUAL", (strncmp((const char*)(cactual), (const char*)(cexpected), (size_t)(count))), actual, expected, const char*, FALSE, "Expected %*s but it was.", (int)count, cexpected)
#define BC_ASSERT_NSTRING_NOT_EQUAL_FATAL(actual, expected, count) _BC_ASSERT_PRED("BC_ASSERT_NSTRING_NOT_EQUAL_FATAL", (strncmp((const char*)(cactual), (const char*)(cexpected), (size_t)(count))), actual, expected, const char*, TRUE, "Expected %*s but it was.", (int)count, cexpected)
#define BC_ASSERT_DOUBLE_EQUAL(actual, expected, granularity) _BC_ASSERT_PRED("BC_ASSERT_DOUBLE_EQUAL", ((fabs((double)(cactual) - (cexpected)) <= fabs((double)(granularity)))), actual, expected, double, FALSE, "Expected %f but was %f.", cexpected, cactual)
#define BC_ASSERT_DOUBLE_EQUAL_FATAL(actual, expected, granularity) _BC_ASSERT_PRED("BC_ASSERT_DOUBLE_EQUAL_FATAL", ((fabs((double)(cactual) - (cexpected)) <= fabs((double)(granularity)))), actual, expected, double, TRUE, "Expected %f but was %f.", cexpected, cactual)
#define BC_ASSERT_DOUBLE_NOT_EQUAL(actual, expected, granularity) _BC_ASSERT_PRED("BC_ASSERT_DOUBLE_NOT_EQUAL", ((fabs((double)(cactual) - (cexpected)) > fabs((double)(granularity)))), actual, expected, double, FALSE, "Expected %f but was %f.", cexpected, cactual)
#define BC_ASSERT_DOUBLE_NOT_EQUAL_FATAL(actual, expected, granularity) _BC_ASSERT_PRED("BC_ASSERT_DOUBLE_NOT_EQUAL_FATAL", ((fabs((double)(cactual) - (cexpected)) > fabs((double)(granularity)))), actual, expected, double, TRUE, "Expected %f but was %f.", cexpected, cactual)
#define BC_ASSERT_GREATER(actual, min, type, type_format) _BC_ASSERT_PRED("BC_ASSERT_GREATER", ((cactual) >= (cexpected)), actual, min, type, FALSE, "Expected at least " type_format " but was " type_format ".", cexpected, cactual)
#define BC_ASSERT_LOWER(actual, max, type, type_format) _BC_ASSERT_PRED("BC_ASSERT_LOWER", ((cactual) <= (cexpected)), actual, max, type, FALSE, "Expected at most " type_format " but was " type_format ".", cexpected, cactual)
#define BC_ASSERT_GREATER_STRICT(actual, min, type, type_format) _BC_ASSERT_PRED("BC_ASSERT_GREATER", ((cactual) > (cexpected)), actual, min, type, FALSE, "Expected more than " type_format " but was " type_format ".", cexpected, cactual)
#define BC_ASSERT_LOWER_STRICT(actual, max, type, type_format) _BC_ASSERT_PRED("BC_ASSERT_LOWER", ((cactual) < (cexpected)), actual, max, type, FALSE, "Expected less than " type_format " but was " type_format ".", cexpected, cactual)

#ifdef __cplusplus
}
#endif
#endif
