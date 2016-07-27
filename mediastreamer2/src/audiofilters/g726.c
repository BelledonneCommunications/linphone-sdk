/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011  Belledonne Communications SARL
Author: Simon Morlat <simon.morlat@linphone.org>

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

#include "mediastreamer2/msfilter.h"
#include <spandsp.h>

typedef struct _EncState{
	MSBufferizer *input;
	g726_state_t *impl;
	int ptime;
	int packing;
	int bitrate;
	int nsamples;
	uint32_t ts;
}EncState;

typedef struct g726_config{
	const char *mime_type;
	int packing;
	int bitrate;
}g726_config_t;

static g726_config_t g726_config[]={
	{"G726-40", G726_PACKING_LEFT, 40000 },
	{"G726-32", G726_PACKING_LEFT, 32000 },
	{"G726-24", G726_PACKING_LEFT, 24000 },
	{"G726-16", G726_PACKING_LEFT, 16000 },
	{"AAL2-G726-40", G726_PACKING_RIGHT, 40000 },
	{"AAL2-G726-32", G726_PACKING_RIGHT, 32000 },
	{"AAL2-G726-24", G726_PACKING_RIGHT, 24000 },
	{"AAL2-G726-16", G726_PACKING_RIGHT, 16000 },
	{	NULL		, 0	,	0}
};

static void enc_init_from_conf(EncState *s, const char *mime_type){
	int i;
	for(i=0;g726_config[i].mime_type!=NULL;++i){
		if (strcasecmp(mime_type,g726_config[i].mime_type)==0){
			s->bitrate=g726_config[i].bitrate;
			s->packing=g726_config[i].packing;
			return ;
		}
	}
	ms_fatal("Bad G726 mime type %s",mime_type);
}

static void enc_init(MSFilter *f){
	EncState *s=ms_new0(EncState,1);
	s->ptime=20;
	s->nsamples=(8000*s->ptime)/1000;
	enc_init_from_conf(s,f->desc->enc_fmt);
	s->impl=g726_init(NULL,s->bitrate,G726_ENCODING_LINEAR,s->packing);
	s->input=ms_bufferizer_new();
	f->data=s;
}

static void enc_preprocess(MSFilter *f){
	EncState *s=(EncState*)f->data;
	s->nsamples=(8000*s->ptime)/1000;
}

static void enc_process(MSFilter *f){
	EncState *s=(EncState*)f->data;
	
	ms_filter_lock(f);
	{
		int16_t *pcmbuf=(int16_t*)alloca(s->nsamples*sizeof(int16_t));
		int encoded_bytes=(s->nsamples*2*s->bitrate)/128000;
		ms_bufferizer_put_from_queue(s->input,f->inputs[0]);
		
		while(ms_bufferizer_read(s->input,(uint8_t*)pcmbuf,s->nsamples*2)!=0){
			mblk_t *om=allocb(encoded_bytes,0);
			om->b_wptr+=g726_encode(s->impl,om->b_wptr,pcmbuf,s->nsamples);
			ms_bufferizer_fill_current_metas(s->input, om);
			mblk_set_timestamp_info(om,s->ts);
			s->ts+=s->nsamples;
			ms_queue_put(f->outputs[0],om);
		}
	}
	ms_filter_unlock(f);
}

static void enc_uninit(MSFilter *f){
	EncState *s=(EncState*)f->data;
	ms_bufferizer_destroy(s->input);
	g726_free(s->impl);
	ms_free(s);
}

static void set_ptime(EncState *s, int ptime){
	if (ptime>0 && ptime<=100){
		s->ptime=ptime;
	}else ms_warning("Bad or unsupported ptime value %i for g726 encoder.",ptime);
}

static int enc_add_fmtp(MSFilter *f, void *data){
	EncState *s=(EncState*)f->data;
	char tmp[16];
	if (fmtp_get_value((const char*)data,"ptime",tmp,sizeof(tmp))){
		ms_filter_lock(f);
		set_ptime(s,atoi(tmp));
		enc_preprocess(f);
		ms_filter_unlock(f);
	}
	return 0;
}

