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

static void android_snd_card_device_create(JNIEnv *env, jobject deviceInfo, MSSndCardManager *m);

// this global variable is shared among all msaaudio files
int DeviceFavoriteSampleRate = 44100;

static void android_snd_card_detect(MSSndCardManager *m) {

	JNIEnv *env = ms_get_jni_env();

	// Get all devices
	jobject devices = ms_android_get_all_devices(env, "all");

	// extract required information from every device
	jobjectArray deviceArray = (jobjectArray) devices;
	jsize deviceNumber = (int) env->GetArrayLength(deviceArray);

	ms_message("[AAudio] Create soundcards for %0d devices", deviceNumber);

	for (int idx=0; idx < deviceNumber; idx++) {
		jobject deviceInfo = env->GetObjectArrayElement(deviceArray, idx);
		android_snd_card_device_create(env, deviceInfo, m);
	}
}

static void android_native_snd_card_init(MSSndCard *card) {
	AAudioContext* context = new AAudioContext();
	card->data = context;
}

static void android_native_snd_card_uninit(MSSndCard *card) {
	AAudioContext *ctx = (AAudioContext*)card->data;
	ms_warning("[AAudio] Deletion of AAudio context [%p]", ctx);
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

static void android_snd_card_device_create(JNIEnv *env, jobject deviceInfo, MSSndCardManager *m) {

	MSSndCardDeviceType type = ms_android_get_device_type(env, deviceInfo);
	if (
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_EARPIECE) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_SPEAKER) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_MICROPHONE) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADSET) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADPHONES) ||
		(type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_GENERIC_USB)
	) {
		MSSndCard *card = ms_snd_card_new(&android_native_snd_aaudio_card_desc);

		card->name = ms_strdup(ms_android_get_device_product_name(env, deviceInfo));
		card->internal_id = ms_android_get_device_id(env, deviceInfo);
		card->device_type = type;

		AAudioContext *card_data = (AAudioContext*)card->data;

		// Card capabilities
		// Assign the value because the default value is MS_SND_CARD_CAP_CAPTURE|MS_SND_CARD_CAP_PLAYBACK
		card->capabilities = ms_android_get_device_capabilities(env, deviceInfo);
		MSDevicesInfo *devices = ms_factory_get_devices_info(m->factory);
		SoundDeviceDescription *d = ms_devices_info_get_sound_device_description(devices);
		if (d->flags & DEVICE_HAS_BUILTIN_AEC) {
			card->capabilities |= MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER;
			card_data->builtin_aec = true;
		}

		card->latency = d->delay;
		if (d->recommended_rate){
			card_data->samplerate = d->recommended_rate;
		}

		// Take capabilities into account as the same device type may have different components with different capabilities and IDs
		if(!ms_snd_card_is_card_duplicate(m, card, TRUE)) {
			card=ms_snd_card_ref(card);
			ms_snd_card_manager_prepend_card(m, card);

			ms_message("[AAudio] Added card: id %s name %s device ID %0d device_type %s capabilities 0'h%0X ", card->id, card->name, card->internal_id, ms_snd_card_device_type_to_string(card->device_type), card->capabilities);
		} else {
			free(card);
		}
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

	DeviceFavoriteSampleRate = ms_android_get_preferred_sample_rate();

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
