/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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
#include "bctoolbox/list.h"
#include "bctoolbox/tester.h"
#include "mediastreamer2/allfilters.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfactory.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msinterfaces.h"
#include "mediastreamer2/msnoisesuppressor.h"
#include "mediastreamer2/msutils.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "ortp/port.h"
#include "ortp/rtpsession.h"
#include <locale.h>

#define NS_DUMP 0

#define FAREND_FILE "sounds/farend_simple_talk_48000_2.wav"
#define SPEECH_FILE "sounds/nearend_simple_talk_48000.wav"
#define NOISY_NEAREND_WITH_ECHO_400_FILE "sounds/nearend_echo_400ms_noise_12dB_simple_talk_48000.wav"
#define NOISY_SPEECH_12DB_FILE "sounds/noisy_nearend_12dB_simple_talk_48000.wav"
#define NOISY_SPEECH_6DB_FILE "sounds/noisy_nearend_6dB_simple_talk_48000.wav"
#define NOISY_SPEECH_0DB_FILE "sounds/noisy_nearend_0dB_simple_talk_48000.wav"
#define NOISY_SPEECH_12DB_STEREO_FILE "sounds/noisy_nearend_12dB_simple_talk_48000_stereo.wav"
#define SPEECH_16000_FILE "sounds/hello16000.wav"
#define SPEECH_STEREO_FILE "sounds/chimes_48000_stereo.wav"

#define MARIELLE_RTP_PORT (base_port + 1)
#define MARIELLE_RTCP_PORT (base_port + 2)

#define MARGAUX_RTP_PORT (base_port + 3)
#define MARGAUX_RTCP_PORT (base_port + 4)

#define PAULINE_IN_RTP_PORT (base_port + 5)
#define PAULINE_IN_RTCP_PORT (base_port + 6)
#define PAULINE_OUT_RTP_PORT (base_port + 7)
#define PAULINE_OUT_RTCP_PORT (base_port + 8)

typedef struct _denoising_test_config {
	char *speech_file;
	char *clean_file;
	char *reference_file;
	int sample_rate_Hz;
	int play_duration_ms;
	int start_comparison_ms;
	int start_noise_ms;
	int start_comparison_short_ms; // interval used to compare tested and reference signal in order to align them before
	                               // computing quality metrics, must include speech
	int stop_comparison_short_ms;
	int max_shift_percent;
	bool_t bypass;
	bool_t add_ns;
} denoising_test_config;

static void init_denoising_test_config(denoising_test_config *config) {
	config->speech_file = NULL;
	config->clean_file = NULL;
	config->reference_file = bc_tester_res(SPEECH_FILE);
	config->sample_rate_Hz = 48000;
	config->play_duration_ms = 12500;
	config->start_comparison_ms = 0;
	config->start_comparison_short_ms = 3500;
	config->stop_comparison_short_ms = 4500;
	config->max_shift_percent = 5;
	config->bypass = FALSE;
	config->add_ns = TRUE;
}

static void uninit_denoising_test_config(denoising_test_config *config) {
	if (config->speech_file) ms_free(config->speech_file);
#if NS_DUMP != 1
	if (config->clean_file) unlink(config->clean_file);
#endif
	if (config->clean_file) ms_free(config->clean_file);
	if (config->reference_file) ms_free(config->reference_file);
	config->speech_file = NULL;
	config->clean_file = NULL;
	config->reference_file = NULL;
}

static MSFactory *msFactory = NULL;
static RtpProfile rtp_profile;

static int tester_before_all(void) {
	msFactory = ms_tester_factory_new();
	ms_factory_enable_statistics(msFactory, TRUE);
	ortp_init();
	rtp_profile_set_payload(&rtp_profile, OPUS_PAYLOAD_TYPE, &payload_type_opus);
	return 0;
}

static int tester_after_all(void) {
	ms_factory_destroy(msFactory);
	rtp_profile_clear_all(&rtp_profile);
	return 0;
}

static void
fileplay_eof(void *user_data, BCTBX_UNUSED(MSFilter *f), unsigned int event, BCTBX_UNUSED(void *event_data)) {
	if (event == MS_FILE_PLAYER_EOF) {
		int *done = (int *)user_data;
		*done = TRUE;
	}
}

typedef struct _stats_t {
	rtp_stats_t rtp;
	int number_of_EndOfFile;
} stats_t;

static void reset_stats(stats_t *s) {
	memset(s, 0, sizeof(stats_t));
}

static void notify_cb(void *user_data, BCTBX_UNUSED(MSFilter *f), unsigned int event, BCTBX_UNUSED(void *eventdata)) {
	stats_t *stats = (stats_t *)user_data;
	switch (event) {
		case MS_FILE_PLAYER_EOF: {
			ms_message("EndOfFile received");
			stats->number_of_EndOfFile++;
			break;
		} break;
	}
}

