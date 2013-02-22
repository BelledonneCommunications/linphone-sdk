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
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2_tester.h"
#include "private.h"

#include <stdio.h>
#include "CUnit/Basic.h"


typedef struct {
	MSDtmfGenCustomTone generated_tone;
	MSToneDetectorDef expected_tone;
} tone_test_def_t;


static tone_test_def_t tone_definition[] = {
	{ { 400, 2000, 0.6, 0 }, { "", 2000, 300, 0.5 } },
	{ { 600, 1500, 1.0, 0 }, { "", 1500, 500, 0.9 } },
	{ { 500,  941, 0.8, 0 }, { "",  941, 400, 0.7 } }
};

static MSTicker *ticker = NULL;
static MSFilter *fileplay = NULL;
static MSFilter *dtmfgen = NULL;
static MSFilter *tonedet = NULL;
static MSFilter *voidsink = NULL;
static MSFilter *encoder = NULL;
static MSFilter *decoder = NULL;
static MSFilter *rtprecv = NULL;
static MSFilter *rtpsend = NULL;
static unsigned char tone_detected;


static MSTicker * create_ticker(void) {
	MSTickerParams params = {0};
	params.name = "Tester MSTicker";
	params.prio = MS_TICKER_PRIO_NORMAL;
	return ms_ticker_new_with_params(&params);
}

static void tone_detected_cb(void *data, MSFilter *f, unsigned int event_id, MSToneDetectorEvent *ev) {
	tone_detected = TRUE;
}

static void common_init(void) {
	ms_init();

	ms_filter_enable_statistics(TRUE);
	ms_filter_reset_statistics();

	ticker = create_ticker();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ticker);
	fileplay = ms_filter_new(MS_FILE_PLAYER_ID);
	CU_ASSERT_PTR_NOT_NULL_FATAL(fileplay);
	dtmfgen = ms_filter_new(MS_DTMF_GEN_ID);
	CU_ASSERT_PTR_NOT_NULL_FATAL(dtmfgen);
	tonedet = ms_filter_new(MS_TONE_DETECTOR_ID);
	CU_ASSERT_PTR_NOT_NULL_FATAL(tonedet);
	ms_filter_set_notify_callback(tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL);
	voidsink = ms_filter_new(MS_VOID_SINK_ID);
	CU_ASSERT_PTR_NOT_NULL_FATAL(voidsink);
}

static void common_uninit(void) {
	ms_filter_destroy(voidsink);
	ms_filter_destroy(tonedet);
	ms_filter_destroy(dtmfgen);
	ms_filter_destroy(fileplay);
	ms_ticker_destroy(ticker);

	ms_exit();
}

static void tone_generation_loop(void) {
	unsigned int i;

	for (i = 0; i < (sizeof(tone_definition) / sizeof(tone_definition[0])); i++) {
		tone_detected = FALSE;
		ms_filter_call_method(tonedet, MS_TONE_DETECTOR_CLEAR_SCANS, NULL);
		ms_filter_call_method(tonedet, MS_TONE_DETECTOR_ADD_SCAN, &tone_definition[i].expected_tone);
		ms_filter_call_method(dtmfgen, MS_DTMF_GEN_PLAY_CUSTOM, &tone_definition[i].generated_tone);
		ms_sleep(1);
		CU_ASSERT_EQUAL(tone_detected, TRUE);
	}
}

static void dtmfgen_direct(void) {
	MSConnectionHelper h;

	common_init();
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, fileplay, -1, 0);
	ms_connection_helper_link(&h, dtmfgen, 0, 0);
	ms_connection_helper_link(&h, tonedet, 0, 0);
	ms_connection_helper_link(&h, voidsink, 0, -1);
	ms_ticker_attach(ticker, fileplay);

	tone_generation_loop();

	ms_ticker_detach(ticker, fileplay);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, fileplay, -1, 0);
	ms_connection_helper_unlink(&h, dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, tonedet, 0, 0);
	ms_connection_helper_unlink(&h, voidsink, 0, -1);
	common_uninit();
}

