/*
 *	Contains the Console Test Interface	implementation.
 *
 *	Created By     : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified  : 19/Aug/2001
 *	Comment        : Added initial registry/group/test framework implementation.
 *	Email          : aksaharan@yahoo.com
 *
 *	Last Modified  : 24/Aug/2001 by Anil Kumar
 *	Comment	       : Changed Data structure from SLL to DLL for all linked lists.
 *	Email          : aksaharan@yahoo.com
 *
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "TestDB.h"
#include "TestRun.h"

/*
 * Initialize/Cleanup the result list.
 */
static void initialize_result_list(void);
static void cleanup_result_list(void);
static int run_single_group_tests(PTestGroup pGroup);
static int run_single_test(PTestCase pTest);

/*
 *	Global data definition to keep handles for the current running Test Group
 *	and Test Case.
 */
PTestGroup	g_pTestGroup 	= NULL;
PTestCase	g_pTestCase	= NULL;

/*
 *	Static global data for this run test handlers/initialization/cleanup
 */
static int f_iNumberOfGroupsRun = 0;
static int f_iNumberOfTestsRun = 0;
static int f_bCleanupResultSet = 0;

static TestStartMessageHandler		f_pTestStartMessageHandler = NULL;
static TestCompleteMessageHandler	f_pTestCompleteMessageHandler = NULL;
static AllTestsCompleteMessageHandler	f_pAllTestsCompleteMessageHandler = NULL;

/*
 * Get/Set functions for Message Handlers.
 */
void set_test_start_handler(TestStartMessageHandler pTestStartMessage)
{
	f_pTestStartMessageHandler = pTestStartMessage;
}

void set_test_complete_handler(TestCompleteMessageHandler pTestCompleteMessage)
{
	f_pTestCompleteMessageHandler = pTestCompleteMessage;
}

void set_all_test_complete_handler(AllTestsCompleteMessageHandler pAllTestsCompleteMessage)
{
	f_pAllTestsCompleteMessageHandler = pAllTestsCompleteMessage;
}

TestStartMessageHandler get_test_start_handler(void)
{
	return f_pTestStartMessageHandler;
}

TestCompleteMessageHandler get_test_complete_handler(void)
{
	return f_pTestCompleteMessageHandler;
}

AllTestsCompleteMessageHandler get_all_test_complete_handler(void)
{
	return f_pAllTestsCompleteMessageHandler;
}

/*
 * Functions to get the Run statistics for the Test Run.
 */
int get_number_of_groups_run(void)
{
	return f_iNumberOfGroupsRun;
}

int get_number_of_tests_run(void)
{
	return f_iNumberOfTestsRun;
}

/*
 * Functions to Run various tests.
 */
int run_all_tests(void)
{
	PTestRegistry pRegistry = g_pTestRegistry;
	PTestGroup pGroup = NULL;
	
	/*
	 *	Check if the Result set for the previous run has to be cleared
	 *	or not.
	 */
	if (f_bCleanupResultSet) {
		initialize_result_list();
	}

	error_number = CUE_SUCCESS;
	if (!pRegistry) {
		error_number = CUE_NOREGISTRY;
		goto exit;
	}

	pGroup = pRegistry->pGroup;
	while (pGroup) {

		g_pTestCase = NULL;
		g_pTestGroup = pGroup;
		
		if (pGroup->pInitializeFunc) {
			if ((*pGroup->pInitializeFunc)()) {
				pGroup = pGroup->pNext;
				continue;
			}
		}

		if (pGroup->uiNumberOfTests) {
			run_single_group_tests(pGroup);
		}

		if (pGroup->pCleanupFunc) {
			if ((*pGroup->pCleanupFunc)()) {
			}
		}

		pGroup = pGroup->pNext;
	}

exit:
	g_pTestCase = NULL;
	g_pTestGroup = NULL;
	if (f_pAllTestsCompleteMessageHandler) {
		(*f_pAllTestsCompleteMessageHandler)(get_registry()->pResult);
	}
	
	f_bCleanupResultSet = 1;
	return error_number;
}

int run_group_tests(PTestGroup pGroup)
{
	/*
	 *	Check if the Result set for the previous run has to be cleared
	 *	or not.
	 */
	if (f_bCleanupResultSet) {
		initialize_result_list();
	}

	error_number = CUE_SUCCESS;
	if (!pGroup) {
		error_number = CUE_NOGROUP;
		goto exit;
	}

	g_pTestCase = NULL;
	g_pTestGroup = pGroup;

	if (pGroup->pInitializeFunc) {
		if ((*pGroup->pInitializeFunc)()) {
			error_number = CUE_GRPINIT_FAILED;
			goto exit;
		}
	}

	run_single_group_tests(pGroup);

	if (pGroup->pCleanupFunc) {
		if ((*pGroup->pCleanupFunc)()) {
			error_number = CUE_GRPCLEAN_FAILED;
		}
	}
	
exit:
	g_pTestGroup = NULL;
	
	if (f_pAllTestsCompleteMessageHandler) {
		(*f_pAllTestsCompleteMessageHandler)(get_registry()->pResult);
	}
	
	f_bCleanupResultSet = 1;
	return error_number;
}

