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

#define NEAR_END_SPEECH_FILE_11 "sounds/nearend_speech_fileid_11.wav"
#define FAR_END_SPEECH_FILE_11 "sounds/farend_speech_fileid_11.wav"
#define ECHO_FILE_11 "sounds/echo_fileid_11.wav"
#define NEAR_END_SPEECH_SIMPLE_TALK_FILE_11 "sounds/nearend_speech_simple_talk_fileid_11.wav"
#define FAR_END_SPEECH_SIMPLE_TALK_FILE_11 "sounds/farend_speech_simple_talk_fileid_11.wav"
#define ECHO_SIMPLE_TALK_FILE_11 "sounds/echo_simple_talk_fileid_11.wav"
#define WHITE_NOISE_FILE "sounds/white_noise.wav"

#define TIME_OFFSET 1000

typedef struct _aec_test_config {
	int sampling_rate;
	int nchannels;
	int time_compensation_ms; // to synchronize the near-end mic and echo files, before applying the delay
	char *nearend_speech_file;
	char *farend_speech_file;
	char *echo_file;
	char *record_file;
	char *noise_file;
	char *mic_rec_file;
} aec_test_config;

static void init_config(aec_test_config *config) {
	config->sampling_rate = 16000;
	config->nchannels = 1;
	config->time_compensation_ms = 0;
	config->nearend_speech_file = NULL;
	config->farend_speech_file = NULL;
	config->echo_file = NULL;
	config->record_file = NULL;
	config->noise_file = NULL;
	config->mic_rec_file = NULL;
}

