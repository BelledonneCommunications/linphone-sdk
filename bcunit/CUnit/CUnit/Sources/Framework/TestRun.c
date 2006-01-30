/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001            Anil Kumar
 *  Copyright (C) 2004,2005,2006  Anil Kumar, Jerry St.Clair
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
 *  Implementation of Console Test Interface.
 *
 *  Aug 2001      Initial implementaion (AK)
 *
 *  19/Aug/2001   Added initial registry/Suite/test framework implementation. (AK)
 *
 *  24/Aug/2001   Changed Data structure from SLL to DLL for all linked lists. (AK)
 *
 *  25/Nov/2001   Added notification for Suite Initialization failure condition. (AK)
 *
 *  5-Aug-2004    New interface, doxygen comments, moved add_failure on suite
 *                initialization so called even if a callback is not registered,
 *                moved CU_assertImplementation into TestRun.c, consolidated
 *                all run summary info out of CU_TestRegistry into TestRun.c,
 *                revised counting and reporting of run stats to cleanly
 *                differentiate suite, test, and assertion failures. (JDS)
 *
 *  1-Sep-2004    Modified CU_assertImplementation() and run_single_test() for
 *                setjmp/longjmp mechanism of aborting test runs, add asserts in
 *                CU_assertImplementation() to trap use outside a registered
 *                test function during an active test run. (JDS)
 *
 *  22-Sep-2004   Initial implementation of internal unit tests, added nFailureRecords
 *                to CU_Run_Summary, added CU_get_n_failure_records(), removed
 *                requirement for registry to be initialized in order to run
 *                CU_run_suite() and CU_run_test(). (JDS)
 *
 *  30-Apr-2005   Added callback for suite cleanup function failure, 
 *                updated unit tests. (JDS)
 */

/** @file
 *  Test run management functions (implementation).
 */
/** @addtogroup Framework
 @{
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>

#include "CUnit.h"
#include "MyMem.h"
#include "TestDB.h"
#include "TestRun.h"

static CU_BOOL   f_bTestIsRunning = CU_FALSE; /**< Flag for whether a test run is in progress */
static CU_pSuite f_pCurSuite = NULL;          /**< Pointer to the suite currently being run. */
static CU_pTest  f_pCurTest  = NULL;          /**< Pointer to the test currently being run. */

/** CU_RunSummary to hold results of each test run. */
static CU_RunSummary f_run_summary = {0, 0, 0, 0, 0, 0, 0};
/** CU_pFailureRecord to hold head of failure record list of each test run. */
static CU_pFailureRecord f_failure_list = NULL;
/** CU_pFailureRecord to hold head of failure record list of each test run. */
static CU_pFailureRecord f_last_failure = NULL;

/* Forward declarations of static functions. */
static void         clear_previous_results(CU_pRunSummary pRunSummary, CU_pFailureRecord* ppFailure);
static void         cleanup_failure_list(CU_pFailureRecord* ppFailure);
static CU_ErrorCode run_single_suite(CU_pSuite pSuite, CU_pRunSummary pRunSummary);
static CU_ErrorCode run_single_test(CU_pTest pTest, CU_pRunSummary pRunSummary);
static void         add_failure(CU_pFailureRecord* ppFailure,
                                CU_pRunSummary pRunSummary,
                                unsigned int uiLineNumber,
                                char szCondition[],
                                char szFileName[],
                                CU_pSuite pSuite,
                                CU_pTest pTest);

/** Pointer to the function to be called before running a test. */
static CU_TestStartMessageHandler           f_pTestStartMessageHandler = NULL;
/** Pointer to the function to be called after running a test. */
static CU_TestCompleteMessageHandler        f_pTestCompleteMessageHandler = NULL;
/** Pointer to the function to be called when all tests have been run. */
static CU_AllTestsCompleteMessageHandler    f_pAllTestsCompleteMessageHandler = NULL;
/** Pointer to the function to be called if a suite initialization function returns an error. */
static CU_SuiteInitFailureMessageHandler    f_pSuiteInitFailureMessageHandler = NULL;
/** Pointer to the function to be called if a suite cleanup function returns an error. */
static CU_SuiteCleanupFailureMessageHandler f_pSuiteCleanupFailureMessageHandler = NULL;

/*------------------------------------------------------------------------*/
/** Assertion implementation function.
 *  All CUnit assertions reduce to a call to this function.
 *  It should only be called during an active test run (checked
 *  by assertion).  This means that CUnit assertions should only
 *  be used in registered test functions during a test run.
 *  @param bValue        Value of the assertion (CU_TRUE or CU_FALSE).
 *  @param uiLine        Line number of failed test statement.
 *  @param strCondition  String containing logical test that failed.
 *  @param strFile       Source file where test statement failed.
 *  @param strFunction   Function where test statement failed.
 *  @param bFatal        CU_TRUE to abort test (via longjmp()), CU_FALSE to continue test.
 *  @return As a convenience, returns the value of the assertion.
 */
CU_BOOL CU_assertImplementation(CU_BOOL bValue, 
                                unsigned int uiLine,
                                char strCondition[], 
                                char strFile[],
                                char strFunction[], 
                                CU_BOOL bFatal)
{
  /* not used in current implementation - stop compiler warning */
  CU_UNREFERENCED_PARAMETER(strFunction);

  /* these should always be non-NULL (i.e. a test run is in progress) */
  assert(NULL != f_pCurSuite);
  assert(NULL != f_pCurTest);

  ++f_run_summary.nAsserts;
  if (CU_FALSE == bValue) {
    ++f_run_summary.nAssertsFailed;
    add_failure(&f_failure_list, &f_run_summary,
                uiLine, strCondition, strFile, f_pCurSuite, f_pCurTest);

    if ((CU_TRUE == bFatal) && (NULL != f_pCurTest->pJumpBuf)) {
      longjmp(*(f_pCurTest->pJumpBuf), 1);
    }
  }

  return bValue;
}

/*
 * Get/Set functions for Message Handlers.
 */
/*------------------------------------------------------------------------*/
/** Set the message handler to call before each test is run. */
void CU_set_test_start_handler(CU_TestStartMessageHandler pTestStartHandler)
{
  f_pTestStartMessageHandler = pTestStartHandler;
}

/*------------------------------------------------------------------------*/
/** Set the message handler to call after each test is run. */
void CU_set_test_complete_handler(CU_TestCompleteMessageHandler pTestCompleteHandler)
{
  f_pTestCompleteMessageHandler = pTestCompleteHandler;
}

/*------------------------------------------------------------------------*/
/** Set the message handler to call after all tests have been run. */
void CU_set_all_test_complete_handler(CU_AllTestsCompleteMessageHandler pAllTestsCompleteHandler)
{
  f_pAllTestsCompleteMessageHandler = pAllTestsCompleteHandler;
}

/*------------------------------------------------------------------------*/
/** Set the message handler to call when a suite
 *  initialization function returns an error.
 */
void CU_set_suite_init_failure_handler(CU_SuiteInitFailureMessageHandler pSuiteInitFailureHandler)
{
  f_pSuiteInitFailureMessageHandler = pSuiteInitFailureHandler;
}

/*------------------------------------------------------------------------*/
/** Set the message handler to call when a suite
 *  cleanup function returns an error.
 */
void CU_set_suite_cleanup_failure_handler(CU_SuiteCleanupFailureMessageHandler pSuiteCleanupFailureHandler)
{
  f_pSuiteCleanupFailureMessageHandler = pSuiteCleanupFailureHandler;
}

/*------------------------------------------------------------------------*/
/** Retrieve the message handler called before each test is run. */
CU_TestStartMessageHandler CU_get_test_start_handler(void)
{
  return f_pTestStartMessageHandler;
}

/*------------------------------------------------------------------------*/
/** Retrieve the message handler called after each test is run. */
CU_TestCompleteMessageHandler CU_get_test_complete_handler(void)
{
  return f_pTestCompleteMessageHandler;
}

/*------------------------------------------------------------------------*/
/** Retrieve the message handler called after all tests are run. */
CU_AllTestsCompleteMessageHandler CU_get_all_test_complete_handler(void)
{
  return f_pAllTestsCompleteMessageHandler;
}

/*------------------------------------------------------------------------*/
/** Retrieve the message handler called when a suite
 *  initialization error occurs.
 */
CU_SuiteInitFailureMessageHandler CU_get_suite_init_failure_handler(void)
{
  return f_pSuiteInitFailureMessageHandler;
}

/*------------------------------------------------------------------------*/
/** Retrieve the message handler called when a suite
 *  cleanup error occurs.
 */
CU_SuiteCleanupFailureMessageHandler CU_get_suite_cleanup_failure_handler(void)
{
  return f_pSuiteCleanupFailureMessageHandler;
}

/*
 * Functions to get the Run statistics for the Test Run.
 */
