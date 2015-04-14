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
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "qosanalyzer.h"

#include <stdio.h>
#include "CUnit/Basic.h"


static RtpProfile rtp_profile;

#define OPUS_PAYLOAD_TYPE    121
#define SPEEX_PAYLOAD_TYPE 122
#define SILK16_PAYLOAD_TYPE  123
#define PCMA8_PAYLOAD_TYPE 8
#define H263_PAYLOAD_TYPE 34
#define H264_PAYLOAD_TYPE 102
#define VP8_PAYLOAD_TYPE 103

#define EDGE_BW 10
#define THIRDGENERATION_BW 200

static int tester_init(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();
	rtp_profile_set_payload (&rtp_profile,0,&payload_type_pcmu8000);
	rtp_profile_set_payload (&rtp_profile,OPUS_PAYLOAD_TYPE,&payload_type_opus);
	rtp_profile_set_payload (&rtp_profile,SPEEX_PAYLOAD_TYPE,&payload_type_speex_wb);
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

#define HELLO_16K_1S_FILE  "sounds/hello16000-1s.wav"
#define RECORDED_16K_1S_FILE  "sounds/recorded_hello16000-1s.wav"

typedef struct _stream_manager_t {
	MSFormatType type;
	int local_rtp;
	int local_rtcp;

	union{
		AudioStream* audio_stream;
		VideoStream* video_stream;
	};

	int rtcp_count;

	struct {
		float loss_estim;
		float congestion_bw_estim;
	} adaptive_stats;

	void* user_data;

} stream_manager_t ;

stream_manager_t * stream_manager_new(MSFormatType type) {
	stream_manager_t * mgr = ms_new0(stream_manager_t,1);
	mgr->type=type;
	mgr->local_rtp=(rand() % ((2^16)-1024) + 1024) & ~0x1;
	mgr->local_rtcp=mgr->local_rtp+1;
	mgr->user_data = 0;

	if (mgr->type==MSAudio){
		mgr->audio_stream=audio_stream_new (mgr->local_rtp, mgr->local_rtcp,FALSE);
	}else{
#if VIDEO_ENABLED
		mgr->video_stream=video_stream_new (mgr->local_rtp, mgr->local_rtcp,FALSE);
#else
		ms_fatal("Unsupported stream type [%s]",ms_format_type_to_string(mgr->type));
#endif

	}
	return mgr;
}

static void stream_manager_delete(stream_manager_t * mgr) {

	if (mgr->type==MSAudio){
		if( mgr->user_data){
			unlink((char*)mgr->user_data);
			// don't reset user_data, it is up to the user to free() it
		}

		audio_stream_stop(mgr->audio_stream);
	}else{
#if VIDEO_ENABLED
		video_stream_stop(mgr->video_stream);
#else
		ms_fatal("Unsupported stream type [%s]",ms_format_type_to_string(mgr->type));
#endif
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

#if VIDEO_ENABLED
static void video_manager_start(	stream_manager_t * mgr
									,int payload_type
									,int remote_port
									,int target_bitrate
									,MSWebCam * cam) {
	int result;
	media_stream_set_target_network_bitrate(&mgr->video_stream->ms,target_bitrate);

	result=video_stream_start(mgr->video_stream
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
#endif

static void qos_analyzer_on_action_suggested(void *user_data, int datac, const char** datav){
	stream_manager_t *mgr = (stream_manager_t*)user_data;
	mgr->rtcp_count++;

	if (mgr->type==MSVideo && mgr->video_stream->ms.rc_enable){
		const MSQosAnalyzer *analyzer=ms_bitrate_controller_get_qos_analyzer(mgr->video_stream->ms.rc);
		if (analyzer->type==MSQosAnalyzerAlgorithmStateful){
			const MSStatefulQosAnalyzer *stateful_analyzer=((const MSStatefulQosAnalyzer*)analyzer);
			mgr->adaptive_stats.loss_estim=stateful_analyzer->network_loss_rate;
			mgr->adaptive_stats.congestion_bw_estim=stateful_analyzer->congestion_bandwidth;
		}
	}
}

void start_adaptive_stream(MSFormatType type, stream_manager_t ** pmarielle, stream_manager_t ** pmargaux,
	int payload, int initial_bitrate, int max_bw, float loss_rate, int latency, float dup_ratio) {
	int pause_time=0;
	PayloadType* pt;
	MediaStream *marielle_ms,*margaux_ms;
	OrtpNetworkSimulatorParams params={0};
#if VIDEO_ENABLED
	MSWebCam * marielle_webcam=mediastreamer2_tester_get_mire_webcam(ms_web_cam_manager_get());
#endif
	stream_manager_t *marielle=*pmarielle=stream_manager_new(type);
	stream_manager_t *margaux=*pmargaux=stream_manager_new(type);

	char* file = ms_strdup_printf("%s/%s", bc_tester_read_dir_prefix, HELLO_16K_1S_FILE);
	char* recorded_file = ms_strdup_printf("%s/%s", bc_tester_writable_dir_prefix, RECORDED_16K_1S_FILE);

	marielle->user_data = recorded_file;
	params.enabled=TRUE;
	params.loss_rate=loss_rate;
	params.max_bandwidth=max_bw;
	params.latency=latency;

	if (type == MSAudio){
		marielle_ms=&marielle->audio_stream->ms;
		margaux_ms=&margaux->audio_stream->ms;
	}else{
		marielle_ms=&marielle->video_stream->ms;
		margaux_ms=&margaux->video_stream->ms;
	}

	/* Disable avpf. */
	pt = rtp_profile_get_payload(&rtp_profile, VP8_PAYLOAD_TYPE);
	CU_ASSERT_PTR_NOT_NULL_FATAL(pt);
	payload_type_unset_flag(pt, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);


	media_stream_enable_adaptive_bitrate_control(marielle_ms,TRUE);
	media_stream_set_adaptive_bitrate_algorithm(marielle_ms, MSQosAnalyzerAlgorithmStateful);
	rtp_session_set_duplication_ratio(marielle_ms->sessions.rtp_session, dup_ratio);

	if (marielle->type == MSAudio){
		audio_manager_start(marielle,payload,margaux->local_rtp,initial_bitrate,file,NULL);
		ms_filter_call_method(marielle->audio_stream->soundread,MS_FILE_PLAYER_LOOP,&pause_time);

		audio_manager_start(margaux,payload,marielle->local_rtp,0,NULL,recorded_file);
	}else{
#if VIDEO_ENABLED
		marielle->video_stream->staticimage_webcam_fps_optimization = FALSE;
		video_manager_start(marielle,payload,margaux->local_rtp,0,marielle_webcam);
		video_stream_set_direction(margaux->video_stream,VideoStreamRecvOnly);
		video_manager_start(margaux,payload,marielle->local_rtp,0,NULL);
#else
		ms_fatal("Unsupported stream type [%s]",ms_format_type_to_string(marielle->type));
#endif
	}

	ms_qos_analyzer_set_on_action_suggested(ms_bitrate_controller_get_qos_analyzer(marielle_ms->rc),
						qos_analyzer_on_action_suggested,
						*pmarielle);
	rtp_session_enable_network_simulation(margaux_ms->sessions.rtp_session,&params);

	ms_free(recorded_file);
	ms_free(file);

}

static void iterate_adaptive_stream(stream_manager_t * marielle, stream_manager_t * margaux,
	int timeout_ms, int* current, int expected){
	int retry=0;

	MediaStream *marielle_ms,*margaux_ms;
	if (marielle->type == MSAudio){
		marielle_ms=&marielle->audio_stream->ms;
		margaux_ms=&margaux->audio_stream->ms;
	}else{
		marielle_ms=&marielle->video_stream->ms;
		margaux_ms=&margaux->video_stream->ms;
	}

	while ((!current||*current<expected) && retry++ <timeout_ms/100) {
		media_stream_iterate(marielle_ms);
		media_stream_iterate(margaux_ms);
		// handle_queue_events(marielle);
		if (retry%10==0) {
			 ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec"	,
				marielle_ms, media_stream_get_down_bw(marielle_ms)/1000, media_stream_get_up_bw(marielle_ms)/1000);
			 ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec"	,
				margaux_ms, media_stream_get_down_bw(margaux_ms)/1000, media_stream_get_up_bw(margaux_ms)/1000);
		 }
		ms_usleep(100000);
	}
}

static void stop_adaptive_stream(stream_manager_t *marielle, stream_manager_t *margaux){
	stream_manager_delete(marielle);
	stream_manager_delete(margaux);
}

typedef struct {
		OrtpLossRateEstimator *estimator;
		OrtpEvQueue *q;
		int loss_rate;
}LossRateEstimatorCtx;
static void event_queue_cb(MediaStream *ms, void *user_pointer) {
	LossRateEstimatorCtx *ctx = (LossRateEstimatorCtx*)user_pointer;
	if (ctx->q != NULL) {
		OrtpEvent *ev = NULL;
		while ((ev = ortp_ev_queue_get(ctx->q)) != NULL) {
			OrtpEventType evt = ortp_event_get_type(ev);
			OrtpEventData *evd = ortp_event_get_data(ev);
			if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
				do {
					const report_block_t *rb=NULL;
					if (rtcp_is_SR(evd->packet)){
						rb=rtcp_SR_get_report_block(evd->packet,0);
					}else if (rtcp_is_RR(evd->packet)){
						rb=rtcp_RR_get_report_block(evd->packet,0);
					}

					if (rb&&ortp_loss_rate_estimator_process_report_block(ctx->estimator,&ms->sessions.rtp_session->rtp,rb)){
						float diff = fabs(ortp_loss_rate_estimator_get_value(ctx->estimator) - ctx->loss_rate);
						CU_ASSERT_TRUE(diff >= 0);
						CU_ASSERT_TRUE(diff <= 10);
					}
				} while (rtcp_next_packet(evd->packet));
			}
			ortp_event_destroy(ev);
		}
	}
}

/********************************** Tests are starting now ********************/
static void packet_duplication() {
	const rtp_stats_t *stats;
	double dup_ratio;
	stream_manager_t * marielle, * margaux;

	dup_ratio = 0;
	start_adaptive_stream(MSAudio, &marielle, &margaux, SPEEX_PAYLOAD_TYPE, 32000, 0, 0, 50,dup_ratio);
	media_stream_enable_adaptive_bitrate_control(&marielle->audio_stream->ms,FALSE);
	iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
	stats=rtp_session_get_stats(margaux->video_stream->ms.sessions.rtp_session);
	CU_ASSERT_EQUAL(stats->packet_dup_recv, dup_ratio ? stats->packet_recv / (dup_ratio+1) : 0);
	/*in theory, cumulative loss should be the invert of duplicated count, but
	since cumulative loss is computed only on received RTCP report and duplicated
	count is updated on each RTP packet received, we cannot accurately compare these values*/
	CU_ASSERT_TRUE(stats->cum_packet_loss <= -.5*stats->packet_dup_recv);
	stop_adaptive_stream(marielle,margaux);

	dup_ratio = 1;
	start_adaptive_stream(MSAudio, &marielle, &margaux, SPEEX_PAYLOAD_TYPE, 32000, 0, 0, 50,dup_ratio);
	media_stream_enable_adaptive_bitrate_control(&marielle->audio_stream->ms,FALSE);
	iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
	stats=rtp_session_get_stats(margaux->video_stream->ms.sessions.rtp_session);
	CU_ASSERT_EQUAL(stats->packet_dup_recv, dup_ratio ? stats->packet_recv / (dup_ratio+1) : 0);
	CU_ASSERT_TRUE(stats->cum_packet_loss <= -.5*stats->packet_dup_recv);
	stop_adaptive_stream(marielle,margaux);
}

static void upload_bandwidth_computation() {
	bool_t supported = ms_filter_codec_supported("pcma");
	if( supported ) {
		stream_manager_t * marielle, * margaux;
		int i;

		start_adaptive_stream(MSAudio, &marielle, &margaux, PCMA8_PAYLOAD_TYPE, 8000, 0, 0, 0, 0);
		media_stream_enable_adaptive_bitrate_control(&marielle->audio_stream->ms,FALSE);

		for (i = 0; i < 5; i++){
			rtp_session_set_duplication_ratio(marielle->audio_stream->ms.sessions.rtp_session, i);
			iterate_adaptive_stream(marielle, margaux, 5000, NULL, 0);
			/*since PCMA uses 80kbit/s, upload bandwidth should just be 80+80*duplication_ratio kbit/s */
			CU_ASSERT_TRUE(fabs(rtp_session_get_send_bandwidth(marielle->audio_stream->ms.sessions.rtp_session)/1000. - 80.*(i+1)) < 5.f);
		}
		stop_adaptive_stream(marielle,margaux);
	}
}

static void loss_rate_estimation() {
	bool_t supported = ms_filter_codec_supported("pcma");
	if( supported ) {
		LossRateEstimatorCtx ctx;
		stream_manager_t * marielle, * margaux;
		int loss_rate = 15;

		start_adaptive_stream(MSAudio, &marielle, &margaux, PCMA8_PAYLOAD_TYPE, 8000, 0, loss_rate, 0, 0);
		ctx.estimator=ortp_loss_rate_estimator_new(120, 2500, marielle->audio_stream->ms.sessions.rtp_session);
		ctx.q = ortp_ev_queue_new();
		rtp_session_register_event_queue(marielle->audio_stream->ms.sessions.rtp_session, ctx.q);
		ctx.loss_rate = loss_rate;

		/*loss rate should be the initial one*/
		wait_for_until_with_parse_events(&marielle->audio_stream->ms, &margaux->audio_stream->ms, &loss_rate, 100, 10000, event_queue_cb,&ctx,NULL,NULL);

		/*let's set some duplication. loss rate should NOT be changed */
		rtp_session_set_duplication_ratio(marielle->audio_stream->ms.sessions.rtp_session, 10);
		wait_for_until_with_parse_events(&marielle->audio_stream->ms, &margaux->audio_stream->ms, &loss_rate, 100, 10000, event_queue_cb,&ctx,NULL,NULL);

		stop_adaptive_stream(marielle,margaux);
		ortp_loss_rate_estimator_destroy(ctx.estimator);
		ortp_ev_queue_destroy(ctx.q);
	}
}

void upload_bitrate(const char* codec, int payload, int target_bw, int expect_bw) {
	bool_t supported = ms_filter_codec_supported(codec);
	if( supported ) {
		float upload_bw;
		stream_manager_t * marielle, * margaux;

		start_adaptive_stream(MSAudio, &marielle, &margaux, payload, target_bw*1000, target_bw*1000, 0, 50,0);
		//these tests check that encoders stick to the guidelines, so we must use NOT
		//the adaptive algorithm which would modify these guidelines
		media_stream_enable_adaptive_bitrate_control(&marielle->audio_stream->ms,FALSE);
		iterate_adaptive_stream(marielle, margaux, 15000, NULL, 0);
		upload_bw=media_stream_get_up_bw(&marielle->audio_stream->ms) / 1000;
		CU_ASSERT_TRUE(upload_bw >= expect_bw-2);
		CU_ASSERT_TRUE(upload_bw <= expect_bw+2);
		stop_adaptive_stream(marielle,margaux);
	}
}

static void upload_bitrate_pcma_3g() {
	// pcma codec bitrate is always 64 kbits, only ptime can change from 20ms to 100ms.
	// ptime=20  ms -> network bitrate=80 kbits/s
	// ptime=100 ms -> network bitrate=67 kbits/s
	upload_bitrate("pcma", PCMA8_PAYLOAD_TYPE, THIRDGENERATION_BW, 80);
}

static void upload_bitrate_speex_low()  {
	// speex codec bitrate can vary from 16 kbits/s to 42 kbits/s
	// bitrate=42 kbits/s ptime=20  ms -> network bitrate=58 kbits/s
	// bitrate=16 kbits/s ptime=100 ms -> network bitrate=19 kbits/s
	upload_bitrate("speex", SPEEX_PAYLOAD_TYPE, 25, 25);
}

static void upload_bitrate_speex_3g()  {
	upload_bitrate("speex", SPEEX_PAYLOAD_TYPE, THIRDGENERATION_BW, 59);
}


static void upload_bitrate_opus_edge() {
	// opus codec bitrate can vary from 6 kbits/s to 184 kbits/s
	// bitrate=6   kbits/s and ptime=100  ms -> network bitrate=  9 kbits/s
	// bitrate=184 kbits/s and ptime=20   ms -> network bitrate=200 kbits/s
	// upload_bitrate("opus", OPUS_PAYLOAD_TYPE, EDGE_BW, 9);
	ms_warning("%s TODO: fix me. ptime in preprocess should be computed to stick the guidelines", __FUNCTION__);
	//until ptime is correctly set on startup, this will not work: currently ptime is set to 40ms but this
	// is not sufficient to match the guidelines without adaptive algorithm.
}

static void upload_bitrate_opus_3g() {
	upload_bitrate("opus", OPUS_PAYLOAD_TYPE, THIRDGENERATION_BW, 200);
}

#if VIDEO_ENABLED && 0
void adaptive_video(int max_bw, int exp_min_bw, int exp_max_bw, int loss_rate, int exp_min_loss, int exp_max_loss) {
	bool_t supported = ms_filter_codec_supported("VP8");
	if( supported ) {
		stream_manager_t * marielle, * margaux;
		start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300*1000, max_bw*1000, loss_rate, 50,0);
		iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 7);
		CU_ASSERT_TRUE(marielle->adaptive_stats.loss_estim >= exp_min_loss);
		CU_ASSERT_TRUE(marielle->adaptive_stats.loss_estim <= exp_max_loss);
		CU_ASSERT_TRUE(marielle->adaptive_stats.congestion_bw_estim >= exp_min_bw);
		CU_ASSERT_TRUE(marielle->adaptive_stats.congestion_bw_estim <= exp_max_bw);
		stop_adaptive_stream(marielle,margaux);
	}
}

