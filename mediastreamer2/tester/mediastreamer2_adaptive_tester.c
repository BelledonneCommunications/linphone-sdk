/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <bctoolbox/defs.h>

#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "qosanalyzer.h"
#include <math.h>
#include <sys/stat.h>

static RtpProfile rtp_profile;

#define EDGE_BW 10
#define THIRDGENERATION_BW 200

static MSFactory *_factory = NULL;
static int tester_before_all(void) {

	_factory = ms_tester_factory_new();

	// ms_filter_enable_statistics(TRUE);
	ms_factory_enable_statistics(_factory, TRUE);
	ortp_init();
	rtp_profile_set_payload(&rtp_profile, 0, &payload_type_pcmu8000);
	rtp_profile_set_payload(&rtp_profile, OPUS_PAYLOAD_TYPE, &payload_type_opus);
	rtp_profile_set_payload(&rtp_profile, SPEEX_PAYLOAD_TYPE, &payload_type_speex_wb);
	rtp_profile_set_payload(&rtp_profile, SILK16_PAYLOAD_TYPE, &payload_type_silk_wb);
	rtp_profile_set_payload(&rtp_profile, PCMA8_PAYLOAD_TYPE, &payload_type_pcma8000);

	rtp_profile_set_payload(&rtp_profile, H263_PAYLOAD_TYPE, &payload_type_h263);
	rtp_profile_set_payload(&rtp_profile, H264_PAYLOAD_TYPE, &payload_type_h264);
	rtp_profile_set_payload(&rtp_profile, VP8_PAYLOAD_TYPE, &payload_type_vp8);
	return 0;
}

static int tester_after_all(void) {
	ortp_exit();

	ms_factory_destroy(_factory);
	rtp_profile_clear_all(&rtp_profile);
	return 0;
}

#define HELLO_16K_1S_FILE "sounds/hello16000-1s.wav"
#define RECORDED_16K_1S_FILE "recorded_hello16000-1s-"
#define RECORDED_16K_1S_NO_PLC_FILE "withoutplc_recorded_hello16000-1sbv16-"

#define MARIELLE_RTP_PORT (base_port + 25)
#define MARGAUX_RTP_PORT (base_port + 27)

typedef struct _stream_manager_t {
	MSFormatType type;
	int local_rtp;
	int local_rtcp;

	union {
		AudioStream *audio_stream;
		VideoStream *video_stream;
	};

	int rtcp_count;

	struct {
		float loss_estim;
		float congestion_bw_estim;
	} adaptive_stats;

	MSBandwidthController *bw_controller;
	void *user_data;

} stream_manager_t;

stream_manager_t *stream_manager_new(MSFormatType type, int base_port) {
	stream_manager_t *mgr = ms_new0(stream_manager_t, 1);
	mgr->type = type;
	mgr->local_rtp = base_port;
	mgr->local_rtcp = base_port + 1;
	mgr->user_data = NULL;

	if (mgr->type == MSAudio) {
		mgr->audio_stream = audio_stream_new(_factory, mgr->local_rtp, mgr->local_rtcp, FALSE);
	} else {
#if VIDEO_ENABLED
		mgr->video_stream = video_stream_new(_factory, mgr->local_rtp, mgr->local_rtcp, FALSE);
		mgr->bw_controller = ms_bandwidth_controller_new();
#else
		ms_fatal("Unsupported stream type [%s]", ms_format_type_to_string(mgr->type));
#endif
	}
	return mgr;
}

static void stream_manager_delete(stream_manager_t *mgr, bool_t destroy_files) {

	if (mgr->type == MSAudio) {
		audio_stream_stop(mgr->audio_stream);
	} else {
#if VIDEO_ENABLED
		ms_bandwidth_controller_destroy(mgr->bw_controller);
		video_stream_stop(mgr->video_stream);
#else
		ms_fatal("Unsupported stream type [%s]", ms_format_type_to_string(mgr->type));
#endif
	}
	if (mgr->user_data && destroy_files == TRUE) {
		ms_message("Destroying file %s", (char *)mgr->user_data);
		unlink((char *)mgr->user_data);
		ms_free(mgr->user_data);
	}
	ms_free(mgr);
}

static void audio_manager_start(stream_manager_t *mgr,
                                int payload_type,
                                int remote_port,
                                int target_bitrate,
                                const char *player_file,
                                const char *recorder_file) {

	media_stream_set_target_network_bitrate(&mgr->audio_stream->ms, target_bitrate);

	BC_ASSERT_EQUAL(audio_stream_start_full(mgr->audio_stream, &rtp_profile, "127.0.0.1", remote_port, "127.0.0.1",
	                                        remote_port + 1, payload_type, 50, player_file, recorder_file, NULL, NULL,
	                                        0),
	                0, int, "%d");
}

