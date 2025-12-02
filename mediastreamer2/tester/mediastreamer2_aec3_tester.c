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

#include "bctoolbox/defs.h"
#include "bctoolbox/tester.h"
#include "mediastreamer2/allfilters.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfactory.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msinterfaces.h"
#include "mediastreamer2/msutils.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "ortp/port.h"

#ifdef ENABLE_WEBRTC_AEC
extern void libmswebrtc_init(MSFactory *factory);
#endif

#define EC_DUMP 0

#define NEAR_END_SPEECH_DOUBLE_TALK "sounds/nearend_double_talk.wav"
#define FAR_END_SPEECH_DOUBLE_TALK "sounds/farend_double_talk.wav"
#define ECHO_DOUBLE_TALK "sounds/echo_double_talk_100ms.wav"
#define NEAR_END_SPEECH_SIMPLE_TALK "sounds/nearend_simple_talk.wav"
#define FAR_END_SPEECH_SIMPLE_TALK "sounds/farend_simple_talk.wav"
#define ECHO_SIMPLE_TALK_100MS "sounds/echo_simple_talk_100ms.wav"
#define ECHO_SIMPLE_TALK_600MS "sounds/echo_simple_talk_600ms.wav"
#define ECHO_DELAY_CHANGE                                                                                              \
	"sounds/echo_delay_change.wav" // like ECHO_SIMPLE_TALK but here the echo is shifted of 50 ms from far-end around
	                               // 10 s.
#define WHITE_NOISE_FILE "sounds/white_noise.wav"

typedef struct _aec_test_config {
	int sampling_rate;
	int nchannels;
	int audio_max_duration_ms;
	char *nearend_speech_file;
	char *farend_speech_file;
	char *echo_file;
	char *record_file;
	char *noise_file;
	char *mic_rec_file;
	char *output_ref_rec_file;
	bool_t set_delay_to_aec_filter;
	bool_t reference_packets_lost;
} aec_test_config;

static void init_config(aec_test_config *config) {
	config->sampling_rate = 16000;
	config->nchannels = 1;
	config->audio_max_duration_ms = 32000;
	config->nearend_speech_file = NULL;
	config->farend_speech_file = NULL;
	config->echo_file = NULL;
	config->record_file = NULL;
	config->noise_file = NULL;
	config->set_delay_to_aec_filter = TRUE;
	config->mic_rec_file = bc_tester_file("aec_input_mic.wav");
	config->output_ref_rec_file = bc_tester_file("aec_output_ref.wav");
	config->reference_packets_lost = FALSE;
}

static void uninit_config(aec_test_config *config) {
	if (config->nearend_speech_file) free(config->nearend_speech_file);
	if (config->farend_speech_file) free(config->farend_speech_file);
	if (config->echo_file) free(config->echo_file);
	if (config->noise_file) free(config->noise_file);
	if (config->record_file) {
#if EC_DUMP != 1
		unlink(config->record_file);
		unlink(config->mic_rec_file);
		unlink(config->output_ref_rec_file);
#endif
		ms_free(config->record_file);
		ms_free(config->mic_rec_file);
		ms_free(config->output_ref_rec_file);
	}
	config->nearend_speech_file = NULL;
	config->farend_speech_file = NULL;
	config->echo_file = NULL;
	config->noise_file = NULL;
	config->record_file = NULL;
	config->mic_rec_file = NULL;
	config->output_ref_rec_file = NULL;
}

typedef struct _audio_analysis_param {
	int expected_final_delay_ms;
	int start_time_short_ms;
	int stop_time_short_ms;
	int start_time_audio_diff_ms;
	MSAudioDiffParams audio_cmp_params;
	double threshold_similarity_in_speech;
	double threshold_energy_in_silence;
} audio_analysis_param;

/**This function returns the MSAudioDiffParams for the comparison of audio segments taken between
 * start_time_short_ms and stop_time_short_ms, when one of the audio is supposed to be delayed by
 * a value between 0 and 1.5*delay_ms. The minimal computed shift is 1 percent. */
static MSAudioDiffParams
audio_diff_param(const int delay_ms, const int start_time_short_ms, const int stop_time_short_ms) {
	int max_shift_percent = (int)((double)delay_ms * 1.5 / (double)(stop_time_short_ms - start_time_short_ms) * 100);
	if (delay_ms == 0) max_shift_percent = 1;
	MSAudioDiffParams audio_cmp_params = {max_shift_percent, 0};
	return audio_cmp_params;
}

