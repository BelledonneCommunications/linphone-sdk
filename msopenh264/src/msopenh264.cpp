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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"

#include "msopenh264dec.h"
#include "msopenh264enc.h"

#ifndef VERSION
#define VERSION "0.1.0"
#endif


/******************************************************************************
 * Implementation of the decoder                                              *
 *****************************************************************************/

static void msopenh264_dec_init(MSFilter *f) {
	MSOpenH264Decoder *d = new MSOpenH264Decoder(f);
	f->data = d;
}

static void msopenh264_dec_preprocess(MSFilter *f) {
	MSOpenH264Decoder *d = static_cast<MSOpenH264Decoder *>(f->data);
	d->initialize();
}

static void msopenh264_dec_process(MSFilter *f) {
	MSOpenH264Decoder *d = static_cast<MSOpenH264Decoder *>(f->data);
	d->feed();
}

static void msopenh264_dec_uninit(MSFilter *f) {
	MSOpenH264Decoder *d = static_cast<MSOpenH264Decoder *>(f->data);
	d->uninitialize();
	delete d;
}


/******************************************************************************
 * Methods to configure the decoder                                           *
 *****************************************************************************/

static int msopenh264_dec_add_fmtp(MSFilter *f, void *arg) {
	MSOpenH264Decoder *d = static_cast<MSOpenH264Decoder *>(f->data);
	const char *fmtp = static_cast<const char *>(arg);
	char value[256];
	if (fmtp_get_value(fmtp, "sprop-parameter-sets", value, sizeof(value))) {
		d->provideSpropParameterSets(value, sizeof(value));
	}
	return 0;
}

static int msopenh264_dec_reset_first_image(MSFilter *f, void *arg) {
	MSOpenH264Decoder *d = static_cast<MSOpenH264Decoder *>(f->data);
	d->resetFirstImageDecoded();
	return 0;
}

static int msopenh264_dec_enable_avpf(MSFilter *f, void *arg) {
	MSOpenH264Decoder *d = static_cast<MSOpenH264Decoder *>(f->data);
	d->enableAVPF(*((bool_t *)arg) ? true : false);
	return 0;
}

static int msopenh264_dec_get_size(MSFilter *f, void *arg) {
	MSOpenH264Decoder *d = static_cast<MSOpenH264Decoder *>(f->data);
	MSVideoSize *size = static_cast<MSVideoSize *>(arg);
	*size = d->getSize();
	return 0;
}

static int msopenh264_dec_get_fps(MSFilter *f, void *arg){
	MSOpenH264Decoder *d = static_cast<MSOpenH264Decoder *>(f->data);
	*(float*)arg=d->getFps();
	return 0;
}

static int msopenh264_dec_get_out_fmt(MSFilter *f, void *arg){
	MSOpenH264Decoder *d = static_cast<MSOpenH264Decoder *>(f->data);
	((MSPinFormat*)arg)->pin=0;
	((MSPinFormat*)arg)->fmt=d->getOutFmt();
	return 0;
}

static MSFilterMethod msopenh264_dec_methods[] = {
	{ MS_FILTER_ADD_FMTP,                              msopenh264_dec_add_fmtp          },
	{ MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION, msopenh264_dec_reset_first_image },
	{ MS_VIDEO_DECODER_ENABLE_AVPF,                    msopenh264_dec_enable_avpf       },
	{ MS_FILTER_GET_VIDEO_SIZE,                        msopenh264_dec_get_size          },
	{ MS_FILTER_GET_FPS,                               msopenh264_dec_get_fps           },
	{ MS_FILTER_GET_OUTPUT_FMT,                        msopenh264_dec_get_out_fmt       },
	{ 0,                                               NULL                             }
};

/******************************************************************************
 * Definition of the decoder                                                  *
 *****************************************************************************/

MSFilterDesc msopenh264_dec_desc = {
	MS_FILTER_PLUGIN_ID, /* id */
	"MSOpenH264Dec", /* name */
	"OpenH264 Video Codec provided by Cisco Systems, Inc.", /* text */
	MS_FILTER_DECODER, /* category */
	"H264", /* enc_fmt */
	1, /* ninputs */
	1, /* noutputs */
	msopenh264_dec_init, /* init */
	msopenh264_dec_preprocess, /* preprocess */
	msopenh264_dec_process, /* process */
	NULL, /* postprocess */
	msopenh264_dec_uninit, /* uninit */
	msopenh264_dec_methods, /* methods */
	0 /* flags */
};


/******************************************************************************
 * Implementation of the encoder                                              *
 *****************************************************************************/

static void msopenh264_enc_init(MSFilter *f) {
	MSOpenH264Encoder *e = new MSOpenH264Encoder(f);
	f->data = e;
}

static void msopenh264_enc_preprocess(MSFilter *f) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	e->initialize();
}

static void msopenh264_enc_process(MSFilter *f) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	e->feed();
}

static void msopenh264_enc_postprocess(MSFilter *f) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	e->uninitialize();
}

static void msopenh264_enc_uninit(MSFilter *f) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	delete e;
}


/******************************************************************************
 * Methods to configure the encoder                                           *
 *****************************************************************************/

