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
#include "mediastreamer2/msvolume.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "private.h"

static MSFactory *msFactory = NULL;
static int basic_audio_tester_before_all(void) {

	msFactory = ms_factory_new_with_voip();

	ms_factory_enable_statistics(msFactory, TRUE);
	ortp_init();
	return 0;
}

static int basic_audio_tester_after_all(void) {
	ms_factory_destroy(msFactory);
	ortp_exit();
	return 0;
}

static void tone_detected_cb(void *data, MSFilter *f, unsigned int event_id, MSToneDetectorEvent *ev) {
	MS_UNUSED(data), MS_UNUSED(f), MS_UNUSED(event_id), MS_UNUSED(ev);
	ms_tester_tone_detected = TRUE;
}

#define TEST_SILENCE_VOICE_48000_FILE_NAME	"sounds/test_silence_voice_48000.wav"
#define TEST_SILENCE_VOICE_44100_FILE_NAME	"sounds/test_silence_voice_44100.wav"
#define TEST_SILENCE_VOICE_32000_FILE_NAME	"sounds/test_silence_voice_32000.wav"
#define TEST_SILENCE_VOICE_16000_FILE_NAME	"sounds/test_silence_voice_16000.wav"
#define TEST_SILENCE_VOICE_8000_FILE_NAME	"sounds/test_silence_voice_8000.wav"
#define MSWEBRTC_VAD_FILTER_NAME	"MSWebRtcVADDec"

typedef struct struct_silence_callback_data {
	int voice_detected_number;
	uint64_t silence_duration[10];
} silence_callback_data;

static void silence_detected_cb(void *data, MSFilter *f, unsigned int event_id, void *arg) {
	silence_callback_data *silence = (silence_callback_data *)data;
	if (event_id == MS_VAD_EVENT_SILENCE_DETECTED) {
		silence->voice_detected_number++;
	} else if (event_id == MS_VAD_EVENT_SILENCE_ENDED) {
		silence->silence_duration[silence->voice_detected_number-1] = *(uint64_t*)arg;
	}
}

typedef struct struct_player_callback_data {
	int end_of_file;
} player_callback_data;

static void player_cb(void *data, MSFilter *f, unsigned int event_id, void *arg) {
	if (event_id == MS_FILE_PLAYER_EOF) {
		player_callback_data *player = (player_callback_data *)data;
		player->end_of_file = TRUE;
	}
}

// Waiting time in ms
static void _silence_detection(const char* filename, unsigned int duration_threshold, uint64_t* silence_duration, uint64_t delay, int vad_mode, int number_detection, int waiting_time) {
	int sample_rate;
	MSConnectionHelper h;
	silence_callback_data silence_data;
	player_callback_data player_data;
	MSFilter *voice_detector;
	unsigned int filter_mask = FILTER_MASK_FILEPLAY | FILTER_MASK_VOIDSINK;
	unsigned int enable_silence = 1;
	char* recorded_file = bc_tester_res(filename);
	MSFilterDesc *vad_desc = ms_factory_lookup_filter_by_name(msFactory, MSWEBRTC_VAD_FILTER_NAME);

	if (!recorded_file) return;

	//Skip test if mswebrtc vad plugin not loaded
	if (vad_desc == NULL) goto end;

	silence_data.voice_detected_number = 0;
	player_data.end_of_file = FALSE;

	ms_factory_reset_statistics(msFactory);
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);

	voice_detector = ms_factory_create_filter_from_desc(msFactory, vad_desc);
	ms_filter_add_notify_callback(voice_detector, silence_detected_cb, &silence_data, TRUE);
	ms_filter_add_notify_callback(ms_tester_fileplay, player_cb, &player_data, TRUE);

	ms_filter_call_method(ms_tester_fileplay, MS_FILE_PLAYER_OPEN, recorded_file);
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_START);
	ms_filter_call_method(ms_tester_fileplay, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);

	ms_filter_call_method(voice_detector, MS_VAD_ENABLE_SILENCE_DETECTION, (void*)&enable_silence);
	ms_filter_call_method(voice_detector, MS_VAD_SET_SILENCE_DURATION_THRESHOLD, (void*)&duration_threshold);
	ms_filter_call_method(voice_detector, MS_FILTER_SET_SAMPLE_RATE, (void*)&sample_rate);
	ms_filter_call_method(voice_detector, MS_VAD_SET_MODE, (void*)&vad_mode);

	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_fileplay, -1, 0);
	ms_connection_helper_link(&h, voice_detector, 0, 0);
	ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_fileplay);

	BC_ASSERT_TRUE(wait_for_until(NULL, NULL, &player_data.end_of_file, TRUE, waiting_time));

	BC_ASSERT_EQUAL(silence_data.voice_detected_number, number_detection, int, "%d");
	if (number_detection > 0 && number_detection == silence_data.voice_detected_number) {
		for (int i = 0 ; i < number_detection-1 ; i++) {
			BC_ASSERT_LOWER_STRICT((unsigned long long)silence_data.silence_duration[i], (unsigned long long)(silence_duration[i] + delay), unsigned long long, "%llu");
			BC_ASSERT_GREATER_STRICT((unsigned long long)silence_data.silence_duration[i], (unsigned long long)(silence_duration[i] - delay), unsigned long long, "%llu");
		}
	}

	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);
	ms_ticker_detach(ms_tester_ticker, ms_tester_fileplay);

	ms_connection_helper_start(&h);

	ms_connection_helper_unlink(&h, ms_tester_fileplay, -1, 0);
	ms_connection_helper_unlink(&h, voice_detector, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);

	ms_factory_log_statistics(msFactory);

	if (voice_detector) ms_filter_destroy(voice_detector);
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();

	end:
	ms_free(recorded_file);
}

