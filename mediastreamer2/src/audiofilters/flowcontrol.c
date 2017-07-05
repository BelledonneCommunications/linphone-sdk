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
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/flowcontrol.h"


void ms_audio_flow_controller_init(MSAudioFlowController *ctl)
{
	ctl->target_samples = 0;
	ctl->total_samples = 0;
	ctl->current_pos = 0;
	ctl->current_dropped = 0;
}

void ms_audio_flow_controller_set_target(MSAudioFlowController *ctl, uint32_t samples_to_drop, uint32_t total_samples)
{
	ctl->target_samples = samples_to_drop;
	ctl->total_samples = total_samples;
	ctl->current_pos = 0;
	ctl->current_dropped = 0;
}

static void discard_well_choosed_samples(mblk_t *m, uint32_t nsamples, uint32_t todrop)
{
	uint32_t i;
	int16_t *samples = (int16_t *) m->b_rptr;
	int min_diff = 32768;
	uint32_t pos = 0;


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

static bool_t ms_audio_flow_controller_running(MSAudioFlowController *ctl) {
	return (ctl->total_samples > 0) && (ctl->target_samples > 0);
}

mblk_t *ms_audio_flow_controller_process(MSAudioFlowController *ctl, mblk_t *m){
	if (ms_audio_flow_controller_running(ctl)) {
		uint32_t nsamples = (uint32_t)((m->b_wptr - m->b_rptr) / 2);
		uint32_t th_dropped;
		uint32_t todrop;

		ctl->current_pos += nsamples;
		th_dropped = (uint32_t)(((uint64_t)ctl->target_samples * (uint64_t)ctl->current_pos) / (uint64_t)ctl->total_samples);
		todrop = (th_dropped > ctl->current_dropped) ? (th_dropped - ctl->current_dropped) : 0;
		if (todrop > 0) {
			if ((todrop * 8) < nsamples) {
				discard_well_choosed_samples(m, nsamples, todrop);
			} else {
				ms_warning("Too many samples to drop, dropping entire frame.");
				freemsg(m);
				m = NULL;
				todrop = nsamples;
			}
			/*ms_message("th_dropped=%i, current_dropped=%i, %i samples dropped", th_dropped, ctl->current_dropped, todrop);*/
			ctl->current_dropped += todrop;
		}
		if (ctl->current_pos >= ctl->total_samples) ctl->target_samples = 0; /*stop discarding*/
	}
	return m;
}




typedef struct MSAudioFlowControlState {
	MSAudioFlowController afc;
	int samplerate;
	int nchannels;
} MSAudioFlowControlState;


static void ms_audio_flow_control_init(MSFilter *f) {
	MSAudioFlowControlState *s = ms_new0(MSAudioFlowControlState, 1);
	f->data = s;
}

static void ms_audio_flow_control_preprocess(MSFilter *f) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	ms_audio_flow_controller_init(&s->afc);
}

static void ms_audio_flow_control_process(MSFilter *f) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	mblk_t *m;

	while((m = ms_queue_get(f->inputs[0])) != NULL) {
		m = ms_audio_flow_controller_process(&s->afc, m);
		if (m) {
			ms_queue_put(f->outputs[0], m);
		}
	}
}

static void ms_audio_flow_control_postprocess(MSFilter *f) {
	
}

static void ms_audio_flow_control_uninit(MSFilter *f) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	ms_free(s);
}


void ms_audio_flow_control_event_handler(void *user_data, MSFilter *source, unsigned int event, void *eventdata) {
	if (event == MS_AUDIO_FLOW_CONTROL_DROP_EVENT) {
		MSFilter *f = (MSFilter *)user_data;
		MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
		MSAudioFlowControlDropEvent *ev = (MSAudioFlowControlDropEvent *)eventdata;
		if (!ms_audio_flow_controller_running(&s->afc)) {
			ms_warning("Too much buffered audio signal, throwing out %u ms", ev->drop_ms);
			ms_audio_flow_controller_set_target(&s->afc,
				(ev->drop_ms * s->samplerate * s->nchannels) / 1000,
				(ev->flow_control_interval_ms * s->samplerate * s->nchannels) / 1000);
		}
	}
}


