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


#ifdef _MSC_VER
#define unlink _unlink
#endif

static RtpProfile rtp_profile;

#define OPUS_PAYLOAD_TYPE    121
#define SPEEX16_PAYLOAD_TYPE 122
#define SILK16_PAYLOAD_TYPE  123
#define ISAC16_PAYLOAD_TYPE  124

static int tester_init(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();
	rtp_profile_set_payload (&rtp_profile,0,&payload_type_pcmu8000);
	rtp_profile_set_payload (&rtp_profile,OPUS_PAYLOAD_TYPE,&payload_type_opus);
	rtp_profile_set_payload (&rtp_profile,SPEEX16_PAYLOAD_TYPE,&payload_type_speex_wb);
	rtp_profile_set_payload (&rtp_profile,SILK16_PAYLOAD_TYPE,&payload_type_silk_wb);
	rtp_profile_set_payload (&rtp_profile,ISAC16_PAYLOAD_TYPE,&payload_type_isac);

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

#define HELLO_8K_1S_FILE SOUND_FILE_PATH "hello8000-1s.wav"
#define HELLO_16K_1S_FILE SOUND_FILE_PATH "hello16000-1s.wav"
#define RECORDED_8K_1S_FILE WRITE_FILE_PATH "recorded_hello8000-1s.wav"
#define RECORDED_16K_1S_FILE WRITE_FILE_PATH "recorded_hello16000-1s.wav"

typedef struct _stats_t {
	rtp_stats_t rtp;
	int number_of_EndOfFile;
} stats_t;
static void reset_stats(stats_t* s) {
	memset(s,0,sizeof(stats_t));
}


bool_t wait_for_list(MSList* mss,int* counter,int value,int timeout_ms) {
	int retry=0;
	MSList* iterator;
	while (*counter<value && retry++ <timeout_ms/100) {
		 for (iterator=mss;iterator!=NULL;iterator=iterator->next) {
			 MediaStream* stream = (MediaStream*)(iterator->data);
			 media_stream_iterate(stream);
			 if (retry%10==0) {
				 ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec"	, stream
																					, media_stream_get_down_bw(stream)/1000
																					, media_stream_get_up_bw(stream)/1000);

			 }
		 }
		ms_usleep(100000);

	}
	if(*counter<value) return FALSE;
	else return TRUE;
}

