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
extern void libmswebrtcaec_init(MSFactory *factory);
#endif

#define EC_DUMP 0

// double talk
#define NEAR_END_SPEECH_DOUBLE_TALK "sounds/nearend_double_talk.wav"
#define FAR_END_SPEECH_DOUBLE_TALK "sounds/farend_double_talk.wav"
#define ECHO_DOUBLE_TALK "sounds/echo_double_talk.wav"
#define NEAR_END_SPEECH_SIMPLE_TALK "sounds/nearend_simple_talk.wav"
#define FAR_END_SPEECH_SIMPLE_TALK "sounds/farend_simple_talk.wav"
#define ECHO_SIMPLE_TALK "sounds/echo_simple_talk.wav"
#define ECHO_DELAY_CHANGE                                                                                              \
	"sounds/echo_delay_change.wav" // like ECHO_SIMPLE_TALK but here the echo is shifted of 50 ms from far-end around
	                               // 9s.
#define WHITE_NOISE_FILE "sounds/white_noise.wav"

typedef struct _aec_test_config {
	int sampling_rate;
	int nchannels;
	char *nearend_speech_file;
	char *farend_speech_file;
	char *echo_file;
	char *record_file;
	char *noise_file;
	char *mic_rec_file;
	bool_t set_delay_to_aec_filter;
} aec_test_config;

static void init_config(aec_test_config *config) {
	config->sampling_rate = 16000;
	config->nchannels = 1;
	config->nearend_speech_file = NULL;
	config->farend_speech_file = NULL;
	config->echo_file = NULL;
	config->record_file = NULL;
	config->noise_file = NULL;
	config->set_delay_to_aec_filter = TRUE;
	config->mic_rec_file = bc_tester_file("aec_input_mic.wav");
}

