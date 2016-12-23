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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
	// mediaBufferInfo Class
	jmethodID _init_mediaBufferInfoClass;
	// MediaCodec Class
	jmethodID configure;
	jmethodID reset;
	jmethodID start;
	jmethodID release;
	jmethodID flush;
	jmethodID stop;
	jmethodID getInputBuffer;
	jmethodID getOutputBuffer;
	jmethodID dequeueInputBuffer;
	jmethodID queueInputBuffer;
	jmethodID dequeueOutputBuffer;
	jmethodID getOutputFormat;
	jmethodID getOutputImageMethod;
	jmethodID getInputImageMethod;
	jmethodID releaseOutputBuffer;
	jmethodID setParameters;
	// image Class
	jmethodID getFormatMethod;
	jmethodID getWidthMethod;
	jmethodID getHeightMethod;
	jmethodID getTimestrampMethod;
	jmethodID getPlanesMethod;
	jmethodID getCropRectMethod;
	// plane Class
	jmethodID getPixelStrideMethod;
	jmethodID getRowStrideMethod;
	jmethodID getBufferMethod;
	// Bundle Class
	jmethodID _init_BundleClass;
	jmethodID putIntId;
	// rect Class
	jfieldID bottomField;
	jfieldID leftField;
	jfieldID rightField;
	jfieldID topField;
	// mediaBufferInfo Class
	jfieldID size;
	jfieldID flags;
	jfieldID offset;
};

struct AMediaFormat {
	jobject jformat;
	// mediaFormat Class
	jmethodID setInteger;
	jmethodID getInteger;
	jmethodID setString;
};

int handle_java_exception() {
	JNIEnv *env = ms_get_jni_env();
	if (env->ExceptionCheck()) {
		env->ExceptionDescribe();
		env->ExceptionClear();
		return -1;
	}
	return 0;
}

static bool _loadClass(JNIEnv *env, const char *className, jclass *_class) {
	*_class = env->FindClass(className);
	if(handle_java_exception() == -1 || *_class == NULL) {
		ms_error("Could not load Java class [%s]", className);
		return false;
	}
	return true;
}

static bool _getMethodID(JNIEnv *env, jclass _class, const char *name, const char *sig, jmethodID *method) {
	*method = env->GetMethodID(_class, name, sig);
	if(handle_java_exception() == -1 || *method == NULL) {
		ms_error("Could not get method %s[%s]", name, sig);
		return false;
	}
	return true;
}

static bool _getStaticMethodID(JNIEnv *env, jclass _class, const char *name, const char *sig, jmethodID *method) {
	*method = env->GetStaticMethodID(_class, name, sig);
	if(handle_java_exception() == -1 || *method == NULL) {
		ms_error("Could not get static method %s[%s]", name, sig);
		return false;
	}
	return true;
}

static bool _getFieldID(JNIEnv *env, jclass _class, const char *name, const char *sig, jfieldID *field) {
	*field = env->GetFieldID(_class, name, sig);
	if(handle_java_exception() == -1 || *field == NULL) {
		ms_error("Could not get field %s[%s]", name, sig);
		return false;
	}
	return true;
}

