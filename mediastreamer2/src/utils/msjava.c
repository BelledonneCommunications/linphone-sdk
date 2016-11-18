/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL

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

#include "mediastreamer2/msjava.h"
#include "mediastreamer2/mscommon.h"

static JavaVM *ms2_vm=NULL;

#ifndef _WIN32
#include <pthread.h>

static pthread_key_t jnienv_key;

/*
 * Do not forget that any log within this routine may cause re-attach of the thread to the jvm because the logs can callback the java application
 * (see LinphoneCoreFactory.setLogHandler() ).
**/
void _android_key_cleanup(void *data){
	JNIEnv *env = (JNIEnv*) data;

	if (env != NULL) {
		ms_message("Thread end, detaching jvm from current thread");
		(*ms2_vm)->DetachCurrentThread(ms2_vm);
		pthread_setspecific(jnienv_key,NULL);
	}
}
#endif

void ms_set_jvm_from_env(JNIEnv *env){
    (*env)->GetJavaVM(env, &ms2_vm);
#ifndef _WIN32
	pthread_key_create(&jnienv_key,_android_key_cleanup);
#endif
}

void ms_set_jvm(JavaVM *vm){
	ms2_vm=vm;
#ifndef _WIN32
	pthread_key_create(&jnienv_key,_android_key_cleanup);
#endif
}

JavaVM *ms_get_jvm(void){
	return ms2_vm;
}

JNIEnv *ms_get_jni_env(void){
	JNIEnv *env=NULL;
	if (ms2_vm==NULL){
		ms_fatal("Calling ms_get_jni_env() while no jvm has been set using ms_set_jvm().");
	}else{
#ifndef _WIN32
		env=(JNIEnv*)pthread_getspecific(jnienv_key);
		if (env==NULL){
			if ((*ms2_vm)->AttachCurrentThread(ms2_vm,&env,NULL)!=0){
				ms_fatal("AttachCurrentThread() failed !");
				return NULL;
			}
			pthread_setspecific(jnienv_key,env);
		}
#else
		ms_fatal("ms_get_jni_env() not implemented on windows.");
#endif
	}
	return env;
}

#ifdef ANDROID

int ms_get_android_sdk_version(void) {
	static int sdk_version = 0;
	if (sdk_version==0){
		/* Get Android SDK version. */
		JNIEnv *env = ms_get_jni_env();
		jclass version_class = (*env)->FindClass(env, "android/os/Build$VERSION");
		jfieldID fid = (*env)->GetStaticFieldID(env, version_class, "SDK_INT", "I");
		sdk_version = (*env)->GetStaticIntField(env, version_class, fid);
		ms_message("SDK version [%i] detected", sdk_version);
		(*env)->DeleteLocalRef(env, version_class);
	}
	return sdk_version;
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_Log_d(JNIEnv* env, jobject thiz, jstring jmsg) {
	const char* msg = jmsg ? (*env)->GetStringUTFChars(env, jmsg, NULL) : NULL;
	ms_debug("%s", msg);
	if (msg) (*env)->ReleaseStringUTFChars(env, jmsg, msg);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_Log_i(JNIEnv* env, jobject thiz, jstring jmsg) {
	const char* msg = jmsg ? (*env)->GetStringUTFChars(env, jmsg, NULL) : NULL;
	ms_message("%s", msg);
	if (msg) (*env)->ReleaseStringUTFChars(env, jmsg, msg);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_Log_w(JNIEnv* env, jobject thiz, jstring jmsg) {
	const char* msg = jmsg ? (*env)->GetStringUTFChars(env, jmsg, NULL) : NULL;
	ms_warning("%s", msg);
	if (msg) (*env)->ReleaseStringUTFChars(env, jmsg, msg);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_Log_e(JNIEnv* env, jobject thiz, jstring jmsg) {
	const char* msg = jmsg ? (*env)->GetStringUTFChars(env, jmsg, NULL) : NULL;
	ms_error("%s", msg);
	if (msg) (*env)->ReleaseStringUTFChars(env, jmsg, msg);
}

#endif
