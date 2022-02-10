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

#include <mediastreamer2/android_utils.h>

jobject ms_android_enable_hardware_echo_canceller(JNIEnv *env, int sessionId) {
	ms_message("[HAEC] Creating AcousticEchoCanceler");
	jobject aec = NULL;
	jclass aecClass = env->FindClass("android/media/audiofx/AcousticEchoCanceler");
	if (aecClass==NULL){
		ms_error("[HAEC] Couldn't find android/media/audiofx/AcousticEchoCanceler class !");
		env->ExceptionClear(); //very important.
		return NULL;
	}
	//aecClass= (jclass)env->NewGlobalRef(aecClass);
	jmethodID isAvailableID = env->GetStaticMethodID(aecClass,"isAvailable","()Z");
	if (isAvailableID!=NULL){
		jboolean ret=env->CallStaticBooleanMethod(aecClass,isAvailableID);
		if (ret){
			jmethodID createID = env->GetStaticMethodID(aecClass,"create","(I)Landroid/media/audiofx/AcousticEchoCanceler;");
			if (createID!=NULL){
				aec=env->CallStaticObjectMethod(aecClass,createID,sessionId);
				if (aec){
					aec=env->NewGlobalRef(aec);
					ms_message("[HAEC] AcousticEchoCanceler successfully created.");
					jclass effectClass=env->FindClass("android/media/audiofx/AudioEffect");
					if (effectClass){
						//effectClass=(jclass)env->NewGlobalRef(effectClass);
						jmethodID isEnabledID = env->GetMethodID(effectClass,"getEnabled","()Z");
						jmethodID setEnabledID = env->GetMethodID(effectClass,"setEnabled","(Z)I");
						if (isEnabledID && setEnabledID){
							jboolean enabled=env->CallBooleanMethod(aec,isEnabledID);
							ms_message("[HAEC] AcousticEchoCanceler enabled: %i",(int)enabled);
							if (!enabled){
								int ret=env->CallIntMethod(aec,setEnabledID,TRUE);
								if (ret!=0){
									ms_error("[HAEC] Could not enable AcousticEchoCanceler: %i",ret);
								} else {
									ms_message("[HAEC] AcousticEchoCanceler enabled");
								}
							} else {
								ms_warning("[HAEC] AcousticEchoCanceler already enabled");
							}
						} else {
							ms_error("[HAEC] Couldn't find either getEnabled or setEnabled method in AudioEffect class for AcousticEchoCanceler !");
						}
						env->DeleteLocalRef(effectClass);
					} else {
						ms_error("[HAEC] Couldn't find android/media/audiofx/AudioEffect class !");
					}
				}else{
					ms_error("[HAEC] Failed to create AcousticEchoCanceler !");
				}
			}else{
				ms_error("[HAEC] create() not found in class AcousticEchoCanceler !");
				env->ExceptionClear(); //very important.
			}
		} else {
			ms_error("[HAEC] AcousticEchoCanceler isn't available !");
		}
	}else{
		ms_error("[HAEC] isAvailable() not found in class AcousticEchoCanceler !");
		env->ExceptionClear(); //very important.
	}
	env->DeleteLocalRef(aecClass);
	return aec;
}

void ms_android_delete_hardware_echo_canceller(JNIEnv *env, jobject aec) {
	ms_message("[HAEC] Deleting AcousticEchoCanceler");
	jclass effectClass = env->FindClass("android/media/audiofx/AudioEffect");
	if (effectClass) {
		jmethodID releaseID = env->GetMethodID(effectClass, "release", "()V");
		if (releaseID) {
			env->CallVoidMethod(aec, releaseID);
			ms_message("[HAEC] AcousticEchoCanceler released");
		} else {
			ms_error("[HAEC] Couldn't find release() method in AudioEffect class for AcousticEchoCanceler !");
		}
		env->DeleteLocalRef(effectClass);
	} else {
		ms_error("[HAEC] Couldn't find android/media/audiofx/AudioEffect class !");
	}
	env->DeleteGlobalRef(aec);
}