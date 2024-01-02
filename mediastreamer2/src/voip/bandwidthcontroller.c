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

#include "mediastreamer2/bitratecontrol.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/mscommon.h"
#include <ortp/ortp.h>

#define NO_INCREASE_THRESHOLD_FOR_CONGESTION 1.4
#define NO_INCREASE_THRESHOLD 1.1

MSBandwidthController *ms_bandwidth_controller_new(void) {
	MSBandwidthController *obj = ms_new0(MSBandwidthController, 1);
	return obj;
}

void ms_bandwidth_controller_reset_state(MSBandwidthController *obj) {
	memset(&obj->stats, 0, sizeof(obj->stats));
	obj->download_video_bandwidth_available_estimated = 0;
	obj->download_audio_bandwidth_available_estimated = 0;
	obj->stats.estimated_download_bandwidth = 0; /*this value is computed under congestion situation, if any*/
	obj->currently_requested_stream_bandwidth = 0;
	obj->congestion_detected = 0;
}

static void ms_bandwidth_controller_send_tmmbr(MSBandwidthController *obj, struct _MediaStream *stream) {
	RtpSession *session = stream->sessions.rtp_session;
	float bandwidth = 0;
	if (obj->currently_requested_stream_bandwidth > 0 && obj->maximum_bw_usage > 0 &&
	    obj->maximum_bw_usage < obj->currently_requested_stream_bandwidth) {
		bandwidth = obj->maximum_bw_usage / bctbx_list_size(obj->controlled_streams);
	} else {
		bandwidth = obj->currently_requested_stream_bandwidth / bctbx_list_size(obj->controlled_streams);
	}
	ms_message(
	    "MSBandwidthController[%p]: for stream[%p] of type [%s], sending TMMBR for a bandwidth usage of [%f] bits/s",
	    obj, stream, ms_format_type_to_string(stream->type), bandwidth);
	rtp_session_send_rtcp_fb_tmmbr(session, (uint64_t)bandwidth);
}

void ms_bandwidth_controller_set_maximum_bandwidth_usage(MSBandwidthController *obj, int bitrate) {
	obj->maximum_bw_usage = (float)bitrate;
	if (obj->currently_requested_stream_bandwidth > 0 && obj->maximum_bw_usage > 0) {
		bctbx_list_t *elem;
		for (elem = obj->controlled_streams; elem != NULL; elem = elem->next) {
			MediaStream *ms = (MediaStream *)elem->data;
			ms_bandwidth_controller_send_tmmbr(obj, ms);
		}
	}
	/* If there is not yet currently_requested_stream_bandwidth (means no congestion detected yet and no bandwidth
	 * estimation available), then don't request anything. Let the estimation to be available first.
	 */
}

static void ms_bandwidth_controller_estimate_bandwidths(MSBandwidthController *obj) {
	bctbx_list_t *elem;
	float bandwidth = 0;
	float controlled_bandwidth = 0;

	for (elem = obj->streams; elem != NULL; elem = elem->next) {
		MediaStream *ms = (MediaStream *)elem->data;
		float bw;
		if (media_stream_get_state(ms) != MSStreamStarted) continue;
		bw = rtp_session_get_recv_bandwidth_smooth(ms->sessions.rtp_session);
		bandwidth += bw;
		if (bctbx_list_find(obj->controlled_streams, ms)) controlled_bandwidth += bw;
	}
	obj->stats.estimated_download_bandwidth = bandwidth;
	obj->stats.controlled_stream_bandwidth = controlled_bandwidth;
	ms_message(
	    "MSBandwidthController: total received bandwidth is %f kbit/s, controlled streams bandwidth is %f kbit/s",
	    bandwidth * 1e-3, obj->stats.controlled_stream_bandwidth * 1.e-3);
}

