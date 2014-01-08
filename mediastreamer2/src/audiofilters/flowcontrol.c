/*
 * flowcontrol.c - routines to silently discard samples in excess (used by AEC implementation)
 *
 * Copyright (C) 2009-2012  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "mediastreamer2/mscommon.h"
#include "flowcontrol.h"

void audio_flow_controller_init(AudioFlowController *ctl)
{
	ctl->target_samples = 0;
	ctl->total_samples = 0;
	ctl->current_pos = 0;
	ctl->current_dropped = 0;
}

void audio_flow_controller_set_target(AudioFlowController *ctl, int samples_to_drop, int total_samples)
{
	ctl->target_samples = samples_to_drop;
	ctl->total_samples = total_samples;
	ctl->current_pos = 0;
	ctl->current_dropped = 0;
}

static void discard_well_choosed_samples(mblk_t *m, int nsamples, int todrop)
{
	int i;
	int16_t *samples = (int16_t *) m->b_rptr;
	int min_diff = 32768;
	int pos = 0;


#ifdef TWO_SAMPLES_CRITERIA
	for (i = 0; i < nsamples - 1; ++i) {
		int tmp = abs((int) samples[i] - (int) samples[i + 1]);
#else
	for (i = 0; i < nsamples - 2; ++i) {
		int tmp = abs((int) samples[i] - (int) samples[i + 1]) + abs((int) samples[i + 1] - (int) samples[i + 2]);
#endif
		if (tmp <= min_diff) {
			pos = i;
			min_diff = tmp;
		}
	}
	/*ms_message("min_diff=%i at pos %i",min_diff, pos);*/
#ifdef TWO_SAMPLES_CRITERIA
	memmove(samples + pos, samples + pos + 1, (nsamples - pos - 1) * 2);
#else
	memmove(samples + pos + 1, samples + pos + 2, (nsamples - pos - 2) * 2);
#endif

	todrop--;
	m->b_wptr -= 2;
	nsamples--;
	if (todrop > 0) {
		/*repeat the same process again*/
		discard_well_choosed_samples(m, nsamples, todrop);
	}
}

mblk_t *audio_flow_controller_process(AudioFlowController *ctl, mblk_t *m)
{
	if (ctl->total_samples > 0 && ctl->target_samples > 0) {
		int nsamples = (m->b_wptr - m->b_rptr) / 2;
		if (ctl->target_samples * 16 > ctl->total_samples) {
			ms_warning("Too many samples to drop, dropping entire frames");
			freemsg(m);
			ctl->current_pos += nsamples;
			m=NULL;
		} else {
			int th_dropped;
			int todrop;

			ctl->current_pos += nsamples;
			th_dropped = (ctl->target_samples * ctl->current_pos) / ctl->total_samples;
			todrop = th_dropped - ctl->current_dropped;
			if (todrop > 0) {
				if (todrop > nsamples) todrop = nsamples;
				discard_well_choosed_samples(m, nsamples, todrop);
				/*ms_message("th_dropped=%i, current_dropped=%i, %i samples dropped.",th_dropped,ctl->current_dropped,todrop);*/
				ctl->current_dropped += todrop;
			}
		}
		if (ctl->current_pos >= ctl->total_samples) ctl->target_samples = 0; /*stop discarding*/
	}
	return m;
}