static void uninit_config(aec_test_config *config) {
	if (config->nearend_speech_file) free(config->nearend_speech_file);
	if (config->farend_speech_file) free(config->farend_speech_file);
	if (config->echo_file) free(config->echo_file);
	if (config->noise_file) free(config->noise_file);
	if (config->record_file) {
		unlink(config->record_file);
		free(config->record_file);
	}
	if (config->mic_rec_file) {
		unlink(config->mic_rec_file);
		// free(config->mic_rec_file);
	}
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

static bool_t aec_base(aec_test_config *config, int delay_ms, int delay_drift_ms, int *estimated_delay_ms) {

	if ((config->farend_speech_file && !config->echo_file) || (!config->farend_speech_file && config->echo_file)) {
		BC_FAIL("Far-end or echo file missing.");
		return FALSE;
	}

	bool_t aec_done = TRUE;
	bool_t rec_output = TRUE;

	player_callback_data player_data;
	MSFilter *player_nearend = NULL;
	MSFilter *player_farend = NULL;
	MSFilter *player_echo = NULL;
	MSFilter *player_noise = NULL;
	MSFilter *aec = NULL;
	MSFilter *sound_rec = NULL;
	MSFilter *mixer_mic = NULL;
	// MSFilter *tmp_mixer = NULL;
	// MSFilter *tmp_rec = NULL;
	MSFilter *mic_rec = NULL;
	MSFilter *resampler_nearend = NULL;
	MSFilter *resampler_farend = NULL;
	MSFilter *resampler_echo = NULL;
	MSFilter *resampler_noise = NULL;
	MSFilter *resampler_output = NULL;
	unsigned int filter_mask = FILTER_MASK_SOUNDWRITE | FILTER_MASK_VOIDSINK | FILTER_MASK_FILEREC |
	                           FILTER_MASK_FILEPLAY | FILTER_MASK_VOIDSOURCE;

	const int expected_sampling_rate = config->sampling_rate;
	const int expected_nchannels = config->nchannels;
	player_data.end_of_file = FALSE;

	int sampling_rate = expected_sampling_rate;
	int nchannels = expected_nchannels;
	int output_sampling_rate = expected_sampling_rate;
	int output_nchannels = expected_nchannels;
	bool_t enable_delay_estimation = TRUE;
	bool_t send_silence = TRUE;
	int nearend_done = 0;
	int farend_done = 0;
	int echo_done = 0;
	int elapsed = 0;
	int waiting_time_ms = 22000;

	ms_factory_reset_statistics(msFactory);
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);

	// AEC filter
	char *echo_canceller_filtername = ms_strdup("MSWebRTCAEC");
	MSFilterDesc *ec_desc = ms_factory_lookup_filter_by_name(msFactory, echo_canceller_filtername);
	ms_free(echo_canceller_filtername);
	bool_t bypass_mode = FALSE;
	aec = ms_factory_create_filter_from_desc(msFactory, ec_desc);
	ms_filter_call_method(aec, MS_ECHO_CANCELLER_SET_STATE_STRING, "1048576");
	ms_filter_call_method(aec, MS_ECHO_CANCELLER_SET_BYPASS_MODE, &bypass_mode);
	ms_filter_call_method(aec, MS_ECHO_CANCELLER_SET_DELAY_ESTIMATION, &enable_delay_estimation);
	if (!BC_ASSERT_PTR_NOT_NULL(aec)) goto end;
	// acoustic echo canceller
	ms_filter_call_method(aec, MS_FILTER_SET_SAMPLE_RATE, &config->sampling_rate);
	ms_filter_call_method(aec, MS_FILTER_GET_SAMPLE_RATE, &sampling_rate);
	ms_filter_call_method(aec, MS_FILTER_SET_NCHANNELS, &config->nchannels);
	ms_filter_call_method(aec, MS_FILTER_GET_NCHANNELS, &nchannels);
	if ((sampling_rate != expected_sampling_rate) || (nchannels != expected_nchannels)) {
		BC_FAIL("AEC filter does not have the expected sampling rate and/or channel number");
		aec_done = FALSE;
		goto end;
	}

	// players
	// near-end speech
	if (config->nearend_speech_file) {
		if (!aec_test_create_player(&player_nearend, &resampler_nearend, config->nearend_speech_file,
		                            config->sampling_rate, config->nchannels)) {
			aec_done = FALSE;
			goto end;
		}
		ms_filter_add_notify_callback(player_nearend, fileplay_eof, &nearend_done, TRUE);
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
		ms_filter_add_notify_callback(player_farend, fileplay_eof, &farend_done, TRUE);
		ms_filter_add_notify_callback(player_farend, player_cb, &player_data, TRUE);
		if (!aec_test_create_player(&player_echo, &resampler_echo, config->echo_file, config->sampling_rate,
		                            config->nchannels)) {
			aec_done = FALSE;
			goto end;
		}
		ms_filter_add_notify_callback(player_echo, fileplay_eof, &echo_done, TRUE);
	} else {
		ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_SAMPLE_RATE, &config->sampling_rate);
		ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_NCHANNELS, &config->nchannels);
		ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);
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
		ms_filter_call_method(resampler_output, MS_FILTER_SET_SAMPLE_RATE, &config->sampling_rate);
		ms_filter_call_method(resampler_output, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &output_sampling_rate);
		ms_filter_call_method(resampler_output, MS_FILTER_SET_NCHANNELS, &config->nchannels);
		ms_filter_call_method(resampler_output, MS_FILTER_SET_OUTPUT_NCHANNELS, &output_nchannels);
		ms_message("resample output for rate %d to get %d Hz, for comparison with reference file",
		           config->sampling_rate, output_sampling_rate);
	}

	// mixer
	if ((player_echo && player_nearend) || (player_noise && player_echo) || (player_noise && player_nearend)) {
		mixer_mic = ms_factory_create_filter(msFactory, MS_AUDIO_MIXER_ID);
		ms_filter_call_method(mixer_mic, MS_FILTER_SET_SAMPLE_RATE, &config->sampling_rate);
		ms_filter_call_method(mixer_mic, MS_FILTER_SET_NCHANNELS, &config->nchannels);
		if (!BC_ASSERT_PTR_NOT_NULL(mixer_mic)) {
			aec_done = FALSE;
			goto end;
		}
		// record mic
		mic_rec = ms_factory_create_filter(msFactory, MS_FILE_REC_ID);
		ms_filter_call_method(mic_rec, MS_FILTER_SET_SAMPLE_RATE, &config->sampling_rate);
		ms_filter_call_method(mic_rec, MS_FILTER_SET_NCHANNELS, &config->nchannels);
		ms_filter_call_method_noarg(mic_rec, MS_FILE_REC_CLOSE);
		config->mic_rec_file = "aec_input_mic.wav";
		unlink(config->mic_rec_file);
		ms_filter_call_method(mic_rec, MS_FILE_REC_OPEN, config->mic_rec_file);
		if (!BC_ASSERT_PTR_NOT_NULL(mic_rec)) {
			aec_done = FALSE;
			goto end;
		}
	}

	// // mp mixer and rec
	// tmp_mixer = ms_factory_create_filter(msFactory, MS_AUDIO_MIXER_ID);
	// ms_filter_call_method(tmp_mixer, MS_FILTER_SET_SAMPLE_RATE, &config->sampling_rate);
	// ms_filter_call_method(tmp_mixer, MS_FILTER_SET_NCHANNELS, &config->nchannels);
	// if (!BC_ASSERT_PTR_NOT_NULL(tmp_mixer)) {
	// 	aec_done = FALSE;
	// 	goto end;
	// }
	// tmp_rec = ms_factory_create_filter(msFactory, MS_FILE_REC_ID);
	// ms_filter_call_method(tmp_rec, MS_FILTER_SET_SAMPLE_RATE, &config->sampling_rate);
	// ms_filter_call_method(tmp_rec, MS_FILTER_SET_NCHANNELS, &config->nchannels);
	// ms_filter_call_method_noarg(tmp_rec, MS_FILE_REC_CLOSE);
	// char *tmp_rec_file = "mix_input_audio.wav";
	// unlink(tmp_rec_file); // FIXME FHA erase previous file
	// ms_filter_call_method(tmp_rec, MS_FILE_REC_OPEN, tmp_rec_file);
	// ms_filter_call_method_noarg(tmp_rec, MS_FILE_REC_START);
	// if (!BC_ASSERT_PTR_NOT_NULL(tmp_rec)) {
	// 	aec_done = FALSE;
	// 	goto end;
	// }

	// ajouter test sur durée + délai

	// à ajouter ?
	// if (stream->ec) {
	// 	if (!stream->is_ec_delay_set && io->input.soundcard) {
	// 		int delay_ms = ms_snd_card_get_minimal_latency(io->input.soundcard);
	// 		ms_message("Setting echo canceller delay with value provided by soundcard: %i ms", delay_ms);
	// 		ms_filter_call_method(stream->ec, MS_ECHO_CANCELLER_SET_DELAY, &delay_ms);
	// 	} else {
	// 		ms_message("Setting echo canceller delay with value configured by application.");
	// 	}
	// 	ms_filter_call_method(stream->ec, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	// }

	// to listen
	if (!rec_output) {
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_SAMPLE_RATE, &output_sampling_rate);
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_SAMPLE_RATE, &sampling_rate);
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &output_nchannels);
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_NCHANNELS, &nchannels);
		if ((sampling_rate != output_sampling_rate) || (nchannels != output_nchannels)) {
			BC_FAIL("Soundwrite filter does not have the expected sampling rate and/or channel number");
			aec_done = FALSE;
			goto end;
		}
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

	// voidsink
	ms_filter_call_method(ms_tester_voidsink, MS_FILTER_SET_SAMPLE_RATE, &config->sampling_rate);
	ms_filter_call_method(ms_tester_voidsink, MS_FILTER_SET_NCHANNELS, &config->nchannels);

	if (player_farend) {
		// synchronize audio when far-end and echo audio signals are slightly shifted
		if (config->time_compensation_ms > 0)
			ms_filter_call_method(player_nearend, MS_PLAYER_SEEK_MS, &config->time_compensation_ms);
		else if (config->time_compensation_ms < 0)
			ms_filter_call_method(player_farend, MS_PLAYER_SEEK_MS, &config->time_compensation_ms);
	}

	// start far-end file sooner to simulate echo path delay
	if (player_farend) ms_filter_call_method(player_farend, MS_PLAYER_SEEK_MS, &delay_ms);

	// FIXME FHA comment here to test without delay given by user
	// TODO FHA: not for AEC3?
	ms_message("Setting echo canceller delay with value provided by user: %i ms", delay_ms);
	ms_filter_call_method(aec, MS_ECHO_CANCELLER_SET_DELAY, &delay_ms);

	MSConnectionHelper h;
	// far end
	ms_connection_helper_start(&h);
	if (player_farend) {
		ms_connection_helper_link(&h, player_farend, -1, 0);
		if (resampler_farend) ms_connection_helper_link(&h, resampler_farend, 0, 0);
	} else {
		ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	}
	// ms_connection_helper_link(&h, tmp_mixer, 0, 0);
	// ms_connection_helper_link(&h, tmp_rec, 0, -1);
	ms_connection_helper_link(&h, aec, 0, 0);
	ms_connection_helper_link(&h, ms_tester_voidsink, 0, -1);
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
	// ms_connection_helper_link(&h, tmp_mixer, 1, 1);
	ms_connection_helper_link(&h, aec, 1, 1);
	// records
	if (resampler_output) ms_connection_helper_link(&h, resampler_output, 0, 0);
	if (rec_output) ms_connection_helper_link(&h, sound_rec, 0, -1);
	else ms_connection_helper_link(&h, ms_tester_soundwrite, 0, -1);
	if (mixer_mic) {
		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, mixer_mic, -1, 1);
		ms_connection_helper_link(&h, mic_rec, 0, -1);
	}

	// in case of delay drift, the audio are recorded on the second part
	if (delay_drift_ms == 0) {
		if (mic_rec) ms_filter_call_method_noarg(mic_rec, MS_FILE_REC_START);
		if (sound_rec) ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_START);
	}

	if (player_farend) ms_ticker_attach(ms_tester_ticker, player_farend);
	else ms_ticker_attach(ms_tester_ticker, player_nearend);

	if ((player_nearend) && (ms_filter_call_method_noarg(player_nearend, MS_PLAYER_START) == -1)) {
		ms_error("Could not play near end. Playing filter failed to start");
	}
	if ((player_farend) && (ms_filter_call_method_noarg(player_farend, MS_PLAYER_START) == -1)) {
		ms_error("Could not play far end. Playing filter failed to start");
	}
	if ((player_echo) && (ms_filter_call_method_noarg(player_echo, MS_PLAYER_START) == -1)) {
		ms_error("Could not play echo. Playing filter failed to start");
	}
	if ((player_noise) && (ms_filter_call_method_noarg(player_noise, MS_PLAYER_START) == -1)) {
		ms_error("Could not play noise. Playing filter failed to start");
	}

	int time_step_usec = 10000;
	if (delay_drift_ms > 0) {
		// In case of delay drift the files are played twice.
		int wait_ms = 0;
		if (player_farend) ms_filter_call_method(player_farend, MS_FILE_PLAYER_LOOP, &wait_ms);
		if (player_nearend) ms_filter_call_method(player_nearend, MS_FILE_PLAYER_LOOP, &wait_ms);
		if (player_noise) ms_filter_call_method(player_noise, MS_FILE_PLAYER_LOOP, &wait_ms);
		if (player_echo) ms_filter_call_method(player_echo, MS_FILE_PLAYER_LOOP, &wait_ms);
		while (elapsed * 10 < waiting_time_ms && echo_done != 1) {
			elapsed++;
			ms_usleep(time_step_usec);
		}

		// The audio in mic input are played again with an increased delay.
		// This delay drift is simulated by a pause in the echo signal (and near-end) whereas the far-end signal
		// continues
		ms_filter_call_method_noarg(player_echo, MS_PLAYER_PAUSE);
		ms_filter_call_method_noarg(player_nearend, MS_PLAYER_PAUSE);
		ms_usleep(delay_drift_ms * 1000);
		ms_filter_call_method_noarg(player_echo, MS_PLAYER_START);
		ms_filter_call_method_noarg(player_nearend, MS_PLAYER_START);
		nearend_done -= 1;
		if (mic_rec) ms_filter_call_method_noarg(mic_rec, MS_FILE_REC_START);
		if (sound_rec) ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_START);
		elapsed += (int)(delay_drift_ms / 10);
		while (elapsed * 10 < waiting_time_ms && nearend_done != 1) {
			elapsed++;
			ms_usleep(time_step_usec);
		}

	} else {
		while (farend_done != 1 && nearend_done != 1 && echo_done != 1 && elapsed * 10 < waiting_time_ms) {
			elapsed++;
			ms_usleep(10000);
		}
	}

	ms_filter_call_method(aec, MS_ECHO_CANCELLER_GET_ESTIMATED_DELAY, estimated_delay_ms);

	ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_CLOSE);
	// ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_CLOSE);
	ms_filter_call_method_noarg(player_nearend, MS_FILE_PLAYER_CLOSE);
	if (player_farend) ms_filter_call_method_noarg(player_farend, MS_FILE_PLAYER_CLOSE);
	if (player_echo) ms_filter_call_method_noarg(player_echo, MS_FILE_PLAYER_CLOSE);
	if (player_noise != NULL) ms_filter_call_method_noarg(player_noise, MS_FILE_PLAYER_CLOSE);
	if (mic_rec != NULL) ms_filter_call_method_noarg(mic_rec, MS_FILE_REC_CLOSE);

	if (player_farend) ms_ticker_detach(ms_tester_ticker, player_farend);
	else ms_ticker_detach(ms_tester_ticker, player_nearend);

	// far end
	ms_connection_helper_start(&h);
	if (player_farend) {
		ms_connection_helper_unlink(&h, player_farend, -1, 0);
		if (resampler_farend) ms_connection_helper_unlink(&h, resampler_farend, 0, 0);
	} else {
		ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	}
	// ms_connection_helper_unlink(&h, tmp_mixer, 0, 0);
	// ms_connection_helper_unlink(&h, tmp_rec, 0, -1);
	ms_connection_helper_unlink(&h, aec, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_voidsink, 0, -1);
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
	// ms_connection_helper_unlink(&h, tmp_mixer, 1, 1);
	ms_connection_helper_unlink(&h, aec, 1, 1);
	// records
	if (resampler_output) ms_connection_helper_unlink(&h, resampler_output, 0, 0);
	if (rec_output) ms_connection_helper_unlink(&h, sound_rec, 0, -1);
	else ms_connection_helper_unlink(&h, ms_tester_soundwrite, 0, -1);
	if (mixer_mic) {
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, mixer_mic, -1, 1);
		ms_connection_helper_unlink(&h, mic_rec, 0, -1);
	}
	ms_connection_helper_start(&h);
	// ms_connection_helper_unlink(&h, tmp_mixer, -1, 1);
	// ms_connection_helper_unlink(&h, tmp_rec, 0, -1);

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
	// if (tmp_mixer) ms_filter_destroy(tmp_mixer);
	// if (tmp_rec) ms_filter_destroy(tmp_rec);
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();

	return aec_done;
}