audio_analysis_param set_audio_analysis_param(const int delay_ms,
                                              const int start_time_short_ms,
                                              const int stop_time_short_ms,
                                              const int start_time_audio_diff_ms,
                                              const double threshold_similarity_in_speech,
                                              const double threshold_energy_in_silence) {

	MSAudioDiffParams audio_cmp_params = audio_diff_param(delay_ms, start_time_short_ms, stop_time_short_ms);
	audio_analysis_param param = {delay_ms,
	                              start_time_short_ms,
	                              stop_time_short_ms,
	                              start_time_audio_diff_ms,
	                              audio_cmp_params,
	                              threshold_similarity_in_speech,
	                              threshold_energy_in_silence};
	return param;
}

static MSFactory *msFactory = NULL;

static int tester_before_all(void) {
	msFactory = ms_tester_factory_new();
	ms_factory_enable_statistics(msFactory, TRUE);
	return 0;
}

static int tester_after_all(void) {
	ms_factory_destroy(msFactory);
	return 0;
}

static void
fileplay_eof(void *user_data, BCTBX_UNUSED(MSFilter *f), unsigned int event, BCTBX_UNUSED(void *event_data)) {
	if (event == MS_FILE_PLAYER_EOF) {
		int *done = (int *)user_data;
		*done = TRUE;
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

static bool_t aec_test_create_player(
    MSFilter **player, MSFilter **resampler, char *input_file, int expected_sampling_rate, int expected_nchannels) {
	if (!input_file) {
		BC_FAIL("no file to play");
		return FALSE;
	}
	int sampling_rate = 0;
	int nchannels = 0;
	*player = ms_factory_create_filter(msFactory, MS_FILE_PLAYER_ID);
	ms_filter_call_method_noarg(*player, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method(*player, MS_FILE_PLAYER_OPEN, input_file);
	ms_filter_call_method(*player, MS_FILTER_GET_SAMPLE_RATE, &sampling_rate);
	ms_filter_call_method(*player, MS_FILTER_GET_NCHANNELS, &nchannels);
	if (nchannels != expected_nchannels) {
		ms_filter_call_method_noarg(*player, MS_FILE_PLAYER_CLOSE);
		BC_FAIL("Audio file does not have the expected channel number");
		return FALSE;
	}
	if (!BC_ASSERT_PTR_NOT_NULL(*player)) {
		return FALSE;
	}
	if (sampling_rate != expected_sampling_rate) {
		*resampler = ms_factory_create_filter(msFactory, MS_RESAMPLE_ID);
		ms_filter_call_method(*resampler, MS_FILTER_SET_SAMPLE_RATE, &sampling_rate);
		ms_filter_call_method(*resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &expected_sampling_rate);
		ms_filter_call_method(*resampler, MS_FILTER_SET_NCHANNELS, &nchannels);
		ms_filter_call_method(*resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &expected_nchannels);
		ms_message("resample audio at rate %d to get %d Hz", sampling_rate, expected_sampling_rate);
	}

	return TRUE;
}

static bool_t aec_base(const aec_test_config *config, int delay_for_init_ms, int *estimated_delay_ms) {

	if (!config->farend_speech_file && config->echo_file) {
		BC_FAIL("Far-end file missing.");
		return FALSE;
	}

	bool_t aec_done = TRUE;
	player_callback_data player_data;
	MSFilter *player_nearend = NULL;
	MSFilter *player_farend = NULL;
	MSFilter *player_echo = NULL;
	MSFilter *player_noise = NULL;
	MSFilter *aec = NULL;
	MSFilter *sound_rec = NULL;
	MSFilter *mixer_mic = NULL;
	MSFilter *mic_rec = NULL;
	MSFilter *output_ref_rec = NULL;
	MSFilter *resampler_nearend = NULL;
	MSFilter *resampler_farend = NULL;
	MSFilter *resampler_echo = NULL;
	MSFilter *resampler_noise = NULL;
	MSFilter *resampler_output = NULL;
	MSFilter *resampler_output_ref = NULL;
	unsigned int filter_mask = FILTER_MASK_FILEREC | FILTER_MASK_FILEPLAY | FILTER_MASK_VOIDSOURCE;
	int config_sampling_rate = config->sampling_rate;
	int config_nchannels = config->nchannels;
	const int expected_sampling_rate = config->sampling_rate;
	const int expected_nchannels = config->nchannels;
	player_data.end_of_file = FALSE;
	int sampling_rate = expected_sampling_rate;
	int nchannels = expected_nchannels;
	int output_sampling_rate = expected_sampling_rate;
	int output_nchannels = expected_nchannels;
	int audio_done = 0;
#if EC_DUMP
	unlink(config->mic_rec_file);
	unlink(config->output_ref_rec_file);
#endif

	ms_factory_reset_statistics(msFactory);
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);

	// AEC3 filter
	MSFilterDesc *ec_desc = ms_factory_lookup_filter_by_name(msFactory, "MSWebRTCAEC");
	bool_t bypass_mode = FALSE;
	aec = ms_factory_create_filter_from_desc(msFactory, ec_desc);
	ms_filter_call_method(aec, MS_ECHO_CANCELLER_SET_STATE_STRING, "1048576");
	ms_filter_call_method(aec, MS_ECHO_CANCELLER_SET_BYPASS_MODE, &bypass_mode);
	if (!BC_ASSERT_PTR_NOT_NULL(aec)) goto end;
	if (ms_filter_call_method(aec, MS_FILTER_SET_SAMPLE_RATE, &config_sampling_rate) == -1) {
		BC_FAIL("Wrong sampling rate, cannot be set for AEC3");
		aec_done = FALSE;
		goto end;
	};
	ms_filter_call_method(aec, MS_FILTER_GET_SAMPLE_RATE, &sampling_rate);
	ms_filter_call_method(aec, MS_FILTER_SET_NCHANNELS, &config_nchannels);
	ms_filter_call_method(aec, MS_FILTER_GET_NCHANNELS, &nchannels);
	if ((sampling_rate != expected_sampling_rate) || (nchannels != expected_nchannels)) {
		BC_FAIL("AEC filter does not have the expected sampling rate and/or channel number");
		aec_done = FALSE;
		goto end;
	}
	ms_filter_call_method(aec, MS_ECHO_CANCELLER_SET_DELAY, &delay_for_init_ms);

	// file players
	// near-end speech
	if (config->nearend_speech_file) {
		if (!aec_test_create_player(&player_nearend, &resampler_nearend, config->nearend_speech_file,
		                            config->sampling_rate, config->nchannels)) {
			aec_done = FALSE;
			goto end;
		}
		if (resampler_nearend) {
			ms_filter_call_method(player_nearend, MS_FILTER_GET_SAMPLE_RATE, &output_sampling_rate);
			ms_filter_call_method(player_nearend, MS_FILTER_GET_NCHANNELS, &output_nchannels);
		}
	}
	// far-end speech and echo
	if (config->farend_speech_file) {
		if (!aec_test_create_player(&player_farend, &resampler_farend, config->farend_speech_file,
		                            config->sampling_rate, config->nchannels)) {
			aec_done = FALSE;
			goto end;
		}
	} else {
		bool_t send_silence = TRUE;
		ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_SAMPLE_RATE, &config_sampling_rate);
		ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_NCHANNELS, &config_nchannels);
		ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);
	}
	// echo
	if (config->echo_file) {
		ms_filter_add_notify_callback(player_farend, player_cb, &player_data, TRUE);
		if (!aec_test_create_player(&player_echo, &resampler_echo, config->echo_file, config->sampling_rate,
		                            config->nchannels)) {
			aec_done = FALSE;
			goto end;
		}
	} else {
		ms_filter_add_notify_callback(player_nearend, player_cb, &player_data, TRUE);
	}
	// noise
	if (config->noise_file) {
		if (!aec_test_create_player(&player_noise, &resampler_noise, config->noise_file, config->sampling_rate,
		                            config->nchannels)) {
			aec_done = FALSE;
			goto end;
		}
	}

	// resampler before record
	if (resampler_nearend) {
		resampler_output = ms_factory_create_filter(msFactory, MS_RESAMPLE_ID);
		ms_filter_call_method(resampler_output, MS_FILTER_SET_SAMPLE_RATE, &config_sampling_rate);
		ms_filter_call_method(resampler_output, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &output_sampling_rate);
		ms_filter_call_method(resampler_output, MS_FILTER_SET_NCHANNELS, &config_nchannels);
		ms_filter_call_method(resampler_output, MS_FILTER_SET_OUTPUT_NCHANNELS, &output_nchannels);
		ms_message("resample output for rate %d to get %d Hz, for comparison with nearend file", config->sampling_rate,
		           output_sampling_rate);
		resampler_output_ref = ms_factory_create_filter(msFactory, MS_RESAMPLE_ID);
		ms_filter_call_method(resampler_output_ref, MS_FILTER_SET_SAMPLE_RATE, &config_sampling_rate);
		ms_filter_call_method(resampler_output_ref, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &output_sampling_rate);
		ms_filter_call_method(resampler_output_ref, MS_FILTER_SET_NCHANNELS, &config_nchannels);
		ms_filter_call_method(resampler_output_ref, MS_FILTER_SET_OUTPUT_NCHANNELS, &output_nchannels);
		ms_message("resample output reference for rate %d to get %d Hz, for comparison with farend file",
		           config->sampling_rate, output_sampling_rate);
	}

	// mixer
	mixer_mic = ms_factory_create_filter(msFactory, MS_AUDIO_MIXER_ID);
	ms_filter_call_method(mixer_mic, MS_FILTER_SET_SAMPLE_RATE, &config_sampling_rate);
	ms_filter_call_method(mixer_mic, MS_FILTER_SET_NCHANNELS, &config_nchannels);
	if (!BC_ASSERT_PTR_NOT_NULL(mixer_mic)) {
		aec_done = FALSE;
		goto end;
	}
	// record mic
	mic_rec = ms_factory_create_filter(msFactory, MS_FILE_REC_ID);
	ms_filter_call_method(mic_rec, MS_FILTER_SET_SAMPLE_RATE, &config_sampling_rate);
	ms_filter_call_method(mic_rec, MS_FILTER_SET_NCHANNELS, &config_nchannels);
	ms_filter_call_method_noarg(mic_rec, MS_FILE_REC_CLOSE);
	ms_filter_call_method(mic_rec, MS_FILE_REC_OPEN, config->mic_rec_file);
	if (!BC_ASSERT_PTR_NOT_NULL(mic_rec)) {
		aec_done = FALSE;
		goto end;
	}

	// AEC output record
	sound_rec = ms_factory_create_filter(msFactory, MS_FILE_REC_ID);
	ms_filter_call_method(sound_rec, MS_FILTER_SET_SAMPLE_RATE, &output_sampling_rate);
	ms_filter_call_method(sound_rec, MS_FILTER_SET_NCHANNELS, &output_nchannels);
	ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_CLOSE);
	ms_filter_call_method(sound_rec, MS_FILE_REC_OPEN, config->record_file);
	if (!BC_ASSERT_PTR_NOT_NULL(sound_rec)) {
		aec_done = FALSE;
		goto end;
	}

	// record reference output
	output_ref_rec = ms_factory_create_filter(msFactory, MS_FILE_REC_ID);
	ms_filter_call_method(output_ref_rec, MS_FILTER_SET_SAMPLE_RATE, &output_sampling_rate);
	ms_filter_call_method(output_ref_rec, MS_FILTER_SET_NCHANNELS, &config_nchannels);
	ms_filter_call_method_noarg(output_ref_rec, MS_FILE_REC_CLOSE);
	ms_filter_call_method(output_ref_rec, MS_FILE_REC_OPEN, config->output_ref_rec_file);
	if (!BC_ASSERT_PTR_NOT_NULL(output_ref_rec)) {
		aec_done = FALSE;
		goto end;
	}

	MSConnectionHelper h;
	// far end
	ms_connection_helper_start(&h);
	if (player_farend) {
		ms_connection_helper_link(&h, player_farend, -1, 0);
		if (resampler_farend) ms_connection_helper_link(&h, resampler_farend, 0, 0);
		ms_connection_helper_link(&h, aec, 0, 0);
	} else {
		ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
		ms_connection_helper_link(&h, aec, 0, 0);
	}
	// mic: near end
	ms_connection_helper_start(&h);
	if (player_nearend) {
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, player_nearend, -1, 0);
		if (resampler_nearend) ms_connection_helper_link(&h, resampler_nearend, 0, 0);
		ms_connection_helper_link(&h, mixer_mic, 0, 0);
	}
	// mic: echo
	if (player_echo) {
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, player_echo, -1, 0);
		if (resampler_echo) ms_connection_helper_link(&h, resampler_echo, 0, 0);
		if (!player_nearend) ms_connection_helper_link(&h, mixer_mic, 0, 0);
		else ms_connection_helper_link(&h, mixer_mic, 1, 0);
	}
	// mic: noise (only with near-end AND echo)
	if (player_noise) {
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, player_noise, -1, 0);
		if (resampler_noise) ms_connection_helper_link(&h, resampler_noise, 0, 0);
		ms_connection_helper_link(&h, mixer_mic, 2, 0);
	}
	ms_connection_helper_link(&h, aec, 1, 1);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, mixer_mic, -1, 1);
	ms_connection_helper_link(&h, mic_rec, 0, -1);
	// AEC
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, aec, -1, 0);
	if (resampler_output_ref) ms_connection_helper_link(&h, resampler_output_ref, 0, 0);
	ms_connection_helper_link(&h, output_ref_rec, 0, -1);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, aec, -1, 1);
	if (resampler_output) ms_connection_helper_link(&h, resampler_output, 0, 0);
	ms_connection_helper_link(&h, sound_rec, 0, -1);
	if (player_farend) ms_ticker_attach(ms_tester_ticker, player_farend);
	else ms_ticker_attach(ms_tester_ticker, player_nearend);

	int farend_duration_ms = 0;
	int echo_duration_ms = 0;
	int nearend_duration_ms = 0;
	if (player_farend) ms_filter_call_method(player_farend, MS_PLAYER_GET_DURATION, &farend_duration_ms);
	if (player_echo) ms_filter_call_method(player_echo, MS_PLAYER_GET_DURATION, &echo_duration_ms);
	if (player_nearend) ms_filter_call_method(player_nearend, MS_PLAYER_GET_DURATION, &nearend_duration_ms);
	int max_duration_ms = MAX(MAX(farend_duration_ms, echo_duration_ms), nearend_duration_ms);
	if (max_duration_ms == farend_duration_ms) {
		ms_filter_add_notify_callback(player_farend, fileplay_eof, &audio_done, TRUE);
	} else if (max_duration_ms == echo_duration_ms) {
		ms_filter_add_notify_callback(player_echo, fileplay_eof, &audio_done, TRUE);
	} else {
		ms_filter_add_notify_callback(player_nearend, fileplay_eof, &audio_done, TRUE);
	}

	// play audio
	if (mic_rec) ms_filter_call_method_noarg(mic_rec, MS_FILE_REC_START);
	if (sound_rec) ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_START);
	if (output_ref_rec) ms_filter_call_method_noarg(output_ref_rec, MS_FILE_REC_START);
	if ((player_farend) && (ms_filter_call_method_noarg(player_farend, MS_PLAYER_START) == -1)) {
		ms_error("Could not play far end. Playing filter failed to start");
	}
	// apply delay before starting echo and near-end
	int time_step_usec = 10000;
	struct timeval start_time;
	struct timeval now;
	float elapsed = 0.;
	if ((player_nearend) && (ms_filter_call_method_noarg(player_nearend, MS_PLAYER_START) == -1)) {
		ms_error("Could not play near end. Playing filter failed to start");
	}
	if ((player_echo) && (ms_filter_call_method_noarg(player_echo, MS_PLAYER_START) == -1)) {
		ms_error("Could not play echo. Playing filter failed to start");
	}
	if (player_noise) {
		int wait_ms = 0;
		ms_filter_call_method(player_noise, MS_FILE_PLAYER_LOOP, &wait_ms);
		if (ms_filter_call_method_noarg(player_noise, MS_PLAYER_START) == -1) {
			ms_error("Could not play noise. Playing filter failed to start");
		}
	}
	elapsed = 0.;
	bool_t farend_paused = FALSE;
	float pause_ms = 8000.;
	int seek_pos_ms = 12000;
	bctbx_gettimeofday(&start_time, NULL);
	while (audio_done != 1 && elapsed < config->audio_max_duration_ms) {
		bctbx_gettimeofday(&now, NULL);
		elapsed = ((now.tv_sec - start_time.tv_sec) * 1000.0f) + ((now.tv_usec - start_time.tv_usec) / 1000.0f);
		if ((config->reference_packets_lost) && (player_farend)) {
			// no audio send from farend and echo during few seconds
			if ((elapsed > pause_ms) && (elapsed < (float)seek_pos_ms) && !farend_paused) {
				ms_filter_call_method_noarg(player_farend, MS_PLAYER_PAUSE);
				ms_filter_call_method_noarg(player_echo, MS_PLAYER_PAUSE);
				farend_paused = TRUE;
				ms_message("Pause reference and echo");
			} else if (((elapsed <= pause_ms) || (elapsed >= (float)seek_pos_ms)) && farend_paused) {
				ms_filter_call_method(player_farend, MS_PLAYER_SEEK_MS, &seek_pos_ms);
				ms_filter_call_method(player_echo, MS_PLAYER_SEEK_MS, &seek_pos_ms);
				ms_filter_call_method_noarg(player_farend, MS_PLAYER_START);
				ms_filter_call_method_noarg(player_echo, MS_PLAYER_START);
				farend_paused = FALSE;
				ms_message("Restart reference and echo");
			}
		}
		ms_usleep(time_step_usec);
	}
	ms_filter_call_method(aec, MS_ECHO_CANCELLER_GET_DELAY, estimated_delay_ms);

	if (player_nearend) ms_filter_call_method_noarg(player_nearend, MS_FILE_PLAYER_CLOSE);
	if (player_farend) ms_filter_call_method_noarg(player_farend, MS_FILE_PLAYER_CLOSE);
	if (player_echo) ms_filter_call_method_noarg(player_echo, MS_FILE_PLAYER_CLOSE);
	if (player_noise) ms_filter_call_method_noarg(player_noise, MS_FILE_PLAYER_CLOSE);
	if (sound_rec) ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_CLOSE);
	if (mic_rec) ms_filter_call_method_noarg(mic_rec, MS_FILE_REC_CLOSE);
	if (output_ref_rec) ms_filter_call_method_noarg(output_ref_rec, MS_FILE_REC_CLOSE);

	if (player_farend) ms_ticker_detach(ms_tester_ticker, player_farend);
	else ms_ticker_detach(ms_tester_ticker, player_nearend);

	// far end
	ms_connection_helper_start(&h);
	if (player_farend) {
		ms_connection_helper_unlink(&h, player_farend, -1, 0);
		if (resampler_farend) ms_connection_helper_unlink(&h, resampler_farend, 0, 0);
		ms_connection_helper_unlink(&h, aec, 0, 0);
	} else {
		ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
		ms_connection_helper_unlink(&h, aec, 0, 0);
	}
	// mic: near end
	ms_connection_helper_start(&h);
	if (player_nearend) {
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, player_nearend, -1, 0);
		if (resampler_nearend) ms_connection_helper_unlink(&h, resampler_nearend, 0, 0);
		ms_connection_helper_unlink(&h, mixer_mic, 0, 0);
	}
	// mic: echo
	if (player_echo) {
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, player_echo, -1, 0);
		if (resampler_echo) ms_connection_helper_unlink(&h, resampler_echo, 0, 0);
		if (!player_nearend) ms_connection_helper_unlink(&h, mixer_mic, 0, 0);
		else ms_connection_helper_unlink(&h, mixer_mic, 1, 0);
	}
	// mic: noise (only with near-end AND echo)
	if (player_noise) {
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, player_noise, -1, 0);
		if (resampler_noise) ms_connection_helper_unlink(&h, resampler_noise, 0, 0);
		ms_connection_helper_unlink(&h, mixer_mic, 2, 0);
	}
	ms_connection_helper_unlink(&h, aec, 1, 1);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, mixer_mic, -1, 1);
	ms_connection_helper_unlink(&h, mic_rec, 0, -1);
	// AEC
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, aec, -1, 0);
	if (resampler_output_ref) ms_connection_helper_unlink(&h, resampler_output_ref, 0, 0);
	ms_connection_helper_unlink(&h, output_ref_rec, 0, -1);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, aec, -1, 1);
	if (resampler_output) ms_connection_helper_unlink(&h, resampler_output, 0, 0);
	ms_connection_helper_unlink(&h, sound_rec, 0, -1);

