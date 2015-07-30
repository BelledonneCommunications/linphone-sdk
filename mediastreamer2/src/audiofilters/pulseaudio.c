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
#include "mediastreamer2/msinterfaces.h"

#include <pulse/pulseaudio.h>

typedef struct pa_device {
	char name[512];
	char description[256];
	uint8_t bidirectionnal;
	char source_name[512];
} pa_device_t;

struct _PAData{
	char *pa_id_sink;
	char *pa_id_source;
};

typedef struct _PAData PAData;

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

static void pulse_card_init(MSSndCard *card){
	PAData *card_data=ms_new0(PAData,1);
	card->data=card_data;
}

static void pulse_card_uninit(MSSndCard *card){
	PAData *card_data=(PAData*)card->data;
	if (card_data->pa_id_sink!=NULL) ms_free(card_data->pa_id_sink);
	if (card_data->pa_id_source!=NULL) ms_free(card_data->pa_id_source);
	ms_free(card_data);
}

MSSndCardDesc pulse_card_desc={
	.driver_type="PulseAudio",
	.detect=pulse_card_detect,
	.init=pulse_card_init,
	.create_reader=pulse_card_create_reader,
	.create_writer=pulse_card_create_writer,
	.uninit=pulse_card_uninit,
	.unload=pulse_card_unload
};

void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata) {
	MSList **pa_devicelist = userdata;
	pa_device_t *pa_device = ms_malloc0(sizeof(pa_device_t));

	/* when eol is set to a positive number : end of the list */
	if (eol > 0) {
		return;
	}

	strncpy(pa_device->name, l->name, 511);
	strncpy(pa_device->description, l->description, 255);

	*pa_devicelist = ms_list_append(*pa_devicelist, pa_device);
}

void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata) {
	MSList **pa_devicelist = userdata;
	pa_device_t *pa_device;

	if (eol > 0) {
		return;
	}

	if (l->monitor_of_sink!=PA_INVALID_INDEX) { /* ignore monitors */
		return;
	}
	
	pa_device = ms_malloc0(sizeof(pa_device_t));
	strncpy(pa_device->name, l->name, 511);
	strncpy(pa_device->description, l->description, 255);

	*pa_devicelist = ms_list_append(*pa_devicelist, pa_device);
}

/* Add cards to card manager, list contains sink only and bidirectionnal cards */
static void pulse_card_sink_create(pa_device_t *pa_device, MSSndCardManager *m) {
	MSSndCard *card;
	PAData *card_data;

	card=ms_snd_card_new(&pulse_card_desc);
	if(card==NULL) {
		ms_error("Creating the pulseaudio soundcard failed");
		return;
	}
	card_data = (PAData*)card->data;
	card->name = ms_strdup(pa_device->description);
	card_data->pa_id_sink = ms_strdup(pa_device->name);
	/* check if this card also support capture */
	if (pa_device->bidirectionnal==1) {
		card->capabilities = MS_SND_CARD_CAP_PLAYBACK|MS_SND_CARD_CAP_CAPTURE;
		card_data->pa_id_source=strdup(pa_device->source_name);
	} else {
		card->capabilities = MS_SND_CARD_CAP_PLAYBACK;
	}
	ms_snd_card_manager_add_card(m, card);
}

/* Add cards to card manager, list contains source only cards */
static void pulse_card_source_create(pa_device_t *pa_device, MSSndCardManager *m) {
	MSSndCard *card;
	PAData *card_data;

	card=ms_snd_card_new(&pulse_card_desc);
	if(card==NULL) {
		ms_error("Creating the pulseaudio soundcard failed");
		return;
	}
	card_data = (PAData*)card->data;
	card->name = ms_strdup(pa_device->description);
	card->capabilities = MS_SND_CARD_CAP_CAPTURE;
	card_data->pa_id_source = strdup(pa_device->name);
	ms_snd_card_manager_add_card(m, card);
}

/* card source and sink list merging into sink list :
 * output: sink and bidirectionnal cards into sink list, source only card into source list
 * merging is based on pulse audio card description */
