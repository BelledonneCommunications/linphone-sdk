/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
 *
 * aaudio_player.cpp - Android Media Player plugin for Linphone, based on AAudio APIs.
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

#include <string.h>

#include "mediastreamer2/msjava.h"
#include <aaudio/AAudio.h>
#include <msaaudio/msaaudio.h>

static const int flowControlIntervalMs = 5000;
static const int flowControlThresholdMs = 40;

static void aaudio_player_callback_error(AAudioStream *stream, void *userData, aaudio_result_t error);

static int32_t getUpdatedDeviceId();

struct AAudioOutputContext {
	AAudioOutputContext(MSFilter *f) {
		mFilter = f;
		ms_flow_controlled_bufferizer_init(&buffer, f, DeviceFavoriteSampleRate, 1);
		ms_mutex_init(&mutex, NULL);
		ms_mutex_init(&stream_mutex, NULL);
		deviceId = AAUDIO_UNSPECIFIED;
		requestedDeviceId = AAUDIO_UNSPECIFIED;
		soundCard = NULL;
		usage = AAUDIO_USAGE_VOICE_COMMUNICATION;
		content_type = AAUDIO_CONTENT_TYPE_SPEECH;
	}

	~AAudioOutputContext() {
		ms_flow_controlled_bufferizer_uninit(&buffer);
		ms_mutex_destroy(&mutex);
		ms_mutex_destroy(&stream_mutex);
	}
	
	void setContext(AAudioContext *context) {
		aaudio_context = context;
		ms_flow_controlled_bufferizer_set_samplerate(&buffer, aaudio_context->samplerate);
		ms_flow_controlled_bufferizer_set_nchannels(&buffer, aaudio_context->nchannels);
		ms_flow_controlled_bufferizer_set_max_size_ms(&buffer, flowControlThresholdMs);
		ms_flow_controlled_bufferizer_set_flow_control_interval_ms(&buffer, flowControlIntervalMs);
	}

	void updateDeviceId() {
		requestedDeviceId = getUpdatedDeviceId();
		// getUpdatedDeviceId returns -1 if device has not been found
		// Get default
		if (requestedDeviceId == -1) {
			setDefaultDeviceIdFromMsSndCard();
		}
		ms_message("[AAudio] Requesting to create stream in device ID %0d", requestedDeviceId);
	}

	void setDefaultDeviceId(std::string streamTypeStr) {
		// env is an object in C++
		JNIEnv *env = ms_get_jni_env();

		jclass msAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
		if (msAndroidContextClass != NULL) {
			jmethodID getDefaultDeviceID = env->GetStaticMethodID(msAndroidContextClass, "getDefaultPlayerDeviceId", "(Ljava/lang/String;)I");
			if (getDefaultDeviceID != NULL) {
				// Convert C++ strings to jstrign in order to pass them to the JAVA code
				jstring jStreamType = env->NewStringUTF(streamTypeStr.c_str());
				jint id = env->CallStaticIntMethod(msAndroidContextClass, getDefaultDeviceID, jStreamType);
				requestedDeviceId = (int32_t) id;
			}
			env->DeleteLocalRef(msAndroidContextClass);
		}
	}

	void setDefaultDeviceIdFromMsSndCard() {
		MSSndCardStreamType type = ms_snd_card_get_stream_type(soundCard);

		std::string streamTypeStr ("");

		switch (type) {
			case MS_SND_CARD_STREAM_RING:
				streamTypeStr.assign("RING");
				break;
			case MS_SND_CARD_STREAM_MEDIA:
				streamTypeStr.assign("MEDIA");
				break;
			case MS_SND_CARD_STREAM_DTMF:
				streamTypeStr.assign("DTMF");
				break;
			case MS_SND_CARD_STREAM_VOICE:
				streamTypeStr.assign("VOICE");
				break;
			default:
				ms_error("[AAudio] Unknown stream type %0d", type);
				break;
		}

		// If known conversion to stream type
		if (!(streamTypeStr.empty())) {
			setDefaultDeviceId(streamTypeStr);
		}
	}

