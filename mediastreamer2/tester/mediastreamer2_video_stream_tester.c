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
#include "mediastreamer2/bitratecontrol.h"
#include "private.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include <stdio.h>
#include "CUnit/Basic.h"

#include "qosanalyzer.h"

#ifdef _MSC_VER
#define unlink _unlink
#endif

static RtpProfile rtp_profile;
#define H263_PAYLOAD_TYPE 34
#define H264_PAYLOAD_TYPE 102
#define VP8_PAYLOAD_TYPE 103

static int tester_init(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();

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

#define MARIELLE_RTP_PORT 2564
#define MARIELLE_RTCP_PORT 2565
#define MARIELLE_IP "127.0.0.1"

#define MARGAUX_RTP_PORT 9864
#define MARGAUX_RTCP_PORT 9865
#define MARGAUX_IP "127.0.0.1"

typedef struct _video_stream_manager_t {
	VideoStream* stream;
	int local_rtp;
	int local_rtcp;
	struct {
		float loss;
		float rtt;
		MSQosAnalyserNetworkState network_state;
	} latest_stats;
} video_stream_manager_t ;
static video_stream_manager_t * video_stream_manager_new() {
	video_stream_manager_t * mgr =  ms_new0(video_stream_manager_t,1);
	mgr->local_rtp= (rand() % ((2^16)-1024) + 1024) & ~0x1;
	mgr->local_rtcp=mgr->local_rtp+1;
	mgr->stream = video_stream_new (mgr->local_rtp, mgr->local_rtcp,FALSE);
	return mgr;

}
static void video_stream_manager_delete(video_stream_manager_t * mgr) {
	video_stream_stop(mgr->stream);
	ms_free(mgr);
}

static void video_manager_start(	video_stream_manager_t * mgr
									,int payload_type
									,int remote_port
									,int target_bitrate
									,MSWebCam * cam) {
	media_stream_set_target_network_bitrate(&mgr->stream->ms,target_bitrate);

	int result = 	video_stream_start(mgr->stream
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

#define EDGE_BW 10000
#define THIRDGENERATION_BW 200000
#define CU_ASSERT_IN_RANGE(value, inf, sup) CU_ASSERT_TRUE(value >= inf); CU_ASSERT_TRUE(value <= sup)

static void handle_queue_events(video_stream_manager_t * stream_mgr, OrtpEvQueue * evq) {
	OrtpEvent *ev;
	while (NULL != (ev=ortp_ev_queue_get(evq))){
		OrtpEventType evt=ortp_event_get_type(ev);
		OrtpEventData *evd=ortp_event_get_data(ev);

		if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED/* || evt == ORTP_EVENT_RTCP_PACKET_EMITTED*/) {
			const report_block_t *rb=NULL;
			if (rtcp_is_SR(evd->packet)){
				rb=rtcp_SR_get_report_block(evd->packet,0);
			}else if (rtcp_is_RR(evd->packet)){
				rb=rtcp_RR_get_report_block(evd->packet,0);
			}

			if (rb) {
				const MSQosAnalyser *analyser=ms_bitrate_controller_get_qos_analyser(stream_mgr->stream->ms.rc);
				if (analyser->type==Stateful){
					stream_mgr->latest_stats.network_state=
						((const MSStatefulQosAnalyser*)analyser)->network_state;
				}

				stream_mgr->latest_stats.loss=100.0*(float)report_block_get_fraction_lost(rb)/256.0;
				stream_mgr->latest_stats.rtt=rtp_session_get_round_trip_propagation(stream_mgr->stream->ms.sessions.rtp_session);

				ms_message("mediastreamer2_video_stream_tester: %s RTCP packet: loss=%f, RTT=%f, network_state=%d"
					,(evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) ? "RECEIVED" : "EMITTED"
					,stream_mgr->latest_stats.loss
					,stream_mgr->latest_stats.rtt
					,stream_mgr->latest_stats.network_state);
			}
		}
		ortp_event_destroy(ev);
	}
}

static void start_adaptive_video_stream(video_stream_manager_t * marielle, video_stream_manager_t * margaux,
	int payload, int initial_bitrate,int target_bw, float loss_rate, int latency, int max_recv_rtcp_packet) {

	MSWebCam * marielle_webcam = ms_web_cam_manager_get_default_cam (ms_web_cam_manager_get());
	MSWebCam * margaux_webcam = ms_web_cam_manager_get_cam(ms_web_cam_manager_get(), "StaticImage: Static picture");

	OrtpNetworkSimulatorParams params={0};
	params.enabled=TRUE;
	params.loss_rate=loss_rate;
	params.max_bandwidth=target_bw;
	params.latency=latency;
	/*this variable should not be changed, since algorithm results rely on this value
	(the bigger it is, the more accurate is bandwidth estimation)*/
	int rtcp_interval = 2500;

	media_stream_enable_adaptive_bitrate_control(&marielle->stream->ms,TRUE);

	video_manager_start(marielle, payload, margaux->local_rtp, initial_bitrate, marielle_webcam);


	video_manager_start(margaux, payload, marielle->local_rtp, -1, margaux_webcam);
	rtp_session_enable_network_simulation(margaux->stream->ms.sessions.rtp_session,&params);

	rtp_session_set_rtcp_report_interval(margaux->stream->ms.sessions.rtp_session, rtcp_interval);

	OrtpEvQueue * evq = ortp_ev_queue_new();
	rtp_session_register_event_queue(marielle->stream->ms.sessions.rtp_session,evq);

	/*just wait for timeout*/
	int retry=0;
	int timeout_ms=rtcp_interval*max_recv_rtcp_packet;
	while (retry++ <timeout_ms/100) {
		media_stream_iterate(&marielle->stream->ms);
		media_stream_iterate(&margaux->stream->ms);
		handle_queue_events(marielle, evq);
		if (retry%10==0) {
			 ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec"	,
			 	&marielle->stream->ms, media_stream_get_down_bw(&marielle->stream->ms)/1000, media_stream_get_up_bw(&marielle->stream->ms)/1000);
			 ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec"	,
			 	&margaux->stream->ms, media_stream_get_down_bw(&margaux->stream->ms)/1000, media_stream_get_up_bw(&margaux->stream->ms)/1000);
		 }
		ms_usleep(100000);
	}

	rtp_session_unregister_event_queue(marielle->stream->ms.sessions.rtp_session,evq);
	ortp_ev_queue_destroy(evq);
}