static void dtmfgen_codec(void) {
	MSConnectionHelper h;

	common_init();
	encoder = ms_filter_new(MS_ALAW_ENC_ID);
	CU_ASSERT_PTR_NOT_NULL_FATAL(encoder);
	decoder = ms_filter_new(MS_ALAW_DEC_ID);
	CU_ASSERT_PTR_NOT_NULL_FATAL(decoder);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, fileplay, -1, 0);
	ms_connection_helper_link(&h, dtmfgen, 0, 0);
	ms_connection_helper_link(&h, encoder, 0, 0);
	ms_connection_helper_link(&h, decoder, 0, 0);
	ms_connection_helper_link(&h, tonedet, 0, 0);
	ms_connection_helper_link(&h, voidsink, 0, -1);
	ms_ticker_attach(ticker, fileplay);

	tone_generation_loop();

	ms_ticker_detach(ticker, fileplay);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, fileplay, -1, 0);
	ms_connection_helper_unlink(&h, dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, encoder, 0, 0);
	ms_connection_helper_unlink(&h, decoder, 0, 0);
	ms_connection_helper_unlink(&h, tonedet, 0, 0);
	ms_connection_helper_unlink(&h, voidsink, 0, -1);
	ms_filter_destroy(decoder);
	ms_filter_destroy(encoder);
	common_uninit();
}


static void dtmfgen_rtp(void) {
	MSConnectionHelper h;
	RtpSession *rtps;

	common_init();
	ortp_init();
	rtps = create_duplex_rtpsession(50060, 0, FALSE);
	rtp_session_set_remote_addr_full(rtps, "127.0.0.1", 50060, NULL, 0);
	rtp_session_set_payload_type(rtps, 8);
	rtp_session_enable_rtcp(rtps,FALSE);
	encoder = ms_filter_new(MS_ALAW_ENC_ID);
	CU_ASSERT_PTR_NOT_NULL_FATAL(encoder);
	decoder = ms_filter_new(MS_ALAW_DEC_ID);
	CU_ASSERT_PTR_NOT_NULL_FATAL(decoder);
	rtprecv = ms_filter_new(MS_RTP_RECV_ID);
	CU_ASSERT_PTR_NOT_NULL_FATAL(rtprecv);
	ms_filter_call_method(rtprecv, MS_RTP_RECV_SET_SESSION, rtps);
	rtpsend = ms_filter_new(MS_RTP_SEND_ID);
	CU_ASSERT_PTR_NOT_NULL_FATAL(rtpsend);
	ms_filter_call_method(rtpsend, MS_RTP_SEND_SET_SESSION, rtps);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, fileplay, -1, 0);
	ms_connection_helper_link(&h, dtmfgen, 0, 0);
	ms_connection_helper_link(&h, encoder, 0, 0);
	ms_connection_helper_link(&h, rtpsend, 0, -1);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, rtprecv, -1, 0);
	ms_connection_helper_link(&h, decoder, 0, 0);
	ms_connection_helper_link(&h, tonedet, 0, 0);
	ms_connection_helper_link(&h, voidsink, 0, -1);
	ms_ticker_attach_multiple(ticker, fileplay, rtprecv, NULL);

	tone_generation_loop();

	ms_ticker_detach(ticker, fileplay);
	ms_ticker_detach(ticker, rtprecv);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, fileplay, -1, 0);
	ms_connection_helper_unlink(&h, dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, encoder, 0, 0);
	ms_connection_helper_unlink(&h, rtpsend, 0, -1);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, rtprecv, -1, 0);
	ms_connection_helper_unlink(&h, decoder, 0, 0);
	ms_connection_helper_unlink(&h, tonedet, 0, 0);
	ms_connection_helper_unlink(&h, voidsink, 0, -1);
	ms_filter_destroy(rtpsend);
	ms_filter_destroy(rtprecv);
	ms_filter_destroy(decoder);
	ms_filter_destroy(encoder);
	common_uninit();
}


test_t dtmfgen_tests[] = {
	{ "dtmfgen-direct", dtmfgen_direct },
	{ "dtmfgen-codec", dtmfgen_codec },
	{ "dtmfgen-rtp", dtmfgen_rtp }
};

test_suite_t dtmfgen_test_suite = {
	"dtmfgen",
	sizeof(dtmfgen_tests) / sizeof(dtmfgen_tests[0]),
	dtmfgen_tests
};
