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
 *	Contains the Curses based Test Interface implementation.
 *
 *	Created By     : Anil Kumar on ...(on 1st Nov. 2001)
 *	Last Modified  : 01/Nov/2001
 *	Comment        : Started Curses based interface for CUnit.
 *	Email          : aksaharan@yahoo.com
 *
 *	Last Modified  : 04/Nov/2001 by Anil Kumar
 *	Comment	       : Added Scrolling Capability to the Details Window.
 *	Email          : aksaharan@yahoo.com
 *	
 *	Last Modified  : 24/Nov/2001 by Anil Kumar
 *	Comment	       : Added List and Show Failure Capability to the Details Window.
 *					 	Also added group initialization failure message handler.
 *	Email          : aksaharan@yahoo.com
 */

#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <curses.h>

#include "CUnit.h"
#include "TestDB.h"
#include "Util.h"
#include "TestRun.h"
#include "Curses.h"

/*
 * Type Definitions
 */
enum bool { false = 0, true };

typedef enum
{
	CONTINUE = 1,
	MOVE_UP,
	STOP
}STATUS;

typedef enum
{
	MAIN_MENU = 1,
	GROUP_MENU
}MENU_TYPE;

typedef struct
{
	WINDOW* pMainWin;
	WINDOW* pTitleWin;
	WINDOW* pProgressWin;
	WINDOW* pSummaryWin;
	WINDOW* pRunSummaryWin;
	WINDOW* pDetailsWin;
	WINDOW* pOptionsWin;
}APPWINDOWS;

typedef struct
{
	WINDOW*			pPad;
	unsigned int 	uiRows;
	unsigned int 	uiColumns;
	unsigned int 	uiPadRow;
	unsigned int 	uiPadCol;
	unsigned int 	uiWinLeft;
	unsigned int 	uiWinTop;
	unsigned int 	uiWinRows;
	unsigned int 	uiWinColumns;
}APPPAD;

/*
 * Constants definitions
 */
const unsigned int STRING_LENGTH = 128;

const char* MAIN_OPTIONS  = "(R)un  (S)elect Group  (L)ist  (F)ailures  (Q)uit";
const char* GROUP_OPTIONS = "(R)un  (S)elect Test  (L)ist  (F)ailures  (U)p  (Q)uit";

/*
 * Color Pairs Initialized for the Various Parameter Display
 */
const int CLEAR_COLOR 				= 1;
const int TITLE_COLOR 				= 2;
const int PROGRESS_BACKGROUND_COLOR	= 3;
const int PROGRESS_SUCCESS_COLOR 	= 4;
const int PROGRESS_FAILURE_COLOR 	= 5;
const int MENU_COLOR 				= 6;

/*
 * Global Definitions
 */
const char* const g_szPackageTitle = "CUnit - A Unit Testing Framework for \'C\'";
const char* const g_szSite = "http:\\\\cunit.sourceforge.net\\";
const char* const g_szProgress = "Progress    ";


const char* g_szOptions = NULL;
const char* g_szDetailsTitle = " Details Window";
MENU_TYPE g_eMenu = MAIN_MENU;

const char* g_szSummary = "Run : %6u   Tests Success : %6u   Failed : %6u";
const char* g_szRunSummary = "Running test  \'%s\' of Group \'%s\'";
const char* g_szGroupRunSummary = "Total Groups : %6u  Groups Run : %6u";
const char* g_szTest = NULL;
const char* g_szGroup = NULL;

unsigned int g_uiTotalTests = 0;
unsigned int g_uiTestsRun = 0;
unsigned int g_uiTestsSkipped = 0;
unsigned int g_uiTestsFailed = 0;
unsigned int g_uiTestsRunSuccessfull = 0;

unsigned int g_uiTotalGroups = 0;
unsigned int g_uiGroupsSkipped = 0;

short g_nLeft, g_nTop, g_nWidth, g_nHeight;

APPWINDOWS application_windows = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
APPPAD	details_pad = {NULL, 0, 0, 0, 0, 0, 0, 0, 0};

/*
 * Function Declarations
 */
void curses_run_tests(void);

bool initialize_windows(void);
void uninitialize_windows(void);

