/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "private.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

#include <stdio.h>
#include "CUnit/Basic.h"


static int tester_init(void) {
/*	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();*/
	return 0;
}

static int tester_cleanup(void) {
/*	ms_exit();*/
	return 0;
}

static void filter_register_tester(void) {
	MSFilter* filter;

	ms_init();
	ms_init();

	CU_ASSERT_PTR_NOT_NULL(ms_filter_lookup_by_name("MSVoidSource"));
	filter= ms_filter_create_decoder("pcma");
	CU_ASSERT_PTR_NOT_NULL(filter);
	ms_filter_destroy(filter);

	ms_exit();

	CU_ASSERT_PTR_NOT_NULL(ms_filter_lookup_by_name("MSVoidSource"));
	filter= ms_filter_create_decoder("pcma");
	CU_ASSERT_PTR_NOT_NULL(filter);
	ms_filter_destroy(filter);

	ms_exit();
	CU_ASSERT_PTR_NULL(ms_factory_get_fallback());
}

static test_t tests[] = {
	{ "Multiple ms_voip_init", filter_register_tester }
};

test_suite_t framework_test_suite = {
	"Framework",
	tester_init,
	tester_cleanup,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
