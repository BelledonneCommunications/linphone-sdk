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
#include "mediastreamer2/msticker.h"

#include <pulse/pulseaudio.h>

static pa_context *context=NULL;
static pa_context_state_t contextState = PA_CONTEXT_UNCONNECTED;
static pa_threaded_mainloop *pa_loop=NULL;
static const int targeted_latency = 20;/*ms*/

static void context_state_notify_cb(pa_context *ctx, void *userdata){
	const char *sname="";
	contextState=pa_context_get_state(ctx);
	switch (contextState){
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
		break;
	case PA_CONTEXT_FAILED:
		sname="PA_CONTEXT_FAILED";
		break;
	case PA_CONTEXT_TERMINATED:
		sname="PA_CONTEXT_TERMINATED";
		break;
	}
	ms_message("New PulseAudio context state: %s",sname);
	pa_threaded_mainloop_signal(pa_loop, FALSE);
}

static bool_t wait_for_context_state(pa_context_state_t successState, pa_context_state_t failureState){
	pa_threaded_mainloop_lock(pa_loop);
	while(contextState != successState && contextState != failureState) {
		pa_threaded_mainloop_wait(pa_loop);
	}
	pa_threaded_mainloop_unlock(pa_loop);
	return contextState == successState;
}

static void init_pulse_context(){
	if (context==NULL){
		pa_loop=pa_threaded_mainloop_new();
		context=pa_context_new(pa_threaded_mainloop_get_api(pa_loop),NULL);
		pa_context_set_state_callback(context,context_state_notify_cb,NULL);
		pa_context_connect(context, NULL, 0, NULL);
		pa_threaded_mainloop_start(pa_loop);
	}
}

static void uninit_pulse_context(){
	pa_context_disconnect(context);
	pa_context_unref(context);
	pa_threaded_mainloop_stop(pa_loop);
	pa_threaded_mainloop_free(pa_loop);
	context = NULL;
	pa_loop = NULL;
}

static void pulse_card_unload(MSSndCardManager *m) {
	if(context) uninit_pulse_context();
}

static void pulse_card_detect(MSSndCardManager *m);
static MSFilter *pulse_card_create_reader(MSSndCard *card);
static MSFilter *pulse_card_create_writer(MSSndCard *card);

MSSndCardDesc pulse_card_desc={
	.driver_type="PulseAudio",
	.detect=pulse_card_detect,
	.init=NULL,
	.create_reader=pulse_card_create_reader,
	.create_writer=pulse_card_create_writer,
	.uninit=NULL,
	.unload=pulse_card_unload
};

static void pulse_card_detect(MSSndCardManager *m){
	MSSndCard *card;
	
	init_pulse_context();
	if(!wait_for_context_state(PA_CONTEXT_READY, PA_CONTEXT_FAILED)) {
		ms_error("Connection to the pulseaudio server failed");
		return;
	}
	card=ms_snd_card_new(&pulse_card_desc);
	if(card==NULL) {
		ms_error("Creating the pulseaudio soundcard failed");
		return;
	}
	card->name=ms_strdup("default");
	card->capabilities = MS_SND_CARD_CAP_CAPTURE | MS_SND_CARD_CAP_PLAYBACK;
	ms_snd_card_manager_add_card(m, card);
}

typedef enum _StreamType {
	STREAM_TYPE_PLAYBACK,
	STREAM_TYPE_RECORD
} StreamType;

typedef struct _Stream{
	pa_sample_spec sampleSpec;
	pa_stream *stream;
	pa_stream_state_t state;
	MSQueue queue;
}Stream;

static void stream_disconnect(Stream *s);

static void stream_state_notify_cb(pa_stream *p, void *userData) {
	Stream *ctx = (Stream *)userData;
	ctx->state = pa_stream_get_state(p);
	pa_threaded_mainloop_signal(pa_loop, 0);
}

static bool_t stream_wait_for_state(Stream *ctx, pa_stream_state_t successState, pa_stream_state_t failureState) {
	pa_threaded_mainloop_lock(pa_loop);
	while(ctx->state != successState && ctx->state != failureState) {
		pa_threaded_mainloop_wait(pa_loop);
	}
	pa_threaded_mainloop_unlock(pa_loop);
	return ctx->state == successState;
}

static Stream *stream_new(void) {
	Stream *s = ms_new0(Stream, 1);
	s->sampleSpec.format = PA_SAMPLE_S16LE;
	s->sampleSpec.channels=1;
	s->sampleSpec.rate=8000;
	s->state = PA_STREAM_UNCONNECTED;
	ms_queue_init(&s->queue);
	return s;
}

