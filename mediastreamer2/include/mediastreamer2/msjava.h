/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
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

#ifndef msjava_h
#define msjava_h

/* Helper routines for filters that use a jvm with upcalls to perform some processing */

#include <jni.h>
#include <bctoolbox/list.h>

#ifdef __cplusplus
extern "C"{
#endif

void ms_set_jvm(JavaVM *vm);

JavaVM *ms_get_jvm(void);

JNIEnv *ms_get_jni_env(void);

#ifdef __ANDROID__
int ms_get_android_sdk_version(void);
bctbx_list_t *ms_get_android_plugins_list(void);
char * ms_get_android_libraries_path(void);
#endif

#ifdef __cplusplus
}
#endif


#endif

