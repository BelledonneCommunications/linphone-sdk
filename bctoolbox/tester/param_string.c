/*
 * Copyright (c) 2016-2020 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "bctoolbox/param_string.h"
#include "bctoolbox_tester.h"

static void get_value_test(void) {
	size_t result_len = 10;
	char *result = bctbx_malloc(result_len);

	char *paramString = "";
	BC_ASSERT_FALSE(bctbx_param_string_get_value(paramString, "param", result, result_len));

	paramString = "param=true";
	BC_ASSERT_TRUE(bctbx_param_string_get_value(paramString, "param", result, result_len));
	BC_ASSERT_TRUE(strcmp(result, "true") == 0);
	BC_ASSERT_FALSE(bctbx_param_string_get_value(paramString, "notparam", result, result_len));

	paramString = "test;param=true;test";
	BC_ASSERT_TRUE(bctbx_param_string_get_value(paramString, "param", result, result_len));
	BC_ASSERT_TRUE(strcmp(result, "true") == 0);
	BC_ASSERT_FALSE(bctbx_param_string_get_value(paramString, "notparam", result, result_len));

	bctbx_free(result);
}

static void get_bool_value_test(void) {
	char *paramString = "";
	BC_ASSERT_FALSE(bctbx_param_string_get_bool_value(paramString, "param"));
	paramString = "param=false";
	BC_ASSERT_FALSE(bctbx_param_string_get_bool_value(paramString, "param"));
	paramString = "param=42";
	BC_ASSERT_FALSE(bctbx_param_string_get_bool_value(paramString, "param"));
	paramString = "param=true";
	BC_ASSERT_TRUE(bctbx_param_string_get_bool_value(paramString, "param"));
}

static test_t param_string_tests[] = {TEST_NO_TAG("Get value", get_value_test),
                                      TEST_NO_TAG("Get bool value", get_bool_value_test)};

test_suite_t param_string_test_suite = {
    "Param string",     NULL, NULL, NULL, NULL, sizeof(param_string_tests) / sizeof(param_string_tests[0]),
    param_string_tests, 0};