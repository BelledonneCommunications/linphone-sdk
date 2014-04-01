/*
H.264 encoder/decoder plugin for mediastreamer2 based on the openh264 library.
Copyright (C) 2006-2012 Belledonne Communications, Grenoble

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msinterfaces.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/rfc3984.h"

#include "wels/codec_api.h"

#ifndef VERSION
#define VERSION "0.1.0"
#endif

/**
 * The goal of this small object is to tell when to send I frames at startup: at 2 and 4 seconds
 */
typedef struct VideoStarter {
	uint64_t next_time;
	int i_frame_count;
} VideoStarter;

/**
 * Definition of the private data structure of the decoder.
 */
typedef struct _MSV4L2H264DecData {
	Rfc3984Context unpacker;
	MSPicture outbuf;
	MSVideoSize vsize;
	uint64_t last_decoded_frame;
	uint64_t last_error_reported_time;
	mblk_t *yuv_msg;
	mblk_t *sps;
	mblk_t *pps;
	uint8_t *bitstream;
	int bitstream_size;
	unsigned int packet_num;
	bool_t first_image_decoded;
} MSV4L2H264DecData;

/**
 * Definition of the private data structure of the encoder.
 */
typedef struct _MSOpenH264EncData {
	VideoStarter starter;
	MSVideoSize vsize;
	MSPixFmt in_fmt;
	uint64_t framenum;
	Rfc3984Context *packer;
	float fps;
	int bitrate;
	int mode;
	bool_t generate_keyframe;
} MSOpenH264EncData;


/******************************************************************************
 * Implementation of the video starter                                        *
 *****************************************************************************/

#if 0
static void video_starter_init(VideoStarter *vs) {
	vs->next_time = 0;
	vs->i_frame_count = 0;
}

static void video_starter_first_frame(VideoStarter *vs, uint64_t curtime) {
	vs->next_time = curtime + 2000;
}

static bool_t video_starter_need_i_frame(VideoStarter *vs, uint64_t curtime) {
	if (vs->next_time == 0) return FALSE;
	if (curtime >= vs->next_time) {
		vs->i_frame_count++;
		if (vs->i_frame_count == 1) {
			vs->next_time += 2000;
		} else {
			vs->next_time = 0;
		}
		return TRUE;
	}
	return FALSE;
}
#endif


/******************************************************************************
 * Implementation of the decoder                                              *
 *****************************************************************************/

static void msopenh264_dec_init(MSFilter *f) {
	MSV4L2H264DecData *d = (MSV4L2H264DecData *)ms_new(MSV4L2H264DecData, 1);
	d->sps = NULL;
	d->pps = NULL;
	d->outbuf.w = 0;
	d->outbuf.h = 0;
	d->vsize.width = MS_VIDEO_SIZE_VGA_W;
	d->vsize.height = MS_VIDEO_SIZE_VGA_H;
	d->packet_num = 0;
	d->last_decoded_frame = 0;
	d->last_error_reported_time = 0;
	f->data = d;
}

static void msopenh264_dec_preprocess(MSFilter *f) {
	MSV4L2H264DecData *d = (MSV4L2H264DecData*)f->data;
	d->first_image_decoded = FALSE;
}

static void msopenh264_dec_process(MSFilter *f) {
	//MSV4L2H264DecData *d = (MSV4L2H264DecData*)f->data;
	// TODO
}

static void msopenh264_dec_uninit(MSFilter *f) {
	MSV4L2H264DecData *d = (MSV4L2H264DecData *)f->data;
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the decoder                                           *
 *****************************************************************************/

static int msopenh264_reset_first_image(MSFilter *f, void *data) {
	MSV4L2H264DecData *d = (MSV4L2H264DecData *)f->data;
	d->first_image_decoded = FALSE;
	return 0;
}

static MSFilterMethod msopenh264_dec_methods[] = {
	{ MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION, msopenh264_reset_first_image },
	{ 0,                                               NULL                         }
};

/******************************************************************************
 * Definition of the decoder                                                  *
 *****************************************************************************/

#define MSOPENH264_DEC_NAME        "MSOpenH264Dec"
#define MSOPENH264_DEC_DESCRIPTION "A H.264 decoder based on the openh264 library"
#define MSOPENH264_DEC_CATEGORY    MS_FILTER_DECODER
#define MSOPENH264_DEC_ENC_FMT     "H264"
#define MSOPENH264_DEC_NINPUTS     1
#define MSOPENH264_DEC_NOUTPUTS    1
#define MSOPENH264_DEC_FLAGS       0

#ifndef _MSC_VER

MSFilterDesc msopenh264_dec_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = MSOPENH264_DEC_NAME,
	.text = MSOPENH264_DEC_DESCRIPTION,
	.category = MSOPENH264_DEC_CATEGORY,
	.enc_fmt = MSOPENH264_DEC_ENC_FMT,
	.ninputs = MSOPENH264_DEC_NINPUTS,
	.noutputs = MSOPENH264_DEC_NOUTPUTS,
	.init = msopenh264_dec_init,
	.preprocess = msopenh264_dec_preprocess,
	.process = msopenh264_dec_process,
	.postprocess = NULL,
	.uninit = msopenh264_dec_uninit,
	.methods = msopenh264_dec_methods,
	.flags = MSOPENH264_DEC_FLAGS
};

