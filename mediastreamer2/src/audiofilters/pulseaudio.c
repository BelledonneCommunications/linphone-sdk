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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msinterfaces.h"

#include <pulse/pulseaudio.h>

#define PA_STRING_SIZE 256

typedef struct pa_device {
	char name[PA_STRING_SIZE];
	char description[PA_STRING_SIZE];
	char source_name[PA_STRING_SIZE];
	uint8_t bidirectionnal;
} pa_device_t;

struct _PAData{
	char *pa_id_sink;
	char *pa_id_source;
};

typedef struct _PAData PAData;

static int the_pa_ref = 0;
static pa_context *the_pa_context=NULL;
static pa_threaded_mainloop *the_pa_loop=NULL;
static const int targeted_latency = 20;/*ms*/

static void context_state_notify_cb(pa_context *ctx, void *userdata){
	const char *sname="";
	pa_context_state_t state=pa_context_get_state(ctx);
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
		break;
	case PA_CONTEXT_FAILED:
		sname="PA_CONTEXT_FAILED";
		break;
	case PA_CONTEXT_TERMINATED:
		sname="PA_CONTEXT_TERMINATED";
		break;
	}
	ms_message("New PulseAudio context state: %s",sname);
	pa_threaded_mainloop_signal(the_pa_loop, FALSE);
}

static bool_t wait_for_context_state(pa_context_state_t success_state, pa_context_state_t failure_state){
	pa_context_state_t state;
	pa_threaded_mainloop_lock(the_pa_loop);
	state = pa_context_get_state(the_pa_context);
	while(state != success_state && state != failure_state) {
		pa_threaded_mainloop_wait(the_pa_loop);
		state = pa_context_get_state(the_pa_context);
	}
	pa_threaded_mainloop_unlock(the_pa_loop);
	return state == success_state;
}


static void init_pulse_context(void){
	if (the_pa_ref == 0){
		the_pa_loop=pa_threaded_mainloop_new();
		the_pa_context=pa_context_new(pa_threaded_mainloop_get_api(the_pa_loop),NULL);
		pa_context_set_state_callback(the_pa_context,context_state_notify_cb,NULL);
		pa_context_connect(the_pa_context, NULL, 0, NULL);
		pa_threaded_mainloop_start(the_pa_loop);
	}
	the_pa_ref++;
}

static void uninit_pulse_context(void){
	the_pa_ref--;
	if (the_pa_ref == 0){
		pa_context_disconnect(the_pa_context);
		pa_context_unref(the_pa_context);
		pa_threaded_mainloop_stop(the_pa_loop);
		pa_threaded_mainloop_free(the_pa_loop);
		the_pa_context = NULL;
		the_pa_loop = NULL;
	}
}

static void pulse_card_unload(MSSndCardManager *m) {
	uninit_pulse_context();
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
	bctbx_list_t **pa_devicelist = userdata;
	pa_device_t *pa_device;

	/* when eol is set to a positive number : end of the list */
	if (eol > 0) {
		goto end;
	}

	pa_device = ms_malloc0(sizeof(pa_device_t));
	strncpy(pa_device->name, l->name, PA_STRING_SIZE-1);
	strncpy(pa_device->description, l->description, PA_STRING_SIZE-1);

	*pa_devicelist = bctbx_list_append(*pa_devicelist, pa_device);
end:
	pa_threaded_mainloop_signal(the_pa_loop, FALSE);
}

