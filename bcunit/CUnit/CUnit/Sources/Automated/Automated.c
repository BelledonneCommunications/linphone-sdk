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
 *	Contains the Automated Test Interface implementation.
 *
 *	Created By     : Anil Kumar on ...(in month of Feb 2002)
 *	Last Modified  : 13/Feb/2002
 *	Comment        : Added initial automated interface functions to generate
 * 					 HTML based Run report.
 *	Email          : aksaharan@yahoo.com
 *
 * 	Modified       : 23/Jul/2002 (Anil Kumar)
 * 	Comment        : Changed HTML to XML Format file generation for Automated Tests.
 *	Email          : aksaharan@yahoo.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include "CUnit.h"
#include "TestDB.h"
#include "Util.h"
#include "TestRun.h"
#include "Automated.h"

static const char *szRunningTestGroup = NULL;
static char szTestListFileName[FILENAME_MAX] = "";
static char szTestResultFileName[FILENAME_MAX] = "";
static FILE* pTestListFile = NULL;
static FILE* pTestResultFile = NULL;

static int nIsFirstGroupResult = 1;

static void automated_registry_level_run(PTestRegistry pRegistry);
static void automated_list_all_tests(PTestRegistry pRegistry);
static void automated_list_group_tests(PTestGroup pGroup);

static int initialize_result_files(void);
static int uninitialize_result_files(void);

static void automated_run_all_tests(PTestRegistry pRegistry);

void automated_test_start_message_handler(const char* szTest, const char* szGroup);
void automated_test_complete_message_handler(const char* szTest, const char* szGroup, const PTestResult pTestResult);
void automated_all_tests_complete_message_handler(const PTestResult pTestResult);
void automated_group_init_failure_message_handler(const PTestGroup pGroup);

void automated_run_tests(void)
{
	PTestRegistry pRegistry = get_registry();
	/*
	 * 	To avoid user from cribbing about the output not coming onto 
	 * 	screen at the moment of SIGSEGV.
	 */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	if (initialize_result_files()) {
		fprintf(stderr, "\nFailed to create/initialize the result file.");
		return;
	}

	if (!pRegistry) {
		goto uninitialize_result;
		return;
	}

	set_test_start_handler(automated_test_start_message_handler);
	set_test_complete_handler(automated_test_complete_message_handler);
	set_all_test_complete_handler(automated_all_tests_complete_message_handler);
	set_group_init_failure_handler(automated_group_init_failure_message_handler);

	/* 
	 * Definitions which are used in the keeping the state of the test run
	 * to generate appropriate HTML tags and formats for the same.
	 */
	nIsFirstGroupResult = 1;

	automated_list_all_tests(pRegistry);
	automated_registry_level_run(pRegistry);

uninitialize_result:
	if (uninitialize_result_files()) {
		fprintf(stderr, "\nFailed to close/uninitialize the result file.");
		return;
	}
	
}

void set_output_filename(char* szFilename)
{
	if (!szFilename)
		return;
	
	sprintf(szTestListFileName, "%s-List.xml", szFilename);
	sprintf(szTestResultFileName, "%s-Run.xml", szFilename);
}

static void automated_registry_level_run(PTestRegistry pRegistry)
{
	automated_run_all_tests(g_pTestRegistry);
}

static void automated_run_all_tests(PTestRegistry pRegistry)
{
	szRunningTestGroup = NULL;
	fprintf(pTestResultFile,
			"<CUNIT_RESULT_LISTING> \n");
	run_all_tests();
	fprintf(pTestResultFile,"</CUNIT_RESULT_LISTING>\n");
}

void automated_test_start_message_handler(const char* szTest, const char* szGroup)
{
	if (!szRunningTestGroup || szRunningTestGroup != szGroup) {
		if (nIsFirstGroupResult)
			nIsFirstGroupResult = 0;
		else {
			fprintf(pTestResultFile,"</CUNIT_RUN_GROUP_SUCCESS> \n"
					"</CUNIT_RUN_GROUP> \n");
		}
		
		fprintf(pTestResultFile,"<CUNIT_RUN_GROUP> \n"
				"<CUNIT_RUN_GROUP_SUCCESS> \n"
				"\t<GROUP_NAME> %s </GROUP_NAME> \n",
				szGroup);

		szRunningTestGroup = szGroup;
	}
}

