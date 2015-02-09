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


static RtpProfile rtp_profile;

#define OPUS_PAYLOAD_TYPE    121
#define SPEEX16_PAYLOAD_TYPE 122
#define SILK16_PAYLOAD_TYPE  123
#define PCMA8_PAYLOAD_TYPE 8

static int tester_init(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();
	rtp_profile_set_payload (&rtp_profile,0,&payload_type_pcmu8000);
	rtp_profile_set_payload (&rtp_profile,OPUS_PAYLOAD_TYPE,&payload_type_opus);
	rtp_profile_set_payload (&rtp_profile,SPEEX16_PAYLOAD_TYPE,&payload_type_speex_wb);
	rtp_profile_set_payload (&rtp_profile,SILK16_PAYLOAD_TYPE,&payload_type_silk_wb);
	rtp_profile_set_payload (&rtp_profile,PCMA8_PAYLOAD_TYPE,&payload_type_pcma8000);

	return 0;
}

static int tester_cleanup(void) {
	ms_exit();
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
#define RECORDED_8K_1S_FILE  "sounds/recorded_hello8000-1s.wav"
#define RECORDED_16K_1S_FILE  "sounds/recorded_hello16000-1s.wav"

#define MULTICAST_IP  "224.1.2.3"

typedef struct _stats_t {
	rtp_stats_t rtp;
	int number_of_EndOfFile;
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

static void basic_audio_stream_base(	const char* marielle_local_ip
									, 	int marielle_local_rtp_port
									, 	int marielle_local_rtcp_port
									, 	const char*  margaux_local_ip
									, 	int margaux_local_rtp_port
									, 	int margaux_local_rtcp_port) {
	AudioStream * 	marielle = audio_stream_new2 (marielle_local_ip, marielle_local_rtp_port, marielle_local_rtcp_port);
	stats_t marielle_stats;
	AudioStream * 	margaux = audio_stream_new2 (margaux_local_ip, margaux_local_rtp_port,margaux_local_rtcp_port);
	stats_t margaux_stats;
	RtpProfile* profile = rtp_profile_new("default profile");
	char* hello_file = ms_strdup_printf("%s/%s", mediastreamer2_tester_get_file_root(), HELLO_8K_1S_FILE);
	char* recorded_file = ms_strdup_printf("%s/%s", mediastreamer2_tester_get_writable_dir(), RECORDED_8K_1S_FILE);
	int dummy=0;
	rtp_session_set_multicast_loopback(marielle->ms.sessions.rtp_session,TRUE);
    rtp_session_set_multicast_loopback(margaux->ms.sessions.rtp_session,TRUE);

    reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);

	rtp_profile_set_payload (profile,0,&payload_type_pcmu8000);


	CU_ASSERT_EQUAL(audio_stream_start_full(margaux
											, profile
											, ms_is_multicast(margaux_local_ip)?margaux_local_ip:marielle_local_ip
											, ms_is_multicast(margaux_local_ip)?margaux_local_rtp_port:marielle_local_rtp_port
											, marielle_local_ip
											, marielle_local_rtcp_port
											, 0
											, 50
											, NULL
											, recorded_file
											, NULL
											, NULL
											, 0),0);

	CU_ASSERT_EQUAL(audio_stream_start_full(marielle
											, profile
											, margaux_local_ip
											, margaux_local_rtp_port
											, margaux_local_ip
											, margaux_local_rtcp_port
											, 0
											, 50
											, hello_file
											, NULL
											, NULL
											, NULL
											, 0),0);

	ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats,TRUE);

	CU_ASSERT_TRUE(wait_for_until(&marielle->ms,&margaux->ms,&marielle_stats.number_of_EndOfFile,1,12000));

	/*make sure packets can cross from sender to receiver*/
	wait_for_until(&marielle->ms,&margaux->ms,&dummy,1,500);

	audio_stream_get_local_rtp_stats(marielle,&marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux,&margaux_stats.rtp);

	/* No packet loss is assumed */
	CU_ASSERT_EQUAL(marielle_stats.rtp.sent,margaux_stats.rtp.recv);

	audio_stream_stop(marielle);
	audio_stream_stop(margaux);

	unlink(recorded_file);
	ms_free(recorded_file);
	ms_free(hello_file);
}
static void basic_audio_stream()  {
	basic_audio_stream_base(MARIELLE_IP,MARIELLE_RTP_PORT,MARIELLE_RTCP_PORT
							,MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT);
}