void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata) {
	bctbx_list_t **pa_devicelist = userdata;
	pa_device_t *pa_device;

	if (eol > 0) {
		goto end;
	}

	if (l->monitor_of_sink!=PA_INVALID_INDEX) { /* ignore monitors */
		goto end;
	}
	
	pa_device = ms_malloc0(sizeof(pa_device_t));
	strncpy(pa_device->name, l->name, PA_STRING_SIZE -1);
	strncpy(pa_device->description, l->description, PA_STRING_SIZE -1);

	*pa_devicelist = bctbx_list_append(*pa_devicelist, pa_device);
end:
	pa_threaded_mainloop_signal(the_pa_loop, FALSE);
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

static void pulse_card_merge_lists(pa_device_t *pa_device, bctbx_list_t **pa_source_list) {
	bctbx_list_t *sourceCard = bctbx_list_find_custom(*pa_source_list, (bctbx_compare_func)pulse_card_compare, pa_device); 
	if (sourceCard!= NULL) {
		pa_device_t *sourceCard_data = (pa_device_t *)sourceCard->data;
		pa_device->bidirectionnal = 1;
		strncpy(pa_device->source_name,sourceCard_data->name, PA_STRING_SIZE -1);
		*pa_source_list = bctbx_list_remove(*pa_source_list, sourceCard->data);
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
	bctbx_list_t *pa_sink_list=NULL;
	bctbx_list_t *pa_source_list=NULL;
	
	/* connect to pulse server */
	init_pulse_context();
	if(!wait_for_context_state(PA_CONTEXT_READY, PA_CONTEXT_FAILED)) {
		ms_error("Connection to the pulseaudio server failed");
		return;
	}

	pa_threaded_mainloop_lock(the_pa_loop);
	
	/* retrieve all available sinks */
	pa_op = pa_context_get_sink_info_list(the_pa_context, pa_sinklist_cb, &pa_sink_list);

	/* wait for the operation to complete */
	while (pa_operation_get_state(pa_op) != PA_OPERATION_DONE) {
		pa_threaded_mainloop_wait(the_pa_loop);
	}
	pa_operation_unref(pa_op);

	/* retrieve all available sources, monitors are ignored */
	pa_op = pa_context_get_source_info_list(the_pa_context, pa_sourcelist_cb, &pa_source_list);

	/* wait for the operation to complete */
	while (pa_operation_get_state(pa_op) != PA_OPERATION_DONE) {
		pa_threaded_mainloop_wait(the_pa_loop);
	}
	pa_operation_unref(pa_op);

	pa_threaded_mainloop_unlock(the_pa_loop);
	
	/* merge source list into sink list for dual capabilities cards */
	bctbx_list_for_each2(pa_sink_list, (MSIterate2Func)pulse_card_merge_lists, &pa_source_list);

	/* create sink and souce cards */
	bctbx_list_for_each2(pa_sink_list, (MSIterate2Func)pulse_card_sink_create, m);
	bctbx_list_for_each2(pa_source_list, (MSIterate2Func)pulse_card_source_create, m);

	bctbx_list_free_with_data(pa_sink_list, ms_free);
	bctbx_list_free_with_data(pa_source_list, ms_free);
}

typedef enum _StreamType {
	STREAM_TYPE_PLAYBACK,
	STREAM_TYPE_RECORD
} StreamType;

typedef struct _Stream{
	ms_mutex_t mutex;
	StreamType type;
	pa_sample_spec sampleSpec;
	pa_stream *stream;
	pa_stream_state_t state;
	MSBufferizer bufferizer;
	char *dev;
	double init_volume;
	uint64_t last_stats;
	uint64_t last_flowcontrol_op;
	int underflow_notifs;
	int overflow_notifs;
	size_t min_buffer_size;
}Stream;

static void stream_disconnect(Stream *s);

static void stream_state_notify_cb(pa_stream *p, void *userData) {
	Stream *ctx = (Stream *)userData;
	ctx->state = pa_stream_get_state(p);
	pa_threaded_mainloop_signal(the_pa_loop, 0);
}

static bool_t stream_wait_for_state(Stream *ctx, pa_stream_state_t successState, pa_stream_state_t failureState) {
	pa_threaded_mainloop_lock(the_pa_loop);
	while(ctx->state != successState && ctx->state != failureState) {
		pa_threaded_mainloop_wait(the_pa_loop);
	}
	pa_threaded_mainloop_unlock(the_pa_loop);
	return ctx->state == successState;
}

static Stream *stream_new(StreamType type) {
	Stream *s = ms_new0(Stream, 1);
	ms_mutex_init(&s->mutex, NULL);
	s->type = type;
	s->sampleSpec.format = PA_SAMPLE_S16LE;
	s->sampleSpec.channels=1;
	s->sampleSpec.rate=8000;
	s->state = PA_STREAM_UNCONNECTED;
	ms_bufferizer_init(&s->bufferizer);
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
	if (s->dev) ms_free(s->dev);
	ms_bufferizer_uninit(&s->bufferizer);
	ms_mutex_destroy(&s->mutex);
	ms_free(s);
}

static size_t stream_play(Stream *s, size_t nbytes) {
	size_t avail;

	if (nbytes == 0)
		return 0;

	avail = ms_bufferizer_get_avail(&s->bufferizer);
	if (avail > 0) {
		uint8_t *data;
		size_t buffer_size;
		if (nbytes > avail)
			nbytes = avail;
		data = ms_new(uint8_t, nbytes);
		ms_mutex_lock(&s->mutex);
		ms_bufferizer_read(&s->bufferizer, data, nbytes);
		buffer_size = ms_bufferizer_get_avail(&s->bufferizer);
		if(s->min_buffer_size == (size_t)-1 || buffer_size < s->min_buffer_size) {
			s->min_buffer_size = buffer_size;
		}
		ms_mutex_unlock(&s->mutex);
		pa_stream_write(s->stream, data, nbytes, ms_free, 0, PA_SEEK_RELATIVE);
	}
		
	
	return nbytes;
}

static void stream_write_request_cb(pa_stream *p, size_t nbytes, void *user_data) {
	Stream *s = (Stream *)user_data;
	stream_play(s, nbytes);
}

static void stream_buffer_overflow_notification(pa_stream *p, void *user_data) {
	Stream *st = (Stream*)user_data;
	st->overflow_notifs++;
}

static void stream_buffer_underflow_notification(pa_stream *p, void *user_data) {
	Stream *st = (Stream*)user_data;
	st->underflow_notifs++;
}

static double volume_to_scale(pa_volume_t volume) {
	return ((double)volume)/((double)PA_VOLUME_NORM);
}

static pa_volume_t scale_to_volume(double scale) {
	return scale * PA_VOLUME_NORM;
}

static bool_t stream_connect(Stream *s) {
	int err;
	pa_buffer_attr attr;
	pa_cvolume volume, *volume_ptr = NULL;
	
	s->min_buffer_size = (size_t)-1;
	
	attr.maxlength = -1;
	attr.fragsize = pa_usec_to_bytes(targeted_latency * 1000, &s->sampleSpec);
	attr.tlength = attr.fragsize;
	attr.minreq = -1;
	attr.prebuf = -1;
	
	if(s->init_volume >= 0.0) {
		pa_volume_t value = scale_to_volume(s->init_volume);
		volume_ptr = pa_cvolume_init(&volume);
		pa_cvolume_set(&volume, s->sampleSpec.channels, value);
	}
	
	
	if (the_pa_context==NULL) {
		ms_error("No PulseAudio context");
		return FALSE;
	}
	s->stream=pa_stream_new(the_pa_context,"phone",&s->sampleSpec,NULL);
	if (s->stream==NULL){
		ms_error("fails to create PulseAudio stream");
		return FALSE;
	}
	pa_stream_set_state_callback(s->stream, stream_state_notify_cb, s);
	pa_threaded_mainloop_lock(the_pa_loop);
	if(s->type == STREAM_TYPE_PLAYBACK) {
		pa_stream_set_write_callback(s->stream, stream_write_request_cb, s);
		pa_stream_set_overflow_callback(s->stream, stream_buffer_overflow_notification, s);
		pa_stream_set_underflow_callback(s->stream, stream_buffer_underflow_notification, s);
		err=pa_stream_connect_playback(
			s->stream,s->dev,&attr,
			PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING,
			volume_ptr, 
			NULL);
	} else {
		err=pa_stream_connect_record(s->stream,s->dev,&attr, PA_STREAM_ADJUST_LATENCY);
	}
	pa_threaded_mainloop_unlock(the_pa_loop);
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
		pa_threaded_mainloop_lock(the_pa_loop);
		err = pa_stream_disconnect(s->stream);
		pa_threaded_mainloop_unlock(the_pa_loop);
		if(err!=0 || !stream_wait_for_state(s, PA_STREAM_TERMINATED, PA_STREAM_FAILED)) {
			ms_error("pa_stream_disconnect() failed. err=%d", err);
		}
		pa_stream_unref(s->stream);
		s->stream = NULL;
		s->state = PA_STREAM_UNCONNECTED;
		s->init_volume = -1.0;
	}
}


static void stream_set_volume_cb(pa_context *c, int success, void *user_data) {
	*(int *)user_data = success;
	pa_threaded_mainloop_signal(the_pa_loop, FALSE);
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
	pa_cvolume_set(&cvolume, s->sampleSpec.channels, scale_to_volume(volume));
	pa_threaded_mainloop_lock(the_pa_loop);
	if(s->type == STREAM_TYPE_PLAYBACK) {
		op = pa_context_set_sink_input_volume(the_pa_context, idx, &cvolume, stream_set_volume_cb, &success);
	} else {
		op = pa_context_set_source_output_volume(the_pa_context, idx, &cvolume, stream_set_volume_cb, &success);
	}
	while(pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
		pa_threaded_mainloop_wait(the_pa_loop);
	}
	pa_threaded_mainloop_unlock(the_pa_loop);
	pa_operation_unref(op);
	return success;
}

static void stream_get_source_volume_cb(pa_context *c, const pa_source_output_info *i, int eol, void *user_data) {
	if(i) {
		*(double *)user_data = volume_to_scale(pa_cvolume_avg(&i->volume));
	}
	pa_threaded_mainloop_signal(the_pa_loop, FALSE);
}

static void stream_get_sink_volume_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *user_data) {
	if(i) {
		*(double *)user_data = volume_to_scale(pa_cvolume_avg(&i->volume));
	}
	pa_threaded_mainloop_signal(the_pa_loop, FALSE);
}