end:
	ms_factory_log_statistics(msFactory);
	if (player_nearend) ms_filter_destroy(player_nearend);
	if (player_farend) ms_filter_destroy(player_farend);
	if (player_echo) ms_filter_destroy(player_echo);
	if (player_noise) ms_filter_destroy(player_noise);
	if (resampler_nearend) ms_filter_destroy(resampler_nearend);
	if (resampler_farend) ms_filter_destroy(resampler_farend);
	if (resampler_echo) ms_filter_destroy(resampler_echo);
	if (resampler_noise) ms_filter_destroy(resampler_noise);
	if (resampler_output) ms_filter_destroy(resampler_output);
	if (resampler_output_ref) ms_filter_destroy(resampler_output_ref);
	if (mixer_mic) ms_filter_destroy(mixer_mic);
	if (aec) ms_filter_destroy(aec);
	if (sound_rec) ms_filter_destroy(sound_rec);
	if (mic_rec) ms_filter_destroy(mic_rec);
	if (output_ref_rec) ms_filter_destroy(output_ref_rec);
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();

	return aec_done;
}

static void near_end_single_talk(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_DOUBLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_nearend_single_talk_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	config.audio_max_duration_ms = 9000;
	bctbx_free(random_filename);
	int delay_ms = 0;
	int estimated_delay_ms = 0;

	if (aec_base(&config, delay_ms, &estimated_delay_ms)) {
		ms_message("estimated delay is %d, real delay is %d ms", estimated_delay_ms, delay_ms);
		BC_ASSERT_LOWER(estimated_delay_ms, 40, int, "%d");
		double similar = 0.;
		double energy = 0.;
		const int start_time_short_ms = 2000;
		const int stop_time_short_ms = 4000;
		const MSAudioDiffParams audio_cmp_params = audio_diff_param(delay_ms, start_time_short_ms, stop_time_short_ms);
		BC_ASSERT_EQUAL(ms_audio_compare_silence_and_speech(config.nearend_speech_file, config.record_file, &similar,
		                                                    &energy, &audio_cmp_params, NULL, NULL, start_time_short_ms,
		                                                    stop_time_short_ms, 0),
		                0, int, "%d");
		BC_ASSERT_GREATER(similar, 0.99, double, "%f");
		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
		BC_ASSERT_LOWER(energy, 1., double, "%f");
	}

	uninit_config(&config);
}

