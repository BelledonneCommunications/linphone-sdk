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

#include <mediastreamer2/android_utils.h>

#include "mediastreamer2/zrtp.h"
#include <cpu-features.h>

#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT jboolean JNICALL Java_org_linphone_mediastream_Version_nativeHasNeon(BCTBX_UNUSED(JNIEnv *env),
                                                                               BCTBX_UNUSED(jclass c)) {
	if (android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM &&
	    (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0) {
		return 1;
	}
	return 0;
}

JNIEXPORT jboolean JNICALL Java_org_linphone_mediastream_Version_nativeHasZrtp(BCTBX_UNUSED(JNIEnv *env),
                                                                               BCTBX_UNUSED(jclass c)) {
	return ms_zrtp_available();
}

JNIEXPORT jboolean JNICALL Java_org_linphone_mediastream_Version_nativeHasVideo(BCTBX_UNUSED(JNIEnv *env),
                                                                                BCTBX_UNUSED(jclass c)) {
#ifdef VIDEO_ENABLED
	return JNI_TRUE;
#else
	return JNI_FALSE;
#endif
}

#ifdef __cplusplus
}
#endif

static const char *GetStringUTFChars(JNIEnv *env, jstring string) {
	const char *cstring = string ? env->GetStringUTFChars(string, nullptr) : nullptr;
	return cstring;
}

static void ReleaseStringUTFChars(JNIEnv *env, jstring string, const char *cstring) {
	if (string) env->ReleaseStringUTFChars(string, cstring);
}

static int ms_android_getJVIntField(JNIEnv *env, const char *className, const char *fieldName) {
	int value = -1;
	jclass JVClass = env->FindClass(className);
	if (JVClass != NULL) {
		jfieldID fieldID = env->GetStaticFieldID(JVClass, fieldName, "I");
		if (fieldID != NULL) {
			value = (int)env->GetStaticIntField(JVClass, fieldID);
		}
		env->DeleteLocalRef(JVClass);
	}

	return value;
}