void refresh_windows(void);
void refresh_title_window(void);
void refresh_progress_window(void);
void refresh_summary_window(void);
void refresh_run_summary_window(void);
void refresh_details_window(void);
void refresh_options_window(void);

bool create_pad(APPPAD* pPad, WINDOW* pParent, unsigned int uiRows,
		unsigned int uiCols);
void scroll_window(int nCommand, APPPAD* pPad, void (*parent_refresh)(void));

bool test_initialize(void);

void show_progress_bar(void);
const char* get_hotkey(const char* szStr, int* pPos);
void read_input_string(const char szPropmt[], char szValue[], int nBytes);

STATUS 	curses_registry_level_run(PTestRegistry pRegistry);
STATUS curses_group_level_run(PTestGroup pGroup);

static void curses_run_all_tests(PTestRegistry pRegistry);
static void curses_run_group_tests(PTestGroup pGroup);
static void curses_run_single_test(PTestGroup pGroup, PTestCase pTest);

void curses_test_start_message_handler(const char* szTest, const char* szGroup);
void curses_test_complete_message_handler(const char* szTest, const char* szGroup, const PTestResult pTestResult);
void curses_all_tests_complete_message_handler(const PTestResult pTestResult);
void curses_group_init_failure_message_handler(const PTestGroup pGroup);

static void list_groups(PTestRegistry pRegistry);
static void list_tests(PTestGroup pGroup);
static void show_failures(PTestRegistry pRegistry);

static void reset_run_parameters(void);

void curses_run_tests(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	g_szOptions = MAIN_OPTIONS;
	if (!initialize_windows())
		return;

	if (!test_initialize())
		goto test_initialize_fail;
	
	curses_registry_level_run(get_registry());
	
	/*
	g_uiTotalTests = 3000;
	{
		int i;
		int iSucc = 0;
		for (i = 0; i < 3000; i++, iSucc++) {
			if (i && !(i%2000))
				iSucc--;

			g_uiTestsRun = i + 1;
			g_uiTestsRunSuccessfull = iSucc + 1;
			refresh_progress_window();
			refresh_summary_window();
			refresh_run_summary_window();
		}
	}

	g_uiTestsRunSuccessfull = g_uiTestsRun = 0;
	refresh_windows();
	{
		int i;
		int iSucc = -1;
		for (i = 0; i < 3000; i++, iSucc++) {
			if (i && !(i%2000))
				iSucc++;

			g_uiTestsRun = i + 1;
			g_uiTestsRunSuccessfull = iSucc + 1;
			refresh_progress_window();
			refresh_summary_window();
			refresh_run_summary_window();
		}
	}
	*/

test_initialize_fail:
	uninitialize_windows();
}

bool initialize_windows(void)
{
	bool bStatus = false;

	if (NULL == (application_windows.pMainWin = initscr()))
		goto main_fail;

	start_color();
	
	g_nLeft = application_windows.pMainWin->_begx;
	g_nTop = application_windows.pMainWin->_begy;
	g_nWidth = application_windows.pMainWin->_maxx;
	g_nHeight = application_windows.pMainWin->_maxy;
	
	if (NULL == (application_windows.pTitleWin = newwin(3, g_nWidth, 0, 0)))
		goto title_fail;
	
	if (NULL == (application_windows.pProgressWin = newwin(2, g_nWidth, 3, 0)))
		goto progress_fail;

	if (NULL == (application_windows.pSummaryWin = newwin(1, g_nWidth, 5, 0)))
		goto summary_fail;

	if (NULL == (application_windows.pRunSummaryWin = newwin(1, g_nWidth, 6, 0)))
		goto run_summary_fail;

	if (NULL == (application_windows.pDetailsWin = newwin(g_nHeight - g_nTop - 7 , g_nWidth, 7, 0)))
		goto details_fail;

	if (NULL == (application_windows.pOptionsWin = newwin(1, g_nWidth, g_nHeight - g_nTop, 0)))
		goto option_fail;

	curs_set(0);
	noecho();
	cbreak();
	keypad(application_windows.pMainWin, TRUE);
	init_pair(CLEAR_COLOR, COLOR_WHITE, COLOR_BLACK);
	init_pair(TITLE_COLOR, COLOR_WHITE, COLOR_BLACK);
	init_pair(PROGRESS_BACKGROUND_COLOR, COLOR_BLACK, COLOR_WHITE);
	init_pair(PROGRESS_SUCCESS_COLOR, COLOR_WHITE, COLOR_GREEN);
	init_pair(PROGRESS_FAILURE_COLOR, COLOR_WHITE, COLOR_RED);
	init_pair(MENU_COLOR, COLOR_WHITE, COLOR_BLACK);

	refresh_windows();
	bStatus = true;
	goto main_fail;

	/*
	 * Error Handlers for all the stages.
	 */
option_fail:
	delwin(application_windows.pDetailsWin);
	
details_fail:
	delwin(application_windows.pRunSummaryWin);
	
run_summary_fail:
	delwin(application_windows.pSummaryWin);
	
summary_fail:
	delwin(application_windows.pProgressWin);
	
progress_fail:
	delwin(application_windows.pTitleWin);
	
title_fail:
	endwin();
		
main_fail:
	return bStatus;

}

