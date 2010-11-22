/*
mediastreamer2 library - modular sound and video processing and streaming

 * Copyright (C) 2010  Belledonne Communications, Grenoble, France

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


#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mswebcam.h"

#include <jni.h>

JavaVM *ms_andvid_jvm =0;



struct AndroidReaderContext {
	AndroidReaderContext():jvm(ms_andvid_jvm),frame(0),fps(5){
		ms_message("Creating AndroidReaderContext for Android VIDEO capture filter");

		ms_mutex_init(&mutex,NULL);

		JNIEnv *env = 0;
		jint result = jvm->AttachCurrentThread(&env,NULL);
		if (result != 0) {
			ms_fatal("cannot attach VM\n");
			return;
		}

		// Get SDK version
		jclass stockRecordClass = env->FindClass("org/linphone/core/AndroidCameraRecord");
		jfieldID sdk_field_id = env->GetStaticFieldID(stockRecordClass, "ANDROID_VERSION", "I");
		sdk_ver = (int) env->GetStaticObjectField(stockRecordClass, sdk_field_id);
		ms_message("SDK version is %i", sdk_ver);

		// Instanciate AndroidCameraRecord according to SDK version
		const char* videoClassName = "org/linphone/core/AndroidCameraRecordImpl";
		if (sdk_ver >= 8) videoClassName = "org/linphone/core/AndroidCameraRecordBufferedImpl";
		videoClassType = env->FindClass(videoClassName);
		if (videoClassType == 0) {
			ms_fatal("cannot find  %s\n", videoClassName);
			return;
		}

		videoClassType = (jclass) env->NewGlobalRef(videoClassType);
		if (videoClassType == 0) {
			ms_fatal("cannot register  %s\n", videoClassName);
			return;
		}


		jmethodID constructorId = env->GetMethodID(videoClassType,"<init>", "(J)V");
		if (constructorId == 0) {
			ms_fatal("cannot find  %s\n", constructorId);
			return;
		}


		javaAndroidCameraRecord = env->NewObject(videoClassType, constructorId, (jlong) this);
		if (javaAndroidCameraRecord == 0) {
			ms_fatal("cannot instantiate  %s\n", javaAndroidCameraRecord);
			return;
		}

		javaAndroidCameraRecord = env->NewGlobalRef(javaAndroidCameraRecord);
		if (javaAndroidCameraRecord == 0) {
			ms_fatal("cannot register  %s\n", javaAndroidCameraRecord);
			return;
		}
	};

	~AndroidReaderContext(){
		ms_mutex_destroy(&mutex);

		// FIXME release JNI references
	};

	/*	long expectedBuffSize() {
		// http://developer.android.com/reference/android/hardware/Camera.html#addCallbackBuffer%28byte[]%29
		return height * width * bits / 8;
	}*/

	JavaVM	*jvm;
	//jbyte* buff;
	mblk_t *frame;
	float fps;
	MSVideoSize vsize;
	jobject javaAndroidCameraRecord;
	jclass videoClassType;
	ms_mutex_t mutex;
	int sdk_ver;
};

static AndroidReaderContext *getContext(MSFilter *f) {
	return (AndroidReaderContext*) f->data;
}

static int attachVM(JNIEnv **env, AndroidReaderContext *d) {
	jint result = d->jvm->AttachCurrentThread(env,NULL);
	if (result != 0) {
		ms_error("cannot attach VM\n");
		return 1;
	}
	return 0;
}



static int video_capture_set_fps(MSFilter *f, void *arg){
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	d->fps=*((float*)arg);
	return 0;
}

static int video_capture_get_fps(MSFilter *f, void *arg){
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	*((float*)arg) = d->fps;
	return 0;
}

static int video_capture_set_vsize(MSFilter *f, void* data){
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;;
	d->vsize=*(MSVideoSize*)data;
	ms_message("Android video capture size set to h=%i w=%i in data structure", d->vsize.height, d->vsize.width);
	return 0;
}

static int video_capture_get_vsize(MSFilter *f, void* data){
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	*(MSVideoSize*)data=d->vsize;
	return 0;
}

static int video_capture_get_pix_fmt(MSFilter *f, void *data){
	*(MSPixFmt*)data=MS_YUV420P;
	return 0;
}

static MSFilterMethod video_capture_methods[]={
		{	MS_FILTER_SET_FPS,	&video_capture_set_fps},
		{	MS_FILTER_GET_FPS,	&video_capture_get_fps},
		{	MS_FILTER_SET_VIDEO_SIZE, &video_capture_set_vsize},
		{	MS_FILTER_GET_VIDEO_SIZE, &video_capture_get_vsize},
		{	MS_FILTER_GET_PIX_FMT, &video_capture_get_pix_fmt},
		{	0,0 }
};


static void video_capture_postprocess(MSFilter *f);
static void video_capture_process(MSFilter *f);
static void video_capture_preprocess(MSFilter *f);

MSFilterDesc ms_video_capture_desc={
		MS_ANDROID_VIDEO_READ_ID,
		"MSAndroidVideoCapture",
		N_("A filter that capture Android video."),
		MS_FILTER_OTHER,
		NULL,
		0,
		1,
		NULL,
		video_capture_preprocess,
		video_capture_process,
		video_capture_postprocess,
		NULL,
		video_capture_methods
};

MS_FILTER_DESC_EXPORT(ms_video_capture_desc)

static void video_capture_detect(MSWebCamManager *obj);

static void video_capture_cam_init(MSWebCam *cam){
	ms_message("Android VIDEO capture filter cam init");
	cam->name=ms_strdup("Android video");
}

