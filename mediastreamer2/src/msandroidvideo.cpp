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

JavaVM *ms_andvid_jvm;


MSFilterMethod video_capture_methods[]={
	{	MS_FILTER_SET_FPS,	/*video_capture_set_fps*/ NULL	},
	{	MS_FILTER_GET_FPS,	/*video_capture_get_fps*/ NULL	},
	{	MS_FILTER_SET_VIDEO_SIZE, /*video_capture_set_vsize*/  NULL},
	{	MS_FILTER_GET_VIDEO_SIZE, /*video_capture_get_vsize*/  NULL},
	{	MS_FILTER_GET_PIX_FMT, /*video_capture_get_pix_fmt*/  NULL},
	{	0,0 }
};


void video_capture_postprocess(MSFilter *f);
void video_capture_process(MSFilter *f);
void video_capture_preprocess(MSFilter *f);

MSFilterDesc ms_video_capture_desc={
	MS_ANDROIR_VIDEO_READ_ID,
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


struct AndroidReaderContext {
	AndroidReaderContext():jvm(ms_andvid_jvm),buff(0){
		ms_message("Creating AndroidReaderContext for Android VIDEO capture filter");

		ms_mutex_init(&mutex,NULL);

		JNIEnv *env = 0;
		jint result = jvm->AttachCurrentThread(&env,NULL);
		if (result != 0) {
			ms_error("cannot attach VM\n");
			return;
		}

		// Get SDK version Build.VERSION.SDK String
		// TODO
		sdk_ver = 0;

		// Instanciate AndroidCameraRecord according to SDK version
		// TODO
		const char* videoClassName = "org/linphone/core/AndroidCameraRecordImpl";
		if (sdk_ver >= 8) videoClassName = "org/linphone/core/AndroidCameraRecord8Impl";
		jclass videoClassType = env->FindClass(videoClassName);
		if (videoClassType == 0) {
			ms_error("cannot find  %s\n", videoClassName);
			return;
		}
		jmethodID constructorId = env->GetMethodID(videoClassType,"<init>", "(JI)V");
		javaAndroidCameraRecord = env->NewObject(videoClassType, constructorId, this, 10); // FIXME, force 10 frames / second
		javaAndroidCameraRecord = env->NewGlobalRef(javaAndroidCameraRecord);




		// Define addback if SDK >= 8
		if (sdk_ver >= 8) {
			javaAddBackVideoBuffer = env->GetMethodID(videoClassType, "addBackBuffer", "[B)V");
			//javaAddBackVideoBuffer = (jmethodID) env->NewGlobalRef(javaAddBackVideoBuffer);
		}
	};

	~AndroidReaderContext(){
		ms_mutex_destroy(&mutex);
	};

/*	long expectedBuffSize() {
		// http://developer.android.com/reference/android/hardware/Camera.html#addCallbackBuffer%28byte[]%29
		return height * width * bits / 8;
	}*/

	JavaVM			*jvm;
	//jbyte* buff;
	jbyteArray buff;
	jmethodID javaAddBackVideoBuffer;
	jobject javaAndroidCameraRecord;
	ms_mutex_t mutex;
	int sdk_ver;
};

void addBackBuffer(JNIEnv *env, AndroidReaderContext* d) {
	env->CallVoidMethod(d->javaAndroidCameraRecord, d->javaAddBackVideoBuffer, d->buff);
}

// ex frame 320x240 115'200 bytes
extern "C" void Java_org_linphone_core_AndroidCameraRecordImpl_putImage(JNIEnv*  env
		,jobject  thiz
		,jlong nativePtr
		,jbyteArray jbuff) {

	ms_message("Native putimage called for Android VIDEO capture filter");

	AndroidReaderContext* d = ((AndroidReaderContext*) nativePtr);

	if (d->buff != 0) {
		ms_error("AndroidReaderContext buffer full, frame lost");
		addBackBuffer(env, d);
		return;
	}

	// Get a pointer to the underlying array of bytes and the actual buffer size.
	ms_mutex_lock(&d->mutex);
	d->buff = (jbyteArray) env->NewGlobalRef(jbuff);
	ms_mutex_unlock(&d->mutex);


	ms_warning("Received array length [%i]");
	/*, expected [%i]",
			d->buffLength,
			d->expectedBuffSize());*/


	// TODO keep a reference on the jbuff
}



static MSFilter *video_capture_create_reader(MSWebCam *obj){
	ms_message("Instanciating Android VIDEO capture MS filter");

	MSFilter* lFilter = ms_filter_new_from_desc(&ms_video_capture_desc);

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
}

void video_capture_preprocess(MSFilter *f){
	ms_message("Preprocessing of Android VIDEO capture filter");
	f->data = new AndroidReaderContext();
}

void video_capture_process(MSFilter *f){
	ms_message("Processing Android VIDEO capture filter");

	AndroidReaderContext* d = (AndroidReaderContext*) f->data;

	// If frame not ready, return
	if (d->buff == 0) return;

	JNIEnv *env = 0;
	jint result = d->jvm->AttachCurrentThread(&env,NULL);
	if (result != 0) {
		ms_error("cannot attach VM\n");
		return;
	}


	ms_mutex_lock(&d->mutex);


	jboolean isCopied;
	jbyte* buff = env->GetByteArrayElements(d->buff, &isCopied);
	size_t length = env->GetArrayLength(d->buff);

	if (isCopied) {
		ms_warning("The video frame received from Java has been copied");
	}

	mblk_t *m;
	mblk_t *om=allocb(length, 0);
	ms_queue_put(f->outputs[0],om);

	// AddBack to
	addBackBuffer(env, d);
	env->DeleteGlobalRef(d->buff);
	d->buff = 0;

	ms_mutex_unlock(&d->mutex);


	end: {
		d->jvm->DetachCurrentThread();
	}
}




void video_capture_postprocess(MSFilter *f){
	ms_message("Postprocessing of Android VIDEO capture filter");

	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	delete d;
}

extern "C" void ms_andvid_set_jvm(JavaVM *jvm) {
	ms_andvid_jvm=jvm;
}