static bool_t stream_get_volume(Stream *s, double *volume) {
	uint32_t idx;
	pa_operation *op;
	
	if(s->stream == NULL) {
		ms_error("stream_get_volume(): no stream");
		return FALSE;
	}
	idx = pa_stream_get_index(s->stream);
	*volume = -1.0;
	pa_threaded_mainloop_lock(the_pa_loop);
	if(s->type == STREAM_TYPE_PLAYBACK) {
		op = pa_context_get_sink_input_info(the_pa_context, idx, stream_get_sink_volume_cb, volume);
	} else {
		op = pa_context_get_source_output_info(the_pa_context, idx, stream_get_source_volume_cb, volume);
	}
	while(pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
		pa_threaded_mainloop_wait(the_pa_loop);
	}
	pa_threaded_mainloop_unlock(the_pa_loop);
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
	pa_threaded_mainloop_lock(the_pa_loop);
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
	pa_threaded_mainloop_unlock(the_pa_loop);
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


static const int flow_control_op_interval = 1000;
static const int flow_control_threshold = 20; // ms

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
	s->last_stats = (uint64_t)-1;
	s->last_flowcontrol_op = (uint64_t)-1;
}

static void pulse_write_process(MSFilter *f){
	PlaybackStream *s=(PlaybackStream*)f->data;
	size_t nwritable;

	if(s->stream) {
		ms_mutex_lock(&s->mutex);
		ms_bufferizer_put_from_queue(&s->bufferizer,f->inputs[0]);
		ms_mutex_unlock(&s->mutex);
		
		pa_threaded_mainloop_lock(the_pa_loop);
		nwritable = pa_stream_writable_size(s->stream);
		stream_play(s, nwritable);
		pa_threaded_mainloop_unlock(the_pa_loop);
		
		if (s->last_stats == (uint64_t)-1) {
			s->last_stats = f->ticker->time;
		} else if (f->ticker->time - s->last_stats >= 5000) {
			pa_usec_t latency;
			int is_negative;
			int err;
			s->last_stats = f->ticker->time;
			pa_threaded_mainloop_lock(the_pa_loop);
			err = pa_stream_get_latency(s->stream, &latency, &is_negative);
			pa_threaded_mainloop_unlock(the_pa_loop);
			if (err == 0 && !is_negative) {
				ms_message("pulseaudio: latency is equal to %d ms", (int)(latency/1000L));
			}
			if (s->underflow_notifs || s->overflow_notifs){
				ms_warning("pulseaudio: there were %i underflows and %i overflows over last 5 seconds", s->underflow_notifs, s->overflow_notifs);
				s->underflow_notifs = 0;
				s->overflow_notifs = 0;
			}
		}
		
		if (s->last_flowcontrol_op == (uint64_t)-1) {
			s->last_flowcontrol_op = f->ticker->time;
		} else if((int)(f->ticker->time - s->last_flowcontrol_op) >= flow_control_op_interval) {
			size_t threshold_bytes = pa_usec_to_bytes(flow_control_threshold * 1000, &s->sampleSpec);
			ms_mutex_lock(&s->mutex);
			if(s->min_buffer_size >= threshold_bytes) {
				size_t nbytes_to_drop = s->min_buffer_size - threshold_bytes/4;
				ms_warning("pulseaudio: too much data waiting in the writing buffer. Droping %i bytes", (int)nbytes_to_drop);
				ms_bufferizer_skip_bytes(&s->bufferizer, nbytes_to_drop);
				s->min_buffer_size = (size_t)-1;
			}
			ms_mutex_unlock(&s->mutex);
			s->last_flowcontrol_op = f->ticker->time;
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
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &pulse_read_desc);
	Stream *s = (Stream *)f->data;
	s->dev = ms_strdup(card_data->pa_id_source);  /* add pulse audio card id to connect the stream to the correct card */
	return f;
}

static MSFilter *pulse_card_create_writer(MSSndCard *card) {
	PAData *card_data = (PAData *)card->data;
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &pulse_write_desc);
	Stream *s = (Stream *)f->data;
	s->dev = ms_strdup(card_data->pa_id_sink); /* add pulse audio card id to connect the stream to the correct card */
	return f;
}


