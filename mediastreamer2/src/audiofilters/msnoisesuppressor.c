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

#include "bctoolbox/defs.h"
#include "mediastreamer2/mscommon.h"
#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msnoisesuppressor.h"
#include "rnnoise.h"

#define FRAME_SIZE 480

typedef struct _NoiseSuppressorData {
	MSBufferizer *bz;
	uint32_t sample_rate_Hz;
	int in_nchannels;
	int out_nchannel;
	uint32_t frame_size_bytes;
	bool_t bypass_mode;
	DenoiseState *denoise_state;
	size_t in_nchannels_size;
	size_t nbytes;
} NoiseSuppressorData;

static void noise_suppression_update_size(NoiseSuppressorData *obj) {
	obj->in_nchannels_size = (size_t)obj->in_nchannels;
	obj->nbytes = (size_t)FRAME_SIZE * obj->in_nchannels_size * 2;
}

static NoiseSuppressorData *noise_suppressor_data_new(void) {
	NoiseSuppressorData *obj = ms_new0(NoiseSuppressorData, 1);
	obj->bz = ms_bufferizer_new();
	obj->in_nchannels = 1;
	obj->out_nchannel = 1;
	obj->sample_rate_Hz = 48000;
	obj->frame_size_bytes = FRAME_SIZE * sizeof(int16_t);
	obj->bypass_mode = FALSE;
	obj->denoise_state = NULL;
	noise_suppression_update_size(obj);
	return obj;
}

static void noise_suppressor_data_destroy(NoiseSuppressorData *obj) {
	ms_bufferizer_destroy(obj->bz);
	ms_free(obj);
}

static void noise_suppressor_init(MSFilter *f) {
	NoiseSuppressorData *data = noise_suppressor_data_new();
	f->data = data;
	int frame_size_ms = (int)((float)(FRAME_SIZE * 1000) / (float)data->sample_rate_Hz);
	noise_suppression_update_size(data);
	ms_message("MSNoiseSuppressor[%p] initialized for %d Hz, %d channel, frames %d ms", f, data->sample_rate_Hz,
	           data->out_nchannel, frame_size_ms);
}

static void noise_suppressor_uninit(MSFilter *f) {
	noise_suppressor_data_destroy((NoiseSuppressorData *)f->data);
}

static void noise_suppressor_preprocess(BCTBX_UNUSED(MSFilter *f)) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	data->denoise_state = rnnoise_create(NULL);
	if (data->denoise_state == NULL) {
		ms_error("MSNoiseSuppressor[%p]: no model found, enable bypass mode", f);
		data->bypass_mode = TRUE;
	}
}

static void noise_suppressor_process(BCTBX_UNUSED(MSFilter *f)) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	if (data->bypass_mode) {
		mblk_t *m;
		while ((m = ms_queue_get(f->inputs[0])) != NULL) {
			ms_queue_put(f->outputs[0], m);
		}
		return;
	}

	ms_bufferizer_put_from_queue(data->bz, f->inputs[0]);
	uint16_t *noisy_audio = ms_malloc0(data->nbytes);
	while (ms_bufferizer_read(data->bz, (uint8_t *)noisy_audio, data->nbytes) >= data->nbytes) {
		float x[FRAME_SIZE];
		for (int i = 0; i < FRAME_SIZE; i++) {
			x[i] = (float)((int16_t *)noisy_audio)[i * data->in_nchannels_size]; // only the first channel is
			                                                                     // processed and sent to output
		}
		rnnoise_process_frame(data->denoise_state, x, x);
		mblk_t *m_clean = allocb((size_t)data->frame_size_bytes, 0);
		for (int i = 0; i < FRAME_SIZE; i++) {
			((int16_t *)m_clean->b_wptr)[i] = (int16_t)x[i];
		}
		m_clean->b_wptr += data->frame_size_bytes;
		ms_queue_put(f->outputs[0], m_clean);
	}
	ms_free(noisy_audio);
}

static void noise_suppressor_postprocess(MSFilter *f) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	ms_bufferizer_flush(data->bz);
	rnnoise_destroy(data->denoise_state);
	data->denoise_state = NULL;
}

static int noise_suppressor_get_sample_rate(MSFilter *f, void *arg) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	*(int *)arg = data->sample_rate_Hz;
	return 0;
}

static int noise_suppressor_set_input_nchannels(MSFilter *f, void *arg) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	data->in_nchannels = *(int *)arg;
	noise_suppression_update_size(data);
	ms_message("MSNoiseSuppressor[%p]: set input channel number: %d", f, data->in_nchannels);
	return 0;
}

static int noise_suppressor_get_nchannels(MSFilter *f, void *arg) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	*((int *)arg) = data->out_nchannel;
	return 0;
}

static int noise_suppressor_set_bypass_mode(MSFilter *f, void *arg) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	bool_t entering_bypass = *(bool_t *)arg;
	if (data->bypass_mode && !entering_bypass) {
		ms_message("MSNoiseSuppressor[%p] is leaving bypass mode", f);
	} else if (!data->bypass_mode && entering_bypass) {
		ms_message("MSNoiseSuppressor[%p] is entering bypass mode", f);
	}
	data->bypass_mode = entering_bypass;
	return 0;
}

static int noise_suppressor_get_bypass_mode(MSFilter *f, void *arg) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	*(bool_t *)arg = data->bypass_mode;
	return 0;
}

static MSFilterMethod methods[] = {{MS_FILTER_SET_NCHANNELS, noise_suppressor_set_input_nchannels},
                                   {MS_FILTER_GET_NCHANNELS, noise_suppressor_get_nchannels},
                                   {MS_FILTER_GET_SAMPLE_RATE, noise_suppressor_get_sample_rate},
                                   {MS_NOISE_SUPPRESSOR_SET_BYPASS_MODE, noise_suppressor_set_bypass_mode},
                                   {MS_NOISE_SUPPRESSOR_GET_BYPASS_MODE, noise_suppressor_get_bypass_mode},
                                   {0, NULL}};

#ifdef _MSC_VER

MSFilterDesc ms_noise_suppressor_desc = {MS_NOISE_SUPPRESSOR_ID,
                                         "MSNoiseSuppressor",
                                         N_("Audio noise suppression filter based on RNNoise library"),
                                         MS_FILTER_OTHER,
                                         NULL,
                                         1,
                                         1,
                                         noise_suppressor_init,
                                         noise_suppressor_preprocess,
                                         noise_suppressor_process,
                                         noise_suppressor_postprocess,
                                         noise_suppressor_uninit,
                                         methods};

#else

MSFilterDesc ms_noise_suppressor_desc = {.id = MS_NOISE_SUPPRESSOR_ID,
                                         .name = "MSNoiseSuppressor",
                                         .text = N_("Audio noise suppression filter based on RNNoise library"),
                                         .category = MS_FILTER_OTHER,
                                         .ninputs = 1,
                                         .noutputs = 1,
                                         .init = noise_suppressor_init,
                                         .preprocess = noise_suppressor_preprocess,
                                         .process = noise_suppressor_process,
                                         .postprocess = noise_suppressor_postprocess,
                                         .uninit = noise_suppressor_uninit,
                                         .methods = methods};

#endif

MS_FILTER_DESC_EXPORT(ms_noise_suppressor_desc)