#if VIDEO_ENABLED
static void
video_manager_start(stream_manager_t *mgr, int payload_type, int remote_port, int target_bitrate, MSWebCam *cam) {
	int result;
	media_stream_set_target_network_bitrate(&mgr->video_stream->ms, target_bitrate);

	result = video_stream_start(mgr->video_stream, &rtp_profile, "127.0.0.1", remote_port, "127.0.0.1", remote_port + 1,
	                            payload_type, 60, cam);
	BC_ASSERT_EQUAL(result, 0, int, "%d");
}
#endif

static void
qos_analyzer_on_action_suggested(void *user_data, BCTBX_UNUSED(int datac), BCTBX_UNUSED(const char **datav)) {
	stream_manager_t *mgr = (stream_manager_t *)user_data;
	mgr->rtcp_count++;

	if (mgr->type == MSVideo && mgr->video_stream->ms.rc_enable) {
		const MSQosAnalyzer *analyzer = ms_bitrate_controller_get_qos_analyzer(mgr->video_stream->ms.rc);
		if (analyzer->type == MSQosAnalyzerAlgorithmStateful) {
			const MSStatefulQosAnalyzer *stateful_analyzer = ((const MSStatefulQosAnalyzer *)analyzer);
			mgr->adaptive_stats.loss_estim = (float)stateful_analyzer->network_loss_rate;
			mgr->adaptive_stats.congestion_bw_estim = (float)stateful_analyzer->congestion_bandwidth;
		}
	}
}

static void disable_plc_on_audio_stream(AudioStream *p1) {

	if (p1 != NULL) {
		uint32_t features_p1 = audio_stream_get_features(p1);
		features_p1 &= ~AUDIO_STREAM_FEATURE_PLC;
		audio_stream_set_features(p1, features_p1);
	}
}

void start_adaptive_stream(MSFormatType type,
                           stream_manager_t **pmarielle,
                           stream_manager_t **pmargaux,
                           int payload,
                           int initial_bitrate,
                           int max_bw,
                           float loss_rate,
                           int latency,
                           float dup_ratio,
                           bool_t disable_plc) {
	int pause_time = 0;
	MediaStream *marielle_ms;
	OrtpNetworkSimulatorParams params = {0};
	char *recorded_file = NULL;
	char *file = bc_tester_res(HELLO_16K_1S_FILE);
#if VIDEO_ENABLED
	MSWebCam *marielle_webcam = mediastreamer2_tester_get_mire(_factory);
#endif

	stream_manager_t *marielle = *pmarielle = stream_manager_new(type, MARIELLE_RTP_PORT);
	stream_manager_t *margaux = *pmargaux = stream_manager_new(type, MARGAUX_RTP_PORT);

	if (disable_plc) {
		disable_plc_on_audio_stream(margaux->audio_stream);
		disable_plc_on_audio_stream(marielle->audio_stream);
	}
	if (!disable_plc) {
		char *random_filename = ms_tester_get_random_filename(RECORDED_16K_1S_FILE, ".wav");
		recorded_file = bc_tester_file(random_filename);
		bctbx_free(random_filename);
	} else {
		char *random_filename = ms_tester_get_random_filename(RECORDED_16K_1S_NO_PLC_FILE, ".wav");
		recorded_file = bc_tester_file(random_filename);
		bctbx_free(random_filename);
	}

	margaux->user_data = ms_strdup(recorded_file);
	params.enabled = TRUE;
	params.loss_rate = loss_rate;
	params.max_bandwidth = (float)max_bw;
	params.max_buffer_size = 1000000;
	params.latency = latency;
	params.mode = OrtpNetworkSimulatorOutbound;

	if (type == MSAudio) {
		marielle_ms = &marielle->audio_stream->ms;
	} else {
		marielle_ms = &marielle->video_stream->ms;
	}

	media_stream_enable_adaptive_bitrate_control(marielle_ms, TRUE);
	media_stream_set_adaptive_bitrate_algorithm(marielle_ms, MSQosAnalyzerAlgorithmStateful);
	rtp_session_set_duplication_ratio(marielle_ms->sessions.rtp_session, dup_ratio);

	if (marielle->type == MSAudio) {
		audio_manager_start(marielle, payload, margaux->local_rtp, initial_bitrate, file, NULL);
		ms_filter_call_method(marielle->audio_stream->soundread, MS_FILE_PLAYER_LOOP, &pause_time);

		audio_manager_start(margaux, payload, marielle->local_rtp, 0, NULL, recorded_file);
	} else {
#if VIDEO_ENABLED
		struct _MediaStream *margaux_video_stream = &(margaux->video_stream->ms);
		OrtpVideoBandwidthEstimatorParams params = {0};
		marielle->video_stream->staticimage_webcam_fps_optimization = FALSE;
		video_manager_start(marielle, payload, margaux->local_rtp, max_bw * 1 / 2, marielle_webcam);
		media_stream_set_direction(&margaux->video_stream->ms, MediaStreamRecvOnly);
		ms_bandwidth_controller_add_stream(margaux->bw_controller, margaux_video_stream);
		params.packet_count_min = 5;
		params.min_required_measurements = 30;
		params.enabled = TRUE;
		rtp_session_enable_video_bandwidth_estimator(margaux_video_stream->sessions.rtp_session, &params);
		video_manager_start(margaux, payload, marielle->local_rtp, 0, NULL);
#else
		ms_fatal("Unsupported stream type [%s]", ms_format_type_to_string(marielle->type));
#endif
	}

	ms_qos_analyzer_set_on_action_suggested(ms_bitrate_controller_get_qos_analyzer(marielle_ms->rc),
	                                        qos_analyzer_on_action_suggested, *pmarielle);
	rtp_session_enable_network_simulation(marielle_ms->sessions.rtp_session, &params);

	free(recorded_file);
	free(file);
}

