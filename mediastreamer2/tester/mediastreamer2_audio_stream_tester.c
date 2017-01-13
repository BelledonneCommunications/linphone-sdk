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

static RtpProfile rtp_profile;
static MSFactory *_factory= NULL;


static int tester_before_all(void) {
	//ms_init();
	_factory = ms_factory_new();
	ms_factory_init_voip(_factory);
	ms_factory_init_plugins(_factory);

	//ms_filter_enable_statistics(TRUE);
	ms_factory_enable_statistics(_factory, TRUE);
	ortp_init();
	rtp_profile_set_payload (&rtp_profile,0,&payload_type_pcmu8000);
	rtp_profile_set_payload (&rtp_profile,OPUS_PAYLOAD_TYPE,&payload_type_opus);
	rtp_profile_set_payload (&rtp_profile,SPEEX16_PAYLOAD_TYPE,&payload_type_speex_wb);
	rtp_profile_set_payload (&rtp_profile,SILK16_PAYLOAD_TYPE,&payload_type_silk_wb);
	rtp_profile_set_payload (&rtp_profile,PCMA8_PAYLOAD_TYPE,&payload_type_pcma8000);
	return 0;
}

static int tester_after_all(void) {
	//ms_exit();

	ms_factory_destroy(_factory);
	rtp_profile_clear_all(&rtp_profile);
	return 0;
}

#define MARIELLE_RTP_PORT 2564
#define MARIELLE_RTCP_PORT 2565
#define MARIELLE_IP "127.0.0.1"

#define MARGAUX_RTP_PORT 9864
#define MARGAUX_RTCP_PORT 9865
#define MARGAUX_IP "127.0.0.1"

#define HELLO_8K_1S_FILE  "sounds/hello8000-1s.wav"
#define HELLO_16K_1S_FILE  "sounds/hello16000-1s.wav"
#define RECORDED_8K_1S_FILE  "recorded_hello8000-1s.wav"
#define RECORDED_16K_1S_FILE  "recorded_hello16000-1s.wav"

#define MULTICAST_IP  "224.1.2.3"

typedef struct _stats_t {
	OrtpEvQueue *q;
	rtp_stats_t rtp;
	int number_of_EndOfFile;
	int number_of_TMMBR;
} stats_t;

static void reset_stats(stats_t* s) {
	memset(s,0,sizeof(stats_t));
}


static void notify_cb(void *user_data, MSFilter *f, unsigned int event, void *eventdata) {
	stats_t* stats = (stats_t*)user_data;
	switch (event) {
		case MS_FILE_PLAYER_EOF: {
			ms_message("EndOfFile received");
			stats->number_of_EndOfFile++;
			break;
		}
		break;
	}
}

static void event_queue_cb(MediaStream *ms, void *user_pointer) {
	stats_t *st = (stats_t *)user_pointer;
	OrtpEvent *ev = NULL;

	if (st->q != NULL) {
		while ((ev = ortp_ev_queue_get(st->q)) != NULL) {
			OrtpEventType evt = ortp_event_get_type(ev);
			OrtpEventData *d = ortp_event_get_data(ev);
			if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
				do {
					if (rtcp_is_RTPFB(d->packet)) {
						switch (rtcp_RTPFB_get_type(d->packet)) {
							case RTCP_RTPFB_TMMBR:
								st->number_of_TMMBR++;
								break;
							default:
								break;
						}
					}
				} while (rtcp_next_packet(d->packet));
			}
			ortp_event_destroy(ev);
		}
	}
}