bool AMediaCodec_loadMethodID(const char *createName, AMediaCodec *codec, const char *mime_type) {
	JNIEnv *env = ms_get_jni_env();
	jobject jcodec = NULL;
	jclass mediaCodecClass = NULL, imageClass = NULL, planeClass = NULL, rectClass = NULL, mediaBufferInfoClass = NULL, BundleClass = NULL;
	jmethodID createMethod = NULL;
	jstring msg = NULL;
	bool success = true;

	success &= _loadClass(env, "android/media/MediaCodec", &mediaCodecClass);
	success &= _loadClass(env, "android/media/Image", &imageClass);
	success &= _loadClass(env, "android/media/Image$Plane", &planeClass);
	success &= _loadClass(env, "android/graphics/Rect", &rectClass);
	success &= _loadClass(env, "android/media/MediaCodec$BufferInfo", &mediaBufferInfoClass);
	success &= _loadClass(env, "android/os/Bundle", &BundleClass);
	if (!success) {
		ms_error("%s(): one class could not be found", __FUNCTION__);
		goto error;
	}

	success &= _getStaticMethodID(env, mediaCodecClass, createName, "(Ljava/lang/String;)Landroid/media/MediaCodec;", &createMethod);
	success &= _getMethodID(env, mediaCodecClass, "configure", "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V", &(codec->configure));
	success &= _getMethodID(env, mediaCodecClass, "reset", "()V", &(codec->reset));
	success &= _getMethodID(env, mediaCodecClass, "start", "()V", &(codec->start));
	success &= _getMethodID(env, mediaCodecClass, "release", "()V", &(codec->release));
	success &= _getMethodID(env, mediaCodecClass, "flush", "()V", &(codec->flush));
	success &= _getMethodID(env, mediaCodecClass, "stop", "()V", &(codec->stop));
	success &= _getMethodID(env, mediaCodecClass, "getInputBuffer", "(I)Ljava/nio/ByteBuffer;", &(codec->getInputBuffer));
	success &= _getMethodID(env, mediaCodecClass, "getOutputBuffer","(I)Ljava/nio/ByteBuffer;", &(codec->getOutputBuffer));
	success &= _getMethodID(env, mediaCodecClass, "dequeueInputBuffer", "(J)I", &(codec->dequeueInputBuffer));
	success &= _getMethodID(env, mediaCodecClass, "queueInputBuffer", "(IIIJI)V", &(codec->queueInputBuffer));
	success &= _getMethodID(env, mediaCodecClass, "dequeueOutputBuffer", "(Landroid/media/MediaCodec$BufferInfo;J)I", &(codec->dequeueOutputBuffer));
	success &= _getMethodID(env, mediaCodecClass, "getOutputFormat", "()Landroid/media/MediaFormat;", &(codec->getOutputFormat));
	success &= _getMethodID(env, mediaCodecClass, "getInputImage", "(I)Landroid/media/Image;", &(codec->getInputImageMethod));
	success &= _getMethodID(env, mediaCodecClass, "getOutputImage", "(I)Landroid/media/Image;", &(codec->getOutputImageMethod));
	success &= _getMethodID(env, mediaCodecClass, "releaseOutputBuffer", "(IZ)V", &(codec->releaseOutputBuffer));
	success &= _getMethodID(env, mediaCodecClass, "setParameters", "(Landroid/os/Bundle;)V", &(codec->setParameters));
	success &= _getMethodID(env, imageClass, "getFormat", "()I", &(codec->getFormatMethod));
	success &= _getMethodID(env, imageClass, "getWidth", "()I", &(codec->getWidthMethod));
	success &= _getMethodID(env, imageClass, "getHeight", "()I", &(codec->getHeightMethod));
	success &= _getMethodID(env, imageClass, "getTimestamp", "()J", &(codec->getTimestrampMethod));
	success &= _getMethodID(env, imageClass, "getPlanes", "()[Landroid/media/Image$Plane;", &(codec->getPlanesMethod));
	success &= _getMethodID(env, imageClass, "getCropRect", "()Landroid/graphics/Rect;", &(codec->getCropRectMethod));
	success &= _getMethodID(env, planeClass, "getPixelStride", "()I", &(codec->getPixelStrideMethod));
	success &= _getMethodID(env, planeClass, "getRowStride", "()I", &(codec->getRowStrideMethod));
	success &= _getMethodID(env, planeClass, "getBuffer", "()Ljava/nio/ByteBuffer;", &(codec->getBufferMethod));
	success &= _getMethodID(env, mediaBufferInfoClass, "<init>", "()V", &(codec->_init_mediaBufferInfoClass));
	success &= _getMethodID(env, BundleClass,"<init>","()V", &(codec->_init_BundleClass));
	success &= _getMethodID(env, BundleClass,"putInt","(Ljava/lang/String;I)V", &(codec->putIntId));
	success &= _getFieldID(env, rectClass, "bottom", "I", &(codec->bottomField));
	success &= _getFieldID(env, rectClass, "left", "I", &(codec->leftField));
	success &= _getFieldID(env, rectClass, "right", "I", &(codec->rightField));
	success &= _getFieldID(env, rectClass, "top", "I", &(codec->topField));
	success &= _getFieldID(env, mediaBufferInfoClass, "size" , "I", &(codec->size));
	success &= _getFieldID(env, mediaBufferInfoClass, "flags" , "I", &(codec->flags));
	success &= _getFieldID(env, mediaBufferInfoClass, "offset" , "I", &(codec->offset));
	if(!success) {
		ms_error("%s(): one method or field could not be found", __FUNCTION__);
		goto error;
	}

	msg = env->NewStringUTF(mime_type);
	jcodec = env->CallStaticObjectMethod(mediaCodecClass, createMethod, msg);
	handle_java_exception();
	if (!jcodec) {
		ms_error("Failed to create codec !");
		goto error;
	}

	codec->jcodec = env->NewGlobalRef(jcodec);
	ms_message("Codec %s successfully created.", mime_type);

	env->DeleteLocalRef(mediaCodecClass);
	env->DeleteLocalRef(jcodec);
	env->DeleteLocalRef(imageClass);
	env->DeleteLocalRef(planeClass);
	env->DeleteLocalRef(rectClass);
	env->DeleteLocalRef(BundleClass);
	env->DeleteLocalRef(msg);
	return true;

	error:
	if (mediaCodecClass) env->DeleteLocalRef(mediaCodecClass);
	if (jcodec) env->DeleteLocalRef(jcodec);
	if (imageClass) env->DeleteLocalRef(imageClass);
	if (planeClass) env->DeleteLocalRef(planeClass);
	if (rectClass) env->DeleteLocalRef(rectClass);
	if (BundleClass) env->DeleteLocalRef(BundleClass);
	if (msg) env->DeleteLocalRef(msg);
	return false;
}

