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
#include "qosanalyzer.h"

#include <stdio.h>
#include "CUnit/Basic.h"


#ifdef _MSC_VER
#define unlink _unlink
#endif

static RtpProfile rtp_profile;

#define OPUS_PAYLOAD_TYPE    121
#define SPEEX16_PAYLOAD_TYPE 122
#define SILK16_PAYLOAD_TYPE  123
#define PCMA8_PAYLOAD_TYPE 8
#define H263_PAYLOAD_TYPE 34
#define H264_PAYLOAD_TYPE 102
#define VP8_PAYLOAD_TYPE 103

#define EDGE_BW 10000
#define THIRDGENERATION_BW 200000

static int tester_init(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();
	rtp_profile_set_payload (&rtp_profile,0,&payload_type_pcmu8000);
	rtp_profile_set_payload (&rtp_profile,OPUS_PAYLOAD_TYPE,&payload_type_opus);
	rtp_profile_set_payload (&rtp_profile,SPEEX16_PAYLOAD_TYPE,&payload_type_speex_wb);
	rtp_profile_set_payload (&rtp_profile,SILK16_PAYLOAD_TYPE,&payload_type_silk_wb);
	rtp_profile_set_payload (&rtp_profile,PCMA8_PAYLOAD_TYPE,&payload_type_pcma8000);

	rtp_profile_set_payload (&rtp_profile,H263_PAYLOAD_TYPE,&payload_type_h263);
	rtp_profile_set_payload(&rtp_profile,H264_PAYLOAD_TYPE,&payload_type_h264);
	rtp_profile_set_payload(&rtp_profile, VP8_PAYLOAD_TYPE, &payload_type_vp8);
	return 0;
}

static int tester_cleanup(void) {
	ortp_exit();
	ms_exit();
	rtp_profile_clear_all(&rtp_profile);
	return 0;
}

#define HELLO_8K_1S_FILE SOUND_FILE_PATH "hello8000-1s.wav"
#define HELLO_16K_1S_FILE SOUND_FILE_PATH "hello16000-1s.wav"
#define RECORDED_8K_1S_FILE WRITE_FILE_PATH "recorded_hello8000-1s.wav"
#define RECORDED_16K_1S_FILE WRITE_FILE_PATH "recorded_hello16000-1s.wav"

typedef struct _audio_stats_t {
	rtp_stats_t rtp;
	int number_of_EndOfFile;
} audio_stats_t;

typedef struct _video_stats_t {
	float loss;
	float rtt;
	MSQosAnalyserNetworkState network_state;
	float loss_estim;
	float congestion_bw_estim;
} video_stats_t;

typedef struct _stream_manager_t {
	StreamType type;
	int local_rtp;
	int local_rtcp;

	union{
		AudioStream* audio_stream;
		VideoStream* video_stream;
	};
	union{
		audio_stats_t audio_stats;
		video_stats_t video_stats;
	};
} stream_manager_t ;

static stream_manager_t * stream_manager_new(StreamType type) {
	stream_manager_t * mgr = ms_new0(stream_manager_t,1);
	mgr->type=type;
	mgr->local_rtp=(rand() % ((2^16)-1024) + 1024) & ~0x1;
	mgr->local_rtcp=mgr->local_rtp+1;
	if (mgr->type==AudioStreamType){
		mgr->audio_stream=audio_stream_new (mgr->local_rtp, mgr->local_rtcp,FALSE);
		memset(&mgr->audio_stats,0,sizeof(audio_stats_t));
	}else{
		mgr->video_stream=video_stream_new (mgr->local_rtp, mgr->local_rtcp,FALSE);
	}
	return mgr;
}
static void stream_manager_delete(stream_manager_t * mgr) {
	if (mgr->type==AudioStreamType){
		audio_stream_stop(mgr->audio_stream);
	}else{
		video_stream_stop(mgr->video_stream);
	}
	ms_free(mgr);
}