// static void all_correlations(
//     char *farend_speech_file, char *echo_file, char *nearend_speech_file, char *nearend_mic_file, char *record_file)
//     {

// 	// measure similarity with near-end speech
// 	const MSAudioDiffParams audio_cmp_params = {10, 200};
// 	double similar = 0.0;

// 	if (record_file == NULL) {
// 		printf("no similiarity measurements\n");
// 		return;
// 	}

// 	ms_message("=== check far end ===");

// 	if (farend_speech_file != NULL) {

// 		ms_message("farend_speech_file %s", farend_speech_file);

// 		BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(farend_speech_file, echo_file, &similar, &audio_cmp_params, NULL,
// 		                                              NULL, TIME_OFFSET),
// 		                0, int, "%d");
// 		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
// 		ms_message("farend_speech_file - echo_file: similarity = %f", similar);

// 		BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(farend_speech_file, nearend_mic_file, &similar, &audio_cmp_params,
// 		                                              NULL, NULL, TIME_OFFSET),
// 		                0, int, "%d");
// 		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
// 		ms_message("farend_speech_file - nearend_mic_file: similarity = %f", similar);

// 		BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(farend_speech_file, record_file, &similar, &audio_cmp_params,
// 		                                              NULL, NULL, TIME_OFFSET),
// 		                0, int, "%d");
// 		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
// 		ms_message("farend_speech_file - record_file: similarity = %f", similar);
// 	}

