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

#include <dlfcn.h>
#include "bctoolbox_tester.h"
#include "bctoolbox/ios_utils.hh"

using namespace bctoolbox;

static void ios_utils_return_values(void) {
    auto &iOSUtils = IOSUtils::getUtils();
    BC_ASSERT_EQUAL(iOSUtils.beginBackgroundTask(nullptr, nullptr), 0, unsigned long, "%lu");
    BC_ASSERT_EQUAL(iOSUtils.isApplicationStateActive(), false, bool, "%d");
}

static test_t ios_utils_tests[] = {
    TEST_NO_TAG("Return values for stubbed functions", ios_utils_return_values),
};

test_suite_t ios_utils_test_suite = {"iOS Utilities", NULL, NULL, NULL, NULL,
sizeof(ios_utils_tests) / sizeof(ios_utils_tests[0]), ios_utils_tests};
