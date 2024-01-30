/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
 *
 * msaaudio_recorder.cpp - Android Media Recorder plugin for Linphone, based on AAudio APIs.
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

#include <mediastreamer2/android_utils.h>
#include <mediastreamer2/msjava.h>
#include <mediastreamer2/msticker.h>

#include <msaaudio/msaaudio.h>

struct AAudioInputContext {
	AAudioInputContext(MSFilter *f) {
		sound_utils = ms_factory_get_android_sound_utils(f->factory);
		sample_rate = ms_android_sound_utils_get_preferred_sample_rate(sound_utils);
		qinit(&q);
		ms_mutex_init(&mutex, NULL);
		ms_mutex_init(&stream_mutex, NULL);
		mTickerSynchronizer = NULL;
		mAvSkew = 0;
		session_id = AAUDIO_SESSION_ID_NONE;
		soundCard = NULL;
		deviceId = -1;
		aec = NULL;
		aecEnabled = true;
		voiceRecognitionMode = false;
		bluetoothScoStarted = false;
	}

	~AAudioInputContext() {
		flushq(&q,0);
		ms_mutex_destroy(&mutex);
		ms_mutex_destroy(&stream_mutex);
	}
	
	void setContext(AAudioContext *context) {
		aaudio_context = context;
	}
	
	AndroidSoundUtils* sound_utils;
	AAudioContext *aaudio_context;
	AAudioStream *stream;
	ms_mutex_t stream_mutex;

	queue_t q;
	ms_mutex_t mutex;
	MSTickerSynchronizer *mTickerSynchronizer;
	MSSndCard *soundCard;
	int deviceId;
	MSFilter *mFilter;
	int sample_rate;
	int64_t read_samples;
	int32_t samplesPerFrame;
	double mAvSkew;
	aaudio_session_id_t session_id;
	jobject aec;
	bool aecEnabled;
	bool voiceRecognitionMode;
	bool bluetoothScoStarted;
};

static AAudioInputContext* aaudio_input_context_init(MSFilter *f) {
	AAudioInputContext* ictx = new AAudioInputContext(f);
	return ictx;
}

static void android_snd_read_init(MSFilter *obj) {
	AAudioInputContext *ictx = aaudio_input_context_init(obj);
	obj->data = ictx;

	bool permissionGranted = ms_android_sound_utils_is_record_audio_permission_granted(ictx->sound_utils);
	if (!permissionGranted) {
		ms_error("[AAudio Recorder] RECORD_AUDIO permission hasn't been granted!");
	}
}

static aaudio_data_callback_result_t aaudio_recorder_callback(AAudioStream *stream, void *userData, void *audioData, int32_t numFrames) {
	AAudioInputContext *ictx = (AAudioInputContext *)userData;
	ictx->read_samples += numFrames * ictx->samplesPerFrame;

	if (numFrames <= 0) {
		ms_error("[AAudio Recorder] aaudio_recorder_callback has %i frames", numFrames);
	}

	int32_t bufferSize = sizeof(int16_t) * numFrames * ictx->samplesPerFrame;
	mblk_t *m = allocb(bufferSize, 0);
	memcpy(m->b_wptr, audioData, bufferSize);
	m->b_wptr += bufferSize;

	ms_mutex_lock(&ictx->mutex);
	putq(&ictx->q, m);
	ms_mutex_unlock(&ictx->mutex);

	return AAUDIO_CALLBACK_RESULT_CONTINUE;	
}

static void aaudio_recorder_callback_error(AAudioStream *stream, void *userData, aaudio_result_t error);

