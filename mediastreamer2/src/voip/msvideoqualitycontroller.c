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

#include "mediastreamer2/msvideoqualitycontroller.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/mscommon.h"

#define INCREASE_LOSS_RATE_TIMER_DELAY 20
#define INCREASE_TIMER_DELAY 10
#define INCREASE_BITRATE_THRESHOLD 1.1f
#define LOSS_RATE_SEQ_INTERVAL 60
#define LOSS_RATE_TIME_INTERVAL 15000

MSVideoQualityController *ms_video_quality_controller_new(struct _VideoStream *stream) {
	MSVideoQualityController *obj = ms_new0(MSVideoQualityController, 1);

	obj->stream = stream;
	obj->last_tmmbr = -1;
	obj->increase_timer_running = FALSE;
	obj->increase_loss_rate_timer_running = TRUE;
	obj->increase_loss_rate_timer_start = ms_time(NULL);
	obj->initial_bitrate = 0;
	RtpSession *session = video_stream_get_rtp_session(stream);
	obj->smooth_loss_rate_estimator =
	    ortp_loss_rate_estimator_new(LOSS_RATE_SEQ_INTERVAL, LOSS_RATE_TIME_INTERVAL, session);

	return obj;
}

void ms_video_quality_controller_destroy(MSVideoQualityController *obj) {
	ortp_loss_rate_estimator_destroy(obj->smooth_loss_rate_estimator);
	ms_free(obj);
}

void ms_video_quality_controller_update_from_feedback(MSVideoQualityController *obj, const mblk_t *rtcp) {
	const report_block_t *rb = NULL;
	if (rtcp_is_SR(rtcp)) {
		rb = rtcp_SR_get_report_block(rtcp, 0);
	} else if (rtcp_is_RR(rtcp)) {
		rb = rtcp_RR_get_report_block(rtcp, 0);
	} else {
		return;
	}
	if (rb) {
		RtpSession *session = video_stream_get_rtp_session(obj->stream);
		ortp_loss_rate_estimator_process_report_block(obj->smooth_loss_rate_estimator, session, rb);
	}
}

static MSVideoConfiguration ms_video_quality_controller_find_best_vconf(MSVideoQualityController *obj,
                                                                        MSVideoConfiguration *vconf_list,
                                                                        int bitrate_limit) {
	MSVideoConfiguration best_vconf;
	if (obj->stream->max_sent_vsize.height > 0 && obj->stream->max_sent_vsize.width > 0) {
		best_vconf = ms_video_find_best_configuration_for_size_and_bitrate(
		    vconf_list, obj->stream->max_sent_vsize, ms_factory_get_cpu_count(obj->stream->ms.factory), bitrate_limit);
	} else {
		best_vconf = ms_video_find_best_configuration_for_bitrate(vconf_list, bitrate_limit,
		                                                          ms_factory_get_cpu_count(obj->stream->ms.factory));
	}
	return best_vconf;
}

static void ms_video_quality_controller_update_video_definition(MSVideoQualityController *obj,
                                                                MSVideoConfiguration *best) {
	/*
	 * Changing the resolution requires a call to video_stream_change_camera() so that the video graph is
	 * reconfigured.
	 */
	obj->stream->sent_vsize = best->vsize;
	obj->stream->preview_vsize = best->vsize;
	media_stream_set_target_network_bitrate(&obj->stream->ms, best->required_bitrate);
	video_stream_update_video_params(obj->stream);
	obj->last_vsize = best->vsize;
}

static void ms_video_quality_controller_update_fps_only(MSVideoQualityController *obj,
                                                        MSVideoConfiguration *current,
                                                        MSVideoConfiguration *best) {
	if (current->fps != best->fps) {
		ms_message("MSVideoQualityController [%p]: Bitrate update will change fps", obj);
		current->fps = best->fps;
		ms_filter_call_method(obj->stream->source, MS_FILTER_SET_FPS, &best->fps);
		obj->stream->configured_fps = best->fps;
	}
}

static void ms_video_quality_controller_update_encoder_bitrate(MSVideoQualityController *obj,
                                                               int bitrate,
                                                               MSVideoConfiguration *current,
                                                               MSVideoConfiguration *best) {
	int new_bitrate_limit = bitrate < best->bitrate_limit ? bitrate : best->bitrate_limit;
	ms_message("MSVideoQualityController [%p]: Changing video encoder's output bitrate to %i", obj, new_bitrate_limit);
	current->required_bitrate = new_bitrate_limit;
	current->bitrate_limit = best->bitrate_limit;
	media_stream_set_target_network_bitrate(&obj->stream->ms, new_bitrate_limit);
	if (ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_SET_CONFIGURATION, current) != 0) {
		ms_warning("MSVideoQualityController [%p]: Failed to apply fps and bitrate constraint to %s", obj,
		           obj->stream->ms.encoder->desc->name);
	}
}

