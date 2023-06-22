/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <bctoolbox/defs.h>

#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2/msvolume.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

#include <sys/stat.h>

static MSFactory *msFactory = NULL;
static int basic_audio_tester_before_all(void) {

	msFactory = ms_tester_factory_new();

	ms_factory_enable_statistics(msFactory, TRUE);
	ortp_init();
	return 0;
}

static int basic_audio_tester_after_all(void) {
	ms_factory_destroy(msFactory);
	ortp_exit();
	return 0;
}

static void tone_detected_cb(BCTBX_UNUSED(void *data),
                             BCTBX_UNUSED(MSFilter *f),
                             BCTBX_UNUSED(unsigned int event_id),
                             BCTBX_UNUSED(MSToneDetectorEvent *ev)) {
	ms_tester_tone_detected = TRUE;
}

#define TEST_SILENCE_VOICE_48000_FILE_NAME "sounds/test_silence_voice_48000.wav"
#define TEST_SILENCE_VOICE_44100_FILE_NAME "sounds/test_silence_voice_44100.wav"
#define TEST_SILENCE_VOICE_32000_FILE_NAME "sounds/test_silence_voice_32000.wav"
#define TEST_SILENCE_VOICE_16000_FILE_NAME "sounds/test_silence_voice_16000.wav"
#define TEST_SILENCE_VOICE_8000_FILE_NAME "sounds/test_silence_voice_8000.wav"
#define MSWEBRTC_VAD_FILTER_NAME "MSWebRtcVADDec"

typedef struct struct_silence_callback_data {
	int voice_detected_number;
	uint64_t silence_duration[10];
} silence_callback_data;

static void silence_detected_cb(void *data, BCTBX_UNUSED(MSFilter *f), unsigned int event_id, void *arg) {
	silence_callback_data *silence = (silence_callback_data *)data;
	if (event_id == MS_VAD_EVENT_SILENCE_DETECTED) {
		silence->voice_detected_number++;
	} else if (event_id == MS_VAD_EVENT_SILENCE_ENDED) {
		silence->silence_duration[silence->voice_detected_number - 1] = *(uint64_t *)arg;
	}
}

typedef struct struct_player_callback_data {
	int end_of_file;
} player_callback_data;

static void player_cb(void *data, BCTBX_UNUSED(MSFilter *f), unsigned int event_id, BCTBX_UNUSED(void *arg)) {
	if (event_id == MS_FILE_PLAYER_EOF) {
		player_callback_data *player = (player_callback_data *)data;
		player->end_of_file = TRUE;
	}
}

// Waiting time in ms
static void _silence_detection(const char *filename,
                               unsigned int duration_threshold,
                               uint64_t *silence_duration,
                               uint64_t delay,
                               int vad_mode,
                               int number_detection,
                               int waiting_time) {
	int sample_rate;
	MSConnectionHelper h;
	silence_callback_data silence_data;
	player_callback_data player_data;
	MSFilter *voice_detector;
	unsigned int filter_mask = FILTER_MASK_FILEPLAY | FILTER_MASK_VOIDSINK;
	unsigned int enable_silence = 1;
	char *recorded_file = bc_tester_res(filename);
	MSFilterDesc *vad_desc = ms_factory_lookup_filter_by_name(msFactory, MSWEBRTC_VAD_FILTER_NAME);
	int i;

	if (!recorded_file) return;

	// Skip test if mswebrtc vad plugin not loaded
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

	ms_filter_call_method(voice_detector, MS_VAD_ENABLE_SILENCE_DETECTION, (void *)&enable_silence);
	ms_filter_call_method(voice_detector, MS_VAD_SET_SILENCE_DURATION_THRESHOLD, (void *)&duration_threshold);
	ms_filter_call_method(voice_detector, MS_FILTER_SET_SAMPLE_RATE, (void *)&sample_rate);
	ms_filter_call_method(voice_detector, MS_VAD_SET_MODE, (void *)&vad_mode);

	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_fileplay, -1, 0);
	ms_connection_helper_link(&h, voice_detector, 0, 0);
	ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_fileplay);

	BC_ASSERT_TRUE(wait_for_until(NULL, NULL, &player_data.end_of_file, TRUE, waiting_time));

	BC_ASSERT_EQUAL(silence_data.voice_detected_number, number_detection, int, "%d");
	if (number_detection > 0 && number_detection == silence_data.voice_detected_number) {
		for (i = 0; i < number_detection - 1; i++) {
			BC_ASSERT_LOWER_STRICT((unsigned long long)silence_data.silence_duration[i],
			                       (unsigned long long)(silence_duration[i] + delay), unsigned long long, "%llu");
			BC_ASSERT_GREATER_STRICT((unsigned long long)silence_data.silence_duration[i],
			                         (unsigned long long)(silence_duration[i] - delay), unsigned long long, "%llu");
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
	unsigned int filter_mask =
	    FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;
	bool_t send_silence = TRUE;

	//	ms_filter_reset_statistics();
	ms_factory_reset_statistics(msFactory);

	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);
	ms_filter_add_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL, TRUE);
	ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_link(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_voidsource);

	ms_tester_tone_generation_and_detection_loop();

	/*try unrecognized DTMF*/
	BC_ASSERT_NOT_EQUAL(ms_filter_call_method(ms_tester_dtmfgen, MS_DTMF_GEN_PLAY, "F"), 0, int, "%d");

	ms_ticker_detach(ms_tester_ticker, ms_tester_voidsource);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_tonedet, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	ms_factory_log_statistics(msFactory);
	// ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