static int ms_audio_flow_control_set_sample_rate(MSFilter *f, void *arg) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	s->samplerate = *((int *)arg);
	return 0;
}

static int ms_audio_flow_control_get_sample_rate(MSFilter *f, void *arg) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	*((int *)arg) = s->samplerate;
	return 0;
}

static int ms_audio_flow_control_set_nchannels(MSFilter *f, void *arg) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	s->nchannels = *((int *)arg);
	return 0;
}

static int ms_audio_flow_control_get_nchannels(MSFilter *f, void *arg) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	*((int *)arg) = s->nchannels;
	return 0;
}

static MSFilterMethod ms_audio_flow_control_methods[] = {
	{ MS_FILTER_SET_SAMPLE_RATE, ms_audio_flow_control_set_sample_rate },
	{ MS_FILTER_GET_SAMPLE_RATE, ms_audio_flow_control_get_sample_rate },
	{ MS_FILTER_SET_NCHANNELS,   ms_audio_flow_control_set_nchannels   },
	{ MS_FILTER_GET_NCHANNELS,   ms_audio_flow_control_get_nchannels   }
};


#define MS_AUDIO_FLOW_CONTROL_NAME        "MSAudioFlowControl"
#define MS_AUDIO_FLOW_CONTROL_DESCRIPTION "Flow control filter to drop sample in the audio graph if too many samples are queued."
#define MS_AUDIO_FLOW_CONTROL_CATEGORY    MS_FILTER_OTHER
#define MS_AUDIO_FLOW_CONTROL_ENC_FMT     NULL
#define MS_AUDIO_FLOW_CONTROL_NINPUTS     1
#define MS_AUDIO_FLOW_CONTROL_NOUTPUTS    1
#define MS_AUDIO_FLOW_CONTROL_FLAGS       0

#ifdef _MSC_VER

MSFilterDesc ms_audio_flow_control_desc = {
	MS_AUDIO_FLOW_CONTROL_ID,
	MS_AUDIO_FLOW_CONTROL_NAME,
	MS_AUDIO_FLOW_CONTROL_DESCRIPTION,
	MS_AUDIO_FLOW_CONTROL_CATEGORY,
	MS_AUDIO_FLOW_CONTROL_ENC_FMT,
	MS_AUDIO_FLOW_CONTROL_NINPUTS,
	MS_AUDIO_FLOW_CONTROL_NOUTPUTS,
	ms_audio_flow_control_init,
	ms_audio_flow_control_preprocess,
	ms_audio_flow_control_process,
	ms_audio_flow_control_postprocess,
	ms_audio_flow_control_uninit,
	ms_audio_flow_control_methods,
	MS_AUDIO_FLOW_CONTROL_FLAGS
};

#else

MSFilterDesc ms_audio_flow_control_desc = {
	.id = MS_AUDIO_FLOW_CONTROL_ID,
	.name = MS_AUDIO_FLOW_CONTROL_NAME,
	.text = MS_AUDIO_FLOW_CONTROL_DESCRIPTION,
	.category = MS_AUDIO_FLOW_CONTROL_CATEGORY,
	.enc_fmt = MS_AUDIO_FLOW_CONTROL_ENC_FMT,
	.ninputs = MS_AUDIO_FLOW_CONTROL_NINPUTS,
	.noutputs = MS_AUDIO_FLOW_CONTROL_NOUTPUTS,
	.init = ms_audio_flow_control_init,
	.preprocess = ms_audio_flow_control_preprocess,
	.process = ms_audio_flow_control_process,
	.postprocess = ms_audio_flow_control_postprocess,
	.uninit = ms_audio_flow_control_uninit,
	.methods = ms_audio_flow_control_methods,
	.flags = MS_AUDIO_FLOW_CONTROL_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_audio_flow_control_desc)
