#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CUnit.h"

int success_init(void) { return 0; }
int success_clean(void) { return 0; }

void testSuccess1(void) { ASSERT(1); }
void testSuccess2(void) { ASSERT(1); }
void testSuccess3(void) { ASSERT(1); }

int failure_init(void) { return 1;}
int failure_clean(void) { return 1; }

void testfailure1(void) { ASSERT(0); }
void testfailure2(void) { ASSERT(2); }
void testfailure3(void) { ASSERT(3); }


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

	console_run_tests();
	cleanup_registry();
}

int main(int argc, char* argv[])
{
	if (argc > 1 && !strcmp("--test", argv[1]))
		RunTests();

	return 0;
}
