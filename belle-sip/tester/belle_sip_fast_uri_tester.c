
/*
	belle-sip - SIP (RFC3261) library.
 Copyright (C) 2010  Belledonne Communications SARL
 
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

#include <inttypes.h>

#define belle_sip_uri_parse belle_sip_fast_uri_parse

/*test body*/
#include "belle_sip_base_uri_tester.c"

#undef belle_sip_uri_parse

static void perf(void) {
	uint64_t t1, t2, start=bctbx_get_cur_time_ms();
	int i=0;
	for (i=0;i<1000;i++) {
		belle_sip_uri_t * uri = belle_sip_uri_parse("sip:+331231231231@sip.exmaple.org;user=phone");
		belle_sip_object_unref(uri);
	}
	
	t1 = bctbx_get_cur_time_ms() - start;
	start=bctbx_get_cur_time_ms();
	belle_sip_message("t1 = %" PRIu64 "",t1);
	
	for (i=0;i<1000;i++) {
		belle_sip_uri_t * uri = belle_sip_fast_uri_parse("sip:+331231231231@sip.exmaple.org;user=phone");
		belle_sip_object_unref(uri);
	}
	t2 = bctbx_get_cur_time_ms() - start;
	belle_sip_message("t2 = %" PRIu64 "",t2);
#ifdef __APPLE__ /*antlr3.4 seems much more sensitive to belle_sip_fast_uri_parse optimisation than 3.2, so reserving this test to apple platform where antlr3.2 is unlikely to be found*/
	BC_ASSERT_GREATER(((float)(t1-t2))/(float)(t1), 0.4, float, "%f");
#endif
}
static test_t tests[] ={TEST_NO_TAG("perf", perf)};


test_suite_t fast_sip_uri_test_suite = {"FAST SIP URI", NULL, NULL, belle_sip_tester_before_each, belle_sip_tester_after_each,
	sizeof(uri_tests) / sizeof(uri_tests[0]), uri_tests};

test_suite_t perf_sip_uri_test_suite = {"FAST SIP URI 2", NULL, NULL, belle_sip_tester_before_each, belle_sip_tester_after_each,
	sizeof(tests) / sizeof(tests[0]), tests};
