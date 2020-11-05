/*
	lime-tester.hpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2017  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef lime_tester_hpp
#define lime_tester_hpp

#include <bctoolbox/tester.h>

#include <string>
#include <memory>
#include <sstream>
#include <iostream>

extern "C" {
extern bool cleanDatabase;
extern bool bench;
extern test_suite_t lime_double_ratchet_test_suite;
extern test_suite_t lime_lime_test_suite;
extern test_suite_t lime_helloworld_test_suite;
extern test_suite_t lime_crypto_test_suite;
extern test_suite_t lime_massive_group_test_suite;
#ifdef FFI_ENABLED
extern test_suite_t lime_ffi_test_suite;
#endif
extern test_suite_t lime_multidomains_test_suite;
extern test_suite_t lime_server_test_suite;

void lime_tester_init(void(*ftester_printf)(int level, const char *fmt, va_list args));
void lime_tester_uninit(void);

};

#endif
