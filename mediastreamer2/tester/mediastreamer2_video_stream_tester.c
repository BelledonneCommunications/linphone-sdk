/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include <math.h>
#include <ortp/port.h>
#include "mediastreamer2/msitc.h"
#include "filters/framemarking_tester.h"

#ifdef _MSC_VER
#define unlink _unlink
#endif

#ifndef TARGET_OS_OSX
#define TARGET_OS_OSX TARGET_OS_MAC && !(TARGET_OS_IPHONE || TARGET_OS_SIMULATOR || TARGET_OS_EMBEDDED)
#endif

typedef struct _CodecsManager CodecsManager;

static RtpProfile rtp_profile;
static MSFactory* _factory = NULL;
static CodecsManager *_h264_codecs_manager = NULL;


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

MSWebCam * mediastreamer2_tester_get_mire(MSFactory *factory){
	return mediastreamer2_tester_get_mire_webcam(ms_factory_get_web_cam_manager(factory));
}

struct _CodecsManager {
	bctbx_list_t *encoders;
	bctbx_list_t *decoders;
};

typedef void (*CodecManagerTestFunc)(void);

static CodecsManager *codecs_manager_new(MSFactory *factory, const char *mime) {
	bctbx_list_t *it;
	CodecsManager *cm = ms_new0(CodecsManager, 1);
	cm->decoders = NULL;
	cm->encoders = NULL;
	for (it=factory->desc_list; it; it=bctbx_list_next(it)) {
		MSFilterDesc *desc = (MSFilterDesc *)it->data;
		if (desc->enc_fmt && strcasecmp(desc->enc_fmt, mime) == 0 && (desc->flags & MS_FILTER_IS_ENABLED)) {
			if (desc->category == MS_FILTER_ENCODER && desc->category != MS_FILTER_ENCODING_CAPTURER) {
				cm->encoders = bctbx_list_append(cm->encoders, desc);
			} else if (desc->category == MS_FILTER_DECODER) {
				cm->decoders = bctbx_list_append(cm->decoders, desc);
			}
		}
	}
	return cm;
}

static void codecs_manager_free(CodecsManager *cm) {
	bctbx_list_free(cm->decoders);
	bctbx_list_free(cm->encoders);
}

static void codecs_manager_enable_all_codecs(const CodecsManager *cm, MSFilterCategory category, bool_t enable) {
	const bctbx_list_t *it;
	if (category == MS_FILTER_ENCODER) {
		it = cm->encoders;
	} else if (category == MS_FILTER_DECODER) {
		it = cm->decoders;
	} else {
		ms_error("CodecsManager: fail to enable all codecs: invalid category [%d]", category);
		return;
	}
	for (; it; it=bctbx_list_next(it)) {
		MSFilterDesc *desc = (MSFilterDesc *)it->data;
		if (enable) desc->flags |= MS_FILTER_IS_ENABLED;
		else desc->flags &= ~MS_FILTER_IS_ENABLED;
	}
}

static void codecs_manager_enable_only_one_codec(const CodecsManager *cm, MSFilterDesc *desc) {
	if (desc->category == MS_FILTER_ENCODER || desc->category == MS_FILTER_DECODER) {
		codecs_manager_enable_all_codecs(cm, desc->category, FALSE);
		desc->flags |= MS_FILTER_IS_ENABLED;
	}
}

static void codecs_manager_test_all_combinations(const CodecsManager *cm, CodecManagerTestFunc test_func) {
	const bctbx_list_t *enc_it, *dec_it;
	for (enc_it=cm->encoders; enc_it; enc_it=bctbx_list_next(enc_it)) {
		MSFilterDesc *enc_desc = (MSFilterDesc *)enc_it->data;
		codecs_manager_enable_only_one_codec(cm, enc_desc);
		for (dec_it=cm->decoders; dec_it; dec_it=bctbx_list_next(dec_it)) {
			MSFilterDesc *dec_desc = (MSFilterDesc *)dec_it->data;
			codecs_manager_enable_only_one_codec(cm, dec_desc);
			ms_message("CodecsManager: calling test function [%p] with %s as encoder and %s as decoder",
					   test_func, enc_desc->name, dec_desc->name);
			test_func();
		}
	}
	codecs_manager_enable_all_codecs(cm, MS_FILTER_ENCODER, TRUE);
	codecs_manager_enable_all_codecs(cm, MS_FILTER_DECODER, TRUE);
}

static int tester_before_all(void) {
	MSWebCamManager *cam_manager = NULL;
	MSWebCam *mire = ms_web_cam_new(ms_mire_webcam_desc_get());

	_factory = ms_factory_new_with_voip();

	ms_factory_create_filter(_factory, TRUE);
	ortp_init();
	rtp_profile_set_payload(&rtp_profile, VP8_PAYLOAD_TYPE, &payload_type_vp8);
	rtp_profile_set_payload(&rtp_profile, H264_PAYLOAD_TYPE, &payload_type_h264);
	rtp_profile_set_payload(&rtp_profile, MP4V_PAYLOAD_TYPE, &payload_type_mp4v);

	_h264_codecs_manager = codecs_manager_new(_factory, "h264");

	cam_manager = ms_factory_get_web_cam_manager(_factory);
	//Disable h264 camera by default because of potential conflict with other h264 encoders
	ms_factory_enable_filter_from_name(_factory, "h264camera", FALSE);
	if(mire != NULL) ms_web_cam_manager_add_cam(cam_manager, mire);

	return 0;
}