int pulse_card_compare(pa_device_t *sink, pa_device_t *source) {
	return strncmp(sink->description, source->description, 512);
}

static void pulse_card_merge_lists(pa_device_t *pa_device, MSList **pa_source_list) {
	MSList *sourceCard = ms_list_find_custom(*pa_source_list, (MSCompareFunc)pulse_card_compare, pa_device); 
	if (sourceCard!= NULL) {
		pa_device_t *sourceCard_data = (pa_device_t *)sourceCard->data;
		pa_device->bidirectionnal = 1;
		strncpy(pa_device->source_name,sourceCard_data->name, 511);
		*pa_source_list = ms_list_remove(*pa_source_list, sourceCard->data);
		ms_free(sourceCard_data);	
	}
}

/** Card detection:
 * - retrieve all sinks, then all sources.
 * - merge dual capabilities card into the sink list(merging is based on card description which may cause problem when two cards with the same description are connected - untested case)
 * - create sinks and dual capabilities cards
 * - create source only cards
 **/
static void pulse_card_detect(MSSndCardManager *m){
	pa_operation *pa_op;
	MSList *pa_sink_list=NULL;
	MSList *pa_source_list=NULL;
	
	/* connect to pulse server */
	init_pulse_context();
	if(!wait_for_context_state(PA_CONTEXT_READY, PA_CONTEXT_FAILED)) {
		ms_error("Connection to the pulseaudio server failed");
		return;
	}

	/* retrieve all available sinks */
	pa_op = pa_context_get_sink_info_list(context, pa_sinklist_cb, &pa_sink_list);

	/* wait for the operation to complete */
	while (pa_operation_get_state(pa_op) != PA_OPERATION_DONE) {
		pa_threaded_mainloop_wait(pa_loop);
	}
	pa_operation_unref(pa_op);

	/* retrieve all available sources, monitors are ignored */
	pa_op = pa_context_get_source_info_list(context, pa_sourcelist_cb, &pa_source_list);

	/* wait for the operation to complete */
	while (pa_operation_get_state(pa_op) != PA_OPERATION_DONE) {
		pa_threaded_mainloop_wait(pa_loop);
	}
	pa_operation_unref(pa_op);

	/* merge source list into sink list for dual capabilities cards */
	ms_list_for_each2(pa_sink_list, (void (*)(void*,void*))pulse_card_merge_lists, pa_source_list);

	/* create sink and souce cards */
	ms_list_for_each2(pa_sink_list, (void (*)(void*,void*))pulse_card_sink_create, m);
	ms_list_for_each2(pa_source_list, (void (*)(void*,void*))pulse_card_source_create, m);

	ms_list_free_with_data(pa_sink_list, ms_free);
	ms_list_free_with_data(pa_source_list, ms_free);
}

typedef enum _StreamType {
	STREAM_TYPE_PLAYBACK,
	STREAM_TYPE_RECORD
} StreamType;