static int enc_add_attr(MSFilter *f, void *data){
	EncState *s=(EncState*)f->data;
	const char *attr=(const char *)data;
	const char *tmp;
	if ((tmp=strstr(attr,"ptime:"))!=NULL){
		set_ptime(s,atoi(tmp+6));
	}
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_ADD_FMTP	,	enc_add_fmtp	},
	{	MS_FILTER_ADD_ATTR	,	enc_add_attr	},
	{	0					,	NULL			}
};

typedef struct _EncState DecState;

static void dec_init(MSFilter *f){
	EncState *s=ms_new0(DecState,1);
	enc_init_from_conf(s,f->desc->enc_fmt);
	s->impl=g726_init(NULL,s->bitrate,G726_ENCODING_LINEAR,s->packing);
	f->data=s;
}

static void dec_process(MSFilter *f){
	DecState *s=(DecState*)f->data;
	mblk_t *im;
	
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		int size=im->b_wptr-im->b_rptr;
		int decoded_bytes=(size*128000)/s->bitrate;
		mblk_t *om=allocb(decoded_bytes,0);
		mblk_meta_copy(im, om);
		g726_decode(s->impl,(int16_t*)om->b_wptr,im->b_rptr,size);
		freemsg(im);
		om->b_wptr+=decoded_bytes;
		ms_queue_put(f->outputs[0],om);
	}
}

static void dec_uninit(MSFilter *f){
	DecState *s=(DecState*)f->data;
	g726_free(s->impl);
	ms_free(s);
}	

#ifndef _MSC_VER

#define MAKE_G726_DESC(bitrate) \
MSFilterDesc ms_g726_##bitrate##_enc_desc={ \
	.id=MS_G726_##bitrate##_ENC_ID, \
	.name="MSG726-" #bitrate "Enc", \
	.text=N_("G726 encoder at " #bitrate " kbit/s"), \
	.category=MS_FILTER_ENCODER, \
	.enc_fmt="G726-" #bitrate, \
	.ninputs=1, \
	.noutputs=1, \
	.init=enc_init, \
	.preprocess=enc_preprocess, \
	.process=enc_process, \
	.uninit=enc_uninit, \
	.methods=methods \
};\
MSFilterDesc ms_aal2_g726_##bitrate##_enc_desc={ \
	.id=MS_AAL2_G726_##bitrate##_ENC_ID, \
	.name="MSAAL2-G726-" #bitrate "Enc", \
	.text=N_("AAL2-G726 encoder at " #bitrate " kbit/s"), \
	.category=MS_FILTER_ENCODER, \
	.enc_fmt="AAL2-G726-" #bitrate, \
	.ninputs=1, \
	.noutputs=1, \
	.init=enc_init, \
	.preprocess=enc_preprocess, \
	.process=enc_process, \
	.uninit=enc_uninit, \
	.methods=methods \
};\
MSFilterDesc ms_g726_##bitrate##_dec_desc={ \
	.id=MS_G726_##bitrate##_DEC_ID, \
	.name="MSG726-" #bitrate "Dec", \
	.text=N_("G726 decoder at " #bitrate " kbit/s"), \
	.category=MS_FILTER_DECODER, \
	.enc_fmt="G726-" #bitrate, \
	.ninputs=1, \
	.noutputs=1, \
	.init=dec_init, \
	.process=dec_process, \
	.uninit=dec_uninit \
};\
MSFilterDesc ms_aal2_g726_##bitrate##_dec_desc={ \
	.id=MS_AAL2_G726_##bitrate##_DEC_ID, \
	.name="MSAAL2-G726-" #bitrate "Dec", \
	.text=N_("AAL2-G726 decoder at " #bitrate " kbit/s"), \
	.category=MS_FILTER_DECODER, \
	.enc_fmt="AAL2-G726-" #bitrate, \
	.ninputs=1, \
	.noutputs=1, \
	.init=dec_init, \
	.process=dec_process, \
	.uninit=dec_uninit, \
};