static float compute_target_bandwith_for_controlled_stream(MSBandwidthController *obj, float reduction_factor) {
	float bw_used_by_other_streams;
	float new_total_bandwidth_requested;
	float controlled_stream_bandwidth_requested;

	bw_used_by_other_streams = obj->stats.estimated_download_bandwidth - obj->stats.controlled_stream_bandwidth;
	new_total_bandwidth_requested = obj->stats.estimated_download_bandwidth * reduction_factor;
	controlled_stream_bandwidth_requested = new_total_bandwidth_requested - bw_used_by_other_streams;
	if (controlled_stream_bandwidth_requested <= 0) {
		ms_error("MSBandwidthController: total controlled streams bandwidth requested is %f (bug)",
		         controlled_stream_bandwidth_requested);
	}
	return controlled_stream_bandwidth_requested;
}

static void resync_jitter_buffers(MSBandwidthController *obj) {
	bctbx_list_t *elem;

	for (elem = obj->streams; elem != NULL; elem = elem->next) {
		MediaStream *ms = (MediaStream *)elem->data;
		rtp_session_resync(ms->sessions.rtp_session);
	}
}

static void on_congestion_state_changed(const OrtpEventData *evd, void *user_pointer) {
	MediaStream *ms = (MediaStream *)user_pointer;
	MSBandwidthController *obj = ms->bandwidth_controller;
	float controlled_stream_bandwidth_requested;
	OrtpVideoBandwidthEstimatorParams video_bandwidth_estimator_params = {0};

	if (!bctbx_list_find(obj->controlled_streams, ms)) {
		ms_message("MSBandwidthController: congestion event (%i) received on stream [%p][%s], not the controlled one.",
		           (int)evd->info.congestion_detected, ms, ms_format_type_to_string(ms->type));
		return;
	}
	obj->congestion_detected = evd->info.congestion_detected;
	if (evd->info.congestion_detected) {
		/*We are detecting a congestion. First estimate the total bandwidth received at the time of the congestion*/
		ms_bandwidth_controller_estimate_bandwidths(obj);

		if ((bctbx_list_size(obj->controlled_streams) > 1) &&
		    (rtp_session_get_recv_bandwidth_smooth(ms->sessions.rtp_session) >
		     obj->stats.estimated_download_bandwidth / bctbx_list_size(obj->controlled_streams))) {
			ms_message("MSBandwidthController: congestion detected ignored because stream [%p][%s] recv bandwidth [%f] "
			           "> average [%f]. Total bandwidth is %f, controlled streams' numbers is %d.",
			           ms, ms_format_type_to_string(ms->type),
			           rtp_session_get_recv_bandwidth_smooth(ms->sessions.rtp_session),
			           obj->stats.estimated_download_bandwidth / bctbx_list_size(obj->controlled_streams),
			           obj->stats.estimated_download_bandwidth, (int)bctbx_list_size(obj->controlled_streams));
			return;
		}

		/*we need to clear the congestion by firstly requesting a bandwidth usage much lower than the theoritically
		 possible, so that the congested router can finaly expedite all the late packets it retains.*/
		controlled_stream_bandwidth_requested = compute_target_bandwith_for_controlled_stream(obj, 0.7f);

		if (controlled_stream_bandwidth_requested > 0) {
			ms_message("MSBandwidthController: congestion detected - sending tmmbr for stream [%p][%s] for target [%f] "
			           "kbit/s. Total is [%f] kbit/s, controlled streams' numbers is %d.",
			           ms, ms_format_type_to_string(ms->type),
			           controlled_stream_bandwidth_requested * 1e-3 / bctbx_list_size(obj->controlled_streams),
			           controlled_stream_bandwidth_requested * 1e-3, (int)bctbx_list_size(obj->controlled_streams));
		}
		video_bandwidth_estimator_params.enabled = FALSE;
	} else {
		/*now that the congestion has ended, we can submit a new TMMBR to request a bandwidth closer to the maximum
		 * available*/
		controlled_stream_bandwidth_requested = compute_target_bandwith_for_controlled_stream(obj, 0.9f);

		if (controlled_stream_bandwidth_requested > 0) {
			ms_message("MSBandwidthController: congestion resolved - sending tmmbr for stream [%p][%s] for target [%f] "
			           "kbit/s. Total is [%f] kbit/s, controlled streams' numbers is %d.",
			           ms, ms_format_type_to_string(ms->type),
			           controlled_stream_bandwidth_requested * 1e-3 / bctbx_list_size(obj->controlled_streams),
			           controlled_stream_bandwidth_requested * 1e-3, (int)bctbx_list_size(obj->controlled_streams));
		}
		/*we shall reset the jitter buffers, so that they recover faster their diverged states*/
		resync_jitter_buffers(obj);
		video_bandwidth_estimator_params.enabled = TRUE;
	}
	obj->currently_requested_stream_bandwidth = controlled_stream_bandwidth_requested;
	ms_bandwidth_controller_send_tmmbr(obj, ms);
	obj->download_video_bandwidth_available_estimated = 0;
	obj->download_audio_bandwidth_available_estimated = 0;
	rtp_session_enable_video_bandwidth_estimator(ms->sessions.rtp_session, &video_bandwidth_estimator_params);
}

