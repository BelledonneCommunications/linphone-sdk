/*
mediastreamer2 android_mediacodec.cpp
Copyright (C) 2015 Belledonne Communications SARL

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

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msjava.h"

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include "android_mediacodec.h"

////////////////////////////////////////////////////
//                                                //
//                 MEDIA CODEC                    //
//                                                //
////////////////////////////////////////////////////

struct AMediaCodec {
	jobject jcodec;
};

struct AMediaFormat {
	jobject jformat;
};

int handle_java_exception(){
	JNIEnv *env = ms_get_jni_env();
	if (env->ExceptionCheck()) {
		env->ExceptionDescribe();
		env->ExceptionClear();
		return -1;
	}
	return 0;
}

AMediaCodec * AMediaCodec_createDecoderByType(const char *mime_type){
	AMediaCodec *codec=ms_new0(AMediaCodec,1);
	JNIEnv *env = ms_get_jni_env();
	jobject jcodec = NULL;

	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return NULL;
	}

	jmethodID methodID = env->GetStaticMethodID(mediaCodecClass, "createDecoderByType", "(Ljava/lang/String;)Landroid/media/MediaCodec;");
	if (methodID != NULL){
		jstring msg = env->NewStringUTF(mime_type);
		jcodec = env->CallStaticObjectMethod(mediaCodecClass, methodID, msg);
		handle_java_exception();
		if (jcodec){
			jcodec=env->NewGlobalRef(jcodec);
			ms_message("Codec %s successfully created.", mime_type);
		}else{
			ms_error("Failed to create codec !");
			return NULL;
		}
		env->DeleteLocalRef(msg);
	}else{
		ms_error("createDecoderByType() not found in class mediaCodec !");
		env->ExceptionClear();
		return NULL;
	}
	env->DeleteLocalRef(mediaCodecClass);
	codec->jcodec = jcodec;
	return codec;
}

AMediaCodec* AMediaCodec_createEncoderByType(const char *mime_type){
	AMediaCodec *codec=ms_new0(AMediaCodec,1);
	JNIEnv *env = ms_get_jni_env();
	jobject jcodec = NULL;

	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return NULL;
	}

	jmethodID methodID = env->GetStaticMethodID(mediaCodecClass, "createEncoderByType", "(Ljava/lang/String;)Landroid/media/MediaCodec;");
	if (methodID != NULL){
		jstring msg = env->NewStringUTF(mime_type);
		jcodec = env->CallStaticObjectMethod(mediaCodecClass, methodID, msg);
		handle_java_exception();
		if (jcodec){
			jcodec=env->NewGlobalRef(jcodec);
			ms_message("Codec %s successfully created.", mime_type);
		}else{
			ms_error("Failed to create codec !");
			return NULL;
		}
		env->DeleteLocalRef(msg);
	}else{
		ms_error("createEncoderByType() not found in class MediaCodec !");
		env->ExceptionClear();
		return NULL;
	}
	env->DeleteLocalRef(mediaCodecClass);
	codec->jcodec = jcodec;
	return codec;
}

media_status_t AMediaCodec_configure(AMediaCodec *codec, const AMediaFormat* format, ANativeWindow* surface, AMediaCrypto *crypto, uint32_t flags){
	JNIEnv *env = ms_get_jni_env();
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return AMEDIA_ERROR_BASE;
	}

	jmethodID methodID = env->GetMethodID(mediaCodecClass, "configure", "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V");
	if (methodID != NULL){
		env->CallVoidMethod(codec->jcodec, methodID, format->jformat, NULL, NULL, flags);
		if(handle_java_exception() == -1){
			return AMEDIA_ERROR_BASE;
		}
	} else {
		ms_error("configure() not found in class MediaCodec !");
		env->ExceptionClear();
		env->DeleteLocalRef(mediaCodecClass);
		return AMEDIA_ERROR_BASE;
	}
	env->DeleteLocalRef(mediaCodecClass);
	return AMEDIA_OK;
}

media_status_t AMediaCodec_delete(AMediaCodec *codec){
	JNIEnv *env = ms_get_jni_env();
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return AMEDIA_ERROR_BASE;
	}

	jmethodID methodID = env->GetMethodID(mediaCodecClass, "release","()V");
	if (methodID != NULL){
		env->CallVoidMethod(codec->jcodec, methodID);
		handle_java_exception();
	} else {
		ms_error("release() not found in class MediaCodec !");
		env->ExceptionClear();
		env->DeleteLocalRef(mediaCodecClass);
		return AMEDIA_ERROR_BASE;
	}
	env->DeleteLocalRef(mediaCodecClass);
	env->DeleteGlobalRef(codec->jcodec);
	return AMEDIA_OK;
}

media_status_t AMediaCodec_start(AMediaCodec *codec){
	JNIEnv *env = ms_get_jni_env();
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return AMEDIA_ERROR_BASE;
	}

	jmethodID methodID = env->GetMethodID(mediaCodecClass, "start", "()V");
	if (methodID != NULL){
		env->CallVoidMethod(codec->jcodec, methodID);
		handle_java_exception();
	} else {
		ms_error("start() not found in class MediaCodec !");
		env->ExceptionClear();
		env->DeleteLocalRef(mediaCodecClass);
		return AMEDIA_ERROR_BASE;
	}
    env->DeleteLocalRef(mediaCodecClass);
	return AMEDIA_OK;
}

media_status_t AMediaCodec_flush(AMediaCodec *codec){
	JNIEnv *env = ms_get_jni_env();
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return AMEDIA_ERROR_BASE;
	}

	jmethodID methodID = env->GetMethodID(mediaCodecClass, "flush", "()V");
	if (methodID != NULL){
		env->CallVoidMethod(codec->jcodec, methodID);
		handle_java_exception();
	} else {
		ms_error("stop() not found in class MediaCodec !");
		env->ExceptionClear();
		env->DeleteLocalRef(mediaCodecClass);
		return AMEDIA_ERROR_BASE;
	}

	env->DeleteLocalRef(mediaCodecClass);
	return AMEDIA_OK;
}

media_status_t AMediaCodec_stop(AMediaCodec *codec){
	JNIEnv *env = ms_get_jni_env();
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return AMEDIA_ERROR_BASE;
	}

	jmethodID methodID = env->GetMethodID(mediaCodecClass, "stop", "()V");
	if (methodID != NULL){
		env->CallVoidMethod(codec->jcodec, methodID);
		handle_java_exception();
	} else {
		ms_error("stop() not found in class MediaCodec !");
		env->ExceptionClear();
		env->DeleteLocalRef(mediaCodecClass);
		return AMEDIA_ERROR_BASE;
	}

	env->DeleteLocalRef(mediaCodecClass);
	return AMEDIA_OK;
}


//API 21
/*uint8_t* AMediaCodec_getInputBuffer(AMediaCodec *codec, size_t idx, size_t *out_size){
	JNIEnv *env = ms_get_jni_env();
	jobject jbuffer;
	uint8_t *buf;
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	jmethodID jmethodID = env->GetMethodID(mediaCodecClass,"getInputBuffer","(I)Ljava/nio/ByteBuffer;");
	if (jmethodID != NULL){
		jbuffer = env->CallObjectMethod(codec->jcodec,jmethodID,(int)idx);
		if(jbuffer == NULL){
			return NULL;
		}
		buf = (uint8_t *) env->GetDirectBufferAddress(jbuffer);
		if (env->ExceptionCheck()) {
			env->ExceptionDescribe();
			env->ExceptionClear();
			ms_error("Exception");
		}
	} else {
		ms_error("getInputBuffer() not found in class mediacodec !");
		env->ExceptionClear(); //very important.
		return NULL;
	}
	env->DeleteLocalRef(mediaCodecClass);
	return buf;
}*/