static float
update_fec_params_from_loss_rate(MSVideoQualityController *obj, int bitrate, uint8_t *fec_level, float *loss_rate) {
	MediaStream *ms = (MediaStream *)&obj->stream->ms;
	if (!ms->fec_parameters) return 0.;

	// get best fec config, do not update yet
	*loss_rate = ortp_loss_rate_estimator_get_value(obj->smooth_loss_rate_estimator);
	float overhead_current = fec_stream_get_overhead(ms->fec_stream);
	float overhead_needed = 0.;
	*fec_level =
	    fec_params_estimate_best_level(ms->fec_parameters, *loss_rate, bitrate, overhead_current, &overhead_needed);

	return overhead_needed;
}

static void ms_video_quality_controller_update_fec_parameters(MediaStream *ms, uint8_t fec_level) {

	fec_params_update(ms->fec_parameters, fec_level);

	if (ms->encoder && ms_filter_has_method(ms->encoder, MS_VIDEO_ENCODER_ENABLE_DIVIDE_PACKETS_EQUAL_SIZE)) {
		bool_t enable = TRUE;
		if (fec_level == 0) enable = FALSE;
		ms_filter_call_method(ms->encoder, MS_VIDEO_ENCODER_ENABLE_DIVIDE_PACKETS_EQUAL_SIZE, &enable);
	}
}

static void update_video_quality_from_bitrate(MSVideoQualityController *obj,
                                              int bitrate,
                                              float bitrate_threshold,
                                              bool_t update_only_fps) {
	MSVideoConfiguration *vconf_list = NULL;
	MSVideoConfiguration current_vconf, best_vconf;
	MediaStream *ms = (MediaStream *)&obj->stream->ms;
	uint8_t fec_level = 0;
	float loss_rate = 0.;
	ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, &vconf_list);

	if (vconf_list == NULL) return;
	ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);
	ms_message("MSVideoQualityController [%p]: current video config is: required bitrate: %d, bitrate limit: %d, "
	           "size: %dx%d, fps: %f",
	           obj, current_vconf.required_bitrate, current_vconf.bitrate_limit, current_vconf.vsize.width,
	           current_vconf.vsize.height, current_vconf.fps);

	if (!update_only_fps) {
		int current_bitrate = media_stream_get_target_network_bitrate(&obj->stream->ms);
		int bitrate_limit = (int)(bitrate / bitrate_threshold);

		if (ms->fec_parameters) {
			float overhead_needed = update_fec_params_from_loss_rate(obj, bitrate_limit, &fec_level, &loss_rate);
			bitrate_limit = (int)((float)bitrate_limit / (1. + overhead_needed));
			ms_message("MSVideoQualityController [%p]: update available bitrate to %d after FEC", obj, bitrate_limit);
		}

		if (bitrate_limit >= current_bitrate || bitrate_threshold == 1.) {

			best_vconf = ms_video_quality_controller_find_best_vconf(obj, vconf_list, bitrate_limit);
			if (!ms_video_size_equal(obj->last_vsize, best_vconf.vsize) &&
			    best_vconf.vsize.width * best_vconf.vsize.height !=
			        current_vconf.vsize.width * current_vconf.vsize.height) {
				ms_message("MSVideoQualityController [%p]: Changing video definition to %dx%d at %f fps", obj,
				           best_vconf.vsize.width, best_vconf.vsize.height, best_vconf.fps);
				ms_video_quality_controller_update_fps_only(obj, &current_vconf, &best_vconf);
				ms_video_quality_controller_update_video_definition(obj, &best_vconf);
				if (ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_SET_CONFIGURATION, &best_vconf) !=
				    0) {
					ms_warning("MSVideoQualityController [%p]: Failed to apply fps and bitrate constraint to %s", obj,
					           obj->stream->ms.encoder->desc->name);
				}
				if (ms->fec_parameters) ms_video_quality_controller_update_fec_parameters(ms, fec_level);
				return;
			}
		} else {
			ms_message("MSVideoQualityController [%p]: new bitrate not sufficient to try a video resolution upgrade "
			           "(%d < %d).",
			           obj, bitrate_limit, current_bitrate);
		}
	}

	if (ms->fec_parameters) {
		float overhead_needed = update_fec_params_from_loss_rate(obj, bitrate, &fec_level, &loss_rate);
		bitrate = (int)((float)bitrate / (1. + overhead_needed));
		ms_message("MSVideoQualityController [%p]: update available bitrate to %d after FEC", obj, bitrate);
		if (bitrate == current_vconf.required_bitrate) {
			ms_message("MSVideoQualityController [%p]: same bitrate, do not update encoder configuration", obj);
			ms_video_quality_controller_update_fec_parameters(ms, fec_level);
			return;
		}
	}

	best_vconf = ms_video_find_best_configuration_for_size_and_bitrate(
	    vconf_list, current_vconf.vsize, ms_factory_get_cpu_count(obj->stream->ms.factory), bitrate);
	ms_message("MSVideoQualityController [%p]: best video config is: required bitrate: %d, bitrate limit: %d, "
	           "size: %dx%d, fps: %f",
	           obj, best_vconf.required_bitrate, best_vconf.bitrate_limit, best_vconf.vsize.width,
	           best_vconf.vsize.height, best_vconf.fps);

	ms_video_quality_controller_update_fps_only(obj, &current_vconf, &best_vconf);
	ms_video_quality_controller_update_encoder_bitrate(obj, bitrate, &current_vconf, &best_vconf);
	if (ms->fec_parameters) ms_video_quality_controller_update_fec_parameters(ms, fec_level);
}