static int tester_after_all(void) {
	codecs_manager_free(_h264_codecs_manager);
	ms_factory_destroy(_factory);
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
	int number_of_sent_PLI;
	int number_of_sent_SLI;
	int number_of_sent_RPSI;
	int number_of_sent_NACK;

	int number_of_decoder_decoding_error;
	int number_of_decoder_first_image_decoded;
	int number_of_decoder_send_request_pli;
	int number_of_decoder_send_request_sli;
	int number_of_decoder_send_request_rpsi;

	int number_of_jitter_update_nack;

    int number_of_packet_reconstructed;

	int number_of_framemarking_start;
	int number_of_framemarking_end;
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

video_stream_tester_t* video_stream_tester_new(void) {
	video_stream_tester_t* vst = ms_new0(video_stream_tester_t,1);
	video_stream_tester_set_local_ip(vst,"127.0.0.1");
	vst->cam = ms_web_cam_manager_get_cam(ms_factory_get_web_cam_manager(_factory), "Mire: Mire (synthetic moving picture)");
	vst->local_rtp=-1; /*random*/
	vst->local_rtcp=-1; /*random*/
#if TARGET_OS_OSX
	vst->vconf = ms_new0(MSVideoConfiguration, 1);
	vst->vconf->vsize = MS_VIDEO_SIZE_VGA;
	vst->vconf->required_bitrate = 500000;
#endif
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
		vs_tester->stats.number_of_decoder_send_request_pli++;
		break;
	case MS_VIDEO_DECODER_SEND_SLI:
		event_name="MS_VIDEO_DECODER_SEND_SLI";
		vs_tester->stats.number_of_decoder_send_request_sli++;
		break;
	case MS_VIDEO_DECODER_SEND_RPSI:
		vs_tester->stats.number_of_decoder_send_request_rpsi++;
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
								st->number_of_sent_PLI++;
								ms_message("event_queue_cb: [%p] sending PLI %d", st, st->number_of_sent_PLI);
								break;
							case RTCP_PSFB_SLI:
								st->number_of_sent_SLI++;
								ms_message("event_queue_cb: [%p] sending SLI %d", st, st->number_of_sent_SLI);
								break;
							case RTCP_PSFB_RPSI:
								st->number_of_sent_RPSI++;
								ms_message("event_queue_cb: [%p] sending RPSI %d", st, st->number_of_sent_RPSI);
								break;
							default:
								break;
						}
					} else if (rtcp_is_RTPFB(d->packet)) {
						switch (rtcp_RTPFB_get_type(d->packet)) {
							case RTCP_RTPFB_NACK:
								st->number_of_sent_NACK++;
								ms_message("event_queue_cb: [%p] sending NACK %d", st, st->number_of_sent_NACK);
								break;
							default:
								break;
						}
					}
				} while (rtcp_next_packet(d->packet));
			} else if (evt == ORTP_EVENT_JITTER_UPDATE_FOR_NACK) {
				st->number_of_jitter_update_nack++;
				ms_message("event_queue_cb: [%p] jitter update NACK %d", st, st->number_of_jitter_update_nack);
            } else if (evt == ORTP_EVENT_SOURCE_PACKET_RECONSTRUCTED){
                st->number_of_packet_reconstructed++;
            }
			ortp_event_destroy(ev);
		}
	}
}

static void create_video_stream(video_stream_tester_t *vst, int payload_type) {
	vst->vs = video_stream_new2(_factory, vst->local_ip, vst->local_rtp, vst->local_rtcp);
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
		if (BC_ASSERT_PTR_NOT_NULL(pt)) {
			pt->normal_bitrate = vst->vconf->required_bitrate;
		}
		video_stream_set_fps(vst->vs, vst->vconf->fps);
		video_stream_set_sent_video_size(vst->vs, vst->vconf->vsize);
	}
	vst->payload_type = payload_type;
}

static void destroy_video_stream(video_stream_tester_t *vst) {
	video_stream_stop(vst->vs);
	ortp_ev_queue_destroy(vst->stats.q);
}

static void init_video_streams(video_stream_tester_t *vst1, video_stream_tester_t *vst2, bool_t avpf, bool_t one_way, OrtpNetworkSimulatorParams *params, int payload_type, bool_t nack, bool_t fec) {
	PayloadType *pt;

	create_video_stream(vst1, payload_type);
	create_video_stream(vst2, payload_type);

	/* Enable/disable avpf. */
	pt = rtp_profile_get_payload(&rtp_profile, payload_type);
	if (BC_ASSERT_PTR_NOT_NULL(pt)) {
		if (avpf == TRUE) {
			payload_type_set_flag(pt, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
		} else {
			payload_type_unset_flag(pt, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
		}
	}

	/* Configure network simulator. */
	if ((params != NULL) && (params->enabled == TRUE)) {
		rtp_session_enable_network_simulation(vst1->vs->ms.sessions.rtp_session, params);
		rtp_session_enable_network_simulation(vst2->vs->ms.sessions.rtp_session, params);
	}

	if (one_way == TRUE) {
		media_stream_set_direction(&vst1->vs->ms, MediaStreamRecvOnly);
	}

	if (nack == TRUE) {
		rtp_session_enable_avpf_feature(video_stream_get_rtp_session(vst2->vs), ORTP_AVPF_FEATURE_GENERIC_NACK, TRUE);
		rtp_session_enable_avpf_feature(video_stream_get_rtp_session(vst2->vs), ORTP_AVPF_FEATURE_IMMEDIATE_NACK, TRUE);
		video_stream_enable_retransmission_on_nack(vst1->vs, TRUE);
		video_stream_enable_retransmission_on_nack(vst2->vs, TRUE);
    }

    if (fec == TRUE) {
        video_stream_enable_fec(vst1->vs, vst1->local_ip, vst1->local_rtp, vst1->local_rtcp, vst2->local_ip, vst2->local_rtp, 3, 0);
        video_stream_enable_fec(vst2->vs, vst2->local_ip, vst2->local_rtp, vst2->local_rtcp, vst1->local_ip, vst1->local_rtp, 3, 0);
    }

	BC_ASSERT_EQUAL(video_stream_start(vst1->vs, &rtp_profile, vst2->local_ip, vst2->local_rtp, vst2->local_ip, vst2->local_rtcp, payload_type, 50, vst1->cam), 0,int,"%d");
	BC_ASSERT_EQUAL(video_stream_start(vst2->vs, &rtp_profile, vst1->local_ip, vst1->local_rtp, vst1->local_ip, vst1->local_rtcp, payload_type, 50, vst2->cam), 0,int,"%d");
}

static void uninit_video_streams(video_stream_tester_t *vst1, video_stream_tester_t *vst2) {
	float rtcp_send_bandwidth;
	PayloadType *vst1_pt;
	PayloadType *vst2_pt;

	vst1_pt = rtp_profile_get_payload(&rtp_profile, vst1->payload_type);
	vst2_pt = rtp_profile_get_payload(&rtp_profile, vst2->payload_type);
	if (!BC_ASSERT_PTR_NOT_NULL(vst1_pt) || !BC_ASSERT_PTR_NOT_NULL(vst2_pt)) return;

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
	MSWebCam *no_webcam = ms_web_cam_manager_get_cam(ms_factory_get_web_cam_manager(_factory), "StaticImage: Static picture");

	if (vst1->payload_type == payload_type) return;

	destroy_video_stream(vst1);
	create_video_stream(vst1, payload_type);
	BC_ASSERT_EQUAL(video_stream_start(vst1->vs, &rtp_profile, vst2->local_ip, vst2->local_rtp, vst2->local_ip, vst2->local_rtcp, payload_type, 50, no_webcam), 0,int,"%d");
}

static void basic_video_stream_base(int payload_type) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();

    init_video_streams(marielle, margaux, FALSE, FALSE, NULL, payload_type, FALSE, FALSE);

	BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms,
													&margaux->vs->ms,
													&marielle->stats.number_of_SR,
													2, 15000, event_queue_cb,
													&marielle->stats, event_queue_cb,
													&margaux->stats));

	video_stream_get_local_rtp_stats(marielle->vs, &marielle->stats.rtp);
	video_stream_get_local_rtp_stats(margaux->vs, &margaux->stats.rtp);

    uninit_video_streams(marielle, margaux);

	video_stream_tester_destroy(margaux);
	video_stream_tester_destroy(marielle);
}

static void basic_video_stream_vp8(void) {
	if(ms_factory_codec_supported(_factory, "vp8")) {
		basic_video_stream_base(VP8_PAYLOAD_TYPE);
	} else {
		ms_error("VP8 codec is not supported!");
	}
}

