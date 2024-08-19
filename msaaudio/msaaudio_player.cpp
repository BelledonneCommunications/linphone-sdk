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

#include <mediastreamer2/android_utils.h>
#include <mediastreamer2/msjava.h>
#include <mediastreamer2/msasync.h>

#include <msaaudio/msaaudio.h>

static const int flowControlIntervalMs = 5000;
static const int flowControlThresholdMs = 40;

static void aaudio_player_callback_error(AAudioStream *stream, void *userData, aaudio_result_t error);

static std::string aaudio_content_type_to_string(aaudio_content_type_t contentType) {
	switch (contentType) {
		case AAUDIO_CONTENT_TYPE_SPEECH:
			return "Speech";
		case AAUDIO_CONTENT_TYPE_MUSIC:
			return "Music";
		case AAUDIO_CONTENT_TYPE_MOVIE:
			return "Movie";
		case AAUDIO_CONTENT_TYPE_SONIFICATION:
			return "Sonification";
		default:
			return "Unknown (" + std::to_string(contentType) + ")";
	}
}

static std::string aaudio_usage_to_string(aaudio_usage_t usage) {
	switch (usage) {
		case AAUDIO_USAGE_MEDIA:
			return "Media";
		case AAUDIO_USAGE_VOICE_COMMUNICATION:
			return "Voice Communication";
		case AAUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING:
			return "Voice Communication Signalling";
		case AAUDIO_USAGE_NOTIFICATION:
			return "Notification";
		case AAUDIO_USAGE_NOTIFICATION_RINGTONE:
			return "Notification (Ringtone)";
		case AAUDIO_USAGE_NOTIFICATION_EVENT:
			return "Notification (Event)";
		case AAUDIO_USAGE_ASSISTANCE_ACCESSIBILITY:
			return "Assistance (Accessibility)";
		case AAUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE:
			return "Assistance (Navigation Guide)";
		case AAUDIO_USAGE_ASSISTANCE_SONIFICATION:
			return "Assistance (Sonification)";
		case AAUDIO_USAGE_GAME:
			return "Game";
		case AAUDIO_USAGE_ASSISTANT:
			return "Assistant";
		case AAUDIO_SYSTEM_USAGE_EMERGENCY:
			return "System (Emergency)";
		case AAUDIO_SYSTEM_USAGE_SAFETY:
			return "System (Safety)";
		case AAUDIO_SYSTEM_USAGE_VEHICLE_STATUS:
			return "System (Vehicle Status)";
		case AAUDIO_SYSTEM_USAGE_ANNOUNCEMENT:
			return "System (Announcement)";
		default:
			return "Unknown (" + std::to_string(usage) + ")";
	}
}

struct AAudioOutputContext {
	AAudioOutputContext(MSFilter *f) {
		sound_utils = ms_factory_get_android_sound_utils(f->factory);
		aaudio_context = nullptr;
		stream = nullptr;
		mFilter = f;
		sample_rate = ms_android_sound_utils_get_preferred_sample_rate(sound_utils);
		ms_flow_controlled_bufferizer_init(&buffer, f, sample_rate, 1);
		ms_mutex_init(&mutex, NULL);
		ms_mutex_init(&stream_mutex, NULL);
		process_thread = ms_worker_thread_new("AAudio Player");
		soundCard = nullptr;
		usage = AAUDIO_USAGE_VOICE_COMMUNICATION;
		content_type = AAUDIO_CONTENT_TYPE_SPEECH;
		prevXRunCount = 0;
		bufferCapacity = 0;
		bufferSize = 0;
		framesPerBurst = 0;
		bluetoothScoStarted = false;
		streamRunning = false;
		volumeHackRequired = false;
		task = nullptr;
		checkForDeviceChange = false;
		adjustingBufferSize = false;
	}

	~AAudioOutputContext() {
		ms_worker_thread_destroy(process_thread, TRUE);
		ms_flow_controlled_bufferizer_uninit(&buffer);
		ms_mutex_destroy(&mutex);
		ms_mutex_destroy(&stream_mutex);
	}
	