#else

#define MAKE_G726_DESC(bitrate) \
MSFilterDesc ms_g726_##bitrate##_desc={ \
	MS_G726_##bitrate##_ENC_ID, \
	"MSG726-" #bitrate "Enc", \
	N_("G726 encoder at " #bitrate " kbit/s"), \
	MS_FILTER_ENCODER, \
	"G726-" #bitrate, \
	1, \
	1, \
	enc_init, \
	enc_preprocess, \
	enc_process, \
	NULL, \
	enc_uninit, \
	methods \
};\
MSFilterDesc ms_aal2_g726_##bitrate##_desc={ \
	MS_AAL2_G726_##bitrate##_ENC_ID, \
	"MSAAL2-G726-" #bitrate "Enc", \
	N_("AAL2-G726 encoder at " #bitrate " kbit/s"), \
	MS_FILTER_ENCODER, \
	"AAL2-G726-" #bitrate, \
	1, \
	1, \
	enc_init, \
	enc_preprocess, \
	enc_process, \
	NULL, \
	enc_uninit, \
	methods \
}; \
MSFilterDesc ms_g726_##bitrate##_dec_desc={ \
	MS_G726_##bitrate##_DEC_ID, \
	"MSG726-" #bitrate "Dec", \
	N_("G726 decoder at " #bitrate " kbit/s"), \
	MS_FILTER_DECODER, \
	"G726-" #bitrate, \
	1, \
	1, \
	dec_init, \
	NULL, \
	dec_process, \
	NULL, \
	dec_uninit, \
	methods \
};\
MSFilterDesc ms_aal2_g726_##bitrate##_dec_desc={ \
	MS_AAL2_G726_##bitrate##_DEC_ID, \
	"MSAAL2-G726-" #bitrate "Dec", \
	N_("AAL2-G726 decoder at " #bitrate " kbit/s"), \
	MS_FILTER_DECODER, \
	"AAL2-G726-" #bitrate, \
	1, \
	1, \
	dec_init, \
	NULL, \
	dec_process, \
	NULL, \
	dec_uninit, \
	methods \
};

#endif

MAKE_G726_DESC(40)
MS_FILTER_DESC_EXPORT(ms_g726_40_enc_desc)
MS_FILTER_DESC_EXPORT(ms_g726_40_dec_desc)
MS_FILTER_DESC_EXPORT(ms_aal2_g726_40_enc_desc)
MS_FILTER_DESC_EXPORT(ms_aal2_g726_40_dec_desc)

MAKE_G726_DESC(32)
MS_FILTER_DESC_EXPORT(ms_g726_32_enc_desc)
MS_FILTER_DESC_EXPORT(ms_g726_32_dec_desc)
MS_FILTER_DESC_EXPORT(ms_aal2_g726_32_enc_desc)
MS_FILTER_DESC_EXPORT(ms_aal2_g726_32_dec_desc)

MAKE_G726_DESC(24)
MS_FILTER_DESC_EXPORT(ms_g726_24_enc_desc)
MS_FILTER_DESC_EXPORT(ms_g726_24_dec_desc)
MS_FILTER_DESC_EXPORT(ms_aal2_g726_24_enc_desc)
MS_FILTER_DESC_EXPORT(ms_aal2_g726_24_dec_desc)

MAKE_G726_DESC(16)
MS_FILTER_DESC_EXPORT(ms_g726_16_enc_desc)
MS_FILTER_DESC_EXPORT(ms_g726_16_dec_desc)
MS_FILTER_DESC_EXPORT(ms_aal2_g726_16_enc_desc)
MS_FILTER_DESC_EXPORT(ms_aal2_g726_16_dec_desc)