void uninitialize_windows(void)
{
	curs_set(1);
	echo();
	nocbreak();
	keypad(application_windows.pMainWin, FALSE);
	
	if (details_pad.pPad)
		delwin(details_pad.pPad);

	delwin(application_windows.pOptionsWin);
	delwin(application_windows.pDetailsWin);
	delwin(application_windows.pRunSummaryWin);
	delwin(application_windows.pSummaryWin);
	delwin(application_windows.pProgressWin);
	delwin(application_windows.pTitleWin);
	
	clear();
	refresh();
	endwin();
}

void refresh_windows(void)
{
	refresh();
	
	g_nLeft = application_windows.pMainWin->_begx;
	g_nTop = application_windows.pMainWin->_begy;
	g_nWidth = application_windows.pMainWin->_maxx;
	g_nHeight = application_windows.pMainWin->_maxy;

	refresh_title_window();
	refresh_progress_window();
	refresh_run_summary_window();
	refresh_summary_window();
	refresh_details_window();
	refresh_options_window();
}

void refresh_title_window(void)
{
	static bool bFirstTime = true;

	if (!bFirstTime)
		return;

	wattrset(application_windows.pTitleWin, A_BOLD | COLOR_PAIR(TITLE_COLOR));
	mvwprintw(application_windows.pTitleWin, 
		0, g_nLeft + (g_nWidth - strlen(g_szPackageTitle))/2,
		"%s", g_szPackageTitle);
	
	wattrset(application_windows.pTitleWin, A_BOLD | A_UNDERLINE | COLOR_PAIR(TITLE_COLOR));
	mvwprintw(application_windows.pTitleWin, 1, g_nLeft + (g_nWidth - strlen(g_szSite))/2,
		"%s", g_szSite);
	wattrset(application_windows.pTitleWin, A_NORMAL);

	wrefresh(application_windows.pTitleWin);
}

void refresh_progress_window(void)
{
	wattrset(application_windows.pProgressWin, A_BOLD);
	mvwprintw(application_windows.pProgressWin, 0, 1, (char *)g_szProgress);
	show_progress_bar();
	wrefresh(application_windows.pProgressWin);
}

void refresh_summary_window(void)
{
	char szTemp[STRING_LENGTH];

	memset(szTemp, 0, sizeof(szTemp));
	snprintf(szTemp, STRING_LENGTH, g_szSummary, g_uiTestsRun, g_uiTestsRunSuccessfull,
		g_uiTestsRun - g_uiTestsRunSuccessfull);
	werase(application_windows.pSummaryWin);
	mvwprintw(application_windows.pSummaryWin, 0, 1, "%s", szTemp);
	wrefresh(application_windows.pSummaryWin);
}

void refresh_run_summary_window(void)
{
	char szTemp[STRING_LENGTH];
	
	if (g_szTest && g_szGroup) {
		snprintf(szTemp, STRING_LENGTH, g_szRunSummary, g_szTest, g_szGroup);
	}
	else {
		snprintf(szTemp, STRING_LENGTH, "%s", "");
	}
	werase(application_windows.pRunSummaryWin);
	mvwprintw(application_windows.pRunSummaryWin, 0, 1, "%s", szTemp);
	wrefresh(application_windows.pRunSummaryWin);
}