AMediaCodec * AMediaCodec_createDecoderByType(const char *mime_type) {
	AMediaCodec *codec = ms_new0(AMediaCodec, 1);
	if (!AMediaCodec_loadMethodID("createDecoderByType", codec, mime_type)) {
		ms_free(codec);
		codec = NULL;
	}
	return codec;
}

AMediaCodec* AMediaCodec_createEncoderByType(const char *mime_type) {
	AMediaCodec *codec = ms_new0(AMediaCodec, 1);
	if (!AMediaCodec_loadMethodID("createEncoderByType", codec, mime_type)) {
		ms_free(codec);
		codec = NULL;
	}
	return codec;
}

media_status_t AMediaCodec_configure(AMediaCodec *codec, const AMediaFormat* format, ANativeWindow* surface, AMediaCrypto *crypto, uint32_t flags) {
	JNIEnv *env = ms_get_jni_env();

	env->CallVoidMethod(codec->jcodec, codec->configure, format->jformat, NULL, NULL, flags);

	return (handle_java_exception() == -1) ? AMEDIA_ERROR_BASE : AMEDIA_OK;
}

media_status_t AMediaCodec_delete(AMediaCodec *codec) {
	JNIEnv *env = ms_get_jni_env();

	env->CallVoidMethod(codec->jcodec, codec->release);
	env->DeleteGlobalRef(codec->jcodec);
	ms_free(codec);

	return (handle_java_exception() == -1) ? AMEDIA_ERROR_BASE : AMEDIA_OK;
}

media_status_t AMediaCodec_start(AMediaCodec *codec) {
	JNIEnv *env = ms_get_jni_env();

	env->CallVoidMethod(codec->jcodec, codec->start);

	return (handle_java_exception() == -1) ? AMEDIA_ERROR_BASE : AMEDIA_OK;
}

media_status_t AMediaCodec_flush(AMediaCodec *codec) {
	JNIEnv *env = ms_get_jni_env();

	env->CallVoidMethod(codec->jcodec, codec->flush);

	return (handle_java_exception() == -1) ? AMEDIA_ERROR_BASE : AMEDIA_OK;
}

media_status_t AMediaCodec_stop(AMediaCodec *codec) {
	JNIEnv *env = ms_get_jni_env();

	env->CallVoidMethod(codec->jcodec, codec->stop);

	return (handle_java_exception() == -1) ? AMEDIA_ERROR_BASE : AMEDIA_OK;
}


uint8_t* AMediaCodec_getInputBuffer(AMediaCodec *codec, size_t idx, size_t *out_size) {
	JNIEnv *env = ms_get_jni_env();
	jobject jbuf = NULL;
	uint8_t *buf = NULL;

	jbuf = env->CallObjectMethod(codec->jcodec, codec->getInputBuffer, (jint) idx);
	if(jbuf != NULL){
		jlong capacity = env->GetDirectBufferCapacity(jbuf);
		*out_size = (size_t) capacity;
		buf = (uint8_t *) env->GetDirectBufferAddress(jbuf);
		env->DeleteLocalRef(jbuf);
	} else {
			ms_error("getInputBuffer() failed !");
			env->ExceptionClear();
	}
	handle_java_exception();
	return buf;
}

