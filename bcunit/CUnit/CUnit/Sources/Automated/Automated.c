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
 *	Created By     : Anil Kumar on ...(in month of Feb 2002)
 *	Last Modified  : 13/Feb/2002
 *	Comment        : Added initial automated interface functions to generate
 * 					 HTML based Run report.
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

#ifdef WIN32
#define NAME_MAX 256
#endif

static const char *szRunningTestGroup = NULL;
static char szTestListFileName[NAME_MAX] = "";
static char szTestResultFileName[NAME_MAX] = "";
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
		fprintf(pTestListFile, "\n<H2><FONT color=red>Test Registry not Initialized.</FONT></H2>");
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
	
	sprintf(szTestListFileName, "%s-list.html", szFilename);
	sprintf(szTestResultFileName, "%s-result.html", szFilename);
}

static void automated_registry_level_run(PTestRegistry pRegistry)
{
	automated_run_all_tests(g_pTestRegistry);
}

static void automated_run_all_tests(PTestRegistry pRegistry)
{
	szRunningTestGroup = NULL;
	fprintf(pTestResultFile,"\n<TABLE COLS=4 WIDTH=90%% ALIGN=CENTER");
	fprintf(pTestResultFile,"\n<TR> <TD WIDTH=25%%></TD> <TD WIDTH=25%%></TD> <TD WIDTH=25%%></TD> <TD WIDTH=25%%></TD> </TR>");
	run_all_tests();
	fprintf(pTestResultFile,"\n</TABLE>");
}

void automated_test_start_message_handler(const char* szTest, const char* szGroup)
{
	if (!szRunningTestGroup || szRunningTestGroup != szGroup) {
		if (nIsFirstGroupResult)
			nIsFirstGroupResult = 0;
		else
			fprintf(pTestResultFile,"\n<BR>");
		
		fprintf(pTestResultFile,"\n<TR><TD COLSPAN=3 BGCOLOR=f0e0f0>Running Group <A HREF=\"%s#%s\">%s</A> ...</TD> <TD BGCOLOR=f0e0f0> </TD></TR>", szTestListFileName, szGroup, szGroup);
		fprintf(pTestResultFile,"\n\t<TR><TD BGCOLOR=e0f0d0 WIDTH=5%%></TD> <TD COLSPAN=2 BGCOLOR=e0f0d0>Running test <A HREF=\"%s#%s-%s\">%s</A> ...</TD>", szTestListFileName, szGroup, szTest, szTest);
		szRunningTestGroup = szGroup;
	}
	else {
		fprintf(pTestResultFile,"\n\t<TR><TD BGCOLOR=e0f0d0 WIDTH=5%%></TD> <TD COLSPAN=2 BGCOLOR=e0f0d0>Running test <A HREF=\"%s#%s-%s\">%s</A> ...</TD>", szTestListFileName, szGroup, szTest, szTest);
	}
}

void automated_test_complete_message_handler(const char* szTest, const char* szGroup, const PTestResult pTestResult)
{
	if (pTestResult 
		&& pTestResult->pTestGroup && pTestResult->pTestGroup->pName == szGroup 
		&& pTestResult->pTestCase && pTestResult->pTestCase->pName == szTest) {
		fprintf(pTestResultFile,"\n\t<TD BGCOLOR=ff5050> %s </TD> </TR>", "Failed");
		fprintf(pTestResultFile,"\n\t<TR> <TD COLSPAN=4 BGCOLOR=ff9090> "
				"\n\t\t<TABLE WIDTH=100%%> "
				"\n\t\t<TR> <TD WIDTH=15%%> File Name </TD> <TD WIDTH=50%% BGCOLOR=e0eee0> %s </TD> <TD WIDTH=20%%> Line Number </TD> <TD WIDTH=10%% BGCOLOR=e0eee0> %u </TD>  </TR>"
				"\n\t\t<TR> <TD WIDTH=15%%> Condition </TD> <TD COLSPAN=3 WIDTH=85%% BGCOLOR=e0eee0> %s </TD> </TR>"
				"\n\t\t</TABLE> "
				"</TD> </TR>", pTestResult->strFileName, pTestResult->uiLineNumber, pTestResult->strCondition);
	} else {
		fprintf(pTestResultFile,"\n\t<TD BGCOLOR=50ff50> %s </TD> </TR>", "Passed");
	}
}