static void far_end_single_talk(void) {
	aec_test_config config;
	init_config(&config);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_DOUBLE_TALK);
	config.echo_file = bc_tester_res(ECHO_DOUBLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_farend_single_talk_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	config.audio_max_duration_ms = 9000;
	int delay_ms = 100;
	int estimated_delay_ms = 0;

	if (aec_base(&config, delay_ms, &estimated_delay_ms)) {
		ms_message("estimated delay is %d, real delay is %d ms", estimated_delay_ms, delay_ms);
		BC_ASSERT_LOWER(estimated_delay_ms, delay_ms + 5, int, "%d");
		BC_ASSERT_GREATER(estimated_delay_ms, delay_ms - 5, int, "%d");
		double energy = 0.;
		ms_audio_energy(config.record_file, &energy);
		ms_message("Energy=%f in file %s", energy, config.record_file);
		BC_ASSERT_LOWER(energy, 3., double, "%f");
	}

	uninit_config(&config);
}

static void
talk_base(aec_test_config *config, int delay_for_init_ms, int *estimated_delay_ms, const audio_analysis_param param) {
	if (aec_base(config, delay_for_init_ms, estimated_delay_ms)) {
		ms_message("estimated delay is %d, real delay is %d ms", *estimated_delay_ms, param.expected_final_delay_ms);
		BC_ASSERT_LOWER(*estimated_delay_ms, param.expected_final_delay_ms + 10, int, "%d");
		BC_ASSERT_GREATER(*estimated_delay_ms, param.expected_final_delay_ms - 20, int, "%d");

		// compare near-end with filtered output
		double nearend_similar = 0.;
		double energy = 0.;
		ms_message(
		    "Try to align filtered output on nearend by computing cross correlation with a maximal shift of %d percent",
		    param.audio_cmp_params.max_shift_percent);
		BC_ASSERT_EQUAL(ms_audio_compare_silence_and_speech(config->nearend_speech_file, config->record_file,
		                                                    &nearend_similar, &energy, &param.audio_cmp_params, NULL,
		                                                    NULL, param.start_time_short_ms, param.stop_time_short_ms,
		                                                    param.start_time_audio_diff_ms),
		                0, int, "%d");
		BC_ASSERT_GREATER(nearend_similar, param.threshold_similarity_in_speech, double, "%f");
		BC_ASSERT_LOWER(nearend_similar, 1.0, double, "%f");
		BC_ASSERT_LOWER(energy, param.threshold_energy_in_silence, double, "%f");

		if (EC_DUMP && !config->reference_packets_lost) {
			// compare reference output with farend
			// in this order because farend has more samples than reference output at the end of audio, and those must
			// be ignored for comparaison
			ms_message(
			    "Try to align farend on reference output by computing cross correlation with a maximal shift of %d "
			    "percent",
			    param.audio_cmp_params.max_shift_percent);
			double ref_similar = 0.;
			MSAudioDiffParams ref_cmp_params = audio_diff_param(0, 500, 12500);
			BC_ASSERT_EQUAL(ms_audio_diff(config->output_ref_rec_file, config->farend_speech_file, &ref_similar,
			                              &ref_cmp_params, NULL, NULL),
			                0, int, "%d");
			BC_ASSERT_GREATER(ref_similar, 0.998, double, "%f");
			BC_ASSERT_LOWER(ref_similar, 1.0, double, "%f");
		}
	}
}