static void iterate_adaptive_stream(
    stream_manager_t *marielle, stream_manager_t *margaux, int timeout_ms, int *current, int expected) {
	int retry = 0;

	MediaStream *marielle_ms, *margaux_ms;
	if (marielle->type == MSAudio) {
		marielle_ms = &marielle->audio_stream->ms;
		margaux_ms = &margaux->audio_stream->ms;
	} else {
		marielle_ms = &marielle->video_stream->ms;
		margaux_ms = &margaux->video_stream->ms;
	}

	while ((!current || *current < expected) && retry++ / 5 < timeout_ms / 100) {
		media_stream_iterate(marielle_ms);
		media_stream_iterate(margaux_ms);
		// handle_queue_events(marielle);
		if (retry % 50 == 0) {
			ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec", marielle_ms,
			           media_stream_get_down_bw(marielle_ms) / 1000, media_stream_get_up_bw(marielle_ms) / 1000);
			ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec", margaux_ms,
			           media_stream_get_down_bw(margaux_ms) / 1000, media_stream_get_up_bw(margaux_ms) / 1000);
		}
		ms_usleep(20000);
	}
}
static void iterate_adaptive_stream_float(
    stream_manager_t *marielle, stream_manager_t *margaux, int timeout_ms, float *current, float expected) {
	int retry = 0;

	MediaStream *marielle_ms, *margaux_ms;
	if (marielle->type == MSAudio) {
		marielle_ms = &marielle->audio_stream->ms;
		margaux_ms = &margaux->audio_stream->ms;
	} else {
		marielle_ms = &marielle->video_stream->ms;
		margaux_ms = &margaux->video_stream->ms;
	}

	while ((!current || *current < expected) && retry++ / 5 < timeout_ms / 100) {
		media_stream_iterate(marielle_ms);
		media_stream_iterate(margaux_ms);
		// handle_queue_events(marielle);
		if (retry % 50 == 0) {
			ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec", marielle_ms,
			           media_stream_get_down_bw(marielle_ms) / 1000, media_stream_get_up_bw(marielle_ms) / 1000);
			ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec", margaux_ms,
			           media_stream_get_down_bw(margaux_ms) / 1000, media_stream_get_up_bw(margaux_ms) / 1000);
		}
		ms_usleep(20000);
	}
}
static void iterate_adaptive_stream_bool(
    stream_manager_t *marielle, stream_manager_t *margaux, int timeout_ms, bool_t *current, bool_t expected) {
	int retry = 0;

	MediaStream *marielle_ms, *margaux_ms;
	if (marielle->type == MSAudio) {
		marielle_ms = &marielle->audio_stream->ms;
		margaux_ms = &margaux->audio_stream->ms;
	} else {
		marielle_ms = &marielle->video_stream->ms;
		margaux_ms = &margaux->video_stream->ms;
	}

	while ((!current || *current != expected) && retry++ / 5 < timeout_ms / 100) {
		media_stream_iterate(marielle_ms);
		media_stream_iterate(margaux_ms);
		// handle_queue_events(marielle);
		if (retry % 50 == 0) {
			ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec", marielle_ms,
			           media_stream_get_down_bw(marielle_ms) / 1000, media_stream_get_up_bw(marielle_ms) / 1000);
			ms_message("stream [%p] bandwidth usage: [d=%.1f,u=%.1f] kbit/sec", margaux_ms,
			           media_stream_get_down_bw(margaux_ms) / 1000, media_stream_get_up_bw(margaux_ms) / 1000);
		}
		ms_usleep(20000);
	}
}

static void stop_adaptive_stream(stream_manager_t *marielle, stream_manager_t *margaux, bool_t destroy_files) {
#if VIDEO_ENABLED
	if (margaux->bw_controller) {
		ms_bandwidth_controller_remove_stream(margaux->bw_controller, &(margaux->video_stream->ms));
	}
#endif
	stream_manager_delete(marielle, destroy_files);
	stream_manager_delete(margaux, destroy_files);
}

typedef struct {
	OrtpLossRateEstimator *estimator;
	OrtpEvQueue *q;
	int loss_rate;
} LossRateEstimatorCtx;