void automated_all_tests_complete_message_handler(const PTestResult pTestResult)
{
	PTestRegistry pRegistry = get_registry();
	assert(pRegistry);
	
	fprintf(pTestResultFile,"\n<BR><BR>"
		"\n\t<TR><TD COLSPAN=4><TABLE WIDTH=100%%>"
		"\n\t\t<TR BGCOLOR=SKYBLUE><TH COLSPAN=5> Cumulative Summary for Run </TH> </TR>"
		"\n\t\t<TR BGCOLOR=LIGHTBLUE><TH WIDTH=20%% BGCOLOR=ffffco> Type </TH> <TH WIDTH=20%% BGCOLOR=ffffco ALIGN=CENTER> Total </TH> <TH WIDTH=20%% BGCOLOR=ffffco ALIGN=CENTER> Run </TH> <TH WIDTH=20%% BGCOLOR=ffffco ALIGN=CENTER> Succedded </TH> <TH WIDTH=20%% BGCOLOR=ffffco ALIGN=CENTER> Failed </TH></TR>"
		"\n\t\t<TR><TD WIDTH=20%% BGCOLOR=GREY> Test Groups </TD> <TD WIDTH=20%% ALIGN=CENTER BGCOLOR=LIGHTGREEN> %u </TD> <TD WIDTH=20%% ALIGN=CENTER BGCOLOR=LIGHTGREEN> %u </TD> <TD WIDTH=20%% ALIGN=CENTER BGCOLOR=LIGHTGREEN> -  NA - </TD> <TD WIDTH=20%% ALIGN=CENTER BGCOLOR=LIGHTGREEN> - NA - </TD></TR>"
		"\n\t\t<TR><TD WIDTH=20%% BGCOLOR=GREY> Test Cases </TD> <TD WIDTH=20%% ALIGN=CENTER BGCOLOR=LIGHTGREEN> %u </TD> <TD WIDTH=20%% ALIGN=CENTER BGCOLOR=LIGHTGREEN> %u </TD> <TD WIDTH=20%% ALIGN=CENTER BGCOLOR=LIGHTGREEN> %u </TD> <TD WIDTH=20%% ALIGN=CENTER BGCOLOR=LIGHTGREEN> %u </TD></TR>"
		"\n\t</TABLE></TD></TR>",
		pRegistry->uiNumberOfGroups, get_number_of_groups_run(), pRegistry->uiNumberOfTests, 
		get_number_of_tests_run(),	get_number_of_tests_run() - pRegistry->uiNumberOfFailures, pRegistry->uiNumberOfFailures);
}

void automated_group_init_failure_message_handler(const PTestGroup pGroup)
{
	if (nIsFirstGroupResult)
		nIsFirstGroupResult = 0;
	else
		fprintf(pTestResultFile, "\n<BR>");
	fprintf(pTestResultFile,"\n<TR><TD COLSPAN=3 BGCOLOR=f0b0f0>Running Group <A HREF=\"%s#%s\">%s</A> ...</TD> <TD BGCOLOR=ff7070> Initialize failed </TD></TR>", szTestListFileName, pGroup->pName, pGroup->pName);
}

void automated_list_all_tests(PTestRegistry pRegistry)
{
	PTestGroup pGroup = pRegistry->pGroup;
	int nFirstRecord = 1;

	fprintf(pTestListFile, "<TABLE ALIGN=CENTER WIDTH=50%%>");
	fprintf(pTestListFile, "\n\t<TR> <TD BGCOLOR=f0f0e0 WIDTH=70%%> Total Number of Test Groups </TD><TD BGCOLOR=f0e0e0> %d </TD>", pRegistry->uiNumberOfGroups);
	fprintf(pTestListFile, "\n\t<TR> <TD BGCOLOR=f0f0e0 WIDTH=70%%> Total Number of Test Cases </TD><TD BGCOLOR=f0e0e0> %d </TD>", pRegistry->uiNumberOfTests);
	fprintf(pTestListFile, "\n</TABLE>");
	fprintf(pTestListFile, "<DIV ALIGN=CENTER><H3><I>Listing of all the tests.</I></H3></DIV>");
	fprintf(pTestListFile, "\n<P>\n<TABLE ALIGN=CENTER WIDTH=90%%>");

	while (pGroup) {

		if (nFirstRecord) {
			automated_list_group_tests(pGroup);
			nFirstRecord = 0;
		} else {
			fprintf(pTestListFile, "\n\n\t<BR>");
			automated_list_group_tests(pGroup);
		}
		
		pGroup = pGroup->pNext;
	}

	fprintf(pTestListFile, "</TABLE>");
}

