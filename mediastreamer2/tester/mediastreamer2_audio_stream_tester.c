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
#include "mediastreamer2/msutils.h"
#include "mediastreamer2/msvolume.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

static RtpProfile rtp_profile;
static MSFactory *_factory = NULL;

static int tester_before_all(void) {
	// ms_init();
	_factory = ms_tester_factory_new();

	// ms_filter_enable_statistics(TRUE);
	ms_factory_enable_statistics(_factory, TRUE);
	ortp_init();
	rtp_profile_set_payload(&rtp_profile, 0, &payload_type_pcmu8000);
	rtp_profile_set_payload(&rtp_profile, OPUS_PAYLOAD_TYPE, &payload_type_opus);
	rtp_profile_set_payload(&rtp_profile, SPEEX16_PAYLOAD_TYPE, &payload_type_speex_wb);
	rtp_profile_set_payload(&rtp_profile, SILK16_PAYLOAD_TYPE, &payload_type_silk_wb);
	rtp_profile_set_payload(&rtp_profile, PCMA8_PAYLOAD_TYPE, &payload_type_pcma8000);
	return 0;
}

static int tester_after_all(void) {
	// ms_exit();

	ms_factory_destroy(_factory);
	rtp_profile_clear_all(&rtp_profile);
	return 0;
}

#define MARIELLE_RTP_PORT (base_port + 1)
#define MARIELLE_RTCP_PORT (base_port + 2)

#define MARGAUX_RTP_PORT (base_port + 3)
#define MARGAUX_RTCP_PORT (base_port + 4)

#define PAULINE_IN_RTP_PORT (base_port + 5)
#define PAULINE_IN_RTCP_PORT (base_port + 6)
#define PAULINE_OUT_RTP_PORT (base_port + 7)
#define PAULINE_OUT_RTCP_PORT (base_port + 8)

#define VOICE_8K_FILE "sounds/test_silence_voice_8000.wav"
#define HELLO_8K_1S_FILE "sounds/hello8000-1s.wav"
#define HELLO_8K_FILE "sounds/hello8000.wav"
#define ARPEGGIO_FILE "sounds/arpeggio_8000_mono.wav"
#define HELLO_16K_1S_FILE "sounds/hello16000-1s.wav"
#define RECORDED_8K_FILE "recorded_test_silence_voice_8000-"
#define RECORDED_8K_1S_FILE "recorded_hello8000-1s-"

typedef struct _stats_t {
	OrtpEvQueue *q;
	rtp_stats_t rtp;
	int number_of_EndOfFile;
	int number_of_TMMBR;
	int number_of_VoiceDetected;
	int number_of_VoiceEnded;
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

static void
voice_detection_cb(void *user_data, BCTBX_UNUSED(MSFilter *f), unsigned int event, BCTBX_UNUSED(void *eventdata)) {
	stats_t *stats = (stats_t *)user_data;
	switch (event) {
		case MS_VAD_EVENT_VOICE_DETECTED:
			ms_message("Voice Activity detected");
			stats->number_of_VoiceDetected++;
			break;
		case MS_VAD_EVENT_VOICE_ENDED:
			ms_message("Voice Activity ended");
			stats->number_of_VoiceEnded++;
			break;
	}
}

static void event_queue_cb(BCTBX_UNUSED(MediaStream *ms), void *user_pointer) {
	stats_t *st = (stats_t *)user_pointer;
	OrtpEvent *ev = NULL;

	if (st->q != NULL) {
		while ((ev = ortp_ev_queue_get(st->q)) != NULL) {
			OrtpEventType evt = ortp_event_get_type(ev);
			OrtpEventData *d = ortp_event_get_data(ev);
			if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
				RtcpParserContext ctx;
				const mblk_t *rtcp_packet = rtcp_parser_context_init(&ctx, d->packet);
				do {
					if (rtcp_is_RTPFB(rtcp_packet)) {
						switch (rtcp_RTPFB_get_type(rtcp_packet)) {
							case RTCP_RTPFB_TMMBR:
								st->number_of_TMMBR++;
								break;
							default:
								break;
						}
					}
				} while ((rtcp_packet = rtcp_parser_context_next_packet(&ctx)) != NULL);
				rtcp_parser_context_uninit(&ctx);
			}
			ortp_event_destroy(ev);
		}
	}
}

static void basic_audio_stream_base_2(const char *marielle_local_ip,
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
                                      int lost_percentage,
                                      bool_t voice_activity) {
	if (voice_activity) {
		MSFilterDesc *vad_desc = ms_factory_lookup_filter_by_name(_factory, "MSWebRtcVADDec");
		if (vad_desc == NULL) {
			BC_PASS("VAD not enabled");
			return;
		}
	}

	AudioStream *marielle =
	    audio_stream_new2(_factory, marielle_local_ip, marielle_local_rtp_port, marielle_local_rtcp_port);
	stats_t marielle_stats;
	AudioStream *margaux =
	    audio_stream_new2(_factory, margaux_local_ip, margaux_local_rtp_port, margaux_local_rtcp_port);

	if (voice_activity) {
		uint32_t features = audio_stream_get_features(marielle);
		features |= AUDIO_STREAM_FEATURE_VAD;
		audio_stream_set_features(marielle, features);
	}

	stats_t margaux_stats;
	RtpProfile *profile = rtp_profile_new("default profile");

	char *hello_file;
	char *random_filename;
	if (voice_activity) {
		hello_file = bc_tester_res(VOICE_8K_FILE);
		random_filename = ms_tester_get_random_filename(RECORDED_8K_FILE, ".wav");
	} else {
		hello_file = bc_tester_res(HELLO_8K_1S_FILE);
		random_filename = ms_tester_get_random_filename(RECORDED_8K_1S_FILE, ".wav");
	}

	char *recorded_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	uint64_t marielle_rtp_sent = 0;
	int dummy = 0;

	rtp_session_set_multicast_loopback(marielle->ms.sessions.rtp_session, TRUE);
	rtp_session_set_multicast_loopback(margaux->ms.sessions.rtp_session, TRUE);
	rtp_session_set_rtcp_report_interval(marielle->ms.sessions.rtp_session, 1000);
	rtp_session_set_rtcp_report_interval(margaux->ms.sessions.rtp_session, 1000);

	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);

	rtp_profile_set_payload(profile, 0, &payload_type_pcmu8000);

	BC_ASSERT_EQUAL(audio_stream_start_full(
	                    margaux, profile, ms_is_multicast(margaux_local_ip) ? margaux_local_ip : margaux_remote_ip,
	                    ms_is_multicast(margaux_local_ip) ? margaux_local_rtp_port : margaux_remote_rtp_port,
	                    margaux_remote_ip, margaux_remote_rtcp_port, 0, 50, NULL, recorded_file, NULL, NULL, 0),
	                0, int, "%d");

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, marielle_remote_ip, marielle_remote_rtp_port,
	                                        marielle_remote_ip, marielle_remote_rtcp_port, 0, 50, hello_file, NULL,
	                                        NULL, NULL, 0),
	                0, int, "%d");

	if (voice_activity) {
		BC_ASSERT_PTR_NOT_NULL(marielle->vad);
		if (marielle->vad) ms_filter_add_notify_callback(marielle->vad, voice_detection_cb, &marielle_stats, TRUE);
	}

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);

	if (voice_activity) {
		wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 26000);
		BC_ASSERT_GREATER_STRICT(marielle_stats.number_of_VoiceDetected, 3, int, "%d");
		BC_ASSERT_GREATER_STRICT(marielle_stats.number_of_VoiceEnded, 3, int, "%d");
	} else {
		wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000);
	}

	audio_stream_play(marielle, NULL);
	wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);

	audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);
	marielle_rtp_sent = marielle_stats.rtp.sent;

	if (rtp_session_rtcp_enabled(marielle->ms.sessions.rtp_session) &&
	    rtp_session_rtcp_enabled(margaux->ms.sessions.rtp_session)) {
		BC_ASSERT_GREATER_STRICT(rtp_session_get_round_trip_propagation(marielle->ms.sessions.rtp_session), 0, float,
		                         "%f");
		BC_ASSERT_GREATER_STRICT(rtp_session_get_stats(marielle->ms.sessions.rtp_session)->recv_rtcp_packets, 0,
		                         unsigned long long, "%llu");
	}

	audio_stream_stop(marielle);

	BC_ASSERT_TRUE(wait_for_until(&margaux->ms, NULL, (int *)&margaux_stats.rtp.hw_recv,
	                              (int)((marielle_rtp_sent * (100 - lost_percentage)) / 100), 2500));

	audio_stream_stop(margaux);

	unlink(recorded_file);
	free(recorded_file);
	free(hello_file);
	rtp_profile_destroy(profile);
}
static void basic_audio_stream_base(const char *marielle_local_ip,
                                    int marielle_local_rtp_port,
                                    int marielle_local_rtcp_port,
                                    const char *margaux_local_ip,
                                    int margaux_local_rtp_port,
                                    int margaux_local_rtcp_port) {
	basic_audio_stream_base_2(marielle_local_ip, margaux_local_ip, marielle_local_rtp_port, margaux_local_rtp_port,
	                          marielle_local_rtcp_port, margaux_local_rtcp_port, margaux_local_ip, marielle_local_ip,
	                          margaux_local_rtp_port, marielle_local_rtp_port, margaux_local_rtcp_port,
	                          marielle_local_rtcp_port, 0, FALSE);
}
static void basic_audio_stream(void) {
	basic_audio_stream_base(MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_IP, MARGAUX_RTP_PORT,
	                        MARGAUX_RTCP_PORT);
}

static void multicast_audio_stream(void) {
	basic_audio_stream_base("0.0.0.0", MARIELLE_RTP_PORT, 0, MULTICAST_IP, MARGAUX_RTP_PORT, 0);
}

