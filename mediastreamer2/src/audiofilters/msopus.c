/*
msopus.c - Opus encoder/decoder for Linphone

mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011-2012 Belledonne Communications, Grenoble, France

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

#include <opus/opus.h>


/* Define codec specific settings */
#define MAX_BYTES_PER_FRAME     250 // Equals peak bitrate of 100 kbps
#define MAX_INPUT_FRAMES        5


/**
 * Definition of the private data structure of the opus encoder.
 */
typedef struct _OpusEncData {
	OpusEncoder *state;
	MSBufferizer *bufferizer;
	uint32_t ts;
	int samplerate;
	int channels;
	int application;
	int vbr;
	int inbandfec;
	int max_network_bitrate;
	int bitrate;
	int ptime;
} OpusEncData;


/******************************************************************************
 * Methods to (de)initialize and run the opus encoder                         *
 *****************************************************************************/

static void ms_opus_enc_init(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)ms_new(OpusEncData, 1);
	d->bufferizer = ms_bufferizer_new();
	d->state = NULL;
	d->ts = 0;
	d->samplerate = 8000;
	d->channels = 1;
	d->application = OPUS_APPLICATION_VOIP;
	d->vbr = 1;
	d->inbandfec = 1;
	d->bitrate = -1;
	d->ptime = 20;
	f->data = d;
}

static void ms_opus_enc_preprocess(MSFilter *f) {
	int error;
	OpusEncData *d = (OpusEncData *)f->data;
	d->state = opus_encoder_create(d->samplerate, d->channels, d->application, &error);
	if (error != OPUS_OK) {
		ms_error("Opus encoder creation failed: %s", opus_strerror(error));
		return;
	}
	error = opus_encoder_ctl(d->state, OPUS_SET_VBR(d->vbr));
	if (error != OPUS_OK) {
		ms_error("Could not set vbr mode to opus encoder: %s", opus_strerror(error));
	}
	error = opus_encoder_ctl(d->state, OPUS_SET_INBAND_FEC(d->inbandfec));
	if (error != OPUS_OK) {
		ms_error("could not set inband fec to opus encoder: %s", opus_strerror(error));
	}
}

static void ms_opus_enc_process(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)f->data;
	mblk_t *im;
	mblk_t *om = NULL;
	uint8_t *buff = NULL;
	opus_int32 ret;
	opus_int32 nbytes;
	int packet_size = d->samplerate * d->ptime / 1000; /* in samples */

	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		ms_bufferizer_put(d->bufferizer, im);
	}
	while (ms_bufferizer_get_avail(d->bufferizer) >= (packet_size * 2)) {
		/* max payload size */
		nbytes = MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES;
		om = allocb(nbytes, 0);
		if (!buff) buff = ms_malloc(packet_size * 2);
		ms_bufferizer_read(d->bufferizer, buff, packet_size * 2);
		ret = opus_encode(d->state, (opus_int16 *)buff, packet_size, om->b_wptr, nbytes);
		if (ret < 0) {
			ms_error("Opus encoder error: %s", opus_strerror(ret));
			freeb(om);
		}
		if (ret > 0) {
			d->ts += packet_size;
			om->b_wptr += nbytes;
			mblk_set_timestamp_info(om, d->ts);
			ms_queue_put(f->outputs[0], om);
			om = NULL;
		}
	}
	if (buff != NULL) {
		ms_free(buff);
	}
}

static void ms_opus_enc_postprocess(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)f->data;
	opus_encoder_destroy(d->state);
	d->state = NULL;
}

static void ms_opus_enc_uninit(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)f->data;
	if (d == NULL) return;
	if (d->state) {
		opus_encoder_destroy(d->state);
		d->state = NULL;
		ms_bufferizer_destroy(d->bufferizer);
		d->bufferizer = NULL;
	}
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the opus encoder                                      *
 *****************************************************************************/

static void apply_max_bitrate(OpusEncData *d) {
	int inital_cbr = 0;
	int normalized_cbr = 0;
	int pps = 1000 / d->ptime;

	normalized_cbr = inital_cbr = (int)(((((float)d->max_network_bitrate) / (pps * 8)) - 20 - 12 -8) * pps * 8);
	switch (d->samplerate) {
		case 8000:
			normalized_cbr = MIN(normalized_cbr, 20000);
			normalized_cbr = MAX(normalized_cbr, 5000);
			break;
		case 12000:
			normalized_cbr = MIN(normalized_cbr, 25000);
			normalized_cbr = MAX(normalized_cbr, 7000);
			break;
		case 16000:
			normalized_cbr = MIN(normalized_cbr, 32000);
			normalized_cbr = MAX(normalized_cbr, 8000);
			break;
		case 24000:
			normalized_cbr = MIN(normalized_cbr, 40000);
			normalized_cbr = MAX(normalized_cbr, 20000);
			break;
	}
	if (normalized_cbr != inital_cbr) {
		ms_warning("Opus encoder doesn't support codec bitrate [%i], normalizing", inital_cbr);
	}
	d->bitrate = normalized_cbr;
	ms_message("Setting opus codec birate to [%i] from network bitrate [%i] with ptime [%i]", d->bitrate, d->max_network_bitrate, d->ptime);
}

