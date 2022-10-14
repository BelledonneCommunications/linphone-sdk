/*
 * Copyright (c) 2010-2021 Belledonne Communications SARL.
 *
 * msoboe.cpp - Android Media plugin for Linphone, based on Oboe APIs.
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
#include <mediastreamer2/devices.h>

#include <sys/types.h>
#include <jni.h>
#include <dlfcn.h>

#include <msoboe/msoboe.h>

static void android_snd_card_device_create(JNIEnv *env, jobject deviceInfo, SoundDeviceDescription *deviceDescription, MSSndCardManager *m);

// this global variable is shared among all msoboe files
int DeviceFavoriteSampleRate = 44100;
int DeviceFavoriteFramesPerBurst = 0;

const char *oboe_state_to_string(oboe::StreamState state) {
	if (state == oboe::StreamState::Uninitialized) {
		return "Uninitialized";
	} else if (state == oboe::StreamState::Unknown) {
		return "Unknown";
	} else if (state == oboe::StreamState::Open) {
		return "Open";
	} else if (state == oboe::StreamState::Starting) {
		return "Starting";
	} else if (state == oboe::StreamState::Started) {
		return "Started";
	} else if (state == oboe::StreamState::Pausing) {
		return "Pausing";
	} else if (state == oboe::StreamState::Paused) {
		return "Paused";
	} else if (state == oboe::StreamState::Flushing) {
		return "Flushing";
	} else if (state == oboe::StreamState::Flushed) {
		return "Flushed";
	} else if (state == oboe::StreamState::Stopping) {
		return "Stopping";
	} else if (state == oboe::StreamState::Stopped) {
		return "Stopped";
	} else if (state == oboe::StreamState::Closing) {
		return "Closing";
	} else if (state == oboe::StreamState::Closed) {
		return "Closed";
	} else if (state == oboe::StreamState::Disconnected) {
		return "Disconnected";
	}
	return "Unexpected";
}

const char *oboe_api_to_string(oboe::AudioApi api) {
	if (api == oboe::AudioApi::AAudio) {
		return "AAudio";
	} else if (api == oboe::AudioApi::OpenSLES) {
		return "OpenSLES";
	} else if (api == oboe::AudioApi::Unspecified) {
		return "Unspecified";
	}
	return "Unexpected";
}

const char *oboe_direction_to_string(oboe::Direction direction) {
	if (direction == oboe::Direction::Input) {
		return "Input";
	} else if (direction == oboe::Direction::Output) {
		return "Output";
	}
	return "Unexpected";
}

const char *oboe_sharing_mode_to_string(oboe::SharingMode mode) {
	if (mode == oboe::SharingMode::Exclusive) {
		return "Exclusive";
	} else if (mode == oboe::SharingMode::Shared) {
		return "Shared";
	}
	return "Unexpected";
}

const char *oboe_performance_mode_to_string(oboe::PerformanceMode mode) {
	if (mode == oboe::PerformanceMode::None) {
		return "None";
	} else if (mode == oboe::PerformanceMode::LowLatency) {
		return "LowLatency";
	} else if (mode == oboe::PerformanceMode::PowerSaving) {
		return "PowerSaving";
	}
	return "Unexpected";
}

const char *oboe_format_to_string(oboe::AudioFormat format) {
	if (format == oboe::AudioFormat::Float) {
		return "Float";
	} else if (format == oboe::AudioFormat::I16) {
		return "I16";
	} else if (format == oboe::AudioFormat::I24) {
		return "I24";
	} else if (format == oboe::AudioFormat::I32) {
		return "I32";
	} else if (format == oboe::AudioFormat::Invalid) {
		return "Invalid";
	} else if (format == oboe::AudioFormat::Unspecified) {
		return "Unspecified";
	}
	return "Unexpected";
}

static void android_snd_card_detect(MSSndCardManager *m) {
	JNIEnv *env = ms_get_jni_env();

	// Get all devices
	jobject devices = ms_android_get_all_devices(env, "all");

	// extract required information from every device
	jobjectArray deviceArray = (jobjectArray) devices;
	jsize deviceNumber = (int) env->GetArrayLength(deviceArray);

	ms_message("[Oboe] Create soundcards for %0d devices", deviceNumber);

	MSDevicesInfo *devicesInfo = ms_factory_get_devices_info(m->factory);
	SoundDeviceDescription *deviceDescription = ms_devices_info_get_sound_device_description(devicesInfo);

	for (int idx=0; idx < deviceNumber; idx++) {
		jobject deviceInfo = env->GetObjectArrayElement(deviceArray, idx);
		android_snd_card_device_create(env, deviceInfo, deviceDescription, m);
	}
}

static void android_native_snd_card_init(MSSndCard *card) {
	OboeContext* context = new OboeContext();
	card->data = context;
}

static void android_native_snd_card_uninit(MSSndCard *card) {
	OboeContext *ctx = (OboeContext*)card->data;
	ms_warning("[Oboe] Deletion of Oboe context [%p]", ctx);
	delete ctx;
}

MSSndCardDesc android_native_snd_oboe_card_desc = {
	"Oboe",
	android_snd_card_detect,
	android_native_snd_card_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	android_snd_card_create_reader,
	android_snd_card_create_writer,
	android_native_snd_card_uninit
};

static void android_snd_card_device_create(JNIEnv *env, jobject deviceInfo, SoundDeviceDescription *deviceDescription, MSSndCardManager *m) {
	MSSndCardDeviceType type = ms_android_get_device_type(env, deviceInfo);
	if (
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_EARPIECE) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_SPEAKER) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_MICROPHONE) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADSET) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADPHONES) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEARING_AID) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_GENERIC_USB)
	) {
		MSSndCard *card = ms_snd_card_new(&android_native_snd_oboe_card_desc);
		card = ms_snd_card_ref(card);

		card->name = ms_android_get_device_product_name(env, deviceInfo);
		card->internal_id = ms_android_get_device_id(env, deviceInfo);
		card->device_type = type;
		card->device_description = deviceDescription;

		OboeContext *card_data = (OboeContext*)card->data;

		// Card capabilities
		// Assign the value because the default value is MS_SND_CARD_CAP_CAPTURE|MS_SND_CARD_CAP_PLAYBACK
		card->capabilities = ms_android_get_device_capabilities(env, deviceInfo);

		if ((card->capabilities & MS_SND_CARD_CAP_CAPTURE) == MS_SND_CARD_CAP_CAPTURE) {
			if (deviceDescription->flags & DEVICE_HAS_BUILTIN_AEC) {
				card->capabilities |= MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER;
				card_data->builtin_aec = true;
			}
		}

		card->latency = deviceDescription->delay;
		if (deviceDescription->recommended_rate){
			card_data->sampleRate = deviceDescription->recommended_rate;
		}

		// Take capabilities into account as the same device type may have different components with different capabilities and IDs
		if (!ms_snd_card_is_card_duplicate(m, card, TRUE)) {
			card = ms_snd_card_ref(card);
			ms_snd_card_manager_prepend_card(m, card);
			ms_message("[Oboe] Added card with ID: [%s], name: [%s], device ID: [%0d], type: [%s] and capabilities: [%0d]", card->id, card->name, card->internal_id, ms_snd_card_device_type_to_string(card->device_type), card->capabilities);
		} else {
			ms_message("[Oboe] Card with ID: [%s], name: [%s], device ID: [%0d], type: [%s] and capabilities: [%0d] not added, considered as duplicate", card->id, card->name, card->internal_id, ms_snd_card_device_type_to_string(card->device_type), card->capabilities);
		}

		ms_snd_card_unref(card);
	} else {
		ms_message("[Oboe] SKipped device with type [%s]", ms_snd_card_device_type_to_string(type));
	}
}

static bool loadLib() {
	void *handle;
	const char * libname = "liboboe.so";

	bool success = false;

	// open library
	if ((handle = dlopen(libname, RTLD_NOW)) == NULL) {
		ms_warning("[Oboe] Fail to load %s : %s", libname, dlerror());
		success = false;
	} else {
		dlerror(); // Clear previous message if present
		ms_message("[Oboe] %s successfully loaded", libname);
		success = true;
	}

	return success;
}

#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) extern "C" __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) extern "C" type
#endif

// Called by ms_load_plugins in ms_common.c
MS_PLUGIN_DECLARE(void) libmsoboe_init(MSFactory* factory) {
	register_oboe_player(factory);
	register_oboe_recorder(factory);

	ms_message("[Oboe] libmsoboe plugin loaded");

	DeviceFavoriteSampleRate = ms_android_get_preferred_sample_rate();
	DeviceFavoriteFramesPerBurst = ms_android_get_preferred_buffer_size();
	ms_message("[Oboe] Device favorite sample rate = %i and frames per bust = %i", DeviceFavoriteSampleRate, DeviceFavoriteFramesPerBurst);

	MSDevicesInfo* devices = ms_factory_get_devices_info(factory);
	SoundDeviceDescription* d = ms_devices_info_get_sound_device_description(devices);

	if (d->flags & DEVICE_HAS_CRAPPY_AAUDIO) {
		ms_error("[Oboe] Device is blacklisted, do not create Oboe soundcard");
		return;
	}

	MSSndCardManager *m = ms_factory_get_snd_card_manager(factory);

	// Register card manager so that it can be automaticallty initialized
	ms_snd_card_manager_register_desc(m, &android_native_snd_oboe_card_desc);

	ms_message("[Oboe] Soundcard created");
}