static void encrypted_audio_stream_base(bool_t change_ssrc,
                                        bool_t change_send_key_in_the_middle,
                                        bool_t set_both_send_recv_key,
                                        bool_t send_key_first,
                                        bool_t encryption_mandatory,
                                        MSCryptoSuite suite) {
	AudioStream *marielle = audio_stream_new(_factory, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, FALSE);
	AudioStream *margaux = audio_stream_new(_factory, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT, FALSE);
	RtpProfile *profile = rtp_profile_new("default profile");
	char *hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	char *random_filename = ms_tester_get_random_filename(RECORDED_8K_1S_FILE, ".wav");
	char *recorded_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	stats_t marielle_stats;
	stats_t margaux_stats;
	int dummy = 0;
	uint64_t number_of_dropped_packets = 0;

	const MSAudioDiffParams audio_cmp_params = {10, 200};
	double similar = 0.0;
	const double threshold = 0.85;

	const char *aes_128_bits_send_key = "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj";
	const char *aes_128_bits_send_key_2 = "eCYF4nYyCvmCpFWjUeDaxI2GWp2BzCRlIPfg52Te";
	const char *aes_128_bits_recv_key = "6jCLmtRkVW9E/BUuJtYj/R2z6+4iEe06/DWohQ9F";

	const char *aes_256_bits_send_key = "nJNTwiMkyAu8zs0MWUiSQbnBL4M+xkWTYgrVLR2eFwZyO+ca2UqBy2Uh9pVRbA==";
	const char *aes_256_bits_send_key_2 = "N3vq6TMfvtyYpqGaEi9vAHMCzgWJvaD1PIfwEYtdEgI2ACezZo2vpOdV2YWEcQ==";
	const char *aes_256_bits_recv_key = "UKg69sFLbrA7d0hEVKMtT83R3GR3sjhE0XMqNBbQ+axoDWMP5dQNfjNuSQQHbw==";

	const char *aes_gcm_128_bits_send_key = "bkTcxXe9N3/vHKKiqQAqmL0qJ+CSiWRat/Tadg==";
	const char *aes_gcm_128_bits_send_key_2 = "MPKEi1/zHMH9osL2FIxUH/r3BiPjgS/LWIiTPA==";
	const char *aes_gcm_128_bits_recv_key = "Ya+BvAxQUqPer3X/AF4gDJUT4pVjbYc6O+u1pg==";

	const char *aes_gcm_256_bits_send_key = "WpvA7zUhbhJ2i1ui2nOX43QjrOwCGBkaCPtjnphQKwv/L+GdscAKGQWzG/c=";
	const char *aes_gcm_256_bits_send_key_2 = "J74fLdR6tp6EwJVgWjtcGufB7GcR64kAHbIbZyGKVq62acCZmx4mNNLIkus=";
	const char *aes_gcm_256_bits_recv_key = "PtyD6l92cGR643om/5dEIGirCCxPeL9/LJF7PaFMoMocqMrz73CO0Fz7L20=";

	const char *send_key = NULL;
	const char *send_key_2 = NULL;
	const char *recv_key = NULL;

	ms_media_stream_sessions_set_encryption_mandatory(&marielle->ms.sessions, encryption_mandatory);

	switch (suite) {
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_80:
			send_key = aes_128_bits_send_key;
			send_key_2 = aes_128_bits_send_key_2;
			recv_key = aes_128_bits_recv_key;
			break;
		case MS_AES_256_SHA1_32:
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
			send_key = aes_256_bits_send_key;
			send_key_2 = aes_256_bits_send_key_2;
			recv_key = aes_256_bits_recv_key;
			break;
		case MS_AEAD_AES_128_GCM:
			send_key = aes_gcm_128_bits_send_key;
			send_key_2 = aes_gcm_128_bits_send_key_2;
			recv_key = aes_gcm_128_bits_recv_key;
			break;
		case MS_AEAD_AES_256_GCM:
			send_key = aes_gcm_256_bits_send_key;
			send_key_2 = aes_gcm_256_bits_send_key_2;
			recv_key = aes_gcm_256_bits_recv_key;
			break;
		default:
			BC_FAIL("Unsupported suite");
			return;
	}

	if (ms_srtp_supported()) {
		reset_stats(&marielle_stats);
		reset_stats(&margaux_stats);

		rtp_profile_set_payload(profile, 0, &payload_type_pcmu8000);

		BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_IP,
		                                        MARIELLE_RTCP_PORT, 0, 50, NULL, recorded_file, NULL, NULL, 0),
		                0, int, "%d");

		BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP,
		                                        MARGAUX_RTCP_PORT, 0, 50, hello_file, NULL, NULL, NULL, 0),
		                0, int, "%d");

		if (encryption_mandatory) {
			/*wait a bit to make sure packets are discarded*/
			wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);
			audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);
			audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
			BC_ASSERT_EQUAL(margaux_stats.rtp.recv, 0, unsigned long long, "%llu");
			number_of_dropped_packets = marielle_stats.rtp.packet_sent;
		}

		if (send_key_first) {
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), suite, send_key,
			                                                              MSSrtpKeySourceSDES) == 0);
			if (set_both_send_recv_key)
				BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(margaux->ms.sessions), suite, recv_key,
				                                                              MSSrtpKeySourceSDES) == 0);

			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(margaux->ms.sessions), suite, send_key,
			                                                              MSSrtpKeySourceSDES) == 0);
			if (set_both_send_recv_key)
				BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(marielle->ms.sessions), suite, recv_key,
				                                                              MSSrtpKeySourceSDES) == 0);

		} else {
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(margaux->ms.sessions), suite, send_key,
			                                                              MSSrtpKeySourceSDES) == 0);
			if (set_both_send_recv_key)
				BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(marielle->ms.sessions), suite, recv_key,
				                                                              MSSrtpKeySourceSDES) == 0);

			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), suite, send_key,
			                                                              MSSrtpKeySourceSDES) == 0);
			if (set_both_send_recv_key)
				BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(margaux->ms.sessions), suite, recv_key,
				                                                              MSSrtpKeySourceSDES) == 0);
		}

		if (set_both_send_recv_key) {
			wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);
			BC_ASSERT_TRUE(media_stream_secured((MediaStream *)marielle));
			BC_ASSERT_TRUE(media_stream_secured((MediaStream *)margaux));
			BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)marielle, MediaStreamSendRecv, FALSE) ==
			               MSSrtpKeySourceSDES);
			BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)margaux, MediaStreamSendRecv, FALSE) ==
			               MSSrtpKeySourceSDES);
			BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)marielle, MediaStreamSendRecv, FALSE) ==
			               suite);
			BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)margaux, MediaStreamSendRecv, FALSE) ==
			               suite);
		} else {
			BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)marielle, MediaStreamSendOnly, FALSE) ==
			               MSSrtpKeySourceSDES);
			BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)margaux, MediaStreamRecvOnly, FALSE) ==
			               MSSrtpKeySourceSDES);
			BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)marielle, MediaStreamSendOnly, FALSE) ==
			               suite);
			BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)margaux, MediaStreamRecvOnly, FALSE) ==
			               suite);
			/*so far, not possible to know audio stream direction*/
			BC_ASSERT_FALSE(media_stream_secured((MediaStream *)marielle));
			BC_ASSERT_FALSE(media_stream_secured((MediaStream *)margaux));
		}

		ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);
		if (change_send_key_in_the_middle) {
			wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 2000);
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), suite, send_key_2,
			                                                              MSSrtpKeySourceSDES) == 0);
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(margaux->ms.sessions), suite, send_key_2,
			                                                              MSSrtpKeySourceSDES) == 0);
		}
		BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000));
		audio_stream_play(marielle, NULL);

		/*make sure packets can cross from sender to receiver*/
		wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);

		audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
		audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);

		/* No packet loss is assumed */
		if (change_send_key_in_the_middle) {
			/*we can accept one or 2 error in such case*/
			BC_ASSERT_TRUE(
			    (marielle_stats.rtp.packet_sent - margaux_stats.rtp.packet_recv - number_of_dropped_packets) < 3);
		} else
			BC_ASSERT_EQUAL(marielle_stats.rtp.packet_sent, margaux_stats.rtp.packet_recv + number_of_dropped_packets,
			                unsigned long long, "%llu");

		if (change_ssrc) {
			audio_stream_stop(marielle);
			marielle = audio_stream_new(_factory, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, FALSE);
			BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP,
			                                        MARGAUX_RTCP_PORT, 0, 50, hello_file, NULL, NULL, NULL, 0),
			                0, int, "%d");
			BC_ASSERT(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), suite, send_key,
			                                                         MSSrtpKeySourceSDES) == 0);

			ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);

			BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 2, 12000));
			audio_stream_play(marielle, NULL);

			/*make sure packets can cross from sender to receiver*/
			wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);

			audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
			audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);

			/* No packet loss is assumed */
			BC_ASSERT_EQUAL(marielle_stats.rtp.sent * 2, margaux_stats.rtp.recv, unsigned long long, "%llu");
		}

	} else {
		ms_warning("srtp not available, skiping...");
	}
	audio_stream_stop(marielle);
	audio_stream_stop(margaux);
	rtp_profile_destroy(profile);

	if (!change_ssrc & !encryption_mandatory) {
		BC_ASSERT_EQUAL(ms_audio_diff(hello_file, recorded_file, &similar, &audio_cmp_params, NULL, NULL), 0, int,
		                "%d");
		BC_ASSERT_GREATER(similar, threshold, double, "%f");
		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
	}

	unlink(recorded_file);
	free(recorded_file);
	free(hello_file);
}