	void updateStreamTypeFromMsSndCard() {
		MSSndCardStreamType type = ms_snd_card_get_stream_type(soundCard);
		switch (type) {
			case MS_SND_CARD_STREAM_RING:
				usage = AAUDIO_USAGE_NOTIFICATION_RINGTONE;
				content_type = AAUDIO_CONTENT_TYPE_SONIFICATION;
				ms_message("[AAudio] Using RING mode");
				break;
			case MS_SND_CARD_STREAM_MEDIA:
				usage = AAUDIO_USAGE_MEDIA;
				content_type = AAUDIO_CONTENT_TYPE_MUSIC;
				ms_message("[AAudio] Using MEDIA mode");
				break;
			case MS_SND_CARD_STREAM_DTMF:
				usage = AAUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING;
				content_type =  AAUDIO_CONTENT_TYPE_SONIFICATION ;
				ms_message("[AAudio] Using DTMF mode");
				break;
			case MS_SND_CARD_STREAM_VOICE:
				usage = AAUDIO_USAGE_VOICE_COMMUNICATION;
				content_type = AAUDIO_CONTENT_TYPE_SPEECH;
				ms_message("[AAudio] Using COMMUNICATION mode");
				break;
			default:
				ms_error("[AAudio] Unknown stream type %0d", type);
				break;
		}
	}

	AAudioContext *aaudio_context;
	AAudioStream *stream;
	ms_mutex_t stream_mutex;

	MSSndCard *soundCard;
	MSFilter *mFilter;
	MSFlowControlledBufferizer buffer;
	int32_t samplesPerFrame;
	ms_mutex_t mutex;
	aaudio_usage_t usage;
	aaudio_content_type_t content_type;
	int32_t deviceId;
	int32_t requestedDeviceId;
};

static void android_snd_write_init(MSFilter *obj){
	AAudioOutputContext *octx = new AAudioOutputContext(obj);
	obj->data = octx;
}

static void android_snd_write_uninit(MSFilter *obj){
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	delete octx;
}

static int android_snd_write_set_sample_rate(MSFilter *obj, void *data) {
	return -1; /*don't accept custom sample rates, use recommended rate always*/
}

static int android_snd_write_get_sample_rate(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	*n = octx->aaudio_context->samplerate;
	return 0;
}

static int android_snd_write_set_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	octx->aaudio_context->nchannels = *n;
	ms_flow_controlled_bufferizer_set_nchannels(&octx->buffer, octx->aaudio_context->nchannels);
	return 0;
}

static int android_snd_write_get_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	*n = octx->aaudio_context->nchannels;
	return 0;
}

static aaudio_data_callback_result_t aaudio_player_callback(AAudioStream *stream, void *userData, void *audioData, int32_t numFrames) {
	AAudioOutputContext *octx = (AAudioOutputContext*)userData;
	
	if (numFrames <= 0) {
		ms_error("[AAudio] aaudio_player_callback has %i frames", numFrames);
	}

	ms_mutex_lock(&octx->mutex);
	int ask = sizeof(int16_t) * numFrames * octx->samplesPerFrame;
	int avail = ms_flow_controlled_bufferizer_get_avail(&octx->buffer);
	int bytes = MIN(ask, avail);
	
	if (bytes > 0) {
		ms_flow_controlled_bufferizer_read(&octx->buffer, (uint8_t *)audioData, bytes);
	} else if (avail < ask) {
		memset(static_cast<int16_t *>(audioData) + avail, 0, ask - avail);
	}
	ms_mutex_unlock(&octx->mutex);

	return AAUDIO_CALLBACK_RESULT_CONTINUE;	
}

void setDeviceIdInStreamBuilder(AAudioOutputContext *octx, AAudioStreamBuilder *builder) {
	octx->deviceId = octx->requestedDeviceId;
	AAudioStreamBuilder_setDeviceId(builder, octx->deviceId);
}

static void updateReceiverContextPtr(AAudioOutputContext *octx) {

	JNIEnv *env = ms_get_jni_env();

	std::string msJVLoc("org/linphone/mediastream/");
	std::string msAudioReceiver("MediastreamerAudioBroadcastReceiver");
	std::string msAndroidContext("MediastreamerAndroidContext");

	std::string androidContextClassPath(msJVLoc + msAndroidContext);
	std::string audioReceiverClassPath(msJVLoc + msAudioReceiver);

	jobject audioReceiver = NULL;

	// Get ContextClass
	jclass msAndroidContextClass = env->FindClass(androidContextClassPath.c_str());

	// Get mMediastreamReceiver
	if (msAndroidContextClass != NULL) {
		std::string getAudioReceiverSig("()L" + audioReceiverClassPath + ";");
		jmethodID getAudioReceiverID = env->GetStaticMethodID(msAndroidContextClass, "getAudioBroadcastReceiver", getAudioReceiverSig.c_str());
		if (getAudioReceiverID != NULL) {
			audioReceiver = env->CallStaticObjectMethod(msAndroidContextClass, getAudioReceiverID);
		}
	}

	// Get BroadcastClass
	jclass msAudioReceiverClass = env->FindClass(audioReceiverClassPath.c_str());

	// Set context pointer to mMediastreamReceiver
	if ((msAudioReceiverClass != NULL) && (audioReceiver != NULL)) {
		jmethodID setContextPtrID = env->GetMethodID(msAudioReceiverClass, "setContextPtr", "(J)V");
		if (setContextPtrID != NULL) {
			env->CallVoidMethod(audioReceiver, setContextPtrID, (jlong)octx);
		}
		env->DeleteLocalRef(msAudioReceiverClass);
	}

	if (msAndroidContextClass != NULL) {
		env->DeleteLocalRef(msAndroidContextClass);
	}

}

