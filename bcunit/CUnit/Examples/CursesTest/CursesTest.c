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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Curses.h"

#define MAX_BULK_TESTS 200

int success_init(void) { return 0; }
int success_clean(void) { return 0; }

void testSuccess1(void) { ASSERT(1); }
void testSuccess2(void) { ASSERT(1); }
void testSuccess3(void) { ASSERT(1); }

int failure_init(void) { return 0;}
int failure_clean(void) { return 0; }

void testfailure1(void) { ASSERT(0); }
void testfailure2(void) { ASSERT(2); }
void testfailure3(void) { ASSERT(3); }

int group_failure_init(void) { return 1;}
int group_failure_clean(void) { return 1; }

void testGroupFailure1(void) { ASSERT(0); }
void testGroupFailure2(void) { ASSERT(2); }

int init(void)
{
	return 0;
}

int clean(void)
{
	return 0;
}

void test1(void)
{
	ASSERT((char *)2 != "THis is positive test.");
	ASSERT((char *)2 == "THis is negative test. test 1");
}

void test2(void)
{
	ASSERT((char *)2 != "THis is positive test.");
	ASSERT((char *)3 == "THis is negative test. test 2");
}

void RunTests(void)
{
	PTestGroup pGroup = NULL;
	PTestCase pTest = NULL;

	if (initialize_registry()) {
		printf("\nInitialize of test Registry failed.");
	}

	pGroup = add_test_group("Test Group for test", init, clean);
	pTest = add_test_case(pGroup, "Test Case for test 1", test1);
	pTest = add_test_case(pGroup, "Test Case for test 2", test2);

	pGroup = add_test_group("Sucess", success_init, success_clean);
	pTest = add_test_case(pGroup, "testSuccess1", testSuccess1);
	pTest = add_test_case(pGroup, "testSuccess2", testSuccess2);
	pTest = add_test_case(pGroup, "testSuccess3", testSuccess3);
	
	pGroup = add_test_group("failure", failure_init, failure_clean);
	pTest = add_test_case(pGroup, "testfailure1", testfailure1);
	pTest = add_test_case(pGroup, "testfailure2", testfailure2);
	pTest = add_test_case(pGroup, "testfailure3", testfailure3);

	pGroup = add_test_group("group_failure", group_failure_init, group_failure_clean);
	pTest = add_test_case(pGroup, "testGroupFailure1", testGroupFailure1);
	pTest = add_test_case(pGroup, "testGroupFailure2", testGroupFailure2);
	
	curses_run_tests();
	cleanup_registry();
}

void RunBulkTests(void)
{
	PTestGroup pGroup = NULL;
	PTestCase pTest = NULL;
	int index = 0;

	if (initialize_registry()) {
		printf("\nInitialize of test Registry failed.");
	}

	pGroup = add_test_group("Sucess", success_init, success_clean);
	for(index = 0; index < MAX_BULK_TESTS; index++) {
		char szTemp[128];

		snprintf(szTemp, sizeof(szTemp), "testSuccess%d", index + 1);
		if (!(pTest = add_test_case(pGroup, szTemp, testSuccess1))) {
			fprintf(stdout, "\nAdd test failed with error : %s", get_error());
		}
	}
	
	pGroup = add_test_group("failure", failure_init, failure_clean);
	for(index = 0; index < MAX_BULK_TESTS; index++) {
		char szTemp[128];

		snprintf(szTemp, sizeof(szTemp), "testFailure%d", index + 1);
		pTest = add_test_case(pGroup, szTemp, testfailure1);
	}

	curses_run_tests();
	cleanup_registry();
}

int main(int argc, char* argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);
	if (argc > 1 && !strcmp("--test", argv[1]))
		RunTests();
	else if (argc > 1 && !strcmp("--bulk", argv[1]))
		RunBulkTests();

	return 0;
}