static void basic_audio_stream_base_2(	const char* marielle_local_ip
									  ,	const char* marielle_remote_ip
									  , int marielle_local_rtp_port
									  , int marielle_remote_rtp_port
									  ,	int marielle_local_rtcp_port
									  , int marielle_remote_rtcp_port
									  ,	const char*  margaux_local_ip
									  , const char*  margaux_remote_ip
									  ,	int margaux_local_rtp_port
									  , int margaux_remote_rtp_port
									  ,	int margaux_local_rtcp_port
									  , int margaux_remote_rtcp_port
									  , int lost_percentage) {
	AudioStream * 	marielle = audio_stream_new2 (_factory, marielle_local_ip, marielle_local_rtp_port, marielle_local_rtcp_port);
	stats_t marielle_stats;
	AudioStream * 	margaux = audio_stream_new2 (_factory, margaux_local_ip, margaux_local_rtp_port,margaux_local_rtcp_port);
	stats_t margaux_stats;
	RtpProfile* profile = rtp_profile_new("default profile");
	char* hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	char* recorded_file = bc_tester_file(RECORDED_8K_1S_FILE);
	uint64_t marielle_rtp_sent=0;

	rtp_session_set_multicast_loopback(marielle->ms.sessions.rtp_session,TRUE);
	rtp_session_set_multicast_loopback(margaux->ms.sessions.rtp_session,TRUE);
	rtp_session_set_rtcp_report_interval(marielle->ms.sessions.rtp_session, 1000);
	rtp_session_set_rtcp_report_interval(margaux->ms.sessions.rtp_session, 1000);

	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);

	rtp_profile_set_payload (profile,0,&payload_type_pcmu8000);

	BC_ASSERT_EQUAL(audio_stream_start_full(margaux
											, profile
											, ms_is_multicast(margaux_local_ip)?margaux_local_ip:margaux_remote_ip
											, ms_is_multicast(margaux_local_ip)?margaux_local_rtp_port:margaux_remote_rtp_port
											, margaux_remote_ip
											, margaux_remote_rtcp_port
											, 0
											, 50
											, NULL
											, recorded_file
											, NULL
											, NULL
											, 0)
					,0, int, "%d");

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle
											, profile
											, marielle_remote_ip
											, marielle_remote_rtp_port
											, marielle_remote_ip
											, marielle_remote_rtcp_port
											, 0
											, 50
											, hello_file
											, NULL
											, NULL
											, NULL
											, 0)
					,0, int, "%d");

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats,TRUE);

	wait_for_until(&marielle->ms,&margaux->ms,&marielle_stats.number_of_EndOfFile,1,12000);

	audio_stream_get_local_rtp_stats(marielle,&marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux,&margaux_stats.rtp);
	marielle_rtp_sent = marielle_stats.rtp.sent;


	if (rtp_session_rtcp_enabled(marielle->ms.sessions.rtp_session) && rtp_session_rtcp_enabled(margaux->ms.sessions.rtp_session)) {
		BC_ASSERT_GREATER_STRICT(rtp_session_get_round_trip_propagation(marielle->ms.sessions.rtp_session),0,float,"%f");
		BC_ASSERT_GREATER_STRICT(rtp_session_get_stats(marielle->ms.sessions.rtp_session)->recv_rtcp_packets,0,unsigned long long,"%llu");
	}

	audio_stream_stop(marielle);

	BC_ASSERT_TRUE(wait_for_until(&margaux->ms,NULL,(int*)&margaux_stats.rtp.hw_recv,(int)((marielle_rtp_sent*(100-lost_percentage))/100),2500));

	audio_stream_stop(margaux);

	unlink(recorded_file);
	free(recorded_file);
	free(hello_file);
	rtp_profile_destroy(profile);
}
static void basic_audio_stream_base(	const char* marielle_local_ip
									, 	int marielle_local_rtp_port
									, 	int marielle_local_rtcp_port
									, 	const char*  margaux_local_ip
									, 	int margaux_local_rtp_port
									, 	int margaux_local_rtcp_port) {
	basic_audio_stream_base_2( marielle_local_ip
							  , margaux_local_ip
							  , marielle_local_rtp_port
							  , margaux_local_rtp_port
							  , marielle_local_rtcp_port
							  , margaux_local_rtcp_port
							  , margaux_local_ip
							  , marielle_local_ip
							  , margaux_local_rtp_port
							  , marielle_local_rtp_port
							  , margaux_local_rtcp_port
							  , marielle_local_rtcp_port
							  , 0);

}
static void basic_audio_stream(void)  {
	basic_audio_stream_base(MARIELLE_IP,MARIELLE_RTP_PORT,MARIELLE_RTCP_PORT
							,MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT);
}