static int ms_opus_enc_set_sample_rate(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	d->samplerate = *((int *)arg);
	return 0;
}

static int ms_opus_enc_get_sample_rate(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	*((int *)arg) = d->samplerate;
	return 0;
}

static int ms_opus_enc_set_bitrate(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	d->max_network_bitrate = *((int *)arg);
	if (d->state) apply_max_bitrate(d);
	return 0;
}

static int ms_opus_enc_get_bitrate(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	*((int *)arg) = d->max_network_bitrate;
	return 0;
}

static int ms_opus_enc_add_fmtp(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	const char *fmtp = (const char *)arg;
	char buf[32];

	memset(buf, '\0', sizeof(buf));
	if (fmtp_get_value(fmtp, "app", buf, sizeof(buf))) {
		if (strcmp(buf, "voip") == 0) d->application = OPUS_APPLICATION_VOIP;
		else if (strcmp(buf, "audio") == 0) d->application = OPUS_APPLICATION_AUDIO;
		else if (strcmp(buf, "lowdelay") == 0) d->application = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
	}

	memset(buf, '\0', sizeof(buf));
	if (fmtp_get_value(fmtp, "vbr", buf, sizeof(buf))) {
		if (strcmp(buf, "off") == 0) d->vbr = 0;
		else if (strcmp(buf, "on") == 0) d->vbr = 1;
	}

	memset(buf, '\0', sizeof(buf));
	if (fmtp_get_value(fmtp, "inbandfec", buf, sizeof(buf))) {
		if (strcmp(buf, "off") == 0) d->inbandfec = 0;
		else if (strcmp(buf, "on") == 0) d->inbandfec = 1;
	}

	return 0;
}

static MSFilterMethod ms_opus_enc_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_opus_enc_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_opus_enc_get_sample_rate	},
	{	MS_FILTER_SET_BITRATE,		ms_opus_enc_set_bitrate		},
	{	MS_FILTER_GET_BITRATE,		ms_opus_enc_get_bitrate		},
	{	MS_FILTER_ADD_FMTP,		ms_opus_enc_add_fmtp		},
	{	0,				NULL				}
};


/******************************************************************************
 * Definition of the opus encoder                                             *
 *****************************************************************************/

#define MS_OPUS_ENC_NAME	"MSOpusEnc"
#define MS_OPUS_ENC_DESCRIPTION	"An opus encoder."
#define MS_OPUS_ENC_CATEGORY	MS_FILTER_ENCODER
#define MS_OPUS_ENC_ENC_FMT	"opus"
#define MS_OPUS_ENC_NINPUTS	1
#define MS_OPUS_ENC_NOUTPUTS	1
#define MS_OPUS_ENC_FLAGS	0

#ifndef _MSC_VER

MSFilterDesc ms_opus_enc_desc = {
	.id = MS_OPUS_ENC_ID,
	.name = MS_OPUS_ENC_NAME,
	.text = MS_OPUS_ENC_DESCRIPTION,
	.category = MS_OPUS_ENC_CATEGORY,
	.enc_fmt = MS_OPUS_ENC_ENC_FMT,
	.ninputs = MS_OPUS_ENC_NINPUTS,
	.noutputs = MS_OPUS_ENC_NOUTPUTS,
	.init = ms_opus_enc_init,
	.preprocess = ms_opus_enc_preprocess,
	.process = ms_opus_enc_process,
	.postprocess = ms_opus_enc_postprocess,
	.uninit = ms_opus_enc_uninit,
	.methods = ms_opus_enc_methods,
	.flags = MS_OPUS_ENC_FLAGS
};

#else

MSFilterDesc ms_opus_enc_desc = {
	MS_OPUS_ENC_ID,
	MS_OPUS_ENC_NAME,
	MS_OPUS_ENC_DESCRIPTION,
	MS_OPUS_ENC_CATEGORY,
	MS_OPUS_ENC_ENC_FMT,
	MS_OPUS_ENC_NINPUTS,
	MS_OPUS_ENC_NOUTPUTS,
	ms_opus_enc_init,
	ms_opus_enc_preprocess,
	ms_opus_enc_process,
	ms_opus_enc_postprocess,
	ms_opus_enc_uninit,
	ms_opus_enc_methods,
	MS_OPUS_ENC_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_opus_enc_desc)




/**
 * Definition of the private data structure of the opus decoder.
 */
typedef struct _OpusDecData {
	OpusDecoder *state;
	int samplerate;
	int channels;
	int frame_size;
	bool_t plc;
} OpusDecData;


/******************************************************************************
 * Methods to (de)initialize and run the opus decoder                         *
 *****************************************************************************/

static void ms_opus_dec_init(MSFilter *f) {
	OpusDecData *d = (OpusDecData *)ms_new(OpusDecData, 1);
	d->state = NULL;
	d->samplerate = 8000;
	d->channels = 1;
	d->frame_size = 20 * d->samplerate;
	f->data = d;
}

