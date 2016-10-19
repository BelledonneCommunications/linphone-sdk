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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "private.h"

static MSFactory *factory = NULL;
static int basic_audio_tester_before_all(void) {

	factory = ms_factory_new_with_voip();

	ms_factory_enable_statistics(factory, TRUE);
	ortp_init();
	return 0;
}

static int basic_audio_tester_after_all(void) {
	ms_factory_destroy(factory);
	ortp_exit();
	return 0;
}

static void tone_detected_cb(void *data, MSFilter *f, unsigned int event_id, MSToneDetectorEvent *ev) {
	MS_UNUSED(data), MS_UNUSED(f), MS_UNUSED(event_id), MS_UNUSED(ev);
	ms_tester_tone_detected = TRUE;
}

static void dtmfgen_tonedet(void) {
	MSConnectionHelper h;
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;
	bool_t send_silence = TRUE;


//	ms_filter_reset_statistics();
	ms_factory_reset_statistics(factory);

	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, factory);
	ms_filter_add_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL,TRUE);
	ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_link(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_voidsource);

	ms_tester_tone_generation_and_detection_loop();

	/*try unrecognized DTMF*/
	BC_ASSERT_NOT_EQUAL(ms_filter_call_method(ms_tester_dtmfgen, MS_DTMF_GEN_PLAY, "F"),0,int,"%d");

	ms_ticker_detach(ms_tester_ticker, ms_tester_voidsource);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	ms_factory_log_statistics(factory);
	//ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

/*fileplay awt to TRUE:  uses soundwrite instaed of voidsink  so we can hear what;s going on */
static void dtmfgen_enc_dec_tonedet(char *mime, int sample_rate, int nchannels, bool_t fileplay) {
	MSConnectionHelper h;
	unsigned int filter_mask ;
	bool_t send_silence = TRUE;
	MSSndCardManager *scm = ms_factory_get_snd_card_manager(factory);
	ms_factory_reset_statistics(scm->factory);

	if (!fileplay){
		filter_mask= FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_ENCODER
		| FILTER_MASK_DECODER | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;

	}else{
		filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_ENCODER
		| FILTER_MASK_DECODER | FILTER_MASK_TONEDET | FILTER_MASK_SOUNDWRITE;

	}

	//ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_codec_mime = mime;
	ms_tester_create_filters(filter_mask, scm->factory);

	/* set sample rate and channel number to all filters (might need to check the return value to insert a resampler if needed?) */
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);
	ms_filter_call_method(ms_tester_dtmfgen, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_dtmfgen, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_encoder, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_encoder, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_decoder, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_decoder, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_tonedet, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_tonedet, MS_FILTER_SET_NCHANNELS, &nchannels);

	if (!fileplay){
		ms_filter_call_method(ms_tester_voidsink, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
		ms_filter_call_method(ms_tester_voidsink, MS_FILTER_SET_NCHANNELS, &nchannels);
	}else{

	ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels);
	}

	ms_filter_add_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL,TRUE);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_link(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_decoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_tonedet, 0, 0);
	if (!fileplay) {
		ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	}else{
		ms_connection_helper_link(&h, ms_tester_soundwrite, 0, -1);


	}
	ms_ticker_attach(ms_tester_ticker, ms_tester_voidsource);

	ms_tester_tone_generation_and_detection_loop();

	ms_ticker_detach(ms_tester_ticker, ms_tester_voidsource);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_decoder, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_tonedet, 0, 0);
	if (!fileplay){
		ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	}else{
		ms_connection_helper_unlink(&h, ms_tester_soundwrite, 0, -1);
	}
	//	ms_filter_log_statistics();
	ms_factory_log_statistics(scm->factory);
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}


#if 0
#define SOUNDREAD_FILE_NAME "soundread_filetest.raw"

static void dtmfgen_enc_dec_tonedet_filerec_fileplay(char *mime, int sample_rate, int nchannels) {
	MSConnectionHelper h;

	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_ENCODER
	| FILTER_MASK_DECODER | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK |  FILTER_MASK_SOUNDREAD | FILTER_MASK_FILEREC | FILTER_MASK_FILEPLAY | FILTER_MASK_SOUNDWRITE;
	bool_t send_silence = TRUE;

	int capture_sample_rate = 8000;
	int playback_sample_rate = 8000;
	int capture_nchannels = 1;
	int playback_nchannels = 1;
	char *writable_filename = bc_tester_file(SOUNDREAD_FILE_NAME);

	MSSndCardManager *scm = ms_factory_get_snd_card_manager(factory);
	ms_factory_reset_statistics(scm->factory);

	//ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_codec_mime = mime;
	ms_tester_create_filters(filter_mask, scm->factory);

	// Write audio capture to a file
	ms_filter_call_method(ms_tester_soundread, MS_FILTER_GET_SAMPLE_RATE, &capture_sample_rate);
	ms_filter_call_method(ms_tester_soundread, MS_FILTER_GET_NCHANNELS, &capture_nchannels);
	ms_filter_call_method(ms_tester_filerec, MS_FILTER_SET_SAMPLE_RATE, &capture_sample_rate);
	ms_filter_call_method(ms_tester_filerec, MS_FILTER_SET_NCHANNELS, &capture_nchannels);
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_CLOSE);
	ms_filter_call_method(ms_tester_filerec, MS_FILE_REC_OPEN, writable_filename);
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_START);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_soundread, -1, 0);
	ms_connection_helper_link(&h, ms_tester_filerec, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_soundread);


	/* set sample rate and channel number to all filters (might need to check the return value to insert a resampler if needed?) */
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);
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


	ms_filter_add_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL,TRUE);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_link(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_decoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_voidsource);

	ms_tester_tone_generation_and_detection_loop();

	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_CLOSE);
	ms_ticker_detach(ms_tester_ticker, ms_tester_soundread);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_soundread, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_filerec, 0, -1);

	// Read the previous file and play it
	ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_SAMPLE_RATE, &playback_sample_rate);
	ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_NCHANNELS, &playback_nchannels);
	if ((capture_sample_rate != playback_sample_rate) || (capture_nchannels != playback_nchannels)) {
		ms_tester_create_filter(&ms_tester_resampler, MS_RESAMPLE_ID, scm->factory);
	}
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method(ms_tester_fileplay, MS_FILE_PLAYER_OPEN, writable_filename);
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_START);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_fileplay, -1, 0);

	ms_ticker_detach(ms_tester_ticker, ms_tester_voidsource);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_decoder, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	//	ms_filter_log_statistics();
	ms_factory_log_statistics(scm->factory);
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

