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

bool ms_android_is_record_audio_permission_granted() {
	JNIEnv *env = ms_get_jni_env();
	jclass mediastreamerAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
	if (mediastreamerAndroidContextClass != NULL) {
		jmethodID isPermissionGranted =
		    env->GetStaticMethodID(mediastreamerAndroidContextClass, "isRecordAudioPermissionGranted", "()Z");
		if (isPermissionGranted != NULL) {
			jboolean ret = env->CallStaticBooleanMethod(mediastreamerAndroidContextClass, isPermissionGranted);
			ms_message("[Android Audio Utils] is RECORD_AUDIO permission granted? %i", ret);
			env->DeleteLocalRef(mediastreamerAndroidContextClass);
			return (bool)ret;
		}
		env->DeleteLocalRef(mediastreamerAndroidContextClass);
	}
	ms_error("[Android Audio Utils] Failed to retrive RECORD_AUDIO permission state from MediastreamerAndroidContext!");
	return true;
}

int ms_android_get_preferred_buffer_size() {
	int DevicePreferredBufferSize = -1;
	JNIEnv *env = ms_get_jni_env();
	jclass mediastreamerAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
	if (mediastreamerAndroidContextClass != NULL) {
		jmethodID getBufferSize =
		    env->GetStaticMethodID(mediastreamerAndroidContextClass, "getDeviceFavoriteBufferSize", "()I");
		if (getBufferSize != NULL) {
			jint ret = env->CallStaticIntMethod(mediastreamerAndroidContextClass, getBufferSize);
			DevicePreferredBufferSize = (int)ret;
			ms_message("[Android Audio Utils] Using %i for buffer size value", DevicePreferredBufferSize);
		}
		env->DeleteLocalRef(mediastreamerAndroidContextClass);
	}

	if (DevicePreferredBufferSize == -1) {
		ms_error("[Android Audio Utils] Failed to retrive sample rate - keeping value %0d", DevicePreferredBufferSize);
	}

	return DevicePreferredBufferSize;
}

int ms_android_get_preferred_sample_rate() {
	int DevicePreferredSampleRate = -1;
	JNIEnv *env = ms_get_jni_env();
	jclass mediastreamerAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
	if (mediastreamerAndroidContextClass != NULL) {
		jmethodID getSampleRate =
		    env->GetStaticMethodID(mediastreamerAndroidContextClass, "getDeviceFavoriteSampleRate", "()I");
		if (getSampleRate != NULL) {
			jint ret = env->CallStaticIntMethod(mediastreamerAndroidContextClass, getSampleRate);
			DevicePreferredSampleRate = (int)ret;
			ms_message("[Android Audio Utils] Using %i for sample rate value", DevicePreferredSampleRate);
		}
		env->DeleteLocalRef(mediastreamerAndroidContextClass);
	}

	if (DevicePreferredSampleRate == -1) {
		ms_error("[Android Audio Utils] Failed to retrive sample rate - keeping value %0d", DevicePreferredSampleRate);
	}

	return DevicePreferredSampleRate;
}

jobjectArray ms_android_get_all_devices(JNIEnv *env, const char *dir) {
	jclass mediastreamerAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");

	jobjectArray audioDevices = NULL;

	if (mediastreamerAndroidContextClass != NULL) {
		jmethodID getAudioDevicesId = env->GetStaticMethodID(mediastreamerAndroidContextClass, "getAudioDevices",
		                                                     "(Ljava/lang/String;)[Landroid/media/AudioDeviceInfo;");
		if (getAudioDevicesId != NULL) {
			jstring device_dir = env->NewStringUTF(dir);
			audioDevices = reinterpret_cast<jobjectArray>(
			    env->CallStaticObjectMethod(mediastreamerAndroidContextClass, getAudioDevicesId, device_dir));

			if (audioDevices == NULL) {
				ms_error("[Android Audio Utils] Failed to convert local ref to audio devices to global ref");
			}
		}
		env->DeleteLocalRef(mediastreamerAndroidContextClass);
	}

	return audioDevices;
}

unsigned int ms_android_get_device_id(JNIEnv *env, jobject deviceInfo) {
	unsigned int id = 0;

	jclass audioDeviceInfoClass = env->FindClass("android/media/AudioDeviceInfo");
	if (audioDeviceInfoClass != NULL) {
		jmethodID getIDID = env->GetMethodID(audioDeviceInfoClass, "getId", "()I");
		if (getIDID != NULL) {
			id = (int)env->CallIntMethod(deviceInfo, getIDID);
		}
		env->DeleteLocalRef(audioDeviceInfoClass);
	}

	return id;
}