static int msopenh264_enc_set_fps(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	float *fps = static_cast<float *>(arg);
	e->setFPS(*fps);
	return 0;
}

static int msopenh264_enc_get_fps(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	float *fps = static_cast<float *>(arg);
	*fps = e->getFPS();
	return 0;
}

static int msopenh264_enc_set_bitrate(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	int *bitrate = static_cast<int *>(arg);
	e->setBitrate(*bitrate);
	return 0;
}

static int msopenh264_enc_get_bitrate(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	int *bitrate = static_cast<int *>(arg);
	*bitrate = e->getBitrate();
	return 0;
}

static int msopenh264_enc_set_vsize(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
	e->setSize(*vsize);
	return 0;
}

static int msopenh264_enc_get_vsize(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
	*vsize = e->getSize();
	return 0;
}

static int msopenh264_enc_add_fmtp(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	const char *fmtp = static_cast<const char *>(arg);
	e->addFmtp(fmtp);
	return 0;
}

static int msopenh264_enc_req_vfu(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	e->requestVFU();
	return 0;
}

static int msopenh264_enc_notify_pli(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	e->notifyPLI();
	return 0;
}

static int msopenh264_enc_notify_fir(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	uint8_t seqnr = *((uint8_t *)arg);
	e->notifyFIR(seqnr);
	return 0;
}

static int msopenh264_enc_enable_avpf(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	ms_message("%s: %s AVPF", f->desc->name, *(bool_t *)arg ? "enabling" : "disabling");
	e->enableAVPF(*((bool_t *)arg) ? true : false);
	return 0;
}

static int msopenh264_enc_get_configuration_list(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	const MSVideoConfiguration **vconf_list = static_cast<const MSVideoConfiguration **>(arg);
	*vconf_list = e->getConfigurationList();
	return 0;
}

static int msopenh264_enc_set_configuration_list(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	const MSVideoConfiguration **vconf_list = static_cast<const MSVideoConfiguration **>(arg);
	e->setConfigurationList(*vconf_list);
	return 0;
}

static int msopenh264_enc_set_configuration(MSFilter *f, void *arg) {
	MSOpenH264Encoder *e = static_cast<MSOpenH264Encoder *>(f->data);
	MSVideoConfiguration *vconf = static_cast<MSVideoConfiguration *>(arg);
	e->setConfiguration(*vconf);
	return 0;
}

static MSFilterMethod msopenh264_enc_methods[] = {
	{ MS_FILTER_SET_FPS,                       msopenh264_enc_set_fps                },
	{ MS_FILTER_GET_FPS,                       msopenh264_enc_get_fps                },
	{ MS_FILTER_SET_BITRATE,                   msopenh264_enc_set_bitrate            },
	{ MS_FILTER_GET_BITRATE,                   msopenh264_enc_get_bitrate            },
	{ MS_FILTER_SET_VIDEO_SIZE,                msopenh264_enc_set_vsize              },
	{ MS_FILTER_GET_VIDEO_SIZE,                msopenh264_enc_get_vsize              },
	{ MS_FILTER_ADD_FMTP,                      msopenh264_enc_add_fmtp               },
	{ MS_FILTER_REQ_VFU,                       msopenh264_enc_req_vfu                },
	{ MS_VIDEO_ENCODER_REQ_VFU,                msopenh264_enc_req_vfu                },
	{ MS_VIDEO_ENCODER_NOTIFY_PLI,             msopenh264_enc_notify_pli             },
	{ MS_VIDEO_ENCODER_NOTIFY_FIR,             msopenh264_enc_notify_fir             },
	{ MS_VIDEO_ENCODER_ENABLE_AVPF,            msopenh264_enc_enable_avpf            },
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, msopenh264_enc_get_configuration_list },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION_LIST, msopenh264_enc_set_configuration_list },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION,      msopenh264_enc_set_configuration      },
	{ 0,                                       NULL                                  }
};

/******************************************************************************
 * Definition of the encoder                                                  *
 *****************************************************************************/

MSFilterDesc msopenh264_enc_desc = {
	MS_FILTER_PLUGIN_ID, /* id */
	"MSOpenH264Enc", /* name */
	"OpenH264 Video Codec provided by Cisco Systems, Inc.", /* text */
	MS_FILTER_ENCODER, /* category */
	"H264", /* enc_fmt */
	1, /* ninputs */
	1, /* noutputs */
	msopenh264_enc_init, /* init */
	msopenh264_enc_preprocess, /* preprocess */
	msopenh264_enc_process, /* process */
	msopenh264_enc_postprocess, /* postprocess */
	msopenh264_enc_uninit, /* uninit */
	msopenh264_enc_methods, /* methods */
	0 /* flags */
};


#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) extern "C" __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) extern "C" type
#endif

MS_PLUGIN_DECLARE(void) libmsopenh264_init(MSFactory *factory){
#ifdef OPENH264_DECODER_ENABLED
	ms_factory_register_filter(factory, &msopenh264_dec_desc);
#endif
	ms_factory_register_filter(factory, &msopenh264_enc_desc);
	ms_message("msopenh264-" VERSION " plugin registered.");
}