void ms_video_quality_controller_process_timers(MSVideoQualityController *obj) {

	if (obj->increase_timer_running) {
		time_t current_time = ms_time(NULL);
		if (current_time - obj->increase_timer_start >= INCREASE_TIMER_DELAY) {
			obj->increase_loss_rate_timer_running = FALSE;
			ms_message("MSVideoQualityController [%p]: %d seconds passed since start of TMMBR increase (last received "
			           "%f kbit/s), "
			           "increasing video quality...",
			           obj->stream, INCREASE_TIMER_DELAY, obj->last_tmmbr * 1e-3);
			update_video_quality_from_bitrate(obj, obj->last_tmmbr, INCREASE_BITRATE_THRESHOLD, FALSE);

			// set the value of the initial bitrate
			if (obj->last_tmmbr == -1) {
				if (obj->initial_bitrate == 0) {
					MSVideoConfiguration current_vconf;
					ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);
					obj->initial_bitrate = current_vconf.required_bitrate;
				}
				ms_message("MSVideoQualityController [%p]: take reference bitrate to = %d", obj, obj->initial_bitrate);
			}

			obj->increase_timer_running = FALSE;
			obj->increase_loss_rate_timer_start = ms_time(NULL);
			obj->increase_loss_rate_timer_running = TRUE;

			MSVideoConfiguration current_vconf;
			ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);
			ms_message("MSVideoQualityController [%p]: -> required bitrate (timer): %d, bitrate limit: %d, size: "
			           "%dx%d, fps: %f",
			           obj, current_vconf.required_bitrate, current_vconf.bitrate_limit, current_vconf.vsize.width,
			           current_vconf.vsize.height, current_vconf.fps);
		}
	}

	if (obj->increase_loss_rate_timer_running) {
		time_t current_time = ms_time(NULL);
		if (current_time - obj->increase_loss_rate_timer_start >= INCREASE_LOSS_RATE_TIMER_DELAY) {

			MediaStream *ms = (MediaStream *)&obj->stream->ms;
			if (!ms->fec_parameters) return;

			obj->increase_loss_rate_timer_running = FALSE;

			float loss_rate = ortp_loss_rate_estimator_get_value(obj->smooth_loss_rate_estimator);

			ms_message("MSVideoQualityController [%p]:LOST RATE TIMER! after %d seconds, "
			           "loss rate is %f, updating fec parameters, increasing video fps and bitrate",
			           obj->stream, INCREASE_LOSS_RATE_TIMER_DELAY, loss_rate);

			int bitrate = obj->last_tmmbr;
			if (bitrate == -1) {
				// if no TMMBR has been received, take the current encoder values as bitrate
				MSVideoConfiguration current_vconf;
				ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);
				if (obj->initial_bitrate == 0) {
					bitrate = current_vconf.required_bitrate;
					obj->initial_bitrate = bitrate;

				} else {
					// set the initial bitrate in order to keep a reference bitrate when the fec and the encoder
					// are updated
					bitrate = obj->initial_bitrate;
				}
				ms_message("MSVideoQualityController [%p]: take reference bitrate to = %d", obj, bitrate);
			}
			update_video_quality_from_bitrate(obj, bitrate, 1.0f, TRUE);

			MSVideoConfiguration current_vconf;
			ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);
			ms_message("MSVideoQualityController [%p]: -> required bitrate (loss rate timer): %d, bitrate limit: "
			           "%d, size: %dx%d, fps: %f",
			           obj, current_vconf.required_bitrate, current_vconf.bitrate_limit, current_vconf.vsize.width,
			           current_vconf.vsize.height, current_vconf.fps);
			obj->increase_loss_rate_timer_running = TRUE;
			obj->increase_loss_rate_timer_start = ms_time(NULL);
		}
	}
}

