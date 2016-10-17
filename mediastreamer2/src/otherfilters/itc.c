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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "mediastreamer2/msitc.h"

typedef struct SharedState{
	ms_mutex_t mutex;
	int refcnt;
	int rate;
	int nchannels;
	MSQueue q;
	const MSFmtDescriptor *fmt;
	MSFilter *source;
}SharedState;

static SharedState * itc_get_shared_state(MSFilter *f){
	SharedState *s=(SharedState*)f->data;
	return s;
}

static SharedState *shared_state_new(void){
	SharedState *s = ms_new0(SharedState,1);
	ms_mutex_init(&s->mutex, NULL);
	ms_queue_init(&s->q);
	return s;
}

static void shared_state_release(SharedState *s){
	ms_mutex_lock(&s->mutex);
	s->refcnt--;
	ms_mutex_unlock(&s->mutex);
	
	if (s->refcnt == 0){
		ms_mutex_destroy(&s->mutex);
		ms_queue_flush(&s->q);
		ms_free(s);
	}
}

static void itc_assign(MSFilter *f, SharedState *s, bool_t is_source){
	SharedState *current;
	current = itc_get_shared_state(f);
	
	if (s){
		ms_mutex_lock(&s->mutex);
		if (is_source) s->source = f;
		s->refcnt++;
		ms_mutex_unlock(&s->mutex);
	}
	f->data = s;
	if (current){
		if (current != s){
			ms_mutex_lock(&current->mutex);
			if (current->source == f)
				current->source = NULL;
			ms_mutex_unlock(&current->mutex);
		}
		shared_state_release(current);
	}
}


static void itc_connect(MSFilter *sink, MSFilter *source){
	SharedState *s;

	s = (SharedState*) sink->data;
	if (!s){
		itc_assign(sink, s = shared_state_new(), FALSE);
	}
	if (source){
		ms_filter_lock(source);
		itc_assign(source, s, TRUE);
		ms_filter_unlock(source);
	}
}


static void itc_source_init(MSFilter *f){
	f->data=NULL;
}

static void itc_source_uninit(MSFilter *f){
	itc_assign(f, NULL, TRUE);
}

static int itc_source_get_nchannels(MSFilter *f, void *data){
	SharedState *ss=itc_get_shared_state(f);
	if (ss){
		*(int*)data=ss->nchannels;
		return 0;
	}
	ms_error("Itc source is not connected.");
	return -1;
}

static int itc_source_get_rate(MSFilter *f, void *data){
	SharedState *ss=itc_get_shared_state(f);
	if (ss){
		*(int*)data=ss->rate;
		return 0;
	}
	ms_error("Itc source is not connected.");
	return -1;
}

static int itc_source_get_out_fmt(MSFilter *f, void *data){
	SharedState *ss=itc_get_shared_state(f);
	if (ss){
		((MSPinFormat*)data)->fmt=ss->fmt;
		return 0;
	}
	ms_error("Itc source is not connected.");
	return 0;
}

static void itc_source_process(MSFilter *f){
	SharedState *ss;
	mblk_t *m;
	
	ms_filter_lock(f);
	ss = itc_get_shared_state(f);
	
	if (ss){
		ms_mutex_lock(&ss->mutex);
		while((m=ms_queue_get(&ss->q))!=NULL){
			ms_mutex_unlock(&ss->mutex);
			ms_queue_put(f->outputs[0],m);
			ms_mutex_lock(&ss->mutex);
		}
		ms_mutex_unlock(&ss->mutex);
	}
	ms_filter_unlock(f);
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
	itc_assign(f, shared_state_new(), FALSE);
}

static void itc_sink_uninit(MSFilter *f){
	itc_assign(f, NULL, FALSE);
}

static void itc_sink_preprocess(MSFilter *f){
	SharedState *ss;
	ms_filter_lock(f);
	ss = itc_get_shared_state(f);
	ms_filter_unlock(f);
	
	ms_mutex_lock(&ss->mutex);
	if (ss->source && ss->fmt != NULL) ms_filter_notify_no_arg(ss->source,MS_FILTER_OUTPUT_FMT_CHANGED);
	ms_mutex_unlock(&ss->mutex);
}

static void itc_sink_process(MSFilter *f){
	SharedState *s;
	mblk_t *im;
	
	ms_filter_lock(f);
	s = itc_get_shared_state(f);
	ms_filter_unlock(f);
	
	ms_mutex_lock(&s->mutex);
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		if (s->source == NULL){
			freemsg(im);
		}else{
			ms_queue_put(&s->q, im);
		}
	}
	ms_mutex_unlock(&s->mutex);
}

static int itc_sink_connect(MSFilter *f, void *data){
	ms_filter_lock(f);
	itc_connect(f, (MSFilter *)data);
	ms_filter_unlock(f);
	return 0;
}

static int itc_sink_set_nchannels(MSFilter *f , void *data){
	SharedState *s;
	ms_filter_lock(f);
	s = itc_get_shared_state(f);
	s->nchannels=*(int*)data;
	ms_filter_unlock(f);
	return 0;
}

static int itc_sink_set_sr(MSFilter *f , void *data){
	SharedState *s;
	ms_filter_lock(f);
	s = itc_get_shared_state(f);
	s->rate=*(int*)data;
	ms_filter_unlock(f);
	return 0;
}

static int itc_sink_get_nchannels(MSFilter *f , void *data){
	SharedState *s;
	ms_filter_lock(f);
	s = itc_get_shared_state(f);
	*(int*)data=s->nchannels;
	ms_filter_unlock(f);
	return 0;
}

static int itc_sink_get_sr(MSFilter *f , void *data){
	SharedState *s;
	ms_filter_lock(f);
	s = itc_get_shared_state(f);
	*(int*)data=s->rate;
	ms_filter_unlock(f);
	return 0;
}

static int itc_sink_set_fmt(MSFilter *f, void *data){
	SharedState *s;
	ms_filter_lock(f);
	s = itc_get_shared_state(f);
	s->fmt=((MSPinFormat*)data)->fmt;
	ms_filter_unlock(f);
	ms_mutex_lock(&s->mutex);
	if (s->source && s->fmt)
		ms_filter_notify_no_arg(s->source,MS_FILTER_OUTPUT_FMT_CHANGED);
	ms_mutex_unlock(&s->mutex);
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