int run_test(PTestGroup pGroup, PTestCase pTest)
{
	/*
	 *	Check if the Result set for the previous run has to be cleared
	 *	or not.
	 */
	if (f_bCleanupResultSet) {
		initialize_result_list();
	}
	
	error_number = CUE_SUCCESS;
	if (!pGroup) {
		error_number = CUE_NOGROUP;
		goto exit;
	}

	if (!pTest) {
		error_number = CUE_NOTEST;
		goto exit;
	}

	g_pTestCase = NULL;
	g_pTestGroup = pGroup;

	if (pGroup->pInitializeFunc) {
		if ((*pGroup->pInitializeFunc)()) {
			error_number = CUE_GRPINIT_FAILED;
			goto exit;
		}
	}

	run_single_test(pTest);

	if (pGroup->pCleanupFunc) {
		if ((*pGroup->pCleanupFunc)()) {
			error_number = CUE_GRPCLEAN_FAILED;
		}
	}

exit:
	g_pTestGroup = NULL;

	if (f_pAllTestsCompleteMessageHandler) {
		(*f_pAllTestsCompleteMessageHandler)(get_registry()->pResult);
	}
	
	f_bCleanupResultSet = 1;
	return error_number;
}

void add_failure(unsigned int uiLineNumber, char szCondition[],
		char szFileName[], PTestGroup pGroup, PTestCase pTest)
{
	PTestResult pResult = NULL;
	PTestResult pTemp = NULL;

	if (!g_pTestRegistry) {
		assert(!"PTestRegistry is NULL which is not supposed "
				"to be so at this stage.");
	}

	pResult = (PTestResult)malloc(sizeof(TestResult));

	if (!pResult) {
		goto exit;
	}

	pResult->strFileName = NULL;
	pResult->strCondition = NULL;
	if (szFileName) {
		pResult->strFileName = (char*)malloc(strlen(szFileName) + 1);
		if(!pResult->strFileName) {
			goto delete_result;
		}
		strcpy(pResult->strFileName, szFileName);
	}
	
	if (szCondition) {
		pResult->strCondition = (char*)malloc(strlen(szCondition) + 1);
		if (!pResult->strCondition) {
			goto delete_filename;
		}
		strcpy(pResult->strCondition, szCondition);
	}

	pResult->uiLineNumber = uiLineNumber;
	pResult->pTestCase = pTest;
	pResult->pTestGroup = pGroup;
	pResult->pNext = pResult->pPrev = NULL;
	
	pTemp = g_pTestRegistry->pResult;
	if (pTemp) {
		while (pTemp->pNext) {
			pTemp = pTemp->pNext;
		}
		pTemp->pNext = pResult;
		pResult->pPrev = pTemp;
	}
	else {
		g_pTestRegistry->pResult = pResult;
		g_pTestRegistry->uiNumberOfFailures = 0;
	}

	g_pTestRegistry->uiNumberOfFailures++;
	goto exit;

delete_filename:
	free(pResult->strFileName);
	
delete_result:
	free(pResult);
	
exit:
	return;
}

/*
 *	Local function for result set initialization/cleanup.
 */
static void initialize_result_list(void)
{
	PTestRegistry pRegistry = get_registry();

	f_iNumberOfGroupsRun = f_iNumberOfTestsRun = 0;
	if (pRegistry->uiNumberOfFailures) {
		cleanup_result_list();
	}
}

static void cleanup_result_list(void)
{
	PTestRegistry pRegistry = get_registry();
	PTestResult pCurResult = NULL;
	PTestResult pNextResult = NULL;
		
	if (!pRegistry) {
		goto exit;
	}

	pCurResult = pRegistry->pResult;

	while (pCurResult) {

		if (pCurResult->strCondition)
			free(pCurResult->strCondition);

		if (pCurResult->strFileName)
			free(pCurResult->strFileName);

		pNextResult = pCurResult->pNext;
		free(pCurResult);
		pCurResult = pNextResult;
	}

	pRegistry->uiNumberOfFailures = 0;
	pRegistry->pResult = NULL;
		
exit:
	return;
}

int run_single_group_tests(PTestGroup pGroup)
{
	PTestCase pTest = NULL;

	error_number = CUE_SUCCESS;
	pTest = pGroup->pTestCase;
	while (pTest) {

		run_single_test(pTest);
		pTest = pTest->pNext;
	}
	f_iNumberOfGroupsRun++;

	return error_number;
}

int run_single_test(PTestCase pTest)
{
	error_number = CUE_SUCCESS;
	g_pTestCase = pTest;

	if (f_pTestStartMessageHandler) {
		(*f_pTestStartMessageHandler)(g_pTestCase->pName, g_pTestGroup->pName);
	}
	
	if (pTest->pTestFunc) {
		(*pTest->pTestFunc)();
	}

#if _DELAYTEST && WIN32
	_sleep(100);
#elif _DELAYTEST
	sleep(1);
#endif

	if (f_pTestCompleteMessageHandler) {
		(*f_pTestCompleteMessageHandler)(g_pTestCase->pName, g_pTestGroup->pName, g_pTestRegistry->pResult);
	}

	g_pTestCase = NULL;
	f_iNumberOfTestsRun++;
	
	return error_number;
}
