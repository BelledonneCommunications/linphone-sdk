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
 *	Contains the Registry/TestGroup/Testcase management Routine	implementation.
 *
 *	Created By      : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified   : 09/Aug/2001
 *	Comment	        : Added startup initialize/cleanup registry functions.
 *	Email           : aksaharan@yahoo.com
 *
 *	Last Modified   : 29/Aug/2001 (Anil Kumar)
 *	Comment	        : Added Test and Group Add functions
 *	Email           : aksaharan@yahoo.com
 *
 * 	Modified        : 02/Oct/2001 (Anil Kumar)
 * 	Comment         : Added Proper Error codes and Messages on the failure conditions.
 *	Email           : aksaharan@yahoo.com
 *
 * 	Modified        : 13/Oct/2001 (Anil Kumar)
 * 	Comment         : Added Code to Check for the Duplicate Group name and test	name.
 *	Email           : aksaharan@yahoo.com
 *
 * 	Modified        : 29-Aug-2004 (Jerry St.Clair)
 *	Comment         : Added suite registration shortcut types & functions
 *                    contributed by K. Cheung.
 *	EMail           : jds2@users.sourceforge.net
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "CUnit.h"
#include "MyMem.h"
#include "TestDB.h"
#include "Util.h"

/*
 *	Global/Static Definitions
 */
int	error_number;

PTestRegistry	g_pTestRegistry = NULL;

/*
 * Extern function declaration
 */
extern void cleanup_result(void);

/*
 * Private function declaration
 */
static void cleanup_test_registry(PTestRegistry pRegistry);
static void cleanup_test_group(PTestGroup pGroup);
static void insert_group_into_registry(PTestRegistry pTestRegistry, PTestGroup pGroup);
static void insert_test_into_group(PTestGroup pGroup, PTestCase pTest);

static int group_exists(PTestRegistry pTestRegistry, char* szGroupName);
static int test_exists(PTestGroup pTestGroup, char* szTestName);

static char* get_error_desc(const int error);
/*
 *	Public Interface functions
 */
int initialize_registry(void)
{
	error_number = CUE_SUCCESS;
	if (NULL != g_pTestRegistry)
		cleanup_registry();

	g_pTestRegistry = (PTestRegistry)MY_MALLOC(sizeof(TestRegistry));
	if (NULL == g_pTestRegistry) {
		error_number = CUE_NOMEMORY;
		goto exit;
	}

	g_pTestRegistry->pGroup = NULL;
	g_pTestRegistry->pResult= NULL;
	g_pTestRegistry->uiNumberOfGroups = 0;
	g_pTestRegistry->uiNumberOfTests = 0;
	g_pTestRegistry->uiNumberOfFailures = 0;

exit:
	return error_number;
}

void cleanup_registry(void)
{
	error_number = CUE_SUCCESS;
	cleanup_result();
	cleanup_test_registry(g_pTestRegistry);
	MY_FREE(g_pTestRegistry);
	g_pTestRegistry = NULL;
	DUMP_MEMORY_USAGE();
}

PTestRegistry get_registry(void)
{
	return g_pTestRegistry;
}

int set_registry(PTestRegistry pTestRegistry)
{
	error_number = CUE_SUCCESS;
	if (NULL != g_pTestRegistry) {
		error_number = CUE_REGISTRY_EXISTS;
		goto exit;
	}

	g_pTestRegistry = pTestRegistry;

exit:
	return error_number;
}