	void setContext(AAudioContext *context) {
		aaudio_context = context;
		// Sample rate has already been set
		ms_flow_controlled_bufferizer_set_nchannels(&buffer, aaudio_context->nchannels);
		ms_flow_controlled_bufferizer_set_max_size_ms(&buffer, flowControlThresholdMs);
		ms_flow_controlled_bufferizer_set_flow_control_interval_ms(&buffer, flowControlIntervalMs);
	}

	void updateStreamTypeFromMsSndCard() {
		MSSndCardStreamType type = ms_snd_card_get_stream_type(soundCard);
		switch (type) {
			case MS_SND_CARD_STREAM_RING:
				usage = AAUDIO_USAGE_NOTIFICATION_RINGTONE;
				content_type = AAUDIO_CONTENT_TYPE_MUSIC;
				ms_message("[AAudio Player] Using RING mode");
				break;
			case MS_SND_CARD_STREAM_MEDIA:
				usage = AAUDIO_USAGE_MEDIA;
				content_type = AAUDIO_CONTENT_TYPE_MUSIC;
				ms_message("[AAudio Player] Using MEDIA mode");
				break;
			case MS_SND_CARD_STREAM_DTMF:
				usage = AAUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING;
				content_type =  AAUDIO_CONTENT_TYPE_SONIFICATION ;
				ms_message("[AAudio Player] Using DTMF mode");
				break;
			case MS_SND_CARD_STREAM_VOICE:
				usage = AAUDIO_USAGE_VOICE_COMMUNICATION;
				content_type = AAUDIO_CONTENT_TYPE_SPEECH;
				ms_message("[AAudio Player] Using COMMUNICATION mode");
				break;
			default:
				ms_error("[AAudio Player] Unknown stream type %0d", type);
				break;
		}
	}

	AndroidSoundUtils* sound_utils;
	AAudioContext *aaudio_context;
	AAudioStream *stream;
	ms_mutex_t stream_mutex;
	MSWorkerThread *process_thread;

	MSSndCard *soundCard;
	MSFilter *mFilter;
	MSFlowControlledBufferizer buffer;
	int32_t samplesPerFrame;
	ms_mutex_t mutex;
	aaudio_usage_t usage;
	aaudio_content_type_t content_type;

	int sample_rate;
	int32_t bufferCapacity;
	int32_t prevXRunCount;
	int32_t bufferSize;
	int32_t framesPerBurst;
	bool bluetoothScoStarted;
	bool streamRunning;
	MSTask *task;
	bool checkForDeviceChange;
	bool volumeHackRequired;
	bool adjustingBufferSize;
};

static void android_snd_write_init(MSFilter *obj){
	AAudioOutputContext *octx = new AAudioOutputContext(obj);
	obj->data = octx;
}

static void android_snd_write_uninit(MSFilter *obj){
	AAudioOutputContext *octx = static_cast<AAudioOutputContext*>(obj->data);

	if (octx->task) {
		ms_task_wait_completion(octx->task);
		ms_task_destroy(octx->task);
		octx->task = nullptr;
	}

	// https://github.com/google/oboe/pull/970/
	// Wait a bit (10ms) to prevent a callback from being dispatched after stop
	ms_usleep(10000);

	if (octx->soundCard) {
		ms_snd_card_unref(octx->soundCard);
		octx->soundCard = nullptr;
	}

	delete octx;
}

static int android_snd_write_set_sample_rate(MSFilter *obj, void *data) {
	return -1; /*don't accept custom sample rates, use recommended rate always*/
}

static int android_snd_write_get_sample_rate(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioOutputContext *octx = static_cast<AAudioOutputContext*>(obj->data);
	*n = octx->sample_rate;
	return 0;
}

static int android_snd_write_set_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioOutputContext *octx = static_cast<AAudioOutputContext*>(obj->data);
	octx->aaudio_context->nchannels = *n;
	ms_flow_controlled_bufferizer_set_nchannels(&octx->buffer, octx->aaudio_context->nchannels);
	return 0;
}

static int android_snd_write_get_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioOutputContext *octx = static_cast<AAudioOutputContext*>(obj->data);
	*n = octx->aaudio_context->nchannels;
	return 0;
}