static void multicast_audio_stream(void)  {
	basic_audio_stream_base("0.0.0.0",MARIELLE_RTP_PORT, 0
							,MULTICAST_IP, MARGAUX_RTP_PORT, 0);
}

static void encrypted_audio_stream_base( bool_t change_ssrc,
										 bool_t change_send_key_in_the_middle
										,bool_t set_both_send_recv_key
										,bool_t send_key_first
										,bool_t encryption_mandatory,
										MSCryptoSuite suite) {
	AudioStream * 	marielle = audio_stream_new (_factory, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT,FALSE);
	AudioStream * 	margaux = audio_stream_new (_factory, MARGAUX_RTP_PORT,MARGAUX_RTCP_PORT, FALSE);
	RtpProfile* profile = rtp_profile_new("default profile");
	char* hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	char* recorded_file = bc_tester_file(RECORDED_8K_1S_FILE);
	stats_t marielle_stats;
	stats_t margaux_stats;
	int dummy=0;
	uint64_t number_of_dropped_packets=0;

	const char *aes_128_bits_send_key = "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj";
	const char *aes_128_bits_send_key_2 = "eCYF4nYyCvmCpFWjUeDaxI2GWp2BzCRlIPfg52Te";
	const char *aes_128_bits_recv_key = "6jCLmtRkVW9E/BUuJtYj/R2z6+4iEe06/DWohQ9F";

	const char *aes_256_bits_send_key = "nJNTwiMkyAu8zs0MWUiSQbnBL4M+xkWTYgrVLR2eFwZyO+ca2UqBy2Uh9pVRbA==";
	const char *aes_256_bits_send_key_2 = "N3vq6TMfvtyYpqGaEi9vAHMCzgWJvaD1PIfwEYtdEgI2ACezZo2vpOdV2YWEcQ==";
	const char *aes_256_bits_recv_key = "UKg69sFLbrA7d0hEVKMtT83R3GR3sjhE0XMqNBbQ+axoDWMP5dQNfjNuSQQHbw==";

	const char *send_key = NULL;
	const char *send_key_2 = NULL;
	const char *recv_key = NULL;

	ms_media_stream_sessions_set_encryption_mandatory(&marielle->ms.sessions,encryption_mandatory);

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
		default:
			BC_FAIL("Unsupported suite");
			return;
	}


 	if (ms_srtp_supported()) {
		reset_stats(&marielle_stats);
		reset_stats(&margaux_stats);

		rtp_profile_set_payload (profile,0,&payload_type_pcmu8000);

		BC_ASSERT_EQUAL(audio_stream_start_full(margaux
												, profile
												, MARIELLE_IP
												, MARIELLE_RTP_PORT
												, MARIELLE_IP
												, MARIELLE_RTCP_PORT
												, 0
												, 50
												, NULL
												, recorded_file
												, NULL
												, NULL
												, 0)
		,0, int, "%d");

		BC_ASSERT_EQUAL(audio_stream_start_full(marielle
												, profile
												, MARGAUX_IP
												, MARGAUX_RTP_PORT
												, MARGAUX_IP
												, MARGAUX_RTCP_PORT
												, 0
												, 50
												, hello_file
												, NULL
												, NULL
												, NULL
												, 0)
		,0, int, "%d");

		if (encryption_mandatory) {
			/*wait a bit to make sure packets are discarded*/
			wait_for_until(&marielle->ms,&margaux->ms,&dummy,1,1000);
			audio_stream_get_local_rtp_stats(margaux,&margaux_stats.rtp);
			audio_stream_get_local_rtp_stats(marielle,&marielle_stats.rtp);
			BC_ASSERT_EQUAL(margaux_stats.rtp.recv,0, unsigned long long, "%llu");
			number_of_dropped_packets=marielle_stats.rtp.packet_sent;
		}

		if (send_key_first) {
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), suite, send_key) == 0);
			if (set_both_send_recv_key)
				BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(margaux->ms.sessions), suite, recv_key) == 0);

			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(margaux->ms.sessions), suite, send_key) ==0);
			if (set_both_send_recv_key)
				BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(marielle->ms.sessions), suite, recv_key) ==0);

		} else {
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(margaux->ms.sessions), suite, send_key) ==0);
			if (set_both_send_recv_key)
				BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(marielle->ms.sessions), suite, recv_key) ==0);

			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), suite, send_key) == 0);
			if (set_both_send_recv_key)
				BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(margaux->ms.sessions), suite, recv_key) == 0);

		}

		if (set_both_send_recv_key) {
			wait_for_until(&marielle->ms,&margaux->ms,&dummy,1,1000);
			BC_ASSERT_TRUE(media_stream_secured((MediaStream*)marielle));
			BC_ASSERT_TRUE(media_stream_secured((MediaStream*)margaux));
		} else {
			/*so far, not possible to know audio stream direction*/
			BC_ASSERT_FALSE(media_stream_secured((MediaStream*)marielle));
			BC_ASSERT_FALSE(media_stream_secured((MediaStream*)margaux));
		}

		ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats,TRUE);
		if (change_send_key_in_the_middle) {
			wait_for_until(&marielle->ms,&margaux->ms,&dummy,1,2000);
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), suite, send_key_2) == 0);
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&(margaux->ms.sessions), suite, send_key_2) ==0);
		}
		BC_ASSERT_TRUE(wait_for_until(&marielle->ms,&margaux->ms,&marielle_stats.number_of_EndOfFile,1,12000));

		/*make sure packets can cross from sender to receiver*/
		wait_for_until(&marielle->ms,&margaux->ms,&dummy,1,500);

		audio_stream_get_local_rtp_stats(marielle,&marielle_stats.rtp);
		audio_stream_get_local_rtp_stats(margaux,&margaux_stats.rtp);

		/* No packet loss is assumed */
		if (change_send_key_in_the_middle) {
			/*we can accept one or 2 error in such case*/
			BC_ASSERT_TRUE((marielle_stats.rtp.packet_sent-margaux_stats.rtp.packet_recv-number_of_dropped_packets)<3);
		} else
			BC_ASSERT_EQUAL(marielle_stats.rtp.packet_sent,margaux_stats.rtp.packet_recv+number_of_dropped_packets, unsigned long long, "%llu");

		if (change_ssrc) {
			audio_stream_stop(marielle);
			marielle = audio_stream_new (_factory, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT,FALSE);
			BC_ASSERT_EQUAL(audio_stream_start_full(marielle
													, profile
													, MARGAUX_IP
													, MARGAUX_RTP_PORT
													, MARGAUX_IP
													, MARGAUX_RTCP_PORT
													, 0
													, 50
													, hello_file
													, NULL
													, NULL
													, NULL
													, 0)
			,0, int, "%d");
			BC_ASSERT(ms_media_stream_sessions_set_srtp_send_key_b64(&(marielle->ms.sessions), suite, send_key) == 0);

			ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats,TRUE);

			BC_ASSERT_TRUE(wait_for_until(&marielle->ms,&margaux->ms,&marielle_stats.number_of_EndOfFile,2,12000));

			/*make sure packets can cross from sender to receiver*/
			wait_for_until(&marielle->ms,&margaux->ms,&dummy,1,500);

			audio_stream_get_local_rtp_stats(marielle,&marielle_stats.rtp);
			audio_stream_get_local_rtp_stats(margaux,&margaux_stats.rtp);

			/* No packet loss is assumed */
			BC_ASSERT_EQUAL(marielle_stats.rtp.sent*2,margaux_stats.rtp.recv, unsigned long long, "%llu");

		}

		unlink(recorded_file);
		free(recorded_file);
		free(hello_file);
	} else {
		ms_warning("srtp not available, skiping...");
	}
	audio_stream_stop(marielle);
	audio_stream_stop(margaux);
	rtp_profile_destroy(profile);
}

