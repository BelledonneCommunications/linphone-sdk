/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
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
 *	Contains the Console Test Interface	implementation.
 *
 *	Created By     : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified  : 19/Aug/2001
 *	Comment        : Added initial console interface functions without
 *						any run functionality.
 *	Email          : aksaharan@yahoo.com
 *
 *	Last Modified  : 24/Aug/2001 by Anil Kumar
 *	Comment	       : Added compare_strings, show_errors, list_groups,
 *						list_tests function declarations.
 *	Email          : aksaharan@yahoo.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "CUnit.h"
#include "TestDB.h"
#include "Util.h"
#include "TestRun.h"
#include "Console.h"

typedef enum
	{
		CONTINUE = 1,
		MOVE_UP,
		STOP
	}STATUS;

static const char *szRunningTestGroup = NULL;
static STATUS console_registry_level_run(PTestRegistry pRegistry);
static STATUS console_group_level_run(PTestGroup pGroup);

static void console_run_all_tests(PTestRegistry pRegistry);
static void console_run_group_tests(PTestGroup pGroup);
static void console_run_single_test(PTestGroup pGroup, PTestCase pTest);

void console_test_start_message_handler(const char* szTest, const char* szGroup);
void console_test_complete_message_handler(const char* szTest, const char* szGroup, const PTestResult pTestResult);
void console_all_tests_complete_message_handler(const PTestResult pTestResult);
void console_group_init_failure_message_handler(const PTestGroup pGroup);

static int select_test(PTestGroup pGroup, PTestCase* pTest);
static int select_group(PTestRegistry pRegistry, PTestGroup* pGroup);

static void list_groups(PTestRegistry pRegistry);
static void list_tests(PTestGroup pGroup);
static void show_failures(PTestRegistry pRegistry);


void console_run_tests(void)
{
	/*
	 * 	To avoid user from cribbing about the output not coming onto 
	 * 	screen at the moment of SIGSEGV.
	 */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	fprintf(stdout, "\n\n\t\t\tCUnit : A Unit testing framework for C.\n"
			    "\t\t\t    http://cunit.sourceforge.net/\n\n");

	if (!get_registry()) {
		fprintf(stderr, "\n\nTest Registry not Initialized.");
		return;
	}

	set_test_start_handler(console_test_start_message_handler);
	set_test_complete_handler(NULL);
	set_all_test_complete_handler(console_all_tests_complete_message_handler);
	set_group_init_failure_handler(console_group_init_failure_message_handler);
	
	console_registry_level_run(get_registry());
}


static STATUS console_registry_level_run(PTestRegistry pRegistry)
{
	char chChoice;
	char szTemp[256];
	PTestGroup pGroup = NULL;
	STATUS eStatus = CONTINUE;
	
	while (CONTINUE == eStatus)
	{
		fprintf(stdout, "\n|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
		                "\n(R)un all, (S)elect group, (L)ist groups, Show (F)ailures, (Q)uit"
						"\nEnter Command : ");
		chChoice = getchar();
		fgets(szTemp, sizeof(szTemp), stdin);

		switch (tolower(chChoice)) {
			case 'r':
				console_run_all_tests(g_pTestRegistry);
				break;

			case 's':
				if (select_group(pRegistry, &pGroup)) {
					if (STOP == console_group_level_run(pGroup))
						eStatus = STOP;
				}
				break;

			case 'l':
				list_groups(pRegistry);
				break;

			case 'f':
				show_failures(pRegistry);
				break;

			case 'q':
				eStatus = STOP;
				break;

			/* To stop gcc from cribbing */
			default:
				break;
		}
				
	}
	
	return eStatus;
}

static STATUS console_group_level_run(PTestGroup pGroup)
{
	char chChoice;
	char szTemp[256];
	PTestCase pTest = NULL;
	STATUS eStatus = CONTINUE;

	while (CONTINUE == eStatus) {
	
		fprintf(stdout, "\n--------------------------------------------------------------------------"
		                "\n(R)un All, (S)elect test, (L)ist tests, Show (F)ailures, (M)move up, (Q)uit"
						"\nEnter Command : ");
		chChoice = getchar();
		fgets(szTemp, sizeof(szTemp), stdin);

		switch (tolower(chChoice)) {
			case 'r':
				console_run_group_tests(pGroup);
				break;

			case 's':
				if (select_test(pGroup, &pTest)) {
					console_run_single_test(pGroup, pTest);
				}
				break;

			case 'l':
				list_tests(pGroup);
				break;

			case 'f':
				show_failures(get_registry());
				break;
			
			case 'm':
				eStatus = MOVE_UP;
				break;
				
			case 'q':
				eStatus = STOP;
				break;

			/* To stop gcc from cribbing */
			default:
				break;
		}
	}

	return eStatus;
}

static void console_run_all_tests(PTestRegistry pRegistry)
{
	szRunningTestGroup = NULL;
	run_all_tests();
}

static void console_run_group_tests(PTestGroup pGroup)
{
	szRunningTestGroup = NULL;
	run_group_tests(pGroup);
}

static void console_run_single_test(PTestGroup pGroup, PTestCase pTest)
{
	szRunningTestGroup = NULL;
	run_test(pGroup, pTest);
}