void ms_video_quality_controller_update_from_tmmbr(MSVideoQualityController *obj, int tmmbr) {
	const char *preset = NULL;
	preset = video_stream_get_video_preset((VideoStream *)obj->stream);

	obj->increase_loss_rate_timer_running = FALSE;

	if (preset && strcmp(preset, "custom") == 0) {
		MSVideoConfiguration *vconf_list = NULL;
		MSVideoConfiguration current_vconf;
		ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, &vconf_list);
		if (vconf_list == NULL) return;
		ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);

		MediaStream *ms = (MediaStream *)&obj->stream->ms;
		int available_bandwidth = tmmbr;

		if (ms->fec_parameters) {
			float loss_rate = 0.f;
			uint8_t fec_level = 0;
			float overhead_needed =
			    update_fec_params_from_loss_rate(obj, current_vconf.required_bitrate, &fec_level, &loss_rate);
			available_bandwidth = (int)((float)available_bandwidth / (1. + overhead_needed));
			ms_video_quality_controller_update_fec_parameters(ms, fec_level);
		}
		ms_message("MSVideoQualityController [%p]: update fec params from tmmbr", obj);
		ms_video_quality_controller_update_encoder_bitrate(obj, available_bandwidth, &current_vconf, &current_vconf);
		return;
	}

	if (obj->last_tmmbr == -1) {
		MSVideoConfiguration current_vconf;

		ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);

		if (tmmbr < current_vconf.required_bitrate) {
			ms_message("MSVideoQualityController [%p]: First TMMBR (%f kbit/s) inferior to preferred video required "
			           "bitrate, reducing video quality...",
			           obj, tmmbr * 1e-3);
			update_video_quality_from_bitrate(obj, tmmbr, 1.0f, FALSE);
			MSVideoConfiguration new_vconf;
			ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &new_vconf);
			ms_message("MSVideoQualityController [%p]: -> required bitrate (first TMMBR): %d, bitrate limit: %d, "
			           "size: %dx%d, fps: %f\n",
			           obj, new_vconf.required_bitrate, new_vconf.bitrate_limit, new_vconf.vsize.width,
			           new_vconf.vsize.height, new_vconf.fps);
			obj->last_tmmbr = tmmbr;
			obj->increase_loss_rate_timer_running = TRUE;
			obj->increase_loss_rate_timer_start = ms_time(NULL);
			return;
		}
	}

	if (tmmbr > obj->last_tmmbr) {
		if (!obj->increase_timer_running) {
			obj->increase_timer_start = ms_time(NULL);
			obj->increase_timer_running = TRUE;
		}
		ms_message("MSVideoQualityController [%p]: TMMBR increase (%f kbit/s), increasing video fps and bitrate", obj,
		           tmmbr * 1e-3);
		update_video_quality_from_bitrate(obj, tmmbr, 1.0f, TRUE);

	} else if (tmmbr < obj->last_tmmbr) {
		if (obj->increase_timer_running) obj->increase_timer_running = FALSE;

		ms_message("MSVideoQualityController [%p]: tmmbr is lower than previous one (%f kbit/s), reducing video "
		           "quality...",
		           obj, tmmbr * 1e-3);
		update_video_quality_from_bitrate(obj, tmmbr, 1.0f, FALSE);
	}

	MSVideoConfiguration current_vconf;
	ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);
	ms_message("MSVideoQualityController [%p]: -> required bitrate (TMMBR received): %d, bitrate limit: %d, size: "
	           "%dx%d, fps: %f",
	           obj, current_vconf.required_bitrate, current_vconf.bitrate_limit, current_vconf.vsize.width,
	           current_vconf.vsize.height, current_vconf.fps);

	obj->last_tmmbr = tmmbr;
	obj->increase_loss_rate_timer_start = ms_time(NULL);
	obj->increase_loss_rate_timer_running = TRUE;
}