AndroidSoundUtils *ms_android_sound_utils_create(MSFactory *factory) {
	JNIEnv *env = ms_get_jni_env();
	AndroidSoundUtils *utils = (AndroidSoundUtils *)ms_new0(AndroidSoundUtils, 1);

	jclass contextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
	if (contextClass != nullptr) {
		utils->mediastreamerAndroidContextClass = (jclass)env->NewGlobalRef(contextClass);
		env->DeleteLocalRef(contextClass);

		utils->isRecordAudioPermissionGranted =
		    env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "isRecordAudioPermissionGranted", "()Z");
		utils->isCameraPermissionGranted =
		    env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "isCameraPermissionGranted", "()Z");
		utils->isAudioRouteChangesDisabled =
		    env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "isAudioRouteChangesDisabled", "()Z");
		utils->startBluetooth =
		    env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "startBluetooth", "()V");
		utils->stopBluetooth = env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "stopBluetooth", "()V");
		utils->enableEarpiece =
		    env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "enableEarpiece", "()V");
		utils->enableSpeaker = env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "enableSpeaker", "()V");

		utils->hackVolumeOnStream =
		    env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "hackVolumeOnStream", "(I)V");

		utils->getAudioDevices = env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "getAudioDevices",
		                                                "(Ljava/lang/String;)[Landroid/media/AudioDeviceInfo;");
		utils->clearCommunicationDevice =
		    env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "clearCommunicationDevice", "()V");
		utils->setCommunicationDevice =
		    env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "setCommunicationDevice", "(I)Z");
	} else {
		ms_error("[Android Audio Utils] Failed to find org/linphone/mediastream/MediastreamerAndroidContext class!");
	}

	jclass versionClass = env->FindClass("android/os/Build$VERSION");
	if (versionClass != nullptr) {
		jfieldID fid = env->GetStaticFieldID(versionClass, "SDK_INT", "I");
		if (fid != nullptr) {
			utils->sdkVersion = env->GetStaticIntField(versionClass, fid);
			ms_message("[Android Audio Utils] SDK version [%i] detected", utils->sdkVersion);
		} else {
			ms_error("[Android Audio Utils] Failed to find SDK_INT static field in android/os/Build$VERSION class!");
		}
		env->DeleteLocalRef(versionClass);
	} else {
		ms_error("[Android Audio Utils] Failed to find android/os/Build$VERSION class!");
	}

	jclass deviceClass = env->FindClass("android/media/AudioDeviceInfo");
	if (deviceClass != nullptr) {
		utils->audioDeviceInfoClass = (jclass)env->NewGlobalRef(deviceClass);

		utils->getAudioDeviceId = env->GetMethodID(utils->audioDeviceInfoClass, "getId", "()I");
		utils->getAudioDeviceAddress =
		    env->GetMethodID(utils->audioDeviceInfoClass, "getAddress", "()Ljava/lang/String;");
		utils->getAudioDeviceType = env->GetMethodID(utils->audioDeviceInfoClass, "getType", "()I");
		utils->isAudioDeviceSink = env->GetMethodID(utils->audioDeviceInfoClass, "isSink", "()Z");
		utils->isAudioDeviceSource = env->GetMethodID(utils->audioDeviceInfoClass, "isSource", "()Z");
		utils->getAudioDeviceProductName =
		    env->GetMethodID(utils->audioDeviceInfoClass, "getProductName", "()Ljava/lang/CharSequence;");

		utils->audioDeviceTypeBluetoothSco =
		    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_BLUETOOTH_SCO", "I"));
		utils->audioDeviceTypeBluetoothA2dp =
		    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_BLUETOOTH_A2DP", "I"));
		utils->audioDeviceTypeBuiltinEarpiece =
		    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_BUILTIN_EARPIECE", "I"));
		utils->audioDeviceTypeBuiltinMicrophone =
		    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_BUILTIN_MIC", "I"));
		utils->audioDeviceTypeWiredHeadphones =
		    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_WIRED_HEADPHONES", "I"));
		utils->audioDeviceTypeUsbDevice =
		    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_USB_DEVICE", "I"));
		utils->audioDeviceTypeAuxLine =
		    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_AUX_LINE", "I"));
		utils->audioDeviceTypeTelephony =
		    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_TELEPHONY", "I"));
		utils->audioDeviceTypeWiredHeadset =
		    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_WIRED_HEADSET", "I"));
		utils->audioDeviceTypeBuiltinSpeaker =
		    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_BUILTIN_SPEAKER", "I"));

		utils->audioDeviceTypeBluetoothSpeaker = -1;
		utils->audioDeviceTypeBuiltinSpeakerSafe = -1;
		if (utils->sdkVersion >= 31) {
			utils->audioDeviceTypeBluetoothSpeaker =
			    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_BLE_SPEAKER", "I"));
		}
		if (utils->sdkVersion >= 31) {
			utils->audioDeviceTypeBuiltinSpeakerSafe = (int)env->GetStaticIntField(
			    deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_BUILTIN_SPEAKER_SAFE", "I"));
		}

		utils->audioDeviceTypeHearingAid = -1;
		if (utils->sdkVersion >= 28) {
			utils->audioDeviceTypeHearingAid =
			    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_HEARING_AID", "I"));
		}

		utils->audioDeviceTypeBluetoothHeadset = -1;
		utils->audioDeviceTypeUsbHeadset = -1;
		if (utils->sdkVersion >= 31) {
			utils->audioDeviceTypeBluetoothHeadset =
			    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_BLE_HEADSET", "I"));
		}
		if (utils->sdkVersion >= 26) {
			utils->audioDeviceTypeUsbHeadset =
			    (int)env->GetStaticIntField(deviceClass, env->GetStaticFieldID(deviceClass, "TYPE_USB_HEADSET", "I"));
		}

		env->DeleteLocalRef(deviceClass);
	} else {
		ms_error("[Android Audio Utils] Failed to find android/media/AudioDeviceInfo class!");
	}

	utils->preferredDeviceBufferSize = -1;
	jmethodID getBufferSize =
	    env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "getDeviceFavoriteBufferSize", "()I");
	if (getBufferSize != nullptr) {
		int buffer_size = (int)env->CallStaticIntMethod(utils->mediastreamerAndroidContextClass, getBufferSize);
		ms_message("[Android Audio Utils] Device's preferred buffer size is %i", buffer_size);
		utils->preferredDeviceBufferSize = buffer_size;
	} else {
		ms_error("[Android Audio Utils] Failed to find getDeviceFavoriteBufferSize() method from "
		         "MediastreamerAndroidContext class!");
	}

	utils->preferredDeviceSampleRate = 44100;
	jmethodID getSampleRate =
	    env->GetStaticMethodID(utils->mediastreamerAndroidContextClass, "getDeviceFavoriteSampleRate", "()I");
	if (getSampleRate != nullptr) {
		int sample_rate = (int)env->CallStaticIntMethod(utils->mediastreamerAndroidContextClass, getSampleRate);
		ms_message("[Android Audio Utils] Device's preferred sample rate is %i", sample_rate);
		utils->preferredDeviceSampleRate = sample_rate;
	} else {
		ms_error("[Android Audio Utils] Failed to find getDeviceFavoriteSampleRate() method from "
		         "MediastreamerAndroidContext class!");
	}
	utils->devices_info = ms_factory_get_devices_info(factory);

	return utils;
}