PTestGroup add_test_group(char* strName, InitializeFunc pInit, CleanupFunc pClean)
{
	PTestGroup pRetValue = NULL;

	error_number = CUE_SUCCESS;
	if (!g_pTestRegistry) {
		error_number = CUE_NOREGISTRY;
		goto exit;
	}

	if (!strName) {
		error_number = CUE_NO_GROUPNAME;
		goto exit;
	}

	if (group_exists(g_pTestRegistry, strName)) {
		error_number = CUE_DUP_GROUP;
		goto exit;
	}
	
	pRetValue = (PTestGroup)MY_MALLOC(sizeof(TestGroup));
	if (NULL == pRetValue) {
		error_number = CUE_NOMEMORY;
		goto exit;
	}
	
	pRetValue->pName = (char *)MY_MALLOC(strlen(strName)+1);
	if (NULL == pRetValue->pName) {
		error_number = CUE_NOMEMORY;
		goto delete_test_group;
	}

	strcpy(pRetValue->pName, strName);
	pRetValue->pInitializeFunc = pInit;
	pRetValue->pCleanupFunc = pClean;
	pRetValue->pTestCase = NULL;
	pRetValue->uiNumberOfTests = 0;
	insert_group_into_registry(g_pTestRegistry, pRetValue);
	goto exit;
	
delete_test_group:
	MY_FREE(pRetValue);
	pRetValue = NULL;

exit:
	return pRetValue;
}

PTestCase add_test_case(PTestGroup pGroup, char* strName, TestFunc pTest)
{
	PTestCase pRetValue = NULL;

	error_number = CUE_SUCCESS;
	if (!pGroup) {
		error_number = CUE_NOGROUP;
		goto exit;
	}

	if (!strName) {
		error_number = CUE_NO_TESTNAME;
		goto exit;
	}
	
	if (!pTest) {
		error_number = CUE_NOTEST;
		goto exit;
	}
	
	if (test_exists(pGroup, strName)) {
		error_number = CUE_DUP_TEST;
		goto exit;
	}

	pRetValue = (PTestCase)MY_MALLOC(sizeof(TestCase));
	if (NULL ==pRetValue) {
		error_number = CUE_NOMEMORY;
		goto exit;
	}

	pRetValue->pName = (char *)MY_MALLOC(strlen(strName)+1);
	if (NULL == pRetValue->pName) {
		error_number = CUE_NOMEMORY;
		goto delete_test_case;
	}

	strcpy(pRetValue->pName, strName);
	pRetValue->pTestFunc = pTest;
	pRetValue->pNext = NULL;
	g_pTestRegistry->uiNumberOfTests++;
	insert_test_into_group(pGroup, pRetValue);

	goto exit;
	
delete_test_case:
	MY_FREE(pRetValue);
	pRetValue = NULL;

exit:
	return pRetValue;
}

const char* get_error(void)
{
	return get_error_desc(error_number);
}

/*
 *	Private static function definitions
 */
static void cleanup_test_registry(PTestRegistry pRegistry)
{
	PTestGroup pCurGroup = pRegistry->pGroup;
	PTestGroup pNextGroup;

	while (pCurGroup) {
		pNextGroup = pCurGroup->pNext;
		cleanup_test_group(pCurGroup);

		if (pCurGroup->pName)
			MY_FREE(pCurGroup->pName);

		MY_FREE(pCurGroup);
		pCurGroup = pNextGroup;
	}
}

static void cleanup_test_group(PTestGroup pGroup)
{
	PTestCase pCurTest = pGroup->pTestCase;
	PTestCase pNextTest;

	while (pCurTest) {
		pNextTest = pCurTest->pNext;

		/*
		 * Temporarily Commented to avoid segmenation fault.
		 * Finally to be reviewed and fixed.
		 */
		if (pCurTest->pName)
			MY_FREE(pCurTest->pName);

		MY_FREE(pCurTest);
		pCurTest = pNextTest;
	}
}

static void insert_group_into_registry(PTestRegistry pRegistry, PTestGroup pGroup)
{
	PTestGroup pCurGroup = pRegistry->pGroup;
	
	pGroup->pNext = pGroup->pPrev = NULL;
	g_pTestRegistry->uiNumberOfGroups++;
	if (NULL == pCurGroup) {
		pRegistry->pGroup = pGroup;
		goto exit;
	}


	while (NULL != pCurGroup->pNext)
		pCurGroup = pCurGroup->pNext;

	pCurGroup->pNext = pGroup;
	pGroup->pPrev = pCurGroup;

exit:
	return;
}
	
