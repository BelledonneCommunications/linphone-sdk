/*
 * Copyright (c) 2010-2021 Belledonne Communications SARL.
 *
 * msoboe_recorder.cpp - Android Media Recorder plugin for Linphone, based on Oboe APIs.
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

#include <msoboe/msoboe.h>

class OboeInputCallback;

struct OboeInputContext {
	OboeInputContext() {
		qinit(&q);
		ms_mutex_init(&mutex, NULL);
		ms_mutex_init(&stream_mutex, NULL);
		mTickerSynchronizer = NULL;
		mAvSkew = 0;
		sessionId = oboe::SessionId::None;
		soundCard = NULL;
		aec = NULL;
	}

	~OboeInputContext() {
		flushq(&q,0);
		ms_mutex_destroy(&mutex);
		ms_mutex_destroy(&stream_mutex);
	}
	
	void setContext(OboeContext *context) {
		oboe_context = context;
	}
	
	OboeContext *oboe_context;
	OboeInputCallback *oboeCallback;
	std::shared_ptr<oboe::AudioStream> stream;
	ms_mutex_t stream_mutex;

	queue_t q;
	ms_mutex_t mutex;
	MSTickerSynchronizer *mTickerSynchronizer;
	MSSndCard *soundCard;
	MSFilter *mFilter;
	int64_t totalReadSamples;
	double mAvSkew;
	oboe::SessionId sessionId;
	jobject aec;
};

class OboeInputCallback: public oboe::AudioStreamDataCallback {
public:
	virtual ~OboeInputCallback() = default;

	OboeInputCallback(OboeInputContext *context): oboeInputContext(context) {
		ms_message("[Oboe] OboeInputCallback created");
	}

	oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
		OboeInputContext *ictx = oboeInputContext;
		int16_t samples = numFrames * oboeStream->getChannelCount();
		ictx->totalReadSamples += samples;

		if (numFrames <= 0) {
			ms_error("[Oboe] onAudioReady has %i frames", numFrames);
			return oboe::DataCallbackResult::Continue;
		}

		int32_t bufferSize = sizeof(int16_t) * samples;
		mblk_t *m = allocb(bufferSize, 0);
		memcpy(m->b_wptr, audioData, bufferSize);
		m->b_wptr += bufferSize;

		ms_mutex_lock(&ictx->mutex);
		if (ictx->mTickerSynchronizer != NULL) {
			ictx->mAvSkew = ms_ticker_synchronizer_update(ictx->mTickerSynchronizer, ictx->totalReadSamples, (unsigned int)ictx->oboe_context->sampleRate);
		}

		putq(&ictx->q, m);	
		ms_mutex_unlock(&ictx->mutex);
		return oboe::DataCallbackResult::Continue;
	}

private:
	OboeInputContext *oboeInputContext;
};

static OboeInputContext* oboe_input_context_init() {
	OboeInputContext* ictx = new OboeInputContext();
	return ictx;
}

static void android_snd_read_init(MSFilter *obj) {
	OboeInputContext *ictx = oboe_input_context_init();
	obj->data = ictx;	
}

static void oboe_recorder_init(OboeInputContext *ictx) {
	oboe::AudioStreamBuilder builder;

	builder.setDirection(oboe::Direction::Input);
	builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
	builder.setSharingMode(oboe::SharingMode::Exclusive);
	builder.setFormat(oboe::AudioFormat::I16);
	builder.setChannelCount(ictx->oboe_context->nchannels);
	builder.setSampleRate(ictx->oboe_context->sampleRate);

	//builder.setErrorCallback(aaudio_recorder_callback_error);
	ictx->oboeCallback = new OboeInputCallback(ictx);
	builder.setDataCallback(ictx->oboeCallback);

	builder.setDeviceId(ictx->soundCard->internal_id);
	ms_message("[Oboe] Using device ID: %s (%i)", ictx->soundCard->id, ictx->soundCard->internal_id);
	
	builder.setInputPreset(oboe::InputPreset::VoiceCommunication);
	builder.setUsage(oboe::Usage::VoiceCommunication);
	builder.setContentType(oboe::ContentType::Speech);

	builder.setSessionId(oboe::SessionId::None);

	if (ictx->soundCard->capabilities & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER) {
		ms_message("[Oboe] Asking for a session ID so we can use echo canceller");
		builder.setSessionId(oboe::SessionId::Allocate);
	}

	ms_message("[Oboe] Record stream configured with sample rate %i and %i channels", ictx->oboe_context->sampleRate, ictx->oboe_context->nchannels);
	
	oboe::Result result = builder.openStream(ictx->stream);
	if (result != oboe::Result::OK) {
		ms_error("[Oboe] Open stream for recorder failed: %i / %s", result, oboe::convertToText(result));
		ictx->stream = nullptr;
		return;
	} else {
		ms_message("[Oboe] Recorder stream opened, status: %i", ictx->stream->getState());
		ms_message("[Oboe] direction = %i, device id = %i, sharing mode = %i, performance mode = %i, sample rate = %i, channel count = %i, format = %i, buffer capacity in frames = %i, frames per callback = %i", 
			ictx->stream->getDirection(), ictx->stream->getDeviceId(), ictx->stream->getSharingMode(), ictx->stream->getPerformanceMode(), 
			ictx->stream->getSampleRate(), ictx->stream->getFormat(), ictx->stream->getBufferCapacityInFrames(), ictx->stream->getFramesPerCallback()
		);
	}

	result = ictx->stream->start();
	if (result != oboe::Result::OK) {
		ms_error("[Oboe] Start stream for recorder failed: %i / %s", result, oboe::convertToText(result));
		result = ictx->stream->close();
		if (result != oboe::Result::OK) {
			ms_error("[Oboe] Recorder stream close failed: %i / %s", result, oboe::convertToText(result));
		} else {
			ms_message("[Oboe] Recorder stream closed");
		}
		ictx->stream = nullptr;
	} else {
		ms_message("[Oboe] Recorder stream started, status: %i", ictx->stream->getState());
	}

	int32_t framesPerBust = ictx->stream->getFramesPerBurst();
	// Set the buffer size to the burst size - this will give us the minimum possible latency
	ictx->stream->setBufferSizeInFrames(framesPerBust * ictx->oboe_context->nchannels);

	if (ictx->soundCard->capabilities & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER) {
		ictx->sessionId = ictx->stream->getSessionId();
		ms_message("[Oboe] Session ID is %i, hardware echo canceller can be enabled", ictx->sessionId);
		if (ictx->sessionId != oboe::SessionId::None) {
			JNIEnv *env = ms_get_jni_env();
			ictx->aec = ms_android_enable_hardware_echo_canceller(env, ictx->sessionId);
			ms_message("[Oboe] Hardware echo canceller enabled");
		} else {
			ms_warning("[Oboe] Session ID is oboe::SessionId::None, can't enable hardware echo canceller");
		}
	}
}

static void oboe_recorder_close(OboeInputContext *ictx) {
	ms_mutex_lock(&ictx->stream_mutex);

	if (ictx->stream) {
		ms_message("[Oboe] Stopping recorder stream, status: %i", ictx->stream->getState());
		oboe::Result result = ictx->stream->stop();
		if (result != oboe::Result::OK) {
			ms_error("[Oboe] Recorder stream stop failed: %i / %s", result, oboe::convertToText(result));
		} else {
			ms_message("[Oboe] Recorder stream stopped");
		}

		ms_message("[Oboe] Closing recorder stream, status: %i", ictx->stream->getState());
		result = ictx->stream->close();
		if (result != oboe::Result::OK) {
			ms_error("[Oboe] Recorder stream close failed: %i / %s", result, oboe::convertToText(result));
		} else {
			ms_message("[Oboe] Recorder stream closed");
		}
		ictx->stream = nullptr;
	}
	if (ictx->oboeCallback) {
		delete ictx->oboeCallback;
		ictx->oboeCallback = nullptr;
	}
	ms_mutex_unlock(&ictx->stream_mutex);
}

static void android_snd_read_preprocess(MSFilter *obj) {
	OboeInputContext *ictx = (OboeInputContext*) obj->data;
	ictx->mFilter = obj;
	ictx->totalReadSamples = 0;
	oboe_recorder_init(ictx);

	if (ictx->mTickerSynchronizer == NULL) {
		MSFilter *obj = ictx->mFilter;
		ictx->mTickerSynchronizer = ms_ticker_synchronizer_new();
		ms_ticker_set_synchronizer(obj->ticker, ictx->mTickerSynchronizer);
	}

	JNIEnv *env = ms_get_jni_env();
	ms_android_set_bt_enable(env, (ms_snd_card_get_device_type(ictx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH));
}

static void android_snd_read_process(MSFilter *obj) {
	OboeInputContext *ictx = (OboeInputContext*) obj->data;
	mblk_t *m;

	ms_mutex_lock(&ictx->stream_mutex);

	if (ictx->oboe_context->device_changed) {
		ms_warning("[Oboe] Device ID changed to %0d", ictx->soundCard->internal_id);
		if (ictx->stream) {
			ictx->stream->close();
			ictx->stream = nullptr;
		}
		ictx->oboe_context->device_changed = false;
	}


	if (!ictx->stream) {
		oboe_recorder_init(ictx);
	} else {
		oboe::StreamState streamState = ictx->stream->getState();
		if (streamState == oboe::StreamState::Disconnected) {
			ms_warning("[Oboe] Recorder stream has disconnected");
			if (ictx->stream) {
				ictx->stream->close();
				ictx->stream = nullptr;
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
		ms_message("[Oboe] sound/wall clock skew is average=%g ms", ictx->mAvSkew);
	}
}

static void android_snd_read_postprocess(MSFilter *obj) {
	OboeInputContext *ictx = (OboeInputContext*)obj->data;
	oboe_recorder_close(ictx);

	ms_ticker_set_synchronizer(obj->ticker, NULL);
	ms_mutex_lock(&ictx->mutex);
	if (ictx->mTickerSynchronizer != NULL) {
		ms_ticker_synchronizer_destroy(ictx->mTickerSynchronizer);
		ictx->mTickerSynchronizer = NULL;
	}

	JNIEnv *env = ms_get_jni_env();

	if (ictx->aec) {
		ms_android_delete_hardware_echo_canceller(env, ictx->aec);
		ictx->aec = NULL;
		ms_message("[Oboe] Hardware echo canceller deleted");
	}

	ms_android_set_bt_enable(env, FALSE);

	ms_mutex_unlock(&ictx->mutex);
}

static void android_snd_read_uninit(MSFilter *obj) {
	OboeInputContext *ictx = static_cast<OboeInputContext*>(obj->data);
	if (ictx->soundCard) {
		ms_snd_card_unref(ictx->soundCard);
		ictx->soundCard = NULL;
	}
	delete ictx;
}

static int android_snd_read_set_sample_rate(MSFilter *obj, void *data) {
	return -1; /*don't accept custom sample rates, use recommended rate always*/
}

