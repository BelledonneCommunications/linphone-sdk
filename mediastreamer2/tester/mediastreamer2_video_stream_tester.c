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
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include <math.h>

#ifdef _MSC_VER
#define unlink _unlink
#endif

static RtpProfile rtp_profile;

#define VP8_PAYLOAD_TYPE   103
#define H264_PAYLOAD_TYPE  104
#define MP4V_PAYLOAD_TYPE  105

MSWebCam* mediastreamer2_tester_get_mire_webcam(MSWebCamManager *mgr) {
	MSWebCam *cam;

	cam = ms_web_cam_manager_get_cam(mgr, "Mire: Mire (synthetic moving picture)");

	if (cam == NULL) {
		MSWebCamDesc *desc = ms_mire_webcam_desc_get();
		if (desc){
			cam=ms_web_cam_new(desc);
			ms_web_cam_manager_add_cam(mgr,cam);
		}
	}

	return cam;
}

static int tester_before_all(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();
	rtp_profile_set_payload(&rtp_profile, VP8_PAYLOAD_TYPE, &payload_type_vp8);
	rtp_profile_set_payload(&rtp_profile, H264_PAYLOAD_TYPE, &payload_type_h264);
	rtp_profile_set_payload(&rtp_profile, MP4V_PAYLOAD_TYPE, &payload_type_mp4v);
	return 0;
}

static int tester_after_all(void) {
	ms_exit();
	rtp_profile_clear_all(&rtp_profile);
	return 0;
}

#ifdef VIDEO_ENABLED

typedef struct _video_stream_tester_stats_t {
	OrtpEvQueue *q;
	rtp_stats_t rtp;
	int number_of_SR;
	int number_of_RR;
	int number_of_SDES;
	int number_of_PLI;
	int number_of_SLI;
	int number_of_RPSI;

	int number_of_decoder_decoding_error;
	int number_of_decoder_first_image_decoded;
	int number_of_decoder_send_pli;
	int number_of_decoder_send_sli;
	int number_of_decoder_send_rpsi;
} video_stream_tester_stats_t;

typedef struct _video_stream_tester_t {
	VideoStream *vs;
	video_stream_tester_stats_t stats;
	MSVideoConfiguration* vconf;
	char* local_ip;
	int local_rtp;
	int local_rtcp;
	MSWebCam * cam;
	int payload_type;
} video_stream_tester_t;

void video_stream_tester_set_local_ip(video_stream_tester_t* obj,const char*ip) {
	char* new_ip = ip?ms_strdup(ip):NULL;
	if (obj->local_ip) ms_free(obj->local_ip);
	obj->local_ip=new_ip;
}

video_stream_tester_t* video_stream_tester_new() {
	video_stream_tester_t* vst = ms_new0(video_stream_tester_t,1);
	video_stream_tester_set_local_ip(vst,"127.0.0.1");
	vst->cam = ms_web_cam_manager_get_cam(ms_web_cam_manager_get(), "StaticImage: Static picture");
	vst->local_rtp=-1; /*random*/
	vst->local_rtcp=-1; /*random*/
	return  vst;
}

video_stream_tester_t* video_stream_tester_create(const char* local_ip, int local_rtp, int local_rtcp) {
	video_stream_tester_t *vst = video_stream_tester_new();
	if (local_ip)
		video_stream_tester_set_local_ip(vst,local_ip);
	vst->local_rtp=local_rtp;
	vst->local_rtcp=local_rtcp;
	return vst;
}

void video_stream_tester_destroy(video_stream_tester_t* obj) {
	if (obj->vconf) ms_free(obj->vconf);
	if(obj->local_ip) ms_free(obj->local_ip);
	ms_free(obj);
}

