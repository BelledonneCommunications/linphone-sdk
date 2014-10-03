/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Simon MORLAT (simon.morlat@linphone.org)

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


#include "mediastreamer2/msitc.h"

typedef struct SinkState{
	ms_mutex_t mutex;
	int rate;
	int nchannels;
	MSQueue q;
	const MSFmtDescriptor *fmt;
	MSFilter *source;
}SinkState;

static void itc_source_init(MSFilter *f){
	f->data=NULL;
}

static void itc_source_uninit(MSFilter *f){
}

static SinkState * itc_source_get_sink_state(MSFilter *f){
	MSFilter *sink=(MSFilter *)f->data;
	if (sink){
		SinkState *s=(SinkState*)sink->data;
		return s;
	}
	return NULL;
}

static int itc_source_get_nchannels(MSFilter *f, void *data){
	SinkState *ss=itc_source_get_sink_state(f);
	if (ss){
		*(int*)data=ss->nchannels;
		return 0;
	}
	ms_error("Itc source is not connected.");
	return -1;
}

static int itc_source_get_rate(MSFilter *f, void *data){
	SinkState *ss=itc_source_get_sink_state(f);
	if (ss){
		*(int*)data=ss->rate;
		return 0;
	}
	ms_error("Itc source is not connected.");
	return -1;
}

static int itc_source_get_out_fmt(MSFilter *f, void *data){
	SinkState *ss=itc_source_get_sink_state(f);
	if (ss){
		((MSPinFormat*)data)->fmt=ss->fmt;
		return 0;
	}
	ms_error("Itc source is not connected.");
	return 0;
}

static void itc_source_process(MSFilter *f){
	SinkState *ss=itc_source_get_sink_state(f);
	mblk_t *m;
	
	if (ss){
		ms_mutex_lock(&ss->mutex);
		while((m=ms_queue_get(&ss->q))!=NULL){
			ms_mutex_unlock(&ss->mutex);
			ms_queue_put(f->outputs[0],m);
			ms_mutex_lock(&ss->mutex);
		}
		ms_mutex_unlock(&ss->mutex);
	}
}

static MSFilterMethod source_methods[]={
	{	MS_FILTER_GET_SAMPLE_RATE , itc_source_get_rate },
	{	MS_FILTER_GET_NCHANNELS , itc_source_get_nchannels },
	{	MS_FILTER_GET_OUTPUT_FMT,	itc_source_get_out_fmt },
	{ 0,NULL}
};

#ifdef _MSC_VER

MSFilterDesc ms_itc_source_desc={
	MS_ITC_SOURCE_ID,
	"MSItcSource",
	N_("Inter ticker communication filter."),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	itc_source_init,
	NULL,
	itc_source_process,
	NULL,
	itc_source_uninit,
	source_methods
};

#else

MSFilterDesc ms_itc_source_desc={
	.id=MS_ITC_SOURCE_ID,
	.name="MSItcSource",
	.text=N_("Inter ticker communication filter."),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=itc_source_init,
	.process=itc_source_process,
	.uninit=itc_source_uninit,
	.methods=source_methods
};

#endif

static void itc_sink_init(MSFilter *f){
	SinkState *s;
	f->data=s=ms_new0(SinkState,1);
	ms_mutex_init(&s->mutex,NULL);
	ms_queue_init(&s->q);
}

static void itc_sink_uninit(MSFilter *f){
	SinkState *s=(SinkState *)f->data;
	ms_queue_flush(&s->q);
	ms_mutex_destroy(&s->mutex);
	ms_free(s);
}

static void itc_sink_preprocess(MSFilter *f){
	SinkState *s=(SinkState *)f->data;
	if (s->source && s->fmt==NULL) ms_filter_notify_no_arg(s->source,MS_FILTER_OUTPUT_FMT_CHANGED);
}

static void itc_sink_queue_packet(MSFilter *f, mblk_t *m){
	SinkState *s=(SinkState *)f->data;
	ms_mutex_lock(&s->mutex);
	if (s->source==NULL){
		freemsg(m);
	}else{
		ms_queue_put(&s->q,m);
	}
	ms_mutex_unlock(&s->mutex);
}

static void itc_sink_process(MSFilter *f){
	mblk_t *im;
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		itc_sink_queue_packet(f,im);
	}
}

static int itc_sink_connect(MSFilter *f, void *data){
	SinkState *s=(SinkState *)f->data;
	MSFilter *srcfilter=(MSFilter*)data;
	if (srcfilter){
		srcfilter->data=f;
	}else{
		MSFilter *oldsrc=s->source;
		if (oldsrc)
			oldsrc->data=NULL;
	}
	s->source=srcfilter;
	return 0;
}

static int itc_sink_set_nchannels(MSFilter *f , void *data){
	SinkState *s=(SinkState *)f->data;
	s->nchannels=*(int*)data;
	return 0;
}

static int itc_sink_set_sr(MSFilter *f , void *data){
	SinkState *s=(SinkState *)f->data;
	s->rate=*(int*)data;
	return 0;
}

static int itc_sink_get_nchannels(MSFilter *f , void *data){
	SinkState *s=(SinkState *)f->data;
	*(int*)data=s->nchannels;
	return 0;
}

static int itc_sink_get_sr(MSFilter *f , void *data){
	SinkState *s=(SinkState *)f->data;
	*(int*)data=s->rate;
	return 0;
}

static int itc_sink_set_fmt(MSFilter *f, void *data){
	SinkState *s=(SinkState *)f->data;
	s->fmt=((MSPinFormat*)data)->fmt;
	if (s->source && s->fmt)
		ms_filter_notify_no_arg(s->source,MS_FILTER_OUTPUT_FMT_CHANGED);
	return 0;
}

static MSFilterMethod sink_methods[]={
	{	MS_ITC_SINK_CONNECT , itc_sink_connect },
	{	MS_FILTER_SET_NCHANNELS , itc_sink_set_nchannels },
	{	MS_FILTER_SET_SAMPLE_RATE , itc_sink_set_sr },
	{	MS_FILTER_GET_NCHANNELS, itc_sink_get_nchannels },
	{	MS_FILTER_GET_SAMPLE_RATE, itc_sink_get_sr },
	{	MS_FILTER_SET_INPUT_FMT, itc_sink_set_fmt },
	{ 0, NULL }
};

#ifdef _MSC_VER

MSFilterDesc ms_itc_sink_desc={
	MS_ITC_SINK_ID,
	"MSItcSink",
	N_("Inter ticker communication filter."),
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	itc_sink_init,
	itc_sink_preprocess,
	itc_sink_process,
	NULL,
	itc_sink_uninit,
	sink_methods
};

#else

MSFilterDesc ms_itc_sink_desc={
	.id=MS_ITC_SINK_ID,
	.name="MSItcSink",
	.text=N_("Inter ticker communication filter."),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.init=itc_sink_init,
	.preprocess=itc_sink_preprocess,
	.process=itc_sink_process,
	.uninit=itc_sink_uninit,
	.methods=sink_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_itc_source_desc)
MS_FILTER_DESC_EXPORT(ms_itc_sink_desc)
