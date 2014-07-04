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
#include "private.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

#include <stdio.h>
#include "CUnit/Basic.h"


#ifdef _MSC_VER
#define unlink _unlink
#endif

static RtpProfile rtp_profile;

#define VP8_PAYLOAD_TYPE    103

static int tester_init(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();
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
} video_stream_tester_t;


static void reset_stats(video_stream_tester_stats_t *s) {
	memset(s, 0, sizeof(video_stream_tester_stats_t));
}
#ifdef VIDEO_ENABLED
static void video_stream_event_cb(void *user_pointer, const MSFilter *f, const unsigned int event_id, const void *args){
	video_stream_tester_t* vs_tester = (video_stream_tester_t*) user_pointer;
	ms_message("Event [%ui] received on video stream [%p]",event_id,vs_tester);
	switch (event_id) {
		case MS_VIDEO_DECODER_DECODING_ERRORS:
			vs_tester->stats.number_of_decoder_decoding_error++;
			break;
		case MS_VIDEO_DECODER_FIRST_IMAGE_DECODED:
			vs_tester->stats.number_of_decoder_first_image_decoded++;
			break;
		case MS_VIDEO_DECODER_SEND_PLI:
			vs_tester->stats.number_of_decoder_send_pli++;
			break;
		case MS_VIDEO_DECODER_SEND_SLI:
			vs_tester->stats.number_of_decoder_send_sli++;
			break;
		case MS_VIDEO_DECODER_SEND_RPSI:
			vs_tester->stats.number_of_decoder_send_rpsi++;
			/* Handled internally by mediastreamer2. */
			break;
		default:
			ms_warning("Unhandled event %i", event_id);
			break;
	}
}
#endif

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