void ms_android_sound_utils_release(AndroidSoundUtils *utils) {
	JNIEnv *env = ms_get_jni_env();

	env->DeleteGlobalRef(utils->mediastreamerAndroidContextClass);
	env->DeleteGlobalRef(utils->audioDeviceInfoClass);

	ms_free(utils);
}

bool ms_android_sound_utils_is_record_audio_permission_granted(const AndroidSoundUtils *utils) {
	JNIEnv *env = ms_get_jni_env();

	if (utils->isRecordAudioPermissionGranted != nullptr) {
		jboolean ret = env->CallStaticBooleanMethod(utils->mediastreamerAndroidContextClass,
		                                            utils->isRecordAudioPermissionGranted);
		ms_message("[Android Audio Utils] is RECORD_AUDIO permission granted? %i", ret);
		return (bool)ret;
	}

	ms_error("[Android Audio Utils] Failed to retrive RECORD_AUDIO permission state from MediastreamerAndroidContext!");
	return true;
}

bool ms_android_sound_utils_is_camera_permission_granted(const AndroidSoundUtils *utils) {
	JNIEnv *env = ms_get_jni_env();

	if (utils->isCameraPermissionGranted != nullptr) {
		jboolean ret =
		    env->CallStaticBooleanMethod(utils->mediastreamerAndroidContextClass, utils->isCameraPermissionGranted);
		ms_message("[Android Audio Utils] is CAMERA permission granted? %i", ret);
		return (bool)ret;
	}

	ms_error("[Android Audio Utils] Failed to retrive CAMERA permission state from MediastreamerAndroidContext!");
	return true;
}

int ms_android_sound_utils_get_preferred_buffer_size(const AndroidSoundUtils *utils) {
	return utils->preferredDeviceBufferSize;
}

int ms_android_sound_utils_get_preferred_sample_rate(const AndroidSoundUtils *utils) {
	if (utils->devices_info) {
		SoundDeviceDescription *sds = ms_devices_info_get_sound_device_description(utils->devices_info);
		if (sds && sds->recommended_rate != 0) return sds->recommended_rate;
	} else ms_error("ms_android_sound_utils_get_preferred_sample_rate(): no DevicesInfo table !");
	return utils->preferredDeviceSampleRate;
}

int ms_android_sound_utils_get_sdk_version(const AndroidSoundUtils *utils) {
	return utils->sdkVersion;
}

bool_t ms_android_sound_utils_is_audio_route_changes_disabled(const AndroidSoundUtils *utils) {
	JNIEnv *env = ms_get_jni_env();

	if (utils->isAudioRouteChangesDisabled != nullptr) {
		return (bool)env->CallStaticBooleanMethod(utils->mediastreamerAndroidContextClass,
		                                          utils->isAudioRouteChangesDisabled);
	}

	return FALSE;
}

