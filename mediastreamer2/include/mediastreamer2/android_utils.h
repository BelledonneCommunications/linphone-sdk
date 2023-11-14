/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef android_utils_h
#define android_utils_h

#include <jni.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/mssndcard.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _AndroidSoundUtils {
	jclass mediastreamerAndroidContextClass;
	jclass audioDeviceInfoClass;

	jmethodID isRecordAudioPermissionGranted;
	jmethodID isAudioRouteChangesDisabled;
	jmethodID startBluetooth;
	jmethodID stopBluetooth;

	MSDevicesInfo *devices_info;

	int sdkVersion;
	int preferredDeviceBufferSize;
	int preferredDeviceSampleRate;
};

typedef struct _AndroidSoundUtils AndroidSoundUtils;

/**
 * Creates a AndroidSoundUtils struct used for all kind of things related to audio on Android.
 * Do not use anymore directly: it is more convenient to use ms_factory_get_android_sound_utils(), in which case no call
 * to ms_android_sound_utils_release() is necessary.
 **/
MS2_PUBLIC AndroidSoundUtils *ms_android_sound_utils_create(MSFactory *f);

/**
 * Frees a previously created AndroidSoundUtils struct.
 **/
MS2_PUBLIC void ms_android_sound_utils_release(AndroidSoundUtils *utils);

bool_t ms_android_sound_utils_is_audio_route_changes_disabled(const AndroidSoundUtils *utils);

/**
 * Retrieves whether or not RECORD_AUDIO permission has been granted.
 **/
MS2_PUBLIC bool ms_android_sound_utils_is_record_audio_permission_granted(const AndroidSoundUtils *utils);

/**
 * Retrieves preferred buffer size from Mediastreamer Android Context.
 **/
MS2_PUBLIC int ms_android_sound_utils_get_preferred_buffer_size(const AndroidSoundUtils *utils);

/**
 * Retrieves preferred sample rate from Mediastreamer Android Context.
 **/
MS2_PUBLIC int ms_android_sound_utils_get_preferred_sample_rate(const AndroidSoundUtils *utils);

/**
 * Retrieves SDK version the app is running on.
 **/
MS2_PUBLIC int ms_android_sound_utils_get_sdk_version(const AndroidSoundUtils *utils);

/**
 * Make upcalls to enable/disable bluetooth devices from mediastreamer.
 **/
MS2_PUBLIC void ms_android_sound_utils_enable_bluetooth(const AndroidSoundUtils *utils, const bool_t enable);

MS2_PUBLIC jobject ms_android_sound_utils_create_hardware_echo_canceller(const AndroidSoundUtils *utils, int sessionID);

MS2_PUBLIC void ms_android_sound_utils_release_hardware_echo_canceller(const AndroidSoundUtils *utils, jobject haec);

/**
 * Hack required to have volume not set to 0 on some devices
 **/
MS2_PUBLIC void ms_android_sound_utils_hack_volume(const AndroidSoundUtils *utils);

#ifdef __cplusplus
}
#endif

/**
 * Retrieve all devices in a given direction.
 * Valid values for string dir are "all" "output" "input"
 **/
MS2_PUBLIC jobjectArray ms_android_get_all_devices(JNIEnv *env, const char *dir);

/**
 * Retrieve device ID for the device deviceInfo.
 **/
MS2_PUBLIC unsigned int ms_android_get_device_id(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve the value of a JV class field fieldName.
 **/
MS2_PUBLIC int ms_android_getJVIntField(JNIEnv *env, const char *className, const char *fieldName);

/**
 * Retrieve device type for the device deviceInfo.
 **/
MS2_PUBLIC MSSndCardDeviceType ms_android_get_device_type(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve device capabilities (recorder or playback) for the device deviceInfo.
 **/
MS2_PUBLIC unsigned int ms_android_get_device_capabilities(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve product name for the device deviceInfo.
 **/
MS2_PUBLIC char *ms_android_get_device_product_name(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve SDK version the app is running on.
 **/
MS2_PUBLIC int ms_android_get_sdk_version(JNIEnv *env);

/**
 * Make upcalls to change device from mediastreamer.
 **/
MS2_PUBLIC void ms_android_change_device(JNIEnv *env, int deviceID, MSSndCardDeviceType type);

/** Deprecated **/

/**
 * Retrieve whether or not RECORD_AUDIO permission has been granted.
 **/
MS2_PUBLIC bool ms_android_is_record_audio_permission_granted(void);

/**
 * Retrieve preferred buffer size from Mediastreamer Android Context.
 **/
MS2_PUBLIC int ms_android_get_preferred_buffer_size(void);

/**
 * Retrieve preferred sample rate from Mediastreamer Android Context.
 **/
MS2_PUBLIC int ms_android_get_preferred_sample_rate(void);

/**
 * Make upcalls to enable/disable bluetooth devices from mediastreamer.
 **/
MS2_PUBLIC void ms_android_set_bt_enable(JNIEnv *env, const bool_t enable);

/**
 * Hack required to have volume not set to 0 on some devices
 **/
MS2_PUBLIC void ms_android_hack_volume(JNIEnv *env);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates an AcousticEchoCanceler java object for the given session ID so the sound card uses the device's hardware
 *echo canceller if available. Currently only AndroidSound, AndroidSoundDepr and AAudio sound cards support it.
 **/
MS2_PUBLIC jobject ms_android_enable_hardware_echo_canceller(JNIEnv *env, int sessionId);

/**
 * Deletes the AcousticEchoCanceler created by ms_android_enable_hardware_echo_canceller.
 **/
MS2_PUBLIC void ms_android_delete_hardware_echo_canceller(JNIEnv *env, jobject aec);

#ifdef __cplusplus
}
#endif

#endif // android_utils_h