static void send_tmmbr_for_controlled_video_streams(MSBandwidthController *obj, float estimated_bandwidth) {
	float bw_used_by_non_controlled_streams;
	bctbx_list_t *elem;
	size_t controlled_streams_count = bctbx_list_size(obj->controlled_streams);
	float requested_stream_bandwidth;

	ms_bandwidth_controller_estimate_bandwidths(obj);
	bw_used_by_non_controlled_streams =
	    obj->stats.estimated_download_bandwidth - obj->stats.controlled_stream_bandwidth;

	requested_stream_bandwidth = (estimated_bandwidth - bw_used_by_non_controlled_streams);

	ms_message("MSBandwidthController[%p]: video bandwidth estimation available:\n"
	           "value: %f kbit/s\n"
	           "bandwidth used by non-controlled streams: %f\n"
	           "number of controlled streams: %d",
	           obj, estimated_bandwidth / 1000, bw_used_by_non_controlled_streams / 1000,
	           (int)controlled_streams_count);

	if (requested_stream_bandwidth <= 0 || controlled_streams_count == 0) {
		ms_error("MSBandwidthController[%p]: inconsistent measurements or parameters, aborting.", obj);
		return;
	}

	obj->currently_requested_stream_bandwidth = requested_stream_bandwidth;
	obj->download_video_bandwidth_available_estimated = estimated_bandwidth;

	/* send a TMMBR request for each one of the controlled video streams. */
	for (elem = obj->controlled_streams; elem != NULL; elem = elem->next) {
		MediaStream *ms = (MediaStream *)elem->data;
		ms_bandwidth_controller_send_tmmbr(obj, ms);
	}
}

static void on_video_bandwidth_estimation_available(const OrtpEventData *evd, void *user_pointer) {
	MediaStream *ms = (MediaStream *)user_pointer;
	MSBandwidthController *obj = ms->bandwidth_controller;
	if (!obj->congestion_detected) {
		float estimated_bitrate = evd->info.video_bandwidth_available;
		if (estimated_bitrate <= obj->download_video_bandwidth_available_estimated) {
			ms_message("MSBandwidthController: %p not using new total video bandwidth estimation (%f kbit/s) because "
			           "it's not greater than the previous one (%f kbit/s)",
			           ms, estimated_bitrate / 1000, obj->download_video_bandwidth_available_estimated / 1000);
			return;
		}

		if (obj->stats.estimated_download_bandwidth != 0 &&
		    estimated_bitrate <= (NO_INCREASE_THRESHOLD_FOR_CONGESTION * obj->stats.estimated_download_bandwidth)) {
			ms_message("MSBandwidthController: %p not using new total video bandwidth estimation (%f kbit/s) because "
			           "it's not enough greater than bandwidth measured under congestion (%f kbit/s)",
			           ms, estimated_bitrate / 1000, obj->stats.estimated_download_bandwidth / 1000);
			return;
		}
		send_tmmbr_for_controlled_video_streams(obj, estimated_bitrate);
	}
}

