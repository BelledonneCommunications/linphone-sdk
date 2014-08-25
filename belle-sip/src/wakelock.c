#include "wakelock_internal.h"
#include "belle-sip/utils.h"

static JavaVM *_jvm = NULL;
static jobject _powerManager = NULL;

void bellesip_wake_lock_init(JavaVM *jvm, jobject powerManager) {
	if(_jvm == NULL) {
		_jvm = jvm;
		_powerManager = powerManager;
	} else {
		ms_error("bellesip_wake_lock_init(): the wakelock system has already been initialized");
	}
}

unsigned long wake_lock_acquire(const char *tag) {
	if(_jvm != NULL && _powerManager != NULL) {
		JNIEnv *env;
		if(AttachCurrentThread(_jvm, &env, NULL) == JNI_OK) {
			jclass PowerManager = FindClass(env, "android/os/PowerManager");
			jclass WakeLock = FindClass(env, "android/os/PowerManager/WakeLock");
			jint PARTIAL_WAKE_LOCK = getStaticIntField(
						env,
						powerManager,
						GetFieldId(env, PowerManager, "PARTIAL_WAKE_LOCK", "I")
						);
			jobject lock = CallObjectMethod(
						env,
						_powerManager,
						GetMethodID(env, PowerManager, "newWakeLock", "(ILjava/lang/String;)Landroid/os/PowerManager/WakeLock;" ),
						PARTIAL_WAKE_LOCK,
						tag);
			CallVoidMethod(
						env,
						lock,
						GetMethodID(env, WakeLock, "acquire", ""));
			DetachCurrentThread(_jvm);
			return (unsigned long)NewGlobalRef(env, lock);
		} else {
			ms_error("bellesip_wake_lock_acquire(): cannot attach current thread to the JVM");
		}
	} else {
		ms_error("bellesip_wake_lock_acquire(): cannot acquire wake lock. No jvm found");
	}
	return 0;
}

void wake_lock_release(unsigned long id) {
	if(_jvm != NULL && _powerManager != NULL) {
		if(id != 0) {
			jobject lock = (jobject)id;
			JNIEnv *env;
			if(AttachCurrentThread(_jvm, &env, NULL) == JNI_OK) {
				jclass WakeLock = FindClass(env, "android/os/PowerManager/WakeLock");
				CallVoidMethod(
							env,
							lock,
							GetMethodID(env, WakeLock, "release", "()V"));
				DeleteGlobalRef(env, lock);
				DetachCurrentThread(_jvm);
			} else {
				ms_error("bellesip_wake_lock_release(): cannot attach current thread to the JVM");
			}
		}
	} else {
		ms_error("wake_lock_release(): cannot release wake lock. No jvm found");
	}
}
