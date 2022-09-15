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
		ms_mutex_init(&streamMutex, NULL);
		mTickerSynchronizer = NULL;
		mAvSkew = 0;
		sessionId = oboe::SessionId::None;
		soundCard = NULL;
		aec = NULL;
		aecEnabled = true;
		voiceRecognitionMode = false;
		deviceChanged = false;
		bluetoothScoStarted = false;
	}

	~OboeInputContext() {
		flushq(&q,0);
		ms_mutex_destroy(&mutex);
		ms_mutex_destroy(&streamMutex);
	}
	
	void setContext(OboeContext *context) {
		oboeContext = context;
	}
	
	OboeContext *oboeContext;
	OboeInputCallback *oboeCallback;
	std::shared_ptr<oboe::AudioStream> stream;
	ms_mutex_t streamMutex;

	queue_t q;
	ms_mutex_t mutex;
	MSTickerSynchronizer *mTickerSynchronizer;
	MSSndCard *soundCard;
	MSFilter *filter;
	int64_t totalReadSamples;
	double mAvSkew;
	oboe::SessionId sessionId;
	oboe::AudioApi usedAudioApi;
	jobject aec;
	bool aecEnabled;
	bool voiceRecognitionMode;
	bool deviceChanged;
	bool bluetoothScoStarted;
};

class OboeInputCallback: public oboe::AudioStreamDataCallback {
public:
	virtual ~OboeInputCallback() = default;

	OboeInputCallback(OboeInputContext *context): oboeInputContext(context) {
		ms_message("[Oboe Recorder] OboeInputCallback created");
	}

	oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
		OboeInputContext *ictx = oboeInputContext;
		int16_t samples = numFrames * oboeStream->getChannelCount();
		ictx->totalReadSamples += samples;

		if (numFrames <= 0) {
			ms_error("[Oboe Recorder] onAudioReady has %i frames", numFrames);
			return oboe::DataCallbackResult::Continue;
		}

		int32_t bufferSize = sizeof(int16_t) * samples;
		mblk_t *m = allocb(bufferSize, 0);
		memcpy(m->b_wptr, audioData, bufferSize);
		m->b_wptr += bufferSize;

		ms_mutex_lock(&ictx->mutex);
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

	bool permissionGranted = ms_android_is_record_audio_permission_granted();
	if (!permissionGranted) {
		ms_error("[Oboe Recorder] RECORD_AUDIO permission hasn't been granted!");
	}
}