//API 19
uint8_t* AMediaCodec_getInputBuffer(AMediaCodec *codec, size_t idx, size_t *out_size){
	JNIEnv *env = ms_get_jni_env();
	jobject object;
	uint8_t *buf = NULL;
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return NULL;
	}

	jmethodID methodID = env->GetMethodID(mediaCodecClass,"getInputBuffers","()[Ljava/nio/ByteBuffer;");
	if (methodID != NULL){
		object = env->CallObjectMethod(codec->jcodec, methodID);
		handle_java_exception();
		if(object != NULL){
			jobjectArray jbuffers = reinterpret_cast<jobjectArray>(object);
			jobject jbuf = env->GetObjectArrayElement(jbuffers,idx);
			jlong capacity = env->GetDirectBufferCapacity(jbuf);
			*out_size = (size_t) capacity;
			buf = (uint8_t *) env->GetDirectBufferAddress(jbuf);
			env->DeleteLocalRef(jbuf);
			env->DeleteLocalRef(object);
		}
	} else {
		ms_error("getInputBuffers() not found in class mediacodec !");
		env->ExceptionClear();
		env->DeleteLocalRef(mediaCodecClass);
		return NULL;
	}
	env->DeleteLocalRef(mediaCodecClass);
	return buf;
}

/*
uint8_t* AMediaCodec_getOutputBuffer(AMediaCodec *codec, size_t idx, size_t *out_size){
	JNIEnv *env = ms_get_jni_env();
	jobject jbuffer;
	uint8_t *buf;
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	jmethodID jmethodID = env->GetMethodID(mediaCodecClass,"getOutputBuffer","(I)Ljava/nio/ByteBuffer;");
	if (jmethodID != NULL){
		jbuffer = env->CallObjectMethod(codec->jcodec,jmethodID,(int)idx);
		if(jbuffer == NULL){
			return NULL;
		}
		buf = (uint8_t *) env->GetDirectBufferAddress(jbuffer);
		if (env->ExceptionCheck()) {
			env->ExceptionDescribe();
			env->ExceptionClear();
			ms_error("Exception");
		}
	} else {
		ms_error("getOutputBuffer() not found in class mediacodec !");
		env->ExceptionClear(); //very important.
		return NULL;
	}
	env->DeleteLocalRef(mediaCodecClass);
	return buf;
}*/

