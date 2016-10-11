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

#ifndef msjava_h
#define msjava_h

/* Helper routines for filters that use a jvm with upcalls to perform some processing */

#include <jni.h>

#ifdef __cplusplus
extern "C"{
#endif

void ms_set_jvm(JavaVM *vm);

JavaVM *ms_get_jvm(void);

JNIEnv *ms_get_jni_env(void);

#ifdef ANDROID
int ms_get_android_sdk_version(void);
#endif

#ifdef __cplusplus
}
#endif


#endif

