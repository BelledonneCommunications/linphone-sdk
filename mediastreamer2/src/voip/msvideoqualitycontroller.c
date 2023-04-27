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
#include <math.h>
#define INCREASE_TIMER_DELAY 10
#define INCREASE_BITRATE_THRESHOLD 1.1f

MSVideoQualityController *ms_video_quality_controller_new(struct _VideoStream *stream) {
	MSVideoQualityController *obj = ms_new0(MSVideoQualityController, 1);

	obj->stream = stream;
	obj->last_tmmbr = -1;
	obj->increase_timer_running = FALSE;

	return obj;
}

void ms_video_quality_controller_destroy(MSVideoQualityController *obj) {
	ms_free(obj);
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
                                                                MSVideoConfiguration *current,
                                                                MSVideoConfiguration *best) {

	if (ms_video_size_equal(obj->last_vsize, best->vsize) ||
	    best->vsize.width * best->vsize.height == current->vsize.width * current->vsize.height)
		return;

	ms_message("MSVideoQualityController [%p]: Changing video definition to %dx%d at %f fps", obj, best->vsize.width,
	           best->vsize.height, best->fps);
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
                                                               int old_bitrate,
                                                               MSVideoConfiguration *current,
                                                               MSVideoConfiguration *best) {
	int new_bitrate_limit = old_bitrate < best->bitrate_limit ? old_bitrate : best->bitrate_limit;
	ms_message("MSVideoQualityController [%p]: Changing video encoder's output bitrate to %i", obj, new_bitrate_limit);
	current->required_bitrate = new_bitrate_limit;
	media_stream_set_target_network_bitrate(&obj->stream->ms, new_bitrate_limit);
	if (ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_SET_CONFIGURATION, current) != 0) {
		ms_warning("MSVideoQualityController [%p]: Failed to apply fps and bitrate constraint to %s", obj,
		           obj->stream->ms.encoder->desc->name);
	}
}

static float compute_flexfec_bandwidth_ratio(MSVideoConfiguration *vconf, int br_limit) {
	return ((float)br_limit - (float)vconf->required_bitrate) / (float)vconf->required_bitrate;
}
static int update_fec_params_from_bitrate(MSVideoQualityController *obj, MSVideoConfiguration *current, int br_limit) {

	MediaStream *ms = (MediaStream *)&obj->stream->ms;
	if (!ms->fec_parameters) return br_limit;

	float ratio = compute_flexfec_bandwidth_ratio(current, br_limit);
	float overhead_needed = fec_params_update_from_ratio(ms->fec_parameters, ratio);
	return (int)((1 - overhead_needed) * (float)br_limit);
}

static void update_video_quality_from_bitrate(MSVideoQualityController *obj,
                                              int bitrate,
                                              float bitrate_threshold,
                                              bool_t update_only_fps) {
	MSVideoConfiguration *vconf_list = NULL;
	MSVideoConfiguration current_vconf, best_vconf;
	int available_bandwidth_after_fec = 0;
	ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, &vconf_list);
	if (vconf_list == NULL) return;
	ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);

	if (!update_only_fps) {
		int current_bitrate = media_stream_get_target_network_bitrate(&obj->stream->ms);
		int bitrate_limit = (int)(bitrate / bitrate_threshold);
		if (bitrate_limit >= current_bitrate) {
			best_vconf = ms_video_quality_controller_find_best_vconf(obj, vconf_list, bitrate_limit);
			available_bandwidth_after_fec = update_fec_params_from_bitrate(obj, &current_vconf, bitrate_limit);
			ms_video_quality_controller_update_video_definition(obj, &current_vconf, &best_vconf);
			return;
		} else {
			ms_message("MSVideoQualityController [%p]: new bitrate not sufficient to try a video resolution upgrade.",
			           obj);
		}
	}
	best_vconf = ms_video_quality_controller_find_best_vconf(obj, vconf_list, bitrate);
	available_bandwidth_after_fec = update_fec_params_from_bitrate(obj, &current_vconf, bitrate);
	ms_video_quality_controller_update_fps_only(obj, &current_vconf, &best_vconf);
	ms_video_quality_controller_update_encoder_bitrate(obj, available_bandwidth_after_fec, &current_vconf, &best_vconf);
}

void ms_video_quality_controller_process_timer(MSVideoQualityController *obj) {
	if (obj->increase_timer_running) {
		time_t current_time = ms_time(NULL);
		if (current_time - obj->increase_timer_start >= INCREASE_TIMER_DELAY) {
			ms_message("MSVideoQualityController [%p]: No further TMMBR (%f kbit/s) received after %d seconds, "
			           "increasing video quality...",
			           obj->stream, obj->last_tmmbr * 1e-3, INCREASE_TIMER_DELAY);

			update_video_quality_from_bitrate(obj, obj->last_tmmbr, INCREASE_BITRATE_THRESHOLD, FALSE);

			obj->increase_timer_running = FALSE;
		}
	}
}

void ms_video_quality_controller_update_from_tmmbr(MSVideoQualityController *obj, int tmmbr) {

	const char *preset = NULL;
	preset = video_stream_get_video_preset((VideoStream *)obj->stream);

	if (preset && strcmp(preset, "custom") == 0) {
		MSVideoConfiguration *vconf_list = NULL;
		MSVideoConfiguration current_vconf, best_vconf;
		ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, &vconf_list);
		if (vconf_list == NULL) return;
		ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);

		best_vconf = ms_video_quality_controller_find_best_vconf(obj, vconf_list, tmmbr);
		ms_video_quality_controller_update_encoder_bitrate(obj, tmmbr, &current_vconf, &best_vconf);
		return;
	}

	if (obj->last_tmmbr == -1) {
		MSVideoConfiguration current_vconf;

		ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);

		if (tmmbr < current_vconf.required_bitrate) {
			ms_message("MSVideoQualityController [%p]: First TMMBR (%f kbit/s) inferior to preferred video size "
			           "required bitrate, reducing video quality...",
			           obj, tmmbr * 1e-3);

			update_video_quality_from_bitrate(obj, tmmbr, 1.0f, FALSE);
			obj->last_tmmbr = tmmbr;
			return;
		}
	}

	if (tmmbr > obj->last_tmmbr) {
		obj->increase_timer_start = ms_time(NULL);
		if (!obj->increase_timer_running) obj->increase_timer_running = TRUE;
		update_video_quality_from_bitrate(obj, tmmbr, 1.0f, TRUE);

	} else if (tmmbr < obj->last_tmmbr) {
		if (obj->increase_timer_running) obj->increase_timer_running = FALSE;

		ms_message(
		    "MSVideoQualityController [%p]: tmmbr is lower than previous one (%f kbit/s), reducing video quality...",
		    obj, tmmbr * 1e-3);
		update_video_quality_from_bitrate(obj, tmmbr, 1.0f, FALSE);
	}

	obj->last_tmmbr = tmmbr;
}
