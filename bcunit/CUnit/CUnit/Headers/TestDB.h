/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
 *  Copyright (C) 2004  Anil Kumar, Jerry St.Clair
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
 *	Contains all the Type Definitions and functions declarations
 *	for the CUnit test database maintenance.
 *
 *	Created By     : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified  : 09/Aug/2001
 *	Comment        : Added Preprocessor conditionals for the file.
 *	EMail          : aksaharan@yahoo.com
 *
 *	Last Modified  : 24/aug/2001 by Anil Kumar
 *	Comment        : Made the linked list from SLL to DLL(doubly linked list).
 *	EMail          : aksaharan@yahoo.com
 *
 *	Last Modified  : 29-Aug-2004 by Jerry St.Clair
 *	Comment        : Added suite registration shortcut types & functions
 *                   contributed by K. Cheung.
 *	EMail          : jds2@users.sourceforge.net
 *
 */

#ifndef _CUNIT_TESTDB_H
#define _CUNIT_TESTDB_H 1

#include "CUnit.h"
#include "Errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *	Type definition for Initialization/Cleaup/TestFunction for TestCase/TestGroup
 */
typedef int (*InitializeFunc)(void);
typedef int (*CleanupFunc)(void);
typedef void (*TestFunc)(void);

typedef struct _TestCase
{
	char*			pName;
	TestFunc		pTestFunc;

	struct _TestCase*		pNext;
	struct _TestCase*		pPrev;

}TestCase;
typedef TestCase *PTestCase;

typedef struct _TestGroup
{
	char*			pName;
	PTestCase		pTestCase;
	InitializeFunc	pInitializeFunc;
	CleanupFunc		pCleanupFunc;

	unsigned int	uiNumberOfTests;
	struct _TestGroup*		pNext;
	struct _TestGroup*		pPrev;

}TestGroup;
typedef TestGroup *PTestGroup;

typedef struct _TestResult
{
	unsigned int	uiLineNumber;
	char*			strFileName;
	char*			strCondition;	
	PTestCase		pTestCase;
	PTestGroup		pTestGroup;

	struct _TestResult*		pNext;
	struct _TestResult*		pPrev;

}TestResult;
typedef TestResult *PTestResult;

typedef struct _TestRegistry
{
	unsigned int	uiNumberOfGroups;
	unsigned int	uiNumberOfTests;
	unsigned int	uiNumberOfFailures;

	PTestGroup		pGroup;
	PTestResult		pResult;
}TestRegistry;
typedef TestRegistry *PTestRegistry;

extern PTestRegistry g_pTestRegistry;
extern int error_number;

extern int initialize_registry(void);
extern void cleanup_registry(void);

/* Used for Testing CUnit itself, DO NOT Use these in ur test cases until you find a useful
 * situation to use it without any side effects to the current registry that is being run */
extern PTestRegistry get_registry(void);
extern int set_registry(PTestRegistry pTestRegistry);


extern PTestGroup add_test_group(char* strName, InitializeFunc pInit, CleanupFunc pClean);
extern PTestCase add_test_case(PTestGroup pGroup, char* strName, TestFunc pTest);

#define ADD_TEST_TO_GROUP(group, test) (add_test_case(group, ##test, (TestFunc)test))

/*
 * This function is for internal use and is used by the 
 * Asssert Implementation function to store the error description
 * and the codes.
 */
extern void add_failure(unsigned int uiLineNumber, char szCondition[],
				char szFileName[], PTestGroup pGroup, PTestCase pTest);

extern const char* get_error(void);

/*=========================================================================
 *  This section 
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
 */ 

/* Contributed by K. Cheung and Aurema Pty Ltd. (thanks!)
 *
 * The changes made herein (JDS) to the contributed code were:
 *    - added this comment block & example usage comment block
 *    - incorporated code into TestDB.h
 *    - eliminated unneeded #include's
 *    - compacted doxygen comments
 *
 * NOTE - this code is likely to change significantly in
 *        the upcoming version 2 CUnit release to better
 *        integrate it with the new CUnit API.
 */

