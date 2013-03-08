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
#include "mediastreamer2/mssndcard.h"

#include "mswasapi_reader.h"
#include "mswasapi_writer.h"


const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);


/******************************************************************************
 * Methods to (de)initialize and run the WASAPI sound capture filter          *
 *****************************************************************************/

static void ms_wasapi_read_init(MSFilter *f) {
	MSWASAPIReader *r = new MSWASAPIReader();
	f->data = r;
}

static void ms_wasapi_read_preprocess(MSFilter *f) {
	MSWASAPIReader *r = (MSWASAPIReader *)f->data;
	r->activate();
}

static void ms_wasapi_read_process(MSFilter *f) {
	MSWASAPIReader *r = (MSWASAPIReader *)f->data;

	if (!r->isStarted()) {
		r->start();
	}
	r->feed(f);
}

static void ms_wasapi_read_postprocess(MSFilter *f) {
	MSWASAPIReader *r = (MSWASAPIReader *)f->data;
	r->stop();
	r->deactivate();
}

static void ms_wasapi_read_uninit(MSFilter *f) {
	MSWASAPIReader *r = (MSWASAPIReader *)f->data;
	delete r;
}


/******************************************************************************
 * Methods to configure the WASAPI sound capture filter                       *
 *****************************************************************************/

static int ms_wasapi_read_set_sample_rate(MSFilter *f, void *arg) {
	/* This is not supported: the Audio Client requires to use the native sample rate. */
	MS_UNUSED(f), MS_UNUSED(arg);
	return -1;
}

static int ms_wasapi_read_get_sample_rate(MSFilter *f, void *arg) {
	MSWASAPIReader *r = (MSWASAPIReader *)f->data;
	*((int *)arg) = r->getRate();
	return 0;
}

static int ms_wasapi_read_set_nchannels(MSFilter *f, void *arg) {
	/* This is not supported: the Audio Client requires to use 1 channel. */
	MS_UNUSED(f), MS_UNUSED(arg);
	return -1;
}

static int ms_wasapi_read_get_nchannels(MSFilter *f, void *arg) {
	MSWASAPIReader *r = (MSWASAPIReader *)f->data;
	*((int *)arg) = r->getNChannels();
	return 0;
}

static MSFilterMethod ms_wasapi_read_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_wasapi_read_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_wasapi_read_get_sample_rate	},
	{	MS_FILTER_SET_NCHANNELS,	ms_wasapi_read_set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS,	ms_wasapi_read_get_nchannels	},
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




/******************************************************************************
 * Methods to (de)initialize and run the WASAPI sound output filter           *
 *****************************************************************************/

static void ms_wasapi_write_init(MSFilter *f) {
	MSWASAPIWriter *w = new MSWASAPIWriter();
	f->data = w;
}

static void ms_wasapi_write_preprocess(MSFilter *f) {
	MSWASAPIWriter *w = (MSWASAPIWriter *)f->data;
	w->activate();
}

static void ms_wasapi_write_process(MSFilter *f) {
	MSWASAPIWriter *w = (MSWASAPIWriter *)f->data;

	if (!w->isStarted()) {
		w->start();
	}
	w->feed(f);
}

static void ms_wasapi_write_postprocess(MSFilter *f) {
	MSWASAPIWriter *w = (MSWASAPIWriter *)f->data;
	w->stop();
	w->deactivate();
}

static void ms_wasapi_write_uninit(MSFilter *f) {
	MSWASAPIWriter *w = (MSWASAPIWriter *)f->data;
	delete w;
}


/******************************************************************************
 * Methods to configure the WASAPI sound output filter                        *
 *****************************************************************************/

static int ms_wasapi_write_set_sample_rate(MSFilter *f, void *arg) {
	/* This is not supported: the Audio Client requires to use the native sample rate. */
	MS_UNUSED(f), MS_UNUSED(arg);
	return -1;
}

static int ms_wasapi_write_get_sample_rate(MSFilter *f, void *arg) {
	MSWASAPIWriter *w = (MSWASAPIWriter *)f->data;
	*((int *)arg) = w->getRate();
	return 0;
}

static int ms_wasapi_write_set_nchannels(MSFilter *f, void *arg) {
	/* This is not supported: the Audio Client requires to use 2 channels. */
	MS_UNUSED(f), MS_UNUSED(arg);
	return -1;
}

static int ms_wasapi_write_get_nchannels(MSFilter *f, void *arg) {
	MSWASAPIWriter *w = (MSWASAPIWriter *)f->data;
	*((int *)arg) = w->getNChannels();
	return 0;
}

static MSFilterMethod ms_wasapi_write_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_wasapi_write_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_wasapi_write_get_sample_rate	},
	{	MS_FILTER_SET_NCHANNELS,	ms_wasapi_write_set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS,	ms_wasapi_write_get_nchannels	},
	{	0,							NULL							}
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



static void ms_wasapi_snd_card_detect(MSSndCardManager *m);
static MSFilter *ms_wasapi_snd_card_create_reader(MSSndCard *card);
static MSFilter *ms_wasapi_snd_card_create_writer(MSSndCard *card);

static MSSndCardDesc ms_wasapi_snd_card_desc = {
	"MSWASAPISndCard",
	ms_wasapi_snd_card_detect,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ms_wasapi_snd_card_create_reader,
	ms_wasapi_snd_card_create_writer,
	NULL,
	NULL,
	NULL
};

static MSSndCard *ms_wasapi_snd_card_new(void) {
	MSSndCard *card = ms_snd_card_new(&ms_wasapi_snd_card_desc);
	card->name = ms_strdup("WASAPI sound card");
	card->latency = 250;
	return card;
}

static void ms_wasapi_snd_card_detect(MSSndCardManager *m) {
	MSSndCard *card = ms_wasapi_snd_card_new();
	ms_snd_card_manager_add_card(m, card);
}

static MSFilter *ms_wasapi_snd_card_create_reader(MSSndCard *card) {
	MS_UNUSED(card);
	return ms_filter_new_from_desc(&ms_wasapi_read_desc);
}

static MSFilter *ms_wasapi_snd_card_create_writer(MSSndCard *card) {
	MS_UNUSED(card);
	return ms_filter_new_from_desc(&ms_wasapi_write_desc);
}




#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) extern "C" __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) extern "C" type
#endif

MS_PLUGIN_DECLARE(void) libmswasapi_init(void) {
	MSSndCardManager *manager = ms_snd_card_manager_get();
	ms_snd_card_manager_register_desc(manager, &ms_wasapi_snd_card_desc);
	ms_message("libmswasapi plugin loaded");
}