/*------------------------------------------------------------------------*/
/** Retrieve the number of suites completed during the previous run.
 *  The count is reset each time the client initiates a run.
 *  @see CU_get_number_of_tests_run()
 */
unsigned int CU_get_number_of_suites_run(void)
{
  return f_run_summary.nSuitesRun;
}

/*------------------------------------------------------------------------*/
/** Retrieve the number of suites which failed to initialize
 *  during the previous run.
 *  The count is reset each time the client initiates a run.
 *  @see CU_get_number_of_tests_run()
 */
unsigned int CU_get_number_of_suites_failed(void)
{
  return f_run_summary.nSuitesFailed;
}

/*------------------------------------------------------------------------*/
/** Retrieve the number of tests completed during the previous run.
 *  The count is reset each time the client initiates a run.
 *  @see CU_get_number_of_suites_run()
 */
unsigned int CU_get_number_of_tests_run(void)
{
  return f_run_summary.nTestsRun;
}

/*------------------------------------------------------------------------*/
/** Retrieve the number of tests which contained failed
 *  assertions during the previous run.
 *  The count is reset each time the client initiates a run.
 *  @see CU_get_number_of_suites_run()
 */
unsigned int CU_get_number_of_tests_failed(void)
{
  return f_run_summary.nTestsFailed;
}

/*------------------------------------------------------------------------*/
/** Retrieve the number of assertions processed during the last run.
 *  The count is reset each time the client initiates a run.
 *  @see CU_get_number_of_successes()
 *  @see CU_get_number_of_failures()
 */
unsigned int CU_get_number_of_asserts(void)
{
  return f_run_summary.nAsserts;
}

/*------------------------------------------------------------------------*/
/** Retrieve the number of successful assertions during the last run.
 *  The count is reset each time the client initiates a run.
 *  @see CU_get_number_of_failures()
 */
unsigned int CU_get_number_of_successes(void)
{
  return (f_run_summary.nAsserts - f_run_summary.nAssertsFailed);
}

/*------------------------------------------------------------------------*/
/** Retrieve the number of failed assertions during the last run.
 *  The count is reset each time the client initiates a run.
 *  @see CU_get_number_of_successes()
 */
unsigned int CU_get_number_of_failures(void)
{
  return f_run_summary.nAssertsFailed;
}

/*------------------------------------------------------------------------*/
/** Retrieve the number failure records created during
 *  the previous run.  Note that this may be more than the
 *  number of failed assertions, since failure records may also
 *  be created for failed suite initialization and cleanup.
 *  The count is reset each time the client initiates a run.
 */
unsigned int CU_get_number_of_failure_records(void)
{
  return f_run_summary.nFailureRecords;
}

/*------------------------------------------------------------------------*/
/** Retrieve the list of failures which occurred during
 *  the last test run.  Note that the pointer returned
 *  is invalidated when the client initiates a run using
 *  CU_run_all_tests(), CU_run_suite(), or CU_run_test().
 *  @see CU_get_number_of_successes()
 */
CU_pFailureRecord CU_get_failure_list(void)
{
  return f_failure_list;
}

/*------------------------------------------------------------------------*/
/** Retrieve the entire run summary for the last test run.
 *  Note that the pFailure pointer in the run summary is
 *  invalidated when the client initiates a run using
 *  CU_run_all_tests(), CU_run_suite(), or CU_run_test().
 *  @see CU_get_number_of_successes()
 */
CU_pRunSummary CU_get_run_summary(void)
{
  return &f_run_summary;
}

/*
 * Functions for running suites and tests.
 */
/*------------------------------------------------------------------------*/
/** Run all tests in all suites registered in the test registry.
 *  The suites are run in the order registered in the test registry.
 *  For each registered suite, any initialization function is first
 *  called, the suite is run using run_single_suite(), and finally
 *  any  suite cleanup function is called.  If an error condition
 *  (other than CUE_NOREGISTRY) occurs during the run, the action
 *  depends on the current error action (see CU_set_error_action()).
 *  @return A CU_ErrorCode indicating the first error condition
 *          encountered while running the tests.
 *  @see CU_run_suite() to run the tests in a specific suite.
 *  @see CU_run_test() for run a specific test only.
 */
CU_ErrorCode CU_run_all_tests(void)
{
  CU_pTestRegistry pRegistry = CU_get_registry();
  CU_pSuite pSuite = NULL;
  CU_ErrorCode result;
  CU_ErrorCode result2;

  CU_set_error(result = CUE_SUCCESS);
  if (NULL == pRegistry) {
    CU_set_error(result = CUE_NOREGISTRY);
  }
  else {
    /* test run is starting - set flag */
    f_bTestIsRunning = CU_TRUE;

    /* Clear results from the previous run */
    clear_previous_results(&f_run_summary, &f_failure_list);

    pSuite = pRegistry->pSuite;
    while ((NULL != pSuite) && ((CUE_SUCCESS == result) || (CU_get_error_action() == CUEA_IGNORE))) {
      /* if the suite has tests, run it */
      if (pSuite->uiNumberOfTests > 0) {
        result2 = run_single_suite(pSuite, &f_run_summary);
        result = (CUE_SUCCESS == result) ? result2 : result;  /* result = 1st error encountered */
      }
      pSuite = pSuite->pNext;
    }

    /* test run is complete - clear flag */
    f_bTestIsRunning = CU_FALSE;

    if (NULL != f_pAllTestsCompleteMessageHandler) {
     (*f_pAllTestsCompleteMessageHandler)(f_failure_list);
    }
  }

  return result;
}

/*------------------------------------------------------------------------*/
/** Run all tests in a specified suite.
 *  The suite need not be registered in the test registry to be run.
 *  Any initialization function for the suite is first called,
 *  then the suite is run using run_single_suite(), and any
 *  suite cleanup function is called.  Note that the
 *  run statistics (counts of tests, successes, failures)
 *  are initialized each time this function is called.
 *  If an error condition occurs during the run, the action
 *  depends on the current error action (see CU_set_error_action()).
 *  @param pSuite The suite containing the test (non-NULL)
 *  @return A CU_ErrorCode indicating the first error condition
 *          encountered while running the suite.  CU_run_suite()
 *          sets and returns CUE_NOSUITE if pSuite is NULL.  Other
 *          error codes can be set during suite initialization or
 *          cleanup or during test runs.
 *  @see CU_run_all_tests() to run all suites.
 *  @see CU_run_test() to run a single test in a specific suite.
 */
CU_ErrorCode CU_run_suite(CU_pSuite pSuite)
{
  CU_ErrorCode result;

  CU_set_error(result = CUE_SUCCESS);
  if (NULL == pSuite) {
    CU_set_error(result = CUE_NOSUITE);
  }
  else {
    /* test run is starting - set flag */
    f_bTestIsRunning = CU_TRUE;

    /* Clear results from the previous run */
    clear_previous_results(&f_run_summary, &f_failure_list);

    if (pSuite->uiNumberOfTests > 0) {
      result = run_single_suite(pSuite, &f_run_summary);
    }

    /* test run is complete - clear flag */
    f_bTestIsRunning = CU_FALSE;

    if (NULL != f_pAllTestsCompleteMessageHandler) {
      (*f_pAllTestsCompleteMessageHandler)(f_failure_list);
    }
  }

  return result;
}

/*------------------------------------------------------------------------*/
/** Run a specific test in a specified suite.
 *  The suite need not be registered in the test registry to be run,
 *  although the test must be registered in the specified suite.
 *  Any initialization function for the suite is first
 *  called, then the test is run using run_single_test(), and
 *  any suite cleanup function is called.  Note that the
 *  run statistics (counts of tests, successes, failures)
 *  are initialized each time this function is called.
 *  @param pSuite The suite containing the test (non-NULL)
 *  @param pTest  The test to run (non-NULL)
 *  @return A CU_ErrorCode indicating the first error condition
 *          encountered while running the suite.  CU_run_test()
 *          sets and returns CUE_NOSUITE if pSuite is NULL,
 *          CUE_NOTEST if pTest is NULL, and CUE_TEST_NOT_IN_SUITE
 *          if pTest is not registered in pSuite.  Other
 *          error codes can be set during suite initialization or
 *          cleanup or during the test run.
 *  @see CU_run_all_tests() to run all tests/suites.
 *  @see CU_run_suite() to run all tests in a specific suite.
 */