uint8_t* AMediaCodec_getOutputBuffer(AMediaCodec *codec, size_t idx, size_t *out_size) {
	JNIEnv *env = ms_get_jni_env();
	jobject jbuf = NULL;
	uint8_t *buf = NULL;
	jlong capacity;

	jbuf = env->CallObjectMethod(codec->jcodec, codec->getOutputBuffer, (jint) idx);
	if (jbuf != NULL){
		buf = (uint8_t *) env->GetDirectBufferAddress(jbuf);
		capacity = env->GetDirectBufferCapacity(jbuf);
		*out_size = (size_t) capacity;
		env->DeleteLocalRef(jbuf);
	} else {
			ms_error("getOutputBuffer() failed !");
			env->ExceptionClear();
	}
	handle_java_exception();
	return buf;
}

ssize_t AMediaCodec_dequeueInputBuffer(AMediaCodec *codec, int64_t timeoutUs) {
	JNIEnv *env = ms_get_jni_env();
	jint jindex = -1;

	jindex = env->CallIntMethod(codec->jcodec, codec->dequeueInputBuffer, timeoutUs);

	/*return value to notify the exception*/
	/*otherwise, if -1 is returned as index, it just means that no buffer are available at this time (not an error)*/
	return (handle_java_exception() == -1) ? AMEDIA_ERROR_UNKNOWN : (ssize_t) jindex;
}

media_status_t AMediaCodec_queueInputBuffer(AMediaCodec *codec, size_t idx, off_t offset, size_t size, uint64_t time, uint32_t flags) {
	JNIEnv *env = ms_get_jni_env();

	env->CallVoidMethod(codec->jcodec, codec->queueInputBuffer, idx, offset, size, time, flags);

	return (handle_java_exception() == -1) ? AMEDIA_ERROR_BASE : AMEDIA_OK;
}

ssize_t AMediaCodec_dequeueOutputBuffer(AMediaCodec *codec, AMediaCodecBufferInfo *info, int64_t timeoutUs) {
	JNIEnv *env = ms_get_jni_env();
	jint jindex = -1;
	jobject jinfo = NULL;
	jclass mediaBufferInfoClass;

	/* We can't stock jclass information due to JNIEnv difference between different threads */
	if(!_loadClass(env, "android/media/MediaCodec$BufferInfo", &mediaBufferInfoClass)) {
		ms_error("%s(): one class could not be found", __FUNCTION__);
		env->ExceptionClear();
		return AMEDIA_ERROR_UNKNOWN;
	}

	jinfo = env->NewObject(mediaBufferInfoClass, codec->_init_mediaBufferInfoClass);

	jindex = env->CallIntMethod(codec->jcodec, codec->dequeueOutputBuffer , jinfo, timeoutUs);
	if (env->ExceptionCheck()) {
		env->ExceptionDescribe();
		env->ExceptionClear();
		ms_error("Exception");
		jindex = AMEDIA_ERROR_UNKNOWN; /*return value to notify the exception*/
		/*otherwise, if -1 is returned as index, it just means that no buffer are available at this time (not an error)*/
	}

	if (jindex >= 0) {
		info->size = env->GetIntField(jinfo, codec->size);
		info->offset = env->GetIntField(jinfo, codec->offset);
		info->flags = env->GetIntField(jinfo, codec->flags);
	}

	env->DeleteLocalRef(mediaBufferInfoClass);
	env->DeleteLocalRef(jinfo);
	return (ssize_t) jindex;
}

AMediaFormat* AMediaCodec_getOutputFormat(AMediaCodec *codec) {
	AMediaFormat *format = AMediaFormat_new();
	JNIEnv *env = ms_get_jni_env();
	jobject jformat = NULL;

	jformat = env->CallObjectMethod(codec->jcodec, codec->getOutputFormat);
	handle_java_exception();
	if (!jformat) {
		ms_error("Failed to create format !");
		return NULL;
	}

	format->jformat = env->NewGlobalRef(jformat);
	env->DeleteLocalRef(jformat);
	return format;
}

media_status_t AMediaCodec_releaseOutputBuffer(AMediaCodec *codec, size_t idx, bool render) {
	JNIEnv *env = ms_get_jni_env();
	env->CallVoidMethod(codec->jcodec, codec->releaseOutputBuffer, (int)idx, FALSE);
	return (handle_java_exception() == -1) ? AMEDIA_ERROR_BASE : AMEDIA_OK;
}

void AMediaCodec_reset(AMediaCodec *codec) {
	JNIEnv *env = ms_get_jni_env();
	env->CallVoidMethod(codec->jcodec, codec->reset);
	handle_java_exception();
}