static bool_t noise_test_create_player(MSFilter **player, char *input_file) {
	if (!input_file) {
		BC_FAIL("no file to play");
		return FALSE;
	}
	*player = ms_factory_create_filter(msFactory, MS_FILE_PLAYER_ID);
	ms_filter_call_method_noarg(*player, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method(*player, MS_FILE_PLAYER_OPEN, input_file);
	if (!BC_ASSERT_PTR_NOT_NULL(*player)) {
		return FALSE;
	}
	return TRUE;
}

static void play_and_denoise_audio(denoising_test_config *config) {

#if NS_DUMP == 1
	unlink(config->clean_file);
#endif

	int talk_duration_ms = 0;
	int audio_done = 0;
	unsigned int filter_mask = FILTER_MASK_FILEPLAY | FILTER_MASK_SOUNDWRITE;
	MSFilter *player_talk = NULL;
	MSFilter *noise_suppressor = NULL;
	MSFilter *sound_rec = NULL;
	char *filter_name = "MSNoiseSuppressor";

	ms_factory_reset_statistics(msFactory);
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, msFactory);

	// talk
	if (!noise_test_create_player(&player_talk, config->speech_file)) {
		BC_FAIL("cannot create talk player");
		goto end;
	}
	int signal_input_rate = 0;
	int nchannels_input = 0;
	ms_filter_call_method(player_talk, MS_FILTER_GET_SAMPLE_RATE, &signal_input_rate);
	ms_filter_call_method(player_talk, MS_FILTER_GET_NCHANNELS, &nchannels_input);
	ms_message("audio parameters read are: sample rate = %d Hz, mono/stereo = %d", signal_input_rate, nchannels_input);

	int ns_sample_rate = 0;
	int ns_nchannels = 0;
	if (config->add_ns) {
		// noise suppressor
		MSFilterDesc *ns_desc = ms_factory_lookup_filter_by_name(msFactory, filter_name);
		if (!ns_desc) {
			ms_error("Filter description not found for %s", filter_name);
			BC_FAIL("Filter description not found.");
			goto end;
		}
		noise_suppressor = ms_factory_create_filter_from_desc(msFactory, ns_desc);
		ms_filter_call_method(noise_suppressor, MS_FILTER_SET_NCHANNELS, &nchannels_input);
		ms_filter_call_method(noise_suppressor, MS_FILTER_GET_NCHANNELS, &ns_nchannels);
		ms_filter_call_method(noise_suppressor, MS_FILTER_GET_SAMPLE_RATE, &ns_sample_rate);
		if (ns_sample_rate != signal_input_rate) {
			ms_error("wrong sampling rate, RNNoise requires %d Hz.", ns_sample_rate);
			BC_FAIL("Cannot apply noise suppression on this audio.");
			goto end;
		}
	} else {
		ns_sample_rate = signal_input_rate;
		ns_nchannels = nchannels_input;
	}

	// record
	sound_rec = ms_factory_create_filter(msFactory, MS_FILE_REC_ID);
	ms_filter_call_method(sound_rec, MS_FILTER_SET_SAMPLE_RATE, &ns_sample_rate);
	ms_filter_call_method(sound_rec, MS_FILTER_SET_NCHANNELS, &ns_nchannels);
	ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_CLOSE);
	ms_filter_call_method(sound_rec, MS_FILE_REC_OPEN, config->clean_file);

	MSConnectionHelper h;
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, player_talk, -1, 0);
	if (noise_suppressor) ms_connection_helper_link(&h, noise_suppressor, 0, 0);
	ms_connection_helper_link(&h, sound_rec, 0, -1);

	if (noise_suppressor) ms_ticker_attach(ms_tester_ticker, noise_suppressor);
	else ms_ticker_attach(ms_tester_ticker, player_talk);

	ms_filter_call_method(player_talk, MS_PLAYER_GET_DURATION, &talk_duration_ms);
	ms_filter_add_notify_callback(player_talk, fileplay_eof, &audio_done, TRUE);
	ms_filter_call_method_noarg(player_talk, MS_PLAYER_START);
	int wait_ms = 0;
	ms_filter_call_method(player_talk, MS_FILE_PLAYER_LOOP, &wait_ms);
	ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_START);

	if (noise_suppressor) {
		if (config->bypass) {
			ms_filter_call_method(noise_suppressor, MS_NOISE_SUPPRESSOR_SET_BYPASS_MODE, &config->bypass);
		}
	}

	// real time
	int time_step_usec = 10000;
	struct timeval start_time;
	struct timeval now;
	float elapsed = 0.;
	bctbx_gettimeofday(&start_time, NULL);
	while (audio_done != 1 && elapsed < config->play_duration_ms) {
		bctbx_gettimeofday(&now, NULL);
		elapsed = ((now.tv_sec - start_time.tv_sec) * 1000.0f) + ((now.tv_usec - start_time.tv_usec) / 1000.0f);
		ms_usleep(time_step_usec);
	}

	if (noise_suppressor) {
		if (config->bypass) {
			ms_filter_call_method(noise_suppressor, MS_NOISE_SUPPRESSOR_GET_BYPASS_MODE, &config->bypass);
			BC_ASSERT_TRUE(config->bypass);
		}
	}

	ms_filter_call_method_noarg(player_talk, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method_noarg(sound_rec, MS_FILE_REC_CLOSE);
	if (noise_suppressor) ms_ticker_detach(ms_tester_ticker, noise_suppressor);
	else ms_ticker_detach(ms_tester_ticker, player_talk);

	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, player_talk, -1, 0);
	if (noise_suppressor) ms_connection_helper_unlink(&h, noise_suppressor, 0, 0);
	ms_connection_helper_unlink(&h, sound_rec, 0, -1);

end:
	ms_factory_log_statistics(msFactory);
	if (player_talk) ms_filter_destroy(player_talk);
	if (noise_suppressor) ms_filter_destroy(noise_suppressor);
	if (sound_rec) ms_filter_destroy(sound_rec);
	if (filter_mask) ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

