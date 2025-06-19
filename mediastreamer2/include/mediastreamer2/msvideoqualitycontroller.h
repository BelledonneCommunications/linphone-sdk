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

#ifndef msvideoqualitycontroller_h
#define msvideoqualitycontroller_h

#include "mediastreamer2/msvideo.h"
#include <ortp/ortp.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _VideoStream;

struct _MSVideoQualityController {
	struct _VideoStream *stream;
	int last_tmmbr;
	MSVideoSize last_vsize;

	time_t increase_timer_start;
	bool_t increase_timer_running;

	time_t increase_loss_rate_timer_start;
	bool_t increase_loss_rate_timer_running;
	int initial_bitrate;

	OrtpLossRateEstimator *smooth_loss_rate_estimator;
};

typedef struct _MSVideoQualityController MSVideoQualityController;

MS2_PUBLIC MSVideoQualityController *ms_video_quality_controller_new(struct _VideoStream *stream);
MS2_PUBLIC void ms_video_quality_controller_destroy(MSVideoQualityController *obj);
MS2_PUBLIC void ms_video_quality_controller_update_from_feedback(MSVideoQualityController *obj, const mblk_t *rtcp);
MS2_PUBLIC void ms_video_quality_controller_process_timers(MSVideoQualityController *obj);
MS2_PUBLIC void ms_video_quality_controller_update_from_tmmbr(MSVideoQualityController *obj, int tmmbr);

#ifdef __cplusplus
}
#endif

#endif /* msvideoqualitycontroller_h */
