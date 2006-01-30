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
 *  Contains all the Type Definitions and functions declarations
 *  for the CUnit test database maintenance.
 *
 *  Aug 2001      Initial implementation. (AK)
 *
 *  09/Aug/2001   Added Preprocessor conditionals for the file. (AK)
 *
 *  24/aug/2001   Made the linked list from SLL to DLL(doubly linked list). (AK)
 *
 *  31-Aug-2004   Restructured to eliminate global variables error_number, 
 *                g_pTestRegistry; new interface, support for deprecated 
 *                version 1 interface, moved error handling code to 
 *                CUError.[ch], moved test run counts and _TestResult out 
 *                of TestRegistry to TestRun.h. (JDS)
 *
 *  01-Sep-2004   Added jmp_buf to CU_Test. (JDS)
 *
 *  05-Sep-2004   Added internal test interface. (JDS)
 */

/** @file
 *  Management functions for tests, suites, and the test registry (user interface).
 *  Unit testing in CUnit follows the standard structure of unit
 *  tests aggregated in suites, which are themselves aggregated
 *  in a test registry.  This module provides functions and
 *  typedef's to support the creation, registration, and manipulation
 *  of test cases, suites, and the registry.
 */
/** @addtogroup Framework
 *  @{
 */

#ifndef CUNIT_TESTDB_H_SEEN
#define CUNIT_TESTDB_H_SEEN

#include <setjmp.h>   /* jmp_buf */

#include "CUnit.h"
#include "CUError.h"

