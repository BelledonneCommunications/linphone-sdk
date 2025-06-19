/*
 * Copyright (c) 2010-2021 Belledonne Communications SARL.
 *
 * oboe_player.cpp - Android Media Player plugin for Linphone, based on Oboe APIs.
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
#include <msoboe/msoboe.h>

static const int flowControlIntervalMs = 5000;
static const int flowControlThresholdMs = 40;

class OboeOutputCallback;

struct OboeOutputContext {
	OboeOutputContext(MSFilter *f) {
		filter = f;
		ms_flow_controlled_bufferizer_init(&buffer, f, DeviceFavoriteSampleRate, 1);
		ms_mutex_init(&mutex, NULL);
		ms_mutex_init(&streamMutex, NULL);
		soundCard = NULL;
		usage = oboe::Usage::VoiceCommunication;
		contentType = oboe::ContentType::Speech;
		prevXRunCount = 0;
		bufferCapacity = 0;
		bufferSize = 0;
		framesPerBurst = 0;
		bluetoothScoStarted = false;
	}

	~OboeOutputContext() {
		ms_flow_controlled_bufferizer_uninit(&buffer);
		ms_mutex_destroy(&mutex);
		ms_mutex_destroy(&streamMutex);
	}
	
	void setContext(OboeContext *context) {
		oboeContext = context;
		ms_flow_controlled_bufferizer_set_samplerate(&buffer, oboeContext->sampleRate);
		ms_flow_controlled_bufferizer_set_nchannels(&buffer, oboeContext->nchannels);
		ms_flow_controlled_bufferizer_set_max_size_ms(&buffer, flowControlThresholdMs);
		ms_flow_controlled_bufferizer_set_flow_control_interval_ms(&buffer, flowControlIntervalMs);
	}

	void updateStreamTypeFromMsSndCard() {
		MSSndCardStreamType type = ms_snd_card_get_stream_type(soundCard);
		switch (type) {
			case MS_SND_CARD_STREAM_RING:
				usage = oboe::Usage::NotificationRingtone;
				contentType = oboe::ContentType::Music;
				ms_message("[Oboe Player] Using NotificationRingtone usage / Music content type");
				break;
			case MS_SND_CARD_STREAM_MEDIA:
				usage = oboe::Usage::Media;
				contentType = oboe::ContentType::Music;
				ms_message("[Oboe Player] Using Media usage / Music content type");
				break;
			case MS_SND_CARD_STREAM_DTMF:
				usage = oboe::Usage::VoiceCommunicationSignalling;
				contentType =  oboe::ContentType::Sonification ;
				ms_message("[Oboe Player] Using VoiceCommunicationSignalling usage / Sonification content type");
				break;
			case MS_SND_CARD_STREAM_VOICE:
				usage = oboe::Usage::VoiceCommunication;
				contentType = oboe::ContentType::Speech;
				ms_message("[Oboe Player] Using VoiceCommunication usage / Speech content type");
				break;
			default:
				ms_error("[Oboe Player] Unknown stream type %0d", type);
				break;
		}
	}

	OboeContext *oboeContext;
	OboeOutputCallback *oboeCallback;
	std::shared_ptr<oboe::AudioStream> stream;
	ms_mutex_t streamMutex;

	MSSndCard *soundCard;
	MSFilter *filter;
	MSFlowControlledBufferizer buffer;
	ms_mutex_t mutex;
	oboe::Usage usage;
	oboe::ContentType contentType;
	oboe::AudioApi usedAudioApi;

	int32_t bufferCapacity;
	int32_t prevXRunCount;
	int32_t bufferSize;
	int32_t framesPerBurst;

	bool bluetoothScoStarted;
};

class OboeOutputCallback: public oboe::AudioStreamDataCallback {
public:
	virtual ~OboeOutputCallback() = default;

	OboeOutputCallback(OboeOutputContext *context): oboeOutputContext(context) {
		ms_message("[Oboe Player] OboeOutputCallback created");
	}

	oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
		OboeOutputContext *octx = oboeOutputContext;
	
		if (numFrames <= 0) {
			ms_error("[Oboe Player] onAudioReady has %i frames", numFrames);
			return oboe::DataCallbackResult::Continue;
		}

		ms_mutex_lock(&octx->mutex);
		int ask = sizeof(int16_t) * numFrames * oboeStream->getChannelCount();
		int avail = ms_flow_controlled_bufferizer_get_avail(&octx->buffer);
		int bytes = MIN(ask, avail);
		
		if (bytes > 0) {
			ms_flow_controlled_bufferizer_read(&octx->buffer, (uint8_t *)audioData, bytes);
		}

		if (avail < ask) {
			memset(static_cast<uint8_t *>(audioData) + avail, 0, ask - avail);
		}
		
		ms_mutex_unlock(&octx->mutex);
		return oboe::DataCallbackResult::Continue;
	}

private:
	OboeOutputContext *oboeOutputContext;
};

static void android_snd_write_init(MSFilter *obj){
	OboeOutputContext *octx = new OboeOutputContext(obj);
	obj->data = octx;
}

static void android_snd_write_uninit(MSFilter *obj){
	OboeOutputContext *octx = static_cast<OboeOutputContext*>(obj->data);
	if (octx->soundCard) {
		ms_snd_card_unref(octx->soundCard);
		octx->soundCard = NULL;
	}
	delete octx;
}

static int android_snd_write_set_sample_rate(MSFilter *obj, void *data) {
	return -1; /*don't accept custom sample rates, use recommended rate always*/
}