static void event_queue_cb(MediaStream *ms, void *user_pointer) {
	LossRateEstimatorCtx *ctx = (LossRateEstimatorCtx *)user_pointer;
	if (ctx->q != NULL) {
		OrtpEvent *ev = NULL;
		while ((ev = ortp_ev_queue_get(ctx->q)) != NULL) {
			OrtpEventType evt = ortp_event_get_type(ev);
			OrtpEventData *evd = ortp_event_get_data(ev);
			if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
				RtcpParserContext parserctx;
				const mblk_t *rtcp_packet = rtcp_parser_context_init(&parserctx, evd->packet);
				do {
					const report_block_t *rb = NULL;
					if (rtcp_is_SR(rtcp_packet)) {
						rb = rtcp_SR_get_report_block(rtcp_packet, 0);
					} else if (rtcp_is_RR(rtcp_packet)) {
						rb = rtcp_RR_get_report_block(rtcp_packet, 0);
					}

					if (rb &&
					    ortp_loss_rate_estimator_process_report_block(ctx->estimator, ms->sessions.rtp_session, rb)) {
						float diff = (float)fabs(ortp_loss_rate_estimator_get_value(ctx->estimator) - ctx->loss_rate);
						BC_ASSERT_GREATER(diff, 0, float, "%f");
						BC_ASSERT_LOWER(diff, 10, float, "%f");
					}
				} while ((rtcp_packet = rtcp_parser_context_next_packet(&parserctx)) != NULL);
				rtcp_parser_context_uninit(&parserctx);
			}
			ortp_event_destroy(ev);
		}
	}
}

/********************************** Tests are starting now ********************/
static void packet_duplication(void) {
	const rtp_stats_t *stats;
	float dup_ratio;
	stream_manager_t *marielle, *margaux;

	dup_ratio = 0.0f;
	start_adaptive_stream(MSAudio, &marielle, &margaux, SPEEX_PAYLOAD_TYPE, 32000, 0, 0.0f, 50, dup_ratio, FALSE);
	media_stream_enable_adaptive_bitrate_control(&marielle->audio_stream->ms, FALSE);
	iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
	stats = rtp_session_get_stats(margaux->video_stream->ms.sessions.rtp_session);
	BC_ASSERT_EQUAL(stats->packet_dup_recv, dup_ratio ? (uint64_t)(stats->packet_recv / (dup_ratio + 1)) : 0,
	                unsigned long long, "%llu");
	/*in theory, cumulative loss should be the invert of duplicated count, but
	since cumulative loss is computed only on received RTCP report and duplicated
	count is updated on each RTP packet received, we cannot accurately compare these values*/
	BC_ASSERT_LOWER(stats->cum_packet_loss, (int64_t)(-.5 * stats->packet_dup_recv), long long, "%lld");
	stop_adaptive_stream(marielle, margaux, TRUE);

	dup_ratio = 1.0f;
	start_adaptive_stream(MSAudio, &marielle, &margaux, SPEEX_PAYLOAD_TYPE, 32000, 0, 0.0f, 50, dup_ratio, FALSE);
	media_stream_enable_adaptive_bitrate_control(&marielle->audio_stream->ms, FALSE);
	iterate_adaptive_stream(marielle, margaux, 10000, NULL, 0);
	stats = rtp_session_get_stats(margaux->video_stream->ms.sessions.rtp_session);
	BC_ASSERT_EQUAL(stats->packet_dup_recv, dup_ratio ? (uint64_t)(stats->packet_recv / (dup_ratio + 1)) : 0,
	                unsigned long long, "%llu");
	BC_ASSERT_LOWER(stats->cum_packet_loss, (int64_t)(-.5 * stats->packet_dup_recv), long long, "%lld");
	stop_adaptive_stream(marielle, margaux, TRUE);
}

static void upload_bandwidth_computation(void) {

	// bool_t supported = ms_filter_codec_supported("pcma");
	bool_t supported = ms_factory_codec_supported(_factory, "pcma");

	if (supported) {
		stream_manager_t *marielle, *margaux;
		int i;

		start_adaptive_stream(MSAudio, &marielle, &margaux, PCMA8_PAYLOAD_TYPE, 8000, 0, 0, 0, 0, 0);
		media_stream_enable_adaptive_bitrate_control(&marielle->audio_stream->ms, FALSE);

		for (i = 0; i < 5; i++) {
			rtp_session_set_duplication_ratio(marielle->audio_stream->ms.sessions.rtp_session, (float)i);
			iterate_adaptive_stream(marielle, margaux, 5000, NULL, 0);
			/*since PCMA uses 80kbit/s, upload bandwidth should just be 80+80*duplication_ratio kbit/s */
			BC_ASSERT_LOWER(
			    (float)fabs(rtp_session_get_send_bandwidth(marielle->audio_stream->ms.sessions.rtp_session) / 1000. -
			                80. * (i + 1)),
			    5.f, float, "%f");
		}
		stop_adaptive_stream(marielle, margaux, TRUE);
	}
}