static aaudio_data_callback_result_t aaudio_player_callback(AAudioStream *stream, void *userData, void *audioData, int32_t numFrames) {
	AAudioOutputContext *octx = (AAudioOutputContext*)userData;
	if (!octx || !octx->stream) {
		ms_error("[AAudio Player] aaudio_player_callback received when either no context or stream");
		return AAUDIO_CALLBACK_RESULT_STOP;
	}
	
	if (numFrames <= 0) {
		ms_error("[AAudio Player] aaudio_player_callback has [%i] frames", numFrames);
	}

	ms_mutex_lock(&octx->mutex);
	int ask = sizeof(int16_t) * numFrames * octx->samplesPerFrame;
	int avail = ms_flow_controlled_bufferizer_get_avail(&octx->buffer);
	int bytes = MIN(ask, avail);
	
	if (bytes > 0) {
		ms_flow_controlled_bufferizer_read(&octx->buffer, (uint8_t *)audioData, bytes);
	}
	if (avail < ask) {
		memset(static_cast<uint8_t *>(audioData) + avail, 0, ask - avail);
	}
	ms_mutex_unlock(&octx->mutex);

	return AAUDIO_CALLBACK_RESULT_CONTINUE;	
}

static void _aaudio_player_init(AAudioOutputContext *octx, bool skip_update_stream_type) {
	ms_mutex_lock(&octx->stream_mutex);

	AAudioStreamBuilder *builder;
	aaudio_result_t result = AAudio_createStreamBuilder(&builder);
	if (result != AAUDIO_OK && !builder) {
		ms_error("[AAudio Player] Couldn't create stream builder for player: %s (%i)", AAudio_convertResultToText(result), result);
	}

	if (skip_update_stream_type) {
		ms_message("[AAudio Player] Stream restarts due to device being changed (probably), do not apply sound card stream type configuration");
	} else {
		octx->updateStreamTypeFromMsSndCard();
	}

	if (ms_android_sound_utils_is_audio_route_changes_disabled(octx->sound_utils)) {
		ms_warning("[AAudio Player] Not using any device ID because audio route changes are disabled by configuration");
	} else {
		AAudioStreamBuilder_setDeviceId(builder, octx->soundCard->internal_id);
		ms_message("[AAudio Player] Using device ID: [%s] (%i)", octx->soundCard->id, octx->soundCard->internal_id);
	}

	AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
	AAudioStreamBuilder_setSampleRate(builder, octx->sample_rate);
	AAudioStreamBuilder_setDataCallback(builder, aaudio_player_callback, octx);
	AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
	AAudioStreamBuilder_setChannelCount(builder, octx->aaudio_context->nchannels);
	AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE); // If EXCLUSIVE mode isn't available the builder will fall back to SHARED mode.
	AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	AAudioStreamBuilder_setErrorCallback(builder, aaudio_player_callback_error, octx);
	AAudioStreamBuilder_setUsage(builder, octx->usage); // Requires NDK build target of 28 instead of 26 !
	AAudioStreamBuilder_setContentType(builder, octx->content_type); // Requires NDK build target of 28 instead of 26 !

	ms_message("[AAudio Player] Player stream configured with samplerate [%i] and [%i] channels", octx->sample_rate, octx->aaudio_context->nchannels);

	result = AAudioStreamBuilder_openStream(builder, &(octx->stream));
	if (result != AAUDIO_OK && !octx->stream) {
		ms_error("[AAudio Player] Open stream for player failed: %s (%i)", AAudio_convertResultToText(result), result);
		AAudioStreamBuilder_delete(builder);
		octx->streamRunning = false;
		octx->stream = nullptr;
		ms_mutex_unlock(&octx->stream_mutex);
		return;
	} else {
		ms_message("[AAudio Player] Player stream opened");
	}

	aaudio_content_type_t contentType = AAudioStream_getContentType(octx->stream);
	if (octx->content_type != contentType) {
		ms_warning("[AAudio Player] Expected content type [%s] but got [%s]", aaudio_content_type_to_string(octx->content_type).c_str(), aaudio_content_type_to_string(contentType).c_str());
	} else {
		ms_message("[AAudio Player] Content type set to [%s]", aaudio_content_type_to_string(contentType).c_str());
	}

	aaudio_usage_t usage = AAudioStream_getUsage(octx->stream);
	if (octx->usage != usage) {
		ms_warning("[AAudio Player] Expected usage [%s] but got [%s]", aaudio_usage_to_string(octx->usage).c_str(), aaudio_usage_to_string(usage).c_str());
	} else {
		ms_message("[AAudio Player] Usage set to [%s]", aaudio_usage_to_string(usage).c_str());
	}

	octx->framesPerBurst = AAudioStream_getFramesPerBurst(octx->stream);
	// Set the buffer size to the burst size - this will give us the minimum possible latency
	AAudioStream_setBufferSizeInFrames(octx->stream, octx->framesPerBurst * octx->aaudio_context->nchannels * 2);
	octx->samplesPerFrame = AAudioStream_getSamplesPerFrame(octx->stream);

	// Current settings
	octx->bufferCapacity = AAudioStream_getBufferCapacityInFrames(octx->stream);
	octx->bufferSize = AAudioStream_getBufferSizeInFrames(octx->stream);

	result = AAudioStream_requestStart(octx->stream);

	if (result != AAUDIO_OK) {
		octx->streamRunning = false;
		ms_error("[AAudio Player] Start stream for player failed: %s (%i)", AAudio_convertResultToText(result), result);
		result = AAudioStream_close(octx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio Player] Player stream close failed: %s (%i)", AAudio_convertResultToText(result), result);
		} else {
			ms_message("[AAudio Player] Player stream closed");
		}
		octx->stream = nullptr;
	} else {
		ms_message("[AAudio Player] Player stream started");
	}

	AAudioStreamBuilder_delete(builder);
	if (octx->stream == nullptr) {
		ms_mutex_unlock(&octx->stream_mutex);
		return;
	}

	aaudio_stream_state_t inputState = AAudioStream_getState(octx->stream);
	ms_message("[AAudio Player] Current state is [%s]", AAudio_convertStreamStateToText(inputState));

	if (inputState == AAUDIO_STREAM_STATE_STARTING) {
		aaudio_stream_state_t nextState = inputState;
		int tries = 0;
		do {
			ms_usleep(10000); // Wait 10ms

			// Waiting more than 0ns can cause a crash
			aaudio_result_t result = AAudioStream_waitForStateChange(octx->stream, inputState, &nextState, 0);
			if (result != AAUDIO_OK && result != AAUDIO_ERROR_TIMEOUT) {
				ms_error("[AAudio Player] Couldn't wait for state change: %s (%i)", AAudio_convertResultToText(result), result);
				break;
			}

			tries += 1;
		} while (nextState == inputState && tries < 10);
		ms_message("[AAudio Player] Waited for state change, current state is [%s] (waited for [%i] ms)", AAudio_convertStreamStateToText(nextState), 10*tries);
	}

	octx->streamRunning = true;
	octx->volumeHackRequired = true;
	ms_mutex_unlock(&octx->stream_mutex);
	return;
}

