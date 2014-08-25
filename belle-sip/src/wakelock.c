#include "wakelock_internal.h"
#include "belle-sip/utils.h"

static JavaVM *_jvm = NULL;
static jobject _powerManager = NULL;

void bellesip_wake_lock_init(JavaVM *jvm, jobject pm) {
	if(_jvm == NULL) {
		_jvm = jvm;
		_powerManager = pm;
	} else {
		belle_sip_error("bellesip_wake_lock_init(): the wakelock system has already been initialized");
	}
}

unsigned long wake_lock_acquire(const char *tag) {
	if(_jvm != NULL && _powerManager != NULL) {
		JNIEnv *env;
		if((*_jvm)->AttachCurrentThread(_jvm, &env, NULL) == JNI_OK) {
			jclass PowerManager = (*env)->FindClass(env, "android/os/PowerManager");
			jclass WakeLock = (*env)->FindClass(env, "android/os/PowerManager/WakeLock");
			jint PARTIAL_WAKE_LOCK = (*env)->GetStaticIntField(
						env, 
						PowerManager,
						(*env)->GetStaticFieldID(env, PowerManager, "PARTIAL_WAKE_LOCK", "I")
						);
			jobject lock = (*env)->CallObjectMethod(
						env, 
						_powerManager,
						(*env)->GetMethodID(env, PowerManager, "newWakeLock", "(ILjava/lang/String;)Landroid/os/PowerManager/WakeLock;" ),
						PARTIAL_WAKE_LOCK,
						tag);
			(*env)->CallVoidMethod(
						env, 
						lock,
						(*env)->GetMethodID(env, WakeLock, "acquire", "()V"));
			belle_sip_message("Android wake lock acquired [ref=%p]", (void *)lock);
			(*_jvm)->DetachCurrentThread(_jvm);
			return (unsigned long)(*env)->NewGlobalRef(env, lock);
		} else {
			belle_sip_error("bellesip_wake_lock_acquire(): cannot attach current thread to the JVM");
		}
	} else {
		belle_sip_error("bellesip_wake_lock_acquire(): cannot acquire wake lock. No jvm found");
	}
	return 0;
}



void wake_lock_release(unsigned long id) {
	if(_jvm != NULL && _powerManager != NULL) {
		if(id != 0) {
			jobject lock = (jobject)id;
			JNIEnv *env;
			if((*_jvm)->AttachCurrentThread(_jvm, &env, NULL) == JNI_OK) {
				jclass WakeLock = (*env)->FindClass(env, "android/os/PowerManager/WakeLock");
				(*env)->CallVoidMethod(
							env,
							lock,
							(*env)->GetMethodID(env, WakeLock, "release", "()V"));
				belle_sip_message("Android wake lock released [ref=%p]", (void *)lock);
				(*env)->DeleteGlobalRef(env, lock);
				(*_jvm)->DetachCurrentThread(_jvm);
			} else {
				belle_sip_error("bellesip_wake_lock_release(): cannot attach current thread to the JVM");
			}
		}
	} else {
		belle_sip_error("wake_lock_release(): cannot release wake lock. No jvm found");
	}
}
