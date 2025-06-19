/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#include "mediastreamer2/video-aggregator.h"

#include "ortp/rtp.h"

#define MS_VIDEO_AGGREGATOR_NINPUTS 10

typedef struct _VideoAggregatorData {
	int last_input_sent;
	uint32_t last_ssrc;
	uint32_t last_kf_timestamp[MS_VIDEO_AGGREGATOR_NINPUTS];
} VideoAggregatorData;

static void video_aggregator_init(MSFilter *f) {
	VideoAggregatorData *d = (VideoAggregatorData *)ms_new0(VideoAggregatorData, 1);
	d->last_input_sent = -1;
	d->last_ssrc = 0;
	memset(d->last_kf_timestamp, 0, sizeof(d->last_kf_timestamp));
	f->data = d;
}

static void video_aggregator_uninit(MSFilter *f) {
	ms_free(f->data);
}

// This filter assumes that we receive packets from one input at a time.
// We switch input only when we receive a keyframe from a new input.
// Only a few corner cases are treated while a new input receives data.
static void video_aggregator_process(MSFilter *f) {
	VideoAggregatorData *d = (VideoAggregatorData *)f->data;
	mblk_t *im;

	// If we have packets from the last input sent, output them immediately before checking all inputs.
	// This is to handle the case where we have received packets from multiple inputs at the same time.
	// Prioritize the current input, then make the switch if any other has a keyframe.
	if (d->last_input_sent != -1) {
		while ((im = ms_queue_get(f->inputs[d->last_input_sent])) != NULL) {
			if (mblk_get_independent_flag(im)) d->last_kf_timestamp[d->last_input_sent] = mblk_get_timestamp_info(im);

			ms_queue_put(f->outputs[0], im);
		}
	}

	// Check all inputs for a possible switch. Ignore last_input_sent since it was treated above.
	for (int i = 0; i < f->desc->ninputs; i++) {
		if (i == d->last_input_sent || f->inputs[i] == NULL) continue;

		while ((im = ms_queue_get(f->inputs[i])) != NULL) {
			// If we have a keyframe from a new input, we make sure this is a new keyframe.
			if (mblk_get_independent_flag(im) && mblk_get_timestamp_info(im) != d->last_kf_timestamp[i]) {
				d->last_input_sent = i;
				d->last_kf_timestamp[i] = mblk_get_timestamp_info(im);

				ms_filter_notify(f, MS_VIDEO_AGGREGATOR_INPUT_CHANGED, &i);

				ms_queue_put(f->outputs[0], im);
			} else {
				// If the packet is not a keyframe release it, we only switch at keyframes.
				// And if it was a keyframe, it was part of an old one from the previous switch.
				freemsg(im);
			}
		}
	}
}

#ifdef _MSC_VER

MSFilterDesc ms_video_aggregator_desc = {
    MS_VIDEO_AGGREGATOR_ID,
    "MSVideoAggregator",
    N_("A filter that reads from multiple video inputs and transmit to one output."),
    MS_FILTER_OTHER,
    NULL,
    MS_VIDEO_AGGREGATOR_NINPUTS,
    1,
    video_aggregator_init,
    NULL,
    video_aggregator_process,
    NULL,
    video_aggregator_uninit,
    NULL};

#else

MSFilterDesc ms_video_aggregator_desc = {
    .id = MS_VIDEO_AGGREGATOR_ID,
    .name = "MSVideoAggregator",
    .text = N_("A filter that reads from multiple video inputs and transmit to one output."),
    .category = MS_FILTER_OTHER,
    .ninputs = MS_VIDEO_AGGREGATOR_NINPUTS,
    .noutputs = 1,
    .init = video_aggregator_init,
    .process = video_aggregator_process,
    .uninit = video_aggregator_uninit,
    .methods = NULL};

#endif

MS_FILTER_DESC_EXPORT(ms_video_aggregator_desc)