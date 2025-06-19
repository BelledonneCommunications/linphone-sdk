/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
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

#include <bctoolbox/defs.h>

#include "mediastreamer2/msasync.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"

static void enc_init(BCTBX_UNUSED(MSFilter *f)) {
	ms_message("init dummy video encoder");
}

static void enc_uninit(BCTBX_UNUSED(MSFilter *f)) {
	ms_message("uninit dummy video encoder");
}

static void enc_preprocess(BCTBX_UNUSED(MSFilter *f)) {
}

static void enc_process(MSFilter *f) {
	ms_queue_flush(f->inputs[0]);
}

static void enc_postprocess(BCTBX_UNUSED(MSFilter *f)) {
}

static int enc_get_configuration(BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(void *data)) {
	ms_warning("Dummy encoder does not returns configuration");
	return 0;
}

static int enc_set_configuration(BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(void *data)) {
	ms_warning("Dummy encoder does not accept configuration");
	return 0;
}
static int enc_enable_avpf(BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(void *data)) {
	ms_warning("Dummy encoder does not manage AVPF");
	return 0;
}

static MSFilterMethod enc_methods[] = {{MS_VIDEO_ENCODER_GET_CONFIGURATION, enc_get_configuration},
                                       {MS_VIDEO_ENCODER_SET_CONFIGURATION, enc_set_configuration},
                                       {MS_VIDEO_ENCODER_ENABLE_AVPF, enc_enable_avpf},
                                       {0, NULL}};

#define MS_DUMMY_ENC_NAME "MSDummyEnc"
#define MS_DUMMY_ENC_DESCRIPTION "A dummy encoder doing nothing"
#define MS_DUMMY_ENC_CATEGORY MS_FILTER_ENCODER
#define MS_DUMMY_ENC_ENC_FMT "DUMMY"
#define MS_DUMMY_ENC_NINPUTS 1
#define MS_DUMMY_ENC_NOUTPUTS 1
#define MS_DUMMY_ENC_FLAGS MS_FILTER_IS_PUMP

#ifdef _MSC_VER

MSFilterDesc ms_dummy_enc_desc = {
    MS_DUMMY_ENC_ID,      MS_DUMMY_ENC_NAME,    MS_DUMMY_ENC_DESCRIPTION, MS_DUMMY_ENC_CATEGORY,
    MS_DUMMY_ENC_ENC_FMT, MS_DUMMY_ENC_NINPUTS, MS_DUMMY_ENC_NOUTPUTS,    enc_init,
    enc_preprocess,       enc_process,          enc_postprocess,          enc_uninit,
    enc_methods,          MS_DUMMY_ENC_FLAGS};

#else

MSFilterDesc ms_dummy_enc_desc = {.id = MS_DUMMY_ENC_ID,
                                  .name = MS_DUMMY_ENC_NAME,
                                  .text = MS_DUMMY_ENC_DESCRIPTION,
                                  .category = MS_DUMMY_ENC_CATEGORY,
                                  .enc_fmt = MS_DUMMY_ENC_ENC_FMT,
                                  .ninputs = MS_DUMMY_ENC_NINPUTS,
                                  .noutputs = MS_DUMMY_ENC_NOUTPUTS,
                                  .init = enc_init,
                                  .preprocess = enc_preprocess,
                                  .process = enc_process,
                                  .postprocess = enc_postprocess,
                                  .uninit = enc_uninit,
                                  .methods = enc_methods,
                                  .flags = MS_DUMMY_ENC_FLAGS};

#endif

MS_FILTER_DESC_EXPORT(ms_dummy_enc_desc)

static void dec_init(BCTBX_UNUSED(MSFilter *f)) {
	ms_message("init dummy video decoder");
}

static void dec_preprocess(BCTBX_UNUSED(MSFilter *f)) {
	ms_message("preprocess dummy video decoder");
}

static void dec_uninit(BCTBX_UNUSED(MSFilter *f)) {
}

static void dec_process(MSFilter *f) {
	ms_queue_flush(f->inputs[0]);
}

static void dec_postprocess(BCTBX_UNUSED(MSFilter *f)) {
}

static int dec_enable_avpf(BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(void *data)) {
	ms_warning("Dummy decoder does not manage AVPF");
	return 0;
}

static int dec_freeze_on_error(BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(void *data)) {
	ms_warning("Dummy decoder does not manage freeze on error");
	return 0;
}
static MSFilterMethod dec_methods[] = {{MS_VIDEO_DECODER_ENABLE_AVPF, dec_enable_avpf},
                                       {MS_VIDEO_DECODER_FREEZE_ON_ERROR, dec_freeze_on_error},
                                       {0, NULL}};

#define MS_DUMMY_DEC_NAME "MSDummyDec"
#define MS_DUMMY_DEC_DESCRIPTION "A dummy decoder doing nothing"
#define MS_DUMMY_DEC_CATEGORY MS_FILTER_DECODER
#define MS_DUMMY_DEC_ENC_FMT "DUMMY"
#define MS_DUMMY_DEC_NINPUTS 1
#define MS_DUMMY_DEC_NOUTPUTS 1
#define MS_DUMMY_DEC_FLAGS MS_FILTER_IS_PUMP

#ifdef _MSC_VER

MSFilterDesc ms_dummy_dec_desc = {
    MS_DUMMY_DEC_ID,      MS_DUMMY_DEC_NAME,    MS_DUMMY_DEC_DESCRIPTION, MS_DUMMY_DEC_CATEGORY,
    MS_DUMMY_DEC_ENC_FMT, MS_DUMMY_DEC_NINPUTS, MS_DUMMY_DEC_NOUTPUTS,    dec_init,
    dec_preprocess,       dec_process,          dec_postprocess,          dec_uninit,
    dec_methods,          MS_DUMMY_DEC_FLAGS};

#else

MSFilterDesc ms_dummy_dec_desc = {.id = MS_DUMMY_DEC_ID,
                                  .name = MS_DUMMY_DEC_NAME,
                                  .text = MS_DUMMY_DEC_DESCRIPTION,
                                  .category = MS_DUMMY_DEC_CATEGORY,
                                  .enc_fmt = MS_DUMMY_DEC_ENC_FMT,
                                  .ninputs = MS_DUMMY_DEC_NINPUTS,
                                  .noutputs = MS_DUMMY_DEC_NOUTPUTS,
                                  .init = dec_init,
                                  .preprocess = dec_preprocess,
                                  .process = dec_process,
                                  .postprocess = dec_postprocess,
                                  .uninit = dec_uninit,
                                  .methods = dec_methods,
                                  .flags = MS_DUMMY_DEC_FLAGS};

#endif

MS_FILTER_DESC_EXPORT(ms_dummy_dec_desc)
