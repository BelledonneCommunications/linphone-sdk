/*
 *	Contains Interface to Run tests.
 *
 *	Created By     : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified  : 09/Aug/2001
 *	Comment        : Contains generic run tests interface which can be used
 *						be used for any type of frontend interface framework.
 *	EMail          : aksaharan@yahoo.com
 */

#ifndef _TestRun_h_
#define _TestRun_h_

#include "CUnit.h"
#include "TestDB.h"

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
							PTestResult pTestResult);
typedef void (*AllTestsCompleteMessageHandler)(PTestResult pTestResult);

/*
 * Get/Set functions for the Message Handlers.
 */
extern void set_test_start_handler(TestStartMessageHandler pTestStartMessage);
extern void set_test_complete_handler(TestCompleteMessageHandler pTestCompleteMessage);
extern void set_all_test_complete_handler(AllTestsCompleteMessageHandler pAllTestsCompleteMessage);
extern TestStartMessageHandler get_test_start_handler(void);
extern TestCompleteMessageHandler get_test_complete_handler(void);
extern AllTestsCompleteMessageHandler get_all_test_complete_handler(void);

/*
 * Function to Run all the Registered tests under various groups
 */
extern int run_all_tests(void);
extern int run_group_tests(PTestGroup pGroup);
extern int run_test(PTestGroup pGroup, PTestCase pTest);

extern int get_number_of_groups_run(void);
extern int get_number_of_tests_run(void);


#endif  /*  _TestRun_h_  */