void AMediaCodec_setParams(AMediaCodec *codec, const char *params) {
	JNIEnv *env = ms_get_jni_env();
	jobject jbundle = NULL;
	jclass BundleClass = NULL;

	/* We can't stock jclass information due to JNIEnv difference between different threads */
	if (!_loadClass(env, "android/os/Bundle", &BundleClass)) {
		ms_error("%s(): one class could not be found", __FUNCTION__);
		handle_java_exception();
		return;
	}

	jstring msg = env->NewStringUTF(params);
	jbundle = env->NewObject(BundleClass, codec->_init_BundleClass);
	env->CallVoidMethod(jbundle, codec->putIntId, msg, 0);
	handle_java_exception();
	env->DeleteLocalRef(msg);

	env->CallVoidMethod(codec->jcodec, codec->setParameters, jbundle);
	handle_java_exception();
	env->DeleteLocalRef(jbundle);
	env->DeleteLocalRef(BundleClass);
}

static bool _getImage(JNIEnv *env, AMediaCodec *codec, const bool isInput, int index, AMediaImage *image) {
	jobject jimage = NULL, jrect = NULL;
	jobjectArray jplanes = NULL;
	int bottom, left, right, top;
	bool success = TRUE;

	jimage = (isInput) ?
		env->CallObjectMethod(codec->jcodec, codec->getInputImageMethod, index):
		env->CallObjectMethod(codec->jcodec, codec->getOutputImageMethod, index);

	if(handle_java_exception() == -1 || jimage == NULL) {
		ms_error("%s(): could not get the %s image with index [%d]", __FUNCTION__, (isInput) ? "input" : "output", index);
		success = FALSE;
		goto end;
	}

	image->format = env->CallIntMethod(jimage, codec->getFormatMethod);
	image->width = env->CallIntMethod(jimage, codec->getWidthMethod);
	image->height = env->CallIntMethod(jimage, codec->getHeightMethod);
	image->timestamp = env->CallLongMethod(jimage, codec->getTimestrampMethod);

	jrect = env->CallObjectMethod(jimage, codec->getCropRectMethod);
	if(jrect == NULL) {
		ms_error("%s: could not get crop rectangle", __FUNCTION__);
		success = FALSE;
		goto end;
	}
	bottom = env->GetIntField(jrect, codec->bottomField);
	left = env->GetIntField(jrect, codec->leftField);
	right = env->GetIntField(jrect, codec->rightField);
	top = env->GetIntField(jrect, codec->topField);
	image->crop_rect.x = left;
	image->crop_rect.y = top;
	image->crop_rect.w = right - left;
	image->crop_rect.h = bottom - top;

	jplanes = reinterpret_cast<jobjectArray>(env->CallObjectMethod(jimage, codec->getPlanesMethod));
	image->nplanes = env->GetArrayLength(jplanes);
	for(int i=0; i<image->nplanes; i++) {
		jobject jplane = env->GetObjectArrayElement(jplanes, i);
		image->pixel_strides[i] = env->CallIntMethod(jplane, codec->getPixelStrideMethod);
		if(env->ExceptionCheck()) {
			image->pixel_strides[i] = -1;
			env->ExceptionClear();
		}
		image->row_strides[i] = env->CallIntMethod(jplane, codec->getRowStrideMethod);
		if(env->ExceptionCheck()) {
			image->row_strides[i] = -1;
			env->ExceptionClear();
		}
		jobject jbuffer = env->CallObjectMethod(jplane, codec->getBufferMethod);
		image->buffers[i] = (uint8_t *)env->GetDirectBufferAddress(jbuffer);
		env->DeleteLocalRef(jbuffer);
		env->DeleteLocalRef(jplane);
	}

	image->priv_ptr = env->NewGlobalRef(jimage);

end:
	if (jimage) env->DeleteLocalRef(jimage);
	if (jplanes) env->DeleteLocalRef(jplanes);
	if (jrect) env->DeleteLocalRef(jrect);
	return success;
}

bool AMediaCodec_getInputImage(AMediaCodec * codec, int index, AMediaImage *image) {
	JNIEnv *env = ms_get_jni_env();
	return _getImage(env, codec, TRUE, index, image);
}

bool AMediaCodec_getOutputImage(AMediaCodec *codec, int index, AMediaImage *image) {
	JNIEnv *env = ms_get_jni_env();
	return _getImage(env, codec, FALSE, index, image);
}