static void init_video_streams(video_stream_tester_t *marielle, video_stream_tester_t *margaux, bool_t avpf, bool_t one_way, OrtpNetworkSimulatorParams *params) {
	PayloadType *pt;
	MSWebCam *no_webcam = ms_web_cam_manager_get_cam(ms_web_cam_manager_get(), "StaticImage: Static picture");
	MSWebCam *default_webcam = ms_web_cam_manager_get_default_cam(ms_web_cam_manager_get());
#if TARGET_OS_MAC
	default_webcam=no_webcam; /*cannot acces real camera without mainloop*/
#endif

	marielle->vs = video_stream_new(MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, FALSE);
	margaux->vs = video_stream_new(MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT, FALSE);
	reset_stats(&marielle->stats);
	reset_stats(&margaux->stats);

	/* Enable/disable avpf. */
	pt = rtp_profile_get_payload(&rtp_profile, VP8_PAYLOAD_TYPE);
	CU_ASSERT_PTR_NOT_NULL_FATAL(pt);
	if (avpf == TRUE) {
		payload_type_set_flag(pt, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
	} else {
		payload_type_unset_flag(pt, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
	}

	/* Configure network simulator. */
	if ((params != NULL) && (params->enabled == TRUE)) {
		rtp_session_enable_network_simulation(marielle->vs->ms.sessions.rtp_session, params);
		rtp_session_enable_network_simulation(margaux->vs->ms.sessions.rtp_session, params);
	}

	marielle->stats.q = ortp_ev_queue_new();
	rtp_session_register_event_queue(marielle->vs->ms.sessions.rtp_session, marielle->stats.q);
	video_stream_set_event_callback(marielle->vs,video_stream_event_cb, marielle);

	margaux->stats.q = ortp_ev_queue_new();
	rtp_session_register_event_queue(margaux->vs->ms.sessions.rtp_session, margaux->stats.q);
	video_stream_set_event_callback(margaux->vs,video_stream_event_cb, margaux);



	if (one_way == TRUE) {
		video_stream_set_direction(marielle->vs, VideoStreamRecvOnly);
	}

	CU_ASSERT_EQUAL(
		video_stream_start(marielle->vs, &rtp_profile, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_IP, MARGAUX_RTCP_PORT, VP8_PAYLOAD_TYPE, 50, default_webcam),
		0);
	CU_ASSERT_EQUAL(
		video_stream_start(margaux->vs, &rtp_profile, MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_IP, MARIELLE_RTCP_PORT, VP8_PAYLOAD_TYPE, 50, no_webcam),
		0);
}

static void uninit_video_streams(video_stream_tester_t *marielle, video_stream_tester_t *margaux) {
	float rtcp_send_bandwidth;
	PayloadType *pt;

	pt = rtp_profile_get_payload(&rtp_profile, VP8_PAYLOAD_TYPE);
	CU_ASSERT_PTR_NOT_NULL_FATAL(pt);

	rtp_session_compute_send_bandwidth(marielle->vs->ms.sessions.rtp_session);
	rtp_session_compute_send_bandwidth(margaux->vs->ms.sessions.rtp_session);
	rtcp_send_bandwidth = rtp_session_get_rtcp_send_bandwidth(marielle->vs->ms.sessions.rtp_session);
	CU_ASSERT_TRUE(rtcp_send_bandwidth <= (0.05 * payload_type_get_bitrate(pt)));
	rtcp_send_bandwidth = rtp_session_get_rtcp_send_bandwidth(margaux->vs->ms.sessions.rtp_session);
	CU_ASSERT_TRUE(rtcp_send_bandwidth <= (0.05 * payload_type_get_bitrate(pt)));

	video_stream_stop(marielle->vs);
	video_stream_stop(margaux->vs);

	ortp_ev_queue_destroy(marielle->stats.q);
	ortp_ev_queue_destroy(margaux->stats.q);
}

static void basic_video_stream(void) {
	video_stream_tester_t marielle;
	video_stream_tester_t margaux;
	bool_t supported = ms_filter_codec_supported("vp8");

	if (supported) {
		init_video_streams(&marielle, &margaux, FALSE, FALSE, NULL);

		CU_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle.vs->ms, &margaux.vs->ms, &marielle.stats.number_of_SR, 2, 15000, event_queue_cb, &marielle.stats, event_queue_cb, &margaux.stats));

		video_stream_get_local_rtp_stats(marielle.vs, &marielle.stats.rtp);
		video_stream_get_local_rtp_stats(margaux.vs, &margaux.stats.rtp);

		uninit_video_streams(&marielle, &margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
}

static void basic_one_way_video_stream(void) {
	video_stream_tester_t marielle;
	video_stream_tester_t margaux;
	bool_t supported = ms_filter_codec_supported("vp8");

	if (supported) {
		init_video_streams(&marielle, &margaux, FALSE, TRUE, NULL);

		CU_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle.vs->ms, &margaux.vs->ms, &marielle.stats.number_of_RR, 2, 15000, event_queue_cb, &marielle.stats, event_queue_cb, &margaux.stats));

		video_stream_get_local_rtp_stats(marielle.vs, &marielle.stats.rtp);
		video_stream_get_local_rtp_stats(margaux.vs, &margaux.stats.rtp);

		uninit_video_streams(&marielle, &margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
}

static void avpf_video_stream(void) {
	video_stream_tester_t marielle;
	video_stream_tester_t margaux;
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = ms_filter_codec_supported("vp8");

	if (supported) {
		params.enabled = TRUE;
		params.loss_rate = 5.;
		init_video_streams(&marielle, &margaux, TRUE, FALSE, &params);

		CU_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle.vs->ms, &margaux.vs->ms, &marielle.stats.number_of_SR, 2, 15000, event_queue_cb, &marielle.stats, event_queue_cb, &margaux.stats));
		CU_ASSERT_TRUE(marielle.stats.number_of_PLI >= 0);
		CU_ASSERT_TRUE(marielle.stats.number_of_SLI > 0);
		CU_ASSERT_TRUE(marielle.stats.number_of_RPSI > 0);

		uninit_video_streams(&marielle, &margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
}

static void video_stream_first_iframe_lost_vp8_base(bool_t use_avp) {
	video_stream_tester_t marielle;
	video_stream_tester_t margaux;
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = ms_filter_codec_supported("vp8");

	if (supported) {
		int dummy=0;
		params.enabled = TRUE;
		params.loss_rate = 100.;
		init_video_streams(&marielle, &margaux, use_avp, FALSE, &params);
		wait_for_until(&marielle.vs->ms, &margaux.vs->ms,&dummy,1,500);
		params.enabled=FALSE;
		/*disable packet losses*/
		rtp_session_enable_network_simulation(marielle.vs->ms.sessions.rtp_session, &params);
		rtp_session_enable_network_simulation(margaux.vs->ms.sessions.rtp_session, &params);

		if (use_avp) {
			CU_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle.vs->ms, &margaux.vs->ms, &marielle.stats.number_of_PLI, 1, 1000, event_queue_cb, &marielle.stats, event_queue_cb, &margaux.stats));
			CU_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle.vs->ms, &margaux.vs->ms, &margaux.stats.number_of_PLI, 1, 1000, event_queue_cb, &marielle.stats, event_queue_cb, &margaux.stats));
			CU_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle.vs->ms, &margaux.vs->ms, &marielle.stats.number_of_decoder_first_image_decoded, 1, 1000, event_queue_cb, &marielle.stats, event_queue_cb, &margaux.stats));
			CU_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle.vs->ms, &margaux.vs->ms, &margaux.stats.number_of_decoder_first_image_decoded, 1, 1000, event_queue_cb, &marielle.stats, event_queue_cb, &margaux.stats));
		} else {
			CU_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle.vs->ms, &margaux.vs->ms, &marielle.stats.number_of_decoder_decoding_error, 1, 1000, event_queue_cb, &marielle.stats, event_queue_cb, &margaux.stats));
			CU_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle.vs->ms, &margaux.vs->ms, &margaux.stats.number_of_decoder_decoding_error, 1, 1000, event_queue_cb, &marielle.stats, event_queue_cb, &margaux.stats));
		}

		uninit_video_streams(&marielle, &margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
}
static void video_stream_first_iframe_lost_vp8() {
	video_stream_first_iframe_lost_vp8_base(FALSE);
}
static void avpf_video_stream_first_iframe_lost_vp8() {
	video_stream_first_iframe_lost_vp8_base(TRUE);
}
static void avpf_high_loss_video_stream(void) {
	video_stream_tester_t marielle;
	video_stream_tester_t margaux;
	OrtpNetworkSimulatorParams params = { 0 };
	bool_t supported = ms_filter_codec_supported("vp8");

	if (supported) {
		params.enabled = TRUE;
		params.loss_rate = 25.;
		init_video_streams(&marielle, &margaux, TRUE, FALSE, &params);

		CU_ASSERT_TRUE(wait_for_until_with_parse_events(&marielle.vs->ms, &margaux.vs->ms, &marielle.stats.number_of_SR, 2, 15000, event_queue_cb, &marielle.stats, event_queue_cb, &margaux.stats));
		CU_ASSERT_TRUE(marielle.stats.number_of_PLI >= 0);
		CU_ASSERT_TRUE(marielle.stats.number_of_SLI > 0);
		CU_ASSERT_TRUE(marielle.stats.number_of_RPSI >= 0);

		uninit_video_streams(&marielle, &margaux);
	} else {
		ms_error("VP8 codec is not supported!");
	}
}


static test_t tests[] = {
	{ "Basic video stream", basic_video_stream },
	{ "Basic one-way video stream", basic_one_way_video_stream },
	{ "AVPF video stream", avpf_video_stream },
	{ "AVPF high-loss video stream", avpf_high_loss_video_stream },
	{ "AVPF PLI on first iframe lost",avpf_video_stream_first_iframe_lost_vp8},
	{ "AVP PLI on first iframe lost",video_stream_first_iframe_lost_vp8}
};

test_suite_t video_stream_test_suite = {
	"VideoStream",
	tester_init,
	tester_cleanup,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
