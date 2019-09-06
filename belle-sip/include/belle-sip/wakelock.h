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