static void encrypted_audio_stream(void) {
	encrypted_audio_stream_base(FALSE, FALSE, FALSE, TRUE, FALSE, MS_AES_128_SHA1_32);
	encrypted_audio_stream_base(FALSE, FALSE, FALSE, TRUE, FALSE, MS_AES_128_SHA1_80);
	encrypted_audio_stream_base(FALSE, FALSE, FALSE, TRUE, FALSE, MS_AES_256_SHA1_32);
	encrypted_audio_stream_base(FALSE, FALSE, FALSE, TRUE, FALSE, MS_AES_256_SHA1_80);
	encrypted_audio_stream_base(FALSE, FALSE, FALSE, TRUE, FALSE, MS_AEAD_AES_128_GCM);
	encrypted_audio_stream_base(FALSE, FALSE, FALSE, TRUE, FALSE, MS_AEAD_AES_256_GCM);
}

static void encrypted_audio_stream_with_2_srtp_stream(void) {
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, TRUE, FALSE, MS_AES_128_SHA1_32);
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, TRUE, FALSE, MS_AES_128_SHA1_80);
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, TRUE, FALSE, MS_AES_256_SHA1_32);
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, TRUE, FALSE, MS_AES_256_SHA1_80);
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, TRUE, FALSE, MS_AEAD_AES_128_GCM);
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, TRUE, FALSE, MS_AEAD_AES_256_GCM);
}

static void encrypted_audio_stream_with_2_srtp_stream_recv_first(void) {
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, FALSE, FALSE, MS_AES_128_SHA1_32);
}

static void encrypted_audio_stream_with_key_change(void) {
	encrypted_audio_stream_base(FALSE, TRUE, FALSE, TRUE, FALSE, MS_AES_128_SHA1_32);
}

static void encrypted_audio_stream_with_ssrc_change(void) {
	encrypted_audio_stream_base(TRUE, FALSE, FALSE, TRUE, FALSE, MS_AES_128_SHA1_32);
}
static void encrypted_audio_stream_encryption_mandatory(void) {
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, TRUE, TRUE, MS_AES_128_SHA1_32);
}

static void encrypted_audio_stream_with_key_change_encryption_mandatory(void) {
	encrypted_audio_stream_base(FALSE, TRUE, FALSE, TRUE, TRUE, MS_AES_128_SHA1_32);
}

static void codec_change_for_audio_stream(void) {
	AudioStream *marielle = audio_stream_new2(_factory, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT);
	stats_t marielle_stats;
	AudioStream *margaux = audio_stream_new2(_factory, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT);
	stats_t margaux_stats;
	RtpProfile *profile = rtp_profile_new("default profile");
	char *hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	char *random_filename = ms_tester_get_random_filename(RECORDED_8K_1S_FILE, ".wav");
	char *recorded_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	uint64_t marielle_rtp_sent = 0;
	int dummy = 0;

	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);

	rtp_profile_set_payload(profile, 0, &payload_type_pcmu8000);
	rtp_profile_set_payload(profile, 8, &payload_type_pcma8000);

	BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_IP,
	                                        MARIELLE_RTCP_PORT, 0, 50, NULL, recorded_file, NULL, NULL, 0),
	                0, int, "%d");

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP,
	                                        MARGAUX_RTCP_PORT, 0, 50, hello_file, NULL, NULL, NULL, 0),
	                0, int, "%d");

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);

	BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000));
	audio_stream_play(marielle, NULL);

	/*make sure packets can cross from sender to receiver*/
	wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);

	audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);

	/* No packet loss is assumed */
	BC_ASSERT_EQUAL(marielle_stats.rtp.sent, margaux_stats.rtp.recv, unsigned long long, "%llu");
	marielle_rtp_sent = marielle_stats.rtp.sent;

	audio_stream_stop(marielle);
	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);
	marielle = audio_stream_new2(_factory, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT);
	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP,
	                                        MARGAUX_RTCP_PORT, 8, 50, hello_file, NULL, NULL, NULL, 0),
	                0, int, "%d");

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);

	BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000));
	audio_stream_play(marielle, NULL);

	/*make sure packets can cross from sender to receiver*/
	wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);

	audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);

	/* No packet loss is assumed */
	BC_ASSERT_EQUAL(marielle_stats.rtp.sent + marielle_rtp_sent, margaux_stats.rtp.recv, unsigned long long, "%llu");
	BC_ASSERT_EQUAL(strcasecmp(margaux->ms.decoder->desc->enc_fmt, "pcma"), 0, int, "%d");
	audio_stream_stop(marielle);
	audio_stream_stop(margaux);

	unlink(recorded_file);
	free(recorded_file);
	free(hello_file);
	rtp_profile_destroy(profile);
}

static void tmmbr_feedback_for_audio_stream(void) {
	AudioStream *marielle = audio_stream_new2(_factory, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT);
	stats_t marielle_stats;
	AudioStream *margaux = audio_stream_new2(_factory, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT);
	stats_t margaux_stats;
	RtpProfile *profile = rtp_profile_new("default profile");
	RtpSession *marielle_session;
	RtpSession *margaux_session;
	char *hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	int dummy = 0;

	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);

	rtp_profile_set_payload(profile, 0, &payload_type_pcmu8000);

	/* Activate AVPF and TMBRR. */
	payload_type_set_flag(&payload_type_pcmu8000, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
	marielle_session = audio_stream_get_rtp_session(marielle);
	rtp_session_enable_avpf_feature(marielle_session, ORTP_AVPF_FEATURE_TMMBR, TRUE);
	marielle_stats.q = ortp_ev_queue_new();
	rtp_session_register_event_queue(marielle->ms.sessions.rtp_session, marielle_stats.q);
	margaux_session = audio_stream_get_rtp_session(margaux);
	rtp_session_enable_avpf_feature(margaux_session, ORTP_AVPF_FEATURE_TMMBR, TRUE);
	margaux_stats.q = ortp_ev_queue_new();
	rtp_session_register_event_queue(margaux->ms.sessions.rtp_session, margaux_stats.q);

	BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_IP,
	                                        MARIELLE_RTCP_PORT, 0, 50, hello_file, NULL, NULL, NULL, 0),
	                0, int, "%d");

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP,
	                                        MARGAUX_RTCP_PORT, 0, 50, hello_file, NULL, NULL, NULL, 0),
	                0, int, "%d");

	ms_filter_add_notify_callback(margaux->soundread, notify_cb, &margaux_stats, TRUE);
	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);

	/* Wait for 1s so that some RTP packets are exchanged before sending the TMMBR. */
	wait_for_until(&margaux->ms, &marielle->ms, &dummy, 1, 1500);

	rtp_session_send_rtcp_fb_tmmbr(margaux_session, 100000);
	rtp_session_send_rtcp_fb_tmmbr(marielle_session, 200000);

	BC_ASSERT_TRUE(wait_for_until(&margaux->ms, &marielle->ms, &margaux_stats.number_of_EndOfFile, 1, 12000));
	BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000));

	BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->ms, &margaux->ms, &marielle_stats.number_of_TMMBR, 1,
	                                                100, event_queue_cb, &marielle_stats, event_queue_cb,
	                                                &margaux_stats));
	BC_ASSERT_TRUE(wait_for_until_with_parse_events(&margaux->ms, &marielle->ms, &margaux_stats.number_of_TMMBR, 1, 100,
	                                                event_queue_cb, &margaux_stats, event_queue_cb, &marielle_stats));
	BC_ASSERT_EQUAL(marielle_stats.number_of_TMMBR, 1, int, "%d");
	BC_ASSERT_EQUAL(margaux_stats.number_of_TMMBR, 1, int, "%d");

	/*make sure packets can cross from sender to receiver*/
	wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);

	audio_stream_stop(marielle);
	audio_stream_stop(margaux);

	free(hello_file);
	ortp_ev_queue_destroy(marielle_stats.q);
	ortp_ev_queue_destroy(margaux_stats.q);
	rtp_profile_destroy(profile);
}

#if 0
static void audio_stream_dtmf(int codec_payload, int initial_bitrate,int target_bw, int max_recv_rtcp_packet) {
	stream_manager_t * marielle = stream_manager_new();
	stream_manager_t * margaux = stream_manager_new();
	int pause_time=0;

	OrtpNetworkSimulatorParams params={0};
	params.enabled=TRUE;
	params.loss_rate=0;
	params.max_bandwidth=target_bw;
	params.max_buffer_size=initial_bitrate;
	float recv_send_bw_ratio;
	int rtcp_interval = 1000;
	float marielle_send_bw;

	media_stream_enable_adaptive_bitrate_control(&marielle->stream->ms,TRUE);


	stream_manager_start(marielle,codec_payload, margaux->local_rtp,initial_bitrate,HELLO_16K_1S_FILE,NULL);
	ms_filter_call_method(marielle->stream->soundread,MS_FILE_PLAYER_LOOP,&pause_time);

	unlink("blibi.wav");
	stream_manager_start(margaux,codec_payload, marielle->local_rtp,-1,NULL,"blibi.wav");
	rtp_session_enable_network_simulation(margaux->stream->ms.session,&params);
	rtp_session_set_rtcp_report_interval(margaux->stream->ms.session, rtcp_interval);

	wait_for_until(&marielle->stream->ms,&margaux->stream->ms,&marielle->stats.number_of_EndOfFile,10,rtcp_interval*max_recv_rtcp_packet);

	marielle_send_bw=media_stream_get_up_bw(&marielle->stream->ms);
	recv_send_bw_ratio=params.max_bandwidth/marielle_send_bw;
	ms_message("marielle sent bw= [%f] , target was [%f] recv/send [%f]",marielle_send_bw,params.max_bandwidth,recv_send_bw_ratio);
	BC_ASSERT_TRUE(recv_send_bw_ratio>0.9);

	stream_manager_delete(marielle);
	stream_manager_delete(margaux);

}
#endif