static void check_audio_quality(char *clean_file,
                                char *reference_file,
                                int start_short_ms,
                                int stop_short_ms,
                                int start_comparison_ms,
                                double energy_min,
                                double energy_max,
                                double similarity_min,
                                int max_shift_percent) {
	setlocale(LC_NUMERIC, "C");
	double energy = 0.;
	double similar = 0.;
	MSAudioDiffParams audio_cmp_params;
	audio_cmp_params.chunk_size_ms = 0;
	audio_cmp_params.max_shift_percent = max_shift_percent;
	ms_message("Compare clean audio with reference");
	ms_message("audio clean     %s", clean_file);
	ms_message("audio reference %s", reference_file);
	ms_message("start audio comparisons at %d ms, align audio between [%d, %d]", start_comparison_ms, start_short_ms,
	           stop_short_ms);
	ms_message("Try to align output on reference by computing cross correlation with a maximal shift of %d percent",
	           audio_cmp_params.max_shift_percent);
	BC_ASSERT_EQUAL(ms_audio_compare_silence_and_speech(reference_file, clean_file, &similar, &energy,
	                                                    &audio_cmp_params, NULL, NULL, start_short_ms, stop_short_ms,
	                                                    start_comparison_ms),
	                0, int, "%d");
	ms_message("energy in silence = %f - min = %f, max = %f", energy, energy_min, energy_max);
	ms_message("similarity in talk = %f - min = %f", similar, similarity_min);
	BC_ASSERT_GREATER(similar, similarity_min, double, "%f");
	BC_ASSERT_LOWER(similar, 1.0, double, "%f");
	BC_ASSERT_GREATER(energy, energy_min, double, "%f");
	BC_ASSERT_LOWER(energy, energy_max, double, "%f");
}

static void talk_with_noise_snr_12dB(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.clean_file = bc_tester_file("clean_talk_with_noise_snr_12dB.wav");
	config.speech_file = bc_tester_res(NOISY_SPEECH_12DB_FILE);
	play_and_denoise_audio(&config);
	check_audio_quality(config.clean_file, config.reference_file, config.start_comparison_short_ms,
	                    config.stop_comparison_short_ms, config.start_comparison_ms, 0.f, 0.1f, 0.97f,
	                    config.max_shift_percent);
	uninit_denoising_test_config(&config);
}

static void talk_with_noise_snr_6dB(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.clean_file = bc_tester_file("clean_talk_with_noise_snr_6dB.wav");
	config.speech_file = bc_tester_res(NOISY_SPEECH_6DB_FILE);
	play_and_denoise_audio(&config);
	check_audio_quality(config.clean_file, config.reference_file, config.start_comparison_short_ms,
	                    config.stop_comparison_short_ms, config.start_comparison_ms, 0.f, 0.1f, 0.96f,
	                    config.max_shift_percent);
	uninit_denoising_test_config(&config);
}

static void talk_with_noise_snr_0dB(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.clean_file = bc_tester_file("clean_talk_with_noise_snr_0dB.wav");
	config.speech_file = bc_tester_res(NOISY_SPEECH_0DB_FILE);
	play_and_denoise_audio(&config);
	check_audio_quality(config.clean_file, config.reference_file, config.start_comparison_short_ms,
	                    config.stop_comparison_short_ms, config.start_comparison_ms, 0.f, 0.15f, 0.95f,
	                    config.max_shift_percent);
	uninit_denoising_test_config(&config);
}

static void talk_with_noise_snr_12dB_for_stereo(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.clean_file = bc_tester_file("clean_talk_with_noise_snr_12dB_for_stereo.wav");
	config.speech_file = bc_tester_res(NOISY_SPEECH_12DB_STEREO_FILE);
	config.play_duration_ms = 7000;
	play_and_denoise_audio(&config);
	check_audio_quality(config.clean_file, config.reference_file, config.start_comparison_short_ms,
	                    config.stop_comparison_short_ms, config.start_comparison_ms, 0.f, 0.1f, 0.97f,
	                    config.max_shift_percent);
	uninit_denoising_test_config(&config);
}

static void talk_with_noise_bypass_mode(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.clean_file = bc_tester_file("clean_talk_with_noise_bypass_mode.wav");
	config.speech_file = bc_tester_res(NOISY_SPEECH_0DB_FILE);
	config.bypass = TRUE;
	play_and_denoise_audio(&config);
	check_audio_quality(config.clean_file, config.reference_file, config.start_comparison_short_ms,
	                    config.stop_comparison_short_ms, config.start_comparison_ms, 150.f, 200.f, 0.86f,
	                    config.max_shift_percent);
	uninit_denoising_test_config(&config);
}

