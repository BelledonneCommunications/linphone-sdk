/*
	@fiel bzrtpTest.h
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

#ifndef bzrtpTester_h
#define bzrtpTester_h

#define BCTOOLBOX_LOG_DOMAIN "bzrtp-tester"
#include <bctoolbox/logging.h>

#define bzrtp_message bctbx_message

#include <bctoolbox/tester.h>

extern test_suite_t crypto_utils_test_suite;
extern test_suite_t packet_parser_test_suite;
extern test_suite_t zidcache_test_suite;
extern test_suite_t key_exchange_test_suite;

extern int verbose;
#endif // bzrtpTester_h