static void aaudio_recorder_init(AAudioInputContext *ictx) {
	AAudioStreamBuilder *builder;

	aaudio_result_t result = AAudio_createStreamBuilder(&builder);
	if (result != AAUDIO_OK && !builder) {
		ms_error("[AAudio Recorder] Couldn't create stream builder for recorder: %i / %s", result, AAudio_convertResultToText(result));
	}

	if (ms_android_sound_utils_is_audio_route_changes_disabled(ictx->sound_utils)) {
		ms_warning("[AAudio Recorder] Not using any device ID because audio route changes are disabled by configuration");
	} else {
		if (ictx->deviceId == -1) {
			ms_warning("[AAudio Recorder] Invalid device ID set [-1], using soundCard internal ID insread [%0d]", ictx->soundCard->internal_id);
			ictx->deviceId = ictx->soundCard->internal_id;
		}

		AAudioStreamBuilder_setDeviceId(builder, ictx->deviceId);
		ms_message("[AAudio Recorder] Using device ID: %s (%i)", ictx->soundCard->id, ictx->deviceId);
	}
	
	AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
	AAudioStreamBuilder_setSampleRate(builder, ictx->sample_rate);
	AAudioStreamBuilder_setDataCallback(builder, aaudio_recorder_callback, ictx);
	AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
	AAudioStreamBuilder_setChannelCount(builder, ictx->aaudio_context->nchannels);
	AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE); // If EXCLUSIVE mode isn't available the builder will fall back to SHARED mode.
	AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	AAudioStreamBuilder_setErrorCallback(builder, aaudio_recorder_callback_error, ictx);

	AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_SPEECH); // Requires NDK build target of 28 instead of 26 !
	if (ictx->voiceRecognitionMode) {
		// Voice recognition preset works better when recording voice message
		ms_message("[AAudio Recorder] Using voice recognition input preset");
		AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_RECOGNITION); // Requires NDK build target of 28 instead of 26 !
	} else {
		ms_message("[AAudio Recorder] Using voice communication input preset");
		AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION); // Requires NDK build target of 28 instead of 26 !
		AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_VOICE_COMMUNICATION); // Requires NDK build target of 28 instead of 26 !
	}

	if (ictx->aecEnabled && ictx->soundCard->capabilities & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER) {
		ms_message("[AAudio Recorder] Asking for a session ID so we can use echo canceller");
		AAudioStreamBuilder_setSessionId(builder, AAUDIO_SESSION_ID_ALLOCATE);
	} else {
		ms_message("[AAudio Recorder] Echo canceller isn't available or has been disabled explicitely");
		AAudioStreamBuilder_setSessionId(builder, AAUDIO_SESSION_ID_NONE);
	}

	ms_message("[AAudio Recorder] Record stream configured with samplerate %i and %i channels", ictx->sample_rate, ictx->aaudio_context->nchannels);
	
	result = AAudioStreamBuilder_openStream(builder, &(ictx->stream));
	if (result != AAUDIO_OK && !ictx->stream) {
		ms_error("[AAudio Recorder] Open stream for recorder failed: %i / %s", result, AAudio_convertResultToText(result));
		AAudioStreamBuilder_delete(builder);
		ictx->stream = NULL;
		return;
	} else {
		ms_message("[AAudio Recorder] Recorder stream opened");
	}

	int32_t framesPerBust = AAudioStream_getFramesPerBurst(ictx->stream);
	// Set the buffer size to the burst size - this will give us the minimum possible latency
	AAudioStream_setBufferSizeInFrames(ictx->stream, framesPerBust * ictx->aaudio_context->nchannels);
	ictx->samplesPerFrame = AAudioStream_getSamplesPerFrame(ictx->stream);

	result = AAudioStream_requestStart(ictx->stream);
	if (result != AAUDIO_OK) {
		ms_error("[AAudio Recorder] Start stream for recorder failed: %i / %s", result, AAudio_convertResultToText(result));
		result = AAudioStream_close(ictx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio Recorder] Recorder stream close failed: %i / %s", result, AAudio_convertResultToText(result));
		} else {
			ms_message("[AAudio Recorder] Recorder stream closed");
		}
		ictx->stream = NULL;
	} else {
		ms_message("[AAudio Recorder] Recorder stream started");
	}

	if (ictx->aecEnabled && ictx->stream && ictx->soundCard->capabilities & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER) {
		ictx->session_id = AAudioStream_getSessionId(ictx->stream);
		ms_message("[AAudio Recorder] Session ID is %i, hardware echo canceller can be enabled", ictx->session_id);
		if (ictx->session_id != AAUDIO_SESSION_ID_NONE) {
			if (ictx->aec == NULL) {
				ictx->aec = ms_android_sound_utils_create_hardware_echo_canceller(ictx->sound_utils, ictx->session_id);
				ms_message("[AAudio Recorder] Hardware echo canceller enabled");
			} else {
				ms_error("[AAudio Recorder] Hardware echo canceller object already created and not released!");
			}
		} else {
			ms_warning("[AAudio Recorder] Session ID is AAUDIO_SESSION_ID_NONE, can't enable hardware echo canceller");
		}
	}

	AAudioStreamBuilder_delete(builder);
}