// 	ms_message("=== check echo ===");

// 	if (echo_file != NULL) {

// 		ms_message("echo_file %s", echo_file);

// 		BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(echo_file, nearend_mic_file, &similar, &audio_cmp_params, NULL,
// 		                                              NULL, TIME_OFFSET),
// 		                0, int, "%d");
// 		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
// 		ms_message("echo_file - nearend_mic_file: similarity = %f", similar);

// 		BC_ASSERT_EQUAL(
// 		    ms_audio_diff_from_given_time(echo_file, record_file, &similar, &audio_cmp_params, NULL, NULL, TIME_OFFSET),
// 		    0, int, "%d");
// 		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
// 		ms_message("echo_file - record_file: similarity = %f", similar);
// 	}

// 	ms_message("=== check near end speech ===");

// 	if (nearend_speech_file != NULL) {

// 		ms_message("nearend_speech_file %s", nearend_speech_file);

// 		BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(nearend_speech_file, nearend_mic_file, &similar,
// 		                                              &audio_cmp_params, NULL, NULL, TIME_OFFSET),
// 		                0, int, "%d");
// 		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
// 		ms_message("nearend_speech_file - nearend_mic_file: similarity = %f", similar);

// 		BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(nearend_speech_file, record_file, &similar, &audio_cmp_params,
// 		                                              NULL, NULL, TIME_OFFSET),
// 		                0, int, "%d");
// 		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
// 		ms_message("nearend_speech_file - record_file: similarity = %f", similar);
// 	}

