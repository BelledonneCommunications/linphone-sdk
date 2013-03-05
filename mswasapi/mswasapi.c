/*
mswasapi.c

mediastreamer2 library - modular sound and video processing and streaming
Windows Audio Session API sound card plugin for mediastreamer2
Copyright (C) 2010-2013 Belledonne Communications, Grenoble, France

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


/**
 * Definition of the private data structure of the WASAPI sound capture filter.
 */
typedef struct _MSWASAPIReadData {
	int rate;
	int nchannels;
} MSWASAPIReadData;

/******************************************************************************
 * Methods to (de)initialize and run the WASAPI sound capture filter          *
 *****************************************************************************/

static void ms_wasapi_read_init(MSFilter *f) {
	MSWASAPIReadData *d = (MSWASAPIReadData *)ms_new(MSWASAPIReadData, 1);
	d->rate = 8000;
	d->nchannels = 1;
	f->data = d;
}

static void ms_wasapi_read_preprocess(MSFilter *f) {
}

static void ms_wasapi_read_process(MSFilter *f) {
}

static void ms_wasapi_read_postprocess(MSFilter *f) {
}

static void ms_wasapi_read_uninit(MSFilter *f) {
	MSWASAPIReadData *d = (MSWASAPIReadData *)f->data;
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the WASAPI sound capture filter                       *
 *****************************************************************************/

static int ms_wasapi_read_set_sample_rate(MSFilter *f, void *arg) {
	return 0;
}

static int ms_wasapi_read_get_sample_rate(MSFilter *f, void *arg) {
	return 0;
}

static MSFilterMethod ms_wasapi_read_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_wasapi_read_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_wasapi_read_get_sample_rate	},
	{	0,				NULL				}
};


/******************************************************************************
 * Definition of the WASAPI sound capture filter                              *
 *****************************************************************************/

#define MS_WASAPI_READ_ID			MS_FILTER_PLUGIN_ID
#define MS_WASAPI_READ_NAME			"MSWASAPIRead"
#define MS_WASAPI_READ_DESCRIPTION	"Windows Audio Session sound capture"
#define MS_WASAPI_READ_CATEGORY		MS_FILTER_OTHER
#define MS_WASAPI_READ_ENC_FMT		NULL
#define MS_WASAPI_READ_NINPUTS		0
#define MS_WASAPI_READ_NOUTPUTS		1
#define MS_WASAPI_READ_FLAGS		0

#ifndef _MSC_VER

MSFilterDesc ms_wasapi_read_desc = {
	.id = MS_WASAPI_READ_ID,
	.name = MS_WASAPI_READ_NAME,
	.text = MS_WASAPI_READ_DESCRIPTION,
	.category = MS_WASAPI_READ_CATEGORY,
	.enc_fmt = MS_WASAPI_READ_ENC_FMT,
	.ninputs = MS_WASAPI_READ_NINPUTS,
	.noutputs = MS_WASAPI_READ_NOUTPUTS,
	.init = ms_wasapi_read_init,
	.preprocess = ms_wasapi_read_preprocess,
	.process = ms_wasapi_read_process,
	.postprocess = ms_wasapi_read_postprocess,
	.uninit = ms_wasapi_read_uninit,
	.methods = ms_wasapi_read_methods,
	.flags = MS_WASAPI_READ_FLAGS
};

#else

MSFilterDesc ms_wasapi_read_desc = {
	MS_WASAPI_READ_ID,
	MS_WASAPI_READ_NAME,
	MS_WASAPI_READ_DESCRIPTION,
	MS_WASAPI_READ_CATEGORY,
	MS_WASAPI_READ_ENC_FMT,
	MS_WASAPI_READ_NINPUTS,
	MS_WASAPI_READ_NOUTPUTS,
	ms_wasapi_read_init,
	ms_wasapi_read_preprocess,
	ms_wasapi_read_process,
	ms_wasapi_read_postprocess,
	ms_wasapi_read_uninit,
	ms_wasapi_read_methods,
	MS_WASAPI_READ_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_wasapi_read_desc)