static void encrypted_audio_stream(void) {
	encrypted_audio_stream_base(FALSE, FALSE, FALSE, TRUE,FALSE,MS_AES_128_SHA1_32);
	encrypted_audio_stream_base(FALSE, FALSE, FALSE, TRUE,FALSE,MS_AES_256_SHA1_80);
}

static void encrypted_audio_stream_with_2_srtp_stream(void) {
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, TRUE,FALSE,MS_AES_128_SHA1_32);
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, TRUE,FALSE,MS_AES_256_SHA1_80);
}

static void encrypted_audio_stream_with_2_srtp_stream_recv_first(void) {
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, FALSE,FALSE,MS_AES_128_SHA1_32);
}

static void encrypted_audio_stream_with_key_change(void) {
	encrypted_audio_stream_base(FALSE, TRUE, FALSE, TRUE,FALSE,MS_AES_128_SHA1_32);
}

static void encrypted_audio_stream_with_ssrc_change(void) {
	encrypted_audio_stream_base(TRUE, FALSE, FALSE, TRUE,FALSE,MS_AES_128_SHA1_32);
}
static void encrypted_audio_stream_encryption_mandatory(void) {
	encrypted_audio_stream_base(FALSE, FALSE, TRUE, TRUE,TRUE,MS_AES_128_SHA1_32);
}