static void aaudio_recorder_close(AAudioInputContext *ictx) {
	ms_mutex_lock(&ictx->stream_mutex);
	if (ictx->stream) {
		aaudio_result_t result = AAudioStream_requestStop(ictx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio Recorder] Recorder stream stop failed: %i / %s", result, AAudio_convertResultToText(result));
		} else {
			ms_message("[AAudio Recorder] Recorder stream stopped");
		}

		if (ictx->aec) {
			ms_android_sound_utils_release_hardware_echo_canceller(ictx->sound_utils, ictx->aec);
			ictx->aec = NULL;
			ms_message("[AAudio Recorder] Hardware echo canceller deleted");
		}

		result = AAudioStream_close(ictx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio Recorder] Recorder stream close failed: %i / %s", result, AAudio_convertResultToText(result));
		} else {
			ms_message("[AAudio Recorder] Recorder stream closed");
		}
		ictx->stream = NULL;
	}
	ms_mutex_unlock(&ictx->stream_mutex);
}

static void aaudio_recorder_callback_error(AAudioStream *stream, void *userData, aaudio_result_t result) {
	AAudioInputContext *ictx = (AAudioInputContext *)userData;
	ms_error("[AAudio Recorder] aaudio_recorder_callback_error has result: %i / %s", result, AAudio_convertResultToText(result));
}

static void android_snd_read_preprocess(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*) obj->data;
	ictx->mFilter = obj;
	ictx->read_samples = 0;

	ms_mutex_lock(&ictx->stream_mutex);

	if (ms_snd_card_get_device_type(ictx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH ||
		ms_snd_card_get_device_type(ictx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEARING_AID)
	{
		ms_message("[AAudio Recorder] We were asked to use a bluetooth (or hearing aid) sound device, starting SCO in Android's AudioManager");
		ictx->bluetoothScoStarted = true;
		ms_android_sound_utils_enable_bluetooth(ictx->sound_utils, ictx->bluetoothScoStarted);
	}

	aaudio_recorder_init(ictx);

	ms_mutex_lock(&ictx->mutex);
	if (ictx->mTickerSynchronizer == NULL) {
		MSFilter *obj = ictx->mFilter;
		ictx->mTickerSynchronizer = ms_ticker_synchronizer_new();
		ms_ticker_set_synchronizer(obj->ticker, ictx->mTickerSynchronizer);
	}
	ms_mutex_unlock(&ictx->mutex);

	ms_mutex_unlock(&ictx->stream_mutex);
}

static void android_snd_read_process(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*) obj->data;
	mblk_t *m;

	ms_mutex_lock(&ictx->stream_mutex);

	if (ictx->aaudio_context->device_changed) {
		ms_warning("[AAudio Recorder] Device ID changed to %0d", ictx->deviceId);
		if (ictx->stream) {
			AAudioStream_close(ictx->stream);
			ictx->stream = NULL;
		}
		ms_mutex_lock(&ictx->mutex);
		if (ictx->mTickerSynchronizer){
			ms_ticker_synchronizer_resync(ictx->mTickerSynchronizer);
			ms_message("[AAudio Recorder] resync ticket synchronizer to avoid audio delay");
		}
		ms_mutex_unlock(&ictx->mutex);
		ictx->aaudio_context->device_changed = false;
	}


	if (!ictx->stream) {
		aaudio_recorder_init(ictx);
	} else {
		aaudio_stream_state_t streamState = AAudioStream_getState(ictx->stream);
		if (streamState == AAUDIO_STREAM_STATE_DISCONNECTED) {
			ms_warning("[AAudio Recorder] Recorder stream has disconnected");
			if (ictx->stream) {
				AAudioStream_close(ictx->stream);
				ictx->stream = NULL;
			}
		}
	}

	if (ictx->stream) {
		int32_t xRunCount = AAudioStream_getXRunCount(ictx->stream);
		if (xRunCount != 0) {
			ms_warning("[AAudio Recorder] recorder xRunCount is %0d", xRunCount);
		}
	}
	ms_mutex_unlock(&ictx->stream_mutex);

	ms_mutex_lock(&ictx->mutex);
	while ((m = getq(&ictx->q)) != NULL) {
		ms_queue_put(obj->outputs[0], m);
	}
	if (ictx->mTickerSynchronizer != NULL) {
		ictx->mAvSkew = ms_ticker_synchronizer_update(ictx->mTickerSynchronizer, ictx->read_samples, (unsigned int)ictx->sample_rate);
	}
	if (obj->ticker->time % 5000 == 0) {
		ms_message("[AAudio Recorder] sound/wall clock skew is average=%g ms", ictx->mAvSkew);
	}
	ms_mutex_unlock(&ictx->mutex);
}