static void multicast_audio_stream()  {
	basic_audio_stream_base("0.0.0.0",MARIELLE_RTP_PORT, 0
							,MULTICAST_IP, MARGAUX_RTP_PORT, 0);
}

static void encrypted_audio_stream_base( bool_t change_ssrc,
										 bool_t change_send_key_in_the_middle
										,bool_t set_both_send_recv_key
										,bool_t send_key_first) {
	AudioStream * 	marielle = audio_stream_new (MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT,FALSE);
	AudioStream * 	margaux = audio_stream_new (MARGAUX_RTP_PORT,MARGAUX_RTCP_PORT, FALSE);
	RtpProfile* profile = rtp_profile_new("default profile");
	char* hello_file = ms_strdup_printf("%s/%s", mediastreamer2_tester_get_file_root(), HELLO_8K_1S_FILE);
	char* recorded_file = ms_strdup_printf("%s/%s", mediastreamer2_tester_get_writable_dir(), RECORDED_8K_1S_FILE);
	stats_t marielle_stats;
	stats_t margaux_stats;
	int dummy=0;

	if (ms_srtp_supported()) {
		reset_stats(&marielle_stats);
		reset_stats(&margaux_stats);

		rtp_profile_set_payload (profile,0,&payload_type_pcmu8000);

		CU_ASSERT_EQUAL(audio_stream_start_full(margaux
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
				, 0),0);

		CU_ASSERT_EQUAL(audio_stream_start_full(marielle
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
				, 0),0);

		if (send_key_first) {
			CU_ASSERT_TRUE(media_stream_set_srtp_send_key_b64(&(marielle->ms.sessions), MS_AES_128_SHA1_32, "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj") == 0);
			if (set_both_send_recv_key)
				CU_ASSERT_TRUE(media_stream_set_srtp_send_key_b64(&(margaux->ms.sessions), MS_AES_128_SHA1_32, "6jCLmtRkVW9E/BUuJtYj/R2z6+4iEe06/DWohQ9F") == 0);

			CU_ASSERT_TRUE(media_stream_set_srtp_recv_key_b64(&(margaux->ms.sessions), MS_AES_128_SHA1_32, "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj") ==0);
			if (set_both_send_recv_key)
				CU_ASSERT_TRUE(media_stream_set_srtp_recv_key_b64(&(marielle->ms.sessions), MS_AES_128_SHA1_32, "6jCLmtRkVW9E/BUuJtYj/R2z6+4iEe06/DWohQ9F") ==0);

		} else {
			CU_ASSERT_TRUE(media_stream_set_srtp_recv_key_b64(&(margaux->ms.sessions), MS_AES_128_SHA1_32, "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj") ==0);
			if (set_both_send_recv_key)
				CU_ASSERT_TRUE(media_stream_set_srtp_recv_key_b64(&(marielle->ms.sessions), MS_AES_128_SHA1_32, "6jCLmtRkVW9E/BUuJtYj/R2z6+4iEe06/DWohQ9F") ==0);

			CU_ASSERT_TRUE(media_stream_set_srtp_send_key_b64(&(marielle->ms.sessions), MS_AES_128_SHA1_32, "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj") == 0);
			if (set_both_send_recv_key)
				CU_ASSERT_TRUE(media_stream_set_srtp_send_key_b64(&(margaux->ms.sessions), MS_AES_128_SHA1_32, "6jCLmtRkVW9E/BUuJtYj/R2z6+4iEe06/DWohQ9F") == 0);

		}

		ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats,TRUE);
		if (change_send_key_in_the_middle) {
			int dummy=0;
			wait_for_until(&marielle->ms,&margaux->ms,&dummy,1,2000);
			CU_ASSERT_TRUE(media_stream_set_srtp_send_key_b64(&(marielle->ms.sessions), MS_AES_128_SHA1_32, "eCYF4nYyCvmCpFWjUeDaxI2GWp2BzCRlIPfg52Te") == 0);
			CU_ASSERT_TRUE(media_stream_set_srtp_recv_key_b64(&(margaux->ms.sessions), MS_AES_128_SHA1_32, "eCYF4nYyCvmCpFWjUeDaxI2GWp2BzCRlIPfg52Te") ==0);
		}
		CU_ASSERT_TRUE(wait_for_until(&marielle->ms,&margaux->ms,&marielle_stats.number_of_EndOfFile,1,12000));

		/*make sure packets can cross from sender to receiver*/
		wait_for_until(&marielle->ms,&margaux->ms,&dummy,1,500);

		audio_stream_get_local_rtp_stats(marielle,&marielle_stats.rtp);
		audio_stream_get_local_rtp_stats(margaux,&margaux_stats.rtp);

		/* No packet loss is assumed */
		if (change_send_key_in_the_middle) {
			/*we can accept one or 2 error in such case*/
			CU_ASSERT_TRUE((marielle_stats.rtp.packet_sent-margaux_stats.rtp.packet_recv)<3);
		} else
			CU_ASSERT_EQUAL(marielle_stats.rtp.sent,margaux_stats.rtp.recv);

		if (change_ssrc) {
			audio_stream_stop(marielle);
			marielle = audio_stream_new (MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT,FALSE);
			CU_ASSERT_EQUAL(audio_stream_start_full(marielle
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
							, 0),0);
			CU_ASSERT_FATAL(media_stream_set_srtp_send_key_b64(&(marielle->ms.sessions), MS_AES_128_SHA1_32, "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj") == 0);

			ms_filter_add_notify_callback(marielle->soundread, notify_cb, &marielle_stats,TRUE);

			CU_ASSERT_TRUE(wait_for_until(&marielle->ms,&margaux->ms,&marielle_stats.number_of_EndOfFile,2,12000));

			/*make sure packets can cross from sender to receiver*/
			wait_for_until(&marielle->ms,&margaux->ms,&dummy,1,500);

			audio_stream_get_local_rtp_stats(marielle,&marielle_stats.rtp);
			audio_stream_get_local_rtp_stats(margaux,&margaux_stats.rtp);

			/* No packet loss is assumed */
			CU_ASSERT_EQUAL(marielle_stats.rtp.sent*2,margaux_stats.rtp.recv);

		}

		unlink(recorded_file);
		ms_free(recorded_file);
		ms_free(hello_file);
	} else {
		ms_warning("srtp not available, skiping...");
	}
	audio_stream_stop(marielle);
	audio_stream_stop(margaux);
	rtp_profile_destroy(profile);

}
static void encrypted_audio_stream() {
	encrypted_audio_stream_base(FALSE,FALSE,FALSE,TRUE);
}

