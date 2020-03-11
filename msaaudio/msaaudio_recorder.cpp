/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
 *
 * aaudio_recorder.cpp - Android Media Recorder plugin for Linphone, based on AAudio APIs.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <mediastreamer2/msjava.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/android_utils.h>

#include <aaudio/AAudio.h>
#include <msaaudio/msaaudio.h>

struct AAudioInputContext {
	AAudioInputContext() {
		qinit(&q);
		ms_mutex_init(&mutex, NULL);
		ms_mutex_init(&stream_mutex, NULL);
		mTickerSynchronizer = NULL;
		mAvSkew = 0;
		deviceId = AAUDIO_UNSPECIFIED;
		session_id = AAUDIO_SESSION_ID_NONE;
		soundCard = NULL;
		aec = NULL;
	}

	~AAudioInputContext() {
		flushq(&q,0);
		ms_mutex_destroy(&mutex);
		ms_mutex_destroy(&stream_mutex);
	}
	
	void setContext(AAudioContext *context) {
		aaudio_context = context;
	}
	
	AAudioContext *aaudio_context;
	AAudioStream *stream;
	ms_mutex_t stream_mutex;

	queue_t q;
	ms_mutex_t mutex;
	MSTickerSynchronizer *mTickerSynchronizer;
	MSSndCard *soundCard;
	MSFilter *mFilter;
	int64_t read_samples;
	int32_t samplesPerFrame;
	double mAvSkew;
	int32_t deviceId;
	aaudio_session_id_t session_id;
	jobject aec;
};

static AAudioInputContext* aaudio_input_context_init() {
	AAudioInputContext* ictx = new AAudioInputContext();
	return ictx;
}

static void android_snd_read_init(MSFilter *obj) {
	AAudioInputContext *ictx = aaudio_input_context_init();
	obj->data = ictx;	
}

static aaudio_data_callback_result_t aaudio_recorder_callback(AAudioStream *stream, void *userData, void *audioData, int32_t numFrames) {
	AAudioInputContext *ictx = (AAudioInputContext *)userData;

	if (ictx->mTickerSynchronizer == NULL) {
		MSFilter *obj = ictx->mFilter;
		ictx->mTickerSynchronizer = ms_ticker_synchronizer_new();
		ms_ticker_set_synchronizer(obj->ticker, ictx->mTickerSynchronizer);
	}
	ictx->read_samples += numFrames * ictx->samplesPerFrame;

	if (numFrames <= 0) {
		ms_error("[AAudio] aaudio_recorder_callback has %i frames", numFrames);
	}

	int32_t bufferSize = sizeof(int16_t) * numFrames * ictx->samplesPerFrame;
	mblk_t *m = allocb(bufferSize, 0);
	memcpy(m->b_wptr, audioData, bufferSize);
	m->b_wptr += bufferSize;

	ms_mutex_lock(&ictx->mutex);
	ictx->mAvSkew = ms_ticker_synchronizer_update(ictx->mTickerSynchronizer, ictx->read_samples, (unsigned int)ictx->aaudio_context->samplerate);
	putq(&ictx->q, m);
	ms_mutex_unlock(&ictx->mutex);

	return AAUDIO_CALLBACK_RESULT_CONTINUE;	
}

static void aaudio_recorder_callback_error(AAudioStream *stream, void *userData, aaudio_result_t error);