void ms_android_sound_utils_enable_bluetooth(const AndroidSoundUtils *utils, const bool_t enable) {
	JNIEnv *env = ms_get_jni_env();

	if (ms_android_sound_utils_is_audio_route_changes_disabled(utils)) {
		return;
	}

	jmethodID bluetooth_method = enable ? utils->startBluetooth : utils->stopBluetooth;
	if (bluetooth_method != nullptr) {
		env->CallStaticVoidMethod(utils->mediastreamerAndroidContextClass, bluetooth_method);
		ms_debug("[Android Audio Utils] setting enable for bluetooth devices to %s", (enable) ? "true" : "false");
	}
}

jobject ms_android_sound_utils_create_hardware_echo_canceller(BCTBX_UNUSED(const AndroidSoundUtils *utils),
                                                              int sessionID) {
	JNIEnv *env = ms_get_jni_env();

	return ms_android_enable_hardware_echo_canceller(env, sessionID);
}

void ms_android_sound_utils_release_hardware_echo_canceller(BCTBX_UNUSED(const AndroidSoundUtils *utils),
                                                            jobject haec) {
	JNIEnv *env = ms_get_jni_env();

	ms_android_delete_hardware_echo_canceller(env, haec);
}

void ms_android_sound_utils_hack_volume(const AndroidSoundUtils *utils, int stream) {
	JNIEnv *env = ms_get_jni_env();

	env->CallStaticVoidMethod(utils->mediastreamerAndroidContextClass, utils->hackVolumeOnStream, stream);
}

jobjectArray ms_android_sound_utils_get_devices(const AndroidSoundUtils *utils, const char *dir) {
	JNIEnv *env = ms_get_jni_env();

	jobjectArray audioDevices = NULL;
	jstring device_dir = env->NewStringUTF(dir);
	audioDevices = reinterpret_cast<jobjectArray>(
	    env->CallStaticObjectMethod(utils->mediastreamerAndroidContextClass, utils->getAudioDevices, device_dir));

	if (audioDevices == NULL) {
		ms_error("[Android Audio Utils] Failed to convert local ref to audio devices to global ref");
	}
	env->DeleteLocalRef(device_dir);

	return audioDevices;
}

unsigned int ms_android_sound_utils_get_device_id(const AndroidSoundUtils *utils, jobject deviceInfo) {
	JNIEnv *env = ms_get_jni_env();

	unsigned int id = (unsigned int)env->CallIntMethod(deviceInfo, utils->getAudioDeviceId);
	return id;
}

char *ms_android_sound_utils_get_microphone_device_address(const AndroidSoundUtils *utils, jobject deviceInfo) {
	JNIEnv *env = ms_get_jni_env();

	char *address = NULL;
	jstring jaddress = (jstring)env->CallObjectMethod(deviceInfo, utils->getAudioDeviceAddress);
	if (jaddress) {
		const char *c_address = GetStringUTFChars(env, jaddress);
		if (c_address != NULL) {
			address = ms_strdup(c_address);
		}
		ReleaseStringUTFChars(env, jaddress, c_address);
	}
	return address;
}

MSSndCardDeviceType ms_android_sound_utils_get_device_type(const AndroidSoundUtils *utils, jobject deviceInfo) {
	JNIEnv *env = ms_get_jni_env();

	int typeID = -1;
	typeID = (int)env->CallIntMethod(deviceInfo, utils->getAudioDeviceType);
	if (typeID == -1) {
		return MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_UNKNOWN;
	}

	MSSndCardDeviceType deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_UNKNOWN;
	if (typeID == utils->audioDeviceTypeBluetoothSco) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH;
	} else if (typeID == utils->audioDeviceTypeBluetoothA2dp) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH_A2DP;
	} else if (typeID == utils->audioDeviceTypeBuiltinEarpiece) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_EARPIECE;
	} else if (typeID == utils->audioDeviceTypeBluetoothSpeaker || typeID == utils->audioDeviceTypeBuiltinSpeakerSafe ||
	           typeID == utils->audioDeviceTypeBuiltinSpeaker) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_SPEAKER;
	} else if (typeID == utils->audioDeviceTypeBuiltinMicrophone) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_MICROPHONE;
	} else if (typeID == utils->audioDeviceTypeWiredHeadphones) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADPHONES;
	} else if (typeID == utils->audioDeviceTypeUsbDevice) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_GENERIC_USB;
	} else if (typeID == utils->audioDeviceTypeAuxLine) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_AUX_LINE;
	} else if (typeID == utils->audioDeviceTypeTelephony) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_TELEPHONY;
	} else if (typeID == utils->audioDeviceTypeBluetoothHeadset || typeID == utils->audioDeviceTypeUsbHeadset ||
	           typeID == utils->audioDeviceTypeWiredHeadset) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADSET;
	} else if (typeID == utils->audioDeviceTypeHearingAid) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEARING_AID;
	} else {
		ms_error("[Android Audio Utils] Unknown device type for type ID %0d", typeID);
	}
	return deviceType;
}

