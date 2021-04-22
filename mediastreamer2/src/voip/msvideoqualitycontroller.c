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
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msvideoqualitycontroller.h"

#define INCREASE_TIMER_DELAY 10
#define INCREASE_BITRATE_THRESHOLD 1.3f

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

static void update_video_quality_from_bitrate(MSVideoQualityController *obj, int bitrate, float bitrate_threshold, bool_t update_only_fps) {
	MSVideoConfiguration *vconf_list = NULL;
	MSVideoConfiguration current_vconf, best_vconf, vconf;
	int new_bitrate_limit;

	ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, &vconf_list);

	if (vconf_list != NULL) {
		ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);

		if (!update_only_fps) {
			/* tmmbr >= required_bitrate * [Threshold] is the same as tmmbr / [Threshold] >= required_bitrate */
			best_vconf = ms_video_find_best_configuration_for_bitrate(vconf_list, (int) (bitrate / bitrate_threshold), ms_factory_get_cpu_count(obj->stream->ms.factory));

			if (!ms_video_size_equal(obj->last_vsize, best_vconf.vsize) && best_vconf.vsize.width * best_vconf.vsize.height != current_vconf.vsize.width * current_vconf.vsize.height) {
				ms_message("MSVideoQualityController [%p]: Changing video definition to %dx%d at %f fps", obj, best_vconf.vsize.width, best_vconf.vsize.height, best_vconf.fps);
				/* 
				 * Changing the resolution requires a call to video_stream_change_camera() so that the video graph is reconfigured.
				 */
				obj->stream->sent_vsize = best_vconf.vsize;
				obj->stream->preview_vsize = best_vconf.vsize;
				media_stream_set_target_network_bitrate(&obj->stream->ms, best_vconf.required_bitrate);
				video_stream_change_camera(obj->stream, obj->stream->cam);
				obj->last_vsize = best_vconf.vsize;
				return;
			}
		}

		vconf = ms_video_find_best_configuration_for_size_and_bitrate(vconf_list, current_vconf.vsize, ms_factory_get_cpu_count(obj->stream->ms.factory), bitrate);

		if (current_vconf.fps != vconf.fps) {
			ms_message("MSVideoQualityController [%p]: Bitrate update will change fps", obj);
			current_vconf.fps = vconf.fps;
			ms_filter_call_method(obj->stream->source, MS_FILTER_SET_FPS, &vconf.fps);
			obj->stream->configured_fps = vconf.fps;
		}

		new_bitrate_limit = bitrate < vconf.bitrate_limit ? bitrate : vconf.bitrate_limit;
		ms_message("MSVideoQualityController [%p]: Changing video encoder's output bitrate to %i", obj, new_bitrate_limit);
		current_vconf.required_bitrate = new_bitrate_limit;
		media_stream_set_target_network_bitrate(&obj->stream->ms, new_bitrate_limit);

		if (ms_filter_call_method(obj->stream->ms.encoder,MS_VIDEO_ENCODER_SET_CONFIGURATION, &current_vconf) != 0) {
			ms_warning("MSVideoQualityController [%p]: Failed to apply fps and bitrate constraint to %s", obj, obj->stream->ms.encoder->desc->name);
		}
	}
}

void ms_video_quality_controller_process_timer(MSVideoQualityController *obj) {
	if (obj->increase_timer_running) {
		time_t current_time = ms_time(NULL);
		if (current_time - obj->increase_timer_start >= INCREASE_TIMER_DELAY) {
			ms_message("MSVideoQualityController [%p]: No further TMMBR (%f kbit/s) received after %d seconds, increasing video quality...", obj->stream, obj->last_tmmbr*1e-3, INCREASE_TIMER_DELAY);
			
			update_video_quality_from_bitrate(obj, obj->last_tmmbr, INCREASE_BITRATE_THRESHOLD, FALSE);
			
			obj->increase_timer_running = FALSE;
		}
	}
}

void ms_video_quality_controller_update_from_tmmbr(MSVideoQualityController *obj, int tmmbr) {
	if (obj->last_tmmbr == -1) {
		MSVideoConfiguration current_vconf;

		ms_filter_call_method(obj->stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION, &current_vconf);

		if (tmmbr < current_vconf.required_bitrate) {
			ms_message("MSVideoQualityController [%p]: First TMMBR (%f kbit/s) inferior to preferred video size required bitrate, reducing video quality...", obj, tmmbr*1e-3);

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

		ms_message("MSVideoQualityController [%p]: Congestion detected (%f kbit/s), reducing video quality...", obj, tmmbr*1e-3);
		update_video_quality_from_bitrate(obj, tmmbr, 1.0f, FALSE);
	}

	obj->last_tmmbr = tmmbr;
}