uint8_t* AMediaCodec_getOutputBuffer(AMediaCodec *codec, size_t idx, size_t *out_size){
	JNIEnv *env = ms_get_jni_env();
	jobject object;
	uint8_t *buf = NULL;
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return NULL;
	}

	jmethodID methodID = env->GetMethodID(mediaCodecClass,"getOutputBuffers","()[Ljava/nio/ByteBuffer;");
	if (methodID != NULL){
		object = env->CallObjectMethod(codec->jcodec, methodID);
		handle_java_exception();
		if(object != NULL){
			jobjectArray jbuffers = reinterpret_cast<jobjectArray>(object);
			jobject jbuf = env->GetObjectArrayElement(jbuffers,idx);
			buf = (uint8_t *) env->GetDirectBufferAddress(jbuf);
			env->DeleteLocalRef(jbuf);
			env->DeleteLocalRef(object);
		}
	} else {
		ms_error("getOutputBuffers() not found in class mediacodec !");
		env->ExceptionClear();
		env->DeleteLocalRef(mediaCodecClass);
		return NULL;
	}
	env->DeleteLocalRef(mediaCodecClass);
	return buf;
}

ssize_t AMediaCodec_dequeueInputBuffer(AMediaCodec *codec, int64_t timeoutUs){
	JNIEnv *env = ms_get_jni_env();
	jint jindex=-1;
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return -1;
	}

	jmethodID methodID = env->GetMethodID(mediaCodecClass,"dequeueInputBuffer","(J)I");
	if (methodID != NULL){
		jindex = env->CallIntMethod(codec->jcodec, methodID, timeoutUs);
		handle_java_exception();
	} else {
		ms_error("stop() not found in class mediacodec !");
		env->ExceptionClear();
		env->DeleteLocalRef(mediaCodecClass);
		return -1;
	}
	env->DeleteLocalRef(mediaCodecClass);
	return (ssize_t) jindex;
}

media_status_t AMediaCodec_queueInputBuffer(AMediaCodec *codec, size_t idx, off_t offset, size_t size, uint64_t time, uint32_t flags){
   	JNIEnv *env = ms_get_jni_env();
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass == NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear();
		return AMEDIA_ERROR_BASE;
	}

	jmethodID methodID = env->GetMethodID(mediaCodecClass,"queueInputBuffer","(IIIJI)V");
	if (methodID != NULL){
		env->CallVoidMethod(codec->jcodec, methodID, idx, offset, size, time, flags);
		handle_java_exception();
	} else {
		ms_error("queueInputBuffer() not found in class mediacodec !");
		env->ExceptionClear();
		env->DeleteLocalRef(mediaCodecClass);
		return AMEDIA_ERROR_BASE;
	}
	env->DeleteLocalRef(mediaCodecClass);
	return AMEDIA_OK;
}