void automated_test_complete_message_handler(const char* szTest, const char* szGroup, const PTestResult pTestResult)
{
	if (pTestResult 
		&& pTestResult->pTestGroup && pTestResult->pTestGroup->pName == szGroup 
		&& pTestResult->pTestCase && pTestResult->pTestCase->pName == szTest) {
		fprintf(pTestResultFile,
				"<CUNIT_RUN_TEST_RECORD> \n"
				"\t<CUNIT_RUN_TEST_FAILURE> \n"
				"\t\t<TEST_NAME> %s </TEST_NAME> \n"
				"\t\t<FILE_NAME> %s </FILE_NAME> \n"
				"\t\t<LINE_NUMBER> %u </LINE_NUMBER> \n"
				"\t\t<CONDITION> %s </CONDITION> \n"
				"\t</CUNIT_RUN_TEST_FAILURE> \n"
				"</CUNIT_RUN_TEST_RECORD> \n",
				szTest, pTestResult->strFileName, pTestResult->uiLineNumber, 
				pTestResult->strCondition);
	} else {
			
		fprintf(pTestResultFile,
				"<CUNIT_RUN_TEST_RECORD> \n"
				"\t<CUNIT_RUN_TEST_SUCCESS> \n"
				"\t\t<TEST_NAME> %s </TEST_NAME> \n"
				"\t</CUNIT_RUN_TEST_SUCCESS> \n"
				"</CUNIT_RUN_TEST_RECORD> \n", szTest);
	}
}

void automated_all_tests_complete_message_handler(const PTestResult pTestResult)
{
	PTestRegistry pRegistry = get_registry();
	assert(pRegistry);
	
	fprintf(pTestResultFile,
			"<CUNIT_RUN_SUMMARY> \n"
			"\t<CUNIT_RUN_SUMMARY_RECORD> \n"
			"\t\t<TYPE> Test Groups </TYPE> \n"
			"\t\t<TOTAL> %u </TOTAL> \n"
			"\t\t<RUN> %u </RUN> \n"
			"\t\t<SUCCEDDED> - NA - </SUCCEDDED> \n"
			"\t\t<FAILED> - NA - </FAILED> \n"
			"\t</CUNIT_RUN_SUMMARY_RECORD> \n"
			"\t<CUNIT_RUN_SUMMARY_RECORD> \n"
			"\t\t<TYPE> Test Cases </TYPE> \n"
			"\t\t<TOTAL> %u </TOTAL> \n"
			"\t\t<RUN> %u </RUN> \n"
			"\t\t<SUCCEDDED> %u </SUCCEDDED> \n"
			"\t\t<FAILED> %u </FAILED> \n"
			"\t</CUNIT_RUN_SUMMARY_RECORD> \n"
			"</CUNIT_RUN_SUMMARY> \n", 
			pRegistry->uiNumberOfGroups, get_number_of_groups_run(), pRegistry->uiNumberOfTests, 
			get_number_of_tests_run(),	get_number_of_tests_run() - pRegistry->uiNumberOfFailures,
			pRegistry->uiNumberOfFailures);
}

void automated_group_init_failure_message_handler(const PTestGroup pGroup)
{
	if (nIsFirstGroupResult)
		nIsFirstGroupResult = 0;
	else {
		fprintf(pTestResultFile,
				"\t</CUNIT_RUN_GROUP_SUCCESS> \n"
				"\t</CUNIT_RUN_GROUP> \n");
	}
	
	fprintf(pTestResultFile,
		"<CUNIT_RUN_GROUP> \n"
		"\t<CUNIT_RUN_GROUP_FAILURE> \n"
		"\t\t<GROUP_NAME> %s </GROUP_NAME> \n"
		"\t\t<FAILURE_REASON> %s </FAILURE_REASON> \n"
		"\t</CUNIT_RUN_GROUP_FAILURE> \n"
		"</CUNIT_RUN_GROUP>	\n",
		pGroup->pName, "Initialize Failed");
	
	nIsFirstGroupResult = 1;
}

void automated_list_all_tests(PTestRegistry pRegistry)
{
	PTestGroup pGroup = pRegistry->pGroup;

	fprintf(pTestListFile, 
			"<CUNIT_LIST_TOTAL_SUMMARY> \n"
			"\t<CUNIT_LIST_TOTAL_SUMMARY_RECORD> \n"
			"\t\t<CUNIT_LIST_TOTAL_SUMMARY_RECORD_TEXT> Total Number of Test Groups </CUNIT_LIST_TOTAL_SUMMARY_RECORD_TEXT> \n"
			"\t\t<CUNIT_LIST_TOTAL_SUMMARY_RECORD_VALUE> %d </CUNIT_LIST_TOTAL_SUMMARY_RECORD_VALUE> \n"
			"\t</CUNIT_LIST_TOTAL_SUMMARY_RECORD> \n"
			"\t<CUNIT_LIST_TOTAL_SUMMARY_RECORD> \n"
			"\t\t<CUNIT_LIST_TOTAL_SUMMARY_RECORD_TEXT> Total Number of Test Cases </CUNIT_LIST_TOTAL_SUMMARY_RECORD_TEXT> \n"
			"\t\t<CUNIT_LIST_TOTAL_SUMMARY_RECORD_VALUE> %d </CUNIT_LIST_TOTAL_SUMMARY_RECORD_VALUE> \n"
			"\t</CUNIT_LIST_TOTAL_SUMMARY_RECORD> \n"
			"</CUNIT_LIST_TOTAL_SUMMARY> \n",
				pRegistry->uiNumberOfGroups, pRegistry->uiNumberOfTests);

	fprintf(pTestListFile, "<CUNIT_ALL_TEST_LISTING> \n");
	while (pGroup) {
		automated_list_group_tests(pGroup);
		pGroup = pGroup->pNext;
	}

	fprintf(pTestListFile, "</CUNIT_ALL_TEST_LISTING> \n");
}