/* Example Usage

    #include <CUnit/CUnit.h>
    #include <CUnit/TestDB.h>
    #include <CUnit/Curses.h>

    #include <stdio.h>

    static void group_A_case_1(void)
    {
    	ASSERT_TRUE(1);
    }
    
    static void group_A_case_2(void)
    {
    	ASSERT_TRUE(2);
    }
    
    static void group_B_case_1(void)
    {
    	ASSERT_FALSE(1);
    }

    static void group_B_case_2(void)
    {
    	ASSERT_FALSE(2);
    }
    
    static test_case_t group_A_test_cases[] = {
    	{ "1", group_A_case_1 },
    	{ "2", group_A_case_2 },
    	TEST_CASE_NULL,
    };
    
    static test_case_t group_B_test_cases[] = {
    	{ "1", group_B_case_1 },
    	{ "2", group_B_case_2 },
    	TEST_CASE_NULL,
    };
    
    static test_group_t groups[] = {
    	{ "A", NULL, NULL, group_A_test_cases },
    	{ "B", NULL, NULL, group_B_test_cases },
    	TEST_GROUP_NULL,
    };
    
    static test_suite_t suite = { "suite", groups };

    int main(void)
    {
    	setbuf(stdout, NULL);

    	if (initialize_registry() != CUE_SUCCESS) {
    		fprintf(stderr, "initialize_registry failed - %s\n",
    			get_error());
    		exit(EXIT_FAILURE);
    	}

    	if (test_suite_register(&suite) != CUE_SUCCESS) {
    		fprintf(stderr, "test_suite_register failed - %s\n",
    			get_error());
    		exit(EXIT_FAILURE);
    	}

    	curses_run_tests();

    	cleanup_registry();
    
    	return 0;
    }
*/

/** Unit test case. */
typedef struct test_case {
	char *name;           /**< Case name. */
 	void (*test)(void);  	/**< Case function. */
} test_case_t;

/** Group of unit tests. */
typedef struct test_group {
	char *name;            /**< Group name.  */
	int (*init)(void);     /**< Group initialisation function. */
	int (*cleanup)(void);  /**< Group cleanup function */
	test_case_t *cases;    /**< Test cases.  This must be a NULL terminated array. */
} test_group_t;

/** Unit test suite. */
typedef struct test_suite {
	char *name;            /**< Suite name.  Currently not used. */
	test_group_t *groups;  /**< Test groups.  This must be a NULL terminated array. */
} test_suite_t;

/** Macros for ease of defining cases and groups. */
#define TEST_CASE_NULL { NULL, NULL }
#define TEST_GROUP_NULL { NULL, NULL, NULL, NULL }

/** Registers unit test group.
 * @param	tg	Pointer to a test group.
 * @precondition	tg is initialised.
 * @return
 *	CUE_SUCCESS      On success.
 * 	CUE_NOREGISTRY   Test Registry is not initialized
 *	CUE_NO_GROUPNAME Test group name is not specified or is NULL
 *	CUE_DUP_GROUP    Test group with this name already exists in the registry
 *	CUE_NOGROUP      Specified group name doesn't exists in the registry
 *	CUE_NO_TESTNAME  Test name is not specified or is NULL
 *	CUE_NOTEST       Test function not specified
 *	CUE_DUP_TEST     Test case with this name already exists in the specified test group
 */
int test_group_register(test_group_t *tg);

/* Registers unit test suite.
 * @param	ts	Pointer to a test suite.
 * @precondition	ts is initialised.
 * @return
 *	CUE_SUCCESS      On success.
 * 	CUE_NOREGISTRY   Test Registry is not initialized
 *	CUE_NO_GROUPNAME Test group name is not specified or is NULL
 *	CUE_DUP_GROUP    Test group with this name already exists in the registry
 *	CUE_NOGROUP      Specified group name doesn't exists in the registry
 *	CUE_NO_TESTNAME  Test name is not specified or is NULL
 *	CUE_NOTEST       Test function not specified
 *	CUE_DUP_TEST     Test case with this name already exists inthe specified test group
 */
int test_suite_register(test_suite_t *ts);
/*=========================================================================
 *  End Aurema section
 *=========================================================================*/

#ifdef __cplusplus
}
#endif
#endif  /*  _CUNIT_TESTDB_H  */