static void audio_manager_start(stream_manager_t * mgr
								,int payload_type
								,int remote_port
								,int target_bitrate
								,const char* player_file
								,const char* recorder_file){
	media_stream_set_target_network_bitrate(&mgr->audio_stream->ms,target_bitrate);

	CU_ASSERT_EQUAL(audio_stream_start_full(mgr->audio_stream
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

static void video_manager_start(	stream_manager_t * mgr
									,int payload_type
									,int remote_port
									,int target_bitrate
									,MSWebCam * cam) {

	media_stream_set_target_network_bitrate(&mgr->video_stream->ms,target_bitrate);

	int result=video_stream_start(mgr->video_stream
												, &rtp_profile
												, "127.0.0.1"
												, remote_port
												, "127.0.0.1"
												, remote_port+1
												, payload_type
												, 60
												, cam);
	CU_ASSERT_EQUAL(result,0);
}

static void notify_cb(void *user_data, MSFilter *f, unsigned int event, void *eventdata) {
	audio_stats_t* stats = (audio_stats_t*)user_data;
	switch (event) {
	case MS_FILE_PLAYER_EOF: {
		ms_message("EndOfFile received");
		stats->number_of_EndOfFile++;
		break;
	}
	break;
	}
}

static void basic_audio_stream() {
	stream_manager_t * marielle = stream_manager_new(AudioStreamType);
	stream_manager_t * margaux = stream_manager_new(AudioStreamType);
	RtpProfile* profile = rtp_profile_new("default profile");

	rtp_profile_set_payload (profile,0,&payload_type_pcmu8000);

	/*recorder should be initialized before sender to avoid missing the first
	emitted packets*/
	audio_manager_start(margaux, 0, marielle->local_rtp,0,0,RECORDED_8K_1S_FILE);
	audio_manager_start(marielle, 0, margaux->local_rtp,0,HELLO_8K_1S_FILE,0);

	ms_filter_add_notify_callback(marielle->audio_stream->soundread, notify_cb,
		&marielle->audio_stats,TRUE);

	CU_ASSERT_TRUE(wait_for_until(&marielle->audio_stream->ms,&margaux->audio_stream->ms,
		&marielle->audio_stats.number_of_EndOfFile,1,12000));
	audio_stream_get_local_rtp_stats(marielle->audio_stream,&marielle->audio_stats.rtp);
	audio_stream_get_local_rtp_stats(margaux->audio_stream,&margaux->audio_stats.rtp);

	/* No packet loss is assumed */
	CU_ASSERT_EQUAL(marielle->audio_stats.rtp.sent,margaux->audio_stats.rtp.recv);

	stream_manager_delete(marielle);
	stream_manager_delete(margaux);

	unlink(RECORDED_8K_1S_FILE);
}

static void handle_queue_events(stream_manager_t * stream_mgr, OrtpEvQueue * evq) {
	OrtpEvent *ev;
	while (NULL != (ev=ortp_ev_queue_get(evq))){
		OrtpEventType evt=ortp_event_get_type(ev);
		OrtpEventData *evd=ortp_event_get_data(ev);

		if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
			const report_block_t *rb=NULL;
			if (rtcp_is_SR(evd->packet)){
				rb=rtcp_SR_get_report_block(evd->packet,0);
			}else if (rtcp_is_RR(evd->packet)){
				rb=rtcp_RR_get_report_block(evd->packet,0);
			}

			if (rb && stream_mgr->type==VideoStreamType) {
				if (stream_mgr->video_stream->ms.use_rc){
					const MSQosAnalyser *analyser=ms_bitrate_controller_get_qos_analyser(stream_mgr->video_stream->ms.rc);
					if (analyser->type==Stateful){
						const MSStatefulQosAnalyser *stateful_analyser=((const MSStatefulQosAnalyser*)analyser);
						stream_mgr->video_stats.network_state=stateful_analyser->network_state;
						stream_mgr->video_stats.loss_estim =100*stateful_analyser->network_loss_rate;
						stream_mgr->video_stats.congestion_bw_estim =stateful_analyser->congestion_bandwidth;
					}
				}

				stream_mgr->video_stats.loss=100.0*(float)report_block_get_fraction_lost(rb)/256.0;
				stream_mgr->video_stats.rtt=rtp_session_get_round_trip_propagation(stream_mgr->video_stream->ms.sessions.rtp_session);

				ms_message("mediastreamer2_video_stream_tester: received RTCP packet: loss=%f, RTT=%f, network_state=%d"
					,stream_mgr->video_stats.loss
					,stream_mgr->video_stats.rtt
					,stream_mgr->video_stats.network_state);
			}
		}
		ortp_event_destroy(ev);
	}
}

static void start_adaptive_stream(stream_manager_t * marielle, stream_manager_t * margaux,
	int payload, int initial_bitrate, int target_bw, float loss_rate, int latency,
	int max_recv_rtcp_packet, float dup_ratio) {

	OrtpNetworkSimulatorParams params={0};
	params.enabled=TRUE;
	params.loss_rate=loss_rate;
	params.max_bandwidth=target_bw;
	params.latency=latency;
	int pause_time=0;
	int* current=NULL;
	int expected;

	MediaStream *marielle_ms,*margaux_ms;
	if (marielle->type == AudioStreamType){
		marielle_ms=&marielle->audio_stream->ms;
		margaux_ms=&margaux->audio_stream->ms;
	}else{
		marielle_ms=&marielle->video_stream->ms;
		margaux_ms=&margaux->video_stream->ms;
	}

	MSWebCam * marielle_webcam=ms_web_cam_manager_get_default_cam (ms_web_cam_manager_get());

	media_stream_enable_adaptive_bitrate_control(marielle_ms,TRUE);
	rtp_session_set_duplication_ratio(marielle_ms->sessions.rtp_session, dup_ratio);

	if (marielle->type == AudioStreamType){
		audio_manager_start(marielle,payload, margaux->local_rtp,initial_bitrate,HELLO_16K_1S_FILE,NULL);
		ms_filter_call_method(marielle->audio_stream->soundread,MS_FILE_PLAYER_LOOP,&pause_time);

		audio_manager_start(margaux,payload, marielle->local_rtp,-1,NULL,RECORDED_16K_1S_FILE);

		current=&marielle->audio_stats.number_of_EndOfFile;
		expected=10;
	}else{
		video_manager_start(marielle, payload, margaux->local_rtp, initial_bitrate, marielle_webcam);
		video_stream_set_direction(margaux->video_stream,VideoStreamRecvOnly);

		video_manager_start(margaux, payload, marielle->local_rtp, -1, NULL);
	}
	rtp_session_enable_network_simulation(margaux_ms->sessions.rtp_session,&params);

	OrtpEvQueue * evq=ortp_ev_queue_new();
	rtp_session_register_event_queue(marielle_ms->sessions.rtp_session,evq);


	int retry=0;
	int packets_after_start=max_recv_rtcp_packet - (15000.0/2500);
	int timeout_ms=((packets_after_start > 0) ? 15000 + (packets_after_start + .5) * 5000 : (max_recv_rtcp_packet + .5) * 2500);
	while ((!current||*current<expected) && retry++ <timeout_ms/100) {
		media_stream_iterate(marielle_ms);
		media_stream_iterate(margaux_ms);
		handle_queue_events(marielle, evq);
		if (retry%10==0) {
			 ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec"	,
			 	marielle_ms, media_stream_get_down_bw(marielle_ms)/1000, media_stream_get_up_bw(marielle_ms)/1000);
			 ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec"	,
			 	margaux_ms, media_stream_get_down_bw(margaux_ms)/1000, media_stream_get_up_bw(margaux_ms)/1000);
		 }
		ms_usleep(100000);
	}

	rtp_session_unregister_event_queue(marielle_ms->sessions.rtp_session,evq);
	ortp_ev_queue_destroy(evq);

	if (marielle->type == AudioStreamType){
		unlink(RECORDED_16K_1S_FILE);
	}
}