// 	ms_message("=== check mic ===");

// 	ms_message("nearend_mic_file %s", nearend_mic_file);

// 	BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(nearend_mic_file, record_file, &similar, &audio_cmp_params, NULL,
// 	                                              NULL, TIME_OFFSET),
// 	                0, int, "%d");
// 	BC_ASSERT_LOWER(similar, 1.0, double, "%f");
// 	ms_message("nearend_mic_file - record_file: similarity = %f", similar);

// 	ms_message("record_file %s", record_file);

// 	return;
// }

static void near_end_single_talk_base(bool_t white_noise) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_FILE_11);
	if (white_noise) {
		config.noise_file = bc_tester_res(WHITE_NOISE_FILE);
		config.record_file = ms_tester_get_random_filename("aec_output_nearend_single_talk_white_noise_", ".wav");
	} else {
		config.record_file = ms_tester_get_random_filename("aec_output_nearend_single_talk_", ".wav");
	}
	int delay_ms = 0;
	int estimated_delay_ms = 0;

	if (aec_base(&config, delay_ms, 0, &estimated_delay_ms)) {
		double threshold = 0.99;
		if (white_noise) threshold = 0.98;
		double similar = 0.0;
		const MSAudioDiffParams audio_cmp_params = {0, 0};

		ms_message("compare file %s", config.nearend_speech_file);
		ms_message("with    file %s", config.record_file);
		BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(config.nearend_speech_file, config.record_file, &similar,
		                                              &audio_cmp_params, NULL, NULL, TIME_OFFSET),
		                0, int, "%d");
		BC_ASSERT_GREATER(similar, threshold, double, "%f");
		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
	}

	uninit_config(&config);
}