static void stream_free(Stream *s) {
	if(s->stream) {
		stream_disconnect(s);
	}
	ms_queue_flush(&s->queue);
	ms_free(s);
}

static size_t stream_play(Stream *s, size_t nbytes) {
	uint8_t *data;
	uint8_t *wptr;
	size_t nread_glob = 0;
	
	if(nbytes == 0) return 0;
	
	data = ms_new(uint8_t, nbytes);
	wptr = data;
	while(!ms_queue_empty(&s->queue) && nread_glob < nbytes) {
		mblk_t *m = ms_queue_peek_first(&s->queue);
		size_t ntoread = nbytes - nread_glob;
		size_t nread = readmsg(m, ntoread, wptr);
		nread_glob += nread;
		wptr += nread;
		if(msgdsize(m) == 0) {
			ms_queue_remove(&s->queue, m);
		}
	}
	
	if(nread_glob > 0) {
		pa_stream_write(s->stream, data, nread_glob, ms_free, 0, PA_SEEK_RELATIVE);
	} else {
		ms_free(data);
	}
	
	return nread_glob;
}

static void stream_write_request_cb(pa_stream *p, size_t nbytes, void *user_data) {
	Stream *s = (Stream *)user_data;
	stream_play(s, nbytes);
}

static void stream_buffer_overflow_notification(pa_stream *p, void *user_data) {
	ms_warning("pulseaudio: playback buffer overflowed");
}

static void stream_buffer_underflow_notification(pa_stream *p, void *user_data) {
	ms_warning("pulseaudio: playback buffer underflow");
}

static bool_t stream_connect(Stream *s, StreamType type) {
	int err;
	
	pa_buffer_attr attr;
	attr.maxlength = -1;
	attr.fragsize = pa_usec_to_bytes(targeted_latency * 1000, &s->sampleSpec);
	attr.tlength = attr.fragsize;
	attr.minreq = -1;
	attr.prebuf = -1;
	
	if (context==NULL) {
		ms_error("No PulseAudio context");
		return FALSE;
	}
	s->stream=pa_stream_new(context,"phone",&s->sampleSpec,NULL);
	if (s->stream==NULL){
		ms_error("fails to create PulseAudio stream");
		return FALSE;
	}
	pa_stream_set_state_callback(s->stream, stream_state_notify_cb, s);
	pa_threaded_mainloop_lock(pa_loop);
	if(type == STREAM_TYPE_PLAYBACK) {
		pa_stream_set_write_callback(s->stream, stream_write_request_cb, s);
		pa_stream_set_overflow_callback(s->stream, stream_buffer_overflow_notification, NULL);
		pa_stream_set_underflow_callback(s->stream, stream_buffer_underflow_notification, NULL);
		err=pa_stream_connect_playback(
			s->stream,NULL,&attr,
			PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING,
			NULL, 
			NULL);
	} else {
		err=pa_stream_connect_record(s->stream,NULL,&attr, PA_STREAM_ADJUST_LATENCY);
	}
	pa_threaded_mainloop_unlock(pa_loop);
	if(err < 0 || !stream_wait_for_state(s, PA_STREAM_READY, PA_STREAM_FAILED)) {
		ms_error("Fails to connect pulseaudio stream. err=%d", err);
		pa_stream_unref(s->stream);
		s->stream = NULL;
		return FALSE;
	}
	ms_message("pulseaudio %s stream connected (%dHz, %dch)",
	           type == STREAM_TYPE_PLAYBACK ? "playback" : "record",
	           s->sampleSpec.rate,
	           s->sampleSpec.channels);
			
	return TRUE;
}

static void stream_disconnect(Stream *s) {
	int err;
	if (s->stream) {
		pa_threaded_mainloop_lock(pa_loop);
		err = pa_stream_disconnect(s->stream);
		pa_threaded_mainloop_unlock(pa_loop);
		if(err!=0 || !stream_wait_for_state(s, PA_STREAM_TERMINATED, PA_STREAM_FAILED)) {
			ms_error("pa_stream_disconnect() failed. err=%d", err);
		}
		pa_stream_unref(s->stream);
		s->stream = NULL;
	}
}

typedef Stream RecordStream;

static void pulse_read_init(MSFilter *f){
	if(!wait_for_context_state(PA_CONTEXT_READY, PA_CONTEXT_FAILED)) {
		ms_error("Could not connect to a pulseaudio server");
		return;
	}
	f->data = stream_new();
}

static void pulse_read_preprocess(MSFilter *f) {
	RecordStream *s=(RecordStream *)f->data;
	if(!stream_connect(s, STREAM_TYPE_RECORD)) {
		ms_error("Pulseaudio: fail to connect record stream");
	}
}