#define INIT(type) \
	marielle=stream_manager_new(type); \
	margaux=stream_manager_new(type);

#define DEINIT() \
	stream_manager_delete(marielle); \
	stream_manager_delete(margaux);

static void adaptive_opus_audio_stream()  {
	bool_t supported = ms_filter_codec_supported("opus");
	if( supported ) {
		// at 8KHz -> 24kb/s
		// at 48KHz -> 48kb/s
		float bw_usage;
		stream_manager_t * marielle, * margaux;

		// on EDGEBW, both should be overconsumming
		INIT(AudioStreamType);
		start_adaptive_stream(marielle, margaux, OPUS_PAYLOAD_TYPE, 8000, EDGE_BW, 0, 0, 14, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms);
		CU_ASSERT_IN_RANGE(bw_usage, 2.f, 3.f); // bad! since this codec cant change its ptime and it is the lower bitrate, no improvement can occur
		DEINIT();

		INIT(AudioStreamType);
		start_adaptive_stream(marielle, margaux, OPUS_PAYLOAD_TYPE, 48000, EDGE_BW, 0, 0, 11, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms);
		CU_ASSERT_IN_RANGE(bw_usage, 1.f, 1.4f); // bad!
		DEINIT();

		// on 3G BW, both should be at max
		INIT(AudioStreamType);
		start_adaptive_stream(marielle, margaux, OPUS_PAYLOAD_TYPE, 8000, THIRDGENERATION_BW, 0, 0, 5, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms);
		CU_ASSERT_IN_RANGE(bw_usage, .1f, .15f);
		DEINIT();

		INIT(AudioStreamType);
		start_adaptive_stream(marielle, margaux, OPUS_PAYLOAD_TYPE, 48000, THIRDGENERATION_BW, 0, 0, 5, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms);
		CU_ASSERT_IN_RANGE(bw_usage, .2f, .3f);
		DEINIT();
	}
}