ssize_t AMediaCodec_dequeueOutputBuffer(AMediaCodec *codec, AMediaCodecBufferInfo *info, int64_t timeoutUs) {
	JNIEnv *env = ms_get_jni_env();
	jint jindex=-1;
	jobject jinfo = NULL;

	jfieldID size;
	jfieldID offset;
	jfieldID flags;
	jclass mediaBufferInfoClass = env->FindClass("android/media/MediaCodec$BufferInfo");
    jmethodID methodID = env->GetMethodID(mediaBufferInfoClass,"<init>","()V");
	if (methodID != NULL){
		jinfo = env->NewObject(mediaBufferInfoClass,methodID);
		size = env->GetFieldID(mediaBufferInfoClass, "size" , "I");
		flags = env->GetFieldID(mediaBufferInfoClass, "flags" , "I");
		offset = env->GetFieldID(mediaBufferInfoClass, "offset" , "I");
		handle_java_exception();
	} else {
		ms_error("init not found in class MediaCodec$BufferInfo !");
    	env->ExceptionClear();
    	return -1;
	}
	env->DeleteLocalRef(mediaBufferInfoClass);

	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	methodID = env->GetMethodID(mediaCodecClass,"dequeueOutputBuffer","(Landroid/media/MediaCodec$BufferInfo;J)I");
	if (methodID != NULL){
		jindex = env->CallIntMethod(codec->jcodec, methodID ,jinfo, timeoutUs);
		if (env->ExceptionCheck()) {
			env->ExceptionDescribe();
			env->ExceptionClear();
			ms_error("Exception");
		}
	} else {
		ms_error("dequeueOutputBuffer() not found in class format !");
		env->ExceptionClear(); //very important.
		return -1;
	}

	info->size = env->GetIntField(jinfo,size);
	info->offset = env->GetIntField(jinfo,offset);
	info->flags = env->GetIntField(jinfo,flags);
	env->DeleteLocalRef(mediaCodecClass);
	env->DeleteLocalRef(jinfo);
	return (ssize_t) jindex;
}

AMediaFormat* AMediaCodec_getOutputFormat(AMediaCodec *codec){
	AMediaFormat *format=ms_new0(AMediaFormat,1);
	JNIEnv *env = ms_get_jni_env();
	jobject jformat = NULL;
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass==NULL){
		ms_error("Couldn't find android/media/MediaCodec class !");
		env->ExceptionClear(); //very important.
		return NULL;
	}

	jmethodID methodID = env->GetMethodID(mediaCodecClass,"getOutputFormat","()Landroid/media/MediaFormat;");
	if (methodID!=NULL){
		jformat=env->CallObjectMethod(codec->jcodec,methodID);
		handle_java_exception();
		if (jformat == NULL){
			ms_error("Failed to create format !");
			return NULL;
		}
	}else{
		ms_error("getOutputFormat() not found in class format !");
		env->ExceptionClear(); //very important.
	}
	env->DeleteLocalRef(mediaCodecClass);
	format->jformat = jformat;
	return format;
}

media_status_t AMediaCodec_releaseOutputBuffer(AMediaCodec *codec, size_t idx, bool render){
	JNIEnv *env = ms_get_jni_env();
	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass==NULL){
    		ms_error("Couldn't find android/media/MediaCodec class !");
    		env->ExceptionClear(); //very important.
    		return AMEDIA_ERROR_BASE;
    }

	jmethodID stopID = env->GetMethodID(mediaCodecClass,"releaseOutputBuffer","(IZ)V");
	if (stopID != NULL){
		env->CallVoidMethod(codec->jcodec,stopID,(int)idx,FALSE);
		handle_java_exception();
	if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				ms_error("Exception");
			}
	} else {
		ms_error("releaseOutputBuffer() not found in class format !");
		env->ExceptionClear(); //very important.
		env->DeleteLocalRef(mediaCodecClass);
		return AMEDIA_ERROR_BASE;
	}
	env->DeleteLocalRef(mediaCodecClass);
	return AMEDIA_OK;
}

void AMediaCodec_setParams(AMediaCodec *codec, const char *params){
	JNIEnv *env = ms_get_jni_env();
	jobject jbundle = NULL;

	jclass BundleClass = env->FindClass("android/os/Bundle");
	jmethodID methodID = env->GetMethodID(BundleClass,"<init>","()V");
	jmethodID putIntId = env->GetMethodID(BundleClass,"putInt","(Ljava/lang/String;I)V");
	if (methodID != NULL){
        jstring msg = env->NewStringUTF("request-sync");
		jbundle = env->NewObject(BundleClass,methodID);
        env->CallVoidMethod(jbundle,putIntId,msg,0);
		handle_java_exception();
        env->DeleteLocalRef(msg);
	} else {
		ms_error("init not found in class MediaCodec$BufferInfo !");
		env->ExceptionClear();
	}
	env->DeleteLocalRef(BundleClass);


	jclass mediaCodecClass = env->FindClass("android/media/MediaCodec");
	if (mediaCodecClass==NULL){
			ms_error("Couldn't find android/media/MediaCodec class !");
			env->ExceptionClear(); //very important.
	}

	jmethodID setParamsID = env->GetMethodID(mediaCodecClass,"setParameters","(Landroid/os/Bundle;)V");
	if (setParamsID != NULL){
		env->CallVoidMethod(codec->jcodec,setParamsID,jbundle);
		env->DeleteLocalRef(jbundle);

	if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				ms_error("Exception");
			}
	} else {
		ms_error("setParameters() not found in class format !");
		env->ExceptionClear(); //very important.
	}
	env->DeleteLocalRef(mediaCodecClass);
}


