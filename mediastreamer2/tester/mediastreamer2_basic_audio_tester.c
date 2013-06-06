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


#ifdef _MSC_VER
#define unlink _unlink
#endif


static int basic_audio_tester_init(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();
	return 0;
}

static int basic_audio_tester_cleanup(void) {
	ms_exit();
	return 0;
}

static void tone_detected_cb(void *data, MSFilter *f, unsigned int event_id, MSToneDetectorEvent *ev) {
	MS_UNUSED(data), MS_UNUSED(f), MS_UNUSED(event_id), MS_UNUSED(ev);
	ms_tester_tone_detected = TRUE;
}

static void dtmfgen_tonedet(void) {
	MSConnectionHelper h;
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask);
	ms_filter_set_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_link(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_voidsource);

	ms_tester_tone_generation_and_detection_loop();

	ms_ticker_detach(ms_tester_ticker, ms_tester_voidsource);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

static void dtmfgen_enc_dec_tonedet(char *mime, int sample_rate, int nchannels) {
	MSConnectionHelper h;

	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_ENCODER
		| FILTER_MASK_DECODER | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_codec_mime = mime;
	ms_tester_create_filters(filter_mask);

	/* set sample rate and channel number to all filters (might need to check the return value to insert a resampler if needed?) */	
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_dtmfgen, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_dtmfgen, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_encoder, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_encoder, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_decoder, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_decoder, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_tonedet, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_tonedet, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_voidsink, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_voidsink, MS_FILTER_SET_NCHANNELS, &nchannels);


	ms_filter_set_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_link(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_decoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_voidsource);

	ms_tester_tone_generation_and_detection_loop();

	ms_ticker_detach(ms_tester_ticker, ms_tester_voidsource);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_decoder, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

static void dtmfgen_enc_dec_tonedet_pcmu(void) {
	dtmfgen_enc_dec_tonedet("pcmu", 8000, 1);
}

static void dtmfgen_enc_dec_tonedet_opus(void) {
	dtmfgen_enc_dec_tonedet("opus", 48000, 1);
}

static void dtmfgen_enc_rtp_dec_tonedet(void) {
	MSConnectionHelper h;
	RtpSession *rtps;
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_ENCODER
		| FILTER_MASK_RTPSEND | FILTER_MASK_RTPRECV | FILTER_MASK_DECODER | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_codec_mime = "pcmu";
	ms_tester_create_filters(filter_mask);
	ms_filter_set_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL);
	rtps = create_duplex_rtpsession(50060, 0, FALSE);
	rtp_session_set_remote_addr_full(rtps, "127.0.0.1", 50060, NULL, 0);
	rtp_session_set_payload_type(rtps, 8);
	rtp_session_enable_rtcp(rtps,FALSE);
	ms_filter_call_method(ms_tester_rtprecv, MS_RTP_RECV_SET_SESSION, rtps);
	ms_filter_call_method(ms_tester_rtpsend, MS_RTP_SEND_SET_SESSION, rtps);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_link(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_rtpsend, 0, -1);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_rtprecv, -1, 0);
	ms_connection_helper_link(&h, ms_tester_decoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	ms_ticker_attach_multiple(ms_tester_ticker, ms_tester_voidsource, ms_tester_rtprecv, NULL);

	ms_tester_tone_generation_and_detection_loop();

	ms_ticker_detach(ms_tester_ticker, ms_tester_voidsource);
	ms_ticker_detach(ms_tester_ticker, ms_tester_rtprecv);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_rtpsend, 0, -1);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_rtprecv, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_decoder, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
	rtp_session_destroy(rtps);
}

#define DTMFGEN_FILE_NAME "dtmfgen_file.raw"

static void dtmfgen_filerec_fileplay_tonedet(void) {
	MSConnectionHelper h;
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_FILEREC
		| FILTER_MASK_FILEPLAY | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask);
	ms_filter_set_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL);

	// Generate tones and save them to a file
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_CLOSE);
	ms_filter_call_method(ms_tester_filerec, MS_FILE_REC_OPEN, DTMFGEN_FILE_NAME);
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_START);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_link(&h, ms_tester_filerec, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_voidsource);
	ms_tester_tone_generation_loop();
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_CLOSE);
	ms_ticker_detach(ms_tester_ticker, ms_tester_voidsource);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_filerec, 0, -1);

	// Read the previous file and detect the tones
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method(ms_tester_fileplay, MS_FILE_PLAYER_OPEN, DTMFGEN_FILE_NAME);
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_START);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_fileplay, -1, 0);
	ms_connection_helper_link(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_fileplay);
	ms_tester_tone_detection_loop();
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);
	ms_ticker_detach(ms_tester_ticker, ms_tester_fileplay);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_fileplay, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
	unlink(DTMFGEN_FILE_NAME);
}


test_t basic_audio_tests[] = {
	{ "dtmfgen-tonedet", dtmfgen_tonedet },
	{ "dtmfgen-enc-dec-tonedet-pcmu", dtmfgen_enc_dec_tonedet_pcmu },
	{ "dtmfgen-enc-dec-tonedet-opus", dtmfgen_enc_dec_tonedet_opus },
	{ "dtmfgen-enc-rtp-dec-tonedet", dtmfgen_enc_rtp_dec_tonedet },
	{ "dtmfgen-filerec-fileplay-tonedet", dtmfgen_filerec_fileplay_tonedet }
};

test_suite_t basic_audio_test_suite = {
	"Basic Audio",
	basic_audio_tester_init,
	basic_audio_tester_cleanup,
	sizeof(basic_audio_tests) / sizeof(basic_audio_tests[0]),
	basic_audio_tests
};
