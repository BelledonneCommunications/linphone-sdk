/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2004  Aurema Pty Ltd.
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

/* Contributed by K. Cheung and Aurema Pty Ltd. (thanks!)
 *
 * The changes made herein (JDS) to the contributed code were:
 *    - added this comment block
 *    - changed name of file from example.c to TestRegister.c
 *    - eliminated unneeded #include's
 *
 * NOTE - this code is likely to change significantly in
 *        the upcoming version 2 CUnit release to better
 *        integrate it with the new CUnit API.
 */

#include "CUnit.h"
#include "TestDB.h"
#include "Console.h"

#include <stdio.h>
#include <stdlib.h>

static void group_A_case_1(void)
{
	ASSERT_TRUE(1);
}
 
static void group_A_case_2(void)
{
	ASSERT_TRUE(2);
}

static void group_B_case_1(void)
{
	ASSERT_FALSE(1);
}

static void group_B_case_2(void)
{
	ASSERT_FALSE(2);
}

static test_case_t group_A_test_cases[] = {
	{ "1", group_A_case_1 },
	{ "2", group_A_case_2 },
	TEST_CASE_NULL,
};

static test_case_t group_B_test_cases[] = {
	{ "1", group_B_case_1 },
	{ "2", group_B_case_2 },
	TEST_CASE_NULL,
};

static test_group_t groups[] = {
	{ "A", NULL, NULL, group_A_test_cases },
	{ "B", NULL, NULL, group_B_test_cases },
	TEST_GROUP_NULL,
};

static test_suite_t suite = { "suite", groups };


int main(void)
{
	/* No line buffering. */
	setbuf(stdout, NULL);

	/* Intialise test registry. */
	if (initialize_registry() != CUE_SUCCESS) {
		fprintf(stderr, "initialize_registry failed - %s\n",
			get_error());
		exit(EXIT_FAILURE);
	}

	/* Register test groups. */
	if (test_suite_register(&suite) != CUE_SUCCESS) {
		fprintf(stderr, "test_suite_register failed - %s\n",
			get_error());
		exit(EXIT_FAILURE);
	}

	/* Execute unit tests. */
	console_run_tests();

	/* Cleanup test registry. */
	cleanup_registry();

	return 0;
}