static void encrypted_audio_stream_with_key_change_encryption_mandatory(void) {
	encrypted_audio_stream_base(FALSE, TRUE, FALSE, TRUE,TRUE,MS_AES_128_SHA1_32);
}

static void codec_change_for_audio_stream(void) {
	AudioStream *marielle = audio_stream_new2(_factory, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT);
	stats_t marielle_stats;
	AudioStream *margaux = audio_stream_new2(_factory, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT);
	stats_t margaux_stats;
	RtpProfile *profile = rtp_profile_new("default profile");
	char* hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	char* recorded_file = bc_tester_file(RECORDED_8K_1S_FILE);
	uint64_t marielle_rtp_sent = 0;
	int dummy=0;

	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);

	rtp_profile_set_payload(profile, 0, &payload_type_pcmu8000);
	rtp_profile_set_payload(profile, 8, &payload_type_pcma8000);

	BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_IP, MARIELLE_RTCP_PORT,
		0, 50, NULL, recorded_file, NULL, NULL, 0), 0, int, "%d");

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP, MARGAUX_RTCP_PORT,
		0, 50, hello_file, NULL, NULL, NULL, 0), 0, int, "%d");

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);

	BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000));

	/*make sure packets can cross from sender to receiver*/
	wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 500);

	audio_stream_get_local_rtp_stats(marielle, &marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux, &margaux_stats.rtp);

	/* No packet loss is assumed */
	BC_ASSERT_EQUAL(marielle_stats.rtp.sent, margaux_stats.rtp.recv, unsigned long long, "%llu");
	marielle_rtp_sent = marielle_stats.rtp.sent;

	audio_stream_stop(marielle);
	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);
	marielle = audio_stream_new2(_factory, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT);
	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP, MARGAUX_RTCP_PORT,
		8, 50, hello_file, NULL, NULL, NULL, 0), 0, int, "%d");

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);

	BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000));

	/*make sure packets can cross from sender to receiver*/
	wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 500);

	audio_stream_get_local_rtp_stats(marielle,&marielle_stats.rtp);
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
	char* hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	int dummy=0;

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

	BC_ASSERT_EQUAL(audio_stream_start_full(margaux, profile, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_IP, MARIELLE_RTCP_PORT,
		0, 50, hello_file, NULL, NULL, NULL, 0), 0, int, "%d");

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP, MARGAUX_RTCP_PORT,
		0, 50, hello_file, NULL, NULL, NULL, 0), 0, int, "%d");

	ms_filter_add_notify_callback(margaux->soundread, notify_cb, &margaux_stats, TRUE);
	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats, TRUE);

	/* Wait for 1s so that some RTP packets are exchanged before sending the TMMBR. */
	wait_for_until(&margaux->ms, &marielle->ms, &dummy, 1, 500);

	rtp_session_send_rtcp_fb_tmmbr(margaux_session, 100000);
	rtp_session_send_rtcp_fb_tmmbr(marielle_session, 200000);

	BC_ASSERT_TRUE(wait_for_until(&margaux->ms, &marielle->ms, &margaux_stats.number_of_EndOfFile, 1, 12000));
	BC_ASSERT_TRUE(wait_for_until(&marielle->ms, &margaux->ms, &marielle_stats.number_of_EndOfFile, 1, 12000));

	BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->ms, &margaux->ms, &marielle_stats.number_of_TMMBR, 1, 100, event_queue_cb, &marielle_stats, event_queue_cb, &margaux_stats));
	BC_ASSERT_TRUE(wait_for_until_with_parse_events(&margaux->ms, &marielle->ms, &margaux_stats.number_of_TMMBR, 1, 100, event_queue_cb, &margaux_stats, event_queue_cb, &marielle_stats));
	BC_ASSERT_EQUAL(marielle_stats.number_of_TMMBR, 1, int, "%d");
	BC_ASSERT_EQUAL(margaux_stats.number_of_TMMBR, 1, int, "%d");

	/*make sure packets can cross from sender to receiver*/
	wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 500);

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

