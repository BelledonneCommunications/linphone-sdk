/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * msaaudio.cpp - Android Media plugin for Linphone, based on AAudio APIs.
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

#include <msaaudio/msaaudio.h>

static void android_snd_card_device_create(const AndroidSoundUtils *soundUtils, jobject deviceInfo, SoundDeviceDescription *deviceDescription, MSSndCardManager *m);

static void android_snd_card_detect(MSSndCardManager *m) {
	JNIEnv *env = ms_get_jni_env();

	AndroidSoundUtils *soundUtils = ms_factory_get_android_sound_utils(m->factory);
	// Get all devices
	jobject devices = ms_android_sound_utils_get_devices(soundUtils, "all");

	// extract required information from every device
	jobjectArray deviceArray = (jobjectArray) devices;
	jsize deviceNumber = (int) env->GetArrayLength(deviceArray);
	ms_message("[AAudio] Create soundcards for %0d devices", deviceNumber);

	MSDevicesInfo *devicesInfo = ms_factory_get_devices_info(m->factory);
	SoundDeviceDescription *deviceDescription = ms_devices_info_get_sound_device_description(devicesInfo);

	for (int idx=0; idx < deviceNumber; idx++) {
		jobject deviceInfo = env->GetObjectArrayElement(deviceArray, idx);
		android_snd_card_device_create(soundUtils, deviceInfo, deviceDescription, m);
	}
}

static void android_native_snd_card_init(MSSndCard *card) {
	AAudioContext* context = new AAudioContext();
	card->data = context;
}

static void android_native_snd_card_uninit(MSSndCard *card) {
	AAudioContext *ctx = (AAudioContext*)card->data;
	ms_message("[AAudio] Deletion of AAudio context [%p]", ctx);
	delete ctx;
}

MSSndCardDesc android_native_snd_aaudio_card_desc = {
	"AAudio",
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

static void android_snd_card_device_create(const AndroidSoundUtils *soundUtils, jobject deviceInfo, SoundDeviceDescription *deviceDescription, MSSndCardManager *m) {
	MSSndCardDeviceType type = ms_android_sound_utils_get_device_type(soundUtils, deviceInfo);
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
		MSSndCard *card = ms_snd_card_new(&android_native_snd_aaudio_card_desc);
		card = ms_snd_card_ref(card);

		card->name = ms_android_sound_utils_get_device_product_name(soundUtils, deviceInfo);
		card->internal_id = ms_android_sound_utils_get_device_id(soundUtils, deviceInfo);
		card->device_type = type;
		card->device_description = deviceDescription;

		AAudioContext *card_data = (AAudioContext*)card->data;

		// Card capabilities
		// Assign the value because the default value is MS_SND_CARD_CAP_CAPTURE|MS_SND_CARD_CAP_PLAYBACK
		card->capabilities = ms_android_sound_utils_get_device_capabilities(soundUtils, deviceInfo);
		if (deviceDescription->flags & DEVICE_HAS_CRAPPY_AAUDIO) {
			ms_warning("[AAudio] Device has been dynamically blacklisted using DEVICE_HAS_CRAPPY_AAUDIO flag");
			ms_snd_card_unref(card);
			return;
		}

		bool isBottom = false;
		if ((card->capabilities & MS_SND_CARD_CAP_CAPTURE) == MS_SND_CARD_CAP_CAPTURE) {
			char *address = ms_android_sound_utils_get_microphone_device_address(soundUtils, deviceInfo);
			if (address != NULL && strcmp(address, "bottom") == 0) {
				ms_message("[AAudio] Microphone device has [%s] address", address);
				isBottom = true;
			} else {
				ms_message("[AAudio] Microphone device have [%s] address, assuming not bottom (back)", address);
			}
			if (deviceDescription->flags & DEVICE_HAS_BUILTIN_AEC) {
				card->capabilities |= MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER;
				card_data->builtin_aec = true;
			}
			if (address != NULL) {
				ms_free(address);
			}
		}

		card->latency = deviceDescription->delay;

		// Take capabilities into account as the same device type may have different components with different capabilities and IDs
		if (!ms_snd_card_is_card_duplicate(m, card, TRUE)) {
			ms_snd_card_manager_prepend_card(m, card);
			ms_message("[AAudio] Added card with ID [%s], name [%s], device ID [%0d], type [%s] and capabilities [%0d]", card->id, card->name, card->internal_id, ms_snd_card_device_type_to_string(card->device_type), card->capabilities);
		} else {
			if (ms_snd_card_get_device_type(card) == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_MICROPHONE) {
				MSSndCard *duplicate = ms_snd_card_get_card_duplicate(m, card, TRUE);
				if (duplicate) {
					if (isBottom) {
						ms_warning("[AAudio] Back microphone already added with device ID [%0d], removing it and adding bottom microphone ID [%0d] instead with back ID as alternative", duplicate->internal_id, card->internal_id);
						card->alternative_id = duplicate->internal_id;
						ms_snd_card_manager_prepend_card(m, card);
						ms_snd_card_manager_remove_card(m, duplicate);
					} else {
						ms_message("[AAudio] Bottom microphone already added with device ID [%0d], storing back microphone ID [%0d] as alternative in it", duplicate->internal_id, card->internal_id);
						duplicate->alternative_id = card->internal_id;
					}
					ms_snd_card_unref(duplicate);
				}
			} else {
				ms_message("[AAudio] Card with ID [%s], name [%s], device ID [%0d], type [%s] and capabilities [%0d] not added, considered as duplicate", card->id, card->name, card->internal_id, ms_snd_card_device_type_to_string(card->device_type), card->capabilities);
			}
		}

		ms_snd_card_unref(card);
	} else {
		ms_message("[AAudio] SKipped device with type [%s]", ms_snd_card_device_type_to_string(type));
	}
}

static bool loadLib() {
	void *handle;
	const char * libname = "libaaudio.so";

	bool success = false;

	// open library
	if ((handle = dlopen(libname, RTLD_NOW)) == NULL){
		ms_warning("[AAudio] Fail to load %s : %s", libname, dlerror());
		success = false;
	} else {
		dlerror(); // Clear previous message if present
		ms_message("[AAudio] %s successfully loaded", libname);
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
MS_PLUGIN_DECLARE(void) libmsaaudio_init(MSFactory* factory) {
	register_aaudio_player(factory);
	register_aaudio_recorder(factory);

	ms_message("[AAudio] libmsaaudio plugin loaded");

	const bool loadOk = loadLib();

	if (loadOk) {
		MSDevicesInfo* devices = ms_factory_get_devices_info(factory);
		SoundDeviceDescription* d = ms_devices_info_get_sound_device_description(devices);

		if (d->flags & DEVICE_HAS_CRAPPY_AAUDIO) {
			ms_error("[AAudio] Device is blacklisted, do not create AAudio soundcard");
			return;
		}

		MSSndCardManager *m = ms_factory_get_snd_card_manager(factory);

		// Register card manager so that it can be automaticallty initialized
		ms_snd_card_manager_register_desc(m, &android_native_snd_aaudio_card_desc);

		ms_message("[AAudio] Soundcard created");
	} else {
		ms_error("[AAudio] Unable to load AAudio plugin shared object");
	}
}