static void audio_stream_base(const char *audio_file_marielle,
                              const char *audio_file_margaux,
                              const char *audio_file_record,
                              const char *audio_file_record_part_2,
                              const char *marielle_local_ip,
                              const char *marielle_remote_ip,
                              int marielle_local_rtp_port,
                              int marielle_remote_rtp_port,
                              int marielle_local_rtcp_port,
                              int marielle_remote_rtcp_port,
                              const char *margaux_local_ip,
                              const char *margaux_remote_ip,
                              int margaux_local_rtp_port,
                              int margaux_remote_rtp_port,
                              int margaux_local_rtcp_port,
                              int margaux_remote_rtcp_port,
                              bool_t echo_cancellation,
                              bool_t noise_suppression,
                              int audio_stop_ms,
                              int delay_ms,
                              int config_change_ms,
                              const char *audio_file_marielle_2) {

	MSFilterDesc *ns_desc = ms_factory_lookup_filter_by_name(msFactory, "MSNoiseSuppressor");
	if (ns_desc == NULL) {
		BC_PASS("Noise suppression not enabled");
		return;
	}
	if (echo_cancellation) {
		MSFilterDesc *aec_desc = ms_factory_lookup_filter_by_name(msFactory, "MSWebRTCAEC");
		if (aec_desc == NULL) {
			BC_PASS("AEC not enabled");
			return;
		}
	}

#if NS_DUMP == 1
	unlink(audio_file_record);
	if (audio_file_record_part_2) unlink(audio_file_record_part_2);
#endif

	stats_t marielle_stats;
	stats_t margaux_stats;
	AudioStream *marielle =
	    audio_stream_new2(msFactory, marielle_local_ip, marielle_local_rtp_port, marielle_local_rtcp_port);
	AudioStream *margaux =
	    audio_stream_new2(msFactory, margaux_local_ip, margaux_local_rtp_port, margaux_local_rtcp_port);
	rtp_session_set_multicast_loopback(marielle->ms.sessions.rtp_session, TRUE);
	rtp_session_set_multicast_loopback(margaux->ms.sessions.rtp_session, TRUE);
	int target_bitrate = 48000;
	media_stream_set_target_network_bitrate(&marielle->ms, target_bitrate);
	media_stream_set_target_network_bitrate(&margaux->ms, target_bitrate);
	if (noise_suppression) {
		audio_stream_enable_noise_suppression(marielle, TRUE);
	}
	if (echo_cancellation) {
		audio_stream_set_echo_canceller_params(marielle, 0, delay_ms, 0);
	}
	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);
	RtpProfile *profile = rtp_profile_new("default profile");
	rtp_profile_set_payload(profile, 0, &payload_type_opus);
	rtp_profile_set_payload(profile, 8, &payload_type_pcma8000);

	RtpSession *marielle_rtp_session = audio_stream_get_rtp_session(marielle);
	RtpSession *margaux_rtp_session = audio_stream_get_rtp_session(margaux);
	rtp_session_enable_adaptive_jitter_compensation(marielle_rtp_session, FALSE);
	rtp_session_enable_adaptive_jitter_compensation(margaux_rtp_session, FALSE);

	BC_ASSERT_EQUAL(audio_stream_start_full(
	                    marielle, profile, ms_is_multicast(marielle_local_ip) ? marielle_local_ip : marielle_remote_ip,
	                    ms_is_multicast(marielle_local_ip) ? marielle_local_rtp_port : marielle_remote_rtp_port,
	                    marielle_remote_ip, marielle_remote_rtcp_port, 0, 50, audio_file_marielle, NULL, NULL, NULL,
	                    echo_cancellation),
	                0, int, "%d");
	BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, margaux_remote_ip, margaux_remote_rtp_port,
	                                        margaux_remote_ip, margaux_remote_rtcp_port, 0, 50, audio_file_margaux,
	                                        audio_file_record, NULL, NULL, 0),
	                0, int, "%d");

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);
	if (config_change_ms == 0) {
		wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, audio_stop_ms);

	} else {
		int dummy = 0;
		wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, config_change_ms);
		bool_t mode = TRUE;
		ms_filter_call_method(marielle->noise_suppressor, MS_NOISE_SUPPRESSOR_GET_BYPASS_MODE, &mode);
		BC_ASSERT_FALSE(mode);
		audio_stream_play(marielle, NULL);
		if (audio_file_record_part_2) {
			ms_filter_call_method_noarg(margaux->soundwrite, MS_FILE_REC_STOP);
			ms_filter_call_method_noarg(margaux->soundwrite, MS_FILE_REC_CLOSE);
		}
		audio_stream_play(marielle, audio_file_marielle_2);
		if (audio_file_record_part_2) {
			ms_filter_call_method(margaux->soundwrite, MS_FILE_REC_OPEN, (void *)audio_file_record_part_2);
			ms_filter_call_method_noarg(margaux->soundwrite, MS_FILE_REC_START);
		}
		wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, audio_stop_ms);
		int sample_rate = 0;
		ms_filter_call_method(marielle->soundread, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);
		ms_filter_call_method(marielle->noise_suppressor, MS_NOISE_SUPPRESSOR_GET_BYPASS_MODE, &mode);
		if (sample_rate != 48000) {
			BC_ASSERT_TRUE(mode);
		} else {
			BC_ASSERT_FALSE(mode);
		}
	}
	audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);
	BC_ASSERT_GREATER_STRICT(marielle_stats.rtp.packet_sent, 0, unsigned long long, "%llu");
	BC_ASSERT_GREATER_STRICT(marielle_stats.rtp.packet_recv, 0, unsigned long long, "%llu");
	BC_ASSERT_GREATER_STRICT(margaux_stats.rtp.packet_sent, 0, unsigned long long, "%llu");
	BC_ASSERT_GREATER_STRICT(margaux_stats.rtp.packet_recv, 0, unsigned long long, "%llu");
	audio_stream_stop(marielle);
	audio_stream_stop(margaux);
	rtp_profile_destroy(profile);
}