static bool_t aaudio_player_init(AAudioOutputContext *octx) {
	_aaudio_player_init(octx, FALSE);
	return TRUE;
}

static bool_t aaudio_player_init_skip_update_stream_type(AAudioOutputContext *octx) {
	_aaudio_player_init(octx, TRUE);
	return TRUE;
}

static bool_t aaudio_player_close(AAudioOutputContext *octx) {
	ms_mutex_lock(&octx->stream_mutex);

	if (octx->stream) {
		octx->streamRunning = false;
		octx->adjustingBufferSize = false;
		aaudio_stream_state_t streamState = AAudioStream_getState(octx->stream);
		ms_message("[AAudio Player] Closing player stream, current state is [%s]", AAudio_convertStreamStateToText(streamState));
		// According to https://developer.android.com/ndk/guides/audio/aaudio/aaudio#state-transitions
		// AAudioStream_close can be called from any state
		aaudio_result_t result = AAudioStream_close(octx->stream);
		if (result != AAUDIO_OK) {
			ms_error("[AAudio Player] Player stream close failed: %s (%i)", AAudio_convertResultToText(result), result);
		} else {
			ms_message("[AAudio Player] Player stream closed");
		}
		octx->stream = nullptr;

		// https://github.com/google/oboe/pull/970/
		// Wait a bit (10ms) to prevent a callback from being dispatched after stop
		ms_usleep(10000);
	}

	ms_mutex_unlock(&octx->stream_mutex);
	return TRUE;
}