static void adaptive_speex16_audio_stream()  {
	bool_t supported = ms_filter_codec_supported("speex");
	if( supported ) {
		// at 16KHz -> 20 kb/s
		// at 32KHz -> 30 kb/s
		float bw_usage;
		stream_manager_t * marielle, * margaux;

		INIT(AudioStreamType);
		start_adaptive_stream(marielle, margaux, SPEEX16_PAYLOAD_TYPE, 32000, EDGE_BW / 2., 0, 0, 10, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms);
		CU_ASSERT_IN_RANGE(bw_usage, 1.f, 5.f);
		DEINIT();
	}
}

static void adaptive_pcma_audio_stream() {
	bool_t supported = ms_filter_codec_supported("pcma");
	if( supported ) {
		// at 8KHz -> 80 kb/s
		float bw_usage;
		stream_manager_t * marielle, * margaux;

		// yet non-adaptative codecs cannot respect low throughput limitations
		INIT(AudioStreamType);
		start_adaptive_stream(marielle, margaux, PCMA8_PAYLOAD_TYPE, 8000, EDGE_BW, 0, 0, 10, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms);
		CU_ASSERT_IN_RANGE(bw_usage,6.f, 8.f); // this is bad!
		DEINIT();

		INIT(AudioStreamType);
		start_adaptive_stream(marielle, margaux, PCMA8_PAYLOAD_TYPE, 8000, THIRDGENERATION_BW, 0, 0, 5, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms);
		CU_ASSERT_IN_RANGE(bw_usage, .3f, .5f);
		DEINIT();
	}
}

static void upload_bandwidth_computation() {
	bool_t supported = ms_filter_codec_supported("pcma");
	if( supported ) {
		stream_manager_t * marielle, * margaux;

		INIT(AudioStreamType);
		start_adaptive_stream(marielle, margaux, PCMA8_PAYLOAD_TYPE, 8000, 0, 0, 0, 10, 0);
		DEINIT();
	}
}