void refresh_details_window(void)
{
	box(application_windows.pDetailsWin, ACS_VLINE, ACS_HLINE);
	mvwprintw(application_windows.pDetailsWin, 0,
		g_nLeft + (g_nWidth - strlen(g_szDetailsTitle))/2, "%s", g_szDetailsTitle);
	scrollok(application_windows.pDetailsWin, TRUE);
	wrefresh(application_windows.pDetailsWin);
	
	if (details_pad.pPad) {
		prefresh(details_pad.pPad, details_pad.uiPadRow, details_pad.uiPadCol, 
			details_pad.uiWinTop, details_pad.uiWinLeft, 
			details_pad.uiWinTop + details_pad.uiWinRows,
			details_pad.uiWinLeft + details_pad.uiWinColumns);
	}
}

void refresh_options_window(void)
{
	int nPos = 0;
	const char* szHotKey = NULL;

	wclear(application_windows.pOptionsWin);
	mvwprintw(application_windows.pOptionsWin, 0, 1, "%s", g_szOptions);

	get_hotkey(g_szOptions, NULL);
	wattron(application_windows.pOptionsWin, A_BOLD);
	while (NULL != (szHotKey = get_hotkey((const char*)NULL, &nPos))) {
		mvwaddstr(application_windows.pOptionsWin, 0, nPos + 1, szHotKey);
	}
	wattroff(application_windows.pOptionsWin, A_BOLD);
	
	wrefresh(application_windows.pOptionsWin);
}

void show_progress_bar(void)
{
	int nLength = 0;
	int nIndex = 0;
	int nStart = strlen(g_szProgress);
	int nColorID = 0;

	if (0 == (g_uiTestsRun + g_uiTestsSkipped)) {
		nLength = g_nWidth - g_nLeft - nStart - 6;
		nColorID = PROGRESS_BACKGROUND_COLOR;
	}
	else {
		nLength = (g_nWidth - g_nLeft - nStart - 6) * ((double)(g_uiTestsRun + g_uiTestsSkipped) / g_uiTotalTests);
		nColorID = (!g_uiTestsSkipped && g_uiTestsRun == g_uiTestsRunSuccessfull) 
						? PROGRESS_SUCCESS_COLOR 
						: PROGRESS_FAILURE_COLOR;
	}

	wattron(application_windows.pProgressWin, A_BOLD | COLOR_PAIR(nColorID));
	for (nIndex = 0; nIndex < nLength; nIndex++) {
		mvwprintw(application_windows.pProgressWin, 0, nStart + nIndex, " ");
	}
	wattroff(application_windows.pProgressWin, COLOR_PAIR(nColorID));
}

bool test_initialize(void)
{
	set_test_start_handler(curses_test_start_message_handler);
	set_test_complete_handler(curses_test_complete_message_handler);
	set_all_test_complete_handler(curses_all_tests_complete_message_handler);
	set_group_init_failure_handler(curses_group_init_failure_message_handler);
	return true;
}


const char* get_hotkey(const char* szStr, int* pPos)
{
	static char szTemp[128] = "";
	static char szString[128] = "";
	static int nPos = 0;

	int nTempIndex;
	char* pS = NULL;

	if (szStr) {
		nPos = 0;
		strcpy(szString, szStr);
		return szString;
	}

	memset(szTemp, 0, sizeof(szTemp));
	for (nTempIndex = 0, pS = szString + nPos; *pS; nPos++, pS++) {
		if (!nTempIndex && '(' == *pS) {
			szTemp[nTempIndex++] = *pS;
			*pPos = nPos;
		}
		else if (nTempIndex && ')' == *pS) {
			szTemp[nTempIndex++] = *pS;
			szTemp[nTempIndex++] = '\0';
			return szTemp; 
		}
		else if (nTempIndex) {
			szTemp[nTempIndex++] = *pS;
		}
	}
	
	return NULL;
}

