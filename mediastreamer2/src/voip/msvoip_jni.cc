/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2016 Belledonne Communications, Grenoble, France

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <jni.h>

extern "C" {
	#include <mediastreamer2/msfactory.h>
	#include <mediastreamer2/devices.h>
}

static const char* GetStringUTFChars(JNIEnv* env, jstring string) {
	const char *cstring = string ? env->GetStringUTFChars(string, NULL) : NULL;
	return cstring;
}

static void ReleaseStringUTFChars(JNIEnv* env, jstring string, const char *cstring) {
	if (string) env->ReleaseStringUTFChars(string, cstring);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_Factory_setDeviceInfo(JNIEnv* env, jobject obj,
    jlong factoryPtr, jstring jmanufacturer, jstring jmodel, jstring jplatform, jint flags, jint delay, jint recommended_rate) {
    const char *manufacturer = GetStringUTFChars(env, jmanufacturer);
    const char *model = GetStringUTFChars(env, jmodel);
    const char *platform = GetStringUTFChars(env, jplatform);

    ms_message("Device infos: [%s,%s,%s], Flags: %d, Delay: %d, Rate: %d",manufacturer,model,platform,flags,delay,recommended_rate);
    ms_devices_info_add(((MSFactory *) factoryPtr)->devices_info, manufacturer, model, platform, flags, delay, recommended_rate);

    ReleaseStringUTFChars(env, jmanufacturer, manufacturer);
    ReleaseStringUTFChars(env, jmodel, model);
    ReleaseStringUTFChars(env, jplatform, platform);
}