void AMediaImage_close(AMediaImage *image) {
	jclass imageClass = NULL;
	jmethodID close;
	bool_t success = TRUE;

	JNIEnv *env = ms_get_jni_env();
	jobject jimage = (jobject)image->priv_ptr;

	success = success && _loadClass(env, "android/media/Image", &imageClass);
	success = success && _getMethodID(env, imageClass, "close", "()V", &close);
	if (!success) {
		ms_error("%s: could not load some class or method ID", __FUNCTION__);
	}
	if (imageClass) {
		env->CallVoidMethod(jimage, close);
		env->DeleteLocalRef(imageClass);
	}
	env->DeleteGlobalRef(jimage);
	image->priv_ptr = NULL;
}

bool_t AMediaImage_isAvailable(void) {
	return ms_get_android_sdk_version() >= 21;
}


////////////////////////////////////////////////////
//                                                //
//                 MEDIA FORMAT                   //
//                                                //
////////////////////////////////////////////////////

bool AMediaFormat_loadMethodID(AMediaFormat * format) {
	JNIEnv *env = ms_get_jni_env();
	jclass mediaFormatClass = NULL;
	jobject jformat = NULL;
	jmethodID createID = NULL;
	jstring msg = NULL;
	bool success = true;

	success &= _loadClass(env, "android/media/MediaFormat", &mediaFormatClass);
	if(!success) {
		ms_error("%s(): one class could not be found", __FUNCTION__);
		goto error;
	}

	success &= _getStaticMethodID(env, mediaFormatClass, "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;", &createID);
	success &= _getMethodID(env, mediaFormatClass, "setInteger", "(Ljava/lang/String;I)V", &(format->setInteger));
	success &= _getMethodID(env, mediaFormatClass, "getInteger", "(Ljava/lang/String;)I", &(format->getInteger));
	success &= _getMethodID(env, mediaFormatClass, "setString", "(Ljava/lang/String;Ljava/lang/String;)V", &(format->setString));
	if(!success) {
		ms_error("%s(): one method or field could not be found", __FUNCTION__);
		goto error;
	}

	msg = env->NewStringUTF("video/avc");
	jformat = env->CallStaticObjectMethod(mediaFormatClass, createID, msg, 240, 320);
	if (!jformat) {
		ms_error("Failed to create format !");
		goto error;
	}

	format->jformat = env->NewGlobalRef(jformat);
	env->DeleteLocalRef(jformat);
	env->DeleteLocalRef(mediaFormatClass);
	env->DeleteLocalRef(msg);
	return true;

	error:
	if (mediaFormatClass) env->DeleteLocalRef(mediaFormatClass);
	if (jformat) env->DeleteLocalRef(jformat);
	if (msg) env->DeleteLocalRef(msg);
	return false;
}

//STUB
AMediaFormat *AMediaFormat_new(void) {
	AMediaFormat *format = ms_new0(AMediaFormat, 1);

	if (!AMediaFormat_loadMethodID(format)) {
		ms_free(format);
		format = NULL;
	}
	return format;
}

media_status_t AMediaFormat_delete(AMediaFormat* format) {
	JNIEnv *env = ms_get_jni_env();

	env->DeleteGlobalRef(format->jformat);
	ms_free(format);

	return AMEDIA_OK;
}

bool AMediaFormat_getInt32(AMediaFormat *format, const char *name, int32_t *out){
	JNIEnv *env = ms_get_jni_env();

	if (!format) {
		ms_error("Format nul");
		return false;
	}

	jstring jkey = env->NewStringUTF(name);
	jint jout = env->CallIntMethod(format->jformat, format->getInteger, jkey);
	*out = jout;
	env->DeleteLocalRef(jkey);
	handle_java_exception();

	return true;
}

void AMediaFormat_setInt32(AMediaFormat *format, const char* name, int32_t value) {
	JNIEnv *env = ms_get_jni_env();
	jstring jkey = env->NewStringUTF(name);
	env->CallVoidMethod(format->jformat, format->setInteger, jkey, value);
	env->DeleteLocalRef(jkey);
	handle_java_exception();
}

void AMediaFormat_setString(AMediaFormat *format, const char* key, const char* name) {
	JNIEnv *env = ms_get_jni_env();

	jstring jkey = env->NewStringUTF(key);
	jstring jvalue = env->NewStringUTF(name);
	env->CallVoidMethod(format->jformat, format->setString, jkey, jvalue);
	env->DeleteLocalRef(jkey);
	env->DeleteLocalRef(jvalue);
	handle_java_exception();
}