static void aaudio_recorder_init(AAudioInputContext *ictx) {
	AAudioStreamBuilder *builder;
	aaudio_result_t result = AAudio_createStreamBuilder(&builder);
	if (result != AAUDIO_OK && !builder) {
		ms_error("[AAudio] Couldn't create stream builder for recorder: %i / %s", result, AAudio_convertResultToText(result));
	}

	AAudioStreamBuilder_setDeviceId(builder, ictx->deviceId);
	AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
	AAudioStreamBuilder_setSampleRate(builder, ictx->aaudio_context->samplerate);
	AAudioStreamBuilder_setDataCallback(builder, aaudio_recorder_callback, ictx);
	AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
	AAudioStreamBuilder_setChannelCount(builder, ictx->aaudio_context->nchannels);
	AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE); // If EXCLUSIVE mode isn't available the builder will fall back to SHARED mode.
	AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	AAudioStreamBuilder_setErrorCallback(builder, aaudio_recorder_callback_error, ictx);
	AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION); // Requires NDK build target of 28 instead of 26 !
	AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_VOICE_COMMUNICATION); // Requires NDK build target of 28 instead of 26 !
	AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_SPEECH); // Requires NDK build target of 28 instead of 26 !

	if (ictx->soundCard->capabilities & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER) {
		ms_message("[AAudio] Asking for a session ID so we can use echo canceller");
		AAudioStreamBuilder_setSessionId(builder, AAUDIO_SESSION_ID_ALLOCATE);
	}

	ms_message("[AAudio] Record stream configured with samplerate %i and %i channels", ictx->aaudio_context->samplerate, ictx->aaudio_context->nchannels);
	
	result = AAudioStreamBuilder_openStream(builder, &(ictx->stream));
	if (result != AAUDIO_OK && !ictx->stream) {
		ms_error("[AAudio] Open stream for recorder failed: %i / %s", result, AAudio_convertResultToText(result));
		AAudioStreamBuilder_delete(builder);
		return;
	} else {
		ms_message("[AAudio] Recorder stream opened");
	}

	int32_t framesPerBust = AAudioStream_getFramesPerBurst(ictx->stream);
	// Set the buffer size to the burst size - this will give us the minimum possible latency
	AAudioStream_setBufferSizeInFrames(ictx->stream, framesPerBust * ictx->aaudio_context->nchannels);
	ictx->samplesPerFrame = AAudioStream_getSamplesPerFrame(ictx->stream);

	result = AAudioStream_requestStart(ictx->stream);
	if (result != AAUDIO_OK) {
		ms_error("[AAudio] Start stream for recorder failed: %i / %s", result, AAudio_convertResultToText(result));
		result = AAudioStream_close(ictx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio] Recorder stream close failed: %i / %s", result, AAudio_convertResultToText(result));
		} else {
			ms_message("[AAudio] Recorder stream closed");
		}
		ictx->stream = NULL;
	} else {
		ms_message("[AAudio] Recorder stream started");
	}

	if (ictx->stream && ictx->soundCard->capabilities & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER) {
		ictx->session_id = AAudioStream_getSessionId(ictx->stream);
		ms_message("[AAudio] Session ID is %i, hardware echo canceller can be enabled", ictx->session_id);
		if (ictx->session_id != AAUDIO_SESSION_ID_NONE) {
			JNIEnv *env = ms_get_jni_env();
			ictx->aec = ms_android_enable_hardware_echo_canceller(env, ictx->session_id);
			ms_message("[AAudio] Hardware echo canceller enabled");
		} else {
			ms_warning("[AAudio] Session ID is AAUDIO_SESSION_ID_NONE, can't enable hardware echo canceller");
		}
	}

	AAudioStreamBuilder_delete(builder);
}

static void aaudio_recorder_close(AAudioInputContext *ictx) {
	ms_mutex_lock(&ictx->stream_mutex);
	if (ictx->stream) {
		aaudio_result_t result = AAudioStream_requestStop(ictx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio] Recorder stream stop failed: %i / %s", result, AAudio_convertResultToText(result));
		} else {
			ms_message("[AAudio] Recorder stream stopped");
		}

		result = AAudioStream_close(ictx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio] Recorder stream close failed: %i / %s", result, AAudio_convertResultToText(result));
		} else {
			ms_message("[AAudio] Recorder stream closed");
		}
		ictx->stream = NULL;
	}
	ms_mutex_unlock(&ictx->stream_mutex);
}

static void aaudio_recorder_callback_error(AAudioStream *stream, void *userData, aaudio_result_t result) {
	AAudioInputContext *ictx = (AAudioInputContext *)userData;
	ms_error("[AAudio] aaudio_recorder_callback_error has result: %i / %s", result, AAudio_convertResultToText(result));
}

static void android_snd_read_preprocess(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*) obj->data;
	ictx->mFilter = obj;
	ictx->read_samples = 0;
	aaudio_recorder_init(ictx);
}