static void audio_stream_without_noise_suppressor(const char *audio_file_marielle,
                                                  const char *audio_file_margaux,
                                                  const char *audio_file_record,
                                                  const char *marielle_local_ip,
                                                  const char *marielle_remote_ip,
                                                  int marielle_local_rtp_port,
                                                  int marielle_remote_rtp_port,
                                                  int marielle_local_rtcp_port,
                                                  int marielle_remote_rtcp_port,
                                                  const char *margaux_local_ip,
                                                  const char *margaux_remote_ip,
                                                  int margaux_local_rtp_port,
                                                  int margaux_remote_rtp_port,
                                                  int margaux_local_rtcp_port,
                                                  int margaux_remote_rtcp_port,
                                                  bool_t noise_suppression,
                                                  int audio_stop_ms) {

	MSFilterDesc *ns_desc = ms_factory_lookup_filter_by_name(msFactory, "MSNoiseSuppressor");
	if (ns_desc == NULL) {
		BC_PASS("Noise suppression not enabled");
		return;
	}

#if NS_DUMP == 1
	unlink(audio_file_record);
#endif

	stats_t marielle_stats;
	stats_t margaux_stats;
	AudioStream *marielle =
	    audio_stream_new2(msFactory, marielle_local_ip, marielle_local_rtp_port, marielle_local_rtcp_port);
	AudioStream *margaux =
	    audio_stream_new2(msFactory, margaux_local_ip, margaux_local_rtp_port, margaux_local_rtcp_port);
	rtp_session_set_multicast_loopback(marielle->ms.sessions.rtp_session, TRUE);
	rtp_session_set_multicast_loopback(margaux->ms.sessions.rtp_session, TRUE);
	int target_bitrate = 48000;
	media_stream_set_target_network_bitrate(&marielle->ms, target_bitrate);
	media_stream_set_target_network_bitrate(&margaux->ms, target_bitrate);
	if (noise_suppression) {
		audio_stream_enable_noise_suppression(marielle, TRUE);
	} else {
		audio_stream_enable_noise_suppression(marielle, FALSE);
	}
	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);
	RtpProfile *profile = rtp_profile_new("default profile");
	rtp_profile_set_payload(profile, 0, &payload_type_opus);
	rtp_profile_set_payload(profile, 8, &payload_type_pcma8000);

	BC_ASSERT_EQUAL(audio_stream_start_full(
	                    marielle, profile, ms_is_multicast(marielle_local_ip) ? marielle_local_ip : marielle_remote_ip,
	                    ms_is_multicast(marielle_local_ip) ? marielle_local_rtp_port : marielle_remote_rtp_port,
	                    marielle_remote_ip, marielle_remote_rtcp_port, 0, 50, audio_file_marielle, NULL, NULL, NULL, 0),
	                0, int, "%d");
	BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, margaux_remote_ip, margaux_remote_rtp_port,
	                                        margaux_remote_ip, margaux_remote_rtcp_port, 0, 50, audio_file_margaux,
	                                        audio_file_record, NULL, NULL, 0),
	                0, int, "%d");

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);
	wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, audio_stop_ms);

	if (noise_suppression) {
		// the feature is enabled, but the noise suppressor filter must be in bypass mode as the sample rate or the
		// channel number of the played file are not 48000 Hz or 1 respectively.
		BC_ASSERT_PTR_NOT_NULL(marielle->noise_suppressor);
		if (marielle->noise_suppressor) {
			bool_t mode = TRUE;
			ms_filter_call_method(marielle->noise_suppressor, MS_NOISE_SUPPRESSOR_GET_BYPASS_MODE, &mode);
			BC_ASSERT_TRUE(mode);
		}
	} else {
		// the feature is not enabled then the noise suppressor filter must not exist
		BC_ASSERT_PTR_NULL(marielle->noise_suppressor);
	}
	audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);
	BC_ASSERT_GREATER_STRICT(marielle_stats.rtp.packet_sent, 0, unsigned long long, "%llu");
	BC_ASSERT_GREATER_STRICT(marielle_stats.rtp.packet_recv, 0, unsigned long long, "%llu");
	BC_ASSERT_GREATER_STRICT(margaux_stats.rtp.packet_sent, 0, unsigned long long, "%llu");
	BC_ASSERT_GREATER_STRICT(margaux_stats.rtp.packet_recv, 0, unsigned long long, "%llu");
	audio_stream_stop(marielle);
	audio_stream_stop(margaux);
	rtp_profile_destroy(profile);
}

static void noise_suppression_in_audio_stream(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res(NOISY_SPEECH_12DB_FILE);
	config.clean_file = bc_tester_file("clean_noise_suppression_in_audio_stream.wav");
	config.play_duration_ms = 15400;
	int max_shift_percent = 20;
	char *audio_file_margaux = bc_tester_res(FAREND_FILE);
	audio_stream_base(config.speech_file, audio_file_margaux, config.clean_file, NULL, MARIELLE_IP, MARGAUX_IP,
	                  MARIELLE_RTP_PORT, MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT, MARGAUX_IP,
	                  MARIELLE_IP, MARGAUX_RTP_PORT, MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT, MARIELLE_RTCP_PORT, FALSE,
	                  TRUE, config.play_duration_ms, 0, 0, NULL);
	// TODO: set a relevant threshold for similarity measurement, when the problem of the small shifts in audio is fixed
	check_audio_quality(config.clean_file, config.reference_file, config.start_comparison_short_ms,
	                    config.stop_comparison_short_ms, config.start_comparison_ms, 0.f, 2.5f, 0.1f,
	                    max_shift_percent);
	uninit_denoising_test_config(&config);
	free(audio_file_margaux);
}