static void aaudio_player_init(AAudioOutputContext *octx) {
	AAudioStreamBuilder *builder;
	aaudio_result_t result = AAudio_createStreamBuilder(&builder);
	if (result != AAUDIO_OK && !builder) {
		ms_error("[AAudio] Couldn't create stream builder for player: %i / %s", result, AAudio_convertResultToText(result));
	}

	octx->updateStreamTypeFromMsSndCard();
	setDeviceIdInStreamBuilder(octx, builder);
	AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
	AAudioStreamBuilder_setSampleRate(builder, octx->aaudio_context->samplerate);
	AAudioStreamBuilder_setDataCallback(builder, aaudio_player_callback, octx);
	AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
	AAudioStreamBuilder_setChannelCount(builder, octx->aaudio_context->nchannels);
	AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE); // If EXCLUSIVE mode isn't available the builder will fall back to SHARED mode.
	AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	AAudioStreamBuilder_setErrorCallback(builder, aaudio_player_callback_error, octx);
	AAudioStreamBuilder_setUsage(builder, octx->usage); // Requires NDK build target of 28 instead of 26 !
	AAudioStreamBuilder_setContentType(builder, octx->content_type); // Requires NDK build target of 28 instead of 26 !
	ms_message("[AAudio] Player stream configured with samplerate %i and %i channels", octx->aaudio_context->samplerate, octx->aaudio_context->nchannels);
	
	result = AAudioStreamBuilder_openStream(builder, &(octx->stream));
	if (result != AAUDIO_OK && !octx->stream) {
		ms_error("[AAudio] Open stream for player failed: %i / %s", result, AAudio_convertResultToText(result));
		AAudioStreamBuilder_delete(builder);
		return;
	} else {
		ms_message("[AAudio] Player stream opened");
	}
	int32_t framesPerBust = AAudioStream_getFramesPerBurst(octx->stream);
	// Set the buffer size to the burst size - this will give us the minimum possible latency
	AAudioStream_setBufferSizeInFrames(octx->stream, framesPerBust * octx->aaudio_context->nchannels);
	octx->samplesPerFrame = AAudioStream_getSamplesPerFrame(octx->stream);

	result = AAudioStream_requestStart(octx->stream);
	if (result != AAUDIO_OK) {
		ms_error("[AAudio] Start stream for player failed: %i / %s", result, AAudio_convertResultToText(result));
		result = AAudioStream_close(octx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio] Player stream close failed: %i / %s", result, AAudio_convertResultToText(result));
		} else {
			ms_message("[AAudio] Player stream closed");
		}
		octx->stream = NULL;
	} else {
		ms_message("[AAudio] Player stream started");
	}

	AAudioStreamBuilder_delete(builder);
}

static void aaudio_player_close(AAudioOutputContext *octx) {
	ms_mutex_lock(&octx->stream_mutex);
	if (octx->stream) {
		aaudio_result_t result = AAudioStream_requestStop(octx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio] Player stream stop failed: %i / %s", result, AAudio_convertResultToText(result));
		} else {
			ms_message("[AAudio] Player stream stopped");
		}

		result = AAudioStream_close(octx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio] Player stream close failed: %i / %s", result, AAudio_convertResultToText(result));
		} else {
			ms_message("[AAudio] Player stream closed");
		}
		octx->stream = NULL;
	}
	ms_mutex_unlock(&octx->stream_mutex);
}

static void aaudio_player_callback_error(AAudioStream *stream, void *userData, aaudio_result_t result) {
	AAudioOutputContext *octx = (AAudioOutputContext *)userData;
	ms_error("[AAudio] aaudio_player_callback_error has result: %i / %s", result, AAudio_convertResultToText(result));
}

