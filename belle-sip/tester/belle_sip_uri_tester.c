/*
	belle-sip - SIP (RFC3261) library.
 Copyright (C) 2017  Belledonne Communications SARL
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "belle-sip/belle-sip.h"
#include "belle_sip_tester.h"
#include "belle_sip_internal.h"


/*test body*/
#include "belle_sip_base_uri_tester.c"


test_suite_t sip_uri_test_suite = {"SIP URI", NULL, NULL, belle_sip_tester_before_each, belle_sip_tester_after_each,
	sizeof(uri_tests) / sizeof(uri_tests[0]), uri_tests};