off_t fsize(const char *filename) {
	struct stat st;

	if (stat(filename, &st) == 0) return st.st_size;

	return -1;
}

static void loss_rate_estimation_bv16(void) {
	bool_t supported = ms_factory_codec_supported(_factory, "bv16");
	int plc_disabled = 0;
	int size_with_plc = 0, size_no_plc = 0;
	int result;

	rtp_profile_set_payload(&rtp_profile, BV16_PAYLOAD_TYPE, &payload_type_bv16);
	if (supported) {
		LossRateEstimatorCtx ctx;
		stream_manager_t *marielle, *margaux;
		int loss_rate = 15;
		char *plc_filename = NULL;
		char *no_plc_filename = NULL;
		for (plc_disabled = 0; plc_disabled <= 1; plc_disabled++) {
			start_adaptive_stream(MSAudio, &marielle, &margaux, BV16_PAYLOAD_TYPE, 8000, 0, (float)loss_rate, 0, 0.0f,
			                      (bool_t)plc_disabled);

			ctx.estimator = ortp_loss_rate_estimator_new(120, 2500, marielle->audio_stream->ms.sessions.rtp_session);
			ctx.q = ortp_ev_queue_new();
			rtp_session_register_event_queue(marielle->audio_stream->ms.sessions.rtp_session, ctx.q);
			ctx.loss_rate = loss_rate;

			/*loss rate should be the initial one*/
			wait_for_until_with_parse_events(&marielle->audio_stream->ms, &margaux->audio_stream->ms, &loss_rate, 100,
			                                 10000, event_queue_cb, &ctx, NULL, NULL);

			if (plc_disabled == 0) {
				plc_filename = margaux->user_data;
			} else {
				no_plc_filename = margaux->user_data;
			}
			stop_adaptive_stream(marielle, margaux, FALSE);
			ortp_loss_rate_estimator_destroy(ctx.estimator);
			ortp_ev_queue_destroy(ctx.q);
		}
		/*file without plc must be approx 15% smaller in size than with plc */
		size_with_plc = fsize(plc_filename);
		size_no_plc = fsize(no_plc_filename);
		BC_ASSERT_TRUE(size_with_plc != -1);
		BC_ASSERT_TRUE(size_no_plc != -1);
		result = (size_with_plc * (loss_rate + 5) / 100 + size_no_plc);
		BC_ASSERT_GREATER(result, size_with_plc, int, "%d");
		// if test worked, remove files
		if (result >= size_with_plc) {
			unlink(plc_filename);
			unlink(no_plc_filename);
		}
		bctbx_free(plc_filename);
		bctbx_free(no_plc_filename);
	}
}

static void loss_rate_estimation(void) {

	bool_t supported = ms_factory_codec_supported(_factory, "pcma");
	if (supported) {
		LossRateEstimatorCtx ctx;
		stream_manager_t *marielle, *margaux;
		int loss_rate = 15;
		start_adaptive_stream(MSAudio, &marielle, &margaux, PCMA8_PAYLOAD_TYPE, 8000, 0, (float)loss_rate, 0, 0.0f,
		                      FALSE);

		ctx.estimator = ortp_loss_rate_estimator_new(120, 2500, marielle->audio_stream->ms.sessions.rtp_session);
		ctx.q = ortp_ev_queue_new();
		rtp_session_register_event_queue(marielle->audio_stream->ms.sessions.rtp_session, ctx.q);
		ctx.loss_rate = loss_rate;

		/*loss rate should be the initial one*/
		wait_for_until_with_parse_events(&marielle->audio_stream->ms, &margaux->audio_stream->ms, &loss_rate, 100,
		                                 10000, event_queue_cb, &ctx, NULL, NULL);

#if 0
/* Loss rate is not correctly estimated when using duplicates (see comment in ortp/src/rtcp.c ortp_loss_rate_estimator_process_report_block function */
		/*let's set some duplication. loss rate should NOT be changed */
		rtp_session_set_duplication_ratio(marielle->audio_stream->ms.sessions.rtp_session, 10);
		wait_for_until_with_parse_events(&marielle->audio_stream->ms, &margaux->audio_stream->ms, &loss_rate, 100,
		                                 10000, event_queue_cb, &ctx, NULL, NULL);
#endif

		stop_adaptive_stream(marielle, margaux, TRUE);
		ortp_loss_rate_estimator_destroy(ctx.estimator);
		ortp_ev_queue_destroy(ctx.q);
	}
}

