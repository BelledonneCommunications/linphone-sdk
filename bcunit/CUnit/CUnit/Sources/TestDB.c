/*
 *	Contains the Registry/TestGroup/Testcase management Routine 
 *	implementation.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CUnit.h"
#include "TestDB.h"

/*
 *	Global/Static Definitions
 */
int	error_number;

PTestRegistry	g_pTestRegistry = NULL;

/*
 * Private function declaration
 */
static void cleanup_test_registry(PTestRegistry pRegistry);
static void cleanup_test_group(PTestGroup pGroup);
static void insert_group_into_registry(PTestRegistry pTestRegistry, PTestGroup pGroup);
static void insert_test_into_group(PTestGroup pGroup, PTestCase pTest);

static char* get_error_desc(const int error);
/*
 *	Public Interface functions
 */
int initialize_registry(void)
{
	error_number = CUE_SUCCESS;
	if (NULL != g_pTestRegistry)
		cleanup_registry();

	g_pTestRegistry = (PTestRegistry)malloc(sizeof(TestRegistry));
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
	cleanup_test_registry(g_pTestRegistry);
	free(g_pTestRegistry);
	g_pTestRegistry = NULL;
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

	pRetValue = (PTestGroup)malloc(sizeof(TestGroup));
	if (NULL == pRetValue) {
		error_number = CUE_NOMEMORY;
		goto exit;
	}
	
	pRetValue->pName = (char *)malloc(strlen(strName)+1);
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
	free(pRetValue);
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
	
	pRetValue = (PTestCase)malloc(sizeof(TestCase));
	if (NULL ==pRetValue) {
		error_number = CUE_NOMEMORY;
		goto exit;
	}

	pRetValue->pName = (char *)malloc(strlen(strName)+1);
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
	free(pRetValue);
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
			free(pCurGroup->pName);

		free(pCurGroup);
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
			free(pCurTest->pName);

		free(pCurTest);
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
			"", "", "", "", "", "", /* From 24 - 29 No Error Codes Defined */
		"Test not Defined", /* CUE_NOTEST - 30 */
			"Test Name not Defined", /* CUE_NO_TESTNAME - 31 */
		"Undefined Error"
		};
	
	iMaxIndex = sizeof(ErrorDescription)/sizeof(char *) - 1;
	if (iError > iMaxIndex)
		return ErrorDescription[iMaxIndex];
	else
		return ErrorDescription[iError];
}
