#include "wakelock_internal.h"
#include "belle-sip/utils.h"
#include <pthread.h>

static JavaVM *_jvm = NULL;
static jobject _powerManager = NULL;
static pthread_key_t jniEnvKey;

static JNIEnv *get_jni_env(void);
static void jni_key_cleanup(void *data);

void bellesip_wake_lock_init(JNIEnv *env, jobject pm) {
	if(_jvm == NULL) {
		(*env)->GetJavaVM(env, &_jvm);
		_powerManager = (*env)->NewGlobalRef(env, pm);
		pthread_key_create(&jniEnvKey, jni_key_cleanup);
		belle_sip_message("bellesip_wake_lock_init(): initialization succeed");
	} else {
		belle_sip_error("bellesip_wake_lock_init(): the wakelock system has already been initialized");
	}
}

void bellesip_wake_lock_uninit(JNIEnv *env) {
	_jvm = NULL;
	if(_powerManager != NULL) {
		(*env)->DeleteGlobalRef(env, _powerManager);
		_powerManager = NULL;
	}
}

static void jni_key_cleanup(void *data) {
	belle_sip_message("Thread end. Cleanup wake lock jni environment");
	JNIEnv *env = pthread_getspecific(jniEnvKey);
	if(env != NULL) {
		if(_jvm != NULL) {
			(*_jvm)->DetachCurrentThread(_jvm);
			pthread_setspecific(jniEnvKey, NULL);
		} else {
			belle_sip_fatal("Wake lock cleanup. No JVM found");
		}
	}
}

static JNIEnv *get_jni_env(void) {
	JNIEnv *jenv = NULL;
	if(_jvm != NULL) {
		jenv = pthread_getspecific(jniEnvKey);
		if(jenv == NULL) {
			if((*_jvm)->AttachCurrentThread(_jvm, &jenv, NULL) == JNI_OK) {
				pthread_setspecific(jniEnvKey, jenv);
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
	if(_jvm != NULL && _powerManager != NULL) {
		JNIEnv *env;
		if(env = get_jni_env()) {
			jclass PowerManager = (*env)->FindClass(env, "android/os/PowerManager");
			jclass WakeLock = (*env)->FindClass(env, "android/os/PowerManager$WakeLock");

			jfieldID fieldID = (*env)->GetStaticFieldID(env, PowerManager, "PARTIAL_WAKE_LOCK", "I");
			jint PARTIAL_WAKE_LOCK = (*env)->GetStaticIntField(env, PowerManager, fieldID);

			jstring tagString = (*env)->NewStringUTF(env, tag);
			jmethodID newWakeLockID = (*env)->GetMethodID(env, PowerManager, "newWakeLock", "(ILjava/lang/String;)Landroid/os/PowerManager$WakeLock;");
			jobject lock = (*env)->CallObjectMethod(env, _powerManager, newWakeLockID, PARTIAL_WAKE_LOCK, tagString);
			(*env)->DeleteLocalRef(env, tagString);

			jmethodID acquireID = (*env)->GetMethodID(env, WakeLock, "acquire", "()V");
			(*env)->CallVoidMethod(env, lock, acquireID);

			belle_sip_message("bellesip_wake_lock_acquire(): Android wake lock acquired [ref=%p]", (void *)lock);
			lock = (*env)->NewGlobalRef(env, lock);
			return (unsigned long)lock;
		} else {
			belle_sip_error("bellesip_wake_lock_acquire(): cannot attach current thread to the JVM");
		}
	} else {
		belle_sip_error("bellesip_wake_lock_acquire(): cannot acquire wake lock. No JVM found");
	}
	return 0;
}



void wake_lock_release(unsigned long id) {
	if(_jvm != NULL && _powerManager != NULL) {
		if(id != 0) {
			jobject lock = (jobject)id;
			JNIEnv *env;
			if(env = get_jni_env()) {
				jclass WakeLock = (*env)->FindClass(env, "android/os/PowerManager$WakeLock");
				(*env)->CallVoidMethod(env, lock, (*env)->GetMethodID(env, WakeLock, "release", "()V"));
				belle_sip_message("wake_lock_release(): Android wake lock released [ref=%p]", (void *)lock);
				(*env)->DeleteGlobalRef(env, lock);
			} else {
				belle_sip_error("bellesip_wake_lock_release(): cannot attach current thread to the JVM");
			}
		}
	} else {
		belle_sip_error("wake_lock_release(): cannot release wake lock. No JVM found");
	}
}
