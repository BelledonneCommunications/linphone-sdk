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

typedef struct _stream_manager_t {
	MSFormatType type;
	int local_rtp;
	int local_rtcp;

	OrtpEvQueue * evq;

	union{
		AudioStream* audio_stream;
		VideoStream* video_stream;
	};

	int rtcp_count;

	struct {
		MSQosAnalyzerNetworkState network_state;
		float loss_estim;
		float congestion_bw_estim;
	} adaptive_stats;

} stream_manager_t ;

stream_manager_t * stream_manager_new(MSFormatType type) {
	stream_manager_t * mgr = ms_new0(stream_manager_t,1);
	mgr->type=type;
	mgr->local_rtp=(rand() % ((2^16)-1024) + 1024) & ~0x1;
	mgr->local_rtcp=mgr->local_rtp+1;

	mgr->evq=ortp_ev_queue_new();

	if (mgr->type==MSAudio){
		mgr->audio_stream=audio_stream_new (mgr->local_rtp, mgr->local_rtcp,FALSE);
		rtp_session_register_event_queue(mgr->audio_stream->ms.sessions.rtp_session,mgr->evq);
	}else{
#if VIDEO_ENABLED
		mgr->video_stream=video_stream_new (mgr->local_rtp, mgr->local_rtcp,FALSE);
		rtp_session_register_event_queue(mgr->video_stream->ms.sessions.rtp_session,mgr->evq);
#else
		ms_fatal("Unsupported stream type [%s]",ms_stream_type_to_string(mgr->type));
#endif

	}
	return mgr;
}

static void stream_manager_delete(stream_manager_t * mgr) {

	if (mgr->type==MSAudio){
		unlink(RECORDED_16K_1S_FILE);
		rtp_session_unregister_event_queue(mgr->audio_stream->ms.sessions.rtp_session,mgr->evq);
		audio_stream_stop(mgr->audio_stream);
	}else{
#if VIDEO_ENABLED
		rtp_session_unregister_event_queue(mgr->video_stream->ms.sessions.rtp_session,mgr->evq);
		video_stream_stop(mgr->video_stream);
#else
		ms_fatal("Unsupported stream type [%s]",ms_stream_type_to_string(mgr->type));
#endif
	}
	ortp_ev_queue_destroy(mgr->evq);
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

static void handle_queue_events(stream_manager_t * stream_mgr) {
	OrtpEvent *ev;
	while (NULL != (ev=ortp_ev_queue_get(stream_mgr->evq))){
		OrtpEventType evt=ortp_event_get_type(ev);
		OrtpEventData *evd=ortp_event_get_data(ev);

		if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
			const report_block_t *rb=NULL;
			if (rtcp_is_SR(evd->packet)){
				rb=rtcp_SR_get_report_block(evd->packet,0);
			}else if (rtcp_is_RR(evd->packet)){
				rb=rtcp_RR_get_report_block(evd->packet,0);
			}

			if (rb){
				stream_mgr->rtcp_count++;

				if (stream_mgr->type==MSVideo && stream_mgr->video_stream->ms.use_rc){
					const MSQosAnalyzer *analyzer=ms_bitrate_controller_get_qos_analyzer(stream_mgr->video_stream->ms.rc);
					if (analyzer->type==Stateful){
						const MSStatefulQosAnalyzer *stateful_analyzer=((const MSStatefulQosAnalyzer*)analyzer);
						stream_mgr->adaptive_stats.network_state=stateful_analyzer->network_state;
						stream_mgr->adaptive_stats.loss_estim =100*stateful_analyzer->network_loss_rate;
						stream_mgr->adaptive_stats.congestion_bw_estim =stateful_analyzer->congestion_bandwidth;
					}
				}
			}
		}
		ortp_event_destroy(ev);
	}
}

