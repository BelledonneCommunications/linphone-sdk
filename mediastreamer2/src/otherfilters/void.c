/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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
#include "mediastreamer2/msticker.h"


struct VoidSourceState {
	int rate;
	int nchannels;
};

typedef struct VoidSourceState VoidSourceState;


static void void_source_init(MSFilter *f) {
	VoidSourceState *s = (VoidSourceState *)ms_new(VoidSourceState, 1);
	s->rate = 8000;
	s->nchannels = 1;
	f->data = s;
}

static void void_source_uninit(MSFilter *f) {
	ms_free(f->data);
}

static void void_source_process(MSFilter *f) {
	mblk_t *m;
	VoidSourceState *s = (VoidSourceState *)f->data;
	int nsamples;

	nsamples = (f->ticker->interval * s->rate) / 1000;
	m = allocb(nsamples * s->nchannels * 2, 0);
	memset(m->b_wptr, 0, nsamples * s->nchannels * 2);
	m->b_wptr += nsamples * s->nchannels * 2;
	ms_queue_put(f->outputs[0], m);
}

static int void_source_set_rate(MSFilter *f, void *arg) {
	VoidSourceState *s = (VoidSourceState *)f->data;
	s->rate = *((int*)arg);
	return 0;
}

static int void_source_get_rate(MSFilter *f, void *arg) {
	VoidSourceState *s = (VoidSourceState *)f->data;
	*((int *)arg) = s->rate;
	return 0;
}

static int void_source_set_nchannels(MSFilter *f, void *arg) {
	VoidSourceState *s = (VoidSourceState *)f->data;
	s->nchannels = *(int *)arg;
	return 0;
}

static int void_source_get_nchannels(MSFilter *f, void *arg) {
	VoidSourceState *s = (VoidSourceState *)f->data;
	*((int *)arg) = s->nchannels;
	return 0;
}

MSFilterMethod void_source_methods[] = {
	{ MS_FILTER_SET_SAMPLE_RATE, void_source_set_rate },
	{ MS_FILTER_GET_SAMPLE_RATE, void_source_get_rate },
	{ MS_FILTER_SET_NCHANNELS, void_source_set_nchannels },
	{ MS_FILTER_GET_NCHANNELS, void_source_get_nchannels },
	{ 0, NULL }
};

static void void_sink_process(MSFilter *f){
	mblk_t *im;
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		freemsg(im);
	}
}

#ifdef _MSC_VER

MSFilterDesc ms_void_source_desc={
	MS_VOID_SOURCE_ID,
	"MSVoidSource",
	N_("A filter that generates silence on its output (useful for beginning some graphs)."),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	void_source_init,
	NULL,
	void_source_process,
	NULL,
	void_source_uninit,
	void_source_methods,
	MS_FILTER_IS_PUMP
};

MSFilterDesc ms_void_sink_desc={
	MS_VOID_SINK_ID,
	"MSVoidSink",
	N_("A filter that trashes its input (useful for terminating some graphs)."),
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	NULL,
	NULL,
	void_sink_process,
	NULL,
	NULL
};

#else

MSFilterDesc ms_void_source_desc={
	.id=MS_VOID_SOURCE_ID,
	.name="MSVoidSource",
	.text=N_("A filter that generates silence on its output (useful for beginning some graphs)."),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=void_source_init,
	.process=void_source_process,
	.uninit=void_source_uninit,
	.methods=void_source_methods,
	.flags=MS_FILTER_IS_PUMP
};

MSFilterDesc ms_void_sink_desc={
	.id=MS_VOID_SINK_ID,
	.name="MSVoidSink",
	.text=N_("A filter that trashes its input (useful for terminating some graphs)."),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.process=void_sink_process,
};

#endif

MS_FILTER_DESC_EXPORT(ms_void_source_desc)
MS_FILTER_DESC_EXPORT(ms_void_sink_desc)
