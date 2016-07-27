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

#include <mediastreamer2/msfilter.h>

struct EncState {
	uint32_t ts;
	int ptime;
	int rate;
	int nchannels;
	size_t nbytes;
	MSBufferizer *bufferizer;
};

static void enc_init(MSFilter *f)
{
	struct EncState *s=ms_new0(struct EncState,1);
	s->ts=0;
	s->bufferizer=ms_bufferizer_new();
	s->ptime = 10;
	s->rate=8000;
	s->nchannels = 1;
	f->data=s;
};

static void enc_uninit(MSFilter *f)
{
	struct EncState *s=(struct EncState*)f->data;
	ms_bufferizer_destroy(s->bufferizer);
	ms_free(s);
	f->data = 0;
};

static void enc_update(struct EncState *s){
	s->nbytes=(2*s->nchannels*s->rate*s->ptime)/1000;
}

static void enc_preprocess(MSFilter *f){
	struct EncState *s=(struct EncState*)f->data;
	enc_update(s);
}

static void host_to_network(int16_t *buffer, int nsamples){
	int i;
	for(i=0;i<nsamples;++i){
		buffer[i]=htons(buffer[i]);
	}
}

static void network_to_host(int16_t *buffer, int nsamples){
	int i;
	for(i=0;i<nsamples;++i){
		buffer[i]=ntohs(buffer[i]);
	}
}

static void enc_process(MSFilter *f){
	struct EncState *s=(struct EncState*)f->data;
	
	ms_filter_lock(f);
	ms_bufferizer_put_from_queue(s->bufferizer,f->inputs[0]);
	
	while(ms_bufferizer_get_avail(s->bufferizer)>=s->nbytes) {
		mblk_t *om=allocb(s->nbytes,0);
		om->b_wptr+=ms_bufferizer_read(s->bufferizer,om->b_wptr,s->nbytes);
		host_to_network((int16_t*)om->b_rptr,(int)(s->nbytes/2));
		ms_bufferizer_fill_current_metas(s->bufferizer, om);
		mblk_set_timestamp_info(om,s->ts);
		ms_queue_put(f->outputs[0],om);
		s->ts += (uint32_t)(s->nbytes/(2*s->nchannels));
	}
	ms_filter_unlock(f);
};

static void set_ptime(struct EncState *s, int value){
	if (value>0 && value<=100){
		s->ptime=value;
		ms_message("L16 encoder using ptime=%i",value);
		enc_update(s);
	}
}

static int enc_add_attr(MSFilter *f, void *arg)
{
	const char *fmtp=(const char*)arg;
	struct EncState *s=(struct EncState*)f->data;
	if(strstr(fmtp,"ptime:")){
		ms_filter_lock(f);
		set_ptime(s,atoi(fmtp+6));
		ms_filter_unlock(f);
	}
	return 0;
};

static int enc_add_fmtp(MSFilter *f, void *arg){
	const char *fmtp=(const char*)arg;
	struct EncState *s=(struct EncState*)f->data;
	char tmp[16]={0};
	if (fmtp_get_value(fmtp,"ptime",tmp,sizeof(tmp))){
		ms_filter_lock(f);
		set_ptime(s,atoi(tmp));
		ms_filter_unlock(f);
	}
	return 0;
}

static int enc_set_sr(MSFilter *f, void *arg){
	struct EncState *s=(struct EncState*)f->data;
	s->rate=*(int*)arg;
	return 0;
}

static int enc_set_nchannels(MSFilter *f, void *arg) {
	struct EncState *s = (struct EncState *)f->data;
	s->nchannels = *(int *)arg;
	return 0;
}

static int enc_get_sr(MSFilter *f, void *arg){
	struct EncState *s=(struct EncState*)f->data;
	*(int*)arg = s->rate;
	return 0;
}

static int enc_get_nchannels(MSFilter *f, void *arg) {
	struct EncState *s = (struct EncState *)f->data;
	*(int *)arg = s->nchannels;
	return 0;
}

