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
 *	Contains Interface to Run tests.
 *
 *	Created By     : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified  : 09/Aug/2001
 *	Comment        : Contains generic run tests interface which can be used
 *						be used for any type of frontend interface framework.
 *	EMail          : aksaharan@yahoo.com
 *	
 *	Last Modified  : 24/Nov/2001
 *	Comment        : Added Handler for Group Initialization failure condition.
 *	EMail          : aksaharan@yahoo.com
 */

#ifndef _CUNIT_TESTRUN_H
#define _CUNIT_TESTRUN_H 1

#include "CUnit.h"
#include "Errno.h"
#include "TestDB.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *	Declarations for the Current running group/test case.
 */
extern PTestGroup	g_pTestGroup;
extern PTestCase	g_pTestCase;


/*
 *	Type Defintions for Message Handlers.
 */
typedef void (*TestStartMessageHandler)(const char* pTest, const char* pGroup);
typedef void (*TestCompleteMessageHandler)(const char* pTest, const char* pGroup,
							const PTestResult pTestResult);
typedef void (*AllTestsCompleteMessageHandler)(const PTestResult pTestResult);
typedef void (*GroupInitFailureMessageHandler)(const PTestGroup pGroup);

/*
 * Get/Set functions for the Message Handlers.
 */
extern void set_test_start_handler(TestStartMessageHandler pTestStartMessage);
extern void set_test_complete_handler(TestCompleteMessageHandler pTestCompleteMessage);
extern void set_all_test_complete_handler(AllTestsCompleteMessageHandler pAllTestsCompleteMessage);
extern void set_group_init_failure_handler(GroupInitFailureMessageHandler pGroupInitFailureMessage);

extern TestStartMessageHandler get_test_start_handler(void);
extern TestCompleteMessageHandler get_test_complete_handler(void);
extern AllTestsCompleteMessageHandler get_all_test_complete_handler(void);
extern GroupInitFailureMessageHandler get_group_init_failure_handler(void);

/*
 * Function to Run all the Registered tests under various groups
 */
extern int run_all_tests(void);
extern int run_group_tests(PTestGroup pGroup);
extern int run_test(PTestGroup pGroup, PTestCase pTest);

extern int get_number_of_groups_run(void);
extern int get_number_of_tests_run(void);

#ifdef __cplusplus
}
#endif
#endif  /*  _CUNIT_TESTRUN_H  */