void automated_list_group_tests(PTestGroup pGroup)
{
	PTestCase pTest = pGroup->pTestCase;

	fprintf(pTestListFile, 
			"<CUNIT_ALL_TEST_LISTING_GROUP> \n"
			"\t<CUNIT_ALL_TEST_LISTING_GROUP_DEFINITION> \n"
			"\t\t<GROUP_NAME> %s </GROUP_NAME> \n"
			"\t\t<INITIALIZE_VALUE> %s </INITIALIZE_VALUE> \n"
			"\t\t<CLEANUP_VALUE>	%s </CLEANUP_VALUE> \n"
			"\t\t<TEST_COUNT_VALUE> %d </TEST_COUNT_VALUE> \n"
			"\t</CUNIT_ALL_TEST_LISTING_GROUP_DEFINITION> \n",
			pGroup->pName, 	pGroup->pInitializeFunc ? "Yes" : "No",
			pGroup->pCleanupFunc ? "Yes" : "No", pGroup->uiNumberOfTests); 
	
	fprintf(pTestListFile, "\t<CUNIT_ALL_TEST_LISTING_GROUP_TESTS> \n");
	while (pTest) {
		fprintf(pTestListFile, "\t\t<TEST_CASE_NAME> %s </TEST_CASE_NAME> \n", pTest->pName);
		pTest = pTest->pNext;
	}

	fprintf(pTestListFile, 
			"\t</CUNIT_ALL_TEST_LISTING_GROUP_TESTS> \n"
			"</CUNIT_ALL_TEST_LISTING_GROUP> \n");
}

int initialize_result_files(void) 
{
	if (!strlen(szTestListFileName)) {
		return 1;
	}

	pTestListFile = fopen(szTestListFileName, "w");
	if (!pTestListFile) {
		return 1;
	}
	
	setvbuf(pTestListFile, NULL, _IONBF, 0);

	pTestResultFile = fopen(szTestResultFileName, "w");
	if (!pTestResultFile) {
		fclose(pTestListFile);
		return 1;
	}
	
	setvbuf(pTestResultFile, NULL, _IONBF, 0);

	fprintf(pTestListFile,
			"<?xml version=\"1.0\" ?> \n"
			"<?xml-stylesheet type=\"text/xsl\" href=\"CUnit-List.xsl\" ?> \n"
			"<!DOCTYPE CUNIT_TEST_LIST_REPORT SYSTEM \"CUnit-List.dtd\"> \n"
			"<CUNIT_TEST_LIST_REPORT> \n"
			"<CUNIT_HEADER/> \n");

	fprintf(pTestResultFile,
			"<?xml version=\"1.0\" ?> \n"
			"<?xml-stylesheet type=\"text/xsl\" href=\"CUnit-Run.xsl\" ?> \n"
			"<!DOCTYPE CUNIT_TEST_RUN_REPORT SYSTEM \"CUnit-Run.dtd\"> \n"
			"<CUNIT_TEST_RUN_REPORT> \n"
			"<CUNIT_HEADER/> \n");

	return 0;
}

int uninitialize_result_files(void)
{
	int nRetVal = 0;
	char* szTime = NULL;
	time_t tTime = 0;

	time(&tTime);
	szTime = ctime(&tTime);
	fprintf(pTestListFile, 
			"<CUNIT_FOOTER> File Generated By CUnit at %s </CUNIT_FOOTER> \n"
			"</CUNIT_TEST_LIST_REPORT>", szTime ? szTime : "");

	fprintf(pTestResultFile, 
			"<CUNIT_FOOTER> File Generated By CUnit at %s </CUNIT_FOOTER> \n"
			"</CUNIT_TEST_RUN_REPORT>", szTime ? szTime : "");

	if (!pTestListFile || !pTestResultFile)
		return 1;

	if (fclose(pTestResultFile)) {
		nRetVal = 1;
	}

	if (fclose(pTestListFile)) {
		nRetVal = 1;
	}

	return 0;
}