static void android_snd_read_postprocess(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;

	aaudio_recorder_close(ictx);

	ms_mutex_lock(&ictx->mutex);

	ms_ticker_set_synchronizer(obj->ticker, NULL);
	if (ictx->mTickerSynchronizer != NULL) {
		ms_ticker_synchronizer_destroy(ictx->mTickerSynchronizer);
		ictx->mTickerSynchronizer = NULL;
	}

	if (ictx->bluetoothScoStarted) {
		ms_message("[AAudio Recorder] We previously started SCO in Android's AudioManager, stopping it now");
		ictx->bluetoothScoStarted = false;
		// At the end of a call, postprocess is called therefore here the bluetooth device is disabled
		ms_android_sound_utils_enable_bluetooth(ictx->sound_utils, FALSE);
	}

	ms_mutex_unlock(&ictx->mutex);
}

static void android_snd_read_uninit(MSFilter *obj) {
	AAudioInputContext *ictx = static_cast<AAudioInputContext*>(obj->data);

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
	AAudioInputContext *ictx = static_cast<AAudioInputContext*>(obj->data);
	*n = ictx->sample_rate;
	return 0;
}

static int android_snd_read_set_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioInputContext *ictx = static_cast<AAudioInputContext*>(obj->data);
	ictx->aaudio_context->nchannels = *n;
	return 0;
}

static int android_snd_read_get_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioInputContext *ictx = static_cast<AAudioInputContext*>(obj->data);
	*n = ictx->aaudio_context->nchannels;
	return 0;
}

static void android_snd_read_set_internal_device_id(AAudioInputContext *ictx, int internal_id) {
	if (!ms_android_sound_utils_is_audio_route_changes_disabled(ictx->sound_utils)) {
		ictx->deviceId = internal_id;
		ictx->aaudio_context->device_changed = true;
		ms_message("[AAudio Recorder] Internal ID changed to [%0d], waiting for next process() to restart the recorder", internal_id);
	}
}