static void double_talk(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_DOUBLE_TALK);
	config.echo_file = bc_tester_res(ECHO_DOUBLE_TALK);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_DOUBLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_double_talk_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 12500, 14500, 10000, 0.83, 1.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void double_talk_white_noise(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_DOUBLE_TALK);
	config.echo_file = bc_tester_res(ECHO_DOUBLE_TALK);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_DOUBLE_TALK);
	config.noise_file = bc_tester_res(WHITE_NOISE_FILE);
	char *random_filename = ms_tester_get_random_filename("aec_output_double_talk_white_noise_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 12500, 14500, 10000, 0.90, 3.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talk(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK_100MS);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_simple_talk_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 18500, 20500, 15500, 0.99, 1.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talk_600ms(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK_600MS);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_simple_talk_600ms_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 600;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 19000, 21000, 15500, 0.99, 1.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talk_white_noise(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK_100MS);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
	config.noise_file = bc_tester_res(WHITE_NOISE_FILE);
	char *random_filename = ms_tester_get_random_filename("aec_output_simple_talk_white_noise_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 18500, 20500, 15500, 0.98, 5.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talk_48000Hz(void) {
	aec_test_config config;
	init_config(&config);
	config.sampling_rate = 48000;
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK_100MS);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_simple_talk_48000Hz_resampled_16000Hz_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 18500, 20500, 15500, 0.98, 1.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talk_with_delay_change(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
	config.echo_file = bc_tester_res(ECHO_DELAY_CHANGE);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_delay_change_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	int expected_final_delay_ms = 150;
	const audio_analysis_param analysis_param =
	    set_audio_analysis_param(expected_final_delay_ms, 18500, 20500, 18500, 0.99, 1.5);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talk_without_initial_delay(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK_100MS);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_simple_talk_without_initial_delay_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 18500, 20500, 15500, 0.99, 1.);
	talk_base(&config, 0, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talk_with_missing_packets(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK_100MS);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_talk_with_missing_packets_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	config.reference_packets_lost = TRUE;
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 18500, 20500, 15500, 0.99, 1.);
	talk_base(&config, 0, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static test_t tests[] = {TEST_NO_TAG("Simple talk", simple_talk),
                         TEST_NO_TAG("Simple talk 600ms delay", simple_talk_600ms),
                         TEST_NO_TAG("Double talk", double_talk),
                         TEST_NO_TAG("Simple talk with white noise", simple_talk_white_noise),
                         TEST_NO_TAG("Double talk with white noise", double_talk_white_noise),
                         TEST_NO_TAG("Near end single talk", near_end_single_talk),
                         TEST_NO_TAG("Far end single talk", far_end_single_talk),
                         TEST_NO_TAG("Simple talk 48000 Hz", simple_talk_48000Hz),
                         TEST_NO_TAG("Simple talk with delay change", simple_talk_with_delay_change),
                         TEST_NO_TAG("Simple talk without initial delay", simple_talk_without_initial_delay),
                         TEST_NO_TAG("Simple talk with missing packets", simple_talk_with_missing_packets)};

test_suite_t aec3_test_suite = {
    "AEC3", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0};