static void android_snd_write_preprocess(MSFilter *obj) {
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	// Set requestedDeviceId to the default value based on the available devices
	octx->setDefaultDeviceIdFromMsSndCard();
	aaudio_player_init(octx);
	// Pass AAudio context pointer address to Java VM
	updateReceiverContextPtr(octx);
}

static int32_t getUpdatedDeviceId() {
	int32_t id = -1;

	// env is an object in C++
	JNIEnv *env = ms_get_jni_env();

	jclass msAndroidContextClassLocalRef = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
	jclass msAndroidContextClass = reinterpret_cast<jclass>(env->NewGlobalRef(msAndroidContextClassLocalRef));
	if (msAndroidContextClass != NULL) {
		jmethodID getOutputDeviceID = env->GetStaticMethodID(msAndroidContextClass, "getOutputAudioDeviceId", "()I");
		if (getOutputDeviceID != NULL) {
			jint jid = env->CallStaticIntMethod(msAndroidContextClass, getOutputDeviceID);
			id = (int32_t) jid;
		}
		env->DeleteGlobalRef(msAndroidContextClass);
		env->DeleteLocalRef(msAndroidContextClassLocalRef);
	}

	return id;
}

static void android_snd_write_process(MSFilter *obj) {
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;

	ms_mutex_lock(&octx->stream_mutex);

	int32_t oldDeviceId = octx->deviceId;
	int32_t newDeviceId = octx->requestedDeviceId;

	ms_warning("[AAudio] DEBUG Device ID old  %0d to new %0d", oldDeviceId, newDeviceId);
	// If deviceId has changed, then destroy the stream
	if (oldDeviceId != newDeviceId) {
		ms_warning("[AAudio] Switching from device ID %0d to %0d", oldDeviceId, newDeviceId);
		// Destroy stream if it exists as output device has changed
		if (octx->stream) {
			AAudioStream_close(octx->stream);
			octx->stream = NULL;
		}
	}

	if (!octx->stream) {
		aaudio_player_init(octx);
	} else {
		aaudio_stream_state_t streamState = AAudioStream_getState(octx->stream);
		if (streamState == AAUDIO_STREAM_STATE_DISCONNECTED) {
			ms_warning("[AAudio] Player stream has disconnected");
			if (octx->stream) {
				AAudioStream_close(octx->stream);
				octx->stream = NULL;
			}
		}
	}
	ms_mutex_unlock(&octx->stream_mutex);

	ms_mutex_lock(&octx->mutex);
	ms_flow_controlled_bufferizer_put_from_queue(&octx->buffer, obj->inputs[0]);
	ms_mutex_unlock(&octx->mutex);
}

static void android_snd_write_postprocess(MSFilter *obj) {
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	aaudio_player_close(octx);
}

static MSFilterMethod android_snd_write_methods[] = {
	{MS_FILTER_SET_SAMPLE_RATE, android_snd_write_set_sample_rate},
	{MS_FILTER_GET_SAMPLE_RATE, android_snd_write_get_sample_rate},
	{MS_FILTER_SET_NCHANNELS, android_snd_write_set_nchannels},
	{MS_FILTER_GET_NCHANNELS, android_snd_write_get_nchannels},
	{0,NULL}
};

MSFilterDesc android_snd_aaudio_player_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSAAudioPlayer",
	"android sound output",
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	android_snd_write_init,
	android_snd_write_preprocess,
	android_snd_write_process,
	android_snd_write_postprocess,
	android_snd_write_uninit,
	android_snd_write_methods
};

// Register aaudio player to the factory
void register_aaudio_player(MSFactory* factory) {
	ms_factory_register_filter(factory, &android_snd_aaudio_player_desc);
}

static MSFilter* ms_android_snd_write_new(MSFactory* factory) {
	MSFilter *f = ms_factory_create_filter_from_desc(factory, &android_snd_aaudio_player_desc);
	return f;
}

MSFilter *android_snd_card_create_writer(MSSndCard *card) {
	MSFilter *f = ms_android_snd_write_new(ms_snd_card_get_factory(card));
	AAudioOutputContext *octx = static_cast<AAudioOutputContext*>(f->data);
	octx->soundCard = card;
	octx->setContext((AAudioContext*)card->data);
	return f;
}

#ifdef __ANDROID__
JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerAudioBroadcastReceiver_requestUpdateDeviceId(JNIEnv * env, jobject obj, jlong ptr) {
	AAudioOutputContext *octx = (AAudioOutputContext*)ptr;
	if (octx != NULL) {
		octx->updateDeviceId();
	} else {
		ms_message("[AAudio] AAudioOutputContext is NULL");
	}
}
#endif