static void noise_suppression_in_audio_stream_for_stereo(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res(NOISY_SPEECH_12DB_STEREO_FILE);
	config.clean_file = bc_tester_file("clean_noise_suppression_in_audio_stream_for_stereo.wav");
	config.play_duration_ms = 7000;
	int max_shift_percent = 20;
	char *audio_file_margaux = bc_tester_res(FAREND_FILE);
	audio_stream_base(config.speech_file, audio_file_margaux, config.clean_file, NULL, MARIELLE_IP, MARGAUX_IP,
	                  MARIELLE_RTP_PORT, MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT, MARGAUX_IP,
	                  MARIELLE_IP, MARGAUX_RTP_PORT, MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT, MARIELLE_RTCP_PORT, FALSE,
	                  TRUE, config.play_duration_ms, 0, 0, NULL);
	// TODO: set a relevant threshold for similarity measurement, when the problem of the small shifts in audio is fixed
	check_audio_quality(config.clean_file, config.reference_file, config.start_comparison_short_ms,
	                    config.stop_comparison_short_ms, config.start_comparison_ms, 0.f, 2.5f, 0.1f,
	                    max_shift_percent);
	uninit_denoising_test_config(&config);
	free(audio_file_margaux);
}

static void noise_suppression_in_audio_stream_with_echo_400ms(void) {
#if SANITIZER_ENABLED == 1
	BC_PASS("Cannot run this test if sanitizer is eanbled");
	return;
#endif
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res(NOISY_NEAREND_WITH_ECHO_400_FILE);
	config.clean_file = bc_tester_file("clean_noise_suppression_in_audio_stream_with_echo_400ms.wav");
	config.start_comparison_ms = 15400;
	config.start_comparison_short_ms = 18500;
	config.stop_comparison_short_ms = 19500;
	config.play_duration_ms = 31000;
	int max_shift_percent = 20;
	int delay_init_ms = 400;
	char *audio_file_margaux = bc_tester_res(FAREND_FILE);
	audio_stream_base(config.speech_file, audio_file_margaux, config.clean_file, NULL, MARIELLE_IP, MARGAUX_IP,
	                  MARIELLE_RTP_PORT, MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT, MARGAUX_IP,
	                  MARIELLE_IP, MARGAUX_RTP_PORT, MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT, MARIELLE_RTCP_PORT, TRUE,
	                  TRUE, config.play_duration_ms, delay_init_ms, 0, NULL);
	check_audio_quality(config.clean_file, config.reference_file, config.start_comparison_short_ms,
	                    config.stop_comparison_short_ms, config.start_comparison_ms, 0.f, 0.15f, 0.77f,
	                    max_shift_percent);
	uninit_denoising_test_config(&config);
	free(audio_file_margaux);
}

static void noise_suppression_in_audio_stream_with_sample_rate_change(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res(NOISY_SPEECH_12DB_FILE);
	config.clean_file = bc_tester_file("clean_noise_suppression_in_audio_stream_with_sample_rate_change.wav");
	config.play_duration_ms = 6000;
	int config_change_ms = 5000;
	char *audio_file_margaux = bc_tester_res(FAREND_FILE);
	char *audio_file_16000 = bc_tester_res(SPEECH_16000_FILE);
	audio_stream_base(config.speech_file, audio_file_margaux, config.clean_file, NULL, MARIELLE_IP, MARGAUX_IP,
	                  MARIELLE_RTP_PORT, MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT, MARGAUX_IP,
	                  MARIELLE_IP, MARGAUX_RTP_PORT, MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT, MARIELLE_RTCP_PORT, FALSE,
	                  TRUE, config.play_duration_ms, 0, config_change_ms, audio_file_16000);
	uninit_denoising_test_config(&config);
	free(audio_file_margaux);
	free(audio_file_16000);
}

static void noise_suppression_in_audio_stream_with_nchannels_change(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res(NOISY_NEAREND_WITH_ECHO_400_FILE);
	char *clean_file_part_1 =
	    bc_tester_file("clean_noise_suppression_in_audio_stream_with_nchannels_change_part_1.wav");
	config.clean_file = bc_tester_file("clean_noise_suppression_in_audio_stream_with_nchannels_change.wav");
	config.play_duration_ms = 6000;
	int config_change_ms = 2500;
	char *audio_file_margaux = bc_tester_res(FAREND_FILE);
	char *audio_file_stereo = bc_tester_res(NOISY_SPEECH_12DB_STEREO_FILE);
	audio_stream_base(config.speech_file, audio_file_margaux, clean_file_part_1, config.clean_file, MARIELLE_IP,
	                  MARGAUX_IP, MARIELLE_RTP_PORT, MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT,
	                  MARGAUX_IP, MARIELLE_IP, MARGAUX_RTP_PORT, MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT,
	                  MARIELLE_RTCP_PORT, FALSE, TRUE, config.play_duration_ms, 0, config_change_ms, audio_file_stereo);
	config.start_comparison_ms = 0;
	config.start_comparison_short_ms = 3000;
	config.stop_comparison_short_ms = 4000;
	int max_shift_percent = 20;
	// TODO: set a relevant threshold for similarity measurement, when the problem of the small shifts in audio is fixed
	check_audio_quality(config.clean_file, config.reference_file, config.start_comparison_short_ms,
	                    config.stop_comparison_short_ms, config.start_comparison_ms, 0.f, 2.5f, 0.1f,
	                    max_shift_percent);
	uninit_denoising_test_config(&config);
#if NS_DUMP != 1
	unlink(clean_file_part_1);
#endif
	free(clean_file_part_1);
	free(audio_file_margaux);
	free(audio_file_stereo);
}