static void oboe_recorder_init(OboeInputContext *ictx) {
	oboe::AudioStreamBuilder builder;
	
	oboe::DefaultStreamValues::SampleRate = (int32_t) DeviceFavoriteSampleRate;
	oboe::DefaultStreamValues::FramesPerBurst = (int32_t) DeviceFavoriteFramesPerBurst;

	builder.setDirection(oboe::Direction::Input);
	builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
	builder.setSharingMode(oboe::SharingMode::Exclusive);
	builder.setFormat(oboe::AudioFormat::I16);
	builder.setChannelCount(ictx->oboeContext->nchannels);
	builder.setSampleRate(ictx->oboeContext->sampleRate);
	
	bool forceOpenSLES = false;
	if (ictx->soundCard->device_description != nullptr) {
		if (ictx->soundCard->device_description->flags & DEVICE_HAS_CRAPPY_AAUDIO) {
			ms_warning("[Oboe Recorder] Device has CRAPPY_AAUDIO flag, asking Oboe to use OpenSLES");
			forceOpenSLES = true;
		}
	}
	if (!forceOpenSLES && ms_get_android_sdk_version() < 28) {
		ms_message("[Oboe Recorder] Android < 28 detected, asking Oboe to use OpenSLES");
	}
	if (forceOpenSLES) {
		ictx->usedAudioApi = oboe::AudioApi::OpenSLES;
	} else {
		ictx->usedAudioApi = oboe::AudioApi::AAudio;
	}
	builder.setAudioApi(ictx->usedAudioApi);

	ictx->oboeCallback = new OboeInputCallback(ictx);
	builder.setDataCallback(ictx->oboeCallback);

	builder.setDeviceId(ictx->soundCard->internal_id);
	ms_message("[Oboe Recorder] Using device ID: %s (%i)", ictx->soundCard->id, ictx->soundCard->internal_id);
	
	builder.setContentType(oboe::ContentType::Speech);
	if (ictx->voiceRecognitionMode) {
		// Voice recognition preset works better when recording voice message
		ms_message("[Oboe Recorder] Using voice recognition input preset");
		builder.setInputPreset(oboe::InputPreset::VoiceRecognition);
	} else {
		ms_message("[Oboe Recorder] Using voice communication input preset");
		builder.setInputPreset(oboe::InputPreset::VoiceCommunication);
		builder.setUsage(oboe::Usage::VoiceCommunication);
	}

	if (ictx->aecEnabled && ictx->soundCard->capabilities & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER) {
		ms_message("[Oboe Recorder] Asking for a session ID so we can use echo canceller");
		builder.setSessionId(oboe::SessionId::Allocate);
	} else {
		ms_message("[Oboe Recorder] Echo canceller isn't available or has been disabled explicitely");
		builder.setSessionId(oboe::SessionId::None);
	}
	
	oboe::Result result = builder.openStream(ictx->stream);
	if (result != oboe::Result::OK) {
		ms_error("[Oboe Recorder] Open stream for recorder failed: %i / %s", result, oboe::convertToText(result));
		ictx->stream = nullptr;
		return;
	} else {
		ms_message("[Oboe Recorder] Recorder stream opened, status: %s", oboe_state_to_string(ictx->stream->getState()));
		oboe::AudioApi audioApi = ictx->stream->getAudioApi();
		ms_message("[Oboe Recorder] Recorder stream configuration: API = %s, direction = %s, device id = %i, sharing mode = %s, performance mode = %s, sample rate = %i, channel count = %i, format = %s, frames per burst = %i, buffer capacity in frames = %i", 
			oboe_api_to_string(audioApi), oboe_direction_to_string(ictx->stream->getDirection()), 
			ictx->stream->getDeviceId(), oboe_sharing_mode_to_string(ictx->stream->getSharingMode()), oboe_performance_mode_to_string(ictx->stream->getPerformanceMode()),
			ictx->stream->getSampleRate(), ictx->stream->getChannelCount(), oboe_format_to_string(ictx->stream->getFormat()), 
			ictx->stream->getFramesPerBurst(), ictx->stream->getBufferCapacityInFrames()
		);
		if (audioApi != ictx->usedAudioApi) {
			ms_warning("[Oboe Recorder] We asked for audio API [%s] but Oboe choosed [%s]", oboe_api_to_string(ictx->usedAudioApi), oboe_api_to_string(audioApi));
			ictx->usedAudioApi = audioApi;
		}
	}

	int32_t framesPerBust = ictx->stream->getFramesPerBurst();
	// Set the buffer size to the burst size - this will give us the minimum possible latency
	ictx->stream->setBufferSizeInFrames(framesPerBust * ictx->oboeContext->nchannels);

	result = ictx->stream->start();
	if (result != oboe::Result::OK) {
		ms_error("[Oboe Recorder] Start stream for recorder failed: %i / %s", result, oboe::convertToText(result));
		result = ictx->stream->close();
		if (result != oboe::Result::OK) {
			ms_error("[Oboe Recorder] Recorder stream close failed: %i / %s", result, oboe::convertToText(result));
		} else {
			ms_message("[Oboe Recorder] Recorder stream closed");
		}
		ictx->stream = nullptr;
	} else {
		ms_message("[Oboe Recorder] Recorder stream started, status: %s", oboe_state_to_string(ictx->stream->getState()));
	}

	if (ictx->aecEnabled && ictx->soundCard->capabilities & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER) {
		ictx->sessionId = ictx->stream->getSessionId();
		ms_message("[Oboe Recorder] Session ID is %i, hardware echo canceller can be enabled", ictx->sessionId);
		if (ictx->sessionId != oboe::SessionId::None) {
			JNIEnv *env = ms_get_jni_env();
			ictx->aec = ms_android_enable_hardware_echo_canceller(env, ictx->sessionId);
			ms_message("[Oboe Recorder] Hardware echo canceller enabled");
		} else {
			ms_warning("[Oboe Recorder] Session ID is oboe::SessionId::None, can't enable hardware echo canceller");
		}
	}
}