int ms_android_getJVIntField(JNIEnv *env, const char *className, const char *fieldName) {
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

bool ms_android_is_audio_route_changes_disabled(JNIEnv *env) {
	bool disabled = false;

	jclass mediastreamerAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
	if (mediastreamerAndroidContextClass != NULL) {
		jmethodID audioRouteChangesDisabled =
		    env->GetStaticMethodID(mediastreamerAndroidContextClass, "isAudioRouteChangesDisabled", "()Z");
		if (audioRouteChangesDisabled != NULL) {
			disabled = (bool)env->CallStaticBooleanMethod(mediastreamerAndroidContextClass, audioRouteChangesDisabled);
		}
		env->DeleteLocalRef(mediastreamerAndroidContextClass);
	}

	return disabled;
}

void ms_android_change_device(JNIEnv *env, int deviceID, MSSndCardDeviceType type) {
	if (ms_android_is_audio_route_changes_disabled(env)) return;

	jclass mediastreamerAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
	if (mediastreamerAndroidContextClass != NULL) {
		bool changeDone = false;
		if (ms_android_get_sdk_version(env) >= 31) {
			if (deviceID == -1) {
				jmethodID clearDevice =
				    env->GetStaticMethodID(mediastreamerAndroidContextClass, "clearCommunicationDevice", "()V");
				if (clearDevice != NULL) {
					env->CallStaticVoidMethod(mediastreamerAndroidContextClass, clearDevice);
					ms_message("[Android Audio Utils] Communication device cleared");
					changeDone = true;
				}
			} else {
				jmethodID changeDevice =
				    env->GetStaticMethodID(mediastreamerAndroidContextClass, "setCommunicationDevice", "(I)Z");
				if (changeDevice != NULL) {
					bool result =
					    env->CallStaticBooleanMethod(mediastreamerAndroidContextClass, changeDevice, deviceID);
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
		}

		if (!changeDone) {
			std::string methodName;
			if (type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_SPEAKER) {
				methodName = "enableSpeaker";
			} else if (type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH ||
			           type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEARING_AID) {
				methodName = "startBluetooth";
			} else if (type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_EARPIECE ||
			           type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADSET ||
			           type == MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADPHONES) {
				methodName = "enableEarpiece";
			}

			if (methodName.empty()) {
				ms_error("[Android Audio Utils] Unable to find method to enable device type %s",
				         ms_snd_card_device_type_to_string(type));
			} else {
				jmethodID changeDevice =
				    env->GetStaticMethodID(mediastreamerAndroidContextClass, methodName.c_str(), "()V");
				if (changeDevice != NULL) {
					env->CallStaticVoidMethod(mediastreamerAndroidContextClass, changeDevice);
					ms_message("[Android Audio Utils] changing device to %s ", ms_snd_card_device_type_to_string(type));
				}
			}
		}
	}
	env->DeleteLocalRef(mediastreamerAndroidContextClass);
}

void ms_android_set_bt_enable(JNIEnv *env, const bool_t enable) {
	if (ms_android_is_audio_route_changes_disabled(env)) return;

	std::string methodName;
	if (enable) {
		methodName = "startBluetooth";
	} else {
		methodName = "stopBluetooth";
	}

	if (methodName.empty()) {
		ms_error("[Android Audio Utils] Unable to find method to toggle bluetooth enable");
	} else {
		jclass mediastreamerAndroidContextClass =
		    env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
		if (mediastreamerAndroidContextClass != NULL) {
			jmethodID toggleBTEnable =
			    env->GetStaticMethodID(mediastreamerAndroidContextClass, methodName.c_str(), "()V");
			if (toggleBTEnable != NULL) {
				env->CallStaticVoidMethod(mediastreamerAndroidContextClass, toggleBTEnable);
				ms_debug("[Android Audio Utils] setting enable for bluetooth devices to %s",
				         (enable) ? "true" : "false");
			}
			env->DeleteLocalRef(mediastreamerAndroidContextClass);
		}
	}
}

void ms_android_hack_volume(JNIEnv *env) {
	jclass mediastreamerAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
	if (mediastreamerAndroidContextClass != NULL) {
		jmethodID hackVolume = env->GetStaticMethodID(mediastreamerAndroidContextClass, "hackVolume", "()V");
		if (hackVolume != NULL) {
			env->CallStaticVoidMethod(mediastreamerAndroidContextClass, hackVolume);
		}
		env->DeleteLocalRef(mediastreamerAndroidContextClass);
	}
}

static bool ms_android_is_device_type_headset(JNIEnv *env, const char *audioDeviceInfoClassName, int typeID) {
	int androidSDK = ms_android_get_sdk_version(env);
	if (androidSDK >= 31) {
		if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_BLE_HEADSET")) {
			return true;
		}
	}
	if (androidSDK >= 26) {
		if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_USB_HEADSET")) {
			return true;
		}
	}
	return typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_WIRED_HEADSET");
}

static bool ms_android_is_device_type_speaker(JNIEnv *env, const char *audioDeviceInfoClassName, int typeID) {
	int androidSDK = ms_android_get_sdk_version(env);
	if (androidSDK >= 31) {
		if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_BLE_SPEAKER")) {
			return true;
		}
	}
	if (androidSDK >= 30) {
		if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_BUILTIN_SPEAKER_SAFE")) {
			return true;
		}
	}
	return typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_BUILTIN_SPEAKER");
}

