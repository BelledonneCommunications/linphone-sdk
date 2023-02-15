/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
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

#include "bctoolbox/port.h"
#include "belle-sip/utils.h"
#include "wakelock_internal.h"
#include <pthread.h>

struct _WakeLock {
	JavaVM *jvm;
	jobject powerManager;
	pthread_key_t jniEnvKey;
	jint PARTIAL_WAKE_LOCK;
	jmethodID newWakeLockID;
	jmethodID acquireID;
	jmethodID releaseID;
	int numberOfWakelocks;
	int numberOfWakelocksAcquired;
};

typedef struct _WakeLock WakeLock;

static WakeLock ctx = {.jvm = NULL, .powerManager = NULL, .numberOfWakelocks = 0, .numberOfWakelocksAcquired = 0};

bctbx_mutex_t wakeLockInitMutex = PTHREAD_MUTEX_INITIALIZER;
bctbx_mutex_t wakeLockMutex = PTHREAD_MUTEX_INITIALIZER;

static JNIEnv *get_jni_env(void);
static void jni_key_cleanup(void *data);

static void belle_sip_set_jvm(JNIEnv *env) {
	if (ctx.jvm == NULL) {
		(*env)->GetJavaVM(env, &ctx.jvm);
		pthread_key_create(&ctx.jniEnvKey, jni_key_cleanup);
		belle_sip_message("belle_sip_set_jvm(): initialization succeed");
	} else {
		belle_sip_warning("belle_sip_set_jvm(): the JNIEnv has already been initialized");
	}
}

void belle_sip_wake_lock_init(JNIEnv *env, jobject pm) {
	bctbx_mutex_lock(&wakeLockInitMutex);
	if (ctx.jvm == NULL) {
		belle_sip_set_jvm(env);
	}

	ctx.numberOfWakelocks++;
	belle_sip_debug("bellesip_wake_lock : Number of wake locks = %d", ctx.numberOfWakelocks);

	if (ctx.powerManager == NULL) {
		jclass powerManagerClass;
		jclass wakeLockClass;
		jfieldID fieldID;

		powerManagerClass = (*env)->FindClass(env, "android/os/PowerManager");
		wakeLockClass = (*env)->FindClass(env, "android/os/PowerManager$WakeLock");
		fieldID = (*env)->GetStaticFieldID(env, powerManagerClass, "PARTIAL_WAKE_LOCK", "I");

		ctx.PARTIAL_WAKE_LOCK = (*env)->GetStaticIntField(env, powerManagerClass, fieldID);
		ctx.newWakeLockID = (*env)->GetMethodID(env, powerManagerClass, "newWakeLock",
		                                        "(ILjava/lang/String;)Landroid/os/PowerManager$WakeLock;");
		ctx.acquireID = (*env)->GetMethodID(env, wakeLockClass, "acquire", "()V");
		ctx.releaseID = (*env)->GetMethodID(env, wakeLockClass, "release", "()V");

		ctx.powerManager = (*env)->NewGlobalRef(env, pm);
		belle_sip_message("bellesip_wake_lock_init(): initialization succeed");
	} else {
		belle_sip_warning("bellesip_wake_lock_init(): the wakelock system has already been initialized");
	}
	bctbx_mutex_unlock(&wakeLockInitMutex);
}

void belle_sip_wake_lock_uninit(JNIEnv *env) {
	bctbx_mutex_lock(&wakeLockInitMutex);
	if (ctx.powerManager != NULL) {
		ctx.numberOfWakelocks--;
		belle_sip_debug("bellesip_wake_lock : Number of wake locks = %d", ctx.numberOfWakelocks);
		if (ctx.numberOfWakelocks == 0) {
			(*env)->DeleteGlobalRef(env, ctx.powerManager);
			ctx.powerManager = NULL;
			belle_sip_message("bellesip_wake_lock_uninit(): uninitialization succeed");
		} else if (ctx.numberOfWakelocks < 0) {
			belle_sip_warning("bellesip_wake_lock_uninit(): There is atleast one extra uninit()");
		}
	} else {
		belle_sip_warning("bellesip_wake_lock_uninit(): the wakelock system has already been uninitialized");
	}
	bctbx_mutex_unlock(&wakeLockInitMutex);
}

