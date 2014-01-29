/*
 * qsa.c - Audio capture/playback filters using QSA (QNX Sound Architecture).
 *
 * Copyright (C) 2009-2014  Belledonne Communications, Grenoble, France
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

#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif

#include <sys/asoundlib.h>

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mssndcard.h"



/**
 * Definition of the private data structure of the QSA playback filter.
 */
typedef struct _MSQSAReadData {
	char some_value;
} MSQSAReadData;

/******************************************************************************
 * Methods to (de)initialize and run the QSA capture filter                   *
 *****************************************************************************/

static void ms_qsa_read_init(MSFilter *f) {
	MSQSAReadData *d = (MSQSAReadData *)ms_new(MSQSAReadData, 1);
	d->some_value = 42;
	f->data = d;
}

static void ms_qsa_read_preprocess(MSFilter *f) {
	/* TODO */
}

static void ms_qsa_read_process(MSFilter *f) {
	/* TODO */
}

static void ms_qsa_read_postprocess(MSFilter *f) {
	/* TODO */
}

static void ms_qsa_read_uninit(MSFilter *f) {
	MSQSAReadData *d = (MSQSAReadData *)f->data;
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the QSA capture filter                               *
 *****************************************************************************/

static int ms_qsa_read_set_sample_rate(MSFilter *f, void *arg) {
	/* TODO */
	return 0;
}

static int ms_qsa_read_get_sample_rate(MSFilter *f, void *arg) {
	/* TODO */
	return 0;
}

static int ms_qsa_read_set_nchannels(MSFilter *f, void *arg) {
	/* TODO */
	return 0;
}

static int ms_qsa_read_get_nchannels(MSFilter *f, void *arg) {
	/* TODO */
	return 0;
}

static MSFilterMethod ms_qsa_read_methods[] = {
	{ MS_FILTER_SET_SAMPLE_RATE, ms_qsa_read_set_sample_rate },
	{ MS_FILTER_GET_SAMPLE_RATE, ms_qsa_read_get_sample_rate },
	{ MS_FILTER_GET_NCHANNELS,   ms_qsa_read_get_nchannels   },
	{ MS_FILTER_SET_NCHANNELS,   ms_qsa_read_set_nchannels   },
	{ 0,                         NULL                         }
};


/******************************************************************************
 * Definition of the QSA capture filter                                      *
 *****************************************************************************/

#define MS_QSA_READ_NAME		"MSQSARead"
#define MS_QSA_READ_DESCRIPTION	"QSA sound capture."
#define MS_QSA_READ_CATEGORY	MS_FILTER_OTHER
#define MS_QSA_READ_ENC_FMT	NULL
#define MS_QSA_READ_NINPUTS	0
#define MS_QSA_READ_NOUTPUTS	1
#define MS_QSA_READ_FLAGS		0

#ifndef _MSC_VER

MSFilterDesc ms_qsa_read_desc = {
	.id = MS_QSA_READ_ID,
	.name = MS_QSA_READ_NAME,
	.text = MS_QSA_READ_DESCRIPTION,
	.category = MS_QSA_READ_CATEGORY,
	.enc_fmt = MS_QSA_READ_ENC_FMT,
	.ninputs = MS_QSA_READ_NINPUTS,
	.noutputs = MS_QSA_READ_NOUTPUTS,
	.init = ms_qsa_read_init,
	.preprocess = ms_qsa_read_preprocess,
	.process = ms_qsa_read_process,
	.postprocess = ms_qsa_read_postprocess,
	.uninit = ms_qsa_read_uninit,
	.methods = ms_qsa_read_methods,
	.flags = MS_QSA_READ_FLAGS
};

#else

MSFilterDesc ms_qsa_read_desc = {
	MS_QSA_READ_ID,
	MS_QSA_READ_NAME,
	MS_QSA_READ_DESCRIPTION,
	MS_QSA_READ_CATEGORY,
	MS_QSA_READ_ENC_FMT,
	MS_QSA_READ_NINPUTS,
	MS_QSA_READ_NOUTPUTS,
	ms_qsa_read_init,
	ms_qsa_read_preprocess,
	ms_qsa_read_process,
	ms_qsa_read_postprocess,
	ms_qsa_read_uninit,
	ms_qsa_read_methods,
	MS_QSA_READ_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_qsa_read_desc)



/**
 * Definition of the private data structure of the QSA playback filter.
 */
typedef struct _MSQSAWriteData {
	char some_value;
} MSQSAWriteData;

/******************************************************************************
 * Methods to (de)initialize and run the QSA playback filter                  *
 *****************************************************************************/

static void ms_qsa_write_init(MSFilter *f) {
	MSQSAWriteData *d = (MSQSAWriteData *)ms_new(MSQSAWriteData, 1);
	d->some_value = 42;
	f->data = d;
}

static void ms_qsa_write_preprocess(MSFilter *f) {
	/* TODO */
}

static void ms_qsa_write_process(MSFilter *f) {
	/* TODO */
}

static void ms_qsa_write_postprocess(MSFilter *f) {
	/* TODO */
}

static void ms_qsa_write_uninit(MSFilter *f) {
	MSQSAWriteData *d = (MSQSAWriteData *)f->data;
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the QSA playback filter                               *
 *****************************************************************************/

static int ms_qsa_write_set_sample_rate(MSFilter *f, void *arg) {
	/* TODO */
	return 0;
}

static int ms_qsa_write_get_sample_rate(MSFilter *f, void *arg) {
	/* TODO */
	return 0;
}

static int ms_qsa_write_set_nchannels(MSFilter *f, void *arg) {
	/* TODO */
	return 0;
}

static int ms_qsa_write_get_nchannels(MSFilter *f, void *arg) {
	/* TODO */
	return 0;
}

static MSFilterMethod ms_qsa_write_methods[] = {
	{ MS_FILTER_SET_SAMPLE_RATE, ms_qsa_write_set_sample_rate },
	{ MS_FILTER_GET_SAMPLE_RATE, ms_qsa_write_get_sample_rate },
	{ MS_FILTER_GET_NCHANNELS,   ms_qsa_write_get_nchannels   },
	{ MS_FILTER_SET_NCHANNELS,   ms_qsa_write_set_nchannels   },
	{ 0,                         NULL                         }
};


/******************************************************************************
 * Definition of the QSA playback filter                                      *
 *****************************************************************************/

#define MS_QSA_WRITE_NAME		"MSQSAWrite"
#define MS_QSA_WRITE_DESCRIPTION	"QSA sound playback."
#define MS_QSA_WRITE_CATEGORY	MS_FILTER_OTHER
#define MS_QSA_WRITE_ENC_FMT	NULL
#define MS_QSA_WRITE_NINPUTS	1
#define MS_QSA_WRITE_NOUTPUTS	0
#define MS_QSA_WRITE_FLAGS		0

#ifndef _MSC_VER

MSFilterDesc ms_qsa_write_desc = {
	.id = MS_QSA_WRITE_ID,
	.name = MS_QSA_WRITE_NAME,
	.text = MS_QSA_WRITE_DESCRIPTION,
	.category = MS_QSA_WRITE_CATEGORY,
	.enc_fmt = MS_QSA_WRITE_ENC_FMT,
	.ninputs = MS_QSA_WRITE_NINPUTS,
	.noutputs = MS_QSA_WRITE_NOUTPUTS,
	.init = ms_qsa_write_init,
	.preprocess = ms_qsa_write_preprocess,
	.process = ms_qsa_write_process,
	.postprocess = ms_qsa_write_postprocess,
	.uninit = ms_qsa_write_uninit,
	.methods = ms_qsa_write_methods,
	.flags = MS_QSA_WRITE_FLAGS
};

#else

MSFilterDesc ms_qsa_write_desc = {
	MS_QSA_WRITE_ID,
	MS_QSA_WRITE_NAME,
	MS_QSA_WRITE_DESCRIPTION,
	MS_QSA_WRITE_CATEGORY,
	MS_QSA_WRITE_ENC_FMT,
	MS_QSA_WRITE_NINPUTS,
	MS_QSA_WRITE_NOUTPUTS,
	ms_qsa_write_init,
	ms_qsa_write_preprocess,
	ms_qsa_write_process,
	ms_qsa_write_postprocess,
	ms_qsa_write_uninit,
	ms_qsa_write_methods,
	MS_QSA_WRITE_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_qsa_write_desc)



/******************************************************************************
 * Definition of the QSA sound card                                           *
 *****************************************************************************/

static void ms_qsa_card_detect(MSSndCardManager *m);
static void ms_qsa_card_init(MSSndCard *card);
static void ms_qsa_card_set_level(MSSndCard *card, MSSndCardMixerElem e, int a);
static int ms_qsa_card_get_level(MSSndCard *card, MSSndCardMixerElem e);
static void ms_qsa_card_set_source(MSSndCard *card, MSSndCardCapture source);
static MSFilter * ms_qsa_card_create_reader(MSSndCard *card);
static MSFilter * ms_qsa_card_create_writer(MSSndCard *card);
static void ms_qsa_card_uninit(MSSndCard *card);
static MSSndCard * ms_qsa_card_duplicate(MSSndCard *card);

#ifndef _MSC_VER

MSSndCardDesc ms_qsa_card_desc = {
	.driver_type = "QSA",
	.detect = ms_qsa_card_detect,
	.init = ms_qsa_card_init,
	.set_level = ms_qsa_card_set_level,
	.get_level = ms_qsa_card_get_level,
	.set_capture = ms_qsa_card_set_source,
	.set_control = NULL,
	.get_control = NULL,
	.create_reader = ms_qsa_card_create_reader,
	.create_writer = ms_qsa_card_create_writer,
	.uninit = ms_qsa_card_uninit,
	.duplicate = ms_qsa_card_duplicate,
	.unload = NULL
};

#else

MSSndCardDesc ms_qsa_card_desc = {
	"QSA",
	ms_qsa_card_detect,
	ms_qsa_card_init,
	ms_qsa_card_set_level,
	ms_qsa_card_get_level,
	ms_qsa_card_set_source,
	NULL,
	NULL,
	ms_qsa_card_create_reader,
	ms_qsa_card_create_writer,
	ms_qsa_card_uninit,
	ms_qsa_card_duplicate,
	NULL
};

#endif

static void ms_qsa_card_detect(MSSndCardManager *m) {
	/* TODO */
	ms_error("ms_qsa_card_detect");
}

static void ms_qsa_card_init(MSSndCard *card) {
	/* TODO */
	ms_error("ms_qsa_card_init");
}

static void ms_qsa_card_set_level(MSSndCard *card, MSSndCardMixerElem e, int a) {
	/* TODO */
	ms_error("ms_qsa_card_set_level");
}

static int ms_qsa_card_get_level(MSSndCard *card, MSSndCardMixerElem e) {
	/* TODO */
	ms_error("ms_qsa_card_get_level");
	return 0;
}

static void ms_qsa_card_set_source(MSSndCard *card, MSSndCardCapture source) {
	/* TODO */
	ms_error("ms_qsa_card_set_source");
}

static MSFilter * ms_qsa_card_create_reader(MSSndCard *card) {
	/* TODO */
	ms_error("ms_qsa_card_create_reader");
	return NULL;
}

static MSFilter * ms_qsa_card_create_writer(MSSndCard *card) {
	/* TODO */
	ms_error("ms_qsa_card_create_writer");
	return NULL;
}

static void ms_qsa_card_uninit(MSSndCard *card) {
	/* TODO */
	ms_error("ms_qsa_card_uninit");
}

static MSSndCard * ms_qsa_card_duplicate(MSSndCard *card) {
	/* TODO */
	ms_error("ms_qsa_card_duplicate");
	return NULL;
}