unsigned int ms_android_sound_utils_get_device_capabilities(const AndroidSoundUtils *utils, jobject deviceInfo) {
	JNIEnv *env = ms_get_jni_env();

	unsigned int cap = MS_SND_CARD_CAP_DISABLED;

	jboolean isSink = env->CallBooleanMethod(deviceInfo, utils->isAudioDeviceSink);
	if (isSink) {
		cap |= MS_SND_CARD_CAP_PLAYBACK;
	}

	jboolean isSource = env->CallBooleanMethod(deviceInfo, utils->isAudioDeviceSource);
	if (isSource) {
		cap |= MS_SND_CARD_CAP_CAPTURE;
	}

	return cap;
}

char *ms_android_sound_utils_get_device_product_name(const AndroidSoundUtils *utils, jobject deviceInfo) {
	JNIEnv *env = ms_get_jni_env();

	char *productName = NULL;

	jobject productNameChar = env->CallObjectMethod(deviceInfo, utils->getAudioDeviceProductName);
	if (productNameChar != NULL) {
		jclass charSequenceClass = env->FindClass("java/lang/CharSequence");
		if (charSequenceClass != NULL) {
			jmethodID toStringID = env->GetMethodID(charSequenceClass, "toString", "()Ljava/lang/String;");
			if (toStringID != NULL) {
				jstring jProductName = static_cast<jstring>(env->CallObjectMethod(productNameChar, toStringID));
				const char *cProductName = GetStringUTFChars(env, jProductName);
				if (cProductName != NULL) {
					productName = ms_strdup(cProductName);
				}
				ReleaseStringUTFChars(env, jProductName, cProductName);
			}
		}
	}

	return productName;
}

void ms_android_sound_utils_change_device(const AndroidSoundUtils *utils, int deviceID, MSSndCardDeviceType type) {
	JNIEnv *env = ms_get_jni_env();

	if (ms_android_sound_utils_is_audio_route_changes_disabled(utils)) return;

	bool changeDone = false;
	if (ms_android_sound_utils_get_sdk_version(utils) >= 31) {
		if (deviceID == -1) {
			env->CallStaticVoidMethod(utils->mediastreamerAndroidContextClass, utils->clearCommunicationDevice);
			ms_message("[Android Audio Utils] Communication device cleared");
			changeDone = true;
		} else {
			bool result = env->CallStaticBooleanMethod(utils->mediastreamerAndroidContextClass,
			                                           utils->setCommunicationDevice, deviceID);
			if (result) {
				ms_message("[Android Audio Utils] Communication device changed to ID: %i (%s)", deviceID,
				           ms_snd_card_device_type_to_string(type));
				changeDone = true;
			} else {
				ms_error("[Android Audio Utils] Failed to change communication device to ID: %i (%s)", deviceID,
				         ms_snd_card_device_type_to_string(type));
			}
		}
	}

	if (!changeDone) {
		if (type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_SPEAKER) {
			env->CallStaticVoidMethod(utils->mediastreamerAndroidContextClass, utils->enableSpeaker);
		} else if (type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH ||
		           type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEARING_AID) {
			env->CallStaticVoidMethod(utils->mediastreamerAndroidContextClass, utils->startBluetooth);
		} else if (type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_EARPIECE ||
		           type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADSET ||
		           type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADPHONES) {
			env->CallStaticVoidMethod(utils->mediastreamerAndroidContextClass, utils->enableEarpiece);
		} else {
			return;
		}
		ms_message("[Android Audio Utils] changing device to %s ", ms_snd_card_device_type_to_string(type));
	}
}