static void far_end_single_talk_base(bool_t white_noise) {
	aec_test_config config;
	init_config(&config);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_FILE_11);
	config.echo_file = bc_tester_res(ECHO_FILE_11);
	if (white_noise) {
		config.noise_file = bc_tester_res(WHITE_NOISE_FILE);
		config.record_file = ms_tester_get_random_filename("aec_output_farend_single_talk_white_noise_", ".wav");
	} else {
		config.record_file = ms_tester_get_random_filename("aec_output_farend_single_talk_", ".wav");
	}
	int delay_ms = 0;
	int estimated_delay_ms = 0;

	if (aec_base(&config, delay_ms, 0, &estimated_delay_ms)) {
		double energy = 0.;
		ms_audio_energy(config.record_file, &energy);
		ms_message("Energy=%f in file %s", energy, config.record_file);
		BC_ASSERT_LOWER(energy, 4., double, "%f");

		// TODO FHA: ERLE must be high
	}

	uninit_config(&config);
}

static void far_end_single_talk(void) {
	far_end_single_talk_base(FALSE);
}

static void far_end_single_talk_white_noise(void) {
	far_end_single_talk_base(TRUE);
}

static void near_end_single_talk(void) {
	near_end_single_talk_base(FALSE);
}

static void near_end_single_talk_white_noise(void) {
	near_end_single_talk_base(TRUE);
}