static void stability_network_detection() {
	stream_manager_t * marielle, * margaux;
	INIT(VideoStreamType);
	start_adaptive_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 0, 0, 500, 10, 0);
	CU_ASSERT_EQUAL(marielle->video_stats.network_state, MSQosAnalyserNetworkFine);
	DEINIT();

	INIT(VideoStreamType);
	start_adaptive_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 70000, 0, 250, 10,0);
	CU_ASSERT_EQUAL(marielle->video_stats.network_state, MSQosAnalyserNetworkCongested);
	DEINIT();

	INIT(VideoStreamType);
	start_adaptive_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 0, 15, 250, 10,0);
	CU_ASSERT_EQUAL(marielle->video_stats.network_state, MSQosAnalyserNetworkLossy);
	DEINIT();
}

static void adaptive_vp8() {
	stream_manager_t * marielle, * margaux;

	INIT(VideoStreamType);
	start_adaptive_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 0, 25, 50, 16,0);
	CU_ASSERT_IN_RANGE(marielle->video_stats.loss_estim, 20, 30);
	CU_ASSERT_IN_RANGE(marielle->video_stats.congestion_bw_estim, 200, 1000);
	DEINIT();

	/*very low bandwidth cause a lot of packets to be dropped since congestion is
	always present even if we are below the limit due to encoding variance*/
	INIT(VideoStreamType);
	start_adaptive_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 40000, 0, 50, 16,0);
	CU_ASSERT_IN_RANGE(marielle->video_stats.loss_estim, 0, 15);
	CU_ASSERT_IN_RANGE(marielle->video_stats.congestion_bw_estim, 20, 65);
	DEINIT();

	INIT(VideoStreamType);
	start_adaptive_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 70000,0, 50, 16,0);
	CU_ASSERT_IN_RANGE(marielle->video_stats.loss_estim, 0, 10);
	CU_ASSERT_IN_RANGE(marielle->video_stats.congestion_bw_estim, 50, 95);
	DEINIT();

	INIT(VideoStreamType);
	start_adaptive_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 100000,15, 50, 16,0);
	CU_ASSERT_IN_RANGE(marielle->video_stats.loss_estim, 10, 20);
	CU_ASSERT_IN_RANGE(marielle->video_stats.congestion_bw_estim, 80, 125);
	DEINIT();
}

static void packet_duplication() {
	const rtp_stats_t *stats;
	double dup_ratio;
	stream_manager_t * marielle, * margaux;

	INIT(VideoStreamType);
	dup_ratio = 0;
	start_adaptive_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 0, 0, 50, 2, dup_ratio);
	stats=rtp_session_get_stats(margaux->video_stream->ms.sessions.rtp_session);
	CU_ASSERT_EQUAL(stats->duplicated, dup_ratio ? stats->packet_recv / (dup_ratio+1) : 0);
	/*in theory, cumulative loss should be the invert of duplicated count, but
	since cumulative loss is computed only on received RTCP report and duplicated
	count is updated on each RTP packet received, we cannot accurately verify the values*/
	CU_ASSERT_TRUE(stats->cum_packet_loss <= -.5*stats->duplicated);
	DEINIT();

	INIT(VideoStreamType);
	dup_ratio = 1;
	start_adaptive_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 0, 0, 50, 2, dup_ratio);
	stats=rtp_session_get_stats(margaux->video_stream->ms.sessions.rtp_session);
	CU_ASSERT_EQUAL(stats->duplicated, dup_ratio ? stats->packet_recv / (dup_ratio+1) : 0);
	CU_ASSERT_TRUE(stats->cum_packet_loss <= -.5*stats->duplicated);
	DEINIT();
}

static test_t tests[] = {
	{ "Basic audio stream", basic_audio_stream },
	{ "Packet duplication", packet_duplication},
	{ "Adaptive audio stream [opus]", adaptive_opus_audio_stream },
	{ "Adaptive audio stream [speex]", adaptive_speex16_audio_stream },
	{ "Adaptive audio stream [pcma]", adaptive_pcma_audio_stream },
	{ "Upload bandwidth computation", upload_bandwidth_computation },
	{ "Stability detection", stability_network_detection },
	{ "Adaptive video stream [VP8]", adaptive_vp8 },
};

test_suite_t adaptive_test_suite = {
	"Adaptive algorithm",
	tester_init,
	tester_cleanup,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
