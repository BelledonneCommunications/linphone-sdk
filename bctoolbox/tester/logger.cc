/*
 * Copyright (c) 2016-2023 Belledonne Communications SARL.
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

#include <list>
#include <sstream>
#include <string>

#include "bctoolbox/crypto.h"
#include "bctoolbox/tester.h"
#include "bctoolbox_tester.h"

static void assert_tag_presence(const std::list<std::string> &expected) {
	const bctbx_list_t *tags = bctbx_get_log_tags();
	std::ostringstream expected_tags_str;
	std::ostringstream current_tags_str;
	for (auto expected_tag : expected) {
		expected_tags_str << "[" << expected_tag << "]";
	}
	for (; tags != NULL; tags = tags->next) {
		const char *expected_tag = (const char *)tags->data;
		current_tags_str << "[" << expected_tag << "]";
	}

	const std::string &currentTags = current_tags_str.str();
	const std::string &expectedTags = expected_tags_str.str();
	BC_ASSERT_STRING_EQUAL(currentTags.c_str(), expectedTags.c_str());
}

static void test_tags(void) {
	const char *sometag = "doing-something";
	const char *someothertag = "doing-something-else";
	bctbx_init_logger(1);

	bctbx_error("Some error");
	bctbx_push_log_tag(sometag, "test1");
	bctbx_error("Error again");
	assert_tag_presence({"test1"});
	bctbx_pop_log_tag(sometag);
	bctbx_error("Error again, no tags expected here");
	assert_tag_presence({});

	bctbx_push_log_tag(sometag, "test1");
	bctbx_error("Error again with tag expected");
	assert_tag_presence({"test1"});
	bctbx_push_log_tag(sometag, "test2");
	assert_tag_presence({"test2"});
	bctbx_error("Error again with new tag value expected");

	bctbx_push_log_tag(someothertag, "yes");
	assert_tag_presence({"test2", "yes"});
	bctbx_error("Error again with 2 tags simulatenously.");
	bctbx_pop_log_tag(sometag);
	assert_tag_presence({"test1", "yes"});
	bctbx_error("Error again with first tag restored to previous value");
	bctbx_pop_log_tag(sometag);
	assert_tag_presence({"yes"});
	bctbx_error("Error with the 2nd tag only");
	bctbx_pop_log_tag(someothertag);

	bctbx_uninit_logger();
}

class LogContextualizer {
public:
	LogContextualizer(const char *tagValue) {
		bctbx_push_log_tag(sTagIdentifier, tagValue);
	}
	LogContextualizer(const std::string &tagValue) {
		bctbx_push_log_tag(sTagIdentifier, tagValue.c_str());
	}
	~LogContextualizer() {
		bctbx_pop_log_tag(sTagIdentifier);
	}

private:
	static constexpr char sTagIdentifier[] = "my-identifier";
};

static void test_cpp_tags(void) {
	bctbx_init_logger(1);

	bctbx_error("Some error");
	{
		LogContextualizer lc("hey");
		bctbx_error("Error again");
		assert_tag_presence({"hey"});
	}
	bctbx_error("Error again, no tags expected here");
	assert_tag_presence({});

	{
		LogContextualizer lc("hello");
		bctbx_error("Error again with tag expected");
		assert_tag_presence({"hello"});
		{
			LogContextualizer lc("hi");
			bctbx_error("Error again with new tag expected");
			assert_tag_presence({"hi"});
		}
		bctbx_error("Error again with previous expected");
		assert_tag_presence({"hello"});
	}
	bctbx_uninit_logger();
}

static test_t logger_tests[] = {TEST_NO_TAG("Log tags", test_tags), TEST_NO_TAG("C++ log tags", test_cpp_tags)};

test_suite_t logger_test_suite = {"Logging",    NULL, NULL, NULL, NULL, sizeof(logger_tests) / sizeof(logger_tests[0]),
                                  logger_tests, 0};
