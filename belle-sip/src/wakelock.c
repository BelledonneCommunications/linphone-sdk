/*
	belle-sip - SIP (RFC3261) library.
	Copyright (C) 2010-2018  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "wakelock_internal.h"
#include "belle-sip/utils.h"
#include <pthread.h>

struct _WakeLock {
	JavaVM *jvm;
	jobject powerManager;
	pthread_key_t jniEnvKey;
	jint PARTIAL_WAKE_LOCK;
	jmethodID newWakeLockID;
	jmethodID acquireID;
	jmethodID releaseID;
};

typedef struct _WakeLock WakeLock;

static WakeLock ctx = {
	.jvm = NULL,
	.powerManager = NULL
};

static JNIEnv *get_jni_env(void);
static void jni_key_cleanup(void *data);

void belle_sip_wake_lock_init(JNIEnv *env, jobject pm) {
	if (ctx.jvm == NULL) {
		jclass powerManagerClass;
		jclass wakeLockClass;
		jfieldID fieldID;

		(*env)->GetJavaVM(env, &ctx.jvm);
		pthread_key_create(&ctx.jniEnvKey, jni_key_cleanup);

		powerManagerClass = (*env)->FindClass(env, "android/os/PowerManager");
		wakeLockClass = (*env)->FindClass(env, "android/os/PowerManager$WakeLock");
		fieldID = (*env)->GetStaticFieldID(env, powerManagerClass, "PARTIAL_WAKE_LOCK", "I");

		ctx.PARTIAL_WAKE_LOCK = (*env)->GetStaticIntField(env, powerManagerClass, fieldID);
		ctx.newWakeLockID = (*env)->GetMethodID(env, powerManagerClass, "newWakeLock", "(ILjava/lang/String;)Landroid/os/PowerManager$WakeLock;");
		ctx.acquireID = (*env)->GetMethodID(env, wakeLockClass, "acquire", "()V");
		ctx.releaseID = (*env)->GetMethodID(env, wakeLockClass, "release", "()V");

		belle_sip_message("bellesip_wake_lock_init(): initialization succeed");
	} else {
		belle_sip_warning("bellesip_wake_lock_init(): the wakelock system has already been initialized");
	}
	if (ctx.powerManager == NULL) {
		ctx.powerManager = (*env)->NewGlobalRef(env, pm);
	}
}

void belle_sip_wake_lock_uninit(JNIEnv *env) {
	ctx.jvm = NULL;
	if(ctx.powerManager != NULL) {
		(*env)->DeleteGlobalRef(env, ctx.powerManager);
		ctx.powerManager = NULL;
	}
}

/**
 * @brief Callback called when a thread terminates it-self.
 * It detaches the thread from the JVM.
 * @param data Unused.
 */
static void jni_key_cleanup(void *data) {
	JNIEnv *env = (JNIEnv*) data;
	belle_sip_message("Thread end. Cleanup wake lock jni environment");
	if (env != NULL) {
		if (ctx.jvm != NULL) {
			(*ctx.jvm)->DetachCurrentThread(ctx.jvm);
			pthread_setspecific(ctx.jniEnvKey, NULL);
		} else {
			belle_sip_error("Wake lock cleanup. No JVM found");
		}
	}
}

/**
 * @brief Get a JNI environment.
 * Get a JNI by attaching the current thread
 * to the internaly stored JVM. That function can be safely
 * called several times.
 * @return JNI environement pointer. NULL if the function fails.
 */
static JNIEnv *get_jni_env(void) {
	JNIEnv *jenv = NULL;
	if(ctx.jvm != NULL) {
		jenv = pthread_getspecific(ctx.jniEnvKey);
		if(jenv == NULL) {
			if((*ctx.jvm)->AttachCurrentThread(ctx.jvm, &jenv, NULL) == JNI_OK) {
				pthread_setspecific(ctx.jniEnvKey, jenv);
				belle_sip_message("get_jni_env(): thread successfuly attached");
			} else {
				belle_sip_fatal("get_jni_env(): thread could not be attached to the JVM");
				jenv = NULL;
			}
		}
	} else {
		belle_sip_error("get_jni_env(): no JVM found");
	}
	return jenv;
}

unsigned long wake_lock_acquire(const char *tag) {
	if(ctx.jvm != NULL && ctx.powerManager != NULL) {
		JNIEnv *env;
		if((env = get_jni_env())) {
			jstring tagString = (*env)->NewStringUTF(env, tag);
			jobject lock = (*env)->CallObjectMethod(env, ctx.powerManager, ctx.newWakeLockID, ctx.PARTIAL_WAKE_LOCK, tagString);
			(*env)->DeleteLocalRef(env, tagString);
			if(lock != NULL) {
				(*env)->CallVoidMethod(env, lock, ctx.acquireID);
				jobject lock2 = (*env)->NewGlobalRef(env, lock);
				(*env)->DeleteLocalRef(env, lock);
				belle_sip_message("bellesip_wake_lock_acquire(): Android wake lock [%s] acquired [ref=%p]", tag, (void *)lock2);
				return (unsigned long)lock2;
			} else {
				belle_sip_message("bellesip_wake_lock_acquire(): wake lock creation failed");
			}
		} else {
			belle_sip_error("bellesip_wake_lock_acquire(): cannot attach current thread to the JVM");
		}
	} else {
		if (ctx.jvm == NULL)
			belle_sip_error("bellesip_wake_lock_acquire(): cannot acquire wake lock. No JVM found");
		else
			belle_sip_error("bellesip_wake_lock_acquire(): cannot acquire wake lock. No PowerManager found");
	}
	return 0;
}


void wake_lock_release(unsigned long id) {
	if(ctx.jvm != NULL && ctx.powerManager != NULL) {
		if(id != 0) {
			jobject lock = (jobject)id;
			JNIEnv *env;
			if((env = get_jni_env())) {
				(*env)->CallVoidMethod(env, lock, ctx.releaseID);
				belle_sip_message("bellesip_wake_lock_release(): Android wake lock released [ref=%p]", (void *)lock);
				(*env)->DeleteGlobalRef(env, lock);
			} else {
				belle_sip_error("bellesip_wake_lock_release(): cannot attach current thread to the JVM");
			}
		}
	} else {
		if(ctx.jvm == NULL)
			belle_sip_error("bellesip_wake_lock_release(): cannot release wake lock. No JVM found");
		else
			belle_sip_error("bellesip_wake_lock_release(): cannot release wake lock. No PowerManager found");
	}
}