static void reset_stats(video_stream_tester_stats_t *s) {
	memset(s, 0, sizeof(video_stream_tester_stats_t));
}
static void video_stream_event_cb(void *user_pointer, const MSFilter *f, const unsigned int event_id, const void *args){
	video_stream_tester_t* vs_tester = (video_stream_tester_t*) user_pointer;
	const char* event_name;
	switch (event_id) {
	case MS_VIDEO_DECODER_DECODING_ERRORS:
		event_name="MS_VIDEO_DECODER_DECODING_ERRORS";
		vs_tester->stats.number_of_decoder_decoding_error++;
		break;
	case MS_VIDEO_DECODER_FIRST_IMAGE_DECODED:
		event_name="MS_VIDEO_DECODER_FIRST_IMAGE_DECODED";
		vs_tester->stats.number_of_decoder_first_image_decoded++;
		break;
	case MS_VIDEO_DECODER_SEND_PLI:
		event_name="MS_VIDEO_DECODER_SEND_PLI";
		vs_tester->stats.number_of_decoder_send_pli++;
		break;
	case MS_VIDEO_DECODER_SEND_SLI:
		event_name="MS_VIDEO_DECODER_SEND_SLI";
		vs_tester->stats.number_of_decoder_send_sli++;
		break;
	case MS_VIDEO_DECODER_SEND_RPSI:
		vs_tester->stats.number_of_decoder_send_rpsi++;
		event_name="MS_VIDEO_DECODER_SEND_RPSI";
		/* Handled internally by mediastreamer2. */
		break;
	default:
		ms_warning("Unhandled event %i", event_id);
		event_name="UNKNOWN";
		break;
	}
	ms_message("Event [%s:%u] received on video stream [%p]",event_name,event_id,vs_tester);

}

