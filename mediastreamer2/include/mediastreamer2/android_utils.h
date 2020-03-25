/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
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

#ifndef android_utils_h
#define android_utils_h

#include <mediastreamer2/msfilter.h>
#include <jni.h>

enum _DeviceType{
	TELEPHONY,
	AUX_LINE,
	GENERIC_USB,
	HEADSET,
	MICROPHONE,
	EARPIECE,
	HEADPHONES,
	SPEAKER,
	BLUETOOTH,
	UNKNOWN
};

/**
 * Android device type enum.
**/
typedef enum _DeviceType DeviceType;

/**
 * Retrieve preferred sample rate from Mediastreamer Android Context.
**/
int get_preferred_sample_rate();

/**
 * Retrieve all devices in a given direction.
 * Valid values for string dir are "all" "output" "input"
**/
jobject get_all_devices(JNIEnv *env, const char * dir);

/**
 * Retrieve device ID for the device deviceInfo.
**/
unsigned int get_device_id(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve the value of a JV class field fieldName.
**/
int getJVIntField(JNIEnv *env, const char * className, const char * fieldName);

/**
 * Retrieve device type for the device deviceInfo.
**/
DeviceType get_device_type(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve device capabilities (recorder or playback) for the device deviceInfo.
**/
unsigned int get_device_capabilities(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve product name for the device deviceInfo.
**/
char * get_device_product_name(JNIEnv *env, jobject deviceInfo);

#ifdef __cplusplus
extern "C"{
#endif

/**
 * Creates an AcousticEchoCanceler java object for the given session ID so the sound card uses the device's hardware echo canceller if available.
 * Currently only AndroidSound, AndroidSoundDepr and AAudio sound cards support it.
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