static void on_audio_bandwidth_estimation_available(const OrtpEventData *evd, void *user_pointer) {
	MediaStream *ms = (MediaStream *)user_pointer;
	MSBandwidthController *obj = ms->bandwidth_controller;
	if (!obj->congestion_detected) {
		float estimated_bitrate = evd->info.audio_bandwidth_available;
		if (estimated_bitrate <= obj->download_audio_bandwidth_available_estimated * NO_INCREASE_THRESHOLD) {
			ms_message("MSBandwidthController: %p not using new total audio bandwidth estimation (%f kbit/s) because "
			           "it's not enough greater than the previous one (%f kbit/s)",
			           ms, estimated_bitrate / 1000, obj->download_audio_bandwidth_available_estimated / 1000);
			return;
		}

		if (obj->stats.estimated_download_bandwidth != 0 &&
		    estimated_bitrate <= (NO_INCREASE_THRESHOLD_FOR_CONGESTION * obj->stats.estimated_download_bandwidth)) {
			ms_message("MSBandwidthController: %p not using new total audio bandwidth estimation (%f kbit/s) because "
			           "it's not enough greater than bandwidth measured under congestion (%f kbit/s)",
			           ms, estimated_bitrate / 1000, obj->stats.estimated_download_bandwidth / 1000);
			return;
		}

		ms_message("MSBandwidthController: audio bandwidth estimation available, sending tmmbr for stream [%p][%s] for "
		           "target [%f] kbit/s. Total is [%f] kbit/s, controlled streams' number is %d.",
		           ms, ms_format_type_to_string(ms->type),
		           estimated_bitrate / (bctbx_list_size(obj->controlled_streams) * 1000), estimated_bitrate / 1000,
		           (int)bctbx_list_size(obj->controlled_streams));
		obj->download_audio_bandwidth_available_estimated = estimated_bitrate;
		obj->currently_requested_stream_bandwidth = estimated_bitrate;
		ms_bandwidth_controller_send_tmmbr(obj, ms);
	}
}

/*This function just selects most consuming video streams, or an audio stream otherwise.*/
void ms_bandwidth_controller_elect_controlled_streams(MSBandwidthController *obj) {
	bctbx_list_t *elem;
	OrtpVideoBandwidthEstimatorParams params = {0};
	MediaStream *as = NULL;

	if (obj->controlled_streams) {
		bctbx_list_free(obj->controlled_streams);
		obj->controlled_streams = NULL;
	}

	for (elem = obj->streams; elem != NULL; elem = elem->next) {
		MediaStream *ms = (MediaStream *)elem->data;
		if (ms->type == MSVideo) {
			VideoStream *vs = (VideoStream *)ms;
			if (vs->content != MSVideoContentThumbnail && media_stream_get_direction(ms) != MediaStreamSendOnly) {
				if (!ms->sessions.rtp_session->video_bandwidth_estimator_enabled) {
					ortp_ev_dispatcher_connect(media_stream_get_event_dispatcher(ms),
					                           ORTP_EVENT_NEW_VIDEO_BANDWIDTH_ESTIMATION_AVAILABLE, 0,
					                           on_video_bandwidth_estimation_available, ms);
					params.enabled = TRUE;
					rtp_session_enable_video_bandwidth_estimator(ms->sessions.rtp_session, &params);
				}
				obj->controlled_streams = bctbx_list_append(obj->controlled_streams, ms);
			} else if (ms->sessions.rtp_session->video_bandwidth_estimator_enabled) {
				ortp_ev_dispatcher_disconnect(media_stream_get_event_dispatcher(ms),
				                              ORTP_EVENT_NEW_VIDEO_BANDWIDTH_ESTIMATION_AVAILABLE, 0,
				                              on_video_bandwidth_estimation_available);
				params.enabled = FALSE;
				rtp_session_enable_video_bandwidth_estimator(ms->sessions.rtp_session, &params);
			}
		} else {
			as = ms;
		}
	}

	if (as) {                           // There is an audio stream
		if (!obj->controlled_streams) { // There is no video stream to control -> enable control on the audio stream
			// Audiostream is enabled even when in MediaStreamSendOnly as it must send duplicates if needed
			if (!as->sessions.rtp_session->audio_bandwidth_estimator_enabled) {
				OrtpAudioBandwidthEstimatorParams params_audio = {0};
				ortp_ev_dispatcher_connect(media_stream_get_event_dispatcher(as),
				                           ORTP_EVENT_NEW_AUDIO_BANDWIDTH_ESTIMATION_AVAILABLE, 0,
				                           on_audio_bandwidth_estimation_available, as);
				params_audio.enabled = TRUE;
				rtp_session_enable_audio_bandwidth_estimator(as->sessions.rtp_session, &params_audio);
			}
			obj->controlled_streams = bctbx_list_append(obj->controlled_streams, as);
		} else { // there is at leat one video stream, make sure the audio stream disable its bandwidth estimator
			if (as->sessions.rtp_session->audio_bandwidth_estimator_enabled) {
				OrtpAudioBandwidthEstimatorParams params_audio = {0};
				ortp_ev_dispatcher_disconnect(media_stream_get_event_dispatcher(as),
				                              ORTP_EVENT_NEW_AUDIO_BANDWIDTH_ESTIMATION_AVAILABLE, 0,
				                              on_audio_bandwidth_estimation_available);
				params_audio.enabled = FALSE;
				rtp_session_enable_audio_bandwidth_estimator(as->sessions.rtp_session, &params_audio);
			}
		}
	}

	ms_bandwidth_controller_reset_state(obj);
}