static void insert_test_into_group(PTestGroup pGroup, PTestCase pTest)
{
	PTestCase pCurTest = pGroup->pTestCase;

	pTest->pNext = pTest->pPrev = NULL;
	pGroup->uiNumberOfTests++;
	if (NULL == pCurTest) {
		pGroup->pTestCase = pTest;
		goto exit;
	}

	while (NULL != pCurTest->pNext)
		pCurTest = pCurTest->pNext;

	pCurTest->pNext = pTest;
	pTest->pPrev = pCurTest;

exit:
	return;
}

static char* get_error_desc(const int iError)
{
	int iMaxIndex = CUE_SUCCESS; 
	static char* ErrorDescription[] = {	
		"No Error", /* CUE_SUCESS - 0 */
			"Memory Allocation Failed", /* CUE_NOMEMORY - 1 */
			"", "", "", "", "", "", "", "", /* From 2 - 9 No Error Codes Defined */
		"Test Registry Does not Exists", /* CUE_NOREGISTRY - 10 */
			"Registry Already Exists", /* CUE_REGISTRY_EXISTS - 11 */
			"", "", "", "", "", "", "", "", /* From 12 - 19 No Error Codes Defined */
		"Group not Defined",  /* CUE_NOGROUP - 20 */
			"Group Name not Defined", /* CUE_NO_GROUPNAME - 21 */
			"Group Initialization Function Failed", /* CUE_GRPINIT_FAILED - 22 */
			"Group Cleanup Function Failed", /* CUE_GRPCLEAN_FAILED - 23 */
			"Group by this Name Already Exists", /* CUE_DUP_GROUP - 24 */ 
			"", "", "", "", "", /* From 24 - 29 No Error Codes Defined */
		"Test not Defined", /* CUE_NOTEST - 30 */
			"Test Name not Defined", /* CUE_NO_TESTNAME - 31 */
			"Test by this Name Already Exists", /*CUE_DUP_TEST - 32 */
		"Undefined Error"
		};
	
	iMaxIndex = sizeof(ErrorDescription)/sizeof(char *) - 1;
	if (iError > iMaxIndex)
		return ErrorDescription[iMaxIndex];
	else
		return ErrorDescription[iError];
}

static int group_exists(PTestRegistry pTestRegistry, char* szGroupName)
{
	PTestGroup pGroup = NULL;

	assert(pTestRegistry);
	
	pGroup = pTestRegistry->pGroup;
	while (pGroup) {
		if (!compare_strings(szGroupName, pGroup->pName))
			return TRUE; 
		pGroup = pGroup->pNext;
	}
	
	return FALSE;
}

static int test_exists(PTestGroup pTestGroup, char* szTestName)
{
	PTestCase pTest = NULL;

	assert(pTestGroup);

	pTest = pTestGroup->pTestCase;
	while (pTest) {
		if (!compare_strings(szTestName, pTest->pName))
			return TRUE;
		pTest = pTest->pNext;
	}
	
	return FALSE;
}

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
 *    - added this comment block
 *    - incorporated code into TestDB.c
 *    - eliminated unneeded #include's
 *
 * NOTE - this code is likely to change significantly in
 *        the upcoming version 2 CUnit release to better
 *        integrate it with the new CUnit API.
 */

/* Registers a unit test group.  */
int test_group_register(test_group_t *tg)
{
	test_case_t *tc;
	PTestGroup group;

	if (!(group = add_test_group(tg->name, tg->init, tg->cleanup)))
		return error_number;

	for (tc = tg->cases; tc->name; tc++)
		if (!add_test_case(group, tc->name, tc->test))
			return error_number;

	return CUE_SUCCESS;
}

/* Registers unit test suite. */
int test_suite_register(test_suite_t *ts)
{
	test_group_t *tg;
	int error;

	for (tg = ts->groups; tg->name; tg++)
		if ((error = test_group_register(tg)) != CUE_SUCCESS)
			return error;

	return CUE_SUCCESS;
}
/*=========================================================================
 *  End Aurema section
 *=========================================================================*/

