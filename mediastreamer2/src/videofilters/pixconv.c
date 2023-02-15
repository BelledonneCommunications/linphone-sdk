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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"

typedef struct PixConvState {
	YuvBuf outbuf;
	MSYuvBufAllocator *allocator;
	MSScalerContext *scaler;
	MSVideoSize size;
	MSPixFmt in_fmt;
	MSPixFmt out_fmt;
} PixConvState;

static void pixconv_init(MSFilter *f) {
	PixConvState *s = ms_new0(PixConvState, 1);
	s->allocator = ms_yuv_buf_allocator_new();
	s->size.width = MS_VIDEO_SIZE_CIF_W;
	s->size.height = MS_VIDEO_SIZE_CIF_H;
	s->in_fmt = MS_YUV420P;
	s->out_fmt = MS_YUV420P;
	s->scaler = NULL;
	f->data = s;
}

static void pixconv_uninit(MSFilter *f) {
	PixConvState *s = (PixConvState *)f->data;
	if (s->scaler != NULL) {
		ms_scaler_context_free(s->scaler);
		s->scaler = NULL;
	}
	ms_yuv_buf_allocator_free(s->allocator);
	ms_free(s);
}

static mblk_t *pixconv_alloc_mblk(PixConvState *s) {
	return ms_yuv_buf_allocator_get(s->allocator, &s->outbuf, s->size.width, s->size.height);
}

static void pixconv_process(MSFilter *f) {
	mblk_t *im, *om = NULL;
	PixConvState *s = (PixConvState *)f->data;

	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		uint32_t frame_ts = mblk_get_timestamp_info(im);
		if (s->in_fmt == s->out_fmt) {
			om = im;
		} else {
			MSPicture inbuf;
			if (ms_picture_init_from_mblk_with_size(&inbuf, im, s->in_fmt, s->size.width, s->size.height) == 0) {
				om = pixconv_alloc_mblk(s);
				if (s->scaler == NULL) {
					s->scaler = ms_scaler_create_context(inbuf.w, inbuf.h, s->in_fmt, inbuf.w, inbuf.h, s->out_fmt,
					                                     MS_SCALER_METHOD_BILINEAR);
				}
				if (s->in_fmt == MS_RGB24_REV) {
					inbuf.planes[0] += inbuf.strides[0] * (inbuf.h - 1);
					inbuf.strides[0] = -inbuf.strides[0];
				}
				if (ms_scaler_process(s->scaler, inbuf.planes, inbuf.strides, s->outbuf.planes, s->outbuf.strides) <
				    0) {
					ms_error("MSPixConv: Error in ms_sws_scale().");
				}
			}
			freemsg(im);
		}
		if (om != NULL) {
			mblk_set_timestamp_info(om, frame_ts);
			ms_queue_put(f->outputs[0], om);
		}
	}
}

static int pixconv_set_vsize(MSFilter *f, void *arg) {
	PixConvState *s = (PixConvState *)f->data;
	s->size = *(MSVideoSize *)arg;
	return 0;
}

static int pixconv_set_pixfmt(MSFilter *f, void *arg) {
	MSPixFmt fmt = *(MSPixFmt *)arg;
	PixConvState *s = (PixConvState *)f->data;
	s->in_fmt = fmt;
	return 0;
}

static MSFilterMethod methods[] = {
    {MS_FILTER_SET_VIDEO_SIZE, pixconv_set_vsize}, {MS_FILTER_SET_PIX_FMT, pixconv_set_pixfmt}, {0, NULL}};

#ifdef _MSC_VER

MSFilterDesc ms_pix_conv_desc = {MS_PIX_CONV_ID,
                                 "MSPixConv",
                                 N_("A pixel format converter"),
                                 MS_FILTER_OTHER,
                                 NULL,
                                 1,
                                 1,
                                 pixconv_init,
                                 NULL,
                                 pixconv_process,
                                 NULL,
                                 pixconv_uninit,
                                 methods};

#else

MSFilterDesc ms_pix_conv_desc = {.id = MS_PIX_CONV_ID,
                                 .name = "MSPixConv",
                                 .text = N_("A pixel format converter"),
                                 .category = MS_FILTER_OTHER,
                                 .ninputs = 1,
                                 .noutputs = 1,
                                 .init = pixconv_init,
                                 .process = pixconv_process,
                                 .uninit = pixconv_uninit,
                                 .methods = methods};

#endif

MS_FILTER_DESC_EXPORT(ms_pix_conv_desc)