static int android_snd_write_get_sample_rate(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeOutputContext *octx = static_cast<OboeOutputContext*>(obj->data);
	*n = octx->oboeContext->sampleRate;
	return 0;
}

static int android_snd_write_set_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeOutputContext *octx = static_cast<OboeOutputContext*>(obj->data);
	octx->oboeContext->nchannels = *n;
	ms_flow_controlled_bufferizer_set_nchannels(&octx->buffer, octx->oboeContext->nchannels);
	return 0;
}

static int android_snd_write_get_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeOutputContext *octx = static_cast<OboeOutputContext*>(obj->data);
	*n = octx->oboeContext->nchannels;
	return 0;
}

static void oboe_player_init(OboeOutputContext *octx) {
	octx->updateStreamTypeFromMsSndCard();

	oboe::AudioStreamBuilder builder;

	oboe::DefaultStreamValues::SampleRate = (int32_t) DeviceFavoriteSampleRate;
	oboe::DefaultStreamValues::FramesPerBurst = (int32_t) DeviceFavoriteFramesPerBurst;

	builder.setDirection(oboe::Direction::Output);
	builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
	builder.setSharingMode(oboe::SharingMode::Exclusive);
	builder.setFormat(oboe::AudioFormat::I16);
	builder.setChannelCount(octx->oboeContext->nchannels);
	builder.setSampleRate(octx->oboeContext->sampleRate);
	
	bool forceOpenSLES = false;
	if (octx->soundCard->device_description != nullptr) {
		if (octx->soundCard->device_description->flags & DEVICE_HAS_CRAPPY_AAUDIO) {
			ms_warning("[Oboe Player] Device has CRAPPY_AAUDIO flag, asking Oboe to use OpenSLES");
			forceOpenSLES = true;
		}
	}
	if (!forceOpenSLES && ms_get_android_sdk_version() < 28) {
		ms_message("[Oboe Player] Android < 28 detected, asking Oboe to use OpenSLES");
	}
	if (forceOpenSLES) {
		octx->usedAudioApi = oboe::AudioApi::OpenSLES;
	} else {
		octx->usedAudioApi = oboe::AudioApi::AAudio;
	}
	builder.setAudioApi(octx->usedAudioApi);

	octx->oboeCallback = new OboeOutputCallback(octx);
	builder.setDataCallback(octx->oboeCallback);

	builder.setDeviceId(octx->soundCard->internal_id);
	ms_message("[Oboe Player] Using device ID: %s (%i)", octx->soundCard->id, octx->soundCard->internal_id);
	
	builder.setUsage(octx->usage);
	builder.setContentType(octx->contentType);

	builder.setSessionId(oboe::SessionId::None);
	
	oboe::Result result = builder.openStream(octx->stream);
	if (result != oboe::Result::OK) {
		ms_error("[Oboe Player] Open stream for player failed: %i / %s", result, oboe::convertToText(result));
		octx->stream = nullptr;
		return;
	} else {
		ms_message("[Oboe Player] Player stream opened, status: %s", oboe_state_to_string(octx->stream->getState()));
		oboe::AudioApi audioApi = octx->stream->getAudioApi();
		ms_message("[Oboe Player] Player stream configuration: API = %s, direction = %s, device id = %i, sharing mode = %s, performance mode = %s, sample rate = %i, channel count = %i, format = %s, frames per burst = %i, buffer capacity in frames = %i", 
			oboe_api_to_string(audioApi), oboe_direction_to_string(octx->stream->getDirection()), 
			octx->stream->getDeviceId(), oboe_sharing_mode_to_string(octx->stream->getSharingMode()), oboe_performance_mode_to_string(octx->stream->getPerformanceMode()), 
			octx->stream->getSampleRate(), octx->stream->getChannelCount(), oboe_format_to_string(octx->stream->getFormat()), 
			octx->stream->getFramesPerBurst(), octx->stream->getBufferCapacityInFrames()
		);
		if (audioApi != octx->usedAudioApi) {
			ms_warning("[Oboe Player] We asked for audio API [%s] but Oboe choosed [%s]", oboe_api_to_string(octx->usedAudioApi), oboe_api_to_string(audioApi));
			octx->usedAudioApi = audioApi;
		}
	}

	int32_t framesPerBust = octx->stream->getFramesPerBurst();
	// Set the buffer size to the burst size - this will give us the minimum possible latency
	octx->stream->setBufferSizeInFrames(framesPerBust * octx->oboeContext->nchannels);

	// Current settings
	octx->bufferCapacity = octx->stream->getBufferCapacityInFrames();
	octx->bufferSize = octx->stream->getBufferSizeInFrames();

	result = octx->stream->start();
	if (result != oboe::Result::OK) {
		ms_error("[Oboe Player] Start stream for player failed: %i / %s", result, oboe::convertToText(result));
		result = octx->stream->close();
		if (result != oboe::Result::OK) {
			ms_error("[Oboe Player] Player stream close failed: %i / %s", result, oboe::convertToText(result));
		} else {
			ms_message("[Oboe Player] Player stream closed");
		}
		octx->stream = nullptr;
	} else {
		ms_message("[Oboe Player] Player stream started, status: %s", oboe_state_to_string(octx->stream->getState()));
	}
}