static void adaptive_vp8_ideal() {
	adaptive_video(0, 200, 10000, 0, 0, 1);
}
static void adaptive_vp8_lossy() {
	adaptive_video(0, 200, 10000, 25, 20, 30);
}
static void adaptive_vp8_congestion() {
	adaptive_video(70, 50, 95, 0, 0, 2);
}
static void adaptive_vp8_lossy_congestion() {
	ms_warning("Disabled yet, too much instable");
	// adaptive_video(130, 110, 150, 7, 4, 10);
}
#endif


static test_t tests[] = {
	{ "Packet duplication", packet_duplication},
	{ "Upload bandwidth computation", upload_bandwidth_computation },
	{ "Loss rate estimation", loss_rate_estimation },

	{ "Upload bitrate [pcma] - 3g", upload_bitrate_pcma_3g },
	{ "Upload bitrate [speex] - low", upload_bitrate_speex_low },
	{ "Upload bitrate [speex] - 3g", upload_bitrate_speex_3g },
	{ "Upload bitrate [opus] - edge", upload_bitrate_opus_edge },
	{ "Upload bitrate [opus] - 3g", upload_bitrate_opus_3g },

#if VIDEO_ENABLED && 0
	{ "Network detection [VP8] - ideal", adaptive_vp8_ideal },
	{ "Network detection [VP8] - lossy", adaptive_vp8_lossy },
	{ "Network detection [VP8] - congested", adaptive_vp8_congestion },
	{ "Network detection [VP8] - lossy congested", adaptive_vp8_lossy_congestion },
#endif
};

test_suite_t adaptive_test_suite = {
	"AdaptiveAlgorithm",
	tester_init,
	tester_cleanup,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