CU_ErrorCode CU_run_test(CU_pSuite pSuite, CU_pTest pTest)
{
  CU_ErrorCode result;
  CU_ErrorCode result2;

  CU_set_error(result = CUE_SUCCESS);
  if (NULL == pSuite) {
    CU_set_error(result = CUE_NOSUITE);
  }
  else if (NULL == pTest) {
    CU_set_error(result = CUE_NOTEST);
  }
  else if ((NULL == pTest->pName) || (NULL == CU_get_test_by_name(pTest->pName, pSuite))) {
    CU_set_error(result = CUE_TEST_NOT_IN_SUITE);
  }
  else {
    /* test run is starting - set flag */
    f_bTestIsRunning = CU_TRUE;

    /* Clear results from the previous run */
    clear_previous_results(&f_run_summary, &f_failure_list);

    f_pCurTest = NULL;
    f_pCurSuite = pSuite;

    if ((NULL != pSuite->pInitializeFunc) && (0 != (*pSuite->pInitializeFunc)())) {
      if (NULL != f_pSuiteInitFailureMessageHandler) {
        (*f_pSuiteInitFailureMessageHandler)(pSuite);
      }
      f_run_summary.nSuitesFailed++;
      add_failure(&f_failure_list, &f_run_summary,
                  0, "Suite Initialization failed - Test Skipped", "CUnit System", pSuite, pTest);
      CU_set_error(result = CUE_SINIT_FAILED);
      /* test run is complete - clear flag */
      f_bTestIsRunning = CU_FALSE;
    }
    /* reach here if no suite initialization, or if it succeeded */
    else {
      result2 = run_single_test(pTest, &f_run_summary);
      result = (CUE_SUCCESS == result) ? result2 : result;

      if ((NULL != pSuite->pCleanupFunc) && (0 != (*pSuite->pCleanupFunc)())) {
        if (NULL != f_pSuiteCleanupFailureMessageHandler) {
          (*f_pSuiteCleanupFailureMessageHandler)(pSuite);
        }
        f_run_summary.nSuitesFailed++;
        add_failure(&f_failure_list, &f_run_summary,
                    0, "Suite cleanup failed.", "CUnit System", pSuite, pTest);
        result = (CUE_SUCCESS == result) ? CUE_SCLEAN_FAILED : result;
        CU_set_error(CUE_SCLEAN_FAILED);
      }

      /* test run is complete - clear flag */
      f_bTestIsRunning = CU_FALSE;

      if (NULL != f_pAllTestsCompleteMessageHandler) {
        (*f_pAllTestsCompleteMessageHandler)(f_failure_list);
      }

      f_pCurSuite = NULL;
    }
  }

  return result;
}

/*------------------------------------------------------------------------*/
/** Initialize the run summary information stored from
 *  the previous test run.  Resets the run counts to zero,
 *  and frees any memory associated with failure records.
 *  Calling this function multiple times, while inefficient,
 *  will not cause an error condition.
 *  @see clear_previous_results()
 */
void CU_clear_previous_results(void)
{
  clear_previous_results(&f_run_summary, &f_failure_list);
}

/*------------------------------------------------------------------------*/
/** Retrieve a pointer to the currently-running suite (NULL if none).
 */
CU_pSuite CU_get_current_suite(void)
{
  return f_pCurSuite;
}

/*------------------------------------------------------------------------*/
/** Retrieve a pointer to the currently-running test (NULL if none).
 */
CU_pTest CU_get_current_test(void)
{
  return f_pCurTest;
}

/*------------------------------------------------------------------------*/
/** Returns <CODE>CU_TRUE</CODE> if a test run is in progress,
 *  <CODE>CU_TRUE</CODE> otherwise.
 */
CU_BOOL CU_is_test_running(void)
{
  return f_bTestIsRunning;
}

/*------------------------------------------------------------------------*/
/** Record a failed test.
 *  This function is called whenever a test fails to record the
 *  details of the failure.  This includes user assertion failures
 *  and system errors such as failure to initialize a suite.
 *  @param ppFailure    Pointer to head of linked list of failure
 *                      records to append with new failure record.
 *                      If it points to a NULL pointer, it will be set
 *                      to point to the new failure record.
 *  @param pRunSummary  Pointer to CU_RunSummary keeping track of failure records
 *                      (ignored if NULL).
 *  @param uiLineNumber Line number of the failure, if applicable.
 *  @param szCondition  Description of failure condition
 *  @param szFileName   Name of file, if applicable
 *  @param pSuite       The suite being run at time of failure
 *  @param pTest        The test being run at time of failure
 */
void add_failure(CU_pFailureRecord* ppFailure,
                 CU_pRunSummary pRunSummary,
                 unsigned int uiLineNumber,
                 char szCondition[],
                 char szFileName[],
                 CU_pSuite pSuite,
                 CU_pTest pTest)
{
  CU_pFailureRecord pFailureNew = NULL;
  CU_pFailureRecord pTemp = NULL;

  assert(NULL != ppFailure);

  pFailureNew = (CU_pFailureRecord)CU_MALLOC(sizeof(CU_FailureRecord));

  if (NULL == pFailureNew) {
    return;
  }

  pFailureNew->strFileName = NULL;
  pFailureNew->strCondition = NULL;
  if (NULL != szFileName) {
    pFailureNew->strFileName = (char*)CU_MALLOC(strlen(szFileName) + 1);
    if(NULL == pFailureNew->strFileName) {
      CU_FREE(pFailureNew);
      return;
    }
    strcpy(pFailureNew->strFileName, szFileName);
  }

  if (NULL != szCondition) {
    pFailureNew->strCondition = (char*)CU_MALLOC(strlen(szCondition) + 1);
    if (NULL == pFailureNew->strCondition) {
      if(NULL != pFailureNew->strFileName) {
        CU_FREE(pFailureNew->strFileName);
      }
      CU_FREE(pFailureNew);
      return;
    }
    strcpy(pFailureNew->strCondition, szCondition);
  }

  pFailureNew->uiLineNumber = uiLineNumber;
  pFailureNew->pTest = pTest;
  pFailureNew->pSuite = pSuite;
  pFailureNew->pNext = NULL;
  pFailureNew->pPrev = NULL;

  pTemp = *ppFailure;
  if (NULL != pTemp) {
    while (NULL != pTemp->pNext) {
      pTemp = pTemp->pNext;
    }
    pTemp->pNext = pFailureNew;
    pFailureNew->pPrev = pTemp;
  }
  else {
    *ppFailure = pFailureNew;
  }

  if (NULL != pRunSummary) {
    ++(pRunSummary->nFailureRecords);
  }
  f_last_failure = pFailureNew;
}

/*
 *  Local function for result set initialization/cleanup.
 */
/*------------------------------------------------------------------------*/
/** Initialize the run summary information in the
 *  specified structure.  Resets the run counts to zero,
 *  and calls cleanup_failure_list() if failures
 *  were recorded by the last test run.
 *  Calling this function multiple times, while inefficient,
 *  will not cause an error condition.
 *  @param pRunSummary CU_RunSummary to initialize (non-NULL).
 *  @param ppFailure   The failure record to clean (non-NULL).
 *  @see CU_clear_previous_results()
 */
static void clear_previous_results(CU_pRunSummary pRunSummary, CU_pFailureRecord* ppFailure)
{
  assert(NULL != pRunSummary);
  assert(NULL != ppFailure);

  pRunSummary->nSuitesRun = 0;
  pRunSummary->nSuitesFailed = 0;
  pRunSummary->nTestsRun = 0;
  pRunSummary->nTestsFailed = 0;
  pRunSummary->nAsserts = 0;
  pRunSummary->nAssertsFailed = 0;
  pRunSummary->nFailureRecords = 0;

  if (NULL != *ppFailure) {
    cleanup_failure_list(ppFailure);
  }

  f_last_failure = NULL;
}

/*------------------------------------------------------------------------*/
/** Free all memory allocated for the linked list of
 *  test failure records.  pFailure is reset to NULL
 *  after its list is cleaned up.
 *  @param ppFailure Pointer to head of linked list of
 *                   CU_pFailureRecords to clean.
 *  @see CU_clear_previous_results()
 */
static void cleanup_failure_list(CU_pFailureRecord* ppFailure)
{
  CU_pFailureRecord pCurFailure = NULL;
  CU_pFailureRecord pNextFailure = NULL;

  pCurFailure = *ppFailure;

  while (NULL != pCurFailure) {

    if (NULL != pCurFailure->strCondition) {
      CU_FREE(pCurFailure->strCondition);
    }

    if (NULL != pCurFailure->strFileName) {
      CU_FREE(pCurFailure->strFileName);
    }

    pNextFailure = pCurFailure->pNext;
    CU_FREE(pCurFailure);
    pCurFailure = pNextFailure;
  }

  *ppFailure = NULL;
}

/*------------------------------------------------------------------------*/
/** Run all tests in a specified suite.
 *  Internal function to run all tests in a suite.  The suite
 *  need not be registered in the test registry to be run.
 *  If the CUnit system is in an error condition after running
 *  a test, no additional tests are run.
 *  @param pSuite The suite containing the test (non-NULL).
 *  @param pRunSummary The CU_RunSummary to receive the results (non-NULL).
 *  @return A CU_ErrorCode indicating the status of the run.
 *  @see CU_run_suite() for public interface function.
 *  @see CU_run_all_tests() for running all suites.
 */