#endif


static void dtmfgen_enc_dec_tonedet_pcmu(void) {
	if (ms_factory_codec_supported(factory, "pcmu")) {
		dtmfgen_enc_dec_tonedet("pcmu", 8000, 1, FALSE);
	}
}

static void dtmfgen_enc_dec_tonedet_bv16(void) {
	if (ms_factory_codec_supported(factory, "bv16")) {
		dtmfgen_enc_dec_tonedet("bv16", 8000, 1, TRUE);
	}
}
static void dtmfgen_enc_dec_tonedet_isac(void) {
	if (ms_factory_codec_supported(factory, "iSAC")) {
		dtmfgen_enc_dec_tonedet("iSAC", 16000, 1,FALSE);
	}
}

#if HAVE_OPUS
static void dtmfgen_enc_dec_tonedet_opus(void) {
	dtmfgen_enc_dec_tonedet("opus", 48000, 1, FALSE);
}
#endif

static void dtmfgen_enc_rtp_dec_tonedet(void) {
	MSConnectionHelper h;
	RtpSession *rtps;
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_ENCODER
		| FILTER_MASK_RTPSEND | FILTER_MASK_RTPRECV | FILTER_MASK_DECODER | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;
	bool_t send_silence = TRUE;
	MSSndCardManager *scm = ms_factory_get_snd_card_manager(factory);
	ms_factory_reset_statistics(factory);

	//ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_codec_mime = "pcmu";
	ms_tester_create_filters(filter_mask, factory);
	ms_filter_add_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL,TRUE);
	rtps = ms_create_duplex_rtp_session("0.0.0.0", 50060, 0, ms_factory_get_mtu(factory));
	rtp_session_set_remote_addr_full(rtps, "127.0.0.1", 50060, "127.0.0.1", 50061);
	rtp_session_set_payload_type(rtps, 8);
	rtp_session_enable_rtcp(rtps,FALSE);
	ms_filter_call_method(ms_tester_rtprecv, MS_RTP_RECV_SET_SESSION, rtps);
	ms_filter_call_method(ms_tester_rtpsend, MS_RTP_SEND_SET_SESSION, rtps);
	ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);
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
//	ms_filter_log_statistics();
	ms_factory_log_statistics(scm->factory);
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
	rtp_session_destroy(rtps);
}

#define DTMFGEN_FILE_NAME  "dtmfgen_file.raw"

static void dtmfgen_filerec_fileplay_tonedet(void) {
	MSConnectionHelper h;
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_FILEREC
		| FILTER_MASK_FILEPLAY | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;
	bool_t send_silence = TRUE;
	char* recorded_file = bc_tester_file(DTMFGEN_FILE_NAME);


	ms_factory_reset_statistics(factory);
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, factory);
	ms_filter_add_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL,TRUE);

	// Generate tones and save them to a file
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_CLOSE);
	ms_filter_call_method(ms_tester_filerec, MS_FILE_REC_OPEN, recorded_file);
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_START);
	ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);
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
	ms_filter_call_method(ms_tester_fileplay, MS_FILE_PLAYER_OPEN, recorded_file);
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
	ms_factory_log_statistics(factory);
//	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
	unlink(recorded_file);
    free(recorded_file);
}


test_t basic_audio_tests[] = {
	TEST_NO_TAG("dtmfgen-tonedet", dtmfgen_tonedet),
	TEST_NO_TAG("dtmfgen-enc-dec-tonedet-bv16", dtmfgen_enc_dec_tonedet_bv16),
	TEST_NO_TAG("dtmfgen-enc-dec-tonedet-pcmu", dtmfgen_enc_dec_tonedet_pcmu),
	TEST_NO_TAG("dtmfgen-enc-dec-tonedet-isac", dtmfgen_enc_dec_tonedet_isac),
#if HAVE_OPUS
	TEST_NO_TAG("dtmfgen-enc-dec-tonedet-opus", dtmfgen_enc_dec_tonedet_opus),
#endif
	TEST_NO_TAG("dtmfgen-enc-rtp-dec-tonedet", dtmfgen_enc_rtp_dec_tonedet),
	TEST_NO_TAG("dtmfgen-filerec-fileplay-tonedet", dtmfgen_filerec_fileplay_tonedet)
};

test_suite_t basic_audio_test_suite = {
	"Basic Audio",
	basic_audio_tester_before_all,
	basic_audio_tester_after_all,
	NULL,
	NULL,
	sizeof(basic_audio_tests) / sizeof(basic_audio_tests[0]),
	basic_audio_tests
};