#define INIT() \
	marielle = video_stream_manager_new(); \
	margaux = video_stream_manager_new();

#define DEINIT() \
	video_stream_manager_delete(marielle); \
	video_stream_manager_delete(margaux);

static void lossy_network() {
	video_stream_manager_t * marielle, * margaux;
	/*verify that some webcam is supported*/
	bool_t supported = (ms_web_cam_manager_get_default_cam(ms_web_cam_manager_get()) != NULL);
	if( supported ) {
		float bw_usage;
		//for test purpose only
		int loss_rate = getenv("GPP_LOSS") ? atoi(getenv("GPP_LOSS")) : 0;
		int max_bw = getenv("GPP_MAXBW") ? atoi(getenv("GPP_MAXBW")) * 1000: 0;
		int latency = getenv("GPP_LAG") ? atoi(getenv("GPP_LAG")): 0;

		printf("\nloss_rate=%d(GPP_LOSS)\n", loss_rate);
		printf("max_bw=%d(GPP_MAXBW)\n", max_bw);
		printf("latency=%d(GPP_LAG)\n", latency);

		INIT();
		start_adaptive_video_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, max_bw, loss_rate, latency, 20);

		float marielle_send_bw=media_stream_get_up_bw(&marielle->stream->ms);
		bw_usage=(max_bw > 0) ? marielle_send_bw/max_bw : 1;
		ms_message("marielle sent bw=[%f], target was [%d] bw_usage [%f]",marielle_send_bw,max_bw,bw_usage);
		DEINIT();

		CU_ASSERT_IN_RANGE(bw_usage, .9f, 1.f);
	}
}

static void stability_network_detection() {
	video_stream_manager_t * marielle, * margaux;
	INIT();
	start_adaptive_video_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 0, 0, 500, 10);
	CU_ASSERT_EQUAL(marielle->latest_stats.network_state, MSQosAnalyserNetworkFine);
	DEINIT();

	INIT();
	start_adaptive_video_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 50000, 0, 250, 10);
	CU_ASSERT_EQUAL(marielle->latest_stats.network_state, MSQosAnalyserNetworkCongested);
	DEINIT();

	INIT();
	start_adaptive_video_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 0, 15, 250, 10);
	CU_ASSERT_EQUAL(marielle->latest_stats.network_state, MSQosAnalyserNetworkLossy);
	DEINIT();
}

static void adaptive_vp8() {
	video_stream_manager_t * marielle, * margaux;
	int bitrate;
	int has_get_bitrate;
/*	INIT();
	start_adaptive_video_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 0,25, 500, 10);
	has_get_bitrate = ms_filter_call_method(marielle->stream->ms.encoder,MS_FILTER_GET_BITRATE,&bitrate);
	CU_ASSERT_EQUAL(has_get_bitrate, 0);
	CU_ASSERT_EQUAL(bitrate, 200000);
	CU_ASSERT_EQUAL(marielle->latest_stats.network_state, MSQosAnalyserNetworkLossy);
	DEINIT();*/
/*
	INIT();
	start_adaptive_video_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 40000,0, 50, 10);
	has_get_bitrate = ms_filter_call_method(marielle->stream->ms.encoder,MS_FILTER_GET_BITRATE,&bitrate);
	CU_ASSERT_EQUAL(has_get_bitrate, 0);
	CU_ASSERT_EQUAL(bitrate, 64000);
	CU_ASSERT_EQUAL(marielle->latest_stats.network_state, MSQosAnalyserNetworkCongested);
	DEINIT();*/

/*	INIT();
	start_adaptive_video_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 70000,0, 50, 10);
	has_get_bitrate = ms_filter_call_method(marielle->stream->ms.encoder,MS_FILTER_GET_BITRATE,&bitrate);
	CU_ASSERT_EQUAL(has_get_bitrate, 0);
	CU_ASSERT_EQUAL(bitrate, 64000);
	CU_ASSERT_EQUAL(marielle->latest_stats.network_state, MSQosAnalyserNetworkFine);
	DEINIT();*/

	INIT();
	start_adaptive_video_stream(marielle, margaux, VP8_PAYLOAD_TYPE, 300000, 100000,15, 50, 100);
	has_get_bitrate = ms_filter_call_method(marielle->stream->ms.encoder,MS_FILTER_GET_BITRATE,&bitrate);
	CU_ASSERT_EQUAL(has_get_bitrate, 0);
	CU_ASSERT_EQUAL(bitrate, 64000);
	CU_ASSERT_EQUAL(marielle->latest_stats.network_state, MSQosAnalyserNetworkLossy);
	DEINIT();
}

static test_t tests[] = {
	{ "Lossy network", lossy_network },
	{ "Stability detection", stability_network_detection },
	{ "Adaptive video stream [VP8]", adaptive_vp8 },
};

test_suite_t video_stream_test_suite = {
	"VideoStream",
	tester_init,
	tester_cleanup,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