void ms_bandwidth_controller_add_stream(MSBandwidthController *obj, struct _MediaStream *stream) {
	ortp_ev_dispatcher_connect(media_stream_get_event_dispatcher(stream), ORTP_EVENT_CONGESTION_STATE_CHANGED, 0,
	                           on_congestion_state_changed, stream);
	rtp_session_enable_congestion_detection(stream->sessions.rtp_session, TRUE);
	stream->bandwidth_controller = obj;
	obj->streams = bctbx_list_append(obj->streams, stream);
	ms_bandwidth_controller_elect_controlled_streams(obj);
}

void ms_bandwidth_controller_remove_stream(MSBandwidthController *obj, struct _MediaStream *stream) {
	OrtpVideoBandwidthEstimatorParams params_video = {0};
	OrtpAudioBandwidthEstimatorParams params_audio = {0};
	if (bctbx_list_find(obj->streams, stream) == NULL) return;
	ortp_ev_dispatcher_disconnect(media_stream_get_event_dispatcher(stream), ORTP_EVENT_CONGESTION_STATE_CHANGED, 0,
	                              on_congestion_state_changed);
	rtp_session_enable_congestion_detection(stream->sessions.rtp_session, FALSE);
	ortp_ev_dispatcher_disconnect(media_stream_get_event_dispatcher(stream),
	                              ORTP_EVENT_NEW_VIDEO_BANDWIDTH_ESTIMATION_AVAILABLE, 0,
	                              on_video_bandwidth_estimation_available);
	ortp_ev_dispatcher_disconnect(media_stream_get_event_dispatcher(stream),
	                              ORTP_EVENT_NEW_AUDIO_BANDWIDTH_ESTIMATION_AVAILABLE, 0,
	                              on_audio_bandwidth_estimation_available);
	params_video.enabled = FALSE;
	rtp_session_enable_video_bandwidth_estimator(stream->sessions.rtp_session, &params_video);
	rtp_session_enable_audio_bandwidth_estimator(stream->sessions.rtp_session, &params_audio);
	stream->bandwidth_controller = NULL;
	obj->streams = bctbx_list_remove(obj->streams, stream);
	ms_bandwidth_controller_elect_controlled_streams(obj);
}

const MSBandwidthControllerStats *ms_bandwidth_controller_get_stats(MSBandwidthController *obj) {
	return &obj->stats;
}

void ms_bandwidth_controller_destroy(MSBandwidthController *obj) {
	bctbx_list_free(obj->streams);
	obj->streams = NULL;
	if (obj->controlled_streams) bctbx_list_free(obj->controlled_streams);
	obj->controlled_streams = NULL;
	ms_free(obj);
}