#else

MSFilterDesc msopenh264_dec_desc = {
	MS_FILTER_PLUGIN_ID,
	MSOPENH264_DEC_NAME,
	MSOPENH264_DEC_DESCRIPTION,
	MSOPENH264_DEC_CATEGORY,
	MSOPENH264_DEC_ENC_FMT,
	MSOPENH264_DEC_NINPUTS,
	MSOPENH264_DEC_NOUTPUTS,
	msopenh264_dec_init,
	msopenh264_dec_preprocess,
	msopenh264_dec_process,
	NULL,
	msopenh264_dec_uninit,
	msopenh264_dec_methods,
	MSOPENH264_DEC_FLAGS
};

#endif


/******************************************************************************
 * Implementation of the encoder                                              *
 *****************************************************************************/

static void msopenh264_enc_init(MSFilter *f) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)ms_new(MSOpenH264EncData, 1);
	d->bitrate = 384000;
	d->fps = 30;
	d->mode = 1;
	d->packer = NULL;
	d->framenum = 0;
	d->generate_keyframe = FALSE;
	f->data = d;
}

static void msopenh264_enc_preprocess(MSFilter *f) {
	//MSOpenH264EncData *d = (MSOpenH264EncData*)f->data;
	// TODO
}

static void msopenh264_enc_process(MSFilter *f) {
	//MSOpenH264EncData *d = (MSOpenH264EncData*)f->data;
	// TODO
}

static void msopenh264_enc_postprocess(MSFilter *f) {
	//MSOpenH264EncData *d = (MSOpenH264EncData*)f->data;
	// TODO
}

static void msopenh264_enc_uninit(MSFilter *f) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)f->data;
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the encoder                                           *
 *****************************************************************************/

static int msopenh264_enc_set_fps(MSFilter *f, void *arg) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)f->data;
	d->fps = *(float*)arg;
	return 0;
}

static int msopenh264_enc_get_fps(MSFilter *f, void *arg) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)f->data;
	*(float*)arg = d->fps;
	return 0;
}

static int msopenh264_enc_set_bitrate(MSFilter *f, void *arg) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)f->data;
	d->bitrate = *(int*)arg;

	if (d->bitrate >= 1024000) {
		d->vsize.width = MS_VIDEO_SIZE_SVGA_W;
		d->vsize.height = MS_VIDEO_SIZE_SVGA_H;
		d->fps = 25;
	} else if (d->bitrate >= 512000) {
		d->vsize.width = MS_VIDEO_SIZE_VGA_W;
		d->vsize.height = MS_VIDEO_SIZE_VGA_H;
		d->fps = 25;
	} else if (d->bitrate >= 256000) {
		d->vsize.width = MS_VIDEO_SIZE_VGA_W;
		d->vsize.height = MS_VIDEO_SIZE_VGA_H;
		d->fps = 15;
	} else if (d->bitrate >= 170000) {
		d->vsize.width = MS_VIDEO_SIZE_QVGA_W;
		d->vsize.height = MS_VIDEO_SIZE_QVGA_H;
		d->fps = 15;
	} else if (d->bitrate >= 128000) {
		d->vsize.width = MS_VIDEO_SIZE_QCIF_W;
		d->vsize.height = MS_VIDEO_SIZE_QCIF_H;
		d->fps = 10;
	} else if (d->bitrate >= 64000) {
		d->vsize.width = MS_VIDEO_SIZE_QCIF_W;
		d->vsize.height = MS_VIDEO_SIZE_QCIF_H;
		d->fps = 7;
	} else {
		d->vsize.width = MS_VIDEO_SIZE_QCIF_W;
		d->vsize.height = MS_VIDEO_SIZE_QCIF_H;
		d->fps = 5;
	}

	ms_message("bitrate requested: %d (%d x %d)", d->bitrate, d->vsize.width, d->vsize.height);
	return 0;
}

static int msopenh264_enc_get_bitrate(MSFilter *f, void*arg) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)f->data;
	*(int*)arg = d->bitrate;
	return 0;
}

static int msopenh264_enc_set_vsize(MSFilter *f, void *arg) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)f->data;
	d->vsize = *(MSVideoSize*)arg;
	return 0;
}