#ifdef __cplusplus
extern "C" {
#endif

/*  Type definition for Initialization/Cleaup/TestFunction */
typedef int  (*CU_InitializeFunc)(void);  /**< Signature for suite initialization function. */
typedef int  (*CU_CleanupFunc)(void);     /**< Signature for suite cleanup function. */
typedef void (*CU_TestFunc)(void);        /**< Signature for a testing function in a test case. */

/** CUnit test case data type.
 *  CU_Test is a linked list of unit tests.  Each test
 *  has a name and a callable test function, as well as
 *  links to the next and previous tests in the list.  A
 *  test also holds a jmp_buf reference for use in
 *  implementing fatal assertions.
 *  <P>
 *  Generally, the linked list includes tests which are
 *  associated with each other in a CU_Suite.  As a result,
 *  tests are run in the order in which they are added to a
 *  suite (see CU_add_test()).
 *  <P>
 *  In the current implementation, the name of each CU_Test
 *  in a suite must have a unique name.  There is no
 *  restriction on the test function.  This means that the
 *  same function could, in principle, be called more than
 *  once as long as it is registered with different tests
 *  having distinct names.
 *  @see CU_Suite
 *  @see CU_TestRegistry
 */
typedef struct CU_Test
{
  char*           pName;                  /**< Test name. */
  CU_TestFunc     pTestFunc;              /**< Pointer to the test function. */
  jmp_buf*        pJumpBuf;               /**< Jump buffer for setjmp/longjmp test abort mechanism. */

  struct CU_Test* pNext;                  /**< Pointer to the next test in linked list. */
  struct CU_Test* pPrev;                  /**< Pointer to the previous test in linked list. */

} CU_Test;
typedef CU_Test* CU_pTest;                /**< Pointer to a CUnit test case. */

/** CUnit suite data type.
 *  CU_Suite is a linked list of CU_Test containers.
 *  Each suite has a name and count of associated unit
 *  tests.  It also holds a pointer to optional
 *  initialization and cleanup functions.  If non-NULL,
 *  these are called before and after running the suite's
 *  tests, respectively.  In addition, the suite holds a
 *  pointer to the head of the linked list of associated
 *  CU_Test objects.  Finally, pointers to the next and
 *  previous suites in the linked list are maintained.
 *  <P>
 *  Generally, the linked list includes suites which are
 *  associated with each other in a CU_TestRegistry.  As a
 *  result, suites are run in the order in which they are
 *  registered (see CU_add_suite()).
 *  <P>
 *  In the current implementation, the name of each CU_Suite
 *  in a test registry must have a unique name.  There is no
 *  restriction on the contained tests.  This means that the
 *  same CU_Test could, in principle, be run more than
 *  once as long as it is registered with different suites
 *  having distinct names.
 *  @see CU_Test
 * @see CU_TestRegistry
 */
typedef struct CU_Suite
{
  char*             pName;                /**< Suite name. */
  CU_pTest          pTest;                /**< Pointer to the 1st test in the suite. */
  CU_InitializeFunc pInitializeFunc;      /**< Pointer to the suite initialization function. */
  CU_CleanupFunc    pCleanupFunc;         /**< Pointer to the suite cleanup function. */

  unsigned int      uiNumberOfTests;      /**< Number of tests in the suite. */
  struct CU_Suite*  pNext;                /**< Pointer to the next suite in linked list. */
  struct CU_Suite*  pPrev;                /**< Pointer to the previous suite in linked list. */

} CU_Suite;
typedef CU_Suite* CU_pSuite;              /**< Pointer to a CUnit suite. */

/** CUnit test registry data type.
 *  CU_TestRegisty is the repository for suites containing
 *  unit tests.  The test registry maintains a count of the
 *  number of CU_Suite objects contained in the registry, as
 *  well as a count of the total number of CU_Test objects
 *  associated with those suites.  It also holds a pointer
 *  to the head of the linked list of CU_Suite objects.
 *  <P>
 *  With this structure, the user will normally add suites
 *  implictly to the internal test registry using CU_add_suite(),
 *  and then add tests to each suite using CU_add_test().
 *  Test runs are then initiated using one of the appropriate
 *  functions in TestRun.c via one of the interfaces.
 *  <P>
 *  Automatic creation and destruction of the internal registry
 *  and its objects is available using CU_initialize_registry()
 *  and CU_cleanup_registry(), respectively.  For internal and
 *  testing purposes, the internal registry can be retrieved and
 *  assigned.  Functions are also provided for creating and
 *  destroying independent registries.
 *  <P>
 *  Note that earlier versions of CUnit also contained a
 *  pointer to a linked list of CU_FailureRecord objects
 *  (termed _TestResults).  This has been removed from the
 *  registry and relocated to TestRun.c.
 *  @see CU_Test
 *  @see CU_Suite
 *  @see CU_initialize_registry()
 *  @see CU_cleanup_registry()
 *  @see CU_get_registry()
 *  @see CU_set_registry()
 *  @see CU_create_new_registry()
 *  @see CU_destroy_existing_registry()
 */
typedef struct CU_TestRegistry
{
#ifdef USE_DEPRECATED_CUNIT_NAMES
  /** Union to support v1.1-1 member name. */
  union {
    unsigned int uiNumberOfSuites;        /**< Number of suites in the test registry. */
    unsigned int uiNumberOfGroups;        /**< Deprecated (version 1). @deprecated Use uiNumberOfSuites. */
  };
  unsigned int uiNumberOfTests;           /**< Number of tests in the test registry. */
  /** Union to support v1.1-1 member name. */
  union {
    CU_pSuite    pSuite;                  /**< Pointer to the 1st suite in the test registry. */
    CU_pSuite    pGroup;                  /**< Deprecated (version 1). @deprecated Use pSuite. */
  };
#else
  unsigned int uiNumberOfSuites;          /**< Number of suites in the test registry. */
  unsigned int uiNumberOfTests;           /**< Number of tests in the test registry. */
  CU_pSuite    pSuite;                    /**< Pointer to the 1st suite in the test registry. */
#endif
} CU_TestRegistry;
typedef CU_TestRegistry* CU_pTestRegistry;  /**< Pointer to a CUnit test registry. */

/* Public interface functions */
CU_EXPORT CU_ErrorCode CU_initialize_registry(void);
CU_EXPORT void         CU_cleanup_registry(void);
CU_EXPORT CU_BOOL      CU_registry_initialized(void);

CU_EXPORT CU_pSuite CU_add_suite(const char* strName, CU_InitializeFunc pInit, CU_CleanupFunc pClean);
CU_EXPORT CU_pTest  CU_add_test(CU_pSuite pSuite, const char* strName, CU_TestFunc pTestFunc);

/** Shortcut macro for adding a test to a suite. */
#define CU_ADD_TEST(suite, test) (CU_add_test(suite, #test, (CU_TestFunc)test))

/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
/*  This section is based conceptually on code
 *  Copyright (C) 2004  Aurema Pty Ltd.
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
 *
 *  Derived from code contributed by K. Cheung and Aurema Pty Ltd. (thanks!)
 *    test_case_t, test_group_t, test_suite_t
 */

/** Test case parameters.
 *  This data type is provided to assist CUnit users
 *  manage collections of test and suites.  It is
 *  intended to be used to build arrays of test case
 *  parameters that can be then be referred to in
 *  a CU_suite_info_t variable.
 */
typedef struct CU_TestInfo {
	char       *pName;      /**< Test name. */
	CU_TestFunc pTestFunc;  /**< Test function. */
} CU_TestInfo;
typedef CU_TestInfo* CU_pTestInfo;  /**< Pointer to CU_TestInfo type. */

/** Suite parameters.
 *  This data type is provided to assist CUnit users
 *  manage collections of test and suites.  It is
 *  intended to be used to build arrays of suite
 *  parameters that can be passed to a bulk registration
 *  function such as CU_register_suite() or
 *  CU_register_suites().
 */
typedef struct CU_SuiteInfo {
	char             *pName;         /**< Suite name. */
	CU_InitializeFunc pInitFunc;     /**< Suite initialization function. */
	CU_CleanupFunc    pCleanupFunc;  /**< Suite cleanup function */
	CU_TestInfo      *pTests;        /**< Test case array - must be NULL terminated. */
} CU_SuiteInfo;
typedef CU_SuiteInfo* CU_pSuiteInfo;  /**< Pointer to CU_SuiteInfo type. */

/** NULL CU_test_info_t to terminate arrays of tests. */
#define CU_TEST_INFO_NULL { NULL, NULL }
/** NULL CU_suite_info_t to terminate arrays of suites. */
#define CU_SUITE_INFO_NULL { NULL, NULL, NULL, NULL }

CU_EXPORT CU_ErrorCode CU_register_suites(CU_SuiteInfo suite_info[]);
CU_EXPORT CU_ErrorCode CU_register_nsuites(int suite_count, ...);

#ifdef USE_DEPRECATED_CUNIT_NAMES
typedef CU_TestInfo test_case_t;    /**< Deprecated (version 1). @deprecated Use CU_TestInfo. */
typedef CU_SuiteInfo test_group_t;  /**< Deprecated (version 1). @deprecated Use CU_SuiteInfo. */

/** Deprecated (version 1). @deprecated Use CU_SuiteInfo and CU_TestInfo. */
typedef struct test_suite {
	char *name;            /**< Suite name.  Currently not used. */
	test_group_t *groups;  /**< Test groups.  This must be a NULL terminated array. */
} test_suite_t;

/** Deprecated (version 1). @deprecated Use CU_TEST_INFO_NULL. */
#define TEST_CASE_NULL { NULL, NULL }
/** Deprecated (version 1). @deprecated Use CU_TEST_GROUP_NULL. */
#define TEST_GROUP_NULL { NULL, NULL, NULL, NULL }

/** Deprecated (version 1). @deprecated Use CU_register_suites(). */
#define test_group_register(tg) CU_register_suites(tg)

/** Deprecated (version 1). @deprecated Use CU_SuiteInfo and CU_register_suites(). */
CU_EXPORT int test_suite_register(test_suite_t *ts)
{
	test_group_t *tg;
	int error;

	for (tg = ts->groups; tg->pName; tg++)
		if ((error = CU_register_suites(tg)) != CUE_SUCCESS)
			return error;

	return CUE_SUCCESS;
}
#endif    /* USE_DEPRECATED_CUNIT_NAMES */
/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#ifdef USE_DEPRECATED_CUNIT_NAMES
typedef CU_InitializeFunc InitializeFunc; /**< Deprecated (version 1). @deprecated Use CU_InitializeFunc. */
typedef CU_CleanupFunc CleanupFunc;       /**< Deprecated (version 1). @deprecated Use CU_CleanupFunc. */
typedef CU_TestFunc TestFunc;             /**< Deprecated (version 1). @deprecated Use CU_TestFunc. */

typedef CU_Test _TestCase;                /**< Deprecated (version 1). @deprecated Use CU_Test. */
typedef CU_pTest PTestCase;               /**< Deprecated (version 1). @deprecated Use CU_pTest. */

typedef CU_Suite  _TestGroup;             /**< Deprecated (version 1). @deprecated Use CU_Suite. */
typedef CU_pSuite PTestGroup;             /**< Deprecated (version 1). @deprecated Use CU_pSuite. */

typedef CU_TestRegistry  _TestRegistry;   /**< Deprecated (version 1). @deprecated Use CU_TestRegistry. */
typedef CU_pTestRegistry PTestRegistry;   /**< Deprecated (version 1). @deprecated Use CU_pTestRegistry. */

/* Public interface functions */
/** Deprecated (version 1). @deprecated Use CU_initialize_registry(). */
#define initialize_registry() CU_initialize_registry()
/** Deprecated (version 1). @deprecated Use CU_cleanup_registry(). */
#define cleanup_registry() CU_cleanup_registry()
/** Deprecated (version 1). @deprecated Use CU_add_suite(). */
#define add_test_group(name, init, clean) CU_add_suite(name, init, clean)
/** Deprecated (version 1). @deprecated Use CU_add_test(). */
#define add_test_case(group, name, test) CU_add_test(group, name, test)

/* private internal CUnit testing functions */
/** Deprecated (version 1). @deprecated Use CU_get_registry(). */
#define get_registry() CU_get_registry()
/** Deprecated (version 1). @deprecated Use CU_set_registry(). */
#define set_registry(reg) CU_set_registry((reg))

/** Deprecated (version 1). @deprecated Use CU_get_suite_by_name(). */
#define get_group_by_name(group, reg) CU_get_suite_by_name(group, reg)
/** Deprecated (version 1). @deprecated Use CU_get_test_by_name(). */
#define get_test_by_name(test, group) CU_get_test_by_name(test, group)

/** Deprecated (version 1). @deprecated Use ADD_TEST_TO_SUITE. */
#define ADD_TEST_TO_GROUP(group, test) (CU_add_test(group, #test, (CU_TestFunc)test))
#endif  /* USE_DEPRECATED_CUNIT_NAMES */

/* Internal CUnit system functions.  Should not be routinely called by users. */
CU_EXPORT CU_pTestRegistry CU_get_registry(void);
CU_EXPORT CU_pTestRegistry CU_set_registry(CU_pTestRegistry pTestRegistry);
CU_EXPORT CU_pTestRegistry CU_create_new_registry(void);
CU_EXPORT void             CU_destroy_existing_registry(CU_pTestRegistry* ppRegistry);
CU_EXPORT CU_pSuite        CU_get_suite_by_name(const char* szSuiteName, CU_pTestRegistry pRegistry);
CU_EXPORT CU_pTest         CU_get_test_by_name(const char* szTestName, CU_pSuite pSuite);

#ifdef CUNIT_BUILD_TESTS
void test_cunit_TestDB(void);
#endif

#ifdef __cplusplus
}
#endif
#endif  /*  CUNIT_TESTDB_H_SEEN  */
/** @} */