MSSndCardDeviceType ms_android_get_device_type(JNIEnv *env, jobject deviceInfo) {
	int typeID = -1;
	const char *audioDeviceInfoClassName = "android/media/AudioDeviceInfo";

	jclass audioDeviceInfoClass = env->FindClass(audioDeviceInfoClassName);
	if (audioDeviceInfoClass != NULL) {
		jmethodID getTypeID = env->GetMethodID(audioDeviceInfoClass, "getType", "()I");
		if (getTypeID != NULL) {
			typeID = (int)env->CallIntMethod(deviceInfo, getTypeID);
		}
		env->DeleteLocalRef(audioDeviceInfoClass);
	}

	MSSndCardDeviceType deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_UNKNOWN;
	if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_BLUETOOTH_SCO")) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH;
	} else if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_BLUETOOTH_A2DP")) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_BLUETOOTH_A2DP;
	} else if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_BUILTIN_EARPIECE")) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_EARPIECE;
	} else if (ms_android_is_device_type_speaker(env, audioDeviceInfoClassName, typeID)) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_SPEAKER;
	} else if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_BUILTIN_MIC")) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_MICROPHONE;
	} else if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_WIRED_HEADPHONES")) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADPHONES;
	} else if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_USB_DEVICE")) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_GENERIC_USB;
	} else if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_AUX_LINE")) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_AUX_LINE;
	} else if (typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_TELEPHONY")) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_TELEPHONY;
	} else if (ms_android_is_device_type_headset(env, audioDeviceInfoClassName, typeID)) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEADSET;
	} else if (ms_android_get_sdk_version(env) >= 28 &&
	           typeID == ms_android_getJVIntField(env, audioDeviceInfoClassName, "TYPE_HEARING_AID")) {
		deviceType = MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_HEARING_AID;
	} else {
		ms_error("[Android Audio Utils] Unknown device type for type ID %0d", typeID);
	}

	return deviceType;
}

unsigned int ms_android_get_device_capabilities(JNIEnv *env, jobject deviceInfo) {
	unsigned int cap = MS_SND_CARD_CAP_DISABLED;

	jclass audioDeviceInfoClass = env->FindClass("android/media/AudioDeviceInfo");
	if (audioDeviceInfoClass != NULL) {
		jmethodID isSinkID = env->GetMethodID(audioDeviceInfoClass, "isSink", "()Z");
		if (isSinkID != NULL) {
			jboolean isSink = env->CallBooleanMethod(deviceInfo, isSinkID);
			if (isSink) {
				cap |= MS_SND_CARD_CAP_PLAYBACK;
			}
		}

		jmethodID isSourceID = env->GetMethodID(audioDeviceInfoClass, "isSource", "()Z");
		if (isSourceID != NULL) {
			jboolean isSource = env->CallBooleanMethod(deviceInfo, isSourceID);
			if (isSource) {
				cap |= MS_SND_CARD_CAP_CAPTURE;
			}
		}
		env->DeleteLocalRef(audioDeviceInfoClass);
	}
	return cap;
}

char *ms_android_get_device_product_name(JNIEnv *env, jobject deviceInfo) {
	char *productName = NULL;

	jclass audioDeviceInfoClass = env->FindClass("android/media/AudioDeviceInfo");
	if (audioDeviceInfoClass != NULL) {
		jmethodID getProductNameID =
		    env->GetMethodID(audioDeviceInfoClass, "getProductName", "()Ljava/lang/CharSequence;");
		if (getProductNameID != NULL) {
			jobject productNameChar = env->CallObjectMethod(deviceInfo, getProductNameID);
			if (productNameChar != NULL) {
				jclass charSequenceClass = env->FindClass("java/lang/CharSequence");
				if (charSequenceClass != NULL) {
					jmethodID toStringID = env->GetMethodID(charSequenceClass, "toString", "()Ljava/lang/String;");
					if (toStringID != NULL) {
						jstring productNameString =
						    static_cast<jstring>(env->CallObjectMethod(productNameChar, toStringID));
						char *cProductName = (char *)env->GetStringUTFChars(productNameString, nullptr);
						if (cProductName != NULL) {
							productName = ms_strdup(cProductName);
						}
						env->ReleaseStringUTFChars(productNameString, cProductName);
						env->DeleteLocalRef(productNameString);
					}
				}
			}
		}
		env->DeleteLocalRef(audioDeviceInfoClass);
	}

	return productName;
}

int ms_android_get_sdk_version(JNIEnv *env) {
	int sdkVersion = -1;

	jclass versionClass = env->FindClass("android/os/Build$VERSION");
	if (versionClass != NULL) {
		jfieldID fid = env->GetStaticFieldID(versionClass, "SDK_INT", "I");
		if (fid != NULL) {
			sdkVersion = env->GetStaticIntField(versionClass, fid);
			ms_debug("SDK version [%i] detected", sdkVersion);
		}
		env->DeleteLocalRef(versionClass);
	}

	return sdkVersion;
}