static void symetric_rtp_with_wrong_addr(void) {
	basic_audio_stream_base_2(MARIELLE_IP, "10.10.10.10" /*dummy ip*/
	                          ,
	                          MARIELLE_RTP_PORT, MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_RTCP_PORT

	                          ,
	                          MARGAUX_IP, MARIELLE_IP, MARGAUX_RTP_PORT, MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT,
	                          MARIELLE_RTCP_PORT, 5, FALSE);
}

static void symetric_rtp_with_wrong_rtcp_port(void) {
	basic_audio_stream_base_2(MARIELLE_IP, MARGAUX_IP, MARIELLE_RTP_PORT, MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT,
	                          MARGAUX_RTCP_PORT

	                          ,
	                          MARGAUX_IP, MARIELLE_IP, MARGAUX_RTP_PORT, MARIELLE_RTP_PORT, MARGAUX_RTCP_PORT,
	                          MARIELLE_RTCP_PORT + 10 /*dummy port*/
	                          ,
	                          5, FALSE);
}

static int
request_volumes(BCTBX_UNUSED(MSFilter *filter), rtp_audio_level_t **audio_levels, BCTBX_UNUSED(void *user_data)) {
	// Adding some test values
	*audio_levels = (rtp_audio_level_t *)ms_malloc0(20 * sizeof(rtp_audio_level_t));

	(*audio_levels)[0].csrc = 1;
	(*audio_levels)[0].dbov = -5;
	(*audio_levels)[1].csrc = 2;
	(*audio_levels)[1].dbov = -15;
	(*audio_levels)[2].csrc = 3;
	(*audio_levels)[2].dbov = -127;

	for (int i = 3; i < 20; i++) {
		(*audio_levels)[i].csrc = i + 1;
		(*audio_levels)[i].dbov = -5;
	}

	return 20;
}

static void participants_volumes_in_audio_stream(void) {
	AudioStream *marielle = audio_stream_new2(_factory, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT);
	stats_t marielle_stats;
	AudioStream *margaux = audio_stream_new2(_factory, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT);
	stats_t margaux_stats;
	RtpProfile *profile = rtp_profile_new("default profile");
	char *hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	int dummy = 0;
	MSFilterRequestMixerToClientDataCb callback;

	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);

	rtp_profile_set_payload(profile, 0, &payload_type_pcmu8000);

	// Set callback and parameters for audio level indications
	audio_stream_set_mixer_to_client_extension_id(marielle, 3);
	audio_stream_set_mixer_to_client_extension_id(margaux, 3);
	callback.cb = request_volumes;
	callback.user_data = marielle;
	ms_filter_call_method(marielle->ms.rtpsend, MS_RTP_SEND_SET_MIXER_TO_CLIENT_DATA_REQUEST_CB, &callback);

	BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_IP,
	                                        MARIELLE_RTCP_PORT, 0, 50, hello_file, NULL, NULL, NULL, 0),
	                0, int, "%d");

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP,
	                                        MARGAUX_RTCP_PORT, 0, 50, hello_file, NULL, NULL, NULL, 0),
	                0, int, "%d");

	ms_filter_add_notify_callback(margaux->soundread, notify_cb, &margaux_stats, TRUE);
	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);

	wait_for_until(&margaux->ms, &marielle->ms, &dummy, 1, 2000);

	BC_ASSERT_TRUE(wait_for_until(&margaux->ms, &marielle->ms, &margaux_stats.number_of_EndOfFile, 1, 12000));
	BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000));

	/*make sure packets can cross from sender to receiver*/
	wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 2000);

	// Check that mixer to client audio levels are received
	BC_ASSERT_EQUAL(audio_stream_get_participant_volume(margaux, 1), (int)ms_volume_dbov_to_dbm0(-5), int, "%d");
	BC_ASSERT_EQUAL(audio_stream_get_participant_volume(margaux, 2), (int)ms_volume_dbov_to_dbm0(-15), int, "%d");
	BC_ASSERT_EQUAL(audio_stream_get_participant_volume(margaux, 3), (int)ms_volume_dbov_to_dbm0(-127), int, "%d");
	BC_ASSERT_EQUAL(audio_stream_get_participant_volume(margaux, 50), AUDIOSTREAMVOLUMES_NOT_FOUND, int, "%d");

	BC_ASSERT_EQUAL(audio_stream_volumes_size(margaux->participants_volumes), 20, int, "%d");

	audio_stream_stop(marielle);
	audio_stream_stop(margaux);

	free(hello_file);
	rtp_profile_destroy(profile);
}