static void uninit_config(aec_test_config *config) {
	if (config->nearend_speech_file) free(config->nearend_speech_file);
	if (config->farend_speech_file) free(config->farend_speech_file);
	if (config->echo_file) free(config->echo_file);
	if (config->noise_file) free(config->noise_file);
	if (config->record_file) {
#if EC_DUMP != 1
		unlink(config->record_file);
#endif
		ms_free(config->record_file);
	}
	if (config->mic_rec_file) {
#if EC_DUMP != 1
		unlink(config->mic_rec_file);
#endif
		ms_free(config->mic_rec_file);
	}
	config->nearend_speech_file = NULL;
	config->farend_speech_file = NULL;
	config->echo_file = NULL;
	config->noise_file = NULL;
	config->record_file = NULL;
	config->mic_rec_file = NULL;
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

static bool_t aec_base(const aec_test_config *config, const int delay_ms, int *estimated_delay_ms) {

	if ((config->farend_speech_file && !config->echo_file) || (!config->farend_speech_file && config->echo_file)) {
		BC_FAIL("Far-end or echo file missing.");
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
	MSFilter *mixer_farend = NULL;
	MSFilter *mixer_silence = NULL;
	MSFilter *resampler_nearend = NULL;
	MSFilter *resampler_farend = NULL;
	MSFilter *resampler_echo = NULL;
	MSFilter *resampler_noise = NULL;
	MSFilter *resampler_output = NULL;
	unsigned int filter_mask =
	    FILTER_MASK_VOIDSINK | FILTER_MASK_FILEREC | FILTER_MASK_FILEPLAY | FILTER_MASK_VOIDSOURCE;
	int config_sampling_rate = config->sampling_rate;
	int config_nchannels = config->nchannels;
	const int expected_sampling_rate = config->sampling_rate;
	const int expected_nchannels = config->nchannels;
	player_data.end_of_file = FALSE;
	int sampling_rate = expected_sampling_rate;
	int nchannels = expected_nchannels;
	int output_sampling_rate = expected_sampling_rate;
	int output_nchannels = expected_nchannels;
	bool_t send_silence = TRUE;
	int audio_done = 0;
	float waiting_time_ms = 20000.;
	if ((!config->farend_speech_file) || (!config->nearend_speech_file)) {
		waiting_time_ms = 9000.;
	}

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
	// far-end speech and echo or void
	if (config->farend_speech_file) {
		if (!aec_test_create_player(&player_farend, &resampler_farend, config->farend_speech_file,
		                            config->sampling_rate, config->nchannels)) {
			aec_done = FALSE;
			goto end;
		}
		ms_filter_add_notify_callback(player_farend, player_cb, &player_data, TRUE);
		if (!aec_test_create_player(&player_echo, &resampler_echo, config->echo_file, config->sampling_rate,
		                            config->nchannels)) {
			aec_done = FALSE;
			goto end;
		}

		// mixer
		mixer_farend = ms_factory_create_filter(msFactory, MS_AUDIO_MIXER_ID);
		ms_filter_call_method(mixer_farend, MS_FILTER_SET_SAMPLE_RATE, &config_sampling_rate);
		ms_filter_call_method(mixer_farend, MS_FILTER_SET_NCHANNELS, &config_nchannels);
		if (!BC_ASSERT_PTR_NOT_NULL(mixer_farend)) {
			aec_done = FALSE;
			goto end;
		}
	} else {
		ms_filter_add_notify_callback(player_nearend, player_cb, &player_data, TRUE);
	}
	// noise
	if (config->noise_file != NULL) {
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
		ms_message("resample output for rate %d to get %d Hz, for comparison with reference file",
		           config->sampling_rate, output_sampling_rate);
	}

	// mixer
	if ((player_echo) || (player_noise && player_nearend)) {
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
	}

	// silence
	mixer_silence = ms_factory_create_filter(msFactory, MS_AUDIO_MIXER_ID);
	ms_filter_call_method(mixer_silence, MS_FILTER_SET_SAMPLE_RATE, &config_sampling_rate);
	ms_filter_call_method(mixer_silence, MS_FILTER_SET_NCHANNELS, &config_nchannels);
	if (!BC_ASSERT_PTR_NOT_NULL(mixer_silence)) {
		aec_done = FALSE;
		goto end;
	}
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_SAMPLE_RATE, &config_sampling_rate);
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_NCHANNELS, &config_nchannels);
	ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);

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

	// voidsink
	ms_filter_call_method(ms_tester_voidsink, MS_FILTER_SET_SAMPLE_RATE, &config_sampling_rate);
	ms_filter_call_method(ms_tester_voidsink, MS_FILTER_SET_NCHANNELS, &config_nchannels);

	if (config->set_delay_to_aec_filter) {
		ms_message("Setting echo canceller delay with value provided by user: %i ms", delay_ms);
		int delay_set = delay_ms;
		ms_filter_call_method(aec, MS_ECHO_CANCELLER_SET_DELAY, &delay_set);
	}

	MSConnectionHelper h;
	// far end
	ms_connection_helper_start(&h);
	if (player_farend) {
		ms_connection_helper_link(&h, player_farend, -1, 0);
		if (resampler_farend) ms_connection_helper_link(&h, resampler_farend, 0, 0);
		ms_connection_helper_link(&h, mixer_farend, 0, 0);
		ms_connection_helper_link(&h, aec, 0, 0);
		ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	}
	// near end
	ms_connection_helper_start(&h);
	if (player_nearend) {
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, player_nearend, -1, 0);
		if (resampler_nearend) ms_connection_helper_link(&h, resampler_nearend, 0, 0);
		if (mixer_mic) ms_connection_helper_link(&h, mixer_mic, 0, 0);
	}
	// echo
	if (player_echo) {
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, player_echo, -1, 0);
		if (resampler_echo) ms_connection_helper_link(&h, resampler_echo, 0, 0);
		if (mixer_mic) {
			if (!player_nearend && player_noise) ms_connection_helper_link(&h, mixer_mic, 0, 0);
			else ms_connection_helper_link(&h, mixer_mic, 1, 0);
		}
	}
	if (player_noise) {
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, player_noise, -1, 0);
		if (resampler_noise) ms_connection_helper_link(&h, resampler_noise, 0, 0);
		if (mixer_mic) {
			if (player_nearend && player_echo) ms_connection_helper_link(&h, mixer_mic, 2, 0);
			else ms_connection_helper_link(&h, mixer_mic, 1, 0);
		}
	}
	// from mic to aec
	ms_connection_helper_link(&h, aec, 1, 1);
	// records
	if (resampler_output) ms_connection_helper_link(&h, resampler_output, 0, 0);
	ms_connection_helper_link(&h, sound_rec, 0, -1);
	if (mixer_mic) {
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, mixer_mic, -1, 1);
		ms_connection_helper_link(&h, mic_rec, 0, -1);
	}
	// silence
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, mixer_silence, 0, 0);
	if (player_farend) ms_connection_helper_link(&h, mixer_farend, 1, 0);
	else {
		ms_connection_helper_link(&h, aec, 0, 0);
		ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
	}
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, mixer_silence, -1, 1);
	if (mixer_mic) {
		if (player_noise && player_nearend && player_echo) ms_connection_helper_link(&h, mixer_mic, 3, 0);
		else ms_connection_helper_link(&h, mixer_mic, 2, 0);
	}

	if (player_farend) ms_ticker_attach(ms_tester_ticker, player_farend);
	else ms_ticker_attach(ms_tester_ticker, player_nearend);

	int farend_duration_ms = 0;
	int echo_duration_ms = 0;
	int nearend_duration_ms = 0;
	if (player_farend) ms_filter_call_method(player_farend, MS_PLAYER_GET_DURATION, &farend_duration_ms);
	if (player_echo) ms_filter_call_method(player_echo, MS_PLAYER_GET_DURATION, &echo_duration_ms);
	if (player_nearend) ms_filter_call_method(player_nearend, MS_PLAYER_GET_DURATION, &nearend_duration_ms);
	int max_duration_ms = MAX(MAX(farend_duration_ms, echo_duration_ms + delay_ms), nearend_duration_ms + delay_ms);
	if (max_duration_ms == farend_duration_ms) {
		ms_filter_add_notify_callback(player_farend, fileplay_eof, &audio_done, TRUE);
	} else if (max_duration_ms == echo_duration_ms + delay_ms) {
		ms_filter_add_notify_callback(player_echo, fileplay_eof, &audio_done, TRUE);
	} else {
		ms_filter_add_notify_callback(player_nearend, fileplay_eof, &audio_done, TRUE);
	}

	// play audio
	if (mic_rec) ms_filter_call_method_noarg(mic_rec, MS_FILE_REC_START);
	if (sound_rec) ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_START);
	if ((player_farend) && (ms_filter_call_method_noarg(player_farend, MS_PLAYER_START) == -1)) {
		ms_error("Could not play far end. Playing filter failed to start");
	}
	// apply delay before starting echo and near-end
	int time_step_usec = 10000;
	int farend_cur_pos_ms = 0;
	if (player_noise) {
		int wait_ms = 0;
		ms_filter_call_method(player_noise, MS_FILE_PLAYER_LOOP, &wait_ms);
		if (ms_filter_call_method_noarg(player_noise, MS_PLAYER_START) == -1) {
			ms_error("Could not play noise. Playing filter failed to start");
		}
	}
	struct timeval start_time;
	struct timeval now;
	float elapsed = 0.;
	if (player_farend) {
		bctbx_gettimeofday(&start_time, NULL);
		ms_filter_call_method(player_farend, MS_PLAYER_GET_CURRENT_POSITION, &farend_cur_pos_ms);
		ms_message("play with delay %d ms", delay_ms);
		while (farend_cur_pos_ms < delay_ms && elapsed < waiting_time_ms) {
			bctbx_gettimeofday(&now, NULL);
			elapsed = ((now.tv_sec - start_time.tv_sec) * 1000.0f) + ((now.tv_usec - start_time.tv_usec) / 1000.0f);
			ms_message("elapsed is %f (ms?) - %f", elapsed, waiting_time_ms);
			ms_filter_call_method(player_farend, MS_PLAYER_GET_CURRENT_POSITION, &farend_cur_pos_ms);
			ms_usleep(time_step_usec);
		}
	}
	if ((player_nearend) && (ms_filter_call_method_noarg(player_nearend, MS_PLAYER_START) == -1)) {
		ms_error("Could not play near end. Playing filter failed to start");
	}
	if ((player_echo) && (ms_filter_call_method_noarg(player_echo, MS_PLAYER_START) == -1)) {
		ms_error("Could not play echo. Playing filter failed to start");
	}
	elapsed = 0.;
	bctbx_gettimeofday(&start_time, NULL);
	while (audio_done != 1 && elapsed < waiting_time_ms) {
		bctbx_gettimeofday(&now, NULL);
		elapsed = ((now.tv_sec - start_time.tv_sec) * 1000.0f) + ((now.tv_usec - start_time.tv_usec) / 1000.0f);
		ms_usleep(time_step_usec);
	}

	ms_filter_call_method(aec, MS_ECHO_CANCELLER_GET_DELAY, estimated_delay_ms);

	if (player_nearend) ms_filter_call_method_noarg(player_nearend, MS_FILE_PLAYER_CLOSE);
	if (player_farend) ms_filter_call_method_noarg(player_farend, MS_FILE_PLAYER_CLOSE);
	if (player_echo) ms_filter_call_method_noarg(player_echo, MS_FILE_PLAYER_CLOSE);
	if (player_noise) ms_filter_call_method_noarg(player_noise, MS_FILE_PLAYER_CLOSE);
	if (sound_rec) ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_CLOSE);
	if (mic_rec) ms_filter_call_method_noarg(mic_rec, MS_FILE_REC_CLOSE);

	if (player_farend) ms_ticker_detach(ms_tester_ticker, player_farend);
	else ms_ticker_detach(ms_tester_ticker, player_nearend);

	// far end
	ms_connection_helper_start(&h);
	if (player_farend) {
		ms_connection_helper_unlink(&h, player_farend, -1, 0);
		if (resampler_farend) ms_connection_helper_unlink(&h, resampler_farend, 0, 0);
		ms_connection_helper_unlink(&h, mixer_farend, 0, 0);
		ms_connection_helper_unlink(&h, aec, 0, 0);
		ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	}
	// near end
	ms_connection_helper_start(&h);
	if (player_nearend) {
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, player_nearend, -1, 0);
		if (resampler_nearend) ms_connection_helper_unlink(&h, resampler_nearend, 0, 0);
		if (mixer_mic) ms_connection_helper_unlink(&h, mixer_mic, 0, 0);
	}
	// echo
	if (player_echo) {
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, player_echo, -1, 0);
		if (resampler_echo) ms_connection_helper_unlink(&h, resampler_echo, 0, 0);
		if (mixer_mic) {
			if (!player_nearend && player_noise) ms_connection_helper_unlink(&h, mixer_mic, 0, 0);
			else ms_connection_helper_unlink(&h, mixer_mic, 1, 0);
		}
	}
	if (player_noise) {
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, player_noise, -1, 0);
		if (resampler_noise) ms_connection_helper_unlink(&h, resampler_noise, 0, 0);
		if (mixer_mic) {
			if (player_nearend && player_echo) ms_connection_helper_unlink(&h, mixer_mic, 2, 0);
			else ms_connection_helper_unlink(&h, mixer_mic, 1, 0);
		}
	}
	// from mic to aec
	ms_connection_helper_unlink(&h, aec, 1, 1);
	// records
	if (resampler_output) ms_connection_helper_unlink(&h, resampler_output, 0, 0);
	ms_connection_helper_unlink(&h, sound_rec, 0, -1);
	if (mixer_mic) {
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, mixer_mic, -1, 1);
		ms_connection_helper_unlink(&h, mic_rec, 0, -1);
	}
	// silence
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, mixer_silence, 0, 0);
	if (player_farend) ms_connection_helper_unlink(&h, mixer_farend, 1, 0);
	else {
		ms_connection_helper_unlink(&h, aec, 0, 0);
		ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
	}
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, mixer_silence, -1, 1);
	if (mixer_mic) {
		if (player_noise && player_nearend && player_echo) ms_connection_helper_unlink(&h, mixer_mic, 3, 0);
		else ms_connection_helper_unlink(&h, mixer_mic, 2, 0);
	}

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
	if (mixer_mic) ms_filter_destroy(mixer_mic);
	if (aec) ms_filter_destroy(aec);
	if (sound_rec) ms_filter_destroy(sound_rec);
	if (mic_rec) ms_filter_destroy(mic_rec);
	if (mixer_farend) ms_filter_destroy(mixer_farend);
	if (mixer_silence) ms_filter_destroy(mixer_silence);
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
	int delay_ms = 100;
	int estimated_delay_ms = 0;

	if (aec_base(&config, delay_ms, &estimated_delay_ms)) {
		ms_message("estimated delay is %d, real delay is %d ms", estimated_delay_ms, delay_ms);
		BC_ASSERT_LOWER(estimated_delay_ms - delay_ms, 40, int, "%d");
		BC_ASSERT_GREATER(estimated_delay_ms - delay_ms, 32, int, "%d");
		double energy = 0.;
		ms_audio_energy(config.record_file, &energy);
		ms_message("Energy=%f in file %s", energy, config.record_file);
		BC_ASSERT_LOWER(energy, 3., double, "%f");
	}

	uninit_config(&config);
}