static void noise_suppression_disabled_in_audio_stream(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res(NOISY_SPEECH_12DB_FILE);
	config.clean_file = bc_tester_file("clean_noise_suppression_disabled_in_audio_stream.wav");
	config.play_duration_ms = 2500; // noise only, without speech
	char *audio_file_margaux = bc_tester_res(FAREND_FILE);
	audio_stream_without_noise_suppressor(
	    config.speech_file, audio_file_margaux, config.clean_file, MARIELLE_IP, MARGAUX_IP, MARIELLE_RTP_PORT,
	    MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT, MARGAUX_IP, MARIELLE_IP, MARGAUX_RTP_PORT,
	    MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT, MARIELLE_RTCP_PORT, FALSE, config.play_duration_ms);
	double energy = 0.;
	ms_audio_energy(config.clean_file, &energy);
	BC_ASSERT_GREATER(energy, 2., double, "%f");
	uninit_denoising_test_config(&config);
	free(audio_file_margaux);
}

static void noise_suppression_by_passed_in_audio_stream_at_16000Hz(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res(SPEECH_16000_FILE);
	config.clean_file = bc_tester_file("clean_noise_suppression_by_passed_in_audio_stream_at_16000Hz.wav");
	config.play_duration_ms = 4000;
	char *audio_file_margaux = bc_tester_res(FAREND_FILE);
	audio_stream_without_noise_suppressor(
	    config.speech_file, audio_file_margaux, config.clean_file, MARIELLE_IP, MARGAUX_IP, MARIELLE_RTP_PORT,
	    MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT, MARGAUX_IP, MARIELLE_IP, MARGAUX_RTP_PORT,
	    MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT, MARIELLE_RTCP_PORT, TRUE, config.play_duration_ms);
	uninit_denoising_test_config(&config);
	free(audio_file_margaux);
}

static void audio_stream_at_start(const char *audio_file_marielle,
                                  const char *audio_file_margaux,
                                  const char *audio_file_record,
                                  const char *marielle_local_ip,
                                  const char *marielle_remote_ip,
                                  int marielle_local_rtp_port,
                                  int marielle_remote_rtp_port,
                                  int marielle_local_rtcp_port,
                                  int marielle_remote_rtcp_port,
                                  const char *margaux_local_ip,
                                  const char *margaux_remote_ip,
                                  int margaux_local_rtp_port,
                                  int margaux_remote_rtp_port,
                                  int margaux_local_rtcp_port,
                                  int margaux_remote_rtcp_port,
                                  int audio_stop_ms) {

	MSFilterDesc *ns_desc = ms_factory_lookup_filter_by_name(msFactory, "MSNoiseSuppressor");
	if (ns_desc == NULL) {
		BC_PASS("Noise suppression not enabled");
		return;
	}

#if NS_DUMP == 1
	if (audio_file_record) unlink(audio_file_record);
#endif

	stats_t marielle_stats;
	stats_t margaux_stats;
	AudioStream *marielle =
	    audio_stream_new2(msFactory, marielle_local_ip, marielle_local_rtp_port, marielle_local_rtcp_port);
	AudioStream *margaux =
	    audio_stream_new2(msFactory, margaux_local_ip, margaux_local_rtp_port, margaux_local_rtcp_port);
	rtp_session_set_multicast_loopback(marielle->ms.sessions.rtp_session, TRUE);
	rtp_session_set_multicast_loopback(margaux->ms.sessions.rtp_session, TRUE);
	int target_bitrate = 48000;
	media_stream_set_target_network_bitrate(&marielle->ms, target_bitrate);
	media_stream_set_target_network_bitrate(&margaux->ms, target_bitrate);
	audio_stream_enable_noise_suppression(marielle, TRUE);
	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);
	RtpProfile *profile = rtp_profile_new("default profile");
	rtp_profile_set_payload(profile, 0, &payload_type_opus);
	rtp_profile_set_payload(profile, 8, &payload_type_pcma8000);

	BC_ASSERT_EQUAL(audio_stream_start_full(
	                    marielle, profile, ms_is_multicast(marielle_local_ip) ? marielle_local_ip : marielle_remote_ip,
	                    ms_is_multicast(marielle_local_ip) ? marielle_local_rtp_port : marielle_remote_rtp_port,
	                    marielle_remote_ip, marielle_remote_rtcp_port, 0, 50, audio_file_marielle, NULL, NULL, NULL, 0),
	                0, int, "%d");
	BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, margaux_remote_ip, margaux_remote_rtp_port,
	                                        margaux_remote_ip, margaux_remote_rtcp_port, 0, 50, audio_file_margaux,
	                                        audio_file_record, NULL, NULL, 0),
	                0, int, "%d");

	// get sound configuration before noise suppression filter
	MSFilter *file_player = marielle->soundread;
	int rate_in_Hz = 0;
	int nchannels = 0;
	ms_filter_call_method(file_player, MS_FILTER_GET_SAMPLE_RATE, &rate_in_Hz);
	ms_filter_call_method(file_player, MS_FILTER_GET_NCHANNELS, &nchannels);

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);
	wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, audio_stop_ms);

	BC_ASSERT_PTR_NOT_NULL(marielle->noise_suppressor);
	if (marielle->noise_suppressor) {
		bool_t mode = FALSE;
		ms_filter_call_method(marielle->noise_suppressor, MS_NOISE_SUPPRESSOR_GET_BYPASS_MODE, &mode);
		if (!audio_file_marielle) {
			// without file, the audio stream is configured at 8000 Hz, the noise suppressor must be bypassed
			BC_ASSERT_TRUE(mode);
		} else {
			// the noise suppressor is by passed if the file is not 48000 Hz and mono
			if (rate_in_Hz == 48000 && nchannels == 1) {
				BC_ASSERT_FALSE(mode);
			} else {
				BC_ASSERT_TRUE(mode);
			}
		}
	}
	audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);
	if (audio_file_marielle) {
		BC_ASSERT_GREATER_STRICT(marielle_stats.rtp.packet_sent, 0, unsigned long long, "%llu");
		BC_ASSERT_GREATER_STRICT(margaux_stats.rtp.packet_recv, 0, unsigned long long, "%llu");
	}
	BC_ASSERT_GREATER_STRICT(marielle_stats.rtp.packet_recv, 0, unsigned long long, "%llu");
	BC_ASSERT_GREATER_STRICT(margaux_stats.rtp.packet_sent, 0, unsigned long long, "%llu");
	audio_stream_stop(marielle);
	audio_stream_stop(margaux);
	rtp_profile_destroy(profile);
}