static void double_encrypted_audio_stream_base(bool_t set_both_send_recv_key,
                                               bool_t encryption_mandatory,
                                               bool_t participant_volume,
                                               MSCryptoSuite outer_suite,
                                               MSCryptoSuite inner_suite) {
	if (!ms_srtp_supported()) {
		ms_warning("srtp not available, skiping...");
		return;
	}

	AudioStream *marielle = audio_stream_new(_factory, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, FALSE);
	AudioStream *margaux = audio_stream_new(_factory, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT, FALSE);
	RtpProfile *profile = rtp_profile_new("default profile");
	char *hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	char *random_filename = ms_tester_get_random_filename(RECORDED_8K_1S_FILE, ".wav");
	char *recorded_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	MSFilterRequestMixerToClientDataCb callback;
	stats_t marielle_stats;
	stats_t margaux_stats;
	int dummy = 0;
	uint64_t number_of_dropped_packets = 0;

	const MSAudioDiffParams audio_cmp_params = {10, 200};
	double similar = 0.0;
	const double threshold = 0.9;

	const char *aes_128_bits_marielle_outer_key = "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj";
	const char *aes_128_bits_marielle_inner_key = "eCYF4nYyCvmCpFWjUeDaxI2GWp2BzCRlIPfg52Te";
	const char *aes_128_bits_margaux_outer_key = "6jCLmtRkVW9E/BUuJtYj/R2z6+4iEe06/DWohQ9F";
	const char *aes_128_bits_margaux_inner_key = "0JygkVbNRoV8wLWoRuhiCxGDB9pOXGdoLl0P5EgJ";

	const char *aes_256_bits_marielle_outer_key = "nJNTwiMkyAu8zs0MWUiSQbnBL4M+xkWTYgrVLR2eFwZyO+ca2UqBy2Uh9pVRbA==";
	const char *aes_256_bits_marielle_inner_key = "N3vq6TMfvtyYpqGaEi9vAHMCzgWJvaD1PIfwEYtdEgI2ACezZo2vpOdV2YWEcQ==";
	const char *aes_256_bits_margaux_outer_key = "UKg69sFLbrA7d0hEVKMtT83R3GR3sjhE0XMqNBbQ+axoDWMP5dQNfjNuSQQHbw==";
	const char *aes_256_bits_margaux_inner_key = "DC6KLSR24psh1zLjbA045mTnA7e7XHqST7Ob0oIUVp1ZeYsawaO1bbkkzrR+NQ==";

	const char *aes_gcm_128_bits_marielle_outer_key = "bkTcxXe9N3/vHKKiqQAqmL0qJ+CSiWRat/Tadg==";
	const char *aes_gcm_128_bits_marielle_inner_key = "MPKEi1/zHMH9osL2FIxUH/r3BiPjgS/LWIiTPA==";
	const char *aes_gcm_128_bits_margaux_outer_key = "Ya+BvAxQUqPer3X/AF4gDJUT4pVjbYc6O+u1pg==";
	const char *aes_gcm_128_bits_margaux_inner_key = "bkTcxXe9N3/vHKKiqQAqmL0qJ+CSiWRat/Tadg==";

	const char *aes_gcm_256_bits_marielle_outer_key = "WpvA7zUhbhJ2i1ui2nOX43QjrOwCGBkaCPtjnphQKwv/L+GdscAKGQWzG/c=";
	const char *aes_gcm_256_bits_marielle_inner_key = "J74fLdR6tp6EwJVgWjtcGufB7GcR64kAHbIbZyGKVq62acCZmx4mNNLIkus=";
	const char *aes_gcm_256_bits_margaux_outer_key = "PtyD6l92cGR643om/5dEIGirCCxPeL9/LJF7PaFMoMocqMrz73CO0Fz7L20=";
	const char *aes_gcm_256_bits_margaux_inner_key = "WpvA7zUhbhJ2i1ui2nOX43QjrOwCGBkaCPtjnphQKwv/L+GdscAKGQWzG/c=";

	const char *outer_send_key = NULL;
	const char *inner_send_key = NULL;
	const char *outer_recv_key = NULL;
	const char *inner_recv_key = NULL;

	ms_media_stream_sessions_set_encryption_mandatory(&marielle->ms.sessions, encryption_mandatory);

	switch (outer_suite) {
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_80:
			outer_send_key = aes_128_bits_marielle_outer_key;
			outer_recv_key = aes_128_bits_margaux_outer_key;
			break;
		case MS_AES_256_SHA1_32:
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
			outer_send_key = aes_256_bits_marielle_outer_key;
			outer_recv_key = aes_256_bits_margaux_outer_key;
			break;
		case MS_AEAD_AES_128_GCM:
			outer_send_key = aes_gcm_128_bits_marielle_outer_key;
			outer_recv_key = aes_gcm_128_bits_margaux_outer_key;
			break;
		case MS_AEAD_AES_256_GCM:
			outer_send_key = aes_gcm_256_bits_marielle_outer_key;
			outer_recv_key = aes_gcm_256_bits_margaux_outer_key;
			break;
		default:
			BC_FAIL("Unsupported suite");
			return;
	}

	switch (inner_suite) {
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_80:
			inner_send_key = aes_128_bits_marielle_inner_key;
			inner_recv_key = aes_128_bits_margaux_inner_key;
			break;
		case MS_AES_256_SHA1_32:
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
			inner_send_key = aes_256_bits_marielle_inner_key;
			inner_recv_key = aes_256_bits_margaux_inner_key;
			break;
		case MS_AEAD_AES_128_GCM:
			inner_send_key = aes_gcm_128_bits_marielle_inner_key;
			inner_recv_key = aes_gcm_128_bits_margaux_inner_key;
			break;
		case MS_AEAD_AES_256_GCM:
			inner_send_key = aes_gcm_256_bits_marielle_inner_key;
			inner_recv_key = aes_gcm_256_bits_margaux_inner_key;
			break;
		default:
			BC_FAIL("Unsupported suite");
			return;
	}

	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);

	rtp_profile_set_payload(profile, 0, &payload_type_pcmu8000);

	// Set callback and parameters for audio level indications
	if (participant_volume) {
		audio_stream_set_mixer_to_client_extension_id(marielle, 3);
		audio_stream_set_mixer_to_client_extension_id(margaux, 3);
		callback.cb = request_volumes;
		callback.user_data = marielle;
		ms_filter_call_method(marielle->ms.rtpsend, MS_RTP_SEND_SET_MIXER_TO_CLIENT_DATA_REQUEST_CB, &callback);
	}

	BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_IP,
	                                        MARIELLE_RTCP_PORT, 0, 50, NULL, recorded_file, NULL, NULL, 0),
	                0, int, "%d");

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP,
	                                        MARGAUX_RTCP_PORT, 0, 50, hello_file, NULL, NULL, NULL, 0),
	                0, int, "%d");

	if (encryption_mandatory) {
		/*wait a bit to make sure packets are discarded*/
		wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);
		audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);
		audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
		BC_ASSERT_EQUAL(margaux_stats.rtp.recv, 0, unsigned long long, "%llu");
		number_of_dropped_packets = marielle_stats.rtp.packet_sent;
	}

	/* for testing purpose, inner keys are set with source ZRTP, even if it does not reflects the reality */
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), outer_suite, outer_send_key,
	                                                              MSSrtpKeySourceSDES) == 0);
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_send_key_b64(&(marielle->ms.sessions), inner_suite,
	                                                                    inner_send_key, MSSrtpKeySourceZRTP) == 0);
	if (set_both_send_recv_key) {
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(margaux->ms.sessions), outer_suite,
		                                                              outer_recv_key, MSSrtpKeySourceSDES) == 0);
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_send_key_b64(&(margaux->ms.sessions), inner_suite,
		                                                                    inner_recv_key, MSSrtpKeySourceZRTP) == 0);
	}

	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(margaux->ms.sessions), outer_suite, outer_send_key,
	                                                              MSSrtpKeySourceSDES) == 0);
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_recv_key_b64(
	                   &(margaux->ms.sessions), inner_suite, inner_send_key, MSSrtpKeySourceZRTP,
	                   marielle->ms.sessions.rtp_session->snd.ssrc) == 0);
	if (set_both_send_recv_key) {
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(marielle->ms.sessions), outer_suite,
		                                                              outer_recv_key, MSSrtpKeySourceSDES) == 0);
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_recv_key_b64(
		                   &(marielle->ms.sessions), inner_suite, inner_recv_key, MSSrtpKeySourceZRTP,
		                   margaux->ms.sessions.rtp_session->snd.ssrc) == 0);
	}

	if (set_both_send_recv_key) {
		wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);
		BC_ASSERT_TRUE(media_stream_secured((MediaStream *)marielle));
		BC_ASSERT_TRUE(media_stream_secured((MediaStream *)margaux));
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)marielle, MediaStreamSendRecv, FALSE) ==
		               MSSrtpKeySourceSDES);
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)margaux, MediaStreamSendRecv, FALSE) ==
		               MSSrtpKeySourceSDES);
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)marielle, MediaStreamSendRecv, TRUE) ==
		               MSSrtpKeySourceZRTP);
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)margaux, MediaStreamSendRecv, TRUE) ==
		               MSSrtpKeySourceZRTP);
		BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)marielle, MediaStreamSendRecv, FALSE) ==
		               outer_suite);
		BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)margaux, MediaStreamSendRecv, FALSE) ==
		               outer_suite);
		BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)marielle, MediaStreamSendRecv, TRUE) ==
		               inner_suite);
		BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)margaux, MediaStreamSendRecv, TRUE) ==
		               inner_suite);
	} else {
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)marielle, MediaStreamSendOnly, FALSE) ==
		               MSSrtpKeySourceSDES);
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)margaux, MediaStreamRecvOnly, FALSE) ==
		               MSSrtpKeySourceSDES);
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)marielle, MediaStreamSendOnly, TRUE) ==
		               MSSrtpKeySourceZRTP);
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)margaux, MediaStreamRecvOnly, TRUE) ==
		               MSSrtpKeySourceZRTP);
		BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)marielle, MediaStreamSendOnly, FALSE) ==
		               outer_suite);
		BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)margaux, MediaStreamRecvOnly, FALSE) ==
		               outer_suite);
		BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)marielle, MediaStreamSendOnly, TRUE) ==
		               inner_suite);
		BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)margaux, MediaStreamRecvOnly, TRUE) ==
		               inner_suite);
		BC_ASSERT_FALSE(media_stream_secured((MediaStream *)marielle));
		BC_ASSERT_FALSE(media_stream_secured((MediaStream *)margaux));
	}

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);
	BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000));
	audio_stream_play(marielle, NULL);

	/*make sure packets can cross from sender to receiver*/
	wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);

	if (participant_volume) {
		BC_ASSERT_EQUAL(audio_stream_get_participant_volume(margaux, 1), (int)ms_volume_dbov_to_dbm0(-5), int, "%d");
		BC_ASSERT_EQUAL(audio_stream_get_participant_volume(margaux, 2), (int)ms_volume_dbov_to_dbm0(-15), int, "%d");
		BC_ASSERT_EQUAL(audio_stream_get_participant_volume(margaux, 3), (int)ms_volume_dbov_to_dbm0(-127), int, "%d");
	}

	audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);

	/* No packet loss is assumed */
	BC_ASSERT_EQUAL(marielle_stats.rtp.packet_sent, margaux_stats.rtp.packet_recv + number_of_dropped_packets,
	                unsigned long long, "%llu");

	/* cleaning */
	audio_stream_stop(marielle);
	audio_stream_stop(margaux);
	rtp_profile_destroy(profile);

	/* compare audio files (only when encryption is not mandatory as in this case we might miss part of the file)*/
	if (!encryption_mandatory) {
		BC_ASSERT_EQUAL(ms_audio_diff(hello_file, recorded_file, &similar, &audio_cmp_params, NULL, NULL), 0, int,
		                "%d");
		BC_ASSERT_GREATER(similar, threshold, double, "%f");
		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
	}

	unlink(recorded_file);
	free(recorded_file);
	free(hello_file);
}

static void double_encrypted_audio_stream(void) {
	double_encrypted_audio_stream_base(FALSE, FALSE, FALSE, MS_AES_128_SHA1_32, MS_AES_128_SHA1_32);
	double_encrypted_audio_stream_base(FALSE, FALSE, FALSE, MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM);
}

static void double_encrypted_audio_stream_both_streams(void) {
	double_encrypted_audio_stream_base(TRUE, FALSE, FALSE, MS_AES_256_SHA1_32, MS_AES_128_SHA1_80);
	double_encrypted_audio_stream_base(TRUE, FALSE, FALSE, MS_AEAD_AES_256_GCM, MS_AEAD_AES_256_GCM);
}

static void double_encrypted_audio_stream_encryption_mandatory(void) {
	double_encrypted_audio_stream_base(FALSE, TRUE, FALSE, MS_AES_128_SHA1_32, MS_AES_128_SHA1_32);
	double_encrypted_audio_stream_base(FALSE, TRUE, FALSE, MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM);
}

static void double_encrypted_audio_stream_with_participants_volumes(void) {
	double_encrypted_audio_stream_base(FALSE, FALSE, TRUE, MS_AES_128_SHA1_32, MS_AES_128_SHA1_32);
	double_encrypted_audio_stream_base(FALSE, FALSE, TRUE, MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM);
}

