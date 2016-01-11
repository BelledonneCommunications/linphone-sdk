#ifndef WAKE_LOCK_H
#define WAKE_LOCK_H

#include <jni.h>
#include "belle-sip/defs.h"

BELLE_SIP_BEGIN_DECLS

/**
 * Initialize the Android wake lock system inside Belle-SIP.
 * This function must be called only once when the program starts.
 * @param env A JNI environment
 * @parma pm An android.os.PowerManager java object.
 */
BELLESIP_EXPORT void belle_sip_wake_lock_init(JNIEnv *env, jobject pm);

/**
 * Uninit the the Android wake lock system. This function may be called
 * while the program stopping.
 * @param env A JNI environment.
 */
BELLESIP_EXPORT void belle_sip_wake_lock_uninit(JNIEnv *env);

BELLE_SIP_END_DECLS

#endif // WALE_LOCK_H
