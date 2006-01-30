/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2004,2005,2006  Jerry St.Clair
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  Support for unit tests of CUnit framework
 *
 *  12-Aug-2004   Initial implementation. (JDS)
 */

/** @file
 * CUnit internal testingfunctions (implementation).
 */
/** @addtogroup Internal
 @{
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "CUnit.h"
#include "MyMem.h"
#include "Util.h"
#include "test_cunit.h"

static unsigned int f_nTests = 0;
static unsigned int f_nFailures = 0;
static unsigned int f_nTests_stored = 0;
static unsigned int f_nFails_stored = 0;
static clock_t      f_start_time;

static void test_cunit_initialize(void);
static void test_cunit_report_results(void);

int main()
{
  /* No line buffering. */
  setbuf(stdout, NULL);

  test_cunit_initialize();
  fprintf(stdout, "\nTesting CUnit internals...");

	/* individual module test functions go here */
  test_cunit_CUError();
  test_cunit_MyMem();
  test_cunit_TestDB();
  test_cunit_TestRun();
  test_cunit_Util();

  test_cunit_report_results();
  CU_cleanup_registry();

	return 0;
}

void test_cunit_start_tests(const char* strName)
{
  fprintf(stdout, "\n\ttesting %s ...", strName);
  f_nTests_stored = f_nTests;
  f_nFails_stored = f_nFailures;
}

void test_cunit_end_tests(void)
{
  fprintf(stdout, "\b\b\b - %d assertions, %d failures",
                  f_nTests - f_nTests_stored,
                  f_nFailures - f_nFails_stored);
}

void test_cunit_add_test(void)
{
  ++f_nTests;
}

void test_cunit_add_failure(void)
{
  ++f_nFailures;
}

unsigned int test_cunit_test_count(void)
{
  return f_nTests;
}

unsigned int test_cunit_failure_count(void)
{
  return f_nFailures;
}

void test_cunit_initialize(void)
{
  f_nTests = 0;
  f_nFailures = 0;
  f_start_time = clock();
}

void test_cunit_report_results(void)
{
  fprintf(stdout,
          "\n\n---------------------------"
          "\nCUnit Internal Test Results"
          "\n---------------------------"
          "\n  Total Number of Assertions: %d"
          "\n     Successes: %d"
          "\n     Failures:  %d"
        "\n\nTotal test time = %8.3f seconds.\n",
          f_nTests, f_nTests-f_nFailures, f_nFailures,
          ((double)clock() - (double)f_start_time)/(double)CLOCKS_PER_SEC);
}

CU_BOOL test_cunit_assert_impl(CU_BOOL value, 
                               const char* condition, 
                               const char* file, 
                               unsigned int line)
{
  test_cunit_add_test();
  if (CU_FALSE == value) {
    test_cunit_add_failure();
    printf("\nTEST FAILED: File '%s', Line %d, Condition '%s.'\n",
           file, line, condition);
  }
  return value;
}



