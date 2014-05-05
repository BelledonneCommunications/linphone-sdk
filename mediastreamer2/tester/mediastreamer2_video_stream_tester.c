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
												, 550
												, cam);
	CU_ASSERT_EQUAL(result,0);
}

#define EDGE_BW 10000
#define THIRDGENERATION_BW 200000
#define CU_ASSERT_IN_RANGE(value, inf, sup) CU_ASSERT_TRUE(value >= inf); CU_ASSERT_TRUE(value <= sup)

static float adaptive_video_stream(int payload, int initial_bitrate,int target_bw, float loss_rate, int max_recv_rtcp_packet) {
	const MSList * webcams = ms_web_cam_manager_get_list(ms_web_cam_manager_get());
	MSWebCam * marielle_webcam = (ms_list_size(webcams) > 0) ? (MSWebCam *) webcams->data : NULL;
	// MSWebCam * margaux_webcam = (ms_list_size(webcams) > 1) ? (MSWebCam *) webcams->next->data : NULL;

	video_stream_manager_t * marielle = video_stream_manager_new();
	video_stream_manager_t * margaux = video_stream_manager_new();

	OrtpNetworkSimulatorParams params={0};
	params.enabled=TRUE;
	params.loss_rate=loss_rate;
	params.max_bandwidth=target_bw;
	params.max_buffer_size=initial_bitrate;
	float bw_usage_ratio = 0.;
	// this variable should not be changed, since algorithm results rely on this value
	// (the bigger it is, the more accurate is bandwidth estimation)
	int rtcp_interval = 1000;
	float marielle_send_bw;

	media_stream_enable_adaptive_bitrate_control(&marielle->stream->ms,TRUE);

	video_manager_start(marielle, payload, margaux->local_rtp, initial_bitrate, marielle_webcam);


	video_manager_start(margaux, payload, marielle->local_rtp, -1, NULL);
	// video_manager_start(margaux, payload, marielle->local_rtp, -1, margaux_webcam);
	rtp_session_enable_network_simulation(margaux->stream->ms.sessions.rtp_session,&params);

	rtp_session_set_rtcp_report_interval(margaux->stream->ms.sessions.rtp_session, rtcp_interval);
	// just wait for timeout
	wait_for_until(&marielle->stream->ms,&margaux->stream->ms,&marielle->local_rtp,100000,rtcp_interval*max_recv_rtcp_packet);

	marielle_send_bw=media_stream_get_up_bw(&marielle->stream->ms);
	bw_usage_ratio=(params.max_bandwidth > 0) ? marielle_send_bw/params.max_bandwidth : 0;
	ms_message("marielle sent bw=[%f], target was [%f] bw_usage_ratio [%f]",marielle_send_bw,params.max_bandwidth,bw_usage_ratio);

	video_stream_manager_delete(marielle);
	video_stream_manager_delete(margaux);
	return bw_usage_ratio;
}

static void lossy_network() {
	bool_t supported = TRUE;//ms_filter_codec_supported("speex");
	if( supported ) {
		float bw_usage;
		int loss_rate = getenv("GPP_LOSS") ? atoi(getenv("GPP_LOSS")) : 0;
		int max_bw = getenv("GPP_MAXBW") ? atoi(getenv("GPP_MAXBW")) * 1000: 0;
		printf("\nloss_rate=%d(GPP_LOSS) max_bw=%d(GPP_MAXBW)\n", loss_rate, max_bw);
		bw_usage = adaptive_video_stream(H264_PAYLOAD_TYPE, 1000000, max_bw, loss_rate, 20);
		CU_ASSERT_IN_RANGE(bw_usage, .9f, 1.f);
	}
}

static test_t tests[] = {
	{ "Lossy network", lossy_network },
};

test_suite_t video_stream_test_suite = {
	"VideoStream",
	tester_init,
	tester_cleanup,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
