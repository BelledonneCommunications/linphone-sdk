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

typedef struct SourceState{
	ms_mutex_t mutex;
	MSQueue q;
}SourceState;

static void itc_source_init(MSFilter *f){
	SourceState *s=ms_new(SourceState,1);
	ms_mutex_init(&s->mutex,NULL);
	ms_queue_init(&s->q);
	f->data=s;
}

static void itc_source_uninit(MSFilter *f){
	SourceState *s=(SourceState *)f->data;
	ms_mutex_destroy(&s->mutex);
	ms_queue_flush (&s->q);
	ms_free(s);
}

static void itc_source_queue_packet(MSFilter *f, mblk_t *m){
	SourceState *s=(SourceState *)f->data;
	ms_mutex_lock(&s->mutex);
	ms_queue_put(&s->q,m);
	ms_mutex_unlock(&s->mutex);
}

static void itc_source_process(MSFilter *f){
	SourceState *s=(SourceState *)f->data;
	mblk_t *m;
	ms_mutex_lock(&s->mutex);
	while((m=ms_queue_get(&s->q))!=NULL){
		ms_mutex_unlock(&s->mutex);
		ms_queue_put(f->outputs[0],m);
		ms_mutex_lock(&s->mutex);
	}
	ms_mutex_unlock(&s->mutex);
}

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
	NULL
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
};

#endif

static void itc_sink_process(MSFilter *f){
	MSFilter *other=f->data;
	mblk_t *im;
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		itc_source_queue_packet(other,im);
	}
}

static int itc_sink_connect(MSFilter *f, void *data){
	f->data=data;
	return 0;
}

static MSFilterMethod sink_methods[]={
	{	MS_ITC_SINK_CONNECT , itc_sink_connect },
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
	NULL,
	NULL,
	itc_sink_process,
	NULL,
	NULL,
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
	.process=itc_sink_process,
	.methods=sink_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_itc_source_desc)
MS_FILTER_DESC_EXPORT(ms_itc_sink_desc)