static int msopenh264_enc_get_vsize(MSFilter *f, void *arg) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)f->data;
	*(MSVideoSize*)arg = d->vsize;
	return 0;
}

static int msopenh264_enc_set_pix_fmt(MSFilter *f, void *arg) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)f->data;
	MSPixFmt fmt = *(MSPixFmt *)arg;
	d->in_fmt = fmt;
	return 0;
}

static int msopenh264_enc_add_fmtp(MSFilter *f, void *arg) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)f->data;
	const char *fmtp = (const char *)arg;
	char value[12];
	if (fmtp_get_value(fmtp, "packetization-mode", value, sizeof(value))) {
		d->mode = atoi(value);
		ms_message("packetization-mode set to %i", d->mode);
	}
	return 0;
}

static int msopenh264_enc_has_builtin_converter(MSFilter *f, void *arg) {
	*((bool_t *)arg) = FALSE;
	return 0;
}

static int msopenh264_enc_req_vfu(MSFilter *f, void *arg) {
	MSOpenH264EncData *d = (MSOpenH264EncData *)f->data;
	d->generate_keyframe = TRUE;
	return 0;
}

static MSFilterMethod msopenh264_enc_methods[] = {
	{ MS_FILTER_SET_FPS,                      msopenh264_enc_set_fps               },
	{ MS_FILTER_GET_FPS,                      msopenh264_enc_get_fps               },
	{ MS_FILTER_SET_BITRATE,                  msopenh264_enc_set_bitrate           },
	{ MS_FILTER_GET_BITRATE,                  msopenh264_enc_get_bitrate           },
	{ MS_FILTER_SET_VIDEO_SIZE,               msopenh264_enc_set_vsize             },
	{ MS_FILTER_GET_VIDEO_SIZE,               msopenh264_enc_get_vsize             },
	{ MS_FILTER_SET_PIX_FMT,                  msopenh264_enc_set_pix_fmt           },
	{ MS_FILTER_ADD_FMTP,                     msopenh264_enc_add_fmtp              },
	{ MS_VIDEO_ENCODER_HAS_BUILTIN_CONVERTER, msopenh264_enc_has_builtin_converter },
	{ MS_FILTER_REQ_VFU,                      msopenh264_enc_req_vfu               },
#ifdef MS_VIDEO_ENCODER_REQ_VFU
	{ MS_VIDEO_ENCODER_REQ_VFU,               msopenh264_enc_req_vfu               },
#endif
	{ 0,                                      NULL                                 }
};

/******************************************************************************
 * Definition of the encoder                                                  *
 *****************************************************************************/

#define MSOPENH264_ENC_NAME        "MSOpenH264Enc"
#define MSOPENH264_ENC_DESCRIPTION "A H.264 encoder based on the openh264 library"
#define MSOPENH264_ENC_CATEGORY    MS_FILTER_ENCODER
#define MSOPENH264_ENC_ENC_FMT     "H264"
#define MSOPENH264_ENC_NINPUTS     1
#define MSOPENH264_ENC_NOUTPUTS    1
#define MSOPENH264_ENC_FLAGS       0

#ifndef _MSC_VER

MSFilterDesc msopenh264_enc_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = MSOPENH264_ENC_NAME,
	.text = MSOPENH264_ENC_DESCRIPTION,
	.category = MSOPENH264_ENC_CATEGORY,
	.enc_fmt = MSOPENH264_ENC_ENC_FMT,
	.ninputs = MSOPENH264_ENC_NINPUTS,
	.noutputs = MSOPENH264_ENC_NOUTPUTS,
	.init = msopenh264_enc_init,
	.preprocess = msopenh264_enc_preprocess,
	.process = msopenh264_enc_process,
	.postprocess = msopenh264_enc_postprocess,
	.uninit = msopenh264_enc_uninit,
	.methods = msopenh264_enc_methods,
	.flags = MSOPENH264_ENC_FLAGS
};

#else

MSFilterDesc msopenh264_enc_desc = {
	MS_FILTER_PLUGIN_ID,
	MSOPENH264_ENC_NAME,
	MSOPENH264_ENC_DESCRIPTION,
	MSOPENH264_ENC_CATEGORY,
	MSOPENH264_ENC_ENC_FMT,
	MSOPENH264_ENC_NINPUTS,
	MSOPENH264_ENC_NOUTPUTS,
	msopenh264_enc_init,
	msopenh264_enc_preprocess,
	msopenh264_enc_process,
	msopenh264_enc_postprocess,
	msopenh264_enc_uninit,
	msopenh264_enc_methods,
	MSOPENH264_ENC_FLAGS
};

#endif


MS2_PUBLIC void libmsopenh264_init(void){
	ms_filter_register(&msopenh264_dec_desc);
	ms_filter_register(&msopenh264_enc_desc);
	ms_message("msopenh264-" VERSION " plugin registered.");
}