static void event_queue_cb(MediaStream *ms, void *user_pointer) {
	video_stream_tester_stats_t *st = (video_stream_tester_stats_t *)user_pointer;
	OrtpEvent *ev = NULL;

	if (st->q != NULL) {
		while ((ev = ortp_ev_queue_get(st->q)) != NULL) {
			OrtpEventType evt = ortp_event_get_type(ev);
			OrtpEventData *d = ortp_event_get_data(ev);
			if (evt == ORTP_EVENT_RTCP_PACKET_EMITTED) {
				do {
					if (rtcp_is_RR(d->packet)) {
						st->number_of_RR++;
					} else if (rtcp_is_SR(d->packet)) {
						st->number_of_SR++;
					} else if (rtcp_is_SDES(d->packet)) {
						st->number_of_SDES++;
					} else if (rtcp_is_PSFB(d->packet)) {
						switch (rtcp_PSFB_get_type(d->packet)) {
							case RTCP_PSFB_PLI:
								st->number_of_PLI++;
								break;
							case RTCP_PSFB_SLI:
								st->number_of_SLI++;
								break;
							case RTCP_PSFB_RPSI:
								st->number_of_RPSI++;
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

static void create_video_stream(video_stream_tester_t *vst, int payload_type) {
	vst->vs = video_stream_new2(vst->local_ip, vst->local_rtp, vst->local_rtcp);
	vst->vs->staticimage_webcam_fps_optimization = FALSE;
	vst->local_rtp = rtp_session_get_local_port(vst->vs->ms.sessions.rtp_session);
	vst->local_rtcp = rtp_session_get_local_rtcp_port(vst->vs->ms.sessions.rtp_session);
	reset_stats(&vst->stats);
	rtp_session_set_multicast_loopback(vst->vs->ms.sessions.rtp_session, TRUE);
	vst->stats.q = ortp_ev_queue_new();
	rtp_session_register_event_queue(vst->vs->ms.sessions.rtp_session, vst->stats.q);
	video_stream_set_event_callback(vst->vs, video_stream_event_cb, vst);
	if (vst->vconf) {
		PayloadType *pt = rtp_profile_get_payload(&rtp_profile, payload_type);
		BC_ASSERT_PTR_NOT_NULL_FATAL(pt);
		pt->normal_bitrate = vst->vconf->required_bitrate;
		video_stream_set_fps(vst->vs, vst->vconf->fps);
		video_stream_set_sent_video_size(vst->vs, vst->vconf->vsize);
	}
	vst->payload_type = payload_type;
}

static void destroy_video_stream(video_stream_tester_t *vst) {
	video_stream_stop(vst->vs);
	ortp_ev_queue_destroy(vst->stats.q);
}

static void init_video_streams(video_stream_tester_t *vst1, video_stream_tester_t *vst2, bool_t avpf, bool_t one_way, OrtpNetworkSimulatorParams *params, int payload_type) {
	PayloadType *pt;

	create_video_stream(vst1, payload_type);
	create_video_stream(vst2, payload_type);

	/* Enable/disable avpf. */
	pt = rtp_profile_get_payload(&rtp_profile, payload_type);
	BC_ASSERT_PTR_NOT_NULL_FATAL(pt);
	if (avpf == TRUE) {
		payload_type_set_flag(pt, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
	} else {
		payload_type_unset_flag(pt, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
	}

	/* Configure network simulator. */
	if ((params != NULL) && (params->enabled == TRUE)) {
		rtp_session_enable_network_simulation(vst1->vs->ms.sessions.rtp_session, params);
		rtp_session_enable_network_simulation(vst2->vs->ms.sessions.rtp_session, params);
	}

	if (one_way == TRUE) {
		video_stream_set_direction(vst1->vs, MediaStreamRecvOnly);
	}

	BC_ASSERT_EQUAL(video_stream_start(vst1->vs, &rtp_profile, vst2->local_ip, vst2->local_rtp, vst2->local_ip, vst2->local_rtcp, payload_type, 50, vst1->cam), 0,int,"%d");
	BC_ASSERT_EQUAL(video_stream_start(vst2->vs, &rtp_profile, vst1->local_ip, vst1->local_rtp, vst1->local_ip, vst1->local_rtcp, payload_type, 50, vst2->cam), 0,int,"%d");
}

static void uninit_video_streams(video_stream_tester_t *vst1, video_stream_tester_t *vst2) {
	float rtcp_send_bandwidth;
	PayloadType *vst1_pt;
	PayloadType *vst2_pt;

	vst1_pt = rtp_profile_get_payload(&rtp_profile, vst1->payload_type);
	BC_ASSERT_PTR_NOT_NULL_FATAL(vst1_pt);
	vst2_pt = rtp_profile_get_payload(&rtp_profile, vst2->payload_type);
	BC_ASSERT_PTR_NOT_NULL_FATAL(vst2_pt);

	rtcp_send_bandwidth = rtp_session_get_rtcp_send_bandwidth(vst1->vs->ms.sessions.rtp_session);
	ms_message("vst1: rtcp_send_bandwidth=%f, payload_type_bitrate=%d, rtcp_target_bandwidth=%f",
		rtcp_send_bandwidth, payload_type_get_bitrate(vst1_pt), 0.06 * payload_type_get_bitrate(vst1_pt));
	BC_ASSERT_TRUE(rtcp_send_bandwidth <= (0.06 * payload_type_get_bitrate(vst1_pt)));
	rtcp_send_bandwidth = rtp_session_get_rtcp_send_bandwidth(vst2->vs->ms.sessions.rtp_session);
	ms_message("vst2: rtcp_send_bandwidth=%f, payload_type_bitrate=%d, rtcp_target_bandwidth=%f",
		rtcp_send_bandwidth, payload_type_get_bitrate(vst2_pt), 0.06 * payload_type_get_bitrate(vst2_pt));
	BC_ASSERT_TRUE(rtcp_send_bandwidth <= (0.06 * payload_type_get_bitrate(vst2_pt)));

	destroy_video_stream(vst1);
	destroy_video_stream(vst2);
}

static void change_codec(video_stream_tester_t *vst1, video_stream_tester_t *vst2, int payload_type) {
	MSWebCam *no_webcam = ms_web_cam_manager_get_cam(ms_web_cam_manager_get(), "StaticImage: Static picture");

	if (vst1->payload_type == payload_type) return;

	destroy_video_stream(vst1);
	create_video_stream(vst1, payload_type);
	BC_ASSERT_EQUAL(video_stream_start(vst1->vs, &rtp_profile, vst2->local_ip, vst2->local_rtp, vst2->local_ip, vst2->local_rtcp, payload_type, 50, no_webcam), 0,int,"%d");
}

static void basic_video_stream(void) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	bool_t supported = ms_filter_codec_supported("vp8");

	if (supported) {
		init_video_streams(marielle, margaux, FALSE, FALSE, NULL,VP8_PAYLOAD_TYPE);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_SR, 2, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		video_stream_get_local_rtp_stats(marielle->vs, &marielle->stats.rtp);
		video_stream_get_local_rtp_stats(margaux->vs, &margaux->stats.rtp);

		uninit_video_streams(marielle, margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
}

static void basic_one_way_video_stream(void) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	bool_t supported = ms_filter_codec_supported("vp8");

	if (supported) {
		init_video_streams(marielle, margaux, FALSE, TRUE, NULL,VP8_PAYLOAD_TYPE);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_RR, 2, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		video_stream_get_local_rtp_stats(marielle->vs, &marielle->stats.rtp);
		video_stream_get_local_rtp_stats(margaux->vs, &margaux->stats.rtp);
		uninit_video_streams(marielle, margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void codec_change_for_video_stream(void) {
	video_stream_tester_t *marielle = video_stream_tester_new();
	video_stream_tester_t *margaux = video_stream_tester_new();
	bool_t vp8_supported = ms_filter_codec_supported("vp8");
	bool_t h264_supported = ms_filter_codec_supported("h264");
	bool_t mp4v_supported = ms_filter_codec_supported("mp4v-es");

	if (vp8_supported) {
		init_video_streams(marielle, margaux, FALSE, FALSE, NULL, VP8_PAYLOAD_TYPE);
		BC_ASSERT_TRUE(wait_for_until(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_decoder_first_image_decoded, 1, 2000));
		BC_ASSERT_TRUE(wait_for_until(&marielle->vs->ms, &margaux->vs->ms, &margaux->stats.number_of_decoder_first_image_decoded, 1, 2000));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_SR, 2, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		if (h264_supported || mp4v_supported) {
			if (h264_supported) change_codec(marielle, margaux, H264_PAYLOAD_TYPE);
			else change_codec(marielle, margaux, MP4V_PAYLOAD_TYPE);
			BC_ASSERT_TRUE(wait_for_until(&marielle->vs->ms, &margaux->vs->ms, &margaux->stats.number_of_decoder_first_image_decoded, 2, 2000));
			BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_SR, 2, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
			if (h264_supported) {
				BC_ASSERT_EQUAL(strcasecmp(margaux->vs->ms.decoder->desc->enc_fmt, "h264"), 0,int,"%d");
			} else {
				BC_ASSERT_EQUAL(strcasecmp(margaux->vs->ms.decoder->desc->enc_fmt, "mp4v-es"), 0,int,"%d");
			}
		} else {
			ms_error("H264 codec is not supported!");
		}
		destroy_video_stream(marielle);
		destroy_video_stream(margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}

	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void multicast_video_stream(void) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	bool_t supported = ms_filter_codec_supported("vp8");
	video_stream_tester_set_local_ip(marielle,"224.1.2.3");
	marielle->local_rtcp=0; /*no rtcp*/
	video_stream_tester_set_local_ip(margaux,"0.0.0.0");


	if (supported) {
		int dummy=0;
		init_video_streams(marielle, margaux, FALSE, TRUE, NULL,VP8_PAYLOAD_TYPE);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &margaux->stats.number_of_SR, 2, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		ms_ticker_detach(margaux->vs->ms.sessions.ticker,margaux->vs->source); /*to stop sending*/
		/*make sure packets can cross from sender to receiver*/
		wait_for_until(&marielle->vs->ms,&margaux->vs->ms,&dummy,1,500);

		video_stream_get_local_rtp_stats(marielle->vs, &marielle->stats.rtp);
		video_stream_get_local_rtp_stats(margaux->vs, &marielle->stats.rtp);
		BC_ASSERT_EQUAL(margaux->stats.rtp.sent,marielle->stats.rtp.recv,int,"%d");

		uninit_video_streams(marielle, margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void avpf_video_stream(void) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = ms_filter_codec_supported("vp8");

	if (supported) {
		params.enabled = TRUE;
		params.loss_rate = 5.;
		init_video_streams(marielle, margaux, TRUE, FALSE, &params,VP8_PAYLOAD_TYPE);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_SR, 2, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_SLI, 1, 5000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_RPSI, 1, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		uninit_video_streams(marielle, margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void avpf_rpsi_count(void) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = ms_filter_codec_supported("vp8");
	int dummy=0;
	int delay = 11000;
	marielle->vconf=ms_new0(MSVideoConfiguration,1);
	marielle->vconf->bitrate_limit=marielle->vconf->required_bitrate=256000;
	marielle->vconf->fps=15;
	marielle->vconf->vsize.height=MS_VIDEO_SIZE_CIF_H;
	marielle->vconf->vsize.width=MS_VIDEO_SIZE_CIF_W;
	marielle->cam = mediastreamer2_tester_get_mire_webcam(ms_web_cam_manager_get());


	margaux->vconf=ms_new0(MSVideoConfiguration,1);
	margaux->vconf->bitrate_limit=margaux->vconf->required_bitrate=256000;
	margaux->vconf->fps=5; /*to save cpu resource*/
	margaux->vconf->vsize.height=MS_VIDEO_SIZE_CIF_H;
	margaux->vconf->vsize.width=MS_VIDEO_SIZE_CIF_W;
	margaux->cam = mediastreamer2_tester_get_mire_webcam(ms_web_cam_manager_get());

	if (supported) {
		init_video_streams(marielle, margaux, TRUE, FALSE, &params,VP8_PAYLOAD_TYPE);
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,  &marielle->stats.number_of_decoder_first_image_decoded, 1, 10000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,  &margaux->stats.number_of_decoder_first_image_decoded, 1, 10000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		/*wait for 4 rpsi*/
		wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,  &dummy, 1, delay, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats);
		BC_ASSERT_EQUAL(marielle->stats.number_of_RPSI,4,int,"%d");
		BC_ASSERT_EQUAL(margaux->stats.number_of_RPSI,4,int,"%d");
		BC_ASSERT_LOWER(fabs(video_stream_get_received_framerate(marielle->vs)-margaux->vconf->fps), 2.f, float, "%f");
		BC_ASSERT_LOWER(fabs(video_stream_get_received_framerate(margaux->vs)-marielle->vconf->fps), 2.f, float, "%f");
		uninit_video_streams(marielle, margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void video_stream_first_iframe_lost_vp8(void) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = ms_filter_codec_supported("vp8");

	if (supported) {
		int dummy=0;
		/* Make sure first Iframe is lost. */
		params.enabled = TRUE;
		params.loss_rate = 100.;
		init_video_streams(marielle, margaux, FALSE, FALSE, &params, VP8_PAYLOAD_TYPE);
		wait_for_until(&marielle->vs->ms, &margaux->vs->ms,&dummy,1,1000);

		/* Use 10% packet lost to be sure to have decoding errors. */
		params.enabled=TRUE;
		params.loss_rate = 10.;
		rtp_session_enable_network_simulation(marielle->vs->ms.sessions.rtp_session, &params);
		rtp_session_enable_network_simulation(margaux->vs->ms.sessions.rtp_session, &params);
		wait_for_until(&marielle->vs->ms, &margaux->vs->ms,&dummy,1,2000);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_decoder_decoding_error,
			1, 1000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &margaux->stats.number_of_decoder_decoding_error,
			1, 1000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		/* Remove the lost to be sure the forced iframe is going through. */
		params.enabled=TRUE;
		params.loss_rate = 0.;
		rtp_session_enable_network_simulation(marielle->vs->ms.sessions.rtp_session, &params);
		rtp_session_enable_network_simulation(margaux->vs->ms.sessions.rtp_session, &params);
		wait_for_until(&marielle->vs->ms, &margaux->vs->ms,&dummy,1,2000);
		video_stream_send_vfu(marielle->vs);
		video_stream_send_vfu(margaux->vs);
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_decoder_first_image_decoded,
			1, 5000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &margaux->stats.number_of_decoder_first_image_decoded,
			1, 5000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		uninit_video_streams(marielle, margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void avpf_video_stream_first_iframe_lost_base(int payload_type) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = FALSE;

	switch (payload_type) {
		case VP8_PAYLOAD_TYPE:
			supported = ms_filter_codec_supported("vp8");
			break;
		case H264_PAYLOAD_TYPE:
			supported = ms_filter_codec_supported("h264");
			break;
		default:
			break;
	}

	if (supported) {
		int dummy=0;
		/* Make sure first Iframe is lost. */
		params.enabled = TRUE;
		params.loss_rate = 100.;
		init_video_streams(marielle, margaux, TRUE, FALSE, &params, payload_type);
		wait_for_until(&marielle->vs->ms, &margaux->vs->ms,&dummy,1,1000);

		/* Remove the lost to be sure that a PLI will be sent and not a SLI. */
		params.enabled=TRUE;
		params.loss_rate = 0.;
		rtp_session_enable_network_simulation(marielle->vs->ms.sessions.rtp_session, &params);
		rtp_session_enable_network_simulation(margaux->vs->ms.sessions.rtp_session, &params);
		wait_for_until(&marielle->vs->ms, &margaux->vs->ms,&dummy,1,2000);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_PLI,
			1, 1000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &margaux->stats.number_of_PLI,
			1, 1000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_decoder_first_image_decoded,
			1, 5000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &margaux->stats.number_of_decoder_first_image_decoded,
			1, 5000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		uninit_video_streams(marielle, margaux);
	} else {
		ms_error("Codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void avpf_video_stream_first_iframe_lost_vp8(void) {
	avpf_video_stream_first_iframe_lost_base(VP8_PAYLOAD_TYPE);
}

static void avpf_video_stream_first_iframe_lost_h264(void) {
	avpf_video_stream_first_iframe_lost_base(H264_PAYLOAD_TYPE);
}

static void avpf_high_loss_video_stream_base(float rate, int payload_type) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = FALSE;

	switch (payload_type) {
		case VP8_PAYLOAD_TYPE:
			supported = ms_filter_codec_supported("vp8");
			break;
		case H264_PAYLOAD_TYPE:
			supported = ms_filter_codec_supported("h264");
			break;
		default:
			break;
	}

	if (supported) {
		params.enabled = TRUE;
		params.loss_rate = rate;
		init_video_streams(marielle, margaux, TRUE, FALSE, &params, payload_type);
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
			&marielle->stats.number_of_SR, 10, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		switch (payload_type) {
			case VP8_PAYLOAD_TYPE:
				BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
					&marielle->stats.number_of_SLI, 1, 5000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
				if (rate <= 10) {
					BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
						&marielle->stats.number_of_RPSI, 1, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
				}
				break;
			case H264_PAYLOAD_TYPE:
				BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
					&marielle->stats.number_of_PLI, 1, 5000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
				break;
			default:
				break;
		}
		uninit_video_streams(marielle, margaux);
	} else {
		ms_error("Codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void avpf_very_high_loss_video_stream_vp8(void) {
	avpf_high_loss_video_stream_base(25., VP8_PAYLOAD_TYPE);
}

static void avpf_high_loss_video_stream_vp8(void) {
	avpf_high_loss_video_stream_base(10., VP8_PAYLOAD_TYPE);
}

static void avpf_high_loss_video_stream_h264(void) {
	avpf_high_loss_video_stream_base(10., H264_PAYLOAD_TYPE);
}

static void video_configuration_stream_base(MSVideoConfiguration* asked, MSVideoConfiguration* expected_result, int payload_type) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	PayloadType* pt = rtp_profile_get_payload(&rtp_profile, payload_type);
	bool_t supported = pt?ms_filter_codec_supported(pt->mime_type):FALSE;

	if (supported) {
		margaux->vconf=ms_new0(MSVideoConfiguration,1);
		margaux->vconf->required_bitrate=asked->required_bitrate;
		margaux->vconf->bitrate_limit=asked->bitrate_limit;
		margaux->vconf->vsize=asked->vsize;
		margaux->vconf->fps=asked->fps;

		init_video_streams(marielle, margaux, FALSE, TRUE, NULL,payload_type);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_RR, 4, 30000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		video_stream_get_local_rtp_stats(marielle->vs, &marielle->stats.rtp);
		video_stream_get_local_rtp_stats(margaux->vs, &margaux->stats.rtp);


		BC_ASSERT_TRUE(ms_video_size_equal(video_stream_get_received_video_size(marielle->vs),
			margaux->vconf->vsize));
		BC_ASSERT_TRUE(fabs(video_stream_get_received_framerate(marielle->vs)-margaux->vconf->fps) <2);
		if (ms_web_cam_manager_get_cam(ms_web_cam_manager_get(), "StaticImage: Static picture")
				!= ms_web_cam_manager_get_default_cam(ms_web_cam_manager_get())) {
			// BC_ASSERT_TRUE(abs(media_stream_get_down_bw((MediaStream*)marielle->vs) - margaux->vconf->required_bitrate) < 0.20f * margaux->vconf->required_bitrate);
		} /*else this test require a real webcam*/


		uninit_video_streams(marielle, margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);

}
static void video_configuration_stream(void) {
	MSVideoConfiguration asked;
	MSVideoConfiguration expected;
	asked.bitrate_limit=expected.bitrate_limit=1024000;
	asked.required_bitrate=expected.required_bitrate=1024000;
	asked.fps=expected.fps=12;
	asked.vsize.width=expected.vsize.width=MS_VIDEO_SIZE_VGA_W;
	asked.vsize.height=expected.vsize.height=MS_VIDEO_SIZE_VGA_H;
	video_configuration_stream_base(&asked,&expected,VP8_PAYLOAD_TYPE);

	/*Test video rotation (inverted height <-> width). Not supported on desktop
	because no real use case yet.*/
#if defined(ANDROID) || defined(TARGET_OS_IPHONE)
	asked.bitrate_limit=expected.bitrate_limit=1024000;
	asked.required_bitrate=expected.required_bitrate=1024000;
	asked.fps=expected.fps=12;
	asked.vsize.height=expected.vsize.height=MS_VIDEO_SIZE_VGA_W;
	asked.vsize.width=expected.vsize.width=MS_VIDEO_SIZE_VGA_H;
	video_configuration_stream_base(&asked,&expected,VP8_PAYLOAD_TYPE);
#endif
}

static test_t tests[] = {
	{ "Basic video stream", basic_video_stream },
	{ "Multicast video stream",multicast_video_stream },
	{ "Basic one-way video stream", basic_one_way_video_stream },
	{ "Codec change for video stream", codec_change_for_video_stream },
	{ "AVPF video stream", avpf_video_stream },
	{ "AVPF high-loss video stream VP8", avpf_high_loss_video_stream_vp8 },
	{ "AVPF very high-loss video stream VP8", avpf_very_high_loss_video_stream_vp8 },
	{ "AVPF high-loss video stream H264", avpf_high_loss_video_stream_h264 },
	{ "AVPF video stream first iframe lost VP8", avpf_video_stream_first_iframe_lost_vp8 },
	{ "AVPF video stream first iframe lost H264", avpf_video_stream_first_iframe_lost_h264 },
	{ "AVP video stream first iframe lost", video_stream_first_iframe_lost_vp8 },
	{ "Video configuration", video_configuration_stream },
	{ "AVPF RPSI count", avpf_rpsi_count}
};

test_suite_t video_stream_test_suite = {
	"VideoStream",
	tester_before_all,
	tester_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
#else
test_suite_t video_stream_test_suite = {
	"VideoStream",
	NULL,
	NULL,
	0,
	NULL
};
#endif