static bool_t aaudio_player_restart(AAudioOutputContext *octx) {
	ms_message("[AAudio Player] Restarting stream");
	aaudio_player_close(octx);
	_aaudio_player_init(octx, TRUE);
	ms_message("[AAudio Player] Stream was restarted");

	return TRUE;
}

static void aaudio_player_callback_error(AAudioStream *stream, void *userData, aaudio_result_t result) {
	ms_error("[AAudio Player] aaudio_player_callback_error has result: %s (%i)", AAudio_convertResultToText(result), result);
}

static void anroid_snd_write_require_volume_hack_depending_on_stream(AAudioOutputContext *octx) {
	if (octx->usage == AAUDIO_USAGE_VOICE_COMMUNICATION || octx->usage == AAUDIO_USAGE_NOTIFICATION_RINGTONE || octx->usage == AAUDIO_USAGE_MEDIA) {
		std::string streamName = "STREAM_VOICE_CALL";
		int stream = 0; // https://developer.android.com/reference/android/media/AudioManager#STREAM_VOICE_CALL
		if (octx->usage == AAUDIO_USAGE_NOTIFICATION_RINGTONE) {
			streamName = "STREAM_RING";
			stream = 2; // https://developer.android.com/reference/android/media/AudioManager#STREAM_RING
		} else if (octx->usage == AAUDIO_USAGE_MEDIA) {
			streamName = "STREAM_MUSIC";
			stream = 3; // https://developer.android.com/reference/android/media/AudioManager#STREAM_MUSIC
		}
		ms_message("[AAudio Player] Asking for volume hack on stream [%s](%i) (lower & raise volume to workaround no sound on speaker issue, mostly on Samsung devices)", streamName.c_str(), stream);
		ms_android_sound_utils_hack_volume(octx->sound_utils, stream);
	}
}

