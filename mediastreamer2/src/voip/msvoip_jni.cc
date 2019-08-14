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

#include <jni.h>

#include <mediastreamer2/msfactory.h>
#include <mediastreamer2/devices.h>

extern "C" {

static const char *GetStringUTFChars(JNIEnv *env, jstring string) {
	const char *cstring = string ? env->GetStringUTFChars(string, NULL) : NULL;
	return cstring;
}

static void ReleaseStringUTFChars(JNIEnv *env, jstring string, const char *cstring) {
	if (string) env->ReleaseStringUTFChars(string, cstring);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_Factory_setDeviceInfo(JNIEnv *env, jobject obj,
		jlong factoryPtr, jstring jmanufacturer, jstring jmodel, jstring jplatform, jint flags, jint delay, jint recommended_rate) {
	const char *manufacturer = GetStringUTFChars(env, jmanufacturer);
	const char *model = GetStringUTFChars(env, jmodel);
	const char *platform = GetStringUTFChars(env, jplatform);

	ms_message("Device infos: [%s,%s,%s], Flags: %d, Delay: %d, Rate: %d", manufacturer, model, platform, flags, delay, recommended_rate);
	ms_devices_info_add(((MSFactory *) factoryPtr)->devices_info, manufacturer, model, platform, flags, delay, recommended_rate);

	ReleaseStringUTFChars(env, jmanufacturer, manufacturer);
	ReleaseStringUTFChars(env, jmodel, model);
	ReleaseStringUTFChars(env, jplatform, platform);
}

JNIEXPORT jint JNICALL Java_org_linphone_mediastream_Factory_getDeviceFlags(JNIEnv *env, jobject obj, jlong factoryPtr) {
	return ms_devices_info_get_sound_device_description(((MSFactory *) factoryPtr)->devices_info)->flags;
}

} // extern "C"