////////////////////////////////////////////////////
//                                                //
//                 MEDIA FORMAT                   //
//                                                //
////////////////////////////////////////////////////

//STUB
AMediaFormat *AMediaFormat_new(){
	AMediaFormat *format=ms_new0(AMediaFormat,1);
	JNIEnv *env = ms_get_jni_env();
	jobject jformat = NULL;
	jclass mediaFormatClass = env->FindClass("android/media/MediaFormat");
	if (mediaFormatClass==NULL){
		ms_error("Couldn't find android/media/MediaFormat class !");
		env->ExceptionClear(); //very important.
		return NULL;
	}

	jmethodID createID = env->GetStaticMethodID(mediaFormatClass,"createVideoFormat","(Ljava/lang/String;II)Landroid/media/MediaFormat;");
	if (createID!=NULL){
		jstring msg = env->NewStringUTF("video/avc");
		jformat=env->CallStaticObjectMethod(mediaFormatClass,createID,msg,240,320);
		if (jformat){
			ms_message("format successfully created.");
		}else{
			ms_error("Failed to create format !");
		}
		env->DeleteLocalRef(msg);
	}else{
		ms_error("create() not found in class format !");
		env->ExceptionClear(); //very important.
	}

	format->jformat = jformat;
	env->DeleteLocalRef(mediaFormatClass);
	return format;
}


media_status_t AMediaFormat_delete(AMediaFormat* format){
	JNIEnv *env = ms_get_jni_env();
	env->DeleteLocalRef(format->jformat);
	return AMEDIA_OK;
}

bool AMediaFormat_getInt32(AMediaFormat *format, const char *name, int32_t *out){
	JNIEnv *env = ms_get_jni_env();
	jclass mediaFormatClass = env->FindClass("android/media/MediaFormat");
	if (mediaFormatClass==NULL){
		ms_error("Couldn't find android/media/MediaFormat class !");
		env->ExceptionClear(); //very important.
		return NULL;
	}
	jmethodID setStringID = env->GetMethodID(mediaFormatClass,"getInteger","(Ljava/lang/String;)I");
	if(format == NULL) {
		ms_error("Format nul");
		return AMEDIA_ERROR_BASE;
	}
	if (setStringID != NULL){
		jstring jkey = env->NewStringUTF(name);
		jint jout = env->CallIntMethod(format->jformat,setStringID,jkey);
		*out = jout;
		env->DeleteLocalRef(jkey);
	} else {
		ms_error("getInteger() not found in class format !");
		env->ExceptionClear(); //very important.
		env->DeleteLocalRef(mediaFormatClass);
		return AMEDIA_ERROR_BASE;
	}
	env->DeleteLocalRef(mediaFormatClass);
	return AMEDIA_OK;
}

void AMediaFormat_setInt32(AMediaFormat *format, const char* name, int32_t value){
	JNIEnv *env = ms_get_jni_env();
	jclass mediaFormatClass = env->FindClass("android/media/MediaFormat");
	jmethodID setStringID = env->GetMethodID(mediaFormatClass,"setInteger","(Ljava/lang/String;I)V");
	if (setStringID != NULL){
		jstring jkey = env->NewStringUTF(name);
		env->CallVoidMethod(format->jformat,setStringID,jkey,value);
		env->DeleteLocalRef(jkey);
	} else {
		ms_error("setstring() not found in class format !");
		env->ExceptionClear(); //very important.
	}
	env->DeleteLocalRef(mediaFormatClass);
}

void AMediaFormat_setString(AMediaFormat *format, const char* key, const char* name){
	JNIEnv *env = ms_get_jni_env();
	jclass mediaFormatClass = env->FindClass("android/media/MediaFormat");
	jmethodID setStringID = env->GetMethodID(mediaFormatClass,"setString","(Ljava/lang/String;Ljava/lang/String;)V");
	if (setStringID != NULL){
		jstring jkey = env->NewStringUTF(key);
		jstring jvalue = env->NewStringUTF(name);
		env->CallVoidMethod(format->jformat,setStringID,jkey,jvalue);
		env->DeleteLocalRef(jkey);
		env->DeleteLocalRef(jvalue);
	} else {
		ms_error("setstring() not found in class format !");
		env->ExceptionClear(); //very important.
	}
	env->DeleteLocalRef(mediaFormatClass);
}