static int android_snd_read_set_device_id(MSFilter *obj, void *data) {
	MSSndCard *card = (MSSndCard*)data;
	AAudioInputContext *ictx = static_cast<AAudioInputContext*>(obj->data);

	if (ms_android_sound_utils_is_audio_route_changes_disabled(ictx->sound_utils)) {
		ms_warning("[AAudio Recorder] Audio route changes have been disabled, do not alter device ID");
		return -1;
	}

	ms_message("[AAudio Recorder] Sound card is being changed from ID [%s], device ID [%0d] to ID [%s], name [%s], device ID [%0d], type [%s] and capabilities [%0d]", ictx->soundCard->id, ictx->deviceId, card->id, card->name, card->internal_id, ms_snd_card_device_type_to_string(card->device_type), card->capabilities);
	// Change device ID only if the new value is different from the previous one
	if (ictx->deviceId != card->internal_id) {
		ms_mutex_lock(&ictx->stream_mutex);

		if (ictx->soundCard) {
			ms_snd_card_unref(ictx->soundCard);
			ictx->soundCard = NULL;
		}
		ictx->soundCard = ms_snd_card_ref(card);
		android_snd_read_set_internal_device_id(ictx, ictx->soundCard->internal_id);

		bool bluetoothSoundDevice = ms_snd_card_get_device_type(ictx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH ||
									ms_snd_card_get_device_type(ictx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEARING_AID;
		if (bluetoothSoundDevice != ictx->bluetoothScoStarted) {
			if (bluetoothSoundDevice) {
				ms_message("[AAudio Recorder] New sound device is bluetooth (or hearing aid), starting Android AudioManager's SCO");
			} else {
				ms_message("[AAudio Recorder] New sound device isn't bluetooth (or hearing aid), stopping Android AudioManager's SCO");
			}

			ms_android_sound_utils_enable_bluetooth(ictx->sound_utils, bluetoothSoundDevice);
			ictx->bluetoothScoStarted = bluetoothSoundDevice;
		}

		ms_mutex_unlock(&ictx->stream_mutex);
	} else {
		ms_warning("[AAudio Recorder] This device ID [%0d] is already in use", card->internal_id);
	}
	return 0;
}

static int android_snd_read_get_device_id(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	*n = ictx->deviceId;
	return 0;
}

static int android_snd_read_hack_speaker_state(MSFilter *obj, void *data) {
	return 0;
}

static int android_snd_read_enable_aec(MSFilter *obj, void *data) {
	bool *enabled = (bool*)data;
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	ictx->aecEnabled = !!(*enabled);
	return 0;
}

static int android_snd_read_enable_voice_rec(MSFilter *obj, void *data) {
	bool *enabled = (bool*)data;
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	ictx->voiceRecognitionMode = !!(*enabled);
	return 0;
}

static void android_snd_read_change_microphone_according_to_speaker(MSFilter *obj, MSSndCard *playback_card) {
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;

	if (playback_card->device_type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_SPEAKER) {
		if (ictx->soundCard->alternative_id != -1) {
			ms_message("[AAudio Recorder] Changing microphone to use the one in the back [%0d]", ictx->soundCard->alternative_id);
			ms_mutex_lock(&ictx->stream_mutex);
			android_snd_read_set_internal_device_id(ictx, ictx->soundCard->alternative_id);
			ms_mutex_unlock(&ictx->stream_mutex);
		} else {
			ms_warning("[AAudio Recorder] No alternative microphone ID found, doing nothing...");
		}
	} else if (playback_card->device_type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_EARPIECE) {
		ms_message("[AAudio Recorder] Changing microphone to use the one in the main one [%0d]", ictx->soundCard->internal_id);
		ms_mutex_lock(&ictx->stream_mutex);
		android_snd_read_set_internal_device_id(ictx, ictx->soundCard->internal_id);
		ms_mutex_unlock(&ictx->stream_mutex);
	} else if ((playback_card->capabilities & MS_SND_CARD_CAP_CAPTURE) == MS_SND_CARD_CAP_CAPTURE) {
		ms_message("[AAudio Recorder] Playback soundcard also has capture capability, switching microphone to it as well");
		android_snd_read_set_device_id(obj, playback_card);
	}
}

static int android_snd_read_playback_device_changed(MSFilter *obj, void *data) {
	MSSndCard *playback_card = (MSSndCard *)data;
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	ms_message("[AAudio Recorder] Playback sound card is being changed to ID [%s], name [%s], device ID [%0d], type [%s] and capabilities [%0d]", playback_card->id, playback_card->name, playback_card->internal_id, ms_snd_card_device_type_to_string(playback_card->device_type), playback_card->capabilities);

	if (playback_card->name != NULL && ictx->soundCard->name != NULL && strcmp(playback_card->name, ictx->soundCard->name) == 0) {
		android_snd_read_change_microphone_according_to_speaker(obj, playback_card);
	} else {
		ms_message("[AAudio Recorder] New playback device [%s] doesn't have the same name as the current recorder device [%s], trying to find a recorder soundcard with the same name to use", playback_card->name, ictx->soundCard->name);
		MSSndCardManager *manager = ms_factory_get_snd_card_manager(obj->factory);
		const bctbx_list_t *list = ms_snd_card_manager_get_list(manager);
		for (const bctbx_list_t *it = list; it != nullptr; it = bctbx_list_next(it)) {
			MSSndCard *card = static_cast<MSSndCard *>(bctbx_list_get_data(it));
			if ((card->capabilities & MS_SND_CARD_CAP_CAPTURE) == MS_SND_CARD_CAP_CAPTURE && card->name != NULL && strcmp(card->name, playback_card->name) == 0) {
				ms_message("[AAudio Recorder] Found a sound card matching playback device name and with CAPTURE capability, using it");
				android_snd_read_set_device_id(obj, card);
				android_snd_read_change_microphone_according_to_speaker(obj, playback_card);
				break;
			}
		}
	}

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
	{MS_AUDIO_CAPTURE_PLAYBACK_DEVICE_CHANGED, android_snd_read_playback_device_changed},
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
	ictx->soundCard = ms_snd_card_ref(card);
	ictx->deviceId = ictx->soundCard->internal_id;
	ms_message("[AAudio Recorder] Created using device ID: %s (%i)", ictx->soundCard->id, ictx->deviceId);
	ictx->setContext((AAudioContext*)card->data);
	return f;
}