static CU_ErrorCode run_single_suite(CU_pSuite pSuite, CU_pRunSummary pRunSummary)
{
  CU_pTest pTest = NULL;
  CU_ErrorCode result;
  CU_ErrorCode result2;

  assert(NULL != pSuite);
  assert(NULL != pRunSummary);

  f_pCurTest = NULL;
  f_pCurSuite = pSuite;

  CU_set_error(result = CUE_SUCCESS);
  /* call the suite initialization function, if any */
  if ((NULL != pSuite->pInitializeFunc) && (0 != (*pSuite->pInitializeFunc)())) {
    if (NULL != f_pSuiteInitFailureMessageHandler) {
      (*f_pSuiteInitFailureMessageHandler)(pSuite);
    }
    pRunSummary->nSuitesFailed++;
    add_failure(&f_failure_list, &f_run_summary,
                0, "Suite Initialization failed - Suite Skipped", "CUnit System", pSuite, NULL);
    CU_set_error(result = CUE_SINIT_FAILED);
  }
  /* reach here if no suite initialization, or if it succeeded */
  else {
    pTest = pSuite->pTest;
    while ((NULL != pTest) && ((CUE_SUCCESS == result) || (CU_get_error_action() == CUEA_IGNORE))) {
      result2 = run_single_test(pTest, pRunSummary);
      result = (CUE_SUCCESS == result) ? result2 : result;
      pTest = pTest->pNext;
    }
    pRunSummary->nSuitesRun++;

    /* call the suite cleanup function, if any */
    if ((NULL != pSuite->pCleanupFunc) && (0 != (*pSuite->pCleanupFunc)())) {
      if (NULL != f_pSuiteCleanupFailureMessageHandler) {
        (*f_pSuiteCleanupFailureMessageHandler)(pSuite);
      }
      pRunSummary->nSuitesFailed++;
      add_failure(&f_failure_list, &f_run_summary,
                  0, "Suite cleanup failed.", "CUnit System", pSuite, pTest);
      CU_set_error(CUE_SCLEAN_FAILED);
      result = (CUE_SUCCESS == result) ? CUE_SCLEAN_FAILED : result;
    }
  }

  f_pCurSuite = NULL;
  return result;
}

/*------------------------------------------------------------------------*/
/** Run a specific test.
 *  Internal function to run a test case.  This includes
 *  calling any handler to be run before executing the test,
 *  running the test's function (if any), and calling any
 *  handler to be run after executing a test.
 *  @param pTest The test to be run (non-NULL).
 *  @param pRunSummary The CU_RunSummary to receive the results (non-NULL).
 *  @return A CU_ErrorCode indicating the status of the run.
 *  @see CU_run_test() for public interface function.
 *  @see CU_run_all_tests() for running all suites.
 */
CU_ErrorCode run_single_test(CU_pTest pTest, CU_pRunSummary pRunSummary)
{
  volatile unsigned int nStartFailures;
  /* keep track of the last failure BEFORE running the test */
  volatile CU_pFailureRecord pLastFailure = f_last_failure;
  jmp_buf buf;

  assert(NULL != f_pCurSuite);
  assert(NULL != pTest);
  assert(NULL != pRunSummary);

  nStartFailures = pRunSummary->nAssertsFailed;

  CU_set_error(CUE_SUCCESS);
  f_pCurTest = pTest;

  if (NULL != f_pTestStartMessageHandler) {
    (*f_pTestStartMessageHandler)(f_pCurTest, f_pCurSuite);
  }

  /* set jmp_buf and run test */
  pTest->pJumpBuf = &buf;
  if (0 == setjmp(buf)) {
    if (NULL != pTest->pTestFunc) {
      (*pTest->pTestFunc)();       
    }
  }

  pRunSummary->nTestsRun++;

  /* if additional assertions have failed... */
  if (pRunSummary->nAssertsFailed > nStartFailures) {
    pRunSummary->nTestsFailed++;
    if (NULL != pLastFailure) {
      pLastFailure = pLastFailure->pNext;  /* was a failure before - go to next one */
    }
    else {
      pLastFailure = f_failure_list;       /* no previous failure - go to 1st one */
    }
  }
  else {
    pLastFailure = NULL;                   /* no additional failure - set to NULL */
  }

  if (NULL != f_pTestCompleteMessageHandler) {
    (*f_pTestCompleteMessageHandler)(f_pCurTest, f_pCurSuite, pLastFailure);
  }

  pTest->pJumpBuf = NULL;
  f_pCurTest = NULL;

  return CU_get_error();
}

/** @} */

#ifdef CUNIT_BUILD_TESTS
#include "test_cunit.h"

typedef enum TET {
  TEST_START = 1,
  TEST_COMPLETE,
  ALL_TESTS_COMPLETE,
  SUITE_INIT_FAILED,
  SUITE_CLEANUP_FAILED
} TestEventType;

typedef struct TE {
  TestEventType     type;
  CU_pSuite         pSuite;
  CU_pTest          pTest;
  CU_pFailureRecord pFailure;
  struct TE *       pNext;
} TestEvent, * pTestEvent;

static int f_nTestEvents = 0;
static pTestEvent f_pFirstEvent = NULL;

static void add_test_event(TestEventType type, CU_pSuite psuite,
                           CU_pTest ptest, CU_pFailureRecord pfailure)
{
  pTestEvent pNewEvent = (pTestEvent)malloc(sizeof(TestEvent));
  pTestEvent pNextEvent = NULL;

  pNewEvent->type = type;
  pNewEvent->pSuite = psuite;
  pNewEvent->pTest = ptest;
  pNewEvent->pFailure = pfailure;
  pNewEvent->pNext = NULL;

  pNextEvent = f_pFirstEvent;
  if (pNextEvent) {
    while (pNextEvent->pNext) {
      pNextEvent = pNextEvent->pNext;
    }
    pNextEvent->pNext = pNewEvent;
  }
  else {
    f_pFirstEvent = pNewEvent;
  }
  ++f_nTestEvents;
}

static void clear_test_events(void)
{
  pTestEvent pCurrentEvent = f_pFirstEvent;
  pTestEvent pNextEvent = NULL;

  while (pCurrentEvent) {
    pNextEvent = pCurrentEvent->pNext;
    free(pCurrentEvent);
    pCurrentEvent = pNextEvent;
  }

  f_pFirstEvent = NULL;
  f_nTestEvents = 0;
}

static void test_start_handler(const CU_pTest pTest, const CU_pSuite pSuite)
{
  TEST(CU_is_test_running());
  TEST(pSuite == CU_get_current_suite());
  TEST(pTest == CU_get_current_test());

  add_test_event(TEST_START, pSuite, pTest, NULL);
}

static void test_complete_handler(const CU_pTest pTest, const CU_pSuite pSuite,
                                  const CU_pFailureRecord pFailure)
{
  TEST(CU_is_test_running());
  TEST(pSuite == CU_get_current_suite());
  TEST(pTest == CU_get_current_test());

  add_test_event(TEST_COMPLETE, pSuite, pTest, pFailure);
}

static void test_all_complete_handler(const CU_pFailureRecord pFailure)
{
  TEST(!CU_is_test_running());

  add_test_event(ALL_TESTS_COMPLETE, NULL, NULL, pFailure);
}

static void suite_init_failure_handler(const CU_pSuite pSuite)
{
  TEST(CU_is_test_running());
  TEST(pSuite == CU_get_current_suite());

  add_test_event(SUITE_INIT_FAILED, pSuite, NULL, NULL);
}

static void suite_cleanup_failure_handler(const CU_pSuite pSuite)
{
  TEST(CU_is_test_running());
  TEST(pSuite == CU_get_current_suite());

  add_test_event(SUITE_CLEANUP_FAILED, pSuite, NULL, NULL);
}

void test_succeed(void) { CU_TEST(CU_TRUE); }
void test_fail(void) { CU_TEST(CU_FALSE); }
int suite_succeed(void) { return 0; }
int suite_fail(void) { return 1; }

/*-------------------------------------------------*/
/* tests:
 *    CU_set_test_start_handler()
 *    CU_set_test_complete_handler()
 *    CU_set_all_test_complete_handler()
 *    CU_set_suite_init_failure_handler()
 *    CU_set_suite_cleanup_failure_handler()
 *    CU_get_test_start_handler()
 *    CU_get_test_complete_handler()
 *    CU_get_all_test_complete_handler()
 *    CU_get_suite_init_failure_handler()
 *    CU_get_suite_cleanup_failure_handler()
 */