static void android_snd_write_preprocess(MSFilter *obj) {
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	
	if (ms_snd_card_get_device_type(octx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH ||
		ms_snd_card_get_device_type(octx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEARING_AID)
	{
		ms_message("[AAudio Player] We were asked to use a bluetooth sound device (or hearing aid), starting SCO in Android's AudioManager");
		octx->bluetoothScoStarted = true;
		ms_android_sound_utils_enable_bluetooth(octx->sound_utils, octx->bluetoothScoStarted);
	}

	ms_worker_thread_add_task(octx->process_thread, (MSTaskFunc)aaudio_player_init, octx);
}

static bool_t android_snd_adjust_buffer_size(AAudioOutputContext *octx) {
	// Ensure that stream has been created before adjusting buffer size
	ms_mutex_lock(&octx->stream_mutex);
	
	if (octx->adjustingBufferSize && octx->streamRunning && octx->stream) {
		AAudioStream_setBufferSizeInFrames(octx->stream, octx->bufferSize);
		octx->adjustingBufferSize = false;
	}

	ms_mutex_unlock(&octx->stream_mutex);
	return TRUE;
}

static void android_snd_write_process(MSFilter *obj) {
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;

	if (octx->streamRunning && octx->stream) {
		aaudio_stream_state_t streamState = AAudioStream_getState(octx->stream);
		if (streamState == AAUDIO_STREAM_STATE_DISCONNECTED) {
			ms_warning("[AAudio Player] Player stream has disconnected");
			ms_worker_thread_add_task(octx->process_thread, (MSTaskFunc)aaudio_player_restart, octx);
		} else {
			if (octx->volumeHackRequired) {
				ms_message("[AAudio Player] Audio stream has been started, applying volume hack");
				anroid_snd_write_require_volume_hack_depending_on_stream(octx);
				octx->volumeHackRequired = false;
			}

			if (octx->checkForDeviceChange) {
				int id = (int)AAudioStream_getDeviceId(octx->stream);
				if (id != octx->soundCard->internal_id) {
					ms_warning("[AAudio Player] Device has changed, restarting stream");
					ms_worker_thread_add_task(octx->process_thread, (MSTaskFunc)aaudio_player_restart, octx);
				}
				octx->checkForDeviceChange = false;
			} else {
				int32_t xRunCount = AAudioStream_getXRunCount(octx->stream);
				// If underrunning is getting worse
				if (xRunCount > octx->prevXRunCount) {
					if (!octx->adjustingBufferSize) {
						// New buffer size
						int32_t newBufferSize = octx->bufferSize + octx->framesPerBurst;

						// Buffer size cannot be bigger than the buffer capacity and it must be larger than 0
						if (octx->bufferCapacity < newBufferSize) {
							newBufferSize = octx->bufferCapacity;
						} else if (newBufferSize <= 0) {
							newBufferSize = 1;
						}

						ms_message("[AAudio Player] xRunCount [%0d] - Changing buffer size from [%0d] to [%0d] frames (maximum capacity [%0d] frames)", xRunCount, octx->bufferSize, newBufferSize, octx->bufferCapacity);
						octx->bufferSize = newBufferSize;

						octx->adjustingBufferSize = true;
						ms_worker_thread_add_task(octx->process_thread, (MSTaskFunc)android_snd_adjust_buffer_size, octx);
					}

					octx->prevXRunCount = xRunCount;
				}
			}
		}
	} else {
		ms_queue_flush(obj->inputs[0]);
		return;
	}

	ms_mutex_lock(&octx->mutex);
	ms_flow_controlled_bufferizer_put_from_queue(&octx->buffer, obj->inputs[0]);
	ms_mutex_unlock(&octx->mutex);
}

static void android_snd_write_postprocess(MSFilter *obj) {
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;

	octx->adjustingBufferSize = false;
	octx->task = ms_worker_thread_add_waitable_task(octx->process_thread, (MSTaskFunc)aaudio_player_close, octx);
	
	if (octx->bluetoothScoStarted) {
		ms_message("[AAudio Player] We previously started SCO in Android's AudioManager, stopping it now");
		octx->bluetoothScoStarted = false;
		// At the end of a call, postprocess is called therefore here the bluetooth device is disabled
		ms_android_sound_utils_enable_bluetooth(octx->sound_utils, FALSE);
	}
}

static int android_snd_write_set_device_id(MSFilter *obj, void *data) {
	MSSndCard *card = (MSSndCard*)data;
	AAudioOutputContext *octx = static_cast<AAudioOutputContext*>(obj->data);

	if (ms_android_sound_utils_is_audio_route_changes_disabled(octx->sound_utils)) {
		ms_warning("[AAudio Player] Audio route changes have been disabled, do not alter device ID");
		return -1;
	}

	ms_message("[AAudio Player] Requesting to output card. Current [%s] (device ID %0d) and requested [%s] (device ID %0d)", ms_snd_card_get_string_id(octx->soundCard), octx->soundCard->internal_id, ms_snd_card_get_string_id(card), card->internal_id);
	// Change device ID only if the new value is different from the previous one
	if (octx->soundCard->internal_id != card->internal_id) {
		if (octx->soundCard) {
			ms_snd_card_unref(octx->soundCard);
			octx->soundCard = nullptr;
		}
		octx->soundCard = ms_snd_card_ref(card);

		bool bluetoothSoundDevice = ms_snd_card_get_device_type(octx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH ||
									ms_snd_card_get_device_type(octx->soundCard) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEARING_AID;
		if (bluetoothSoundDevice != octx->bluetoothScoStarted) {
			if (bluetoothSoundDevice) {
				ms_message("[AAudio Player] New sound device has bluetooth (or hearing aid) type, starting Android AudioManager's SCO");
			} else {
				ms_message("[AAudio Player] New sound device isn't bluetooth (or hearing aid), stopping Android AudioManager's SCO");
			}

			ms_android_sound_utils_enable_bluetooth(octx->sound_utils, bluetoothSoundDevice);
			octx->bluetoothScoStarted = bluetoothSoundDevice;
		}

		if (octx->streamRunning || octx->stream) {
			ms_message("[AAudio Player] Requesting the stream to restart to apply new device ID");
			ms_worker_thread_add_task(octx->process_thread, (MSTaskFunc)aaudio_player_restart, octx);
		} else {
			ms_warning("[AAudio Player] No stream, will check when stream will be running if device needs to be updated");
			octx->checkForDeviceChange = true;
		}
	}
	
	return 0;
}

static int android_snd_write_get_device_id(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
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
	octx->soundCard = ms_snd_card_ref(card);
	ms_message("[AAudio Player] Created using device ID: %s (%i)", octx->soundCard->id, octx->soundCard->internal_id);
	octx->setContext((AAudioContext*)card->data);
	return f;
}
