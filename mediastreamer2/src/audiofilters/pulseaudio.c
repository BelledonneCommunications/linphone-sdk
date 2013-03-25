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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mssndcard.h"

#include <pulse/pulseaudio.h>

static const float latency_req=0.02;

static void init_pulse_context();
static void pulse_card_detect(MSSndCardManager *m);
static MSFilter *pulse_card_create_reader(MSSndCard *card);
static MSFilter *pulse_card_create_writer(MSSndCard *card);


static void pulse_card_init(MSSndCard *obj){
	
}

static void pulse_card_uninit(MSSndCard *obj){
	
}


MSSndCardDesc pulse_card_desc={
	.driver_type="PulseAudio",
	.detect=pulse_card_detect,
	.init=pulse_card_init,
	.create_reader=pulse_card_create_reader,
	.create_writer=pulse_card_create_writer,
	.uninit=pulse_card_uninit
};


static void pulse_card_detect(MSSndCardManager *m){
	MSSndCard *card=ms_snd_card_new(&pulse_card_desc);
	if (card!=NULL){
		card->name=ms_strdup("default");
		card->capabilities=MS_SND_CARD_CAP_CAPTURE|MS_SND_CARD_CAP_PLAYBACK;
		ms_snd_card_manager_add_card(m,card);
		init_pulse_context();
	}
}

static pa_context *context=NULL;
static pa_threaded_mainloop *pa_loop=NULL;
static bool_t pa_ready=FALSE;

static void uninit_pulse_context(){
	pa_context_disconnect(context);
	pa_context_unref(context);
	pa_threaded_mainloop_stop(pa_loop);
}

static void state_notify(pa_context *ctx, void *userdata){
	pa_context_state_t state=pa_context_get_state(ctx);
	const char *sname="";
	switch (state){
		case PA_CONTEXT_UNCONNECTED:
			sname="PA_CONTEXT_UNCONNECTED";
		break;
		case PA_CONTEXT_CONNECTING:
			sname="PA_CONTEXT_CONNECTING";
		break;
		case PA_CONTEXT_AUTHORIZING:
			sname="PA_CONTEXT_AUTHORIZING";
		break;
		case PA_CONTEXT_SETTING_NAME:
			sname="PA_CONTEXT_SETTING_NAME";
		break;
		case PA_CONTEXT_READY:
			sname="PA_CONTEXT_READY";
			pa_ready=TRUE;
		break;
		case PA_CONTEXT_FAILED:
			sname="PA_CONTEXT_FAILED";
		break;
		case PA_CONTEXT_TERMINATED:
			sname="PA_CONTEXT_TERMINATED";
		break;
	}
	ms_message("New PulseAudio context state: %s",sname);
}

static void init_pulse_context(){
	if (context==NULL){
		pa_loop=pa_threaded_mainloop_new();
		context=pa_context_new(pa_threaded_mainloop_get_api(pa_loop),NULL);
		pa_context_set_state_callback(context,state_notify,NULL);	
		pa_context_connect(context,NULL,0,NULL);
		pa_threaded_mainloop_start(pa_loop);
		atexit(uninit_pulse_context);
	}
}

static void check_pulse_ready(){
	int i=0;
	while(pa_ready==FALSE && i<10){
		usleep(20000);
	}
}

typedef struct _PulseReadState{
	int channels;
	int rate;
	pa_stream *stream;
	int fragsize;
}PulseReadState;

static void pulse_read_init(MSFilter *f){
	PulseReadState *s=ms_new0(PulseReadState,1);
	
	s->channels=1;
	s->rate=8000;
	f->data=s;
	check_pulse_ready();
}

static void pulse_read_preprocess(MSFilter *f){
	PulseReadState *s=(PulseReadState *)f->data;
	int err;
	pa_sample_spec pss;
	pa_buffer_attr attr;

	if (context==NULL) return;
	
	pss.format=PA_SAMPLE_S16LE;
	pss.channels=s->channels;
	pss.rate=s->rate;
	
	attr.maxlength=-1;
	attr.tlength=-1;
	attr.prebuf=-1;
	attr.minreq=-1;
	attr.fragsize=s->fragsize=latency_req*(float)s->channels*(float)s->rate*2;
	
	s->stream=pa_stream_new(context,"phone",&pss,NULL);
	if (s->stream==NULL){
		ms_error("pa_stream_new() failed: %s",pa_strerror(pa_context_errno(context)));
		return;
	}
	pa_threaded_mainloop_lock(pa_loop);
	err=pa_stream_connect_record(s->stream,NULL,&attr, PA_STREAM_ADJUST_LATENCY);
	pa_threaded_mainloop_unlock(pa_loop);
	if (err!=0){
		ms_error("pa_stream_connect_record() failed");
	}
}


static void pulse_read_process(MSFilter *f){
	PulseReadState *s=(PulseReadState *)f->data;
	const void *buffer=NULL;
	size_t nbytes=0;
	
	if (s->stream!=NULL){
		pa_threaded_mainloop_lock(pa_loop);
		while (pa_stream_peek(s->stream,&buffer,&nbytes)==0 && nbytes>0){
			mblk_t *om;
			om=allocb(nbytes,0);
			memcpy(om->b_wptr,buffer,nbytes);
			om->b_wptr+=nbytes;
			ms_queue_put(f->outputs[0],om);
			nbytes=0;
			pa_stream_drop(s->stream);
		}
		pa_threaded_mainloop_unlock(pa_loop);
	}
}