static void encrypted_audio_stream_with_2_srtp_stream() {
	encrypted_audio_stream_base(FALSE,FALSE,TRUE,TRUE);
}

static void encrypted_audio_stream_with_2_srtp_stream_recv_first() {
	encrypted_audio_stream_base(FALSE,FALSE,TRUE,FALSE);
}

static void encrypted_audio_stream_with_key_change() {
	encrypted_audio_stream_base(FALSE,TRUE,FALSE,TRUE);
}

static void encrypted_audio_stream_with_ssrc_change() {
	encrypted_audio_stream_base(TRUE,FALSE,FALSE,TRUE);
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
	CU_ASSERT_TRUE(recv_send_bw_ratio>0.9);

	stream_manager_delete(marielle);
	stream_manager_delete(margaux);

}

#endif


static test_t tests[] = {
	{ "Basic audio stream", basic_audio_stream },
	{ "Multicast audio stream", multicast_audio_stream },
	{ "Encrypted audio stream", encrypted_audio_stream },
	{ "Encrypted audio stream with 2 srtp context",encrypted_audio_stream_with_2_srtp_stream},
	{ "Encrypted audio stream with 2 srtp context, recv first",encrypted_audio_stream_with_2_srtp_stream_recv_first},
	{ "Encrypted audio stream with ssrc changes", encrypted_audio_stream_with_ssrc_change},
	{ "Encrypted audio stream with key change",encrypted_audio_stream_with_key_change},
};

test_suite_t audio_stream_test_suite = {
	"AudioStream",
	tester_init,
	tester_cleanup,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