/*fileplay awt to TRUE:  uses soundwrite instaed of voidsink  so we can hear what;s going on */
static void dtmfgen_enc_dec_tonedet(char *mime, int sample_rate, int nchannels, bool_t fileplay) {
	MSConnectionHelper h;
	unsigned int filter_mask;
	bool_t send_silence = TRUE;
	MSSndCardManager *scm = ms_factory_get_snd_card_manager(msFactory);
	ms_factory_reset_statistics(scm->factory);

	if (!fileplay) {
		filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_ENCODER | FILTER_MASK_DECODER |
		              FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;

	} else {
		filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_ENCODER | FILTER_MASK_DECODER |
		              FILTER_MASK_TONEDET | FILTER_MASK_SOUNDWRITE;
	}

	// ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_codec_mime = mime;
	ms_tester_create_filters(filter_mask, scm->factory);

	/* set sample rate and channel number to all filters (might need to check the return value to insert a resampler if
	 * needed?) */
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

	if (!fileplay) {
		ms_filter_call_method(ms_tester_voidsink, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
		ms_filter_call_method(ms_tester_voidsink, MS_FILTER_SET_NCHANNELS, &nchannels);
	} else {

		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels);
	}

	ms_filter_add_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL, TRUE);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	ms_connection_helper_link(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_decoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_tonedet, 0, 0);
	if (!fileplay) {
		ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	} else {
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
	if (!fileplay) {
		ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	} else {
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
		dtmfgen_enc_dec_tonedet("iSAC", 16000, 1, FALSE);
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
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_ENCODER |
	                           FILTER_MASK_RTPSEND | FILTER_MASK_RTPRECV | FILTER_MASK_DECODER | FILTER_MASK_TONEDET |
	                           FILTER_MASK_VOIDSINK;
	bool_t send_silence = TRUE;
	MSSndCardManager *scm = ms_factory_get_snd_card_manager(msFactory);
	ms_factory_reset_statistics(msFactory);

	// ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_codec_mime = "pcmu";
	ms_tester_create_filters(filter_mask, msFactory);
	ms_filter_add_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL, TRUE);
	rtps = ms_create_duplex_rtp_session("0.0.0.0", 50060, 0, ms_factory_get_mtu(msFactory));
	rtp_session_set_remote_addr_full(rtps, "127.0.0.1", 50060, "127.0.0.1", 50061);
	rtp_session_set_payload_type(rtps, 8);
	rtp_session_enable_rtcp(rtps, FALSE);
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

#define DTMFGEN_FILE_NAME "dtmfgen_file.raw"

static void dtmfgen_filerec_fileplay_tonedet(void) {
	MSConnectionHelper h;
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_FILEREC |
	                           FILTER_MASK_FILEPLAY | FILTER_MASK_TONEDET | FILTER_MASK_VOIDSINK;
	bool_t send_silence = TRUE;
	char *recorded_file = bc_tester_file(DTMFGEN_FILE_NAME);

	ms_factory_reset_statistics(msFactory);
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);
	ms_filter_add_notify_callback(ms_tester_tonedet, (MSFilterNotifyFunc)tone_detected_cb, NULL, TRUE);

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

#define SOUND_TEST_1 "sounds/hello8000.wav"
#define SOUND_TEST_2 "sounds/arpeggio_8000_mono.wav"
#define RECORD_SOUND "mixed_file.wav"

static void _two_mono_into_one_stereo(bool_t with_unsynchronized_inputs) {
	// struct stat sound_file1, sound_file2, sound_record;
	// unsigned int max_sound_size;
	player_callback_data player1_data, player2_data;
	MSFilter *mixer_mono, *player1, *player2;
	unsigned int filter_mask = FILTER_MASK_FILEREC;
	int sample_rate1, sample_rate2, nb_channels = 2;
	char *played_file1 = bc_tester_res(SOUND_TEST_1);
	char *played_file2 = bc_tester_res(SOUND_TEST_2);
	char *recorded_file = bc_tester_file(RECORD_SOUND);

	unlink(recorded_file); /*make sure the file doesn't exist, otherwise new content will be appended.*/
	player1_data.end_of_file = FALSE;
	player2_data.end_of_file = FALSE;

	ms_factory_reset_statistics(msFactory);
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);

	player1 = ms_factory_create_filter(msFactory, MS_FILE_PLAYER_ID);
	player2 = ms_factory_create_filter(msFactory, MS_FILE_PLAYER_ID);
	mixer_mono = ms_factory_create_filter(msFactory, MS_CHANNEL_ADAPTER_ID);

	ms_filter_add_notify_callback(player1, player_cb, &player1_data, TRUE);
	ms_filter_add_notify_callback(player2, player_cb, &player2_data, TRUE);

	ms_filter_call_method(mixer_mono, MS_FILTER_SET_NCHANNELS, &nb_channels);

	ms_filter_call_method(player1, MS_FILE_PLAYER_OPEN, played_file1);
	ms_filter_call_method(player2, MS_FILE_PLAYER_OPEN, played_file2);

	ms_filter_call_method(player1, MS_FILTER_GET_SAMPLE_RATE, &sample_rate1);
	ms_filter_call_method(player2, MS_FILTER_GET_SAMPLE_RATE, &sample_rate2);

	BC_ASSERT_EQUAL(sample_rate1, sample_rate2, int, "%i");
	if (sample_rate1 != sample_rate2) {
		ms_error("The two sounds do not have the same sample rate");
		ms_filter_call_method_noarg(player1, MS_FILE_PLAYER_CLOSE);
		ms_filter_call_method_noarg(player2, MS_FILE_PLAYER_CLOSE);
		goto end;
	}

	if (with_unsynchronized_inputs) {
		int bit_too_fast_rate = sample_rate1 + (sample_rate1 * 10 / 100);
		ms_filter_call_method(player1, MS_FILTER_SET_SAMPLE_RATE, &bit_too_fast_rate);
		ms_message("Player 1 configured with a sample rate of %i Hz", bit_too_fast_rate);
	}

	ms_filter_call_method(mixer_mono, MS_FILTER_SET_SAMPLE_RATE, &sample_rate1);

	ms_filter_call_method(ms_tester_filerec, MS_FILE_REC_OPEN, recorded_file);
	ms_filter_call_method(ms_tester_filerec, MS_FILTER_SET_SAMPLE_RATE, &sample_rate1);
	ms_filter_call_method(ms_tester_filerec, MS_FILTER_SET_NCHANNELS, &nb_channels);

	ms_filter_link(player1, 0, mixer_mono, 0);
	ms_filter_link(player2, 0, mixer_mono, 1);
	ms_filter_link(mixer_mono, 0, ms_tester_filerec, 0);

	ms_filter_call_method_noarg(player1, MS_FILE_PLAYER_START);
	ms_filter_call_method_noarg(player2, MS_FILE_PLAYER_START);
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_START);

	ms_ticker_attach(ms_tester_ticker, player1);

	BC_ASSERT_TRUE(wait_for_until(NULL, NULL, &player1_data.end_of_file, TRUE, 15000));
	BC_ASSERT_TRUE(wait_for_until(NULL, NULL, &player2_data.end_of_file, TRUE, 15000));

	ms_filter_call_method_noarg(player1, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method_noarg(player2, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_STOP);
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_CLOSE);

	ms_ticker_detach(ms_tester_ticker, player1);

	ms_filter_unlink(player1, 0, mixer_mono, 0);
	ms_filter_unlink(player2, 0, mixer_mono, 1);
	ms_filter_unlink(mixer_mono, 0, ms_tester_filerec, 0);

	/* TODO
	stat(SOUND_TEST_1, &sound_file1);
	stat(SOUND_TEST_2, &sound_file2);
	stat(RECORD_SOUND, &sound_record);

	max_sound_size = (sound_file1.st_size > sound_file2.st_size) ? sound_file1.st_size : sound_file2.st_size;

	BC_ASSERT_EQUAL((unsigned long long)sound_record.st_size, ((unsigned long long)max_sound_size) * 2 - 44, unsigned
	long long, "%llu");
	*/