typedef struct _Stream{
	StreamType type;
	pa_sample_spec sampleSpec;
	pa_stream *stream;
	pa_stream_state_t state;
	MSQueue queue;
	char *dev;
	double init_volume;
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

static Stream *stream_new(StreamType type) {
	Stream *s = ms_new0(Stream, 1);
	s->type = type;
	s->sampleSpec.format = PA_SAMPLE_S16LE;
	s->sampleSpec.channels=1;
	s->sampleSpec.rate=8000;
	s->state = PA_STREAM_UNCONNECTED;
	ms_queue_init(&s->queue);
	s->dev = NULL;
	s->init_volume = -1.0;
	return s;
}

static void stream_free(Stream *s) {
	if(s->stream) {
		stream_disconnect(s);
		if (s->dev) {
			ms_free(s->dev);
		}
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

static bool_t stream_connect(Stream *s) {
	int err;
	pa_buffer_attr attr;
	pa_cvolume volume, *volume_ptr = NULL;
	
	attr.maxlength = -1;
	attr.fragsize = pa_usec_to_bytes(targeted_latency * 1000, &s->sampleSpec);
	attr.tlength = attr.fragsize;
	attr.minreq = -1;
	attr.prebuf = -1;
	
	if(s->init_volume >= 0.0) {
		pa_volume_t value = pa_sw_volume_from_linear(s->init_volume);
		volume_ptr = pa_cvolume_init(&volume);
		pa_cvolume_set(&volume, s->sampleSpec.channels, value);
	}
	
	
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
	if(s->type == STREAM_TYPE_PLAYBACK) {
		pa_stream_set_write_callback(s->stream, stream_write_request_cb, s);
		pa_stream_set_overflow_callback(s->stream, stream_buffer_overflow_notification, NULL);
		pa_stream_set_underflow_callback(s->stream, stream_buffer_underflow_notification, NULL);
		err=pa_stream_connect_playback(
			s->stream,s->dev,&attr,
			PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING,
			volume_ptr, 
			NULL);
	} else {
		err=pa_stream_connect_record(s->stream,s->dev,&attr, PA_STREAM_ADJUST_LATENCY);
	}
	pa_threaded_mainloop_unlock(pa_loop);
	if(err < 0 || !stream_wait_for_state(s, PA_STREAM_READY, PA_STREAM_FAILED)) {
		ms_error("Fails to connect pulseaudio stream. err=%d", err);
		pa_stream_unref(s->stream);
		s->stream = NULL;
		return FALSE;
	}
	ms_message("pulseaudio %s stream connected (%dHz, %dch)",
	           s->type == STREAM_TYPE_PLAYBACK ? "playback" : "record",
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
		ms_queue_flush(&s->queue);
		s->state = PA_STREAM_UNCONNECTED;
		s->init_volume = -1.0;
	}
}

static void stream_set_volume_cb(pa_context *c, int success, void *user_data) {
	*(int *)user_data = success;
	pa_threaded_mainloop_signal(pa_loop, FALSE);
}

static bool_t stream_set_volume(Stream *s, double volume) {
	pa_cvolume cvolume;
	uint32_t idx;
	pa_operation *op;
	int success;
	
	if(s->stream == NULL) {
		if(s->type == STREAM_TYPE_PLAYBACK) {
			ms_error("stream_set_volume(): no stream");
			return FALSE;
		} else {
			s->init_volume = volume;
			return TRUE;
		}
	}
	idx = pa_stream_get_index(s->stream);
	pa_cvolume_init(&cvolume);
	pa_cvolume_set(&cvolume, s->sampleSpec.channels, pa_sw_volume_from_linear(volume));
	pa_threaded_mainloop_lock(pa_loop);
	if(s->type == STREAM_TYPE_PLAYBACK) {
		op = pa_context_set_sink_input_volume(context, idx, &cvolume, stream_set_volume_cb, &success);
	} else {
		op = pa_context_set_source_output_volume(context, idx, &cvolume, stream_set_volume_cb, &success);
	}
	while(pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
		pa_threaded_mainloop_wait(pa_loop);
	}
	pa_threaded_mainloop_unlock(pa_loop);
	pa_operation_unref(op);
	return success;
}

static void stream_get_source_volume_cb(pa_context *c, const pa_source_output_info *i, int eol, void *user_data) {
	if(i) {
		*(double *)user_data = pa_sw_volume_to_linear(*i->volume.values);
	} else {
		ms_error("stream_get_source_volume_cb(): no source info");
		*(double *)user_data = -1.0;
	}
	pa_threaded_mainloop_signal(pa_loop, FALSE);
}

static void stream_get_sink_volume_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *user_data) {
	if(i) {
		*(double *)user_data = pa_sw_volume_to_linear(*i->volume.values);
	} else {
		ms_error("stream_get_sink_volume_cb(): no source info");
		*(double *)user_data = -1.0;
	}
	pa_threaded_mainloop_signal(pa_loop, FALSE);
}

static bool_t stream_get_volume(Stream *s, double *volume) {
	uint32_t idx;
	pa_operation *op;
	
	if(s->stream == NULL) {
		ms_error("stream_get_volume(): no stream");
		return FALSE;
	}
	idx = pa_stream_get_index(s->stream);
	pa_threaded_mainloop_lock(pa_loop);
	if(s->type == STREAM_TYPE_PLAYBACK) {
		op = pa_context_get_sink_input_info(context, idx, stream_get_sink_volume_cb, volume);
	} else {
		op = pa_context_get_source_output_info(context, idx, stream_get_source_volume_cb, volume);
	}
	while(pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
		pa_threaded_mainloop_wait(pa_loop);
	}
	pa_threaded_mainloop_unlock(pa_loop);
	pa_operation_unref(op);
	return TRUE;
}

typedef Stream RecordStream;

static void pulse_read_init(MSFilter *f){
	if(!wait_for_context_state(PA_CONTEXT_READY, PA_CONTEXT_FAILED)) {
		ms_error("Could not connect to a pulseaudio server");
		return;
	}
	f->data = stream_new(STREAM_TYPE_RECORD);
}

static void pulse_read_preprocess(MSFilter *f) {
	RecordStream *s=(RecordStream *)f->data;
	if(!stream_connect(s)) {
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

static int pulse_read_set_volume(MSFilter *f, void *arg) {
	Stream *s = (Stream *)f->data;
	const float *volume = (const float *)arg;
	bool_t success;
	ms_filter_lock(f);
	success = stream_set_volume(s, *volume);
	ms_filter_unlock(f);
	return success ? 0 : -1;
}

static int pulse_read_get_volume(MSFilter *f, void *arg) {
	Stream *s = (Stream *)f->data;
	bool_t success;
	double volume;
	
	ms_filter_lock(f);
	success = stream_get_volume(s, &volume);
	ms_filter_unlock(f);
	if(success) *(float *)arg = (float)volume;
	return  success ? 0 : -1;
}

static MSFilterMethod pulse_read_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE , pulse_read_set_sr },
	{	MS_FILTER_GET_SAMPLE_RATE , pulse_read_get_sr },
	{	MS_FILTER_SET_NCHANNELS	, pulse_read_set_nchannels },
	{	MS_FILTER_GET_NCHANNELS	, pulse_read_get_nchannels },
	{	MS_AUDIO_CAPTURE_SET_VOLUME_GAIN	, pulse_read_set_volume },
	{	MS_AUDIO_CAPTURE_GET_VOLUME_GAIN	, pulse_read_get_volume },
	{	0	, NULL }
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
	f->data = stream_new(STREAM_TYPE_PLAYBACK);
}

static void pulse_write_preprocess(MSFilter *f) {
	PlaybackStream *s=(PlaybackStream*)f->data;
	if(!stream_connect(s)) {
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

static MSFilterMethod pulse_write_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE , pulse_read_set_sr },
	{	MS_FILTER_GET_SAMPLE_RATE , pulse_read_get_sr },
	{	MS_FILTER_SET_NCHANNELS	, pulse_read_set_nchannels },
	{	MS_FILTER_GET_NCHANNELS	, pulse_read_get_nchannels },
	{	MS_AUDIO_PLAYBACK_SET_VOLUME_GAIN	, pulse_read_set_volume },
	{	MS_AUDIO_PLAYBACK_GET_VOLUME_GAIN	, pulse_read_get_volume },
	{	0	, NULL }
};

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
	.methods=pulse_write_methods
};

static MSFilter *pulse_card_create_reader(MSSndCard *card) {
	PAData *card_data = (PAData *)card->data;
	MSFilter *f=ms_filter_new_from_desc (&pulse_read_desc);
	Stream *s = (Stream *)f->data;
	s->dev = ms_strdup(card_data->pa_id_source);  /* add pulse audio card id to connect the stream to the correct card */
	return f;
}

static MSFilter *pulse_card_create_writer(MSSndCard *card) {
	PAData *card_data = (PAData *)card->data;
	MSFilter *f=ms_filter_new_from_desc (&pulse_write_desc);
	Stream *s = (Stream *)f->data;
	s->dev = ms_strdup(card_data->pa_id_sink); /* add pulse audio card id to connect the stream to the correct card */
	return f;
}