static int select_test(PTestGroup pGroup, PTestCase* pTest)
{
	char szTestName[MAX_TEST_NAME_LENGTH];
	
	fprintf(stdout,"\nEnter Test Name : ");
	fgets(szTestName, MAX_TEST_NAME_LENGTH, stdin);
	sscanf(szTestName, "%[^\n]s", szTestName);
	
	*pTest = get_test_by_name(szTestName, pGroup);
	if (*pTest) {
		return 1;
	}
	
	return 0;
}

static int select_group(PTestRegistry pRegistry, PTestGroup* pGroup)
{
	char szGroupName[MAX_GROUP_NAME_LENGTH];
	
	fprintf(stdout,"\nEnter Test Group Name : ");
	fgets(szGroupName, MAX_GROUP_NAME_LENGTH, stdin);
	sscanf(szGroupName, "%[^\n]s", szGroupName);
	
	*pGroup = get_group_by_name(szGroupName, pRegistry);
	if (*pGroup) {
		return 1;
	}

	return 0;
}

static void list_groups(PTestRegistry pRegistry)
{
	PTestGroup pCurGroup = NULL;
	
	assert(pRegistry);
	if (0 == pRegistry->uiNumberOfGroups) {
		fprintf(stdout, "\nNo test groups defined.");
		return;
	}
	assert(pRegistry->pGroup);
	
	fprintf(stdout, "\n=============================== Test Group List ===========================\n");
	fprintf(stdout, "Format -- (Group Name : Initialize Function : Cleanup Function : Number of Tests)\n");
	
	for (pCurGroup = pRegistry->pGroup; pCurGroup; pCurGroup = pCurGroup->pNext) {
		fprintf(stdout, "\n(%s : %s : %s : %d)", pCurGroup->pName,
				pCurGroup->pInitializeFunc ? "Init defined" : "Init not defined",
				pCurGroup->pCleanupFunc ? "Cleanup defined" : "Cleanup not defined",
				pCurGroup->uiNumberOfTests);
	}
	fprintf(stdout, "\n====================================="
					"\nTotal Number of Test Groups : %-d", pRegistry->uiNumberOfGroups);
}

static void list_tests(PTestGroup pGroup)
{
	PTestCase pCurTest = NULL;
	unsigned int uiCount = 0;
	
	assert(pGroup);
	if (0 == pGroup->uiNumberOfTests) {
		fprintf(stdout, "\nNo test groups defined.");
		return;
	}
	assert(pGroup->pTestCase);
	
	fprintf(stdout, "\n=============================== Test Group List ===========================\n");
	fprintf(stdout, "Format -- (Test Name)\n");
	
	for (uiCount = 1, pCurTest = pGroup->pTestCase; pCurTest; uiCount++, pCurTest = pCurTest->pNext) {
		fprintf(stdout, "\n%d> (%s)", uiCount, pCurTest->pName);
	}
	fprintf(stdout, "\n====================================="
					"\nTotal Number of Tests : %-d", pGroup->uiNumberOfTests);
}

static void show_failures(PTestRegistry pRegistry)
{
	PTestResult pResult = NULL;
	int i;
	
	assert(pRegistry);
	if (0 == pRegistry->uiNumberOfFailures) {
		fprintf(stdout, "\nNo failures.");
		return;
	}
	assert(pRegistry->pResult);
	
	fprintf(stdout, "\n============================= Test Case Failure List =========================\n");

	for (i = 1, pResult = pRegistry->pResult; pResult; pResult = pResult->pNext, i++) {
		fprintf(stdout, "%d>  %s:%d : (%s : %s) : %s\n", i,
				pResult->strFileName ? pResult->strFileName : "",
				pResult->uiLineNumber,
				pResult->pTestGroup ? pResult->pTestGroup->pName : "",
				pResult->pTestCase ? pResult->pTestCase->pName : "",
				pResult->strCondition ? pResult->strCondition : "");
	}
	fprintf(stdout, "\n======================================="
					"\nTotal Number of Failures : %-d", pRegistry->uiNumberOfFailures);
}


void console_test_start_message_handler(const char* szTest, const char* szGroup)
{
	/*
	 * 	Comparing the Addresses rather than the Group Names.
	 */
	if (!szRunningTestGroup || szRunningTestGroup != szGroup) {
		fprintf(stdout,"\nRunning Group : %s",szGroup);
		fprintf(stdout,"\n\tRunning test : %s",szTest);
		szRunningTestGroup = szGroup;
	}
	else {
		fprintf(stdout,"\n\tRunning test : %s",szTest);
	}
}

void console_test_complete_message_handler(const char* szTest, const char* szGroup, const PTestResult pTestResult)
{
	/*
	 * 	For console interface do nothing. This is useful only for the test
	 * 	interface where UI is involved.
	 */
}

void console_all_tests_complete_message_handler(const PTestResult pTestResult)
{
	PTestRegistry pRegistry = get_registry();
	assert(pRegistry);
	
	fprintf(stdout,"\n\n--Completed %d Groups, %d Test run, %d succeded and %d failed.",
		get_number_of_groups_run(), get_number_of_tests_run(),
		get_number_of_tests_run() - pRegistry->uiNumberOfFailures, pRegistry->uiNumberOfFailures);
}

void console_group_init_failure_message_handler(const PTestGroup pGroup)
{
	/*
	 * TODO for Console interface.
	 */
}