bool_t wait_for_until(MediaStream* ms_1, MediaStream* ms_2,int* counter,int value,int timeout) {
	MSList* mss=NULL;
	bool_t result;
	if (ms_1)
		mss=ms_list_append(mss,ms_1);
	if (ms_2)
		mss=ms_list_append(mss,ms_2);
	result=wait_for_list(mss,counter,value,timeout);
	ms_list_free(mss);
	return result;
}
bool_t wait_for(MediaStream* ms_1, MediaStream* ms_2,int* counter,int value)  {
	return wait_for_until( ms_1, ms_2,counter,value,2000);
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

typedef struct _stream_manager_t {
	AudioStream* stream;
	int local_rtp;
	int local_rtcp;
	stats_t stats;

} stream_manager_t ;
static stream_manager_t * stream_manager_new() {
	stream_manager_t * mgr =  ms_new0(stream_manager_t,1);
	mgr->local_rtp= (rand() % ((2^16)-1024) + 1024) & ~0x1;
	mgr->local_rtcp=mgr->local_rtp+1;
	mgr->stream = audio_stream_new (mgr->local_rtp, mgr->local_rtcp,FALSE);
	return mgr;

}
static void stream_manager_delete(stream_manager_t * mgr) {
	audio_stream_stop(mgr->stream);
	ms_free(mgr);
}


static void stream_manager_start(	stream_manager_t * mgr
									,int payload_type
									,int remote_port
									,int target_bitrate
									,const char* player_file
									,const char* recorder_file) {
	media_stream_set_target_network_bitrate(&mgr->stream->ms,target_bitrate);
	CU_ASSERT_EQUAL(audio_stream_start_full(mgr->stream
												, &rtp_profile
												, "127.0.0.1"
												, remote_port
												, "127.0.0.1"
												, remote_port+1
												, payload_type
												, 50
												, player_file
												, recorder_file
												, NULL
												, NULL
												, 0),0);

}
static void basic_audio_stream() {
	AudioStream * 	marielle = audio_stream_new (MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT,FALSE);
	stats_t marielle_stats;
	AudioStream * 	margaux = audio_stream_new (MARGAUX_RTP_PORT,MARGAUX_RTCP_PORT, FALSE);
	stats_t margaux_stats;
	RtpProfile* profile = rtp_profile_new("default profile");

	reset_stats(&marielle_stats);
	reset_stats(&margaux_stats);

	rtp_profile_set_payload (profile,0,&payload_type_pcmu8000);

	CU_ASSERT_EQUAL(audio_stream_start_full(marielle
											, profile
											, MARGAUX_IP
											, MARGAUX_RTP_PORT
											, MARGAUX_IP
											, MARGAUX_RTCP_PORT
											, 0
											, 50
											, HELLO_8K_1S_FILE
											, NULL
											, NULL
											, NULL
											, 0),0);

	ms_filter_set_notify_callback(marielle->soundread, notify_cb, &marielle_stats);

	CU_ASSERT_EQUAL(audio_stream_start_full(margaux
											, profile
											, MARIELLE_IP
											, MARIELLE_RTP_PORT
											, MARIELLE_IP
											, MARIELLE_RTCP_PORT
											, 0
											, 50
											, NULL
											, RECORDED_8K_1S_FILE
											, NULL
											, NULL
											, 0),0);

	CU_ASSERT_TRUE(wait_for_until(&marielle->ms,&margaux->ms,&marielle_stats.number_of_EndOfFile,1,12000));
	/* Last chance to purge jitter buffer */
	ms_usleep(100000);
	audio_stream_iterate(margaux);
	audio_stream_iterate(marielle);

	audio_stream_get_local_rtp_stats(marielle,&marielle_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux,&margaux_stats.rtp);

	/* No packet loss is assumed */
	CU_ASSERT_EQUAL(marielle_stats.rtp.sent,margaux_stats.rtp.recv);

	audio_stream_stop(marielle);
	audio_stream_stop(margaux);

	unlink(RECORDED_8K_1S_FILE);
}

#define EDGE_BW 10000

static void adaptive_audio_stream(int codec_payload, int initial_bitrate,int target_bw, int max_recv_rtcp_packet) {
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

	stream_manager_start(margaux,codec_payload, marielle->local_rtp,-1,NULL,RECORDED_16K_1S_FILE);
	rtp_session_enable_network_simulation(margaux->stream->ms.session,&params);
	rtp_session_set_rtcp_report_interval(margaux->stream->ms.session, rtcp_interval);

	wait_for_until(&marielle->stream->ms,&margaux->stream->ms,&marielle->stats.number_of_EndOfFile,10,rtcp_interval*max_recv_rtcp_packet);

	marielle_send_bw=media_stream_get_up_bw(&marielle->stream->ms);
	recv_send_bw_ratio=params.max_bandwidth/marielle_send_bw;
	ms_message("marielle sent bw= [%f] , target was [%f] recv/send [%f]",marielle_send_bw,params.max_bandwidth,recv_send_bw_ratio);
	CU_ASSERT_TRUE(recv_send_bw_ratio>0.9);

	stream_manager_delete(marielle);
	stream_manager_delete(margaux);

	unlink(RECORDED_16K_1S_FILE);
}

static void adaptive_opus_audio_stream()  {
	adaptive_audio_stream(OPUS_PAYLOAD_TYPE, 32000, EDGE_BW, 7);
}

static void adaptive_speek16_audio_stream()  {
	adaptive_audio_stream(SPEEX16_PAYLOAD_TYPE, 32000, EDGE_BW, 7);
}

static void adaptative_isac16_audio_stream() {
	bool_t supported = ms_filter_codec_supported("iSAC");
	if( supported ) {
		adaptive_audio_stream(ISAC16_PAYLOAD_TYPE, 32000, 32000, 7);
	}
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
	{ "Adaptive audio stream [opus]", adaptive_opus_audio_stream },
	{ "Adaptive audio stream [speex]", adaptive_speek16_audio_stream },
	{ "Adaptive audio stream [iSAC]", adaptative_isac16_audio_stream }
};

test_suite_t audio_stream_test_suite = {
	"AudioStream",
	tester_init,
	tester_cleanup,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