void start_adaptive_stream(MSFormatType type, stream_manager_t ** pmarielle, stream_manager_t ** pmargaux,
	int payload, int initial_bitrate, int target_bw, float loss_rate, int latency, float dup_ratio) {
	int pause_time=0;
	PayloadType* pt;
	MediaStream *marielle_ms,*margaux_ms;
	OrtpNetworkSimulatorParams params={0};
#if VIDEO_ENABLED
	MSWebCam * marielle_webcam=ms_web_cam_manager_get_default_cam (ms_web_cam_manager_get());
#endif
	stream_manager_t *marielle=*pmarielle=stream_manager_new(type);
	stream_manager_t *margaux=*pmargaux=stream_manager_new(type);
	params.enabled=TRUE;
	params.loss_rate=loss_rate;
	params.max_bandwidth=target_bw;
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
	rtp_session_set_duplication_ratio(marielle_ms->sessions.rtp_session, dup_ratio);

	if (marielle->type == MSAudio){
		audio_manager_start(marielle,payload,margaux->local_rtp,initial_bitrate,HELLO_16K_1S_FILE,NULL);
		ms_filter_call_method(marielle->audio_stream->soundread,MS_FILE_PLAYER_LOOP,&pause_time);

		audio_manager_start(margaux,payload,marielle->local_rtp,0,NULL,RECORDED_16K_1S_FILE);
	}else{
#if VIDEO_ENABLED
		video_manager_start(marielle,payload,margaux->local_rtp,0,marielle_webcam);
		video_stream_set_direction(margaux->video_stream,VideoStreamRecvOnly);

		video_manager_start(margaux,payload,marielle->local_rtp,0,NULL);
#else
		ms_fatal("Unsupported stream type [%s]",ms_stream_type_to_string(marielle->type));
#endif
	}

	rtp_session_enable_network_simulation(margaux_ms->sessions.rtp_session,&params);
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
		handle_queue_events(marielle);
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

static void adaptive_opus_audio_stream()  {
	bool_t supported = ms_filter_codec_supported("opus");
	if( supported ) {
		// at 8KHz -> 24kb/s
		// at 48KHz -> 48kb/s
		float bw_usage;
		stream_manager_t * marielle, * margaux;

		// on EDGEBW, both should be overconsumming
		start_adaptive_stream(MSAudio, &marielle, &margaux, OPUS_PAYLOAD_TYPE, 8000, EDGE_BW, 0, 0, 0);
		iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms)*1./EDGE_BW;
		CU_ASSERT_IN_RANGE(bw_usage, 2.f, 3.f); // bad! since this codec cant change its ptime and it is the lower bitrate, no improvement can occur
		stop_adaptive_stream(marielle,margaux);

		start_adaptive_stream(MSAudio, &marielle, &margaux, OPUS_PAYLOAD_TYPE, 48000, EDGE_BW, 0, 0, 0);
		iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms)*1./EDGE_BW;
		CU_ASSERT_IN_RANGE(bw_usage, 1.f, 1.4f); // bad!
		stop_adaptive_stream(marielle,margaux);

		// on 3G BW, both should be at max
		start_adaptive_stream(MSAudio, &marielle, &margaux, OPUS_PAYLOAD_TYPE, 8000, THIRDGENERATION_BW, 0, 0, 0);
		iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms)*1./THIRDGENERATION_BW;
		CU_ASSERT_IN_RANGE(bw_usage, .1f, .15f);
		stop_adaptive_stream(marielle,margaux);

		start_adaptive_stream(MSAudio, &marielle, &margaux, OPUS_PAYLOAD_TYPE, 48000, THIRDGENERATION_BW, 0, 0, 0);
		iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms)*1./THIRDGENERATION_BW;
		CU_ASSERT_IN_RANGE(bw_usage, .2f, .3f);
		stop_adaptive_stream(marielle,margaux);
	}
}

static void adaptive_speex16_audio_stream()  {
	bool_t supported = ms_filter_codec_supported("speex");
	if( supported ) {
		// at 16KHz -> 20 kb/s
		// at 32KHz -> 30 kb/s
		float bw_usage;
		stream_manager_t * marielle, * margaux;

		start_adaptive_stream(MSAudio, &marielle, &margaux, SPEEX16_PAYLOAD_TYPE, 32000, EDGE_BW / 2., 0, 0, 0);
		iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms)*1./(EDGE_BW / 2.);
		CU_ASSERT_IN_RANGE(bw_usage, 1.f, 5.f);
		stop_adaptive_stream(marielle,margaux);
	}
}

static void adaptive_pcma_audio_stream() {
	bool_t supported = ms_filter_codec_supported("pcma");
	if( supported ) {
		// at 8KHz -> 80 kb/s
		float bw_usage;
		stream_manager_t * marielle, * margaux;

		// yet non-adaptative codecs cannot respect low throughput limitations
		start_adaptive_stream(MSAudio, &marielle, &margaux, PCMA8_PAYLOAD_TYPE, 8000, EDGE_BW, 0, 0, 0);
		iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms)*1./EDGE_BW;
		CU_ASSERT_IN_RANGE(bw_usage,6.f, 8.f); // this is bad!
		stop_adaptive_stream(marielle,margaux);

		start_adaptive_stream(MSAudio, &marielle, &margaux, PCMA8_PAYLOAD_TYPE, 8000, THIRDGENERATION_BW, 0, 0, 0);
		iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
		bw_usage=media_stream_get_up_bw(&marielle->audio_stream->ms)*1./THIRDGENERATION_BW;
		CU_ASSERT_IN_RANGE(bw_usage, .3f, .5f);
		stop_adaptive_stream(marielle,margaux);
	}
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
			iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 2*(i+1));
			/*since PCMA uses 80kbit/s, upload bandwidth should just be 80+80*duplication_ratio kbit/s */
			CU_ASSERT_TRUE(fabs(rtp_session_get_send_bandwidth(marielle->audio_stream->ms.sessions.rtp_session)/1000. - 80.*(i+1)) < 1.f);
		}
		stop_adaptive_stream(marielle,margaux);
	}
}