static void talk_base(aec_test_config *config,
                      const int delay_ms,
                      const int delay_drift_ms,
                      int *estimated_delay_ms,
                      const double threshold,
                      const MSAudioDiffParams audio_cmp_params) {

	if (aec_base(config, delay_ms, delay_drift_ms, estimated_delay_ms)) {
		ms_message("compare file %s", config->nearend_speech_file);
		ms_message("with    file %s", config->record_file);
		double nearend_similar = 0.0;
		int time_offset = TIME_OFFSET;
		if (delay_ms > 500) time_offset = 1500;
		BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(config->nearend_speech_file, config->record_file,
		                                              &nearend_similar, &audio_cmp_params, NULL, NULL, time_offset),
		                0, int, "%d");
		BC_ASSERT_GREATER(nearend_similar, threshold, double, "%f");
		BC_ASSERT_LOWER(nearend_similar, 1.0, double, "%f");
	}
}

static void double_talk(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_FILE_11);
	config.echo_file = bc_tester_res(ECHO_FILE_11);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_FILE_11);
	config.record_file = ms_tester_get_random_filename("aec_output_double_talk_", ".wav");
	config.time_compensation_ms = 23;
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	double threshold = 0.90;
	const MSAudioDiffParams audio_cmp_params = {1, 200};
	talk_base(&config, delay_ms, 0, &estimated_delay_ms, threshold, audio_cmp_params);
	uninit_config(&config);
}

static void double_talk_white_noise(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_FILE_11);
	config.echo_file = bc_tester_res(ECHO_FILE_11);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_FILE_11);
	config.noise_file = bc_tester_res(WHITE_NOISE_FILE);
	config.record_file = ms_tester_get_random_filename("aec_output_double_talk_white_noise_", ".wav");
	config.time_compensation_ms = 23;
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	double threshold = 0.89;
	const MSAudioDiffParams audio_cmp_params = {1, 200};
	talk_base(&config, delay_ms, 0, &estimated_delay_ms, threshold, audio_cmp_params);
	uninit_config(&config);
}

static void simple_talk(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK_FILE_11);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK_FILE_11);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK_FILE_11);
	char *baseName = "aec_output_simple_talk_";
	config.record_file = ms_tester_get_random_filename(baseName, ".wav");
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	double threshold = 0.99;
	const MSAudioDiffParams audio_cmp_params = {0, 0};
	talk_base(&config, delay_ms, 0, &estimated_delay_ms, threshold, audio_cmp_params);
	uninit_config(&config);
}

static void simple_talk_48000Hz(void) {
	aec_test_config config;
	init_config(&config);
	config.sampling_rate = 48000;
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK_FILE_11);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK_FILE_11);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK_FILE_11);
	config.record_file = ms_tester_get_random_filename("aec_output_simple_talk_48000Hz_resampled_16000Hz_", ".wav");
	int delay_ms = 100;
	int estimated_delay_ms = 0;
	double threshold = 0.98;
	const MSAudioDiffParams audio_cmp_params = {0, 0};
	talk_base(&config, delay_ms, 0, &estimated_delay_ms, threshold, audio_cmp_params);
	uninit_config(&config);
}

static void simple_talks_with_several_delays(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK_FILE_11);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK_FILE_11);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK_FILE_11);

	int delays[7] = {0, 40, 80, 100, 150, 300, 500};
	int delay_ms = 0;
	int estimated_delay_ms = 0;
	int length;
	char *baseName = NULL;

	const MSAudioDiffParams audio_cmp_params = {0, 0};
	double threshold = 0.99;

	for (int i = 0; i < 7; i++) {
		delay_ms = delays[i];
		if (baseName) {
			free(baseName);
		}
		length = snprintf(NULL, 0, "aec_output_delay_%dms_", delay_ms);
		baseName = malloc(length + 1);
		snprintf(baseName, length + 1, "aec_output_delay_%dms_", delay_ms);
		if (config.record_file) {
			unlink(config.record_file);
			free(config.record_file);
		}
		config.record_file = ms_tester_get_random_filename(baseName, ".wav");

		talk_base(&config, delay_ms, 0, &estimated_delay_ms, threshold, audio_cmp_params);
		ms_message("estimated delay is %d, real delay is %d", estimated_delay_ms, delay_ms);
		BC_ASSERT_LOWER(abs(estimated_delay_ms), 32, int, "%d");
	}
	free(baseName);
	uninit_config(&config);
}

