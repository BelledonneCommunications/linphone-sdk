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

#include "bctoolbox/list.h"
#include "mediastreamer2/mscommon.h"
#include <bctoolbox/defs.h>
#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msnoisesuppressor.h"
#include <rnnoise.h>

#define FRAME_SIZE 480

typedef struct _NoiseSuppressorData {
	MSBufferizer *bz;
	uint32_t sample_rate_Hz;
	int nchannels;
	uint32_t frame_size_bytes;
	bool_t bypass_mode;
	DenoiseState *denoise_state;
} NoiseSuppressorData;

static NoiseSuppressorData *noise_suppressor_data_new(void) {
	NoiseSuppressorData *obj = ms_new0(NoiseSuppressorData, 1);
	obj->bz = ms_bufferizer_new();
	obj->nchannels = 1;
	obj->sample_rate_Hz = 48000;
	obj->frame_size_bytes = FRAME_SIZE * sizeof(int16_t);
	obj->bypass_mode = FALSE;
	obj->denoise_state = NULL;
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
	ms_message("NoiseSuppressor [%p] initialized for %d Hz, %d channel, frames %d ms", f, data->sample_rate_Hz,
	           data->nchannels, frame_size_ms);
}

static void noise_suppressor_uninit(MSFilter *f) {
	noise_suppressor_data_destroy((NoiseSuppressorData *)f->data);
}

static void noise_suppressor_preprocess(BCTBX_UNUSED(MSFilter *f)) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	data->denoise_state = rnnoise_create(NULL);
	if (data->denoise_state == NULL) {
		ms_error("NoiseSuppressor [%p]: no model found, enable bypass mode", f);
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
	int16_t noisy_audio[FRAME_SIZE] = {0};
	while (ms_bufferizer_read(data->bz, (uint8_t *)noisy_audio, (size_t)data->frame_size_bytes) >=
	       (size_t)data->frame_size_bytes) {
		float x[FRAME_SIZE];
		for (int i = 0; i < FRAME_SIZE; i++) {
			x[i] = (float)noisy_audio[i];
		}
		rnnoise_process_frame(data->denoise_state, x, x);
		int16_t clean_audio[FRAME_SIZE] = {0};
		for (int i = 0; i < FRAME_SIZE; i++) {
			clean_audio[i] = (int16_t)x[i];
		}
		mblk_t *m_clean = allocb((size_t)data->frame_size_bytes, 0);
		memcpy(m_clean->b_wptr, clean_audio, data->frame_size_bytes);
		m_clean->b_wptr += data->frame_size_bytes;
		ms_queue_put(f->outputs[0], m_clean);
	}
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

static int noise_suppressor_get_nchannels(MSFilter *f, void *arg) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	*((int *)arg) = data->nchannels;
	return 0;
}

static int noise_suppressor_set_bypass_mode(MSFilter *f, void *arg) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	bool_t entering_bypass = *(bool_t *)arg;
	if (data->bypass_mode && !entering_bypass) {
		ms_message("NoiseSuppressor [%p] is leaving bypass mode", f);
	} else if (!data->bypass_mode && entering_bypass) {
		ms_message("NoiseSuppressor [%p] is entering bypass mode", f);
	}
	data->bypass_mode = entering_bypass;
	return 0;
}

static int noise_suppressor_get_bypass_mode(MSFilter *f, void *arg) {
	NoiseSuppressorData *data = (NoiseSuppressorData *)f->data;
	*(bool_t *)arg = data->bypass_mode;
	return 0;
}

static MSFilterMethod methods[] = {{MS_FILTER_GET_NCHANNELS, noise_suppressor_get_nchannels},
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