static void pulse_read_process(MSFilter *f){
	RecordStream *s=(RecordStream *)f->data;
	const void *buffer=NULL;
	size_t nbytes;

	if(s->stream == NULL) {
		ms_error("Record stream not connected");
		return;
	}
	pa_threaded_mainloop_lock(pa_loop);
	while(pa_stream_readable_size(s->stream) > 0) {
		if(pa_stream_peek(s->stream, &buffer, &nbytes) >= 0) {
			if(buffer != NULL) {
				mblk_t *om = allocb(nbytes, 0);
				memcpy(om->b_wptr, buffer, nbytes);
				om->b_wptr += nbytes;
				ms_queue_put(f->outputs[0], om);
			}
			if(nbytes > 0) {
				pa_stream_drop(s->stream);
			}
		} else {
			ms_error("pa_stream_peek() failed");
			break;
		}
	}
	pa_threaded_mainloop_unlock(pa_loop);
}

static void pulse_read_postprocess(MSFilter *f) {
	RecordStream *s=(RecordStream *)f->data;
	stream_disconnect(s);
}

static void pulse_read_uninit(MSFilter *f) {
	stream_free((Stream *)f->data);
}

static int pulse_read_set_sr(MSFilter *f, void *arg){
	RecordStream *s = (RecordStream *)f->data;
	
	if(s->state == PA_STREAM_READY) {
		ms_error("pulseaudio: cannot set sample rate: stream is connected");
		return -1;
	}
	
	s->sampleSpec.rate = *(int *)arg;
	return 0;
}

static int pulse_read_get_sr(MSFilter *f, void *arg) {
	RecordStream *s = (RecordStream *)f->data;
	*(int *)arg = s->sampleSpec.rate;
	return 0;
}

static int pulse_read_set_nchannels(MSFilter *f, void *arg){
	RecordStream *s = (RecordStream *)f->data;
	
	if(s->state == PA_STREAM_READY) {
		ms_error("pulseaudio: cannot set channels number: stream is connected");
		return -1;
	}
	
	s->sampleSpec.channels = *(int *)arg;
	return 0;
}

static int pulse_read_get_nchannels(MSFilter *f, void *arg){
	RecordStream *s = (RecordStream *)f->data;
	*(int *)arg = s->sampleSpec.channels;
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
	.uninit=pulse_read_uninit,
	.methods=pulse_read_methods
};


typedef Stream PlaybackStream;

static void pulse_write_init(MSFilter *f){
	if(!wait_for_context_state(PA_CONTEXT_READY, PA_CONTEXT_FAILED)) {
		ms_error("Could not connect to a pulseaudio server");
		return;
	}
	f->data = stream_new();
}

static void pulse_write_preprocess(MSFilter *f) {
	PlaybackStream *s=(PlaybackStream*)f->data;
	if(!stream_connect(s, STREAM_TYPE_PLAYBACK)) {
		ms_error("Pulseaudio: fail to connect playback stream");
	}
}

static void pulse_write_process(MSFilter *f){
	PlaybackStream *s=(PlaybackStream*)f->data;
	size_t nwritable;

	if(s->stream) {
		mblk_t *im;
		while((im=ms_queue_get(f->inputs[0]))) {
			pa_threaded_mainloop_lock(pa_loop);
			ms_queue_put(&s->queue, im);
			pa_threaded_mainloop_unlock(pa_loop);
		}
		
		pa_threaded_mainloop_lock(pa_loop);
		nwritable = pa_stream_writable_size(s->stream);
		stream_play(s, nwritable);
		pa_threaded_mainloop_unlock(pa_loop);

		if(f->ticker->time % 5000 == 0) {
			pa_usec_t latency;
			int is_negative;
			int err;
			pa_threaded_mainloop_lock(pa_loop);
			err = pa_stream_get_latency(s->stream, &latency, &is_negative);
			pa_threaded_mainloop_unlock(pa_loop);
			if(err == 0 && !is_negative) {
				ms_message("pulseaudio: latency is equal to %d ms", (int)(latency/1000L));
			}
		}
	} else {
		ms_queue_flush(f->inputs[0]);
	}
}

static void pulse_write_postprocess(MSFilter *f) {
	PlaybackStream *s=(PlaybackStream*)f->data;
	stream_disconnect(s);
}

static void pulse_write_uninit(MSFilter *f) {
	stream_free((Stream *)f->data);
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
	.uninit=pulse_write_uninit,
	.methods=pulse_read_methods
};

static MSFilter *pulse_card_create_reader(MSSndCard *card) {
	return ms_filter_new_from_desc (&pulse_read_desc);
}

static MSFilter *pulse_card_create_writer(MSSndCard *card) {
	return ms_filter_new_from_desc (&pulse_write_desc);
}