static void stability_network_detection() {
	stream_manager_t * marielle, * margaux;
	start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300000, 0, 0, 500, 0);
	iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 9);
	CU_ASSERT_EQUAL(marielle->adaptive_stats.network_state, MSQosAnalyzerNetworkFine);
	stop_adaptive_stream(marielle,margaux);

	start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300000, 70000, 0, 250,0);
	iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 9);
	CU_ASSERT_EQUAL(marielle->adaptive_stats.network_state, MSQosAnalyzerNetworkCongested);
	stop_adaptive_stream(marielle,margaux);

	start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300000, 0, 15, 250,0);
	iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 9);
	CU_ASSERT_EQUAL(marielle->adaptive_stats.network_state, MSQosAnalyzerNetworkLossy);
	stop_adaptive_stream(marielle,margaux);
}

static void adaptive_vp8() {
	stream_manager_t * marielle, * margaux;

	start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300000, 0, 25, 50, 0);
	iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 12);
	CU_ASSERT_IN_RANGE(marielle->adaptive_stats.loss_estim, 20, 30);
	CU_ASSERT_TRUE(marielle->adaptive_stats.congestion_bw_estim > 200);
	stop_adaptive_stream(marielle,margaux);

	start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300000, 45000, 0, 50,0);
	iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 12);
	CU_ASSERT_IN_RANGE(marielle->adaptive_stats.loss_estim, 0, 2);
	CU_ASSERT_IN_RANGE(marielle->adaptive_stats.congestion_bw_estim, 30, 60);
	stop_adaptive_stream(marielle,margaux);

	start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300000, 70000,0, 50,0);
	iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 12);
	CU_ASSERT_IN_RANGE(marielle->adaptive_stats.loss_estim, 0, 2);
	CU_ASSERT_IN_RANGE(marielle->adaptive_stats.congestion_bw_estim, 50, 95);
	stop_adaptive_stream(marielle,margaux);

	start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300000, 100000,15, 50,0);
	iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 12);
	CU_ASSERT_IN_RANGE(marielle->adaptive_stats.loss_estim, 10, 20);
	CU_ASSERT_IN_RANGE(marielle->adaptive_stats.congestion_bw_estim, 80, 125);
	stop_adaptive_stream(marielle,margaux);
}

static void packet_duplication() {
	const rtp_stats_t *stats;
	double dup_ratio;
	stream_manager_t * marielle, * margaux;

	dup_ratio = 0;
	start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300000, 0, 0, 50,dup_ratio);
	iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 2);
	stats=rtp_session_get_stats(margaux->video_stream->ms.sessions.rtp_session);
	CU_ASSERT_EQUAL(stats->duplicated, dup_ratio ? stats->packet_recv / (dup_ratio+1) : 0);
	/*in theory, cumulative loss should be the invert of duplicated count, but
	since cumulative loss is computed only on received RTCP report and duplicated
	count is updated on each RTP packet received, we cannot accurately compare these values*/
	CU_ASSERT_TRUE(stats->cum_packet_loss <= -.5*stats->duplicated);
	stop_adaptive_stream(marielle,margaux);

	dup_ratio = 1;
	start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300000, 0, 0, 50,dup_ratio);
	iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 2);
	stats=rtp_session_get_stats(margaux->video_stream->ms.sessions.rtp_session);
	CU_ASSERT_EQUAL(stats->duplicated, dup_ratio ? stats->packet_recv / (dup_ratio+1) : 0);
	CU_ASSERT_TRUE(stats->cum_packet_loss <= -.5*stats->duplicated);
	stop_adaptive_stream(marielle,margaux);
}

static test_t tests[] = {
	{ "Packet duplication", packet_duplication},
	{ "Upload bandwidth computation", upload_bandwidth_computation },
	{ "Stability detection", stability_network_detection },
	{ "Adaptive audio stream [opus]", adaptive_opus_audio_stream },
	{ "Adaptive audio stream [speex]", adaptive_speex16_audio_stream },
	{ "Adaptive audio stream [pcma]", adaptive_pcma_audio_stream },
	{ "Adaptive video stream [VP8]", adaptive_vp8 },
};

test_suite_t adaptive_test_suite = {
	"AdaptiveAlgorithm",
	tester_init,
	tester_cleanup,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