static void basic_video_stream_h264(void) {
	if(ms_factory_codec_supported(_factory, "h264")) {
		basic_video_stream_base(H264_PAYLOAD_TYPE);
	} else {
		ms_error("H264 codec is not supported!");
	}
}

static void basic_video_stream_all_h264_codec_combinations(void) {
	codecs_manager_test_all_combinations(_h264_codecs_manager, basic_video_stream_h264);
}

static void basic_one_way_video_stream(void) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	bool_t supported = ms_factory_codec_supported(_factory, "vp8");

	if (supported) {
        init_video_streams(marielle, margaux, FALSE, TRUE, NULL,VP8_PAYLOAD_TYPE, FALSE, FALSE);

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
	bool_t vp8_supported = ms_factory_codec_supported(_factory, "vp8");
	bool_t h264_supported = ms_factory_codec_supported(_factory, "h264");
	bool_t mp4v_supported = ms_factory_codec_supported(_factory, "mp4v-es");

	if (vp8_supported) {
        init_video_streams(marielle, margaux, FALSE, FALSE, NULL, VP8_PAYLOAD_TYPE, FALSE, FALSE);
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
	bool_t supported = ms_factory_codec_supported(_factory, "vp8");
	video_stream_tester_set_local_ip(marielle,"224.1.2.3");
	marielle->local_rtcp=0; /*no rtcp*/
	video_stream_tester_set_local_ip(margaux,"0.0.0.0");

	if (supported) {
		int dummy=0;
        init_video_streams(marielle, margaux, FALSE, TRUE, NULL,VP8_PAYLOAD_TYPE, FALSE, FALSE);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &margaux->stats.number_of_SR, 2, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		ms_ticker_detach(margaux->vs->ms.sessions.ticker,margaux->vs->source); /*to stop sending*/
		/*make sure packets can cross from sender to receiver*/
		wait_for_until(&marielle->vs->ms,&margaux->vs->ms,&dummy,1,500);

		video_stream_get_local_rtp_stats(marielle->vs, &marielle->stats.rtp);
		video_stream_get_local_rtp_stats(margaux->vs, &marielle->stats.rtp);
		BC_ASSERT_EQUAL(margaux->stats.rtp.sent,marielle->stats.rtp.recv,unsigned long long,"%llu");

        uninit_video_streams(marielle, margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void avpf_video_stream_base(int payload_type) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported;
    int dummy = 0;

	switch(payload_type) {
	case VP8_PAYLOAD_TYPE:
		supported = ms_factory_codec_supported(_factory, "vp8");
		break;
	case H264_PAYLOAD_TYPE:
		supported = ms_factory_codec_supported(_factory, "h264");
		break;
	default:
		supported = FALSE;
		break;
	}

	if (supported) {
		params.enabled = TRUE;
		params.loss_rate = 5.;
		params.rtp_only = TRUE;
        init_video_streams(marielle, margaux, TRUE, FALSE, &params, payload_type, FALSE, FALSE);

        BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_SR, 2, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));



		if (payload_type == VP8_PAYLOAD_TYPE) {
			BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_sent_SLI, 1, 5000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
			BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_sent_RPSI, 1, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		}

        //disable network simulator
        params.enabled = FALSE;
        rtp_session_enable_network_simulation(marielle->vs->ms.sessions.rtp_session, &params);
        rtp_session_enable_network_simulation(margaux->vs->ms.sessions.rtp_session, &params);

        wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &dummy, 1, 1000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats);

         // wait for all packets reception before closing streams
		if (payload_type == VP8_PAYLOAD_TYPE) {
			BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &margaux->vs->ms_video_stat.counter_rcvd_rpsi , marielle->stats.number_of_sent_RPSI, 10000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
			BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->vs->ms_video_stat.counter_rcvd_rpsi , margaux->stats.number_of_sent_RPSI, 10000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		}
        BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &margaux->vs->ms_video_stat.counter_rcvd_pli, marielle->stats.number_of_sent_PLI, 10000,event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
        BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->vs->ms_video_stat.counter_rcvd_pli , margaux->stats.number_of_sent_PLI,10000,event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		if (payload_type == VP8_PAYLOAD_TYPE) {
			BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &margaux->vs->ms_video_stat.counter_rcvd_sli , marielle->stats.number_of_sent_SLI,10000,event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
			BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->vs->ms_video_stat.counter_rcvd_sli , margaux->stats.number_of_sent_SLI,10000,event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		}

        BC_ASSERT_EQUAL(margaux->vs->ms_video_stat.counter_rcvd_sli , marielle->stats.number_of_sent_SLI, int, "%d");
        BC_ASSERT_EQUAL(margaux->vs->ms_video_stat.counter_rcvd_rpsi , marielle->stats.number_of_sent_RPSI, int, "%d");
        BC_ASSERT_EQUAL(margaux->vs->ms_video_stat.counter_rcvd_pli , marielle->stats.number_of_sent_PLI, int, "%d");

        BC_ASSERT_EQUAL(marielle->vs->ms_video_stat.counter_rcvd_sli , margaux->stats.number_of_sent_SLI, int, "%d");
        BC_ASSERT_EQUAL(marielle->vs->ms_video_stat.counter_rcvd_rpsi , margaux->stats.number_of_sent_RPSI, int, "%d");
        BC_ASSERT_EQUAL(marielle->vs->ms_video_stat.counter_rcvd_pli , margaux->stats.number_of_sent_PLI, int, "%d");

        uninit_video_streams(marielle, margaux);
	} else {
		ms_error("Codec is not supported!");
	}

	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void avpf_video_stream_vp8(void) {
	avpf_video_stream_base(VP8_PAYLOAD_TYPE);
}

static void avpf_video_stream_h264(void) {
	avpf_video_stream_base(H264_PAYLOAD_TYPE);
}

static void avpf_video_stream_all_h264_codec_combinations(void) {
	codecs_manager_test_all_combinations(_h264_codecs_manager, avpf_video_stream_h264);
}

static void avpf_rpsi_count(void) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = ms_factory_codec_supported(_factory, "vp8");
	int dummy=0;
	int delay = 11000;
	marielle->vconf=ms_new0(MSVideoConfiguration,1);
	marielle->vconf->bitrate_limit=marielle->vconf->required_bitrate=256000;
	marielle->vconf->fps=15;
	marielle->vconf->vsize.height=MS_VIDEO_SIZE_CIF_H;
	marielle->vconf->vsize.width=MS_VIDEO_SIZE_CIF_W;
	marielle->cam = mediastreamer2_tester_get_mire_webcam(ms_factory_get_web_cam_manager(_factory));


	margaux->vconf=ms_new0(MSVideoConfiguration,1);
	margaux->vconf->bitrate_limit=margaux->vconf->required_bitrate=256000;
	margaux->vconf->fps=5; /*to save cpu resource*/
	margaux->vconf->vsize.height=MS_VIDEO_SIZE_CIF_H;
	margaux->vconf->vsize.width=MS_VIDEO_SIZE_CIF_W;
	margaux->cam = mediastreamer2_tester_get_mire_webcam(ms_factory_get_web_cam_manager(_factory));

	if (supported) {
        init_video_streams(marielle, margaux, TRUE, FALSE, &params,VP8_PAYLOAD_TYPE, FALSE, FALSE);
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,  &marielle->stats.number_of_decoder_first_image_decoded, 1, 10000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,  &margaux->stats.number_of_decoder_first_image_decoded, 1, 10000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		/*wait for 4 rpsi*/
		wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,  &dummy, 1, delay, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats);
		BC_ASSERT_EQUAL(marielle->stats.number_of_sent_RPSI,4,int,"%d");
		BC_ASSERT_EQUAL(margaux->stats.number_of_sent_RPSI,4,int,"%d");
		BC_ASSERT_LOWER((float)fabs(video_stream_get_received_framerate(marielle->vs)-margaux->vconf->fps), 2.f, float, "%f");
		BC_ASSERT_LOWER((float)fabs(video_stream_get_received_framerate(margaux->vs)-marielle->vconf->fps), 2.f, float, "%f");
        uninit_video_streams(marielle, margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void video_stream_first_iframe_lost_base(int payload_type) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported;

	switch(payload_type) {
	case VP8_PAYLOAD_TYPE:
		supported = ms_factory_codec_supported(_factory, "vp8");
		break;
	case H264_PAYLOAD_TYPE:
		supported = ms_factory_codec_supported(_factory, "h264");
		break;
	default:
		supported = FALSE;
		break;
	}

	if (supported) {
		int dummy=0;
		/* Make sure first Iframe is lost. */
		params.enabled = TRUE;
		params.loss_rate = 100.;
        init_video_streams(marielle, margaux, FALSE, FALSE, &params, payload_type, FALSE, FALSE);
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
		ms_error("Codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void video_stream_first_iframe_lost_vp8(void) {
	video_stream_first_iframe_lost_base(VP8_PAYLOAD_TYPE);
}

static void video_stream_first_iframe_lost_h264(void) {
	video_stream_first_iframe_lost_base(H264_PAYLOAD_TYPE);
}

static void video_stream_first_iframe_lost_all_h264_codec_combinations(void) {
	codecs_manager_test_all_combinations(_h264_codecs_manager, video_stream_first_iframe_lost_h264);
}

static void avpf_video_stream_first_iframe_lost_base(int payload_type) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = FALSE;

	switch (payload_type) {
		case VP8_PAYLOAD_TYPE:
			supported = ms_factory_codec_supported(_factory, "vp8");
			break;
		case H264_PAYLOAD_TYPE:
			supported = ms_factory_codec_supported(_factory, "h264");
			break;
		default:
			break;
	}

	if (supported) {
		int dummy=0;
		/* Make sure first Iframe is lost. */
		params.enabled = TRUE;
		params.loss_rate = 100.;
        init_video_streams(marielle, margaux, TRUE, FALSE, &params, payload_type, FALSE, FALSE);
		wait_for_until(&marielle->vs->ms, &margaux->vs->ms,&dummy,1,1000);

		/* Remove the lost to be sure that a PLI will be sent and not a SLI. */
		params.enabled=TRUE;
		params.loss_rate = 0.;
		rtp_session_enable_network_simulation(marielle->vs->ms.sessions.rtp_session, &params);
		rtp_session_enable_network_simulation(margaux->vs->ms.sessions.rtp_session, &params);
		wait_for_until(&marielle->vs->ms, &margaux->vs->ms,&dummy,1,2000);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_sent_PLI,
			1, 1000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &margaux->stats.number_of_sent_PLI,
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

static void avpf_video_stream_first_iframe_lost_all_h264_codec_combinations(void) {
	codecs_manager_test_all_combinations(_h264_codecs_manager, avpf_video_stream_first_iframe_lost_h264);
}

static void avpf_high_loss_video_stream_base(float rate, int payload_type) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = FALSE;

	switch (payload_type) {
		case VP8_PAYLOAD_TYPE:
			supported = ms_factory_codec_supported(_factory, "vp8");
			break;
		case H264_PAYLOAD_TYPE:
			supported = ms_factory_codec_supported(_factory, "h264");
			break;
		default:
			break;
	}

	if (supported) {
		params.enabled = TRUE;
		params.loss_rate = rate;
        init_video_streams(marielle, margaux, TRUE, FALSE, &params, payload_type, FALSE, FALSE);
		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
			&marielle->stats.number_of_SR, 10, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
		switch (payload_type) {
			case VP8_PAYLOAD_TYPE:
				BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
					&marielle->stats.number_of_sent_SLI, 1, 5000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
				if (rate <= 10) {
					BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
						&marielle->stats.number_of_sent_RPSI, 1, 15000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
				}
				break;
			case H264_PAYLOAD_TYPE:
				BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
					&marielle->stats.number_of_sent_PLI, 1, 5000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));
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

static void avpf_high_loss_video_stream_all_h264_codec_conbinations(void) {
	codecs_manager_test_all_combinations(_h264_codecs_manager, avpf_high_loss_video_stream_h264);
}

static void video_configuration_stream_base(MSVideoConfiguration* asked, MSVideoConfiguration* expected_result, int payload_type) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	PayloadType* pt = rtp_profile_get_payload(&rtp_profile, payload_type);
	bool_t supported = pt?ms_factory_codec_supported(_factory, pt->mime_type):FALSE;

	if (supported) {
		margaux->vconf=ms_new0(MSVideoConfiguration,1);
		margaux->vconf->required_bitrate=asked->required_bitrate;
		margaux->vconf->bitrate_limit=asked->bitrate_limit;
		margaux->vconf->vsize=asked->vsize;
		margaux->vconf->fps=asked->fps;

        init_video_streams(marielle, margaux, FALSE, TRUE, NULL,payload_type, FALSE, FALSE);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_RR, 4, 30000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		video_stream_get_local_rtp_stats(marielle->vs, &marielle->stats.rtp);
		video_stream_get_local_rtp_stats(margaux->vs, &margaux->stats.rtp);


		BC_ASSERT_TRUE(ms_video_size_equal(video_stream_get_received_video_size(marielle->vs),
			margaux->vconf->vsize));
		BC_ASSERT_TRUE(fabs(video_stream_get_received_framerate(marielle->vs)-margaux->vconf->fps) <2);
		if (ms_web_cam_manager_get_cam(ms_factory_get_web_cam_manager(_factory), "StaticImage: Static picture")
				!= ms_web_cam_manager_get_default_cam(ms_factory_get_web_cam_manager(_factory))) {
			// BC_ASSERT_TRUE(abs(media_stream_get_down_bw((MediaStream*)marielle->vs) - margaux->vconf->required_bitrate) < 0.20f * margaux->vconf->required_bitrate);
		} /*else this test require a real webcam*/


        uninit_video_streams(marielle, margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);

}

static void video_configuration_stream_vp8(void) {
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
#if defined(__ANDROID__) || defined(TARGET_OS_IPHONE)
	asked.bitrate_limit=expected.bitrate_limit=1024000;
	asked.required_bitrate=expected.required_bitrate=1024000;
	asked.fps=expected.fps=12;
	asked.vsize.height=expected.vsize.height=MS_VIDEO_SIZE_VGA_W;
	asked.vsize.width=expected.vsize.width=MS_VIDEO_SIZE_VGA_H;
	video_configuration_stream_base(&asked,&expected,VP8_PAYLOAD_TYPE);
#endif
}

static void video_configuration_stream_h264(void) {
	MSVideoConfiguration asked;
	MSVideoConfiguration expected;
	asked.bitrate_limit=expected.bitrate_limit=1024000;
	asked.required_bitrate=expected.required_bitrate=1024000;
	asked.fps=expected.fps=12;
	asked.vsize.width=expected.vsize.width=MS_VIDEO_SIZE_VGA_W;
	asked.vsize.height=expected.vsize.height=MS_VIDEO_SIZE_VGA_H;
	video_configuration_stream_base(&asked,&expected,H264_PAYLOAD_TYPE);

	/*Test video rotation (inverted height <-> width). Not supported on desktop
	because no real use case yet.*/
#if defined(__ANDROID__) || defined(TARGET_OS_IPHONE)
	asked.bitrate_limit=expected.bitrate_limit=1024000;
	asked.required_bitrate=expected.required_bitrate=1024000;
	asked.fps=expected.fps=12;
	asked.vsize.height=expected.vsize.height=MS_VIDEO_SIZE_VGA_W;
	asked.vsize.width=expected.vsize.width=MS_VIDEO_SIZE_VGA_H;
	video_configuration_stream_base(&asked,&expected,H264_PAYLOAD_TYPE);
#endif
}

video_stream_tester_t* video_stream_tester_elph264_cam_new(void) {
	video_stream_tester_t* vst = ms_new0(video_stream_tester_t,1);
	video_stream_tester_set_local_ip(vst,"127.0.0.1");
	vst->cam = ms_web_cam_manager_get_cam(ms_factory_get_web_cam_manager(_factory), "ELP-USB100W04H: /dev/elp-h264");
	vst->local_rtp=-1; /*random*/
	vst->local_rtcp=-1; /*random*/
	vst->vconf = ms_new0(MSVideoConfiguration, 1);
	vst->vconf->vsize = MS_VIDEO_SIZE_720P;
	vst->vconf->fps = 30;
	vst->vconf->required_bitrate = 500000;
	return  vst;
}

static void video_stream_elph264_camera(void) {
	MSVideoConfiguration asked;
	MSVideoConfiguration expected;
	video_stream_tester_t* marielle = NULL;
	video_stream_tester_t* margaux = NULL;
	PayloadType* pt = NULL;
	bool_t supported = FALSE;

	if (bctbx_file_exist("/dev/elp-h264") != 0) {
		ms_error("Ignoring video_stream_elph264_camera test because required device is not present.");
		return;
	} else {
		ms_factory_enable_filter_from_name(_factory, "h264camera", TRUE);
	}
	asked.bitrate_limit=expected.bitrate_limit=1024000;
	asked.required_bitrate=expected.required_bitrate=1024000;
	asked.fps=expected.fps=30;
	asked.vsize.width=expected.vsize.width=MS_VIDEO_SIZE_720P_W;
	asked.vsize.height=expected.vsize.height=MS_VIDEO_SIZE_720P_H;

	marielle=video_stream_tester_elph264_cam_new();
	margaux=video_stream_tester_elph264_cam_new();
	pt = rtp_profile_get_payload(&rtp_profile, H264_PAYLOAD_TYPE);
	supported = pt ? ms_factory_codec_supported(_factory, pt->mime_type) : FALSE;

	if (supported) {
		margaux->vconf = ms_new0(MSVideoConfiguration, 1);
		margaux->vconf->required_bitrate = asked.required_bitrate;
		margaux->vconf->bitrate_limit = asked.bitrate_limit;
		margaux->vconf->vsize = asked.vsize;
		margaux->vconf->fps = asked.fps;

        init_video_streams(marielle, margaux, FALSE, TRUE, NULL, H264_PAYLOAD_TYPE, FALSE, FALSE);

		BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms, &marielle->stats.number_of_RR, 4, 30000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

		video_stream_get_local_rtp_stats(marielle->vs, &marielle->stats.rtp);
		video_stream_get_local_rtp_stats(margaux->vs, &margaux->stats.rtp);

		BC_ASSERT_TRUE(ms_video_size_equal(video_stream_get_received_video_size(marielle->vs), margaux->vconf->vsize));
		BC_ASSERT_TRUE(fabs(video_stream_get_received_framerate(marielle->vs)-margaux->vconf->fps) < 2);

        uninit_video_streams(marielle, margaux);
	} else {
		ms_error("H264 codec is not supported!");
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void video_configuration_stream_all_h264_codec_combinations(void) {
	codecs_manager_test_all_combinations(_h264_codecs_manager, video_configuration_stream_h264);
}

static void video_stream_normal_loss_with_retransmission_on_nack(void) {
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = ms_factory_codec_supported(_factory, "vp8");

	if (supported) {
		JBParameters initial_jitter_params, current_jitter_params;
		int dummy = 0;
		params.enabled = TRUE;
		params.loss_rate = 3.;

        init_video_streams(marielle, margaux, TRUE, FALSE, &params, VP8_PAYLOAD_TYPE, TRUE, FALSE);

		rtp_session_get_jitter_buffer_params(margaux->vs->ms.sessions.rtp_session, &initial_jitter_params);

		wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
			&dummy, 1, 10000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats);

		BC_ASSERT_LOWER(margaux->stats.number_of_sent_SLI, 5, int, "%d");

		/* Since there is some loss_rate the nack context should increase the min_size */
		rtp_session_get_jitter_buffer_params(margaux->vs->ms.sessions.rtp_session, &current_jitter_params);
		BC_ASSERT_GREATER(current_jitter_params.min_size, initial_jitter_params.min_size, int, "%d");

		params.loss_rate = 0.;
		rtp_session_enable_network_simulation(marielle->vs->ms.sessions.rtp_session, &params);
		rtp_session_enable_network_simulation(margaux->vs->ms.sessions.rtp_session, &params);

		wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
			&dummy, 1, 6000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats);

		/* Since no NACK is sent in the last 5 seconds, the jitter min size is set back to default */
		rtp_session_get_jitter_buffer_params(margaux->vs->ms.sessions.rtp_session, &current_jitter_params);
		BC_ASSERT_EQUAL(current_jitter_params.min_size, initial_jitter_params.min_size, int, "%d");

        uninit_video_streams(marielle, margaux);
	} else {
		ms_error("Codec is not supported!");
	}

	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
}

static void video_stream_with_itcsink(void) {
	// test for ItcResource and SizeConv
	video_stream_tester_t* marielle=video_stream_tester_new();
	video_stream_tester_t* margaux=video_stream_tester_new();
	video_stream_tester_t* pauline=video_stream_tester_new();
	bool_t supported = ms_factory_codec_supported(_factory, "vp8");
	if (supported) {
		PayloadType *pt;
		create_video_stream(marielle, VP8_PAYLOAD_TYPE);
		create_video_stream(margaux, VP8_PAYLOAD_TYPE);
		create_video_stream(pauline, VP8_PAYLOAD_TYPE);

		/* Enable/disable avpf. */
		pt = rtp_profile_get_payload(&rtp_profile, VP8_PAYLOAD_TYPE);
		if (BC_ASSERT_PTR_NOT_NULL(pt)) {
			payload_type_unset_flag(pt, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
		}
		media_stream_set_direction(&marielle->vs->ms, MediaStreamSendOnly);
		media_stream_set_direction(&margaux->vs->ms, MediaStreamSendOnly);
		media_stream_set_direction(&pauline->vs->ms, MediaStreamRecvOnly);
		marielle->vs->use_preview_window = TRUE;

		BC_ASSERT_EQUAL(video_stream_start(marielle->vs, &rtp_profile, "127.0.0.1",65004, "127.0.0.1", 65005, VP8_PAYLOAD_TYPE, 50, marielle->cam), 0,int,"%d");
		link_video_stream_with_itc_sink(marielle->vs);

		MSMediaStreamIO io = MS_MEDIA_STREAM_IO_INITIALIZER;
		io.input.type = MSResourceItc;
		io.input.itc = marielle->vs->itcsink;
		io.output.type = MSResourceDefault;
		io.output.resource_arg = NULL;
		rtp_session_set_jitter_compensation(margaux->vs->ms.sessions.rtp_session, 50);
		video_stream_start_from_io(margaux->vs, &rtp_profile, pauline->local_ip, pauline->local_rtp, pauline->local_ip, pauline->local_rtcp, VP8_PAYLOAD_TYPE, &io);

		BC_ASSERT_EQUAL(video_stream_start(pauline->vs, &rtp_profile, margaux->local_ip, margaux->local_rtp, margaux->local_ip, margaux->local_rtcp, VP8_PAYLOAD_TYPE, 50, pauline->cam), 0,int,"%d");


		// Run some time to see the result.
		wait_for_until_with_parse_events(&margaux->vs->ms, &pauline->vs->ms, &margaux->stats.number_of_RR, 2, 10000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats);
		
		destroy_video_stream(marielle);
		destroy_video_stream(margaux);
		destroy_video_stream(pauline);
	}
	video_stream_tester_destroy(marielle);
	video_stream_tester_destroy(margaux);
	video_stream_tester_destroy(pauline);
}

mblk_t *fec_stream_on_new_source_packet_sent_test(FecStream *fec_stream, mblk_t *source_packet){
    msgpullup(source_packet, -1);

    if(fec_stream->cpt == 0){
        fec_stream->SSRC = rtp_get_ssrc(source_packet);
        memset(fec_stream->bitstring, 0, UDP_MAX_SIZE * sizeof(uint8_t));
        fec_stream->bitstring[0] = 1 << 6;
    }

    if(fec_stream->max_size < (msgdsize(source_packet) - RTP_FIXED_HEADER_SIZE)) fec_stream->max_size = msgdsize(source_packet) - RTP_FIXED_HEADER_SIZE;

    fec_stream->bitstring[0] ^= rtp_get_padbit(source_packet) << 5;
    fec_stream->bitstring[0] ^= rtp_get_extbit(source_packet) << 4;
    fec_stream->bitstring[0] ^= rtp_get_cc(source_packet);
    fec_stream->bitstring[1] ^= rtp_get_markbit(source_packet) << 7;
    fec_stream->bitstring[1] ^= rtp_get_payload_type(source_packet);

    //Length
    *(uint16_t *) &fec_stream->bitstring[2] ^= htons((uint16_t)(msgdsize(source_packet) - RTP_FIXED_HEADER_SIZE));

    //Timestamp
    *(uint32_t *) &fec_stream->bitstring[4] ^= rtp_get_timestamp(source_packet);

    //All octets after the fixed 12-bytes RTPheader
    for(size_t i = 0 ; i < (msgdsize(source_packet) - RTP_FIXED_HEADER_SIZE) ; i++){
        fec_stream->bitstring[8 + i] ^= *(uint8_t *) (source_packet->b_rptr+RTP_FIXED_HEADER_SIZE+i);
    }

    fec_stream->seqnumlist[fec_stream->cpt] = rtp_get_seqnumber(source_packet);

    fec_stream->cpt++;

    if(fec_stream->cpt == fec_stream->params.L){
        uint16_t *p16 = NULL;
        uint8_t *p8 = NULL;
        mblk_t *repair_packet = rtp_session_create_packet(fec_stream->fec_session, RTP_FIXED_HEADER_SIZE, NULL, 0);

        rtp_set_version(repair_packet, 2);
        rtp_set_padbit(repair_packet, 0);
        rtp_set_extbit(repair_packet, 0);
        rtp_set_markbit(repair_packet, 0);

        msgpullup(repair_packet, msgdsize(repair_packet) + 4 + 8 + fec_stream->params.L*4 + fec_stream->max_size);

        rtp_add_csrc(repair_packet, fec_stream->SSRC);
        repair_packet->b_wptr += sizeof(uint32_t);

        memcpy(repair_packet->b_wptr, &fec_stream->bitstring[0], 8*sizeof(uint8_t));
        repair_packet->b_wptr += 8*sizeof(uint8_t);

        for (int i = 0 ; i < fec_stream->params.L ; i++){
            p16 = (uint16_t *) (repair_packet->b_wptr);
            *p16 = fec_stream->seqnumlist[i];
            repair_packet->b_wptr += sizeof(uint16_t);
            p8 = repair_packet->b_wptr;
            *p8 = fec_stream->params.L;
            repair_packet->b_wptr++;
            p8 = repair_packet->b_wptr;
            *p8 = fec_stream->params.D;
            repair_packet->b_wptr++;
        }

        memcpy(repair_packet->b_wptr, &fec_stream->bitstring[8], fec_stream->max_size);
        repair_packet->b_wptr += fec_stream->max_size;

        fec_stream->cpt = 0;
        fec_stream->max_size = 0;

        return repair_packet;
    }
    return NULL;
}

static void fec_stream_test_reconstruction(void){

    int L = 5;
    int packet_size = 0;
    int pos = 0;

    uint8_t *buffer = NULL;
    mblk_t *packet = NULL;
    mblk_t *repair = NULL;
    mblk_t *lost_packet = NULL;
    mblk_t *new_packet = NULL;
    RtpSession *source_session = rtp_session_new(RTP_SESSION_SENDONLY);
    RtpSession *repair_session = rtp_session_new(RTP_SESSION_SENDONLY);
    const FecParameters *params = fec_params_new(L, 0, 2);
    FecStream *fec_stream = fec_stream_new(source_session, repair_session, params);
    source_session->fec_stream = fec_stream;

    srand((unsigned int) time(NULL));

    for(int stop = 0 ; stop < 1000 ; stop++){

        bool_t reconstruction = FALSE;
        uint16_t num = 0;
        int position = 0;

        rtp_session_set_payload_type(source_session, 114);

        for(int i = 0 ; i < 100 ; i++){
            packet_size = rand()%1400;
            buffer = (uint8_t *) malloc(packet_size*sizeof(uint8_t));
            for(int i = 0 ; i < packet_size ; i++){
                buffer[i] = rand()%256;
            }
            packet = rtp_session_create_packet(source_session, RTP_FIXED_HEADER_SIZE, buffer, packet_size);
            rtp_set_seqnumber(packet, num);
            num++;
            rtp_set_timestamp(packet, rand()%429496729);
            putq(&fec_stream->source_packets_recvd, packet);
            repair = fec_stream_on_new_source_packet_sent_test(fec_stream, packet);
            if(repair != NULL){
                putq(&fec_stream->repair_packets_recvd, repair);
            }
            free(buffer);
        }

        pos = rand()%100;
        for(mblk_t *tmp = qbegin(&fec_stream->source_packets_recvd) ; !qend(&fec_stream->source_packets_recvd, tmp) ; tmp = qnext(&fec_stream->source_packets_recvd, tmp)){
            if(position == pos){
                lost_packet = tmp;
                remq(&fec_stream->source_packets_recvd, lost_packet);
                break;
            } position++;
        }

        new_packet = fec_stream_reconstruct_missing_packet(fec_stream, rtp_get_seqnumber(lost_packet));

        if(msgdsize(lost_packet) == msgdsize(new_packet)){
            if(memcmp(lost_packet->b_rptr, new_packet->b_rptr, msgdsize(lost_packet)) == 0){
                reconstruction = TRUE;
            }
        }

        BC_ASSERT_TRUE(reconstruction);

        flushq(&fec_stream->source_packets_recvd, 0);
        flushq(&fec_stream->repair_packets_recvd, 0);
    }

    rtp_session_destroy(source_session);
}

static void fec_stream_test_lost_repair_packet(void){

    int L = 5;
    int packet_size = 0;
    int pos = 0;

    uint8_t *buffer = NULL;
    mblk_t *packet = NULL;
    mblk_t *lost_packet = NULL;
    mblk_t *new_packet = NULL;
    RtpSession *source_session = rtp_session_new(RTP_SESSION_SENDONLY);
    RtpSession *repair_session = rtp_session_new(RTP_SESSION_SENDONLY);
    const FecParameters *params = fec_params_new(L, 0, 2);
    FecStream *fec_stream = fec_stream_new(source_session, repair_session, params);
    source_session->fec_stream = fec_stream;

    srand((unsigned int) time(NULL));

    for(int stop = 0 ; stop < 1000 ; stop++){

        uint16_t num = 0;
        int position = 0;

        rtp_session_set_payload_type(source_session, 114);

        for(int i = 0 ; i < L ; i++){
            packet_size = rand()%1400;
            buffer = (uint8_t *) malloc(packet_size*sizeof(uint8_t));
            for(int i = 0 ; i < packet_size ; i++){
                buffer[i] = rand()%256;
            }
            packet = rtp_session_create_packet(source_session, RTP_FIXED_HEADER_SIZE, buffer, packet_size);
            rtp_set_seqnumber(packet, num);
            rtp_set_timestamp(packet, rand()%429496729);
            num++;
            putq(&fec_stream->source_packets_recvd, packet);
            free(buffer);
        }

        pos = rand()%L;
        for(mblk_t *tmp = qbegin(&fec_stream->source_packets_recvd) ; !qend(&fec_stream->source_packets_recvd, tmp) ; tmp = qnext(&fec_stream->source_packets_recvd, tmp)){
            if(position == pos){
                lost_packet = tmp;
                remq(&fec_stream->source_packets_recvd, lost_packet);
                break;
            } position++;
        }

        new_packet = fec_stream_reconstruct_missing_packet(fec_stream, rtp_get_seqnumber(lost_packet));

        BC_ASSERT_PTR_NULL(new_packet);

        flushq(&fec_stream->source_packets_recvd, 0);
        flushq(&fec_stream->repair_packets_recvd, 0);
    }

    rtp_session_destroy(source_session);
}

static void fec_stream_test_lost_2_source_packets(void){

    int L = 5;
    int packet_size = 0;
    int pos1 = 0;
    int pos2 = 0;

    uint8_t *buffer = NULL;
    mblk_t *packet = NULL;
    mblk_t *repair = NULL;
    mblk_t *lost_packet = NULL;
    mblk_t *new_packet = NULL;
    RtpSession *source_session = rtp_session_new(RTP_SESSION_SENDONLY);
    RtpSession *repair_session = rtp_session_new(RTP_SESSION_SENDONLY);
    const FecParameters *params = fec_params_new(L, 0, 2);
    FecStream *fec_stream = fec_stream_new(source_session, repair_session, params);
    source_session->fec_stream = fec_stream;

    srand((unsigned int) time(NULL));

    for(int stop = 0 ; stop < 1000 ; stop++){

        uint16_t num = 0;

        rtp_session_set_payload_type(source_session, 114);

        pos1 = rand()%L;
        pos2 = rand()%L;
        pos2 = (pos2 != pos1 ? pos2 : (pos2 + 1) % L);
        for(int i = 0 ; i < L ; i++){
            packet_size = rand()%1400;
            buffer = (uint8_t *) malloc(packet_size*sizeof(uint8_t));
            for(int i = 0 ; i < packet_size ; i++){
                buffer[i] = rand()%256;
            }
            packet = rtp_session_create_packet(source_session, RTP_FIXED_HEADER_SIZE, buffer, packet_size);
            if(i == pos2) lost_packet = packet;
            rtp_set_seqnumber(packet, num);
            rtp_set_timestamp(packet, rand()%429496729);
            num++;
            if(i != pos1 && i != pos2) putq(&fec_stream->source_packets_recvd, packet);
            repair = fec_stream_on_new_source_packet_sent_test(fec_stream, packet);
            if(repair != NULL){
                putq(&fec_stream->repair_packets_recvd, repair);
            }
            free(buffer);
        }

        new_packet = fec_stream_reconstruct_missing_packet(fec_stream, rtp_get_seqnumber(lost_packet));

        BC_ASSERT_PTR_NULL(new_packet);

        flushq(&fec_stream->source_packets_recvd, 0);
        flushq(&fec_stream->repair_packets_recvd, 0);
    }

    rtp_session_destroy(source_session);
}

static void media_stream_test_2_videostreams(int payload_type){
    video_stream_tester_t* marielle=video_stream_tester_new();
    video_stream_tester_t* margaux=video_stream_tester_new();
    OrtpNetworkSimulatorParams params = { 0 };

    params.enabled = TRUE;
    params.loss_rate = 5.;

    init_video_streams(marielle, margaux, FALSE, FALSE, &params, payload_type, FALSE, TRUE);

    BC_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle->vs->ms, &margaux->vs->ms,
        &marielle->stats.number_of_packet_reconstructed, 1, 6000, event_queue_cb, &marielle->stats, event_queue_cb, &margaux->stats));

    uninit_video_streams(marielle, margaux);

    video_stream_tester_destroy(margaux);
    video_stream_tester_destroy(marielle);
}

static void fec_video_stream_vp8(void) {
    if(ms_factory_codec_supported(_factory, "vp8")) {
        media_stream_test_2_videostreams(VP8_PAYLOAD_TYPE);
    } else {
        ms_error("VP8 codec is not supported!");
    }
}

static void fec_video_stream_h264(void) {
    if(ms_factory_codec_supported(_factory, "h264")) {
        media_stream_test_2_videostreams(H264_PAYLOAD_TYPE);
    } else {
        ms_error("H264 codec is not supported!");
    }

}

static void frame_marker_received(MSFilter *f, uint8_t marker, void* user_data) {
	video_stream_tester_stats_t *stats = (video_stream_tester_stats_t *) user_data;
	if (marker & RTP_FRAME_MARKER_START) {
		stats->number_of_framemarking_start++;
	}
	if (marker & RTP_FRAME_MARKER_END) {
		stats->number_of_framemarking_end++;
	}
}

static void basic_vp8_stream_with_frame_marking(void) {
	video_stream_tester_t* marielle = video_stream_tester_new();
	MSConnectionHelper ch;
	bool_t activate = TRUE;

	ms_factory_register_filter(_factory, &ms_framemarking_tester_desc);

	create_video_stream(marielle, VP8_PAYLOAD_TYPE);
	video_stream_set_frame_marking_extension_id(marielle->vs, RTP_EXTENSION_FRAME_MARKING);

	RtpSession *session = ms_create_duplex_rtp_session("127.0.0.1", -1, -1, ms_factory_get_mtu(_factory));
	int local_rtp = rtp_session_get_local_port(session);
	int local_rtcp = rtp_session_get_local_rtcp_port(session);

	MSFilter *rtp_receive = NULL;
	ms_tester_create_filter(&rtp_receive, MS_RTP_RECV_ID, _factory);
	ms_filter_call_method(rtp_receive, MS_RTP_RECV_SET_SESSION, session);
	ms_filter_call_method(rtp_receive, MS_RTP_RECV_ENABLE_RTP_TRANSFER_MODE, &activate);

	MSFilter *framemarking = NULL;
	ms_tester_create_filter(&framemarking, MS_FRAMEMARKING_TESTER_ID, _factory);
	if (framemarking == NULL) goto end; 
	MSFrameMarkingTesterCbData cb_data;
	cb_data.cb = frame_marker_received;
	cb_data.user_data = &marielle->stats;
	ms_filter_call_method(framemarking, MS_FRAMEMARKING_TESTER_SET_CALLBACK, &cb_data);

	ms_connection_helper_start(&ch);
	ms_connection_helper_link(&ch, rtp_receive, -1, 0);
	ms_connection_helper_link(&ch, framemarking, 0, -1);

	ms_tester_create_ticker();
	ms_ticker_attach(ms_tester_ticker, rtp_receive);

	BC_ASSERT_EQUAL(video_stream_start(marielle->vs, &rtp_profile, "127.0.0.1", local_rtp, "127.0.0.1", local_rtcp, VP8_PAYLOAD_TYPE, 50, marielle->cam), 0, int, "%d");

	BC_ASSERT_TRUE(wait_for_until(&marielle->vs->ms, NULL, &marielle->stats.number_of_framemarking_start, 3, 5000));
	BC_ASSERT_GREATER(marielle->stats.number_of_framemarking_end, 2, int, "%d");

	ms_ticker_detach(ms_tester_ticker, rtp_receive);
	ms_tester_destroy_ticker();

	ms_connection_helper_start(&ch);
	ms_connection_helper_unlink(&ch, rtp_receive, -1, 0);
	ms_connection_helper_unlink(&ch, framemarking, 0, -1);

	ms_filter_destroy(framemarking);
end:
	ms_filter_destroy(rtp_receive);
	rtp_session_destroy(session);

    destroy_video_stream(marielle);
	video_stream_tester_destroy(marielle);
}

static test_t tests[] = {
	TEST_NO_TAG("Basic video stream VP8"                     , basic_video_stream_vp8),
	TEST_NO_TAG("Basic video stream H264"                    , basic_video_stream_all_h264_codec_combinations),
	TEST_NO_TAG("Multicast video stream"                     , multicast_video_stream),
	TEST_NO_TAG("Basic one-way video stream"                 , basic_one_way_video_stream),
	TEST_NO_TAG("Codec change for video stream"              , codec_change_for_video_stream),
	TEST_NO_TAG("AVPF video stream VP8"                      , avpf_video_stream_vp8),
	TEST_NO_TAG("AVPF video stream H264"                     , avpf_video_stream_all_h264_codec_combinations),
	TEST_NO_TAG("AVPF high-loss video stream VP8"            , avpf_high_loss_video_stream_vp8),
	TEST_NO_TAG("AVPF high-loss video stream H264"           , avpf_high_loss_video_stream_all_h264_codec_conbinations),
	TEST_NO_TAG("AVPF very high-loss video stream VP8"       , avpf_very_high_loss_video_stream_vp8),
	TEST_NO_TAG("AVPF video stream first iframe lost VP8"    , avpf_video_stream_first_iframe_lost_vp8),
	TEST_NO_TAG("AVPF video stream first iframe lost H264"   , avpf_video_stream_first_iframe_lost_all_h264_codec_combinations),
	TEST_NO_TAG("AVP video stream first iframe lost VP8"     , video_stream_first_iframe_lost_vp8),
	TEST_NO_TAG("AVP video stream first iframe lost H264"    , video_stream_first_iframe_lost_all_h264_codec_combinations),
	TEST_NO_TAG("Video configuration VP8"                    , video_configuration_stream_vp8),
	TEST_NO_TAG("Video configuration H264"                   , video_configuration_stream_all_h264_codec_combinations),
	TEST_NO_TAG("Video steam camera ELPH264"                 , video_stream_elph264_camera),
	TEST_NO_TAG("AVPF RPSI count"                            , avpf_rpsi_count),
	TEST_NO_TAG("Video stream normal loss with retransmission on NACK" , video_stream_normal_loss_with_retransmission_on_nack),
	TEST_NO_TAG("One-way video stream with itcsink"          , video_stream_with_itcsink),
	TEST_NO_TAG("Reconstruction packet with FEC"             , fec_stream_test_reconstruction),
	TEST_NO_TAG("Lost repair packet"                         , fec_stream_test_lost_repair_packet),
	TEST_NO_TAG("Lost 2 source packets"                      , fec_stream_test_lost_2_source_packets),
	TEST_NO_TAG("FEC video stream VP8"                       , fec_video_stream_vp8),
	TEST_NO_TAG("FEC video stream H264"                      , fec_video_stream_h264),
	TEST_NO_TAG("Basic VP8 stream with frame marking"        , basic_vp8_stream_with_frame_marking)
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