static void test_message_handlers(void)
{
  CU_pSuite pSuite1 = NULL;
  CU_pSuite pSuite2 = NULL;
  CU_pSuite pSuite3 = NULL;
  CU_pTest  pTest1 = NULL;
  CU_pTest  pTest2 = NULL;
  CU_pTest  pTest3 = NULL;
  CU_pTest  pTest4 = NULL;
  CU_pTest  pTest5 = NULL;
  pTestEvent pEvent = NULL;
  CU_pRunSummary pRunSummary = NULL;

  TEST(!CU_is_test_running());

  /* handlers should be NULL on startup */
  TEST(NULL == CU_get_test_start_handler());
  TEST(NULL == CU_get_test_complete_handler());
  TEST(NULL == CU_get_all_test_complete_handler());
  TEST(NULL == CU_get_suite_init_failure_handler());
  TEST(NULL == CU_get_suite_cleanup_failure_handler());

  /* register some suites and tests */
  CU_initialize_registry();
  pSuite1 = CU_add_suite("suite1", NULL, NULL);
  pTest1 = CU_add_test(pSuite1, "test1", test_succeed);
  pTest2 = CU_add_test(pSuite1, "test2", test_fail);
  pTest3 = CU_add_test(pSuite1, "test3", test_succeed);
  pSuite2 = CU_add_suite("suite2", suite_fail, NULL);
  pTest4 = CU_add_test(pSuite2, "test4", test_succeed);
  pSuite3 = CU_add_suite("suite3", suite_succeed, suite_fail);
  pTest5 = CU_add_test(pSuite3, "test5", test_fail);

  TEST_FATAL(CUE_SUCCESS == CU_get_error());

  /* first run tests without handlers set */
  clear_test_events();
  CU_run_all_tests();

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(2 == CU_get_number_of_suites_run());
  TEST(2 == CU_get_number_of_suites_failed());
  TEST(4 == CU_get_number_of_tests_run());
  TEST(2 == CU_get_number_of_tests_failed());
  TEST(4 == CU_get_number_of_asserts());
  TEST(2 == CU_get_number_of_successes());
  TEST(2 == CU_get_number_of_failures());
  TEST(4 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* set handlers to local functions */
  CU_set_test_start_handler(test_start_handler);
  CU_set_test_complete_handler(test_complete_handler);
  CU_set_all_test_complete_handler(test_all_complete_handler);
  CU_set_suite_init_failure_handler(suite_init_failure_handler);
  CU_set_suite_cleanup_failure_handler(suite_cleanup_failure_handler);

  /* confirm handlers set properly */
  TEST(test_start_handler == CU_get_test_start_handler());
  TEST(test_complete_handler == CU_get_test_complete_handler());
  TEST(test_all_complete_handler == CU_get_all_test_complete_handler());
  TEST(suite_init_failure_handler == CU_get_suite_init_failure_handler());
  TEST(suite_cleanup_failure_handler == CU_get_suite_cleanup_failure_handler());

  /* run tests again with handlers set */
  clear_test_events();
  CU_run_all_tests();

  TEST(11 == f_nTestEvents);
  if (11 == f_nTestEvents) {
    pEvent = f_pFirstEvent;
    TEST(TEST_START == pEvent->type);
    TEST(pSuite1 == pEvent->pSuite);
    TEST(pTest1 == pEvent->pTest);
    TEST(NULL == pEvent->pFailure);

    pEvent = pEvent->pNext;
    TEST(TEST_COMPLETE == pEvent->type);
    TEST(pSuite1 == pEvent->pSuite);
    TEST(pTest1 == pEvent->pTest);
    TEST(NULL == pEvent->pFailure);

    pEvent = pEvent->pNext;
    TEST(TEST_START == pEvent->type);
    TEST(pSuite1 == pEvent->pSuite);
    TEST(pTest2 == pEvent->pTest);
    TEST(NULL == pEvent->pFailure);

    pEvent = pEvent->pNext;
    TEST(TEST_COMPLETE == pEvent->type);
    TEST(pSuite1 == pEvent->pSuite);
    TEST(pTest2 == pEvent->pTest);
    TEST(NULL != pEvent->pFailure);

    pEvent = pEvent->pNext;
    TEST(TEST_START == pEvent->type);
    TEST(pSuite1 == pEvent->pSuite);
    TEST(pTest3 == pEvent->pTest);
    TEST(NULL == pEvent->pFailure);

    pEvent = pEvent->pNext;
    TEST(TEST_COMPLETE == pEvent->type);
    TEST(pSuite1 == pEvent->pSuite);
    TEST(pTest3 == pEvent->pTest);
    TEST(NULL == pEvent->pFailure);

    pEvent = pEvent->pNext;
    TEST(SUITE_INIT_FAILED == pEvent->type);
    TEST(pSuite2 == pEvent->pSuite);
    TEST(NULL == pEvent->pTest);
    TEST(NULL == pEvent->pFailure);

    pEvent = pEvent->pNext;
    TEST(TEST_START == pEvent->type);
    TEST(pSuite3 == pEvent->pSuite);
    TEST(pTest5 == pEvent->pTest);
    TEST(NULL == pEvent->pFailure);

    pEvent = pEvent->pNext;
    TEST(TEST_COMPLETE == pEvent->type);
    TEST(pSuite3 == pEvent->pSuite);
    TEST(pTest5 == pEvent->pTest);
    TEST(NULL != pEvent->pFailure);

    pEvent = pEvent->pNext;
    TEST(SUITE_CLEANUP_FAILED == pEvent->type);
    TEST(pSuite3 == pEvent->pSuite);
    TEST(NULL == pEvent->pTest);
    TEST(NULL == pEvent->pFailure);

    pEvent = pEvent->pNext;
    TEST(ALL_TESTS_COMPLETE == pEvent->type);
    TEST(NULL == pEvent->pSuite);
    TEST(NULL == pEvent->pTest);
    TEST(NULL != pEvent->pFailure);
    if (4 == CU_get_number_of_failure_records()) {
      TEST(NULL != pEvent->pFailure->pNext);
      TEST(NULL != pEvent->pFailure->pNext->pNext);
      TEST(NULL != pEvent->pFailure->pNext->pNext->pNext);
      TEST(NULL == pEvent->pFailure->pNext->pNext->pNext->pNext);
    }
    TEST(pEvent->pFailure == CU_get_failure_list());
  }

  TEST(2 == CU_get_number_of_suites_run());
  TEST(2 == CU_get_number_of_suites_failed());
  TEST(4 == CU_get_number_of_tests_run());
  TEST(2 == CU_get_number_of_tests_failed());
  TEST(4 == CU_get_number_of_asserts());
  TEST(2 == CU_get_number_of_successes());
  TEST(2 == CU_get_number_of_failures());
  TEST(4 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* clear handlers and run again */
  CU_set_test_start_handler(NULL);
  CU_set_test_complete_handler(NULL);
  CU_set_all_test_complete_handler(NULL);
  CU_set_suite_init_failure_handler(NULL);
  CU_set_suite_cleanup_failure_handler(NULL);

  TEST(NULL == CU_get_test_start_handler());
  TEST(NULL == CU_get_test_complete_handler());
  TEST(NULL == CU_get_all_test_complete_handler());
  TEST(NULL == CU_get_suite_init_failure_handler());
  TEST(NULL == CU_get_suite_cleanup_failure_handler());

  clear_test_events();
  CU_run_all_tests();

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(2 == CU_get_number_of_suites_run());
  TEST(2 == CU_get_number_of_suites_failed());
  TEST(4 == CU_get_number_of_tests_run());
  TEST(2 == CU_get_number_of_tests_failed());
  TEST(4 == CU_get_number_of_asserts());
  TEST(2 == CU_get_number_of_successes());
  TEST(2 == CU_get_number_of_failures());
  TEST(4 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  CU_cleanup_registry();
  clear_test_events();
}

static CU_BOOL f_exit_called = CU_FALSE;

/* intercept exit for testing of CUEA_ABORT action */
void test_exit(int status)
{
  CU_UNREFERENCED_PARAMETER(status);  /* not used */
  f_exit_called = CU_TRUE;
}

/*-------------------------------------------------*/
static void test_CU_run_all_tests(void)
{
  CU_pSuite pSuite1 = NULL;
  CU_pSuite pSuite2 = NULL;
  CU_pSuite pSuite3 = NULL;
  CU_pSuite pSuite4 = NULL;
  CU_pRunSummary pRunSummary = NULL;

  /* error - uninitialized registry  (CUEA_IGNORE) */
  CU_cleanup_registry();
  CU_set_error_action(CUEA_IGNORE);

  TEST(CUE_NOREGISTRY == CU_run_all_tests());
  TEST(CUE_NOREGISTRY == CU_get_error());

  /* error - uninitialized registry  (CUEA_FAIL) */
  CU_cleanup_registry();
  CU_set_error_action(CUEA_FAIL);

  TEST(CUE_NOREGISTRY == CU_run_all_tests());
  TEST(CUE_NOREGISTRY == CU_get_error());

  /* error - uninitialized registry  (CUEA_ABORT) */
  CU_cleanup_registry();
  CU_set_error_action(CUEA_ABORT);

  f_exit_called = CU_FALSE;
  CU_run_all_tests();
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  /* run with no tests registered */
  CU_set_error_action(CUEA_IGNORE);
  CU_initialize_registry();
  clear_test_events();
  TEST(CUE_SUCCESS == CU_run_all_tests());

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  TEST(NULL == CU_get_failure_list());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* register some suites and tests */
  CU_initialize_registry();
  pSuite1 = CU_add_suite("suite1", NULL, NULL);
  CU_add_test(pSuite1, "test1", test_succeed);
  CU_add_test(pSuite1, "test2", test_fail);
  CU_add_test(pSuite1, "test3", test_succeed);
  CU_add_test(pSuite1, "test4", test_fail);
  CU_add_test(pSuite1, "test5", test_succeed);
  pSuite2 = CU_add_suite("suite2", suite_fail, NULL);
  CU_add_test(pSuite2, "test6", test_succeed);
  CU_add_test(pSuite2, "test7", test_succeed);
  pSuite3 = CU_add_suite("suite3", NULL, NULL);
  CU_add_test(pSuite3, "test8", test_fail);
  CU_add_test(pSuite3, "test9", test_succeed);
  pSuite4 = CU_add_suite("suite4", NULL, suite_fail);
  CU_add_test(pSuite4, "test10", test_succeed);

  TEST_FATAL(CUE_SUCCESS == CU_get_error());

  /* run all tests (CUEA_IGNORE) */
  clear_test_events();
  CU_set_error_action(CUEA_IGNORE);
  TEST(CUE_SINIT_FAILED == CU_run_all_tests());

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(3 == CU_get_number_of_suites_run());
  TEST(2 == CU_get_number_of_suites_failed());
  TEST(8 == CU_get_number_of_tests_run());
  TEST(3 == CU_get_number_of_tests_failed());
  TEST(8 == CU_get_number_of_asserts());
  TEST(5 == CU_get_number_of_successes());
  TEST(3 == CU_get_number_of_failures());
  TEST(5 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* run all tests (CUEA_FAIL) */
  clear_test_events();
  CU_set_error_action(CUEA_FAIL);
  TEST(CUE_SINIT_FAILED == CU_run_all_tests());

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(1 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(5 == CU_get_number_of_tests_run());
  TEST(2 == CU_get_number_of_tests_failed());
  TEST(5 == CU_get_number_of_asserts());
  TEST(3 == CU_get_number_of_successes());
  TEST(2 == CU_get_number_of_failures());
  TEST(3 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* run all tests (CUEA_ABORT) */
  clear_test_events();
  CU_set_error_action(CUEA_ABORT);

  f_exit_called = CU_FALSE;
  TEST(CUE_SINIT_FAILED == CU_run_all_tests());
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(1 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(5 == CU_get_number_of_tests_run());
  TEST(2 == CU_get_number_of_tests_failed());
  TEST(5 == CU_get_number_of_asserts());
  TEST(3 == CU_get_number_of_successes());
  TEST(2 == CU_get_number_of_failures());
  TEST(3 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* clean up after testing */
  CU_set_error_action(CUEA_IGNORE);
  CU_cleanup_registry();
  clear_test_events();
}

/*-------------------------------------------------*/
static void test_CU_run_suite(void)
{
  CU_pSuite pSuite1 = NULL;
  CU_pSuite pSuite2 = NULL;
  CU_pSuite pSuite3 = NULL;
  CU_pSuite pSuite4 = NULL;
  CU_pRunSummary pRunSummary = NULL;

  /* error - NULL suite (CUEA_IGNORE) */
  CU_set_error_action(CUEA_IGNORE);

  TEST(CUE_NOSUITE == CU_run_suite(NULL));
  TEST(CUE_NOSUITE == CU_get_error());

  /* error - NULL suite (CUEA_FAIL) */
  CU_set_error_action(CUEA_FAIL);

  TEST(CUE_NOSUITE == CU_run_suite(NULL));
  TEST(CUE_NOSUITE == CU_get_error());

  /* error - NULL suite (CUEA_ABORT) */
  CU_set_error_action(CUEA_ABORT);

  f_exit_called = CU_FALSE;
  CU_run_suite(NULL);
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  /* register some suites and tests */
  CU_initialize_registry();
  pSuite1 = CU_add_suite("suite1", NULL, NULL);
  CU_add_test(pSuite1, "test1", test_succeed);
  CU_add_test(pSuite1, "test2", test_fail);
  CU_add_test(pSuite1, "test3", test_succeed);
  CU_add_test(pSuite1, "test4", test_fail);
  CU_add_test(pSuite1, "test5", test_succeed);
  pSuite2 = CU_add_suite("suite2", suite_fail, NULL);
  CU_add_test(pSuite2, "test6", test_succeed);
  CU_add_test(pSuite2, "test7", test_succeed);
  pSuite3 = CU_add_suite("suite3", NULL, suite_fail);
  CU_add_test(pSuite3, "test8", test_fail);
  CU_add_test(pSuite3, "test9", test_succeed);
  pSuite4 = CU_add_suite("suite4", NULL, NULL);

  TEST_FATAL(CUE_SUCCESS == CU_get_error());

  /* run each suite (CUEA_IGNORE) */
  clear_test_events();
  CU_set_error_action(CUEA_IGNORE);

  TEST(CUE_SUCCESS == CU_run_suite(pSuite1));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(1 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(5 == CU_get_number_of_tests_run());
  TEST(2 == CU_get_number_of_tests_failed());
  TEST(5 == CU_get_number_of_asserts());
  TEST(3 == CU_get_number_of_successes());
  TEST(2 == CU_get_number_of_failures());
  TEST(2 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SINIT_FAILED == CU_run_suite(pSuite2));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SCLEAN_FAILED == CU_run_suite(pSuite3));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(1 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(2 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(2 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(2 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_suite(pSuite4));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* run each suite (CUEA_FAIL) */
  clear_test_events();
  CU_set_error_action(CUEA_FAIL);

  TEST(CUE_SUCCESS == CU_run_suite(pSuite1));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(1 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(5 == CU_get_number_of_tests_run());
  TEST(2 == CU_get_number_of_tests_failed());
  TEST(5 == CU_get_number_of_asserts());
  TEST(3 == CU_get_number_of_successes());
  TEST(2 == CU_get_number_of_failures());
  TEST(2 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SINIT_FAILED == CU_run_suite(pSuite2));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SCLEAN_FAILED == CU_run_suite(pSuite3));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(1 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(2 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(2 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(2 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_suite(pSuite4));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* run each suite (CUEA_ABORT) */
  clear_test_events();
  CU_set_error_action(CUEA_ABORT);

  f_exit_called = CU_FALSE;
  TEST(CUE_SUCCESS == CU_run_suite(pSuite1));
  TEST(CU_FALSE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(1 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(5 == CU_get_number_of_tests_run());
  TEST(2 == CU_get_number_of_tests_failed());
  TEST(5 == CU_get_number_of_asserts());
  TEST(3 == CU_get_number_of_successes());
  TEST(2 == CU_get_number_of_failures());
  TEST(2 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SINIT_FAILED == CU_run_suite(pSuite2));
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SCLEAN_FAILED == CU_run_suite(pSuite3));
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(1 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(2 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(2 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(2 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_suite(pSuite4));
  TEST(CU_FALSE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* clean up after testing */
  CU_set_error_action(CUEA_IGNORE);
  CU_cleanup_registry();
  clear_test_events();
}

/*-------------------------------------------------*/
static void test_CU_run_test(void)
{
  CU_pSuite pSuite1 = NULL;
  CU_pSuite pSuite2 = NULL;
  CU_pSuite pSuite3 = NULL;
  CU_pTest pTest1 = NULL;
  CU_pTest pTest2 = NULL;
  CU_pTest pTest3 = NULL;
  CU_pTest pTest4 = NULL;
  CU_pTest pTest5 = NULL;
  CU_pTest pTest6 = NULL;
  CU_pTest pTest7 = NULL;
  CU_pTest pTest8 = NULL;
  CU_pTest pTest9 = NULL;
  CU_pRunSummary pRunSummary = NULL;

  /* register some suites and tests */
  CU_initialize_registry();
  pSuite1 = CU_add_suite("suite1", NULL, NULL);
  pTest1 = CU_add_test(pSuite1, "test1", test_succeed);
  pTest2 = CU_add_test(pSuite1, "test2", test_fail);
  pTest3 = CU_add_test(pSuite1, "test3", test_succeed);
  pTest4 = CU_add_test(pSuite1, "test4", test_fail);
  pTest5 = CU_add_test(pSuite1, "test5", test_succeed);
  pSuite2 = CU_add_suite("suite2", suite_fail, NULL);
  pTest6 = CU_add_test(pSuite2, "test6", test_succeed);
  pTest7 = CU_add_test(pSuite2, "test7", test_succeed);
  pSuite3 = CU_add_suite("suite3", NULL, suite_fail);
  pTest8 = CU_add_test(pSuite3, "test8", test_fail);
  pTest9 = CU_add_test(pSuite3, "test9", test_succeed);

  TEST_FATAL(CUE_SUCCESS == CU_get_error());

  /* error - NULL suite (CUEA_IGNORE) */
  CU_set_error_action(CUEA_IGNORE);

  TEST(CUE_NOSUITE == CU_run_test(NULL, pTest1));
  TEST(CUE_NOSUITE == CU_get_error());

  /* error - NULL suite (CUEA_FAIL) */
  CU_set_error_action(CUEA_FAIL);

  TEST(CUE_NOSUITE == CU_run_test(NULL, pTest1));
  TEST(CUE_NOSUITE == CU_get_error());

  /* error - NULL test (CUEA_ABORT) */
  CU_set_error_action(CUEA_ABORT);

  f_exit_called = CU_FALSE;
  CU_run_test(NULL, pTest1);
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  /* error - NULL test (CUEA_IGNORE) */
  CU_set_error_action(CUEA_IGNORE);

  TEST(CUE_NOTEST == CU_run_test(pSuite1, NULL));
  TEST(CUE_NOTEST == CU_get_error());

  /* error - NULL test (CUEA_FAIL) */
  CU_set_error_action(CUEA_FAIL);

  TEST(CUE_NOTEST == CU_run_test(pSuite1, NULL));
  TEST(CUE_NOTEST == CU_get_error());

  /* error - NULL test (CUEA_ABORT) */
  CU_set_error_action(CUEA_ABORT);

  f_exit_called = CU_FALSE;
  CU_run_test(pSuite1, NULL);
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  /* error - test not in suite (CUEA_IGNORE) */
  CU_set_error_action(CUEA_IGNORE);

  TEST(CUE_TEST_NOT_IN_SUITE == CU_run_test(pSuite3, pTest1));
  TEST(CUE_TEST_NOT_IN_SUITE == CU_get_error());

  /* error - NULL test (CUEA_FAIL) */
  CU_set_error_action(CUEA_FAIL);

  TEST(CUE_TEST_NOT_IN_SUITE == CU_run_test(pSuite3, pTest1));
  TEST(CUE_TEST_NOT_IN_SUITE == CU_get_error());

  /* error - NULL test (CUEA_ABORT) */
  CU_set_error_action(CUEA_ABORT);

  f_exit_called = CU_FALSE;
  CU_run_test(pSuite3, pTest1);
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  /* run each test (CUEA_IGNORE) */
  clear_test_events();
  CU_set_error_action(CUEA_IGNORE);

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest1));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest2));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest3));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest4));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest5));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SINIT_FAILED == CU_run_test(pSuite2, pTest6));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SINIT_FAILED == CU_run_test(pSuite2, pTest7));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SCLEAN_FAILED == CU_run_test(pSuite3, pTest8));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(2 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SCLEAN_FAILED == CU_run_test(pSuite3, pTest9));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* run each test (CUEA_FAIL) */
  clear_test_events();
  CU_set_error_action(CUEA_FAIL);

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest1));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest2));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest3));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest4));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest5));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SINIT_FAILED == CU_run_test(pSuite2, pTest6));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SINIT_FAILED == CU_run_test(pSuite2, pTest7));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SCLEAN_FAILED == CU_run_test(pSuite3, pTest8));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(2 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SCLEAN_FAILED == CU_run_test(pSuite3, pTest9));

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* run each test (CUEA_ABORT) */
  clear_test_events();
  CU_set_error_action(CUEA_ABORT);
  f_exit_called = CU_FALSE;

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest1));
  TEST(CU_FALSE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest2));
  TEST(CU_FALSE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest3));
  TEST(CU_FALSE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest4));
  TEST(CU_FALSE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SUCCESS == CU_run_test(pSuite1, pTest5));
  TEST(CU_FALSE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(0 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SINIT_FAILED == CU_run_test(pSuite2, pTest6));
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SINIT_FAILED == CU_run_test(pSuite2, pTest7));
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(0 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SCLEAN_FAILED == CU_run_test(pSuite3, pTest8));
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(1 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(1 == CU_get_number_of_failures());
  TEST(2 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  TEST(CUE_SCLEAN_FAILED == CU_run_test(pSuite3, pTest9));
  TEST(CU_TRUE == f_exit_called);
  f_exit_called = CU_FALSE;

  TEST(0 == f_nTestEvents);
  TEST(NULL == f_pFirstEvent);
  TEST(0 == CU_get_number_of_suites_run());
  TEST(1 == CU_get_number_of_suites_failed());
  TEST(1 == CU_get_number_of_tests_run());
  TEST(0 == CU_get_number_of_tests_failed());
  TEST(1 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());
  pRunSummary = CU_get_run_summary();
  TEST(pRunSummary->nSuitesRun      == CU_get_number_of_suites_run());
  TEST(pRunSummary->nSuitesFailed   == CU_get_number_of_suites_failed());
  TEST(pRunSummary->nTestsRun       == CU_get_number_of_tests_run());
  TEST(pRunSummary->nTestsFailed    == CU_get_number_of_tests_failed());
  TEST(pRunSummary->nAsserts        == CU_get_number_of_asserts());
  TEST(pRunSummary->nAssertsFailed  == CU_get_number_of_failures());
  TEST(pRunSummary->nFailureRecords == CU_get_number_of_failure_records());

  /* clean up after testing */
  CU_set_error_action(CUEA_IGNORE);
  CU_cleanup_registry();
  clear_test_events();
}

/*-------------------------------------------------*/
static void test_CU_get_number_of_suites_run(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_get_number_of_suites_failed(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_get_number_of_tests_run(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_get_number_of_tests_failed(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_get_number_of_asserts(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_get_number_of_successes(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_get_number_of_failures(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_get_failure_list(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_get_run_summary(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_get_current_suite(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_get_current_test(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_is_test_running(void)
{
  /* tested adequately in other tests */
}

/*-------------------------------------------------*/
static void test_CU_assertImplementation(void)
{
  CU_Test dummy_test;
  CU_Suite dummy_suite;
  CU_pFailureRecord pFailure1 = NULL;
  CU_pFailureRecord pFailure2 = NULL;
  CU_pFailureRecord pFailure3 = NULL;
  CU_pFailureRecord pFailure4 = NULL;
  CU_pFailureRecord pFailure5 = NULL;
  CU_pFailureRecord pFailure6 = NULL;

  CU_clear_previous_results();

  TEST(NULL == CU_get_failure_list());
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());

  /* fool CU_assertImplementation into thinking test run is in progress */
  f_pCurTest = &dummy_test;
  f_pCurSuite = &dummy_suite;

  /* asserted value is CU_TRUE*/
  TEST(CU_TRUE == CU_assertImplementation(CU_TRUE, 100, "Nothing happened 0.", "dummy0.c", "dummy_func0", CU_FALSE));

  TEST(NULL == CU_get_failure_list());
  TEST(1 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());

  TEST(CU_TRUE == CU_assertImplementation(CU_TRUE, 101, "Nothing happened 1.", "dummy1.c", "dummy_func1", CU_FALSE));

  TEST(NULL == CU_get_failure_list());
  TEST(2 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());

  /* asserted value is CU_FALSE */
  TEST(CU_FALSE == CU_assertImplementation(CU_FALSE, 102, "Something happened 2.", "dummy2.c", "dummy_func2", CU_FALSE));

  TEST(NULL != CU_get_failure_list());
  TEST(3 == CU_get_number_of_asserts());
  TEST(1 == CU_get_number_of_failures());
  TEST(1 == CU_get_number_of_failure_records());

  TEST(CU_FALSE == CU_assertImplementation(CU_FALSE, 103, "Something happened 3.", "dummy3.c", "dummy_func3", CU_FALSE));

  TEST(NULL != CU_get_failure_list());
  TEST(4 == CU_get_number_of_asserts());
  TEST(2 == CU_get_number_of_failures());
  TEST(2 == CU_get_number_of_failure_records());

  TEST(CU_FALSE == CU_assertImplementation(CU_FALSE, 104, "Something happened 4.", "dummy4.c", "dummy_func4", CU_FALSE));

  TEST(NULL != CU_get_failure_list());
  TEST(5 == CU_get_number_of_asserts());
  TEST(3 == CU_get_number_of_failures());
  TEST(3 == CU_get_number_of_failure_records());

  if (3 == CU_get_number_of_failure_records()) {
    pFailure1 = CU_get_failure_list();
    TEST(102 == pFailure1->uiLineNumber);
    TEST(!strcmp("dummy2.c", pFailure1->strFileName));
    TEST(!strcmp("Something happened 2.", pFailure1->strCondition));
    TEST(&dummy_test == pFailure1->pTest);
    TEST(&dummy_suite == pFailure1->pSuite);
    TEST(NULL != pFailure1->pNext);
    TEST(NULL == pFailure1->pPrev);

    pFailure2 = pFailure1->pNext;
    TEST(103 == pFailure2->uiLineNumber);
    TEST(!strcmp("dummy3.c", pFailure2->strFileName));
    TEST(!strcmp("Something happened 3.", pFailure2->strCondition));
    TEST(&dummy_test == pFailure2->pTest);
    TEST(&dummy_suite == pFailure2->pSuite);
    TEST(NULL != pFailure2->pNext);
    TEST(pFailure1 == pFailure2->pPrev);

    pFailure3 = pFailure2->pNext;
    TEST(104 == pFailure3->uiLineNumber);
    TEST(!strcmp("dummy4.c", pFailure3->strFileName));
    TEST(!strcmp("Something happened 4.", pFailure3->strCondition));
    TEST(&dummy_test == pFailure3->pTest);
    TEST(&dummy_suite == pFailure3->pSuite);
    TEST(NULL == pFailure3->pNext);
    TEST(pFailure2 == pFailure3->pPrev);
  }
  else
    FAIL("Unexpected number of failure records.");

  /* confirm destruction of failure records */
  pFailure4 = pFailure1;
  pFailure5 = pFailure2;
  pFailure6 = pFailure3;
  TEST(0 != test_cunit_get_n_memevents(pFailure4));
  TEST(test_cunit_get_n_allocations(pFailure4) != test_cunit_get_n_deallocations(pFailure4));
  TEST(0 != test_cunit_get_n_memevents(pFailure5));
  TEST(test_cunit_get_n_allocations(pFailure5) != test_cunit_get_n_deallocations(pFailure5));
  TEST(0 != test_cunit_get_n_memevents(pFailure6));
  TEST(test_cunit_get_n_allocations(pFailure6) != test_cunit_get_n_deallocations(pFailure6));

  CU_clear_previous_results();
  TEST(0 != test_cunit_get_n_memevents(pFailure4));
  TEST(test_cunit_get_n_allocations(pFailure4) == test_cunit_get_n_deallocations(pFailure4));
  TEST(0 != test_cunit_get_n_memevents(pFailure5));
  TEST(test_cunit_get_n_allocations(pFailure5) == test_cunit_get_n_deallocations(pFailure5));
  TEST(0 != test_cunit_get_n_memevents(pFailure6));
  TEST(test_cunit_get_n_allocations(pFailure6) == test_cunit_get_n_deallocations(pFailure6));
  TEST(0 == CU_get_number_of_asserts());
  TEST(0 == CU_get_number_of_successes());
  TEST(0 == CU_get_number_of_failures());
  TEST(0 == CU_get_number_of_failure_records());

  f_pCurTest = NULL;
  f_pCurSuite = NULL;
}

/*-------------------------------------------------*/
static void test_CU_clear_previous_results(void)
{
  /* covered by test_CU_assertImplementation() */
}

/*-------------------------------------------------*/
static void test_clear_previous_results(void)
{
  /* covered by test_CU_clear_previous_result() */
}

/*-------------------------------------------------*/
static void test_cleanup_failure_list(void)
{
  /* covered by test_clear_previous_result() */
}

/*-------------------------------------------------*/
static void test_run_single_suite(void)
{
  /* covered by test_CU_run_suite() */
}

/*-------------------------------------------------*/
static void test_run_single_test(void)
{
  /* covered by test_CU_run_test() */
}

/*-------------------------------------------------*/
static void test_add_failure(void)
{
  CU_Test test1;
  CU_Suite suite1;
  CU_pFailureRecord pFailure1 = NULL;
  CU_pFailureRecord pFailure2 = NULL;
  CU_pFailureRecord pFailure3 = NULL;
  CU_pFailureRecord pFailure4 = NULL;
  CU_RunSummary run_summary = {0, 0, 0, 0, 0, 0, 0};

  /* test under memory exhaustion */
  test_cunit_deactivate_malloc();
  add_failure(&pFailure1, &run_summary, 100, "condition 0", "file0.c", &suite1, &test1);
  TEST(NULL == pFailure1);
  TEST(0 == run_summary.nFailureRecords);
  test_cunit_activate_malloc();

  /* normal operation */
  add_failure(&pFailure1, &run_summary, 101, "condition 1", "file1.c", &suite1, &test1);
  TEST(1 == run_summary.nFailureRecords);
  if (TEST(NULL != pFailure1)) {
    TEST(101 == pFailure1->uiLineNumber);
    TEST(!strcmp("condition 1", pFailure1->strCondition));
    TEST(!strcmp("file1.c", pFailure1->strFileName));
    TEST(&test1 == pFailure1->pTest);
    TEST(&suite1 == pFailure1->pSuite);
    TEST(NULL == pFailure1->pNext);
    TEST(NULL == pFailure1->pPrev);
    TEST(pFailure1 == f_last_failure);
    TEST(0 != test_cunit_get_n_memevents(pFailure1));
    TEST(test_cunit_get_n_allocations(pFailure1) != test_cunit_get_n_deallocations(pFailure1));
  }

  add_failure(&pFailure1, &run_summary, 102, "condition 2", "file2.c", NULL, &test1);
  TEST(2 == run_summary.nFailureRecords);
  if (TEST(NULL != pFailure1)) {
    TEST(101 == pFailure1->uiLineNumber);
    TEST(!strcmp("condition 1", pFailure1->strCondition));
    TEST(!strcmp("file1.c", pFailure1->strFileName));
    TEST(&test1 == pFailure1->pTest);
    TEST(&suite1 == pFailure1->pSuite);
    TEST(NULL != pFailure1->pNext);
    TEST(NULL == pFailure1->pPrev);
    TEST(pFailure1 != f_last_failure);
    TEST(0 != test_cunit_get_n_memevents(pFailure1));
    TEST(test_cunit_get_n_allocations(pFailure1) != test_cunit_get_n_deallocations(pFailure1));

    if (TEST(NULL != (pFailure2 = pFailure1->pNext))) {
      TEST(102 == pFailure2->uiLineNumber);
      TEST(!strcmp("condition 2", pFailure2->strCondition));
      TEST(!strcmp("file2.c", pFailure2->strFileName));
      TEST(&test1 == pFailure2->pTest);
      TEST(NULL == pFailure2->pSuite);
      TEST(NULL == pFailure2->pNext);                          
      TEST(pFailure1 == pFailure2->pPrev);
      TEST(pFailure2 == f_last_failure);
      TEST(0 != test_cunit_get_n_memevents(pFailure2));
      TEST(test_cunit_get_n_allocations(pFailure2) != test_cunit_get_n_deallocations(pFailure2));
    }
  }

  pFailure3 = pFailure1;
  pFailure4 = pFailure2;
  clear_previous_results(&run_summary, &pFailure1);

  TEST(0 == run_summary.nFailureRecords);
  TEST(0 != test_cunit_get_n_memevents(pFailure3));
  TEST(test_cunit_get_n_allocations(pFailure3) == test_cunit_get_n_deallocations(pFailure3));
  TEST(0 != test_cunit_get_n_memevents(pFailure4));
  TEST(test_cunit_get_n_allocations(pFailure4) == test_cunit_get_n_deallocations(pFailure4));
}

/*-------------------------------------------------*/
void test_cunit_TestRun(void)
{
  test_cunit_start_tests("TestRun.c");

  test_message_handlers();

  test_CU_run_all_tests();
  test_CU_run_suite();
  test_CU_run_test();
  test_CU_get_number_of_suites_run();
  test_CU_get_number_of_suites_failed();
  test_CU_get_number_of_tests_run();
  test_CU_get_number_of_tests_failed();
  test_CU_get_number_of_asserts();
  test_CU_get_number_of_successes();
  test_CU_get_number_of_failures();
  test_CU_get_failure_list();
  test_CU_get_run_summary();
  test_CU_get_current_suite();
  test_CU_get_current_test();
  test_CU_is_test_running();
  test_CU_clear_previous_results();
  test_CU_assertImplementation();
  test_clear_previous_results();
  test_cleanup_failure_list();
  test_run_single_suite();
  test_run_single_test();
  test_add_failure();

  test_cunit_end_tests();
}

#endif    /* CUNIT_BUILD_TESTS */
