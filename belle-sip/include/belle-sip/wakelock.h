#ifndef WAKE_LOCK_H
#define WAKE_LOCK_H

#include <jni.h>
#include "belle-sip/defs.h"

BELLESIP_EXPORT void bellesip_wake_lock_init(JNIEnv *env, jobject pm);
BELLESIP_EXPORT void bellesip_wake_lock_uninit(JNIEnv *env);

#endif // WALE_LOCK_H