static void ms_opus_dec_preprocess(MSFilter *f) {
	int error;
	OpusDecData *d = (OpusDecData *)f->data;
	d->state = opus_decoder_create(d->samplerate, d->channels, &error);
	if (error != OPUS_OK) {
		ms_error("Opus decoder creation failed: %s", opus_strerror(error));
	}
}

static void ms_opus_dec_process(MSFilter *f) {
	OpusDecData *d = (OpusDecData *)f->data;
	mblk_t *im;
	mblk_t *om;
	int bytes = d->frame_size * d->channels * 2;
	int frames;

	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		om = allocb(bytes, 0);
		frames = opus_decode(d->state, (const unsigned char *)im->b_rptr, im->b_wptr - im->b_rptr, (opus_int16 *)om->b_wptr, d->frame_size, 0);
		if (frames < 0) {
			ms_warning("Opus decoder error: %s", opus_strerror(frames));
			freemsg(om);
		} else {
			om->b_wptr += frames * d->channels * 2;
			ms_queue_put(f->outputs[0], om);
		}
		freemsg(im);
	}
}

static void ms_opus_dec_postprocess(MSFilter *f) {
	OpusDecData *d = (OpusDecData *)f->data;
	opus_decoder_destroy(d->state);
	d->state = NULL;
}

static void ms_opus_dec_uninit(MSFilter *f) {
	OpusDecData *d = (OpusDecData *)f->data;
	if (d == NULL) return;
	if (d->state) {
		opus_decoder_destroy(d->state);
		d->state = NULL;
	}
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the opus decoder                                      *
 *****************************************************************************/

static int ms_opus_dec_set_sample_rate(MSFilter *f, void *arg) {
	OpusDecData *d = (OpusDecData *)f->data;
	d->samplerate = *((int *)arg);
	d->frame_size = 20 * d->samplerate;
	return 0;
}

static int ms_opus_dec_get_sample_rate(MSFilter *f, void *arg) {
	OpusDecData *d = (OpusDecData *)f->data;
	*((int *)arg) = d->samplerate;
	return 0;
}

static int ms_opus_dec_add_fmtp(MSFilter *f, void *arg) {
	OpusDecData *d = (OpusDecData *)f->data;
	const char *fmtp = (const char *)arg;
	char buf[32];

	memset(buf, '\0', sizeof(buf));
	if (fmtp_get_value(fmtp, "plc", buf, sizeof(buf))) {
		d->plc = atoi(buf);
	}
	return 0;
}

static int ms_opus_dec_have_plc(MSFilter *f, void *arg) {
	*((int *)arg) = 1;
	return 0;
}

static MSFilterMethod ms_opus_dec_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_opus_dec_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_opus_dec_get_sample_rate	},
	{	MS_FILTER_ADD_FMTP,		ms_opus_dec_add_fmtp		},
	{ 	MS_DECODER_HAVE_PLC,		ms_opus_dec_have_plc		},
	{	0,				NULL				}
};


/******************************************************************************
 * Definition of the opus decoder                                             *
 *****************************************************************************/

#define MS_OPUS_DEC_NAME	"MSOpusDec"
#define MS_OPUS_DEC_DESCRIPTION	"An opus decoder."
#define MS_OPUS_DEC_CATEGORY	MS_FILTER_DECODER
#define MS_OPUS_DEC_ENC_FMT	"opus"
#define MS_OPUS_DEC_NINPUTS	1
#define MS_OPUS_DEC_NOUTPUTS	1
#define MS_OPUS_DEC_FLAGS	0

#ifndef _MSC_VER

MSFilterDesc ms_opus_dec_desc = {
	.id = MS_OPUS_DEC_ID,
	.name = MS_OPUS_DEC_NAME,
	.text = MS_OPUS_DEC_DESCRIPTION,
	.category = MS_OPUS_DEC_CATEGORY,
	.enc_fmt = MS_OPUS_DEC_ENC_FMT,
	.ninputs = MS_OPUS_DEC_NINPUTS,
	.noutputs = MS_OPUS_DEC_NOUTPUTS,
	.init = ms_opus_dec_init,
	.preprocess = ms_opus_dec_preprocess,
	.process = ms_opus_dec_process,
	.postprocess = ms_opus_dec_postprocess,
	.uninit = ms_opus_dec_uninit,
	.methods = ms_opus_dec_methods,
	.flags = MS_OPUS_DEC_FLAGS
};

#else

MSFilterDesc ms_opus_dec_desc = {
	MS_OPUS_DEC_ID,
	MS_OPUS_DEC_NAME,
	MS_OPUS_DEC_DESCRIPTION,
	MS_OPUS_DEC_CATEGORY,
	MS_OPUS_DEC_ENC_FMT,
	MS_OPUS_DEC_NINPUTS,
	MS_OPUS_DEC_NOUTPUTS,
	ms_opus_dec_init,
	ms_opus_dec_preprocess,
	ms_opus_dec_process,
	ms_opus_dec_postprocess,
	ms_opus_dec_uninit,
	ms_opus_dec_methods,
	MS_OPUS_DEC_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_opus_dec_desc)