static void silence_detection_48000(void) {
	uint64_t duration[5] = {3710, 2210, 1780, 5290};
	_silence_detection(TEST_SILENCE_VOICE_48000_FILE_NAME, 1000, duration, 50, 3, 5, 26000);
}

static void silence_detection_44100(void) {
	_silence_detection(TEST_SILENCE_VOICE_44100_FILE_NAME, 1000, NULL, 0, 3, 0, 26000);
}

static void silence_detection_32000(void) {
	uint64_t duration[6] = {3710, 2210, 1780, 1050, 5290};
	_silence_detection(TEST_SILENCE_VOICE_32000_FILE_NAME, 1000, duration, 50, 3, 6, 26000);
}

static void silence_detection_16000(void) {
	uint64_t duration[6] = {3710, 2210, 1780, 1050, 5290};
	_silence_detection(TEST_SILENCE_VOICE_16000_FILE_NAME, 1000, duration, 50, 3, 6, 26000);
}

static void silence_detection_8000(void) {
	uint64_t duration[6] = {3710, 2210, 1780, 1050, 5290};
	_silence_detection(TEST_SILENCE_VOICE_8000_FILE_NAME, 1000, duration, 50, 3, 6, 26000);
}

static void dtmfgen_tonedet(void) {
	MSConnectionHelper h;
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;
	bool_t send_silence = TRUE;


//	ms_filter_reset_statistics();
	ms_factory_reset_statistics(msFactory);

	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);
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
	ms_factory_log_statistics(msFactory);
	//ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

/*fileplay awt to TRUE:  uses soundwrite instaed of voidsink  so we can hear what;s going on */
static void dtmfgen_enc_dec_tonedet(char *mime, int sample_rate, int nchannels, bool_t fileplay) {
	MSConnectionHelper h;
	unsigned int filter_mask ;
	bool_t send_silence = TRUE;
	MSSndCardManager *scm = ms_factory_get_snd_card_manager(msFactory);
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
	if (ms_factory_codec_supported(msFactory, "pcmu")) {
		dtmfgen_enc_dec_tonedet("pcmu", 8000, 1, FALSE);
	}
}

static void dtmfgen_enc_dec_tonedet_bv16(void) {
	if (ms_factory_codec_supported(msFactory, "bv16")) {
		dtmfgen_enc_dec_tonedet("bv16", 8000, 1, TRUE);
	}
}
static void dtmfgen_enc_dec_tonedet_isac(void) {
	if (ms_factory_codec_supported(msFactory, "iSAC")) {
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
	MSSndCardManager *scm = ms_factory_get_snd_card_manager(msFactory);
	ms_factory_reset_statistics(msFactory);

	//ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_codec_mime = "pcmu";
	ms_tester_create_filters(filter_mask, msFactory);
	ms_filter_add_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL,TRUE);
	rtps = ms_create_duplex_rtp_session("0.0.0.0", 50060, 0, ms_factory_get_mtu(msFactory));
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


	ms_factory_reset_statistics(msFactory);
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);
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
	ms_factory_log_statistics(msFactory);
//	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
	unlink(recorded_file);
	free(recorded_file);
}


test_t basic_audio_tests[] = {
	TEST_ONE_TAG("silence detection 48000", silence_detection_48000, "VAD"),
	TEST_ONE_TAG("silence detection 44100", silence_detection_44100, "VAD"),
	TEST_ONE_TAG("silence detection 32000", silence_detection_32000, "VAD"),
	TEST_ONE_TAG("silence detection 16000", silence_detection_16000, "VAD"),
	TEST_ONE_TAG("silence detection 8000", silence_detection_8000, "VAD"),
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
