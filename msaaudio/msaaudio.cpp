/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * androidsound_aaudio.cpp - Android Media plugin for Linphone, based on AAudio APIs.
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
#include <mediastreamer2/android_utils.h>

#include <sys/types.h>
#include <jni.h>
#include <dlfcn.h>

#include <aaudio/AAudio.h>
#include <msaaudio/msaaudio.h>

static MSSndCard* android_snd_card_new(MSSndCardManager *m);

static AAudioContext* aaudio_context_init() {
	AAudioContext* ctx = new AAudioContext();
	return ctx;
}

aaudio_result_t initAAudio() {
	// Initialize to AAUDIO_ERROR_BASE to ensure that the default value is an error condition
	aaudio_result_t result = AAUDIO_ERROR_BASE;
	void *handle;

	if ((handle = dlopen("libaaudio.so", RTLD_NOW)) == NULL){
		ms_warning("[AAudio] Fail to load libAAudio : %s", dlerror());
		result = AAUDIO_ERROR_BASE;
	} else {
		dlerror(); // Clear previous message if present

		AAudioStreamBuilder *builder;
		result = AAudio_createStreamBuilder(&builder);
		if (result != AAUDIO_OK && !builder) {
			ms_error("[AAudio] Couldn't create stream builder: %i / %s", result, AAudio_convertResultToText(result));
		}
	}
	return result;
}

static void android_snd_card_detect(MSSndCardManager *m) {
	SoundDeviceDescription* d = NULL;
	MSDevicesInfo *devices = NULL;
	aaudio_result_t initResult = initAAudio();
	if (initResult == AAUDIO_OK) {
		devices = ms_factory_get_devices_info(m->factory);
		d = ms_devices_info_get_sound_device_description(devices);
		MSSndCard *card = android_snd_card_new(m);
		ms_snd_card_manager_prepend_card(m, card);
	} else {
		ms_warning("[AAudio] Failed to dlopen libAAudio, AAudio MS soundcard unavailable - Initialization exited with the following error code %i / %s", initResult, AAudio_convertResultToText(initResult));
	}
}

static void android_native_snd_card_init(MSSndCard *card) {

}

static void android_native_snd_card_uninit(MSSndCard *card) {
	AAudioContext *ctx = (AAudioContext*)card->data;
	ms_warning("[AAudio] Deletion of AAudio context [%p]", ctx);
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

static MSSndCard* android_snd_card_new(MSSndCardManager *m) {
	MSSndCard* card = NULL;
	SoundDeviceDescription *d = NULL;
	MSDevicesInfo *devices = NULL;

	card = ms_snd_card_new(&android_native_snd_aaudio_card_desc);
	card->name = ms_strdup("android aaudio sound card");

	devices = ms_factory_get_devices_info(m->factory);
	d = ms_devices_info_get_sound_device_description(devices);

	AAudioContext *context = aaudio_context_init();
	if (d->flags & DEVICE_HAS_BUILTIN_AEC) card->capabilities |= MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER;
	context->builtin_aec = true;
	card->latency = d->delay;
	card->data = context;
	if (d->recommended_rate){
		context->samplerate = d->recommended_rate;
	}
	return card;
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

	JNIEnv *env = ms_get_jni_env();
	jclass mediastreamerAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
	if (mediastreamerAndroidContextClass != NULL) {
		jmethodID getSampleRate = env->GetStaticMethodID(mediastreamerAndroidContextClass, "getDeviceFavoriteSampleRate", "()I");
		if (getSampleRate != NULL) {
			jint ret = env->CallStaticIntMethod(mediastreamerAndroidContextClass, getSampleRate);
			DeviceFavoriteSampleRate = (int)ret;
			ms_message("[AAudio] Using %i for sample rate value", DeviceFavoriteSampleRate);
		}
		env->DeleteLocalRef(mediastreamerAndroidContextClass);
	}

	aaudio_result_t initResult = initAAudio();
	if (initResult == AAUDIO_OK) {
		MSDevicesInfo* devices = ms_factory_get_devices_info(factory);
		SoundDeviceDescription* d = ms_devices_info_get_sound_device_description(devices);

		if (d->flags & DEVICE_HAS_CRAPPY_AAUDIO) {
			ms_error("[AAudio] Device is blacklisted, do not create AAudio soundcard");
			return;
		}

		MSSndCardManager *m = ms_factory_get_snd_card_manager(factory);
		MSSndCard *card = android_snd_card_new(m);
		ms_snd_card_manager_prepend_card(m, card);
		ms_message("[AAudio] Soundcard created");
	} else {
		ms_warning("[AAudio] AAudio MS soundcard unavailable - Initialization exited with the following error code %i / %s", initResult, AAudio_convertResultToText(initResult));
	}
}