static void noise_suppression_by_passed_at_start_in_audio_stream(void) {
	denoising_test_config config;
	init_denoising_test_config(&config);
	config.play_duration_ms = 2500; // noise only, without speech
	char *audio_file_margaux = bc_tester_res(FAREND_FILE);
	// start without file then rate is 8000Hz, the noise suppressor must be by-passed
	audio_stream_at_start(NULL, audio_file_margaux, NULL, MARIELLE_IP, MARGAUX_IP, MARIELLE_RTP_PORT, MARGAUX_RTP_PORT,
	                      MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT, MARGAUX_IP, MARIELLE_IP, MARGAUX_RTP_PORT,
	                      MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT, MARIELLE_RTCP_PORT, config.play_duration_ms);
	// start with file at rate 16000Hz, the noise suppressor must be by-passed
	config.speech_file = bc_tester_res(SPEECH_16000_FILE);
	config.play_duration_ms = 15000;
	config.clean_file = bc_tester_file("clean_noise_suppression_enabled_at_start_in_audio_stream.wav");
	audio_stream_at_start(config.speech_file, audio_file_margaux, config.clean_file, MARIELLE_IP, MARGAUX_IP,
	                      MARIELLE_RTP_PORT, MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT, MARGAUX_IP,
	                      MARIELLE_IP, MARGAUX_RTP_PORT, MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT, MARIELLE_RTCP_PORT,
	                      config.play_duration_ms);
	uninit_denoising_test_config(&config);
	// start with file at rate 48000Hz, the noise suppressor must run
	init_denoising_test_config(&config);
	config.speech_file = bc_tester_res(SPEECH_FILE);
	config.play_duration_ms = 2500;
	config.clean_file = bc_tester_file("clean_noise_suppression_enabled_at_start_in_audio_stream.wav");
	audio_stream_at_start(config.speech_file, audio_file_margaux, config.clean_file, MARIELLE_IP, MARGAUX_IP,
	                      MARIELLE_RTP_PORT, MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT, MARGAUX_IP,
	                      MARIELLE_IP, MARGAUX_RTP_PORT, MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT, MARIELLE_RTCP_PORT,
	                      config.play_duration_ms);
	double energy = 0.;
	ms_audio_energy(config.clean_file, &energy);
	BC_ASSERT_LOWER(energy, 0.1, double, "%f");
	uninit_denoising_test_config(&config);
	free(audio_file_margaux);
}

static test_t tests[] = {
    TEST_NO_TAG("Talk with noise SNR 12dB", talk_with_noise_snr_12dB),
    TEST_NO_TAG("Talk with noise SNR 6dB", talk_with_noise_snr_6dB),
    TEST_NO_TAG("Talk with noise SNR 0dB", talk_with_noise_snr_0dB),
    TEST_NO_TAG("Talk with noise SNR 12dB for stereo", talk_with_noise_snr_12dB_for_stereo),
    TEST_NO_TAG("Talk with noise bypass mode", talk_with_noise_bypass_mode),
    TEST_NO_TAG("Noise suppression in audio stream", noise_suppression_in_audio_stream),
    TEST_NO_TAG("Noise suppression in audio stream for stereo", noise_suppression_in_audio_stream_for_stereo),
    TEST_NO_TAG("Noise suppression in audio stream with echo 400ms", noise_suppression_in_audio_stream_with_echo_400ms),
    TEST_NO_TAG("Noise suppression in audio stream with sample rate change",
                noise_suppression_in_audio_stream_with_sample_rate_change),
    TEST_NO_TAG("Noise suppression in audio stream with nchannels change",
                noise_suppression_in_audio_stream_with_nchannels_change),
    TEST_NO_TAG("Noise suppression disabled in audio stream", noise_suppression_disabled_in_audio_stream),
    TEST_NO_TAG("Noise suppression by passed in audio stream at 16000Hz",
                noise_suppression_by_passed_in_audio_stream_at_16000Hz),
    TEST_NO_TAG("Noise suppression by passed at start in audio stream",
                noise_suppression_by_passed_at_start_in_audio_stream),
};

test_suite_t noise_suppression_test_suite = {
    "Noise suppression", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0};