end:
	ms_factory_log_statistics(msFactory);

	if (player1) ms_filter_destroy(player1);
	if (player2) ms_filter_destroy(player2);
	if (mixer_mono) ms_filter_destroy(mixer_mono);

	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();

	if (recorded_file) ms_free(recorded_file);
	if (played_file1) ms_free(played_file1);
	if (played_file2) ms_free(played_file2);
}

static void two_mono_into_one_stereo(void) {
	_two_mono_into_one_stereo(FALSE);
}

static void two_mono_into_one_stereo_with_unsynchronized_inputs(void) {
	_two_mono_into_one_stereo(TRUE);
}

static void max_ptime(void) {
	const bctbx_list_t *filters = ms_factory_get_filter_decs(msFactory);
	bctbx_list_t *it = bctbx_list_copy(filters);
	for (; it != NULL; it = bctbx_list_next(it)) {
		MSFilterDesc *desc = bctbx_list_get_data(it);
		if (desc->category != MS_FILTER_ENCODER ||
		    ms_filter_desc_implements_interface(desc, MSFilterVideoEncoderInterface))
			continue;

		MSFilter *filter = ms_factory_create_filter_from_desc(msFactory, desc);
		if (!ms_filter_has_method(filter, MS_AUDIO_ENCODER_GET_PTIME)) {
			ms_message("Skiping max ptime for [%s , %s] as no method MS_AUDIO_ENCODER_GET_PTIME implemented",
			           desc->name, desc->text);
		} else {
			ms_message("Testing max ptime for [%s , %s", desc->name, desc->text);
			BC_ASSERT_EQUAL(ms_filter_call_method(filter, MS_FILTER_ADD_FMTP, (void *)"maxptime=60"), 0, int, "%i");

			int ptime = 40;
			int read_ptime;
			if (ms_filter_has_method(filter, MS_AUDIO_ENCODER_SET_PTIME))
				ms_filter_call_method(filter, MS_AUDIO_ENCODER_SET_PTIME, &ptime);
			else BC_ASSERT_EQUAL(ms_filter_call_method(filter, MS_FILTER_ADD_FMTP, (void *)"ptime=40"), 0, int, "%i");

			ms_filter_call_method(filter, MS_AUDIO_ENCODER_GET_PTIME, &read_ptime);
			BC_ASSERT_EQUAL(read_ptime, ptime, int, "%i");

			ptime = 80;
			if (ms_filter_has_method(filter, MS_AUDIO_ENCODER_SET_PTIME))
				ms_filter_call_method(filter, MS_AUDIO_ENCODER_SET_PTIME, &ptime);
			else BC_ASSERT_EQUAL(ms_filter_call_method(filter, MS_FILTER_ADD_FMTP, (void *)"ptime=80"), 0, int, "%i");
			ms_filter_call_method(filter, MS_AUDIO_ENCODER_GET_PTIME, &read_ptime);
			BC_ASSERT_EQUAL(read_ptime, 60, int, "%i");
		}
		ms_filter_destroy(filter);
	}
	bctbx_list_free(it);
}

test_t basic_audio_tests[] = {TEST_ONE_TAG("silence detection 48000", silence_detection_48000, "VAD"),
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
                              TEST_NO_TAG("dtmfgen-filerec-fileplay-tonedet", dtmfgen_filerec_fileplay_tonedet),
                              TEST_NO_TAG("Mix two mono files into one stereo file", two_mono_into_one_stereo),
                              TEST_NO_TAG("Mix two mono files into one stereo file with unsynchronized inputs",
                                          two_mono_into_one_stereo_with_unsynchronized_inputs),
                              TEST_NO_TAG("Max ptime", max_ptime)};

test_suite_t basic_audio_test_suite = {"Basic Audio",
                                       basic_audio_tester_before_all,
                                       basic_audio_tester_after_all,
                                       NULL,
                                       NULL,
                                       sizeof(basic_audio_tests) / sizeof(basic_audio_tests[0]),
                                       basic_audio_tests};