/**
 * @brief Callback called when a thread terminates it-self.
 * It detaches the thread from the JVM.
 * @param data Unused.
 */
static void jni_key_cleanup(void *data) {
	JNIEnv *env = (JNIEnv *)data;
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
	if (ctx.jvm != NULL) {
		jenv = pthread_getspecific(ctx.jniEnvKey);
		if (jenv == NULL) {
			if ((*ctx.jvm)->AttachCurrentThread(ctx.jvm, &jenv, NULL) == JNI_OK) {
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
	bctbx_mutex_lock(&wakeLockMutex);
	if (ctx.jvm != NULL && ctx.powerManager != NULL) {
		JNIEnv *env;
		if ((env = get_jni_env())) {
			jstring tagString = (*env)->NewStringUTF(env, tag);
			jobject lock =
			    (*env)->CallObjectMethod(env, ctx.powerManager, ctx.newWakeLockID, ctx.PARTIAL_WAKE_LOCK, tagString);
			(*env)->DeleteLocalRef(env, tagString);
			if (lock != NULL) {
				(*env)->CallVoidMethod(env, lock, ctx.acquireID);
				jobject lock2 = (*env)->NewGlobalRef(env, lock);
				(*env)->DeleteLocalRef(env, lock);
				ctx.numberOfWakelocksAcquired++;
				belle_sip_debug("bellesip_wake_lock : Number of wake locks ACQUIRED = %d",
				                ctx.numberOfWakelocksAcquired);
				belle_sip_message("bellesip_wake_lock_acquire(): Android wake lock [%s] acquired [ref=%p]", tag,
				                  (void *)lock2);
				unsigned long lock2value = (unsigned long)lock2;
				belle_sip_message("bellesip_wake_lock_acquire(): cast long of wakelock %lu", lock2value);
				bctbx_mutex_unlock(&wakeLockMutex);
				return (unsigned long)lock2;
			} else {
				belle_sip_message("bellesip_wake_lock_acquire(): wake lock creation failed");
			}
		} else {
			belle_sip_error("bellesip_wake_lock_acquire(): cannot attach current thread to the JVM");
		}
	} else {
		if (ctx.jvm == NULL) belle_sip_error("bellesip_wake_lock_acquire(): cannot acquire wake lock. No JVM found");
		else belle_sip_error("bellesip_wake_lock_acquire(): cannot acquire wake lock. No PowerManager found");
	}
	return 0;
}

void wake_lock_release(unsigned long id) {
	bctbx_mutex_lock(&wakeLockMutex);
	if (ctx.jvm != NULL && ctx.powerManager != NULL) {
		if (id != 0) {
			jobject lock = (jobject)id;
			JNIEnv *env;
			if ((env = get_jni_env())) {
				(*env)->CallVoidMethod(env, lock, ctx.releaseID);
				belle_sip_message("bellesip_wake_lock_release(): Android wake lock released [ref=%p]", (void *)lock);
				ctx.numberOfWakelocksAcquired--;
				belle_sip_debug("bellesip_wake_lock : Number of wake locks ACQUIRED = %d",
				                ctx.numberOfWakelocksAcquired);
				(*env)->DeleteGlobalRef(env, lock);
			} else {
				belle_sip_error("bellesip_wake_lock_release(): cannot attach current thread to the JVM");
			}
		}
	} else {
		if (ctx.jvm == NULL) belle_sip_error("bellesip_wake_lock_release(): cannot release wake lock. No JVM found");
		else belle_sip_error("bellesip_wake_lock_release(): cannot release wake lock. No PowerManager found");
	}
	bctbx_mutex_unlock(&wakeLockMutex);
}