void automated_list_group_tests(PTestGroup pGroup)
{
	PTestCase pTest = pGroup->pTestCase;
	int nFirstRecord = 1;

	fprintf(pTestListFile, "\n\t<TR> <TD BGCOLOR=f0e0f0 WIDTH=20%%> <A NAME=\"%s\"></A> Group Name </TD> <TD COLSPAN=5 BGCOLOR=e0f0f0 WIDTH=70%%> %s </TD> </TR> "
			"\n\t<TR> <TD BGCOLOR=f0e0f0 WIDTH=20%%>Initialize Defined </TD> <TD BGCOLOR=e0f0f0 WIDTH=13%%> %s </TD> "
				"<TD BGCOLOR=f0e0f0 WIDTH=20%%>Cleanup Defined </TD> <TD BGCOLOR=e0f0f0 WIDTH=13%%> %s </TD> "
				"<TD BGCOLOR=f0e0f0 WIDTH=20%%>Number of Tests </TD> <TD BGCOLOR=e0f0f0 WIDTH=13%%> %u </TD> </TR>",
			pGroup->pName, pGroup->pName, 	pGroup->pInitializeFunc ? "Yes" : "No",
			pGroup->pCleanupFunc ? "Yes" : "No", pGroup->uiNumberOfTests); 
	
	nFirstRecord = 1;
	while (pTest) {

		if (nFirstRecord) {
			fprintf(pTestListFile, "\n\t<TR> <TD ROWSPAN=%u BGCOLOR=e0f0d0 WIDTH=20%% ALIGN=CENTER VALIGN=MIDDLE> Test Cases </TD> <TD COLSPAN=5 BGCOLOR=e0e0d0> <A NAME=\"%s-%s\"> </A> %s </TD> </TR>", pGroup->uiNumberOfTests, pGroup->pName, pTest->pName, pTest->pName);
			nFirstRecord = 0;
		} else {
			fprintf(pTestListFile, "\n\t<TR> <TD COLSPAN=5 BGCOLOR=e0e0d0> <A NAME=\"%s-%s\"> </A> %s </TD> </TR>", pGroup->pName, pTest->pName, pTest->pName);
		}

		pTest = pTest->pNext;
	}
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
			"<HTML>"
				"<HEAD>\n"
					"<TITLE>CUnit - Logical Test Case Organization in Test Registry.</TITLE>\n"
				"</HEAD>\n"
				"<BODY bgcolor=e0e0f0>"
				"\n<H2 ALIGN=CENTER>CUnit - A Unit testing framework for C."
					"\n\t<BR><U> <A HREF=\"http://cunit.sourceforge.net/\">http://cunit.sourceforge.net/</A></U>"
				"\n</H2>");

	fprintf(pTestResultFile,
			"<HTML>"
				"<HEAD>\n"
					"<TITLE>CUnit - Run Summary for All Tests.</TITLE>\n"
				"</HEAD>\n"
				"<BODY bgcolor=e0e0f0>"
				"\n<H2 ALIGN=CENTER>CUnit - A Unit testing framework for C."
					"\n\t<BR><U> <A HREF=\"http://cunit.sourceforge.net/\">http://cunit.sourceforge.net/</A></U>"
				"\n</H2>");

	return 0;
}

int uninitialize_result_files(void)
{
	int nRetVal = 0;
	char* szTime = NULL;
	time_t tTime = 0;

	time(&tTime);
	szTime = ctime(&tTime);
	fprintf(pTestListFile, "\n<BR><BR> <HR ALIGN=CENTER WIDTH=90%% COLOR=RED> \n<H5 ALIGN=CENTER>File Generated By CUnit at %s</H5>", szTime ? szTime : "\"Failed to Generate Time\"");
	fprintf(pTestListFile, "</BODY> \n</HEAD>");

	fprintf(pTestResultFile, "\n<BR><BR> <HR ALIGN=CENTER WIDTH=90%% COLOR=RED> \n<H5 ALIGN=CENTER>File Generated By CUnit at %s</H5>", szTime ? szTime : "\"Failed to Generate Time\"");
	fprintf(pTestResultFile, "</BODY> \n</HEAD>");

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