void upload_bitrate(const char *codec, int payload, int target_bw, int expect_bw) {
	// bool_t supported = ms_filter_codec_supported("pcma");
	bool_t supported = ms_factory_codec_supported(_factory, codec);
	if (supported) {
		float upload_bw;
		stream_manager_t *marielle, *margaux;

		start_adaptive_stream(MSAudio, &marielle, &margaux, payload, target_bw * 1000, target_bw * 1000, 0, 50, 0, 0);
		// these tests check that encoders stick to the guidelines, so we must use NOT
		// the adaptive algorithm which would modify these guidelines
		media_stream_enable_adaptive_bitrate_control(&marielle->audio_stream->ms, FALSE);
		iterate_adaptive_stream(marielle, margaux, 15000, NULL, 0);
		upload_bw = media_stream_get_up_bw(&marielle->audio_stream->ms) / 1000;
		BC_ASSERT_GREATER(upload_bw, (float)(expect_bw - 2), float, "%f");
		BC_ASSERT_LOWER(upload_bw, (float)(expect_bw + 2), float, "%f");
		stop_adaptive_stream(marielle, margaux, TRUE);
	}
}

static void upload_bitrate_pcma_3g(void) {
	// pcma codec bitrate is always 64 kbits, only ptime can change from 20ms to 100ms.
	// ptime=20  ms -> network bitrate=80 kbits/s
	// ptime=100 ms -> network bitrate=67 kbits/s
	upload_bitrate("pcma", PCMA8_PAYLOAD_TYPE, THIRDGENERATION_BW, 80);
}

static void upload_bitrate_speex_low(void) {
	// speex codec bitrate can vary from 16 kbits/s to 42 kbits/s
	// bitrate=42 kbits/s ptime=20  ms -> network bitrate=58 kbits/s
	// bitrate=16 kbits/s ptime=100 ms -> network bitrate=19 kbits/s
	upload_bitrate("speex", SPEEX_PAYLOAD_TYPE, 25, 25);
}

static void upload_bitrate_speex_3g(void) {
	upload_bitrate("speex", SPEEX_PAYLOAD_TYPE, THIRDGENERATION_BW, 59);
}

static void upload_bitrate_opus_edge(void) {
	// opus codec bitrate can vary from 6 kbits/s to 184 kbits/s
	// bitrate=6   kbits/s and ptime=100  ms -> network bitrate=  9 kbits/s
	// bitrate=184 kbits/s and ptime=20   ms -> network bitrate=200 kbits/s
	// upload_bitrate("opus", OPUS_PAYLOAD_TYPE, EDGE_BW, 9);
	ms_warning("%s TODO: fix me. ptime in preprocess should be computed to stick the guidelines", __FUNCTION__);
	// until ptime is correctly set on startup, this will not work: currently ptime is set to 40ms but this
	//  is not sufficient to match the guidelines without adaptive algorithm.
}

static void upload_bitrate_opus_3g(void) {
	upload_bitrate("opus", OPUS_PAYLOAD_TYPE, THIRDGENERATION_BW, 200);
}