static void simple_talk_with_delay_drift(void) {
	aec_test_config config;
	init_config(&config);
	config.nearend_speech_file = bc_tester_res(NEAR_END_SPEECH_SIMPLE_TALK_FILE_11);
	config.echo_file = bc_tester_res(ECHO_SIMPLE_TALK_FILE_11);
	config.farend_speech_file = bc_tester_res(FAR_END_SPEECH_SIMPLE_TALK_FILE_11);
	config.record_file = ms_tester_get_random_filename("aec_output_delay_drift_", ".wav");
	int delay_ms = 100;
	int delay_drift_ms = 50;
	int estimated_delay_ms = 0;
	const MSAudioDiffParams audio_cmp_params = {0, 0};

	bool_t aec_done = aec_base(&config, delay_ms, delay_drift_ms, &estimated_delay_ms);
	ms_message("final estimated delay is %d, real delay is %d + %d", estimated_delay_ms, delay_ms, delay_drift_ms);
	BC_ASSERT_LOWER(abs(estimated_delay_ms), 64, int, "%d"); // TODO FHA must confirm this

	if (aec_done) {
		ms_message("compare file %s", config.mic_rec_file);
		ms_message("with    file %s", config.record_file);
		double mic_similar = 0.0;
		BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(config.mic_rec_file, config.record_file, &mic_similar,
		                                              &audio_cmp_params, NULL, NULL, TIME_OFFSET),
		                0, int, "%d");
		// TODO FHA for efficient dynamic AEC, test should be
		// BC_ASSERT_GREATER(mic_similar, 0.5, double, "%f");
		// BC_ASSERT_LOWER(mic_similar, 0.7, double, "%f");
		BC_ASSERT_GREATER(mic_similar, 0.80, double, "%f");
		BC_ASSERT_LOWER(mic_similar, 0.85, double, "%f");

		ms_message("compare file %s", config.nearend_speech_file);
		ms_message("with    file %s", config.record_file);
		double nearend_similar = 0.0;
		BC_ASSERT_EQUAL(ms_audio_diff_from_given_time(config.nearend_speech_file, config.record_file, &nearend_similar,
		                                              &audio_cmp_params, NULL, NULL, TIME_OFFSET),
		                0, int, "%d");
		// TODO FHA for efficient dynamic AEC, test should be
		// BC_ASSERT_GREATER(nearend_similar, 0.9, double, "%f");
		// BC_ASSERT_LOWER(nearend_similar, 1., double, "%f");
		BC_ASSERT_GREATER(nearend_similar, 0.78, double, "%f");
		BC_ASSERT_LOWER(nearend_similar, 1., double, "%f");
	}
	uninit_config(&config);
}

static test_t tests[] = {
    TEST_NO_TAG("Simple talk", simple_talk),
    TEST_NO_TAG("Double talk", double_talk),
    TEST_NO_TAG("Double talk with white noise", double_talk_white_noise),
    TEST_NO_TAG("Near end single talk", near_end_single_talk),
    TEST_NO_TAG("Far end single talk", far_end_single_talk),
    TEST_NO_TAG("Near end single talk with white noise", near_end_single_talk_white_noise),
    TEST_NO_TAG("Far end single talk with white noise", far_end_single_talk_white_noise),
    TEST_NO_TAG("Simple talk 48000 Hz", simple_talk_48000Hz),
    TEST_NO_TAG("Simple talk with delay drift", simple_talk_with_delay_drift),
    TEST_NO_TAG("Simple talks with several delays", simple_talks_with_several_delays),
};

test_suite_t aec_test_suite = {
    "AEC", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0};