static MSFilterMethod enc_methods[]={
	{	MS_FILTER_ADD_ATTR		,	enc_add_attr},
	{	MS_FILTER_ADD_FMTP		,	enc_add_fmtp},
	{	MS_FILTER_SET_SAMPLE_RATE	,	enc_set_sr	},
	{	MS_FILTER_SET_NCHANNELS		,	enc_set_nchannels},
	{	MS_FILTER_GET_SAMPLE_RATE	,	enc_get_sr	},
	{	MS_FILTER_GET_NCHANNELS		,	enc_get_nchannels},
	{	0				,	NULL		}
};

#ifdef _MSC_VER

MSFilterDesc ms_l16_enc_desc={
	MS_L16_ENC_ID,
	"MSL16Enc",
	"L16 dummy encoder",
	MS_FILTER_ENCODER,
	"L16",
	1,
	1,
	enc_init,
	enc_preprocess,
	enc_process,
	NULL,
	enc_uninit,
	enc_methods
};

#else

MSFilterDesc ms_l16_enc_desc={
	.id			= MS_L16_ENC_ID,
	.name		= "MSL16Enc",
	.text		= "L16 dummy encoder",
	.category	= MS_FILTER_ENCODER,
	.enc_fmt	= "L16",
	.ninputs	= 1,
	.noutputs	= 1,
	.init		= enc_init,
	.preprocess = enc_preprocess,
	.process	= enc_process,
	.uninit		= enc_uninit,
	.methods	= enc_methods
};
#endif


typedef struct _DecState {
	int rate;
	int nchannels;
}DecState;


static void dec_init(MSFilter *f){
	DecState *s = ms_new0(DecState,1);
	s->rate = 8000;
	s->nchannels = 1;
	f->data = s;
};

static void dec_uninit(MSFilter *f){
	ms_free(f->data);
};

static void dec_process(MSFilter *f)
{
	mblk_t *im;

	while((im=ms_queue_get(f->inputs[0]))) {
		network_to_host((int16_t*)im->b_rptr,(int)((im->b_wptr-im->b_rptr)/2));
		ms_queue_put(f->outputs[0],im);
	}
};

static int dec_set_sr(MSFilter *f, void *arg){
	DecState *s = (DecState*)f->data;
	int *sample_rate = (int *)arg;
	s->rate = *sample_rate;
	return 0;
}

static int dec_get_sr(MSFilter *f, void *arg){
	DecState *s = (DecState*)f->data;
	int *sample_rate = (int *)arg;
	*sample_rate = s->rate;
	return 0;
}

static int dec_get_nchannels(MSFilter *f, void *arg){
	DecState *s = (DecState*)f->data;
	int *nchannels = (int *)arg;
	*nchannels = s->nchannels;
	return 0;
}

static int dec_set_nchannels(MSFilter *f, void *arg){
	DecState *s = (DecState*)f->data;
	int *nchannels = (int *)arg;
	s->nchannels = *nchannels;
	return 0;
}

static MSFilterMethod dec_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	,	dec_set_sr	},
	{	MS_FILTER_GET_SAMPLE_RATE	,	dec_get_sr	},
	{	MS_FILTER_GET_NCHANNELS		,	dec_get_nchannels},
	{	MS_FILTER_SET_NCHANNELS		,	dec_set_nchannels},
	{	0				,	NULL		}
};

#ifdef _MSC_VER

MSFilterDesc ms_l16_dec_desc={
	MS_L16_DEC_ID,
	"MSL16Dec",
	"L16 dummy decoder",
	MS_FILTER_DECODER,
	"L16",
	1,
	1,
	dec_init,
	NULL,
	dec_process,
	NULL,
	dec_uninit,
	dec_methods
};

#else

MSFilterDesc ms_l16_dec_desc={
	.id			= MS_L16_DEC_ID,
	.name		= "MSL16Dec",
	.text		= "L16 dummy decoder",
	.category	= MS_FILTER_DECODER,
	.enc_fmt	= "L16",
	.ninputs	= 1,
	.noutputs	= 1,
	.init		= dec_init,
	.process	= dec_process,
	.uninit		= dec_uninit,
	.methods	= dec_methods
};

#endif


MS_FILTER_DESC_EXPORT(ms_l16_dec_desc)
MS_FILTER_DESC_EXPORT(ms_l16_enc_desc)