static void pulse_read_postprocess(MSFilter *f){
	PulseReadState *s=(PulseReadState *)f->data;
	
	if (s->stream) {
		pa_threaded_mainloop_lock(pa_loop);
		pa_stream_disconnect(s->stream);
		pa_stream_unref(s->stream);
		pa_threaded_mainloop_unlock(pa_loop);
	}
}

static int pulse_read_set_sr(MSFilter *f, void *arg){
	PulseReadState *s=(PulseReadState *)f->data;
	s->rate=*(int*)arg;
	return 0;
}

static int pulse_read_get_sr(MSFilter *f, void *arg){
	PulseReadState *s=(PulseReadState *)f->data;
	*(int*)arg=s->rate;
	return 0;
}

static int pulse_read_set_nchannels(MSFilter *f, void *arg){
	PulseReadState *s=(PulseReadState *)f->data;
	s->channels=*(int*)arg;
	return 0;
}

static int pulse_read_get_nchannels(MSFilter *f, void *arg){
	PulseReadState *s=(PulseReadState *)f->data;
	*(int*)arg=s->channels;
	return 0;
}

static MSFilterMethod pulse_read_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE , pulse_read_set_sr },
	{	MS_FILTER_GET_SAMPLE_RATE , pulse_read_get_sr },
	{	MS_FILTER_SET_NCHANNELS	, pulse_read_set_nchannels },
	{	MS_FILTER_GET_NCHANNELS	, pulse_read_get_nchannels },
	{	0	, 0 }
};

static MSFilterDesc pulse_read_desc={
	.id=MS_PULSE_READ_ID,
	.name="MSPulseRead",
	.text="Sound input plugin based on PulseAudio",
	.ninputs=0,
	.noutputs=1,
	.category=MS_FILTER_OTHER,
	.init=pulse_read_init,
	.preprocess=pulse_read_preprocess,
	.process=pulse_read_process,
	.postprocess=pulse_read_postprocess,
	.methods=pulse_read_methods
};


typedef struct _PulseReadState PulseWriteState;

static void pulse_write_init(MSFilter *f){
	PulseWriteState *s=ms_new0(PulseWriteState,1);
	s->rate=8000;
	s->channels=1;
	f->data=s;
	check_pulse_ready();
}

static void pulse_write_preprocess(MSFilter *f){
	PulseWriteState *s=(PulseWriteState*)f->data;
	int err;
	pa_sample_spec pss;
	pa_buffer_attr attr;

	if (context==NULL) return;
	
	pss.format=PA_SAMPLE_S16LE;
	pss.channels=s->channels;
	pss.rate=s->rate;

	s->fragsize=latency_req*(float)s->channels*(float)s->rate*2;
	
	attr.maxlength=-1;
	attr.tlength=s->fragsize;
	attr.prebuf=-1;
	attr.minreq=-1;
	attr.fragsize=-1;
	
	s->stream=pa_stream_new(context,"phone",&pss,NULL);
	if (s->stream==NULL){
		ms_error("pa_stream_new() failed: %s",pa_strerror(pa_context_errno(context)));
		return;
	}
	pa_threaded_mainloop_lock(pa_loop);
	err=pa_stream_connect_playback(s->stream,NULL,&attr, PA_STREAM_ADJUST_LATENCY,NULL,NULL);
	pa_threaded_mainloop_unlock(pa_loop);
	if (err!=0){
		ms_error("pa_stream_connect_playback() failed");
	}
}

static void pulse_write_process(MSFilter *f){
	PulseWriteState *s=(PulseWriteState*)f->data;
	mblk_t *im;
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		int bsize=msgdsize(im);
		if (s->stream){
			pa_threaded_mainloop_lock(pa_loop);
			if (pa_stream_writable_size(s->stream)>=bsize){
				//ms_message("Pushing data to pulseaudio");
				pa_stream_write(s->stream,im->b_rptr,bsize,NULL,0,PA_SEEK_RELATIVE);
			}
			pa_threaded_mainloop_unlock(pa_loop);
		}
		freemsg(im);
	}
}

static void pulse_write_postprocess(MSFilter *f){
	PulseWriteState *s=(PulseWriteState*)f->data;
	if (s->stream) {
		pa_threaded_mainloop_lock(pa_loop);
		pa_stream_disconnect(s->stream);
		pa_stream_unref(s->stream);
		pa_threaded_mainloop_unlock(pa_loop);
	}
}

static MSFilterDesc pulse_write_desc={
	.id=MS_PULSE_WRITE_ID,
	.name="MSPulseWrite",
	.text="Sound output plugin based on PulseAudio",
	.ninputs=1,
	.noutputs=0,
	.category=MS_FILTER_OTHER,
	.init=pulse_write_init,
	.preprocess=pulse_write_preprocess,
	.process=pulse_write_process,
	.postprocess=pulse_write_postprocess,
	.methods=pulse_read_methods
};

static MSFilter *pulse_card_create_reader(MSSndCard *card)
{
	return ms_filter_new_from_desc (&pulse_read_desc);
}

static MSFilter *pulse_card_create_writer(MSSndCard *card)
{
	return ms_filter_new_from_desc (&pulse_write_desc);
}