static MSFilter *video_capture_create_reader(MSWebCam *obj){
	ms_message("Instanciating Android VIDEO capture MS filter");

	MSFilter* lFilter = ms_filter_new_from_desc(&ms_video_capture_desc);
	lFilter->data = new AndroidReaderContext();
	ms_message("Android VIDEO capture MS filter instanciated");
	return lFilter;


}

MSWebCamDesc ms_android_video_capture_desc={
		"AndroidVideoCapture",
		&video_capture_detect,
		&video_capture_cam_init,
		&video_capture_create_reader,
		NULL
};

static void video_capture_detect(MSWebCamManager *obj){
	// FIXME list available camera throw a JNI call
	// Currently only create one camera

	ms_message("Detecting Android VIDEO cards");
	MSWebCam *cam=ms_web_cam_new(&ms_android_video_capture_desc);
	ms_web_cam_manager_add_cam(obj,cam);
	ms_message("Detection of Android VIDEO cards done");
}




// Set Video parameters to java recorder
void video_capture_preprocess(MSFilter *f){
	ms_message("Preprocessing of Android VIDEO capture filter");

	AndroidReaderContext *d = getContext(f);
	JNIEnv *env = 0;
	if (attachVM(&env, d) != 0) return;

	jmethodID setParamMethod = env->GetMethodID(d->videoClassType,"setParameters", "(IIFZ)V");
	if (setParamMethod == 0) {
		ms_message("cannot find  %s\n", setParamMethod);
		return;
	}

	ms_message("Android video capture setting parameters h=%i, w=%i fps=%f through JNI", d->vsize.height, d->vsize.width, d->fps);
	ms_mutex_lock(&d->mutex);
	env->CallVoidMethod(d->javaAndroidCameraRecord, setParamMethod, d->vsize.height, d->vsize.width, d->fps, false);
	ms_mutex_unlock(&d->mutex);

	ms_message("Preprocessing of Android VIDEO capture filter done");
}



static void video_capture_process(MSFilter *f){
	AndroidReaderContext* d = getContext(f);

	// If frame not ready, return
	if (d->frame == 0) {
		return;
	}


	ms_mutex_lock(&d->mutex);

	ms_queue_put(f->outputs[0],d->frame);
	d->frame = 0;

	ms_mutex_unlock(&d->mutex);
}


static mblk_t *copy_frame_to_true_yuv(jbyte* initial_frame, int orientation, int w, int h) {

	//ms_message("Orientation %i; width %i; heigth %i", orientation, w, h);
	MSPicture pict;
	mblk_t *yuv_block = ms_yuv_buf_alloc(&pict, w, h);

	uint8_t* dstu = pict.planes[2];
	uint8_t* dstv = pict.planes[1];
	int ysize = w * h;
	int uorvsize = ysize / 4;
	uint8_t* src = (uint8_t*) initial_frame + ysize;

	// Copying Y
	uint8_t* dsty = pict.planes[0];
	uint8_t* srcy = (uint8_t*) initial_frame;
	int leftStrip = (w-h)/2;
	int rightStrip = (w+h) / 2;
	switch (orientation) {
	case 2: // -->
		for (int i=0; i < ysize; i++) {
			*(dsty+i) = *(srcy + ysize - i - 1);
		}
		break;
	case 1: // <--
		for (int i=0; i< ysize ; i++) {
			*(dsty+i) = 0;

		break;
	case 0: // ^^^
		for (int i=0; i < ysize; i++) {
			*(dsty+i) = *(srcy + i - 1);
		}
		break;
		//memcpy(pict.planes[0],srcy,ysize);
		break;
	default:
		break;
	}


	//	src+=orientation*oheigth/4; // shift to the real start
	uint8_t* enduv = src+uorvsize;
	for (int i = 0; i < uorvsize; i++) {
		*dstu = *src; // Copying U
		src++;
		dstu++;
		*dstv = *src; // Copying V
		src++;
		dstv++;
		//		if (src==enduv) {
		//			src-=uorvsize;
		//		}
	}

	return yuv_block;
}

extern "C" void Java_org_linphone_core_AndroidCameraRecordImpl_putImage(JNIEnv*  env,
		jobject  thiz,jlong nativePtr,jbyteArray jbadyuvframe, jint jorientation) {

	AndroidReaderContext* d = ((AndroidReaderContext*) nativePtr);

	jboolean isCopied;
	jbyte* jinternal_buff = env->GetByteArrayElements(jbadyuvframe, &isCopied);
	if (isCopied) {
		ms_warning("The video frame received from Java has been copied");
	}


	// Get a copy of the frame, encoded in a non interleaved YUV format
	mblk_t *yuv_frame=copy_frame_to_true_yuv(jinternal_buff, (int) jorientation, d->vsize.width, d->vsize.height);


	ms_mutex_lock(&d->mutex);
	if (d->frame != 0) {
		ms_message("Android video capture: putImage replacing old frame with new one");
		freemsg(d->frame);
		d->frame = 0;
	}

	d->frame = yuv_frame;
	ms_mutex_unlock(&d->mutex);

	// JNI_ABORT free the buffer without copying back the possible changes
	env->ReleaseByteArrayElements(jbadyuvframe, jinternal_buff, JNI_ABORT);

}




static void video_capture_postprocess(MSFilter *f){
	ms_message("Postprocessing of Android VIDEO capture filter");

	AndroidReaderContext* d = getContext(f);

	JNIEnv *env = 0;
	if (attachVM(&env, d) != 0) return;

	ms_message("Stoping video capture callback");
	jmethodID stopMethod = env->GetMethodID(d->videoClassType,"stopCaptureCallback", "()V");
	env->CallVoidMethod(d->javaAndroidCameraRecord, stopMethod);
	delete d;
	ms_message("Postprocessing of Android VIDEO capture filter done");
}

extern "C" void ms_andvid_set_jvm(JavaVM *jvm) {
	ms_andvid_jvm=jvm;
}