/* Marielle and Pauline audiostream aimed at Margaux, on the same port -> they are automatically bundled in reception */
static void multiple_audiostreams_to_bundled_base(MSCryptoSuite outer_suite, BCTBX_UNUSED(MSCryptoSuite inner_suite)) {
	if (!ms_srtp_supported()) {
		ms_warning("srtp not available, skiping...");
		return;
	}

	AudioStream *marielle = audio_stream_new(_factory, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, FALSE);
	AudioStream *pauline = audio_stream_new(_factory, PAULINE_IN_RTP_PORT, PAULINE_IN_RTCP_PORT, FALSE);
	AudioStream *margaux = audio_stream_new(_factory, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT, FALSE);
	media_stream_set_direction(&marielle->ms, MediaStreamSendOnly);
	media_stream_set_direction(&pauline->ms, MediaStreamSendOnly);
	media_stream_set_direction(&margaux->ms, MediaStreamRecvOnly);
	media_stream_enable_conference_local_mix(&margaux->ms, TRUE);
	RtpProfile *profile = rtp_profile_new("default profile");
	char *hello_file = bc_tester_res(HELLO_8K_FILE);
	char *arpeggio_file = bc_tester_res(ARPEGGIO_FILE);
	char *random_filename = ms_tester_get_random_filename(RECORDED_8K_1S_FILE, ".wav");
	char *recorded_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	stats_t marielle_stats;
	stats_t margaux_stats;
	stats_t pauline_stats;
	bctbx_list_t *audiostreams = bctbx_list_new(marielle);
	audiostreams = bctbx_list_append(audiostreams, pauline);
	audiostreams = bctbx_list_append(audiostreams, margaux);
	int dummy = 0;

	/* sessions are in a bundle (even if we have only one session) for each one
	 * they all use the same MID */
	RtpBundle *rtpBundle_margaux = rtp_bundle_new();
	rtp_bundle_add_session(rtpBundle_margaux, "as", margaux->ms.sessions.rtp_session);
	/* the sending ones are using a bundle just to insert the corred MID */
	RtpBundle *rtpBundle_marielle = rtp_bundle_new();
	rtp_bundle_add_session(rtpBundle_marielle, "as", marielle->ms.sessions.rtp_session);
	RtpBundle *rtpBundle_pauline = rtp_bundle_new();
	rtp_bundle_add_session(rtpBundle_pauline, "as", pauline->ms.sessions.rtp_session);

	const char *aes_128_bits_marielle_outer_key = "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj";

	const char *aes_256_bits_marielle_outer_key = "nJNTwiMkyAu8zs0MWUiSQbnBL4M+xkWTYgrVLR2eFwZyO+ca2UqBy2Uh9pVRbA==";

	const char *aes_gcm_128_bits_marielle_outer_key = "bkTcxXe9N3/vHKKiqQAqmL0qJ+CSiWRat/Tadg==";

	const char *aes_gcm_256_bits_marielle_outer_key = "WpvA7zUhbhJ2i1ui2nOX43QjrOwCGBkaCPtjnphQKwv/L+GdscAKGQWzG/c=";

	const char *outer_send_key = NULL;

	switch (outer_suite) {
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_80:
			outer_send_key = aes_128_bits_marielle_outer_key;
			break;
		case MS_AES_256_SHA1_32:
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
			outer_send_key = aes_256_bits_marielle_outer_key;
			break;
		case MS_AEAD_AES_128_GCM:
			outer_send_key = aes_gcm_128_bits_marielle_outer_key;
			break;
		case MS_AEAD_AES_256_GCM:
			outer_send_key = aes_gcm_256_bits_marielle_outer_key;
			break;
		default:
			BC_FAIL("Unsupported suite");
			return;
	}

	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);
	reset_stats(&pauline_stats);

	rtp_profile_set_payload(profile, 0, &payload_type_pcmu8000);

	/* marielle and pauline use the SRTP key (but they do not share SSRC so stream are actually encrypted with different
	 * keys) */
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), outer_suite, outer_send_key,
	                                                              MSSrtpKeySourceSDES) == 0);
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(pauline->ms.sessions), outer_suite, outer_send_key,
	                                                              MSSrtpKeySourceSDES) == 0);

	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(margaux->ms.sessions), outer_suite, outer_send_key,
	                                                              MSSrtpKeySourceSDES) == 0);

	/* start all streams, margaux first as she is in recv only */
	BC_ASSERT_EQUAL(
	    audio_stream_start_full(margaux, profile, NULL, 0, NULL, 0, 0, 50, NULL, recorded_file, NULL, NULL, 0), 0, int,
	    "%d");

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP,
	                                        MARGAUX_RTCP_PORT, 0, 50, hello_file, NULL, NULL, NULL, 0),
	                0, int, "%d");
	BC_ASSERT_EQUAL(audio_stream_start_full(pauline, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP,
	                                        MARGAUX_RTCP_PORT, 0, 50, arpeggio_file, NULL, NULL, NULL, 0),
	                0, int, "%d");

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);
	ms_filter_add_notify_callback(pauline->soundread, notify_cb, &pauline_stats, TRUE);

	/* check streams are encrypted */
	BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)marielle, MediaStreamSendOnly, FALSE) ==
	               MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)pauline, MediaStreamSendOnly, FALSE) ==
	               MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)margaux, MediaStreamRecvOnly, FALSE) ==
	               MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)marielle, MediaStreamSendOnly, FALSE) ==
	               outer_suite);
	BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)pauline, MediaStreamSendOnly, FALSE) ==
	               outer_suite);
	BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)margaux, MediaStreamRecvOnly, FALSE) ==
	               outer_suite);
	BC_ASSERT_TRUE(media_stream_secured((MediaStream *)marielle));
	BC_ASSERT_TRUE(media_stream_secured((MediaStream *)pauline));
	BC_ASSERT_TRUE(media_stream_secured((MediaStream *)margaux));

	/* Marielle audio file is longer, so pauline should end first -> not really a problem anyway */
	BC_ASSERT_TRUE(wait_for_list(audiostreams, &pauline_stats.number_of_EndOfFile, 1, 15000));
	audio_stream_play(pauline, NULL);
	BC_ASSERT_TRUE(wait_for_list(audiostreams, &marielle_stats.number_of_EndOfFile, 1, 15000));
	audio_stream_play(marielle, NULL);

	/*make sure packets can cross from sender to receiver*/
	wait_for_list(audiostreams, &dummy, 1, 1500);

	audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(pauline, &pauline_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);

	/* No packet loss is assumed */
	/* sums packets received by margaux on the main session and the one added on discovery */
	size_t margaux_received_packets = margaux_stats.rtp.packet_recv; // main session
	bctbx_list_t *it = margaux->bundledRecvBranches;
	for (; it != NULL; it = it->next) {
		AudioStreamMixedRecvBranch *b = (AudioStreamMixedRecvBranch *)it->data;
		margaux_received_packets += rtp_session_get_stats(b->rtp_session)->packet_recv;
	}
	BC_ASSERT_EQUAL(marielle_stats.rtp.packet_sent + pauline_stats.rtp.packet_sent, margaux_received_packets,
	                unsigned long long, "%llu");

	/* cleaning */
	rtp_bundle_delete(rtpBundle_margaux);
	rtp_bundle_delete(rtpBundle_marielle);
	rtp_bundle_delete(rtpBundle_pauline);
	audio_stream_stop(marielle);
	audio_stream_stop(pauline);
	audio_stream_stop(margaux);
	rtp_profile_destroy(profile);
	bctbx_list_free(audiostreams);

	// unlink(recorded_file);
	free(recorded_file);
	free(hello_file);
	free(arpeggio_file);
}

static void multiple_audiostreams_auto_bundled(void) {
	multiple_audiostreams_to_bundled_base(MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM);
}