/**
 * Definition of the private data structure of the WASAPI sound output filter.
 */
typedef struct _MSWASAPIWriteData {
	int rate;
	int nchannels;
} MSWASAPIWriteData;


/******************************************************************************
 * Methods to (de)initialize and run the WASAPI sound output filter           *
 *****************************************************************************/

static void ms_wasapi_write_init(MSFilter *f) {
	MSWASAPIWriteData *d = (MSWASAPIWriteData *)ms_new(MSWASAPIWriteData, 1);
	d->rate = 8000;
	d->nchannels = 1;
	f->data = d;
}

static void ms_wasapi_write_preprocess(MSFilter *f) {
}

static void ms_wasapi_write_process(MSFilter *f) {
}

static void ms_wasapi_write_postprocess(MSFilter *f) {
}

static void ms_wasapi_write_uninit(MSFilter *f) {
	MSWASAPIWriteData *d = (MSWASAPIWriteData *)f->data;
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the WASAPI sound output filter                        *
 *****************************************************************************/

static int ms_wasapi_write_set_sample_rate(MSFilter *f, void *arg) {
	return 0;
}

static int ms_wasapi_write_get_sample_rate(MSFilter *f, void *arg) {
	return 0;
}

static MSFilterMethod ms_wasapi_write_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_wasapi_write_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_wasapi_write_get_sample_rate	},
	{	0,				NULL				}
};


/******************************************************************************
 * Definition of the WASAPI sound output filter                               *
 *****************************************************************************/

#define MS_WASAPI_WRITE_ID				MS_FILTER_PLUGIN_ID
#define MS_WASAPI_WRITE_NAME			"MSWASAPIWrite"
#define MS_WASAPI_WRITE_DESCRIPTION		"Windows Audio Session sound output"
#define MS_WASAPI_WRITE_CATEGORY		MS_FILTER_OTHER
#define MS_WASAPI_WRITE_ENC_FMT			NULL
#define MS_WASAPI_WRITE_NINPUTS			1
#define MS_WASAPI_WRITE_NOUTPUTS		0
#define MS_WASAPI_WRITE_FLAGS			0

#ifndef _MSC_VER

MSFilterDesc ms_wasapi_write_desc = {
	.id = MS_WASAPI_WRITE_ID,
	.name = MS_WASAPI_WRITE_NAME,
	.text = MS_WASAPI_WRITE_DESCRIPTION,
	.category = MS_WASAPI_WRITE_CATEGORY,
	.enc_fmt = MS_WASAPI_WRITE_ENC_FMT,
	.ninputs = MS_WASAPI_WRITE_NINPUTS,
	.noutputs = MS_WASAPI_WRITE_NOUTPUTS,
	.init = ms_wasapi_write_init,
	.preprocess = ms_wasapi_write_preprocess,
	.process = ms_wasapi_write_process,
	.postprocess = ms_wasapi_write_postprocess,
	.uninit = ms_wasapi_write_uninit,
	.methods = ms_wasapi_write_methods,
	.flags = MS_WASAPI_WRITE_FLAGS
};

#else

MSFilterDesc ms_wasapi_write_desc = {
	MS_WASAPI_WRITE_ID,
	MS_WASAPI_WRITE_NAME,
	MS_WASAPI_WRITE_DESCRIPTION,
	MS_WASAPI_WRITE_CATEGORY,
	MS_WASAPI_WRITE_ENC_FMT,
	MS_WASAPI_WRITE_NINPUTS,
	MS_WASAPI_WRITE_NOUTPUTS,
	ms_wasapi_write_init,
	ms_wasapi_write_preprocess,
	ms_wasapi_write_process,
	ms_wasapi_write_postprocess,
	ms_wasapi_write_uninit,
	ms_wasapi_write_methods,
	MS_WASAPI_WRITE_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_wasapi_write_desc)


#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) type
#endif

MS_PLUGIN_DECLARE(void) libmswasapi_init(void) {
	ms_filter_register(&ms_wasapi_read_desc);
	ms_filter_register(&ms_wasapi_write_desc);
	ms_message("libmswasapi plugin loaded");
}