static void oboe_recorder_close(OboeInputContext *ictx) {
	ms_mutex_lock(&ictx->streamMutex);

	if (ictx->stream) {
		ms_message("[Oboe Recorder] Stopping recorder stream, status: %s", oboe_state_to_string(ictx->stream->getState()));
		oboe::Result result = ictx->stream->stop();
		if (result != oboe::Result::OK) {
			ms_error("[Oboe Recorder] Recorder stream stop failed: %i / %s", result, oboe::convertToText(result));
		} else {
			ms_message("[Oboe Recorder] Recorder stream stopped");
		}

		ms_message("[Oboe Recorder] Closing recorder stream, status: %s", oboe_state_to_string(ictx->stream->getState()));
		result = ictx->stream->close();
		if (result != oboe::Result::OK) {
			ms_error("[Oboe Recorder] Recorder stream close failed: %i / %s", result, oboe::convertToText(result));
		} else {
			ms_message("[Oboe Recorder] Recorder stream closed");
		}
		ictx->stream = nullptr;
	} else {
		ms_warning("[Oboe Recorder] Recorder stream already closed?");
	}

	if (ictx->oboeCallback) {
		delete ictx->oboeCallback;
		ictx->oboeCallback = nullptr;
		ms_message("[Oboe Recorder] OboeInputCallback destroyed");
	}

	ms_mutex_unlock(&ictx->streamMutex);
}

static void android_snd_read_preprocess(MSFilter *obj) {
	OboeInputContext *ictx = (OboeInputContext*) obj->data;
	ictx->filter = obj;
	ictx->totalReadSamples = 0;

	if ((ms_snd_card_get_device_type(ictx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH)) {
		ms_message("[Oboe Recorder] We were asked to use a bluetooth sound device, starting SCO in Android's AudioManager");
		ictx->bluetoothScoStarted = true;

		JNIEnv *env = ms_get_jni_env();
		ms_android_set_bt_enable(env, ictx->bluetoothScoStarted);
	}
	
	oboe_recorder_init(ictx);

	ms_mutex_lock(&ictx->mutex);
	if (ictx->mTickerSynchronizer == NULL) {
		MSFilter *obj = ictx->filter;
		ictx->mTickerSynchronizer = ms_ticker_synchronizer_new();
		ms_ticker_set_synchronizer(obj->ticker, ictx->mTickerSynchronizer);
	}
	ms_mutex_unlock(&ictx->mutex);
}

static void android_snd_read_process(MSFilter *obj) {
	OboeInputContext *ictx = (OboeInputContext*) obj->data;
	mblk_t *m;

	ms_mutex_lock(&ictx->streamMutex);

	if (ictx->deviceChanged) {
		ms_warning("[Oboe Recorder] Device ID changed to %0d", ictx->soundCard->internal_id);
		if (ictx->stream) {
			ictx->stream->close();
			ictx->stream = nullptr;
		}
		ms_mutex_lock(&ictx->mutex);
		if (ictx->mTickerSynchronizer){
			ms_ticker_synchronizer_resync(ictx->mTickerSynchronizer);
			ms_message("[Oboe Recorder] resync ticket synchronizer to avoid audio delay");
		}
		ms_mutex_unlock(&ictx->mutex);
		ictx->deviceChanged = false;
	}

	if (!ictx->stream) {
		oboe_recorder_init(ictx);
	} else {
		oboe::StreamState streamState = ictx->stream->getState();
		if (streamState == oboe::StreamState::Disconnected) {
			ms_warning("[Oboe Recorder] Recorder stream has disconnected");
			if (ictx->stream) {
				ictx->stream->close();
				ictx->stream = nullptr;
			}
		}
	}
	ms_mutex_unlock(&ictx->streamMutex);

	ms_mutex_lock(&ictx->mutex);
	while ((m = getq(&ictx->q)) != NULL) {
		ms_queue_put(obj->outputs[0], m);
	}
	if (ictx->mTickerSynchronizer != NULL) {
		ictx->mAvSkew = ms_ticker_synchronizer_update(ictx->mTickerSynchronizer, ictx->totalReadSamples, (unsigned int)ictx->oboeContext->sampleRate);
	}
	if (obj->ticker->time % 5000 == 0) {
		ms_message("[Oboe Recorder] sound/wall clock skew is average=%g ms", ictx->mAvSkew);
	}
	ms_mutex_unlock(&ictx->mutex);
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
		ms_message("[Oboe Recorder] Hardware echo canceller deleted");
	}
	
	if (ictx->bluetoothScoStarted) {
		ms_message("[Oboe Recorder] We previously started SCO in Android's AudioManager, stopping it now");
		ictx->bluetoothScoStarted = false;
		// At the end of a call, postprocess is called therefore here the bluetooth device is disabled
		ms_android_set_bt_enable(env, FALSE);
	}

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
	*n = ictx->oboeContext->sampleRate;
	return 0;
}