static void double_encrypted_rtp_relay_audio_stream_base(bool_t encryption_mandatory,
                                                         bool_t participant_volume,
                                                         bool_t use_ekt,
                                                         MSCryptoSuite outer_suite,
                                                         MSCryptoSuite inner_suite) {
	if (!ms_srtp_supported()) {
		ms_warning("srtp not available, skiping...");
		return;
	}

	AudioStream *marielle = audio_stream_new(_factory, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, FALSE);
	AudioStream *margaux = audio_stream_new(_factory, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT, FALSE);

	RtpProfile *profile = rtp_profile_new("default profile");
	char *hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	char *random_filename = ms_tester_get_random_filename(RECORDED_8K_1S_FILE, ".wav");
	char *recorded_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	MSFilterRequestMixerToClientDataCb callback;
	stats_t marielle_stats;
	stats_t margaux_stats;
	int dummy = 0;
	uint64_t number_of_dropped_packets = 0;

	const MSAudioDiffParams audio_cmp_params = {10, 200};
	double similar = 0.0;
	const double threshold = 0.85;

	const char *aes_128_bits_marielle_outer_key = "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj";
	const char *aes_128_bits_marielle_inner_key = "eCYF4nYyCvmCpFWjUeDaxI2GWp2BzCRlIPfg52Te";
	const char *aes_128_bits_pauline_outer_key = "6jCLmtRkVW9E/BUuJtYj/R2z6+4iEe06/DWohQ9F";

	const char *aes_256_bits_marielle_outer_key = "nJNTwiMkyAu8zs0MWUiSQbnBL4M+xkWTYgrVLR2eFwZyO+ca2UqBy2Uh9pVRbA==";
	const char *aes_256_bits_marielle_inner_key = "N3vq6TMfvtyYpqGaEi9vAHMCzgWJvaD1PIfwEYtdEgI2ACezZo2vpOdV2YWEcQ==";
	const char *aes_256_bits_pauline_outer_key = "UKg69sFLbrA7d0hEVKMtT83R3GR3sjhE0XMqNBbQ+axoDWMP5dQNfjNuSQQHbw==";

	const char *aes_gcm_128_bits_marielle_outer_key = "bkTcxXe9N3/vHKKiqQAqmL0qJ+CSiWRat/Tadg==";
	const char *aes_gcm_128_bits_marielle_inner_key = "MPKEi1/zHMH9osL2FIxUH/r3BiPjgS/LWIiTPA==";
	const char *aes_gcm_128_bits_pauline_outer_key = "Ya+BvAxQUqPer3X/AF4gDJUT4pVjbYc6O+u1pg==";

	const char *aes_gcm_256_bits_marielle_outer_key = "WpvA7zUhbhJ2i1ui2nOX43QjrOwCGBkaCPtjnphQKwv/L+GdscAKGQWzG/c=";
	const char *aes_gcm_256_bits_marielle_inner_key = "J74fLdR6tp6EwJVgWjtcGufB7GcR64kAHbIbZyGKVq62acCZmx4mNNLIkus=";
	const char *aes_gcm_256_bits_pauline_outer_key = "PtyD6l92cGR643om/5dEIGirCCxPeL9/LJF7PaFMoMocqMrz73CO0Fz7L20=";

	const char *outer_key = NULL;
	const char *outer_relay_key = NULL;
	const char *inner_key = NULL;

	ms_media_stream_sessions_set_encryption_mandatory(&marielle->ms.sessions, encryption_mandatory);

	switch (outer_suite) {
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_80:
			outer_key = aes_128_bits_marielle_outer_key;
			outer_relay_key = aes_128_bits_pauline_outer_key;
			break;
		case MS_AES_256_SHA1_32:
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
			outer_key = aes_256_bits_marielle_outer_key;
			outer_relay_key = aes_256_bits_pauline_outer_key;
			break;
		case MS_AEAD_AES_128_GCM:
			outer_key = aes_gcm_128_bits_marielle_outer_key;
			outer_relay_key = aes_gcm_128_bits_pauline_outer_key;
			break;
		case MS_AEAD_AES_256_GCM:
			outer_key = aes_gcm_256_bits_marielle_outer_key;
			outer_relay_key = aes_gcm_256_bits_pauline_outer_key;
			break;
		default:
			BC_FAIL("Unsupported suite");
			return;
	}

	switch (inner_suite) {
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_80:
			inner_key = aes_128_bits_marielle_inner_key;
			break;
		case MS_AES_256_SHA1_32:
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
			inner_key = aes_256_bits_marielle_inner_key;
			break;
		case MS_AEAD_AES_128_GCM:
			inner_key = aes_gcm_128_bits_marielle_inner_key;
			break;
		case MS_AEAD_AES_256_GCM:
			inner_key = aes_gcm_256_bits_marielle_inner_key;
			break;
		default:
			BC_FAIL("Unsupported suite");
			return;
	}

	MSEKTParametersSet ekt_params;
	if (use_ekt) {
		uint8_t master_salt[14] = {
		    0x01, 0x10, 0x11, 0x04, 0x40, 0x44, 0x07,
		    0x70, 0x77, 0x0a, 0xa0, 0xaa, 0xf0, 0x0f}; // 14 bytes master salt even if we may end up using only 12 bytes
		// 32 bytes EKT key value even if we may endup using only 16
		uint8_t key_value[32] = {0x23, 0xd4, 0x19, 0x12, 0x7e, 0x85, 0xa3, 0x14, 0xa8, 0x47, 0x71,
		                         0x2d, 0x04, 0x3c, 0x31, 0x50, 0xad, 0x2d, 0x16, 0x97, 0xa1, 0x60,
		                         0x41, 0xe4, 0xc5, 0xec, 0x78, 0xc1, 0xdf, 0x99, 0xb8, 0xd9};
		MSEKTCipherType ekt_cipher = MS_EKT_CIPHERTYPE_AESKW128;
		if (inner_suite == MS_AES_256_SHA1_80 || inner_suite == MS_AES_256_SHA1_32 ||
		    inner_suite == MS_AEAD_AES_256_GCM) {
			ekt_cipher = MS_EKT_CIPHERTYPE_AESKW256;
		}
		ekt_params.ekt_cipher_type = ekt_cipher;
		ekt_params.ekt_srtp_crypto_suite = inner_suite;
		memcpy(ekt_params.ekt_key_value, key_value, 32);
		memcpy(ekt_params.ekt_master_salt, master_salt, 14);
		ekt_params.ekt_spi = 0x1234;
		ekt_params.ekt_ttl = 0; // do not use ttl
	}

	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);

	rtp_profile_set_payload(profile, 121, &payload_type_pcmu8000);
	rtp_profile_set_payload(profile, 122, &payload_type_pcmu8000);

	// Set callback and parameters for audio level indications
	if (participant_volume) {
		audio_stream_set_mixer_to_client_extension_id(marielle, 3);
		audio_stream_set_mixer_to_client_extension_id(margaux, 3);
		callback.cb = request_volumes;
		callback.user_data = marielle;
		ms_filter_call_method(marielle->ms.rtpsend, MS_RTP_SEND_SET_MIXER_TO_CLIENT_DATA_REQUEST_CB, &callback);
	}

	/* Margaux is the final recipient, store received audio in he recorded_file, remote ip/port is useless */
	BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, PAULINE_IP, PAULINE_OUT_RTP_PORT, PAULINE_IP,
	                                        PAULINE_OUT_RTCP_PORT, 122, 50, NULL, recorded_file, NULL, NULL, 0),
	                0, int, "%d");

	// create leg A rtp session - marielle - pauline
	RtpSession *rtpSession_legA = ms_create_duplex_rtp_session(PAULINE_IP, PAULINE_IN_RTP_PORT, PAULINE_IN_RTCP_PORT,
	                                                           ms_factory_get_mtu(_factory));
	rtp_session_set_profile(rtpSession_legA, profile);
	rtp_session_set_remote_addr_and_port(rtpSession_legA, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT);
	rtp_session_set_payload_type(rtpSession_legA, 121);
	rtp_session_enable_transfer_mode(rtpSession_legA, TRUE);
	rtp_session_enable_rtcp(rtpSession_legA, FALSE);
	MSMediaStreamSessions sessions_legA;
	sessions_legA.rtp_session = rtpSession_legA;
	sessions_legA.srtp_context = NULL;
	sessions_legA.zrtp_context = NULL;
	sessions_legA.dtls_context = NULL;
	sessions_legA.ticker = ms_ticker_new();
	sessions_legA.bundledSndRtpSessions = NULL;
	if (use_ekt) {
		ms_media_stream_sessions_set_ekt_mode(&sessions_legA, MS_EKT_TRANSFER);
	} else {
		ms_media_stream_sessions_set_ekt_mode(&sessions_legA, MS_EKT_DISABLED_WITH_TRANSFER);
	}

	MSFilter *rtpsend_legA = NULL;
	ms_tester_create_filter(&rtpsend_legA, MS_RTP_SEND_ID, _factory);
	MSFilter *rtprecv_legA = NULL;
	ms_tester_create_filter(&rtprecv_legA, MS_RTP_RECV_ID, _factory);
	ms_filter_call_method(rtpsend_legA, MS_RTP_SEND_SET_SESSION, rtpSession_legA);
	bool_t trueBool = TRUE;
	bool_t falseBool = FALSE;
	ms_filter_call_method(rtpsend_legA, MS_RTP_SEND_ENABLE_RTP_TRANSFER_MODE, &trueBool);
	ms_filter_call_method(rtpsend_legA, MS_RTP_SEND_ENABLE_STUN, &falseBool);
	ms_filter_call_method(rtprecv_legA, MS_RTP_RECV_SET_SESSION, rtpSession_legA);
	ms_filter_call_method(rtprecv_legA, MS_RTP_RECV_ENABLE_RTP_TRANSFER_MODE, &trueBool);

	// create leg B rtp session - marielle - pauline
	RtpSession *rtpSession_legB = ms_create_duplex_rtp_session(PAULINE_IP, PAULINE_OUT_RTP_PORT, PAULINE_OUT_RTCP_PORT,
	                                                           ms_factory_get_mtu(_factory));
	rtp_session_set_profile(rtpSession_legB, profile);
	rtp_session_set_remote_addr_and_port(rtpSession_legB, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT);
	rtp_session_set_payload_type(rtpSession_legB, 122);
	rtp_session_enable_transfer_mode(rtpSession_legB, TRUE);
	rtp_session_enable_rtcp(rtpSession_legB, FALSE);
	MSMediaStreamSessions sessions_legB;
	sessions_legB.rtp_session = rtpSession_legB;
	sessions_legB.srtp_context = NULL;
	sessions_legB.zrtp_context = NULL;
	sessions_legB.dtls_context = NULL;
	sessions_legB.ticker = ms_ticker_new();
	sessions_legB.bundledSndRtpSessions = NULL;
	if (use_ekt) {
		ms_media_stream_sessions_set_ekt_mode(&sessions_legB, MS_EKT_TRANSFER);
	} else {
		ms_media_stream_sessions_set_ekt_mode(&sessions_legB, MS_EKT_DISABLED_WITH_TRANSFER);
	}

	MSFilter *rtpsend_legB = NULL;
	ms_tester_create_filter(&rtpsend_legB, MS_RTP_SEND_ID, _factory);
	MSFilter *rtprecv_legB = NULL;
	ms_tester_create_filter(&rtprecv_legB, MS_RTP_RECV_ID, _factory);
	ms_filter_call_method(rtpsend_legB, MS_RTP_SEND_SET_SESSION, rtpSession_legB);
	ms_filter_call_method(rtpsend_legB, MS_RTP_SEND_ENABLE_RTP_TRANSFER_MODE, &trueBool);
	ms_filter_call_method(rtpsend_legB, MS_RTP_SEND_ENABLE_STUN, &falseBool);
	ms_filter_call_method(rtprecv_legB, MS_RTP_RECV_SET_SESSION, rtpSession_legB);
	ms_filter_call_method(rtprecv_legB, MS_RTP_RECV_ENABLE_RTP_TRANSFER_MODE, &trueBool);

	// plug filters
	ms_filter_link(rtprecv_legA, 0, rtpsend_legB, 0);
	ms_filter_link(rtprecv_legB, 0, rtpsend_legA, 0);

	ms_ticker_attach(sessions_legA.ticker, rtprecv_legA);
	ms_ticker_attach(sessions_legB.ticker, rtprecv_legB);

	/* Marielle is the original source, she send audio stream to Pauline, gets her source from hello_file */
	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, PAULINE_IP, PAULINE_IN_RTP_PORT, PAULINE_IP,
	                                        PAULINE_IN_RTCP_PORT, 121, 50, hello_file, NULL, NULL, NULL, 0),
	                0, int, "%d");

	if (encryption_mandatory) {
		/*wait a bit to make sure packets are discarded*/
		wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);
		audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);
		audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
		BC_ASSERT_EQUAL(margaux_stats.rtp.recv, 0, unsigned long long, "%llu");
		number_of_dropped_packets = marielle_stats.rtp.packet_sent;
	}

	/* set marielle outer key outer */
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), outer_suite, outer_key,
	                                                              MSSrtpKeySourceSDES) == 0);

	/* set pauline keys: receive from marielle (outer key) and send using her own key (outer_relay_key)*/
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&sessions_legA, outer_suite, outer_key,
	                                                              MSSrtpKeySourceSDES) == 0);
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&sessions_legB, outer_suite, outer_relay_key,
	                                                              MSSrtpKeySourceDTLS) == 0);

	/* set margaux recv keys: inner and outer */
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(margaux->ms.sessions), outer_suite, outer_relay_key,
	                                                              MSSrtpKeySourceSDES) == 0);

	/* set inner keys */
	if (use_ekt) {
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_ekt(&(marielle->ms.sessions), &ekt_params) == 0);
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_ekt(&(margaux->ms.sessions), &ekt_params) == 0);
	} else {
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_recv_key_b64(
		                   &(margaux->ms.sessions), inner_suite, inner_key, MSSrtpKeySourceDTLS,
		                   marielle->ms.sessions.rtp_session->snd.ssrc) == 0);
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_send_key_b64(&(marielle->ms.sessions), inner_suite,
		                                                                    inner_key, MSSrtpKeySourceDTLS) == 0);
	}

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);
	BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000));
	audio_stream_play(marielle, NULL);

	/*make sure packets can cross from sender to receiver*/
	wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1500);

	if (participant_volume) {
		BC_ASSERT_EQUAL(audio_stream_get_participant_volume(margaux, 1), (int)ms_volume_dbov_to_dbm0(-5), int, "%d");
		BC_ASSERT_EQUAL(audio_stream_get_participant_volume(margaux, 2), (int)ms_volume_dbov_to_dbm0(-15), int, "%d");
		BC_ASSERT_EQUAL(audio_stream_get_participant_volume(margaux, 3), (int)ms_volume_dbov_to_dbm0(-127), int, "%d");
	}

	/* check SRTP stats */
	BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)marielle, MediaStreamSendOnly, FALSE) ==
	               MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)margaux, MediaStreamRecvOnly, FALSE) ==
	               MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)marielle, MediaStreamSendOnly, FALSE) ==
	               outer_suite);
	BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)margaux, MediaStreamRecvOnly, FALSE) ==
	               outer_suite);
	if (use_ekt) {
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)marielle, MediaStreamSendOnly, TRUE) ==
		               MSSrtpKeySourceEKT);
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)margaux, MediaStreamRecvOnly, TRUE) ==
		               MSSrtpKeySourceEKT);
	} else {
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)marielle, MediaStreamSendOnly, TRUE) ==
		               MSSrtpKeySourceDTLS);
		BC_ASSERT_TRUE(media_stream_get_srtp_key_source((MediaStream *)margaux, MediaStreamRecvOnly, TRUE) ==
		               MSSrtpKeySourceDTLS);
	}
	BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)marielle, MediaStreamSendOnly, TRUE) ==
	               inner_suite);
	BC_ASSERT_TRUE(media_stream_get_srtp_crypto_suite((MediaStream *)margaux, MediaStreamRecvOnly, TRUE) ==
	               inner_suite);
	BC_ASSERT_FALSE(media_stream_secured((MediaStream *)marielle));
	BC_ASSERT_FALSE(media_stream_secured((MediaStream *)margaux));

	audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);

	/* No packet loss is assumed */
	BC_ASSERT_EQUAL(marielle_stats.rtp.packet_sent, margaux_stats.rtp.packet_recv + number_of_dropped_packets,
	                unsigned long long, "%llu");

	/* cleaning */
	ms_ticker_detach(sessions_legA.ticker, rtprecv_legA);
	ms_ticker_detach(sessions_legB.ticker, rtprecv_legB);
	ms_filter_unlink(rtprecv_legA, 0, rtpsend_legB, 0);
	ms_filter_unlink(rtprecv_legB, 0, rtpsend_legA, 0);
	ms_tester_destroy_filter(&rtpsend_legA);
	ms_tester_destroy_filter(&rtpsend_legB);
	ms_tester_destroy_filter(&rtprecv_legA);
	ms_tester_destroy_filter(&rtprecv_legB);
	audio_stream_stop(marielle);
	ms_media_stream_sessions_uninit(&sessions_legA);
	ms_media_stream_sessions_uninit(&sessions_legB);
	audio_stream_stop(margaux);
	rtp_profile_destroy(profile);

	/* compare audio files (only when encryption is not mandatory as in this case we might miss part of the file)*/
	if (!encryption_mandatory) {
		BC_ASSERT_EQUAL(ms_audio_diff(hello_file, recorded_file, &similar, &audio_cmp_params, NULL, NULL), 0, int,
		                "%d");
		BC_ASSERT_GREATER(similar, threshold, double, "%f");
		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
	}

	unlink(recorded_file);
	free(recorded_file);
	free(hello_file);
}