static void android_snd_read_process(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*) obj->data;
	mblk_t *m;

	ms_mutex_lock(&ictx->stream_mutex);
	if (!ictx->stream) {
		aaudio_recorder_init(ictx);
	} else {
		aaudio_stream_state_t streamState = AAudioStream_getState(ictx->stream);
		if (streamState == AAUDIO_STREAM_STATE_DISCONNECTED) {
			ms_warning("[AAudio] Recorder stream has disconnected");
			if (ictx->stream) {
				AAudioStream_close(ictx->stream);
				ictx->stream = NULL;
			}
		}
	}
	ms_mutex_unlock(&ictx->stream_mutex);

	ms_mutex_lock(&ictx->mutex);
	while ((m = getq(&ictx->q)) != NULL) {
		ms_queue_put(obj->outputs[0], m);
	}
	ms_mutex_unlock(&ictx->mutex);
	if (obj->ticker->time % 5000 == 0) {
		ms_message("[AAudio] sound/wall clock skew is average=%g ms", ictx->mAvSkew);
	}
}

static void android_snd_read_postprocess(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	aaudio_recorder_close(ictx);	

	ms_ticker_set_synchronizer(obj->ticker, NULL);
	ms_mutex_lock(&ictx->mutex);
	ms_ticker_synchronizer_destroy(ictx->mTickerSynchronizer);
	ictx->mTickerSynchronizer = NULL;

	if (ictx->aec) {
		JNIEnv *env = ms_get_jni_env();
		ms_android_delete_hardware_echo_canceller(env, ictx->aec);
		ictx->aec = NULL;
		ms_message("[AAudio] Hardware echo canceller deleted");
	}

	ms_mutex_unlock(&ictx->mutex);
}

static void android_snd_read_uninit(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	delete ictx;
}

static int android_snd_read_set_sample_rate(MSFilter *obj, void *data) {
	return -1; /*don't accept custom sample rates, use recommended rate always*/
}

static int android_snd_read_get_sample_rate(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	*n = ictx->aaudio_context->samplerate;
	return 0;
}

static int android_snd_read_set_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	ictx->aaudio_context->nchannels = *n;
	return 0;
}

static int android_snd_read_get_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	*n = ictx->aaudio_context->nchannels;
	return 0;
}

static int android_snd_read_hack_speaker_state(MSFilter *f, void *arg) {
	return 0;
}

static MSFilterMethod android_snd_read_methods[] = {
	{MS_FILTER_SET_SAMPLE_RATE, android_snd_read_set_sample_rate},
	{MS_FILTER_GET_SAMPLE_RATE, android_snd_read_get_sample_rate},
	{MS_FILTER_SET_NCHANNELS, android_snd_read_set_nchannels},
	{MS_FILTER_GET_NCHANNELS, android_snd_read_get_nchannels},
	{MS_AUDIO_CAPTURE_FORCE_SPEAKER_STATE, android_snd_read_hack_speaker_state},
	{0,NULL}
};

MSFilterDesc android_snd_aaudio_recorder_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSAAudioRecorder",
	"android sound source",
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	android_snd_read_init,
	android_snd_read_preprocess,
	android_snd_read_process,
	android_snd_read_postprocess,
	android_snd_read_uninit,
	android_snd_read_methods
};

// Register aaudio recorder to the factory
void register_aaudio_recorder(MSFactory* factory) {
	ms_factory_register_filter(factory, &android_snd_aaudio_recorder_desc);
}

static MSFilter* ms_android_snd_read_new(MSFactory *factory) {
	MSFilter *f = ms_factory_create_filter_from_desc(factory, &android_snd_aaudio_recorder_desc);
	return f;
}


MSFilter *android_snd_card_create_reader(MSSndCard *card) {
	MSFilter *f = ms_android_snd_read_new(ms_snd_card_get_factory(card));
	AAudioInputContext *ictx = static_cast<AAudioInputContext*>(f->data);
	ictx->soundCard = card;
	ictx->setContext((AAudioContext*)card->data);
	return f;
}