STATUS curses_registry_level_run(PTestRegistry pRegistry)
{
	char szGroupName[STRING_LENGTH];
	PTestGroup pGroup = NULL;
	bool bContinue = true; 

	while (bContinue) {
		int option = toupper(getch());
		
		switch (option) {
			case 'R':
				curses_run_all_tests(pRegistry);
				break;

			case 'S':
				read_input_string("Enter Group Name : ", szGroupName, STRING_LENGTH);
				refresh_details_window();
				if (NULL != (pGroup = get_group_by_name(szGroupName, pRegistry))) {
					if (STOP == curses_group_level_run(pGroup))
						bContinue = false;
					g_szOptions = MAIN_OPTIONS;
					refresh_options_window();
				}
				break;

			case 'L':
				list_groups(pRegistry);
				break;

			case 'F':
				show_failures(pRegistry);
				break;

			case 'Q':
				return bContinue = false;

			case KEY_UP:
			case KEY_DOWN:
			case KEY_RIGHT:
			case KEY_LEFT:
				scroll_window(option, &details_pad, refresh_details_window);
				break;

			default:
				break;
		}
	}

	return STOP;
}

STATUS curses_group_level_run(PTestGroup pGroup) 
{
	char szTestName[STRING_LENGTH];
	PTestCase pTest = NULL;

	g_szOptions = GROUP_OPTIONS;
	refresh_options_window();

	while (true) {
		int option = toupper(getch());
		
		switch (option) {
			case 'R':
				curses_run_group_tests(pGroup);
				break;

			case 'S':
				read_input_string("Enter Test Name : ", szTestName, STRING_LENGTH);
				if (NULL != (pTest = get_test_by_name(szTestName, pGroup))) {
					curses_run_single_test(pGroup, pTest);
				}
				refresh_details_window();
				break;

			case 'L':
				list_tests(pGroup);
				break;

			case 'F':
				show_failures(get_registry());
				break;

			case 'U':
				return CONTINUE;

			case 'Q':
				return STOP;
		
			case KEY_UP:
			case KEY_DOWN:
			case KEY_RIGHT:
			case KEY_LEFT:
				scroll_window(option, &details_pad, refresh_details_window);
				break;

			default:
				break;
		}
	}

	return CONTINUE;
}

void read_input_string(const char szPropmt[], char szValue[], int nBytes)
{
	echo();
	curs_set(1);
	nocbreak();
	
	wclear(application_windows.pOptionsWin);
	mvwprintw(application_windows.pOptionsWin, 0, 1, "%s", szPropmt);
	wgetnstr(application_windows.pOptionsWin, szValue, nBytes - 1);
	refresh_options_window();

	cbreak();
	curs_set(0);
	noecho();
}

void scroll_window(int nCommand, APPPAD* pPad, void (*parent_refresh)(void))
{
	if (NULL == pPad->pPad)
		return;

	switch (nCommand) {
		case KEY_UP:
			if (pPad->uiPadRow) {
				--pPad->uiPadRow;
				(*parent_refresh)();
			}
			break;

		case KEY_DOWN:
			if (pPad->uiRows - 1 > pPad->uiPadRow + pPad->uiWinRows) {
				++pPad->uiPadRow;
				(*parent_refresh)();
			}
			break;
		
		case KEY_LEFT:
			if (pPad->uiPadCol) {
				--pPad->uiPadCol;
				(*parent_refresh)();
			}
			break;

		case KEY_RIGHT:
			if (details_pad.uiColumns - 1 > details_pad.uiPadCol + details_pad.uiWinColumns) {
				++pPad->uiPadCol;
				(*parent_refresh)();
			}
			break;

		default:
			break;
	}
}

bool create_pad(APPPAD* pPad, WINDOW* pParent, unsigned int uiRows, 
		unsigned int uiCols)
{
	bool bStatus = false;

	assert(pParent);
	if (pPad->pPad)
		delwin(pPad->pPad);

	if (NULL != pPad && NULL == (pPad->pPad = newpad(uiRows, uiCols)))
		goto newpad_fail;

	pPad->uiRows = uiRows;
	pPad->uiColumns = uiCols;
	pPad->uiPadRow = 0;
	pPad->uiPadCol = 0;
	pPad->uiWinLeft = application_windows.pDetailsWin->_begx + 1;
	pPad->uiWinTop = application_windows.pDetailsWin->_begy + 1;
	pPad->uiWinColumns = application_windows.pDetailsWin->_maxx - 2;
	pPad->uiWinRows = application_windows.pDetailsWin->_maxy - 2;

	bStatus = true;
	
newpad_fail:
	return bStatus;	
}