static void double_encrypted_relayed_audio_stream(void) {
	double_encrypted_rtp_relay_audio_stream_base(FALSE, FALSE, FALSE, MS_AES_128_SHA1_32, MS_AES_128_SHA1_32);
	double_encrypted_rtp_relay_audio_stream_base(FALSE, FALSE, FALSE, MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM);
}

static void double_encrypted_relayed_audio_stream_encryption_mandatory(void) {
	double_encrypted_rtp_relay_audio_stream_base(TRUE, FALSE, FALSE, MS_AES_128_SHA1_32, MS_AES_128_SHA1_32);
	double_encrypted_rtp_relay_audio_stream_base(TRUE, FALSE, FALSE, MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM);
}

static void double_encrypted_relayed_audio_stream_with_participants_volumes(void) {
	double_encrypted_rtp_relay_audio_stream_base(FALSE, TRUE, FALSE, MS_AES_128_SHA1_32, MS_AES_128_SHA1_32);
	double_encrypted_rtp_relay_audio_stream_base(FALSE, TRUE, FALSE, MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM);
}

static void double_encrypted_relayed_audio_stream_use_ekt(void) {
	double_encrypted_rtp_relay_audio_stream_base(FALSE, FALSE, TRUE, MS_AES_128_SHA1_32, MS_AES_128_SHA1_32);
	double_encrypted_rtp_relay_audio_stream_base(FALSE, FALSE, TRUE, MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM);
}

static void double_encrypted_relayed_audio_stream_with_participants_volumes_use_ekt(void) {
	double_encrypted_rtp_relay_audio_stream_base(FALSE, TRUE, TRUE, MS_AES_128_SHA1_32, MS_AES_128_SHA1_32);
	double_encrypted_rtp_relay_audio_stream_base(FALSE, TRUE, TRUE, MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM);
}

static void voice_activity_detection(void) {
	basic_audio_stream_base_2(MARIELLE_IP, MARGAUX_IP, MARIELLE_RTP_PORT, MARGAUX_RTP_PORT, MARIELLE_RTCP_PORT,
	                          MARGAUX_RTCP_PORT, MARGAUX_IP, MARIELLE_IP, MARGAUX_RTP_PORT, MARIELLE_RTP_PORT,
	                          MARGAUX_RTCP_PORT, MARIELLE_RTCP_PORT, 0, TRUE);
}

static test_t tests[] = {
    TEST_NO_TAG("Basic audio stream", basic_audio_stream),
    TEST_NO_TAG("Multicast audio stream", multicast_audio_stream),
    TEST_NO_TAG("Encrypted audio stream", encrypted_audio_stream),
    TEST_NO_TAG("Encrypted audio stream with 2 srtp context", encrypted_audio_stream_with_2_srtp_stream),
    TEST_NO_TAG("Encrypted audio stream with 2 srtp context, recv first",
                encrypted_audio_stream_with_2_srtp_stream_recv_first),
    TEST_NO_TAG("Encrypted audio stream with ssrc changes", encrypted_audio_stream_with_ssrc_change),
    TEST_NO_TAG("Encrypted audio stream with key change", encrypted_audio_stream_with_key_change),
    TEST_NO_TAG("Encrypted audio stream, encryption mandatory", encrypted_audio_stream_encryption_mandatory),
    TEST_NO_TAG("Encrypted audio stream with key change + encryption mandatory",
                encrypted_audio_stream_with_key_change_encryption_mandatory),
    TEST_NO_TAG("Double Encrypted audio stream", double_encrypted_audio_stream),
    TEST_NO_TAG("Double Encrypted audio stream with 2 srtp context", double_encrypted_audio_stream_both_streams),
    TEST_NO_TAG("Double Encrypted audio stream, encryption mandatory",
                double_encrypted_audio_stream_encryption_mandatory),
    TEST_NO_TAG("Double Encrypted audio stream with participants volumes",
                double_encrypted_audio_stream_with_participants_volumes),
    TEST_NO_TAG("Double Encrypted relayed audio stream", double_encrypted_relayed_audio_stream),
    TEST_NO_TAG("Double Encrypted relayed audio stream, encryption mandatory",
                double_encrypted_relayed_audio_stream_encryption_mandatory),
    TEST_NO_TAG("Double Encrypted relayed audio stream with participants volumes",
                double_encrypted_relayed_audio_stream_with_participants_volumes),
    TEST_NO_TAG("Double Encrypted relayed audio stream using ekt", double_encrypted_relayed_audio_stream_use_ekt),
    TEST_NO_TAG("Double Encrypted relayed audio stream with participants volumes using ekt",
                double_encrypted_relayed_audio_stream_with_participants_volumes_use_ekt),
    TEST_NO_TAG("Codec change for audio stream", codec_change_for_audio_stream),
    TEST_NO_TAG("TMMBR feedback for audio stream", tmmbr_feedback_for_audio_stream),
    TEST_NO_TAG("Symetric rtp with wrong address", symetric_rtp_with_wrong_addr),
    TEST_NO_TAG("Symetric rtp with wrong rtcp port", symetric_rtp_with_wrong_rtcp_port),
    TEST_NO_TAG("Participants volumes in audio stream", participants_volumes_in_audio_stream),
    TEST_NO_TAG("Voice activity detection", voice_activity_detection),
    TEST_NO_TAG("Auto bundle multiple audiostream in reception", multiple_audiostreams_auto_bundled),
};

test_suite_t audio_stream_test_suite = {
    "AudioStream", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0};
