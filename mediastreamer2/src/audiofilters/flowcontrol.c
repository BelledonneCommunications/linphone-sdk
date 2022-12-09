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

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/flowcontrol.h"

#include <math.h>

static void ms_audio_flow_controller_reset(MSAudioFlowController *ctl){
	ctl->target_samples = 0;
	ctl->total_samples = 0;
	ctl->current_pos = 0;
	ctl->current_dropped = 0;
}

void ms_audio_flow_controller_init(MSAudioFlowController *ctl){
	ctl->config.strategy = MSAudioFlowControlSoft;
	ctl->config.silent_threshold = 0.02f;
	ms_audio_flow_controller_reset(ctl);
}

void ms_audio_flow_controller_set_config(MSAudioFlowController *ctl, const MSAudioFlowControlConfig *cfg) {
	ctl->config = *cfg;
	ms_message("MSAudioFlowControl: configured with strategy=[%i] and silent_threshold=[%f].", cfg->strategy, cfg->silent_threshold);
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

static const float max_e = (32768* 0.7f);   /* 0.7 - is RMS factor **/

static float compute_frame_power(int16_t *samples, uint32_t nsamples){
	float acc = 0;
	uint32_t i;
	for (i = 0; i < nsamples; ++i){
		int sample = samples[i];
		acc += (float) (sample*sample);
	}
	return sqrtf(acc / (float)nsamples) / max_e;
}

mblk_t *ms_audio_flow_controller_process(MSAudioFlowController *ctl, mblk_t *m){
	if (ms_audio_flow_controller_running(ctl)) {
		uint32_t nsamples = (uint32_t)((m->b_wptr - m->b_rptr) / 2);
		uint32_t th_dropped;
		uint32_t todrop;

		ctl->current_pos += nsamples;
		if (ctl->config.strategy == MSAudioFlowControlBasic){
			if (ctl->current_dropped + nsamples <= ctl->target_samples){
				freemsg(m);
				ctl->current_dropped += nsamples;
				//ms_message("MSAudioFlowControl: dropped %i samples in basic strategy.", (int) nsamples);
				m = NULL;
			}
		}else{
			th_dropped = (uint32_t)(((uint64_t)ctl->target_samples * (uint64_t)ctl->current_pos) / (uint64_t)ctl->total_samples);
			todrop = (th_dropped > ctl->current_dropped) ? (th_dropped - ctl->current_dropped) : 0;
			if (todrop > 0) {
				if (nsamples <= ctl->target_samples && compute_frame_power((int16_t*)m->b_rptr, nsamples) < ctl->config.silent_threshold){
					/* This frame is almost silent, let's drop it entirely, it won't be noticeable.*/
					//ms_message("MSAudioFlowControl: dropping silent frame.");
					freemsg(m);
					m = NULL;
					todrop = nsamples;
				}else if ((todrop * 8) < nsamples) {
					/* eliminate samples at zero-crossing */
					discard_well_choosed_samples(m, nsamples, todrop);
				} else {
					ms_warning("MSAudioFlowControl: too many samples to drop, dropping entire frame.");
					freemsg(m);
					m = NULL;
					todrop = nsamples;
				}
				/*ms_message("th_dropped=%i, current_dropped=%i, %i samples dropped", th_dropped, ctl->current_dropped, todrop);*/
				ctl->current_dropped += todrop;
			}
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
	ms_audio_flow_controller_init(&s->afc);
	f->data = s;
}

static void ms_audio_flow_control_preprocess(MSFilter *f) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	ms_audio_flow_controller_reset(&s->afc);
}

static void ms_audio_flow_control_process(MSFilter *f) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	mblk_t *m;

	ms_filter_lock(f);
	while((m = ms_queue_get(f->inputs[0])) != NULL) {
		m = ms_audio_flow_controller_process(&s->afc, m);
		if (m) {
			ms_queue_put(f->outputs[0], m);
		}
	}
	ms_filter_unlock(f);
}

static void ms_audio_flow_control_postprocess(MSFilter *f) {

}

static void ms_audio_flow_control_uninit(MSFilter *f) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	ms_free(s);
}


static int ms_audio_flow_control_set_config(MSFilter *f, void *arg) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	ms_audio_flow_controller_set_config(&s->afc, (MSAudioFlowControlConfig*) arg);
	return 0;
}

static int ms_audio_flow_control_drop(MSFilter *f, void *arg) {
	MSAudioFlowControlState *s = (MSAudioFlowControlState *)f->data;
	MSAudioFlowControlDropEvent *ctl = (MSAudioFlowControlDropEvent *)arg;
	ms_filter_lock(f);
	if (!ms_audio_flow_controller_running(&s->afc)) {
		ms_message("MSAudioFlowControl: requested to drop %i ms ", (int)ctl->drop_ms);
		ms_audio_flow_controller_set_target(&s->afc,
			(ctl->drop_ms * s->samplerate * s->nchannels) / 1000,
			(ctl->flow_control_interval_ms * s->samplerate * s->nchannels) / 1000);
	}
	ms_filter_unlock(f);
	return 0;
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
	{ MS_AUDIO_FLOW_CONTROL_SET_CONFIG, ms_audio_flow_control_set_config },
	{ MS_AUDIO_FLOW_CONTROL_DROP, ms_audio_flow_control_drop },
	{ MS_FILTER_SET_SAMPLE_RATE, ms_audio_flow_control_set_sample_rate },
	{ MS_FILTER_GET_SAMPLE_RATE, ms_audio_flow_control_get_sample_rate },
	{ MS_FILTER_SET_NCHANNELS,   ms_audio_flow_control_set_nchannels   },
	{ MS_FILTER_GET_NCHANNELS,   ms_audio_flow_control_get_nchannels   },
	{ 0, NULL }
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