static void list_groups(PTestRegistry pRegistry)
{

	PTestGroup pCurGroup = NULL;
	int i;
	
	if (!create_pad(&details_pad, application_windows.pDetailsWin, 
					pRegistry->uiNumberOfGroups == 0 ? 1 : pRegistry->uiNumberOfGroups + 4, 256)) {
		return;
	}
	
	assert(pRegistry);
	if (0 == pRegistry->uiNumberOfGroups) {
		mvwprintw(details_pad.pPad, 0, 0, "%s", "No test groups defined.");
		refresh_details_window();
		return;
	}
	assert(pRegistry->pGroup);
	
	mvwprintw(details_pad.pPad, 0, 0, "%s", "Format -- (Group Name : Initialize Function : Cleanup Function : Number of Tests)");
	
	for (i = 0, pCurGroup = pRegistry->pGroup; pCurGroup; pCurGroup = pCurGroup->pNext, i++) {
		char szTemp[256];

		snprintf(szTemp, sizeof(szTemp), "(%d) - (%s : %s : %s : %d)", i + 1, pCurGroup->pName, 
				pCurGroup->pInitializeFunc ? "Init defined" : "Init not defined",
				pCurGroup->pCleanupFunc ? "Cleanup defined" : "Cleanup not defined",
				pCurGroup->uiNumberOfTests);
		mvwprintw(details_pad.pPad, i + 2, 0, "%s", szTemp);
	}

	mvwprintw(details_pad.pPad, i + 2, 0, "%s", "=============================================");
	mvwprintw(details_pad.pPad, i + 3, 0, "Total Number of Test Groups : %-d", pRegistry->uiNumberOfGroups);
	refresh_details_window();
}

static void list_tests(PTestGroup pGroup)
{
	PTestCase pCurTest = NULL;
	int i;
	
	assert(pGroup);
	if (!create_pad(&details_pad, application_windows.pDetailsWin, 
					pGroup->uiNumberOfTests == 0 ? 1 : pGroup->uiNumberOfTests + 4, 256)) {
		return;
	}
	
	if (0 == pGroup->uiNumberOfTests) {
		mvwprintw(details_pad.pPad, 0, 0, "%s", "No tests defined under this group.");
		refresh_details_window();
		return;
	}
	assert(pGroup->pTestCase);
	
	mvwprintw(details_pad.pPad, 0, 0, "%s", "Format -- (Test Name)");
	
	for (i = 0, pCurTest = pGroup->pTestCase; pCurTest; pCurTest = pCurTest->pNext, i++) {
		char szTemp[256];

		snprintf(szTemp, sizeof(szTemp), "(%d) - (%s)", i + 1, pCurTest->pName);
		mvwprintw(details_pad.pPad, i + 2, 0, "%s", szTemp);
	}

	mvwprintw(details_pad.pPad, i + 2, 0, "%s", "=============================================");
	mvwprintw(details_pad.pPad, i + 3, 0, "Total Number of Tests : %-d", pGroup->uiNumberOfTests);
	refresh_details_window();
}