static int android_snd_read_set_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeInputContext *ictx = static_cast<OboeInputContext*>(obj->data);
	ictx->oboeContext->nchannels = *n;
	return 0;
}

static int android_snd_read_get_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeInputContext *ictx = static_cast<OboeInputContext*>(obj->data);
	*n = ictx->oboeContext->nchannels;
	return 0;
}

static int android_snd_read_set_device_id(MSFilter *obj, void *data) {
	MSSndCard *card = (MSSndCard*)data;
	OboeInputContext *ictx = static_cast<OboeInputContext*>(obj->data);
	ms_message("[Oboe Recorder] Requesting to change capture device ID from %0d to %0d", ictx->soundCard->internal_id, card->internal_id);

	// Change device ID only if the new value is different from the previous one
	if (ictx->soundCard->internal_id != card->internal_id) {
		if (ictx->soundCard) {
			ms_snd_card_unref(ictx->soundCard);
			ictx->soundCard = NULL;
		}
		ictx->soundCard = ms_snd_card_ref(card);
		ictx->deviceChanged = true;

		bool bluetoothSoundDevice = (ms_snd_card_get_device_type(ictx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH);
		if (bluetoothSoundDevice != ictx->bluetoothScoStarted) {
			if (bluetoothSoundDevice) {
				ms_message("[Oboe Recorder] New sound device is bluetooth, starting Android AudioManager's SCO");
			} else {
				ms_message("[Oboe Recorder] New sound device isn't bluetooth, stopping Android AudioManager's SCO");
			}

			JNIEnv *env = ms_get_jni_env();
			ms_android_set_bt_enable(env, bluetoothSoundDevice);
			ictx->bluetoothScoStarted = bluetoothSoundDevice;
		}

		ms_mutex_unlock(&ictx->streamMutex);
	}
	return 0;
}

static int android_snd_read_get_device_id(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeInputContext *ictx = (OboeInputContext*)obj->data;
	*n = ictx->soundCard->internal_id;
	return 0;
}

static int android_snd_read_hack_speaker_state(MSFilter *obj, void *data) {
	return 0;
}

static int android_snd_read_enable_aec(MSFilter *obj, void *data) {
	bool *enabled = (bool*)data;
	OboeInputContext *ictx = (OboeInputContext*)obj->data;
	ictx->aecEnabled = !!(*enabled);
	return 0;
}

static int android_snd_read_enable_voice_rec(MSFilter *obj, void *data) {
	bool *enabled = (bool*)data;
	OboeInputContext *ictx = (OboeInputContext*)obj->data;
	ictx->voiceRecognitionMode = !!(*enabled);
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
	{MS_AUDIO_CAPTURE_ENABLE_AEC, android_snd_read_enable_aec},
	{MS_AUDIO_CAPTURE_ENABLE_VOICE_REC, android_snd_read_enable_voice_rec},
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
	ms_message("[Oboe Recorder] Created using device ID: %s (%i)", ictx->soundCard->id, ictx->soundCard->internal_id);
	ictx->setContext((OboeContext*)card->data);
	return f;
}