static void symetric_rtp_with_wrong_addr(void)  {
	basic_audio_stream_base_2(  MARIELLE_IP
							  , "10.10.10.10" /*dummy ip*/
							  , MARIELLE_RTP_PORT
							  , MARGAUX_RTP_PORT
							  , MARIELLE_RTCP_PORT
							  , MARGAUX_RTCP_PORT

							  , MARGAUX_IP
							  , MARIELLE_IP
							  , MARGAUX_RTP_PORT
							  , MARIELLE_RTP_PORT
							  , MARGAUX_RTCP_PORT
							  , MARIELLE_RTCP_PORT
							  ,5);
}

static void symetric_rtp_with_wrong_rtcp_port(void)  {
	basic_audio_stream_base_2(  MARIELLE_IP
							  , MARGAUX_IP
							  , MARIELLE_RTP_PORT
							  , MARGAUX_RTP_PORT
							  , MARIELLE_RTCP_PORT
							  , MARGAUX_RTCP_PORT

							  , MARGAUX_IP
							  , MARIELLE_IP
							  , MARGAUX_RTP_PORT
							  , MARIELLE_RTP_PORT
							  , MARGAUX_RTCP_PORT
							  , MARIELLE_RTCP_PORT +10 /*dummy port*/
							  ,5);
}


static test_t tests[] = {
	TEST_NO_TAG("Basic audio stream", basic_audio_stream),
	TEST_NO_TAG("Multicast audio stream", multicast_audio_stream),
	TEST_NO_TAG("Encrypted audio stream", encrypted_audio_stream),
	TEST_NO_TAG("Encrypted audio stream with 2 srtp context", encrypted_audio_stream_with_2_srtp_stream),
	TEST_NO_TAG("Encrypted audio stream with 2 srtp context, recv first", encrypted_audio_stream_with_2_srtp_stream_recv_first),
	TEST_NO_TAG("Encrypted audio stream with ssrc changes", encrypted_audio_stream_with_ssrc_change),
	TEST_NO_TAG("Encrypted audio stream with key change", encrypted_audio_stream_with_key_change),
	TEST_NO_TAG("Encrypted audio stream, encryption mandatory", encrypted_audio_stream_encryption_mandatory),
	TEST_NO_TAG("Encrypted audio stream with key change + encryption mandatory", encrypted_audio_stream_with_key_change_encryption_mandatory),
	TEST_NO_TAG("Codec change for audio stream", codec_change_for_audio_stream),
	TEST_NO_TAG("TMMBR feedback for audio stream", tmmbr_feedback_for_audio_stream),
	TEST_NO_TAG("Symetric rtp with wrong address", symetric_rtp_with_wrong_addr),
	TEST_NO_TAG("Symetric rtp with wrong rtcp port", symetric_rtp_with_wrong_rtcp_port),
};

test_suite_t audio_stream_test_suite = {
	"AudioStream",
	tester_before_all,
	tester_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
