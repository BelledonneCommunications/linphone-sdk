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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/msjava.h"
#include "mediastreamer2/mscommon.h"

static JavaVM *ms2_vm=NULL;

#ifndef WIN32
#include <pthread.h>

static pthread_key_t jnienv_key;

void _android_key_cleanup(void *data){
	ms_message("Thread end, detaching jvm from current thread");
	JNIEnv* env=(JNIEnv*)pthread_getspecific(jnienv_key);
	if (env != NULL) {
		(*ms2_vm)->DetachCurrentThread(ms2_vm);
		pthread_setspecific(jnienv_key,NULL);
	}
}
#endif



void ms_set_jvm(JavaVM *vm){
	ms2_vm=vm;
#ifndef WIN32
	pthread_key_create(&jnienv_key,_android_key_cleanup);
#endif
}

JavaVM *ms_get_jvm(void){
	return ms2_vm;
}

JNIEnv *ms_get_jni_env(void){
	JNIEnv *env=NULL;
	if (ms2_vm==NULL){
		ms_fatal("Calling ms_get_jni_env() while no jvm has been set using ms_set_jvm().");
	}else{
#ifndef WIN32
		env=(JNIEnv*)pthread_getspecific(jnienv_key);
		if (env==NULL){
			if ((*ms2_vm)->AttachCurrentThread(ms2_vm,&env,NULL)!=0){
				ms_fatal("AttachCurrentThread() failed !");
				return NULL;
			}
			pthread_setspecific(jnienv_key,env);
		}
#else
		ms_fatal("ms_get_jni_env() not implemented on windows.");
#endif
	}
	return env;
}