static void
talk_base(aec_test_config *config, int delay_ms, int *estimated_delay_ms, const audio_analysis_param param) {
	if (aec_base(config, delay_ms, estimated_delay_ms)) {
		ms_message("estimated delay is %d, real delay is %d ms", *estimated_delay_ms, param.expected_final_delay_ms);
		int delay_diff = *estimated_delay_ms - param.expected_final_delay_ms;
		BC_ASSERT_LOWER(delay_diff, 50, int, "%d");
		BC_ASSERT_GREATER(delay_diff, 20, int, "%d");
		double nearend_similar = 0.;
		double energy = 0.;
		ms_message("Try to align output on nearend by computing cross correlation with a maximal shift of %d percent",
		           param.audio_cmp_params.max_shift_percent);
		BC_ASSERT_EQUAL(ms_audio_compare_silence_and_speech(config->nearend_speech_file, config->record_file,
		                                                    &nearend_similar, &energy, &param.audio_cmp_params, NULL,
		                                                    NULL, param.start_time_short_ms, param.stop_time_short_ms,
		                                                    param.start_time_audio_diff_ms),
		                0, int, "%d");
		BC_ASSERT_GREATER(nearend_similar, param.threshold_similarity_in_speech, double, "%f");
		BC_ASSERT_LOWER(nearend_similar, 1.0, double, "%f");
		BC_ASSERT_LOWER(energy, param.threshold_energy_in_silence, double, "%f");
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
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 11500, 13500, 9500, 0.83, 1.);
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
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 11500, 13500, 9500, 0.90, 3.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talk(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_simple_talk_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 12500, 14500, 11000, 0.99, 1.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talk_white_noise(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
	config.noise_file = bc_tester_res(WHITE_NOISE_FILE);
	char *random_filename = ms_tester_get_random_filename("aec_output_simple_talk_white_noise_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 12500, 14500, 11000, 0.98, 4.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talk_48000Hz(void) {
	aec_test_config config;
	init_config(&config);
	config.sampling_rate = 48000;
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
	char *random_filename = ms_tester_get_random_filename("aec_output_simple_talk_48000Hz_resampled_16000Hz_", ".wav");
	config.record_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	const audio_analysis_param analysis_param = set_audio_analysis_param(delay_ms, 12500, 14500, 11000, 0.98, 1.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static void simple_talks_with_several_delays(void) {
	int delays[5] = {0, 40, 80, 200, 470};
	int estimated_delay_ms = 0;
	int length;
	char *baseName = NULL;
	for (int i = 0; i < 5; i++) {
		aec_test_config config;
		init_config(&config);
		config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK);
		config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK);
		config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK);
		config.set_delay_to_aec_filter = FALSE;

		int delay_ms = delays[i];
		if (baseName) {
			free(baseName);
		}
		if (config.mic_rec_file) {
			unlink(config.mic_rec_file);
		}
		length = snprintf(NULL, 0, "aec_output_delay_%dms_", delay_ms);
		baseName = malloc(length + 1);
		snprintf(baseName, length + 1, "aec_output_delay_%dms_", delay_ms);
		char *random_filename = ms_tester_get_random_filename(baseName, ".wav");
		config.record_file = bc_tester_file(random_filename);
		bctbx_free(random_filename);
		double energy_threshold = 1.;
		if (delays[i] > 400) energy_threshold = 3.3;
		const audio_analysis_param analysis_param =
		    set_audio_analysis_param(delay_ms, 12500, 14500, 11000, 0.99, energy_threshold);
		talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
		uninit_config(&config);
	}
	free(baseName);
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
	    set_audio_analysis_param(expected_final_delay_ms, 12500, 14500, 11000, 0.99, 1.);
	talk_base(&config, delay_ms, &estimated_delay_ms, analysis_param);
	uninit_config(&config);
}

static test_t tests[] = {
    TEST_NO_TAG("Simple talk", simple_talk),
    TEST_NO_TAG("Double talk", double_talk),
    TEST_NO_TAG("Simple talk with white noise", simple_talk_white_noise),
    TEST_NO_TAG("Double talk with white noise", double_talk_white_noise),
    TEST_NO_TAG("Near end single talk", near_end_single_talk),
    TEST_NO_TAG("Far end single talk", far_end_single_talk),
    TEST_NO_TAG("Simple talk 48000 Hz", simple_talk_48000Hz),
    TEST_NO_TAG("Simple talk with delay change", simple_talk_with_delay_change),
    TEST_NO_TAG("Simple talks with several delays", simple_talks_with_several_delays),
};

test_suite_t aec3_test_suite = {
    "AEC3", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0};