static void oboe_player_close(OboeOutputContext *octx) {
	if (octx->stream) {
		ms_message("[Oboe Player] Stopping player stream, status: %s", oboe_state_to_string(octx->stream->getState()));
		oboe::Result result = octx->stream->stop();
		if (result != oboe::Result::OK) {
			ms_error("[Oboe Player] Player stream stop failed: %i / %s", result, oboe::convertToText(result));
		} else {
			ms_message("[Oboe Player] Player stream stopped");
		}

		ms_message("[Oboe Player] Closing player stream, status: %s", oboe_state_to_string(octx->stream->getState()));
		result = octx->stream->close();
		if (result != oboe::Result::OK) {
			ms_error("[Oboe Player] Player stream close failed: %i / %s", result, oboe::convertToText(result));
		} else {
			ms_message("[Oboe Player] Player stream closed");
		}
		octx->stream = nullptr;
	} else {
		ms_warning("[Oboe Player] Player stream already closed?");
	}

	if (octx->oboeCallback) {
		delete octx->oboeCallback;
		octx->oboeCallback = nullptr;
		ms_message("[Oboe Player] OboeOutputCallback destroyed");
	}
}

static void android_snd_write_preprocess(MSFilter *obj) {
	OboeOutputContext *octx = (OboeOutputContext*)obj->data;
	
	if ((ms_snd_card_get_device_type(octx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH)) {
		ms_message("[Oboe Player] We were asked to use a bluetooth sound device, starting SCO in Android's AudioManager");
		octx->bluetoothScoStarted = true;

		JNIEnv *env = ms_get_jni_env();
		ms_android_set_bt_enable(env, octx->bluetoothScoStarted);
	}

	oboe_player_init(octx);
}

static void android_snd_adjust_buffer_size(OboeOutputContext *octx) {
	// Ensure that stream has been created before adjusting buffer size
	if (octx->stream) {
		oboe::ResultWithValue<int32_t> xRun = octx->stream->getXRunCount();
		int32_t xRunCount = xRun.value();

		// If underrunning is getting worse
		if (xRunCount > octx->prevXRunCount) {
			// New buffer size
			int32_t newBufferSize = octx->bufferSize + octx->framesPerBurst;

			// Buffer size cannot be bigger than the buffer capacity and it must be larger than 0
			if (octx->bufferCapacity < newBufferSize) {
				newBufferSize = octx->bufferCapacity;
			} else if (newBufferSize <= 0) {
				newBufferSize = 1;
			}

			ms_message("[Oboe Player] xRunCount %0d - Changing buffer size from %0d to %0d frames (maximum capacity %0d frames)", xRunCount, octx->bufferSize, newBufferSize, octx->bufferCapacity);
			octx->stream->setBufferSizeInFrames(newBufferSize);

			octx->bufferSize = newBufferSize;
			octx->prevXRunCount = xRunCount;
		}
	}
}

static void android_snd_write_process(MSFilter *obj) {
	OboeOutputContext *octx = (OboeOutputContext*)obj->data;

	ms_mutex_lock(&octx->streamMutex);
	if (!octx->stream) {
		oboe_player_init(octx);
	} else {
		oboe::StreamState streamState = octx->stream->getState();
		if (streamState == oboe::StreamState::Disconnected) {
			ms_warning("[Oboe Player] Player stream has disconnected");
			if (octx->stream) {
				octx->stream->close();
				octx->stream = nullptr;
			}
		}
	}
	android_snd_adjust_buffer_size(octx);
	ms_mutex_unlock(&octx->streamMutex);

	ms_mutex_lock(&octx->mutex);
	ms_flow_controlled_bufferizer_put_from_queue(&octx->buffer, obj->inputs[0]);
	ms_mutex_unlock(&octx->mutex);
}

static void android_snd_write_postprocess(MSFilter *obj) {
	OboeOutputContext *octx = (OboeOutputContext*)obj->data;
	ms_mutex_lock(&octx->streamMutex);
	oboe_player_close(octx);
	ms_mutex_unlock(&octx->streamMutex);
	
	JNIEnv *env = ms_get_jni_env();
	if (octx->bluetoothScoStarted) {
		ms_message("[Oboe Player] We previously started SCO in Android's AudioManager, stopping it now");
		octx->bluetoothScoStarted = false;
		// At the end of a call, postprocess is called therefore here the bluetooth device is disabled
		ms_android_set_bt_enable(env, FALSE);
	}
	if (octx->usedAudioApi == oboe::AudioApi::OpenSLES) {
		ms_message("[Oboe Player] Used Audio API is OpenSLES, clearing communication device");
		ms_android_change_device(env, -1, MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_EARPIECE);
	}
}

static int android_snd_write_set_device_id(MSFilter *obj, void *data) {
	MSSndCard *card = (MSSndCard*)data;
	OboeOutputContext *octx = static_cast<OboeOutputContext*>(obj->data);
	ms_message("[Oboe Player] Requesting to change output card. Current device is %s (device ID %0d) and requested device is %s (device ID %0d)", ms_snd_card_get_string_id(octx->soundCard), octx->soundCard->internal_id, ms_snd_card_get_string_id(card), card->internal_id);
	
	// Change device ID only if the new value is different from the previous one
	if (octx->soundCard->internal_id != card->internal_id) {
		if (octx->soundCard) {
			ms_snd_card_unref(octx->soundCard);
			octx->soundCard = NULL;
		}
		octx->soundCard = ms_snd_card_ref(card);

		JNIEnv *env = ms_get_jni_env();
		if (octx->usedAudioApi == oboe::AudioApi::OpenSLES) {
			ms_mutex_lock(&octx->streamMutex);
			ms_android_change_device(env, card->internal_id, card->device_type);
			ms_mutex_unlock(&octx->streamMutex);
		} else {
			if (octx->stream) {
				ms_mutex_lock(&octx->streamMutex);
				oboe_player_close(octx);
				ms_mutex_unlock(&octx->streamMutex);
			}

			bool bluetoothSoundDevice = (ms_snd_card_get_device_type(octx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH);
			if (bluetoothSoundDevice != octx->bluetoothScoStarted) {
				if (bluetoothSoundDevice) {
					ms_message("[Oboe Player] New sound device has bluetooth type, starting Android AudioManager's SCO");
				} else {
					ms_message("[Oboe Player] New sound device isn't bluetooth, stopping Android AudioManager's SCO");
				}

				ms_android_set_bt_enable(env, bluetoothSoundDevice);
				octx->bluetoothScoStarted = bluetoothSoundDevice;
			}
			
			ms_mutex_lock(&octx->streamMutex);
			oboe_player_init(octx);
			ms_mutex_unlock(&octx->streamMutex);

			if (octx->stream == nullptr) {
				return -1;
			}

			oboe::StreamState inputState = octx->stream->getState();
			ms_message("[Oboe Player] Current state is: %s", oboe_state_to_string(inputState));
			if (inputState == oboe::StreamState::Starting) {
				oboe::StreamState nextState = inputState;
				int tries = 0;
				do {
					ms_usleep(10000); // Wait 10ms

					oboe::Result result = octx->stream->waitForStateChange(inputState, &nextState, 0);
					if (result != oboe::Result::OK) {
						ms_error("[Oboe Player] Couldn't wait for state change: %i / %s", result, oboe::convertToText(result));
						break;
					}

					tries += 1;
				} while (nextState == inputState && tries < 10);
				ms_message("[Oboe Player] Waited for state change, current state is %s (waited for %i ms)", oboe_state_to_string(nextState), 10*tries);
			}

			if (octx->usage == oboe::Usage::VoiceCommunication) {
				ms_message("[Oboe Player] Asking for volume hack (lower & raise volume to workaround no sound on speaker issue, mostly on Samsung devices)");
				ms_android_hack_volume(env);
			}
		}
	} else {
		ms_warning("[Oboe Player] Sound cards internal ids are the same, nothing has been done!");
	}

	return 0;
}

static int android_snd_write_get_device_id(MSFilter *obj, void *data) {
	int *n = (int*)data;
	OboeOutputContext *octx = (OboeOutputContext*)obj->data;
	*n = octx->soundCard->internal_id;
	return 0;
}

static MSFilterMethod android_snd_write_methods[] = {
	{MS_FILTER_SET_SAMPLE_RATE, android_snd_write_set_sample_rate},
	{MS_FILTER_GET_SAMPLE_RATE, android_snd_write_get_sample_rate},
	{MS_FILTER_SET_NCHANNELS, android_snd_write_set_nchannels},
	{MS_FILTER_GET_NCHANNELS, android_snd_write_get_nchannels},
	{MS_AUDIO_PLAYBACK_SET_INTERNAL_ID, android_snd_write_set_device_id},
	{MS_AUDIO_PLAYBACK_GET_INTERNAL_ID, android_snd_write_get_device_id},
	{0,NULL}
};

MSFilterDesc android_snd_oboe_player_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSOboePlayer",
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

// Register oboe player to the factory
void register_oboe_player(MSFactory* factory) {
	ms_factory_register_filter(factory, &android_snd_oboe_player_desc);
}

static MSFilter* ms_android_snd_write_new(MSFactory* factory) {
	MSFilter *f = ms_factory_create_filter_from_desc(factory, &android_snd_oboe_player_desc);
	return f;
}

MSFilter *android_snd_card_create_writer(MSSndCard *card) {
	MSFilter *f = ms_android_snd_write_new(ms_snd_card_get_factory(card));
	OboeOutputContext *octx = static_cast<OboeOutputContext*>(f->data);
	octx->soundCard = ms_snd_card_ref(card);
	ms_message("[Oboe Player] Created using device ID: %s (%i)", octx->soundCard->id, octx->soundCard->internal_id);
	octx->setContext((OboeContext*)card->data);
	return f;
}