static void show_failures(PTestRegistry pRegistry)
{
	PTestResult pResult = NULL;
	int i;
	
	assert(pRegistry);

	if (!create_pad(&details_pad, application_windows.pDetailsWin, 
					pRegistry->uiNumberOfFailures == 0 ? 1 : pRegistry->uiNumberOfFailures + 4, 256)) {
		return;
	}
	
	if (0 == pRegistry->uiNumberOfFailures) {
		mvwprintw(details_pad.pPad, 0, 0, "%s", "No Failures.");
		refresh_details_window();
		return;
	}
	assert(pRegistry->pResult);
	
	for (i = 0, pResult = pRegistry->pResult; pResult; pResult = pResult->pNext, i++) {
		char szTemp[256];
		
		snprintf(szTemp, 256, "%d>  %s:%d : (%s : %s) : %s\n", i + 1,
				pResult->strFileName ? pResult->strFileName : "",
				pResult->uiLineNumber,
				pResult->pTestGroup ? pResult->pTestGroup->pName : "",
				pResult->pTestCase ? pResult->pTestCase->pName : "",
				pResult->strCondition ? pResult->strCondition : "");
		
		mvwprintw(details_pad.pPad, i + 2, 0, "%s", szTemp);
	}
	
	mvwprintw(details_pad.pPad, i + 2, 0, "%s", "=============================================");
	mvwprintw(details_pad.pPad, i + 3, 0, "Total Number of Failures : %-d", pRegistry->uiNumberOfFailures);
	refresh_details_window();
}

static void curses_run_all_tests(PTestRegistry pRegistry)
{
	reset_run_parameters();
	g_uiTotalTests = pRegistry->uiNumberOfTests;
	g_uiTotalGroups = pRegistry->uiNumberOfGroups;
	run_all_tests();
}

static void curses_run_group_tests(PTestGroup pGroup)
{
	reset_run_parameters();
	g_uiTotalTests = pGroup->uiNumberOfTests;
	g_uiTotalGroups = 1;
	run_group_tests(pGroup);
}

static void curses_run_single_test(PTestGroup pGroup, PTestCase pTest)
{
	reset_run_parameters();
	g_uiTotalTests = 1;
	g_uiTotalGroups = 1;
	run_test(pGroup, pTest);
}

static void reset_run_parameters(void)
{
	g_szTest = NULL;
	g_szGroup = NULL;
	g_uiTestsRunSuccessfull = g_uiTestsRun = g_uiTotalTests = g_uiTestsFailed = g_uiTestsSkipped = 0;
	g_uiTotalGroups = g_uiGroupsSkipped = 0;
	refresh_progress_window();
	refresh_summary_window();
	refresh_run_summary_window();
}
	
void curses_test_start_message_handler(const char* szTest, const char* szGroup)
{
	g_szTest = szTest;
	g_szGroup = szGroup;
	refresh_run_summary_window();
}

void curses_test_complete_message_handler(const char* szTest, const char* szGroup, const PTestResult pTestResult)
{
	PTestRegistry pRegistry = get_registry();

	assert(pRegistry);
	g_uiTestsRun++;
	if (pRegistry->uiNumberOfFailures != g_uiTestsFailed) {
		g_uiTestsFailed++;
	}
	else {
		g_uiTestsRunSuccessfull++;
	}
	
	refresh_summary_window();
	refresh_progress_window();
}

void curses_all_tests_complete_message_handler(const PTestResult pTestResult)
{
	PTestRegistry pRegistry = get_registry();

	assert(pRegistry);
	g_szTest = g_szGroup = NULL;

	if (!create_pad(&details_pad, application_windows.pDetailsWin, 5 , 256)) {
		return;
	}
	
	mvwprintw(details_pad.pPad, 0, 0, "%s", "======  Test Group Run Summary  ======");
	mvwprintw(details_pad.pPad, 1, 0, "Total : %u  Skipped : %u  Run : %u  Successfull : %u  Failed : %u",
			g_uiTotalTests, g_uiTestsSkipped, g_uiTestsRun, g_uiTestsRunSuccessfull, g_uiTestsFailed);
	
	mvwprintw(details_pad.pPad, 3, 0, "%s", "======  Test Case Run Summary  ======");
	mvwprintw(details_pad.pPad, 4, 0, "Total : %u  Skipped : %u  Run : %u",
			g_uiTotalGroups, g_uiGroupsSkipped, g_uiTotalGroups - g_uiGroupsSkipped);

	refresh_details_window();
	refresh_run_summary_window();
}

void curses_group_init_failure_message_handler(const PTestGroup pGroup)
{
	assert(pGroup);
	g_uiTestsSkipped += pGroup->uiNumberOfTests;
	g_uiGroupsSkipped++;

	refresh_summary_window();
	refresh_progress_window();
}