#if VIDEO_ENABLED && 0
void adaptive_video(int max_bw, int exp_min_bw, int exp_max_bw, int loss_rate, int exp_min_loss, int exp_max_loss) {
	bool_t supported = ms_filter_codec_supported("VP8");
	if (supported) {
		stream_manager_t *marielle, *margaux;
		start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 300 * 1000, max_bw * 1000, loss_rate, 50,
		                      0, 0);
		iterate_adaptive_stream(marielle, margaux, 100000, &marielle->rtcp_count, 7);
		BC_ASSERT_GREATER(marielle->adaptive_stats.loss_estim, exp_min_loss, int, "%d");
		BC_ASSERT_LOWER(marielle->adaptive_stats.loss_estim, exp_max_loss, int, "%d");
		BC_ASSERT_GREATER(marielle->adaptive_stats.congestion_bw_estim, exp_min_bw, int, "%d");
		BC_ASSERT_LOWER(marielle->adaptive_stats.congestion_bw_estim, exp_max_bw, int, "%d");
		stop_adaptive_stream(marielle, margaux, TRUE);
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

void video_bandwidth_estimation(float exp_bw_min, float exp_bw_max) {
	stream_manager_t *marielle, *margaux;
	start_adaptive_stream(MSVideo, &marielle, &margaux, VP8_PAYLOAD_TYPE, 256000, 1000000, 0, 50, 0, FALSE);
	iterate_adaptive_stream_float(marielle, margaux, 30000,
	                              &margaux->bw_controller->download_video_bandwidth_available_estimated, exp_bw_min);
	BC_ASSERT_GREATER(margaux->bw_controller->download_video_bandwidth_available_estimated, exp_bw_min, float, "%f");
	BC_ASSERT_LOWER(margaux->bw_controller->download_video_bandwidth_available_estimated, exp_bw_max, float, "%f");
	stop_adaptive_stream(marielle, margaux, TRUE);
}

static void video_bandwidth_estimator(void) {
	video_bandwidth_estimation(750000, 1300000); // kbits/s
}

/** Scenario:
 *  - Marielle is sending audio stream to Margaux, they both have
 *        - audio_bandwith_estimator enabled (required on both side to activate duplicates sending)
 *        - TMMBR enabled
 *  - Margaux (receiver side) has bandwidth controller enabled (so it detects congestion and send TMMBR to Marielle)
 *  - Marielle original target bitrate is 60kb/s but the network would allow only 24kb/s
 *  - Congestion is detected on Margaux' side (it requires approx. 5 seconds), she sends a TMMBR to Marielle
 *    with target bitrate at 0.7 the estimated bandwith (-> ends up around 15kb/s on IPV4)
 *  - As Marielle is given a bitrate lower than the original one, she starts sending duplicate packet to enable bandwith
 * measure by Margaux
 *  - Once the congestion is detected to be over (around 20 more seconds), Margaux sends another TMMBR to
 *    increase the target bitrate to 0.9 the estimated bandwidth (around 20kb/s on IPV4)
 *  - Wait 20s and check the ABE is still ON but no false positive arises
 *  - At this point the restricion on bandwidth is lifted, so Margaux' bandwidth estimator measure an available
 * bandwidth much higher than the original requested (60kb/s)
 *  - Margaux sends a TMMBR to restore the original bitrate
 *  - Marielle stops sending duplicates for the audio bw estimator
 */
static void audio_bandwidth_estimator(void) {
	int dummy = 0;
	char *file = bc_tester_res(HELLO_16K_1S_FILE);
	char *random_filename = ms_tester_get_random_filename(RECORDED_16K_1S_FILE, ".wav");
	char *recorded_file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	/* streams use opus, enable feedback on this payload */
	payload_type_set_flag(&payload_type_opus, PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);

	/* create audio streams with bandwith controller, Marielle is sender */
	stream_manager_t *marielle = stream_manager_new(MSAudio, MARIELLE_RTP_PORT);
	MediaStream *marielle_ms = &marielle->audio_stream->ms;
	RtpSession *marielle_session = audio_stream_get_rtp_session(marielle->audio_stream);
	rtp_session_enable_avpf_feature(marielle_session, ORTP_AVPF_FEATURE_TMMBR, TRUE);

	stream_manager_t *margaux = stream_manager_new(MSAudio, MARGAUX_RTP_PORT);
	margaux->bw_controller = ms_bandwidth_controller_new(); // This will enable the bandwidth estimator on margaux side,
	                                                        // it will select the only available stream, the audio one
	MediaStream *margaux_ms = &margaux->audio_stream->ms;
	RtpSession *margaux_session = audio_stream_get_rtp_session(margaux->audio_stream);
	rtp_session_enable_avpf_feature(margaux_session, ORTP_AVPF_FEATURE_TMMBR, TRUE);

	/* receiver specific settings: enable bandwith controller*/
	ms_bandwidth_controller_add_stream(margaux->bw_controller, margaux_ms);
	media_stream_set_direction(margaux_ms, MediaStreamRecvOnly); // Margaux is not sending anything
	margaux->user_data = ms_strdup(recorded_file); // So the file is deleted when destroying the stream manager

	/* enable audio bw estimator on marielle's side to activate the sending of packets duplicates */
	OrtpAudioBandwidthEstimatorParams bweParams = {0};
	bweParams.enabled = TRUE;
	rtp_session_enable_audio_bandwidth_estimator(marielle_session, &bweParams);

	const abe_stats_t *marielle_abe_stats = rtp_session_get_audio_bandwidth_estimator_stats(marielle_session);
	const abe_stats_t *margaux_abe_stats = rtp_session_get_audio_bandwidth_estimator_stats(margaux_session);

	/* Network simulator: limit Marielle's outbound to 24kb/s */
	int pause_time = 0;
	OrtpNetworkSimulatorParams networkParams = {0};
	networkParams.enabled = TRUE;
	networkParams.loss_rate = 0;
	networkParams.max_bandwidth = 24000.f; // Max bandwidth simulated is 24 kb/s
	networkParams.max_buffer_size = 72000;
	networkParams.latency = 60;
	networkParams.mode = OrtpNetworkSimulatorOutbound;
	rtp_session_enable_network_simulation(marielle_ms->sessions.rtp_session, &networkParams);

	/* Start audio streams */
	audio_manager_start(marielle, OPUS_PAYLOAD_TYPE, margaux->local_rtp, 60000, file, NULL); // target 60 kb/s
	ms_filter_call_method(marielle->audio_stream->soundread, MS_FILE_PLAYER_LOOP, &pause_time);
	audio_manager_start(margaux, OPUS_PAYLOAD_TYPE, marielle->local_rtp, 0, NULL, recorded_file);
	free(recorded_file);
	free(file);
	// When the stream starts, Marielle target bitrate is 60 kb/s
	BC_ASSERT_EQUAL(marielle_ms->target_bitrate, 60000, int, "%d");
	BC_ASSERT_EQUAL(marielle_abe_stats->sent_dup, 0, int, "%d");
	BC_ASSERT_EQUAL(margaux_abe_stats->recv_dup, 0, int, "%d");

	// the stream starts congested as target bitrate is set to 60kb/s but network simulator limit it to 24
	// It shall take around 5 seconds to detect congestion on margaux' side and send a TMMBR to marielle
	iterate_adaptive_stream_bool(marielle, margaux, 15000, &margaux->bw_controller->congestion_detected, TRUE);
	BC_ASSERT_TRUE(margaux->bw_controller->congestion_detected);
	// it can take up to 20 seconds for the congestion to be considered finished
	// When the congestion is finished, the requested bandwidth goes up
	iterate_adaptive_stream_bool(marielle, margaux, 25000, &margaux->bw_controller->congestion_detected, FALSE);
	BC_ASSERT_FALSE(margaux->bw_controller->congestion_detected);
	// Check Marielle target bitrate was lowered by the congestion detection (even after the end of it)
	BC_ASSERT_LOWER(marielle_ms->target_bitrate, 30000, int, "%d");
	BC_ASSERT_GREATER(marielle_abe_stats->sent_dup, 0, int, "%d");

	// Wait 25 seconds, check no high bandwidth was detected but we get some duplicates
	uint32_t margaux_recv_dup = margaux_abe_stats->recv_dup;
	uint64_t margaux_recv_rtp = margaux_session->stats.packet_recv;
	iterate_adaptive_stream(marielle, margaux, 25000, &dummy, 1);
	BC_ASSERT_LOWER(marielle_ms->target_bitrate, 30000, int,
	                "%d"); // target bitrate must be unchanged : ABE did not produced a false positive
	// ABE duplicated paquets/RTP paquets received, should be around 1/10
	BC_ASSERT_GREATER((double)(margaux_abe_stats->recv_dup - margaux_recv_dup) /
	                      (double)(margaux_session->stats.packet_recv - margaux_recv_rtp),
	                  0.075, double, "%f");

	// remove the outbound limit on marielle's side, bandwidth estimator can figure that we can go back to the
	// original target bitrate
	networkParams.enabled = FALSE;
	rtp_session_enable_network_simulation(marielle_ms->sessions.rtp_session, &networkParams);
	// Wait for Marielle target bitrate to be back to the original target bitrate, it may take a while
	iterate_adaptive_stream(marielle, margaux, 25000, &marielle_ms->target_bitrate, 59999);
	BC_ASSERT_EQUAL(marielle_ms->target_bitrate, 60000, int, "%d");
	BC_ASSERT_GREATER(margaux_abe_stats->recv_dup, 0, int, "%d");

	// Now Marielle should stop sending duplicates packets
	// Wait 10 seconds and check we didn't sent more than already did
	int marielle_abe_dup_sent = marielle_abe_stats->sent_dup;
	iterate_adaptive_stream(marielle, margaux, 10000, &dummy, 1);
	BC_ASSERT_EQUAL(marielle_abe_stats->sent_dup, marielle_abe_dup_sent, int, "%d");
	// ABE duplicated packet are swallowed before reaching the 'real' duplicated packet detector
	BC_ASSERT_EQUAL((double)margaux_session->stats.packet_dup_recv, 0, double, "%f");

	/* cleaning */
	ms_bandwidth_controller_remove_stream(margaux->bw_controller, &(margaux->audio_stream->ms));
	ms_bandwidth_controller_destroy(margaux->bw_controller);
	stream_manager_delete(marielle, TRUE);
	stream_manager_delete(margaux, TRUE);
}

static test_t tests[] = {
    TEST_NO_TAG("Packet duplication", packet_duplication),
    TEST_NO_TAG("Upload bandwidth computation", upload_bandwidth_computation),
    TEST_NO_TAG("Loss rate estimation", loss_rate_estimation),
    TEST_NO_TAG("Loss rate estimationBV16", loss_rate_estimation_bv16),

    TEST_NO_TAG("Upload bitrate [pcma] - 3g", upload_bitrate_pcma_3g),
    TEST_NO_TAG("Upload bitrate [speex] - low", upload_bitrate_speex_low),
    TEST_NO_TAG("Upload bitrate [speex] - 3g", upload_bitrate_speex_3g),
    TEST_NO_TAG("Upload bitrate [opus] - edge", upload_bitrate_opus_edge),
    TEST_NO_TAG("Upload bitrate [opus] - 3g", upload_bitrate_opus_3g),

#if VIDEO_ENABLED && 0
    TEST_NO_TAG("Network detection [VP8] - ideal", adaptive_vp8_ideal),
    TEST_NO_TAG("Network detection [VP8] - lossy", adaptive_vp8_lossy),
    TEST_NO_TAG("Network detection [VP8] - congested", adaptive_vp8_congestion),
    TEST_NO_TAG("Network detection [VP8] - lossy congested", adaptive_vp8_lossy_congestion),
#endif
    TEST_NO_TAG("Video bandwidth estimator", video_bandwidth_estimator),
    TEST_NO_TAG("Audio bandwidth estimator", audio_bandwidth_estimator),
};

test_suite_t adaptive_test_suite = {
    "AdaptiveAlgorithm", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0};
