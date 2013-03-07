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


static int sound_card_tester_init(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	return 0;
}

static int sound_card_tester_cleanup(void) {
	ms_exit();
	return 0;
}

static void configure_resampler(MSFilter *resampler, MSFilter *from, MSFilter *to) {
	int from_rate = 0, to_rate = 0;
	int from_channels = 0, to_channels = 0;
	ms_filter_call_method(from, MS_FILTER_GET_SAMPLE_RATE, &from_rate);
	ms_filter_call_method(to, MS_FILTER_GET_SAMPLE_RATE, &to_rate);
	ms_filter_call_method(resampler, MS_FILTER_SET_SAMPLE_RATE, &from_rate);
	ms_filter_call_method(resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &to_rate);
	ms_filter_call_method(from, MS_FILTER_GET_NCHANNELS, &from_channels);
	ms_filter_call_method(to, MS_FILTER_GET_NCHANNELS, &to_channels);
	ms_filter_call_method(resampler, MS_FILTER_SET_NCHANNELS, &from_channels);
	ms_filter_call_method(resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &to_channels);
}

static void dtmfgen_soundwrite(void) {
	MSConnectionHelper h;
	bool_t need_resampler = FALSE;
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_SOUNDWRITE;
	int sample_rate = 8000;
	int nchannels = 1;

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask);
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_BITRATE, &sample_rate);
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_dtmfgen, MS_FILTER_SET_BITRATE, &sample_rate);
	ms_filter_call_method(ms_tester_dtmfgen, MS_FILTER_SET_NCHANNELS, &nchannels);
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_BITRATE, &sample_rate) != 0) {
		need_resampler = TRUE;
	}
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels) != 0) {
		need_resampler = TRUE;
	}
	if (need_resampler == TRUE) {
		ms_tester_create_filters(FILTER_MASK_RESAMPLER);
		configure_resampler(ms_tester_resampler, ms_tester_dtmfgen, ms_tester_soundwrite);
	}
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	if (need_resampler == TRUE) {
		ms_connection_helper_link(&h, ms_tester_resampler, 0, 0);
	}
	ms_connection_helper_link(&h, ms_tester_soundwrite, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_voidsource);

	ms_tester_tone_generation_loop();

	ms_ticker_detach(ms_tester_ticker, ms_tester_voidsource);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_dtmfgen, 0, 0);
	if (need_resampler == TRUE) {
		ms_connection_helper_unlink(&h, ms_tester_resampler, 0, 0);
	}
	ms_connection_helper_unlink(&h, ms_tester_soundwrite, 0, -1);
	if (need_resampler == TRUE) {
		ms_tester_destroy_filters(FILTER_MASK_RESAMPLER);
	}
	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}


test_t sound_card_tests[] = {
	{ "dtmfgen-soundwrite", dtmfgen_soundwrite }
};

test_suite_t sound_card_test_suite = {
	"Sound Card",
	sound_card_tester_init,
	sound_card_tester_cleanup,
	sizeof(sound_card_tests) / sizeof(sound_card_tests[0]),
	sound_card_tests
};