static int android_snd_read_get_sample_rate(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeInputContext *ictx = static_cast<OboeInputContext*>(obj->data);
	*n = ictx->oboe_context->sampleRate;
	return 0;
}

static int android_snd_read_set_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeInputContext *ictx = static_cast<OboeInputContext*>(obj->data);
	ictx->oboe_context->nchannels = *n;
	return 0;
}

static int android_snd_read_get_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeInputContext *ictx = static_cast<OboeInputContext*>(obj->data);
	*n = ictx->oboe_context->nchannels;
	return 0;
}

static int android_snd_read_set_device_id(MSFilter *obj, void *data) {
	MSSndCard *card = (MSSndCard*)data;
	OboeInputContext *ictx = static_cast<OboeInputContext*>(obj->data);
	ms_message("[Oboe] Requesting to change capture device ID from %0d to %0d", ictx->soundCard->internal_id, card->internal_id);

	// Change device ID only if the new value is different from the previous one
	if (ictx->soundCard->internal_id != card->internal_id) {
		ms_mutex_lock(&ictx->stream_mutex);
		if (ictx->soundCard) {
			ms_snd_card_unref(ictx->soundCard);
			ictx->soundCard = NULL;
		}
		ictx->soundCard = ms_snd_card_ref(card);
		ictx->oboe_context->device_changed = true;

		JNIEnv *env = ms_get_jni_env();
		ms_android_set_bt_enable(env, (ms_snd_card_get_device_type(ictx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH));
		ms_mutex_unlock(&ictx->stream_mutex);
	}
	return 0;
}

static int android_snd_read_get_device_id(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeInputContext *ictx = (OboeInputContext*)obj->data;
	*n = ictx->soundCard->internal_id;
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
	{MS_AUDIO_CAPTURE_SET_INTERNAL_ID, android_snd_read_set_device_id},
	{MS_AUDIO_CAPTURE_GET_INTERNAL_ID, android_snd_read_get_device_id},
	{0,NULL}
};

MSFilterDesc android_snd_oboe_recorder_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSOboeRecorder",
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

// Register oboe recorder to the factory
void register_oboe_recorder(MSFactory* factory) {
	ms_factory_register_filter(factory, &android_snd_oboe_recorder_desc);
}

static MSFilter* ms_android_snd_read_new(MSFactory *factory) {
	MSFilter *f = ms_factory_create_filter_from_desc(factory, &android_snd_oboe_recorder_desc);
	return f;
}


MSFilter *android_snd_card_create_reader(MSSndCard *card) {
	MSFilter *f = ms_android_snd_read_new(ms_snd_card_get_factory(card));
	OboeInputContext *ictx = static_cast<OboeInputContext*>(f->data);
	ictx->soundCard = ms_snd_card_ref(card);
	ictx->setContext((OboeContext*)card->data);
	return f;
}
