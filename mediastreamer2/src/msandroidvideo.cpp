/*
mediastreamer2 library - modular sound and video processing and streaming
This is the video capture filter for Android.
It uses one of the JNI wrappers to access Android video capture API.
See:
	org.linphone.mediastream.video.capture.AndroidVideoApi9JniWrapper
	org.linphone.mediastream.video.capture.AndroidVideoApi8JniWrapper
	org.linphone.mediastream.video.capture.AndroidVideoApi5JniWrapper

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

extern "C" {
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mswebcam.h"
#include "mediastreamer2/msjava.h"
#include "mediastreamer2/msticker.h"
}

#include <jni.h>
#include <math.h>

static int android_sdk_version = 5;

static const char* AndroidApi9WrapperPath = "org.linphone.mediastream.video.capture.AndroidVideoApi9JniWrapper";
static const char* AndroidApi8WrapperPath = "org.linphone.mediastream.video.capture.AndroidVideoApi8JniWrapper";
static const char* AndroidApi5WrapperPath = "org.linphone.mediastream.video.capture.AndroidVideoApi5JniWrapper";

#define UNDEFINED_ROTATION -1

/************************ Data structures              ************************/
// Struct holding Android's cameras properties
struct AndroidWebcamConfig {
	int id;
	int frontFacing;
	int orientation;
};

struct AndroidReaderContext {
	AndroidReaderContext(MSFilter *f, MSWebCam *cam):filter(f), webcam(cam),frame(0),fps(5){
		ms_message("Creating AndroidReaderContext for Android VIDEO capture filter");
		ms_mutex_init(&mutex,NULL);
		androidCamera = 0;
		previewWindow = 0;
		rotation = UNDEFINED_ROTATION;
	};

	~AndroidReaderContext(){
		if (frame != 0) {
			freeb(frame);
		}
		ms_mutex_destroy(&mutex);
	};

	MSFrameRateController fpsControl;
	MSAverageFPS averageFps;

	MSFilter *filter;
	MSWebCam *webcam;

	mblk_t *frame;
	float fps;
	MSVideoSize requestedSize, hwCapableSize;
	ms_mutex_t mutex;
	int rotation;

	jobject androidCamera;
	jobject previewWindow;
};

/************************ Private helper methods       ************************/
static jclass getHelperClass(JNIEnv *env);
static int compute_image_rotation_correction(AndroidReaderContext* d);
static void compute_cropping_offsets(MSVideoSize hwSize, MSVideoSize outputSize, int* yoff, int* cbcroff);
static AndroidReaderContext *getContext(MSFilter *f);


/************************ MS2 filter methods           ************************/
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
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	ms_mutex_lock(&d->mutex);

	d->requestedSize=*(MSVideoSize*)data;

	JNIEnv *env = ms_get_jni_env();

	jclass helperClass = getHelperClass(env);
	jmethodID method = env->GetStaticMethodID(helperClass,"selectNearestResolutionAvailable", "(III)[I");

	// find neareast hw-available resolution (using jni call);
	jobject resArray = env->CallStaticObjectMethod(helperClass, method, ((AndroidWebcamConfig*)d->webcam->data)->id, d->requestedSize.width, d->requestedSize.height);

	if (!resArray) {
		ms_error("Failed to retrieve camera '%d' supported resolutions\n", ((AndroidWebcamConfig*)d->webcam->data)->id);
		return -1;
	}

	// handle result :
	//   - 0 : width
   //   - 1 : height
	jint res[2];
   env->GetIntArrayRegion((jintArray)resArray, 0, 2, res);
	ms_message("Camera selected resolution is: %dx%d (requested: %dx%d)\n", res[0], res[1], d->requestedSize.width, d->requestedSize.height);
	d->hwCapableSize.width =  res[0];
	d->hwCapableSize.height = res[1];

	int rqSize = d->requestedSize.width * d->requestedSize.height;
	int hwSize = d->hwCapableSize.width * d->hwCapableSize.height;

	// if hw supplies a smaller resolution, modify requested size accordingly
	if (hwSize < rqSize) {
		ms_message("Camera cannot produce requested resolution %dx%d, will supply smaller one: %dx%d\n",
			d->requestedSize.width, d->requestedSize.height, res[0], res[1]);
		d->requestedSize = d->hwCapableSize;
	} else if (hwSize > rqSize) {
		ms_message("Camera cannot produce requested resolution %dx%d, will capture a bigger one (%dx%d) and crop it to match encoder requested resolution\n",
			d->requestedSize.width, d->requestedSize.height, res[0], res[1]);
	}

	// is phone held |_ to cam orientation ?
	if (d->rotation == UNDEFINED_ROTATION || compute_image_rotation_correction(d) % 180 != 0) {
		if (d->rotation == UNDEFINED_ROTATION)
			ms_warning("Capture filter do not know yet about device's orientation.\nCurrent assumption: device is held perpendicular to its webcam (ie: portrait mode for a phone)\n");
		bool camIsLandscape = d->hwCapableSize.width > d->hwCapableSize.height;
		bool reqIsLandscape = d->requestedSize.width > d->requestedSize.height;

		// if both are landscape or both portrait, swap
		if (camIsLandscape == reqIsLandscape) {
			int t = d->requestedSize.width;
			d->requestedSize.width = d->requestedSize.height;
			d->requestedSize.height = t;
			ms_message("Swapped resolution width and height to : %dx%d\n", d->requestedSize.width, d->requestedSize.height);
		}
	}
	ms_mutex_unlock(&d->mutex);
	return 0;
}

static int video_capture_get_vsize(MSFilter *f, void* data){
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	*(MSVideoSize*)data=d->requestedSize;
	return 0;
}

static int video_capture_get_pix_fmt(MSFilter *f, void *data){
	*(MSPixFmt*)data=MS_YUV420P;
	return 0;
}

// Java will give us a pointer to capture preview surface.
static int video_set_native_preview_window(MSFilter *f, void *arg) {
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	ms_mutex_lock(&d->mutex);

	jobject w = *((jobject*)arg);

	if (w == d->previewWindow) {
		ms_mutex_unlock(&d->mutex);
		return 0;
	}

	JNIEnv *env = ms_get_jni_env();

	jclass helperClass = getHelperClass(env);
	jmethodID method = env->GetStaticMethodID(helperClass,"setPreviewDisplaySurface", "(Ljava/lang/Object;Ljava/lang/Object;)V");

	if (d->androidCamera) {
		if (d->previewWindow == 0) {
			ms_message("Preview capture window set for the 1st time (rotation:%d)\n", d->rotation);
			// env->CallStaticVoidMethod(helperClass, method, d->androidCamera, w);
		} else {
			ms_message("Preview capture window changed (rotation:%d)\n", d->rotation);
			env->DeleteGlobalRef(d->androidCamera);
		}
			env->CallStaticVoidMethod(helperClass,
						env->GetStaticMethodID(helperClass,"stopRecording", "(Ljava/lang/Object;)V"),
						d->androidCamera);

			d->androidCamera = env->NewGlobalRef(
				env->CallStaticObjectMethod(helperClass,
						env->GetStaticMethodID(helperClass,"startRecording", "(IIIIIJ)Ljava/lang/Object;"),
						((AndroidWebcamConfig*)d->webcam->data)->id,
						d->hwCapableSize.width,
						d->hwCapableSize.height,
						(jint)d->fps,
						(d->rotation != UNDEFINED_ROTATION) ? d->rotation:0,
						(jlong)d));

			env->CallStaticVoidMethod(helperClass, method, d->androidCamera, w);

	} else {
		ms_message("Preview capture window set but camera not created yet; remembering it for later use\n");
	}
	d->previewWindow = w;

	ms_mutex_unlock(&d->mutex);
	return 0;
}

static int video_get_native_preview_window(MSFilter *f, void *arg) {
    AndroidReaderContext* d = (AndroidReaderContext*) f->data;
    arg = &d->previewWindow;
    return 0;
}

static int video_set_device_rotation(MSFilter* f, void* arg) {
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	d->rotation=*((int*)arg);
ms_message("%s : %d\n", __FUNCTION__, d->rotation);
	return 0;
}

void video_capture_preprocess(MSFilter *f){
	ms_message("Preprocessing of Android VIDEO capture filter");

	AndroidReaderContext *d = getContext(f);
	ms_mutex_lock(&d->mutex);

	ms_video_init_framerate_controller(&d->fpsControl, d->fps);
	ms_video_init_average_fps(&d->averageFps, d->fps);

	JNIEnv *env = ms_get_jni_env();

	jclass helperClass = getHelperClass(env);
	jmethodID method = env->GetStaticMethodID(helperClass,"startRecording", "(IIIIIJ)Ljava/lang/Object;");

	ms_message("Starting Android camera '%d' (rotation:%d)\n", ((AndroidWebcamConfig*)d->webcam->data)->id, d->rotation);
	jobject cam = env->CallStaticObjectMethod(helperClass, method,
			((AndroidWebcamConfig*)d->webcam->data)->id,
			d->hwCapableSize.width,
			d->hwCapableSize.height,
			(jint)d->fps,
			(d->rotation != UNDEFINED_ROTATION) ? d->rotation:0,
			(jlong)d);
	d->androidCamera = env->NewGlobalRef(cam);

	if (d->previewWindow) {
		method = env->GetStaticMethodID(helperClass,"setPreviewDisplaySurface", "(Ljava/lang/Object;Ljava/lang/Object;)V");
		env->CallStaticVoidMethod(helperClass, method, d->androidCamera, d->previewWindow);
	}
	ms_message("Preprocessing of Android VIDEO capture filter done");
	ms_mutex_unlock(&d->mutex);
}

static void video_capture_process(MSFilter *f){
	AndroidReaderContext* d = getContext(f);

	ms_mutex_lock(&d->mutex);

	// If frame not ready, return
	if (d->frame == 0) {
		ms_mutex_unlock(&d->mutex);
		return;
	}

	ms_video_update_average_fps(&d->averageFps, f->ticker->time);

	ms_queue_put(f->outputs[0],d->frame);
	d->frame = 0;
	ms_mutex_unlock(&d->mutex);
}

static void video_capture_postprocess(MSFilter *f){
	ms_message("Postprocessing of Android VIDEO capture filter");
	AndroidReaderContext* d = getContext(f);
	ms_mutex_lock(&d->mutex);
	JNIEnv *env = ms_get_jni_env();

	if (d->androidCamera) {
		jclass helperClass = getHelperClass(env);
		jmethodID method = env->GetStaticMethodID(helperClass,"stopRecording", "(Ljava/lang/Object;)V");

		env->CallStaticVoidMethod(helperClass, method, d->androidCamera);
		env->DeleteGlobalRef(d->androidCamera);
	}
	d->androidCamera = 0;
	d->previewWindow = 0;
	ms_mutex_unlock(&d->mutex);
}


static void video_capture_uninit(MSFilter *f) {
	ms_message("Uninit of Android VIDEO capture filter");
	AndroidReaderContext* d = getContext(f);
	ms_mutex_lock(&d->mutex);
	ms_mutex_unlock(&d->mutex);
	delete d;
}

static MSFilterMethod video_capture_methods[]={
		{	MS_FILTER_SET_FPS,	&video_capture_set_fps},
		{	MS_FILTER_GET_FPS,	&video_capture_get_fps},
		{	MS_FILTER_SET_VIDEO_SIZE, &video_capture_set_vsize},
		{	MS_FILTER_GET_VIDEO_SIZE, &video_capture_get_vsize},
		{	MS_FILTER_GET_PIX_FMT, &video_capture_get_pix_fmt},
		{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , &video_set_native_preview_window },//preview is managed by capture filter
		{	MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID , &video_get_native_preview_window },
		{  MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION, &video_set_device_rotation },
		{	0,0 }
};

MSFilterDesc ms_video_capture_desc={
		MS_ANDROID_VIDEO_READ_ID,
		"MSAndroidVideoCapture",
		N_("A filter that captures Android video."),
		MS_FILTER_OTHER,
		NULL,
		0,
		1,
		NULL,
		video_capture_preprocess,
		video_capture_process,
		video_capture_postprocess,
		video_capture_uninit,
		video_capture_methods
};

MS_FILTER_DESC_EXPORT(ms_video_capture_desc)

/* Webcam methods */
static void video_capture_detect(MSWebCamManager *obj);
static void video_capture_cam_init(MSWebCam *cam){
	ms_message("Android VIDEO capture filter cam init");
}

static MSFilter *video_capture_create_reader(MSWebCam *obj){
	ms_message("Instanciating Android VIDEO capture MS filter");

	MSFilter* lFilter = ms_filter_new_from_desc(&ms_video_capture_desc);
	lFilter->data = new AndroidReaderContext(lFilter, obj);

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
	ms_message("Detecting Android VIDEO cards");
	JNIEnv *env = ms_get_jni_env();

	// create 3 int arrays - assuming 2 webcams at most
	jintArray indexes = (jintArray)env->NewIntArray(2);
	jintArray frontFacing = (jintArray)env->NewIntArray(2);
	jintArray orientation = (jintArray)env->NewIntArray(2);

	jclass helperClass = getHelperClass(env);
	jmethodID method = env->GetStaticMethodID(helperClass,"detectCameras", "([I[I[I)I");

	int count = env->CallStaticIntMethod(helperClass, method, indexes, frontFacing, orientation);

	ms_message("%d cards detected", count);
	for(int i=0; i<count; i++) {
		MSWebCam *cam = ms_web_cam_new(&ms_android_video_capture_desc);
		AndroidWebcamConfig* c = new AndroidWebcamConfig();
		env->GetIntArrayRegion(indexes, i, 1, &c->id);
		env->GetIntArrayRegion(frontFacing, i, 1, &c->frontFacing);
		env->GetIntArrayRegion(orientation, i, 1, &c->orientation);
		cam->data = c;
		cam->name = ms_strdup("Android video name");
		char* idstring = (char*) malloc(15);
		snprintf(idstring, 15, "Android%d", c->id);
		cam->id = idstring;
		ms_web_cam_manager_add_cam(obj,cam);
		ms_message("camera created: id=%d frontFacing=%d orientation=%d [msid:%s]\n", c->id, c->frontFacing, c->orientation, idstring);
	}

	ms_message("Detection of Android VIDEO cards done");
}



/************************ JNI methods                  ************************/
#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_capture_AndroidVideoApi5JniWrapper_setAndroidSdkVersion
  (JNIEnv *env, jclass c, jint version) {
	android_sdk_version = version;
	ms_message("Android SDK version: %d\n", android_sdk_version);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_capture_AndroidVideoApi5JniWrapper_putImage(JNIEnv*  env,
		jclass  thiz,jlong nativePtr,jbyteArray frame) {
	AndroidReaderContext* d = (AndroidReaderContext*) nativePtr;
	if (!d->androidCamera)
		return;
	ms_mutex_lock(&d->mutex);

	if (!ms_video_capture_new_frame(&d->fpsControl,d->filter->ticker->time)) {
		ms_mutex_unlock(&d->mutex);
		return;
	}

	if (d->rotation == UNDEFINED_ROTATION) {
		ms_mutex_unlock(&d->mutex);
		ms_warning("Cannot produce image: we do not know yet about device's orientation. Dropping camera frame\n");
		return;
	}

	int image_rotation_correction = (d->rotation != UNDEFINED_ROTATION) ? compute_image_rotation_correction(d) : 0;

	jboolean isCopied;
	jbyte* jinternal_buff = env->GetByteArrayElements(frame, &isCopied);
	if (isCopied) {
		ms_warning("The video frame received from Java has been copied");
	}

	int y_cropping_offset=0, cbcr_cropping_offset=0;
	compute_cropping_offsets(d->hwCapableSize, d->requestedSize, &y_cropping_offset, &cbcr_cropping_offset);

	int width = d->hwCapableSize.width;
	int height = d->hwCapableSize.height;

	char* y_src = (char*)(jinternal_buff + y_cropping_offset);
	char* cbcr_src = (char*) (jinternal_buff + width * height + cbcr_cropping_offset);

	/* Warning note: image_rotation_correction == 90 does not imply portrait mode !
	   (incorrect function naming).
	   It only implies one thing: image needs to rotated by that amount to be correctly
	   displayed.
	*/
#if 1
 	mblk_t* yuv_block = copy_ycbcrbiplanar_to_true_yuv_with_rotation(y_src
														, cbcr_src
														, image_rotation_correction
														, d->requestedSize.width
														, d->requestedSize.height
														, d->hwCapableSize.width
														, d->hwCapableSize.width,
														false);
#else
MSPicture pict;
	mblk_t *yuv_block = ms_yuv_buf_alloc(&pict, d->requestedSize.width, d->requestedSize.height);
for(int i=0; i<pict.w * pict.h; i++)
	pict.planes[0][i]=128;
#endif
	if (yuv_block) {
		if (d->frame)
			freemsg(d->frame);
		d->frame = yuv_block;
	}
	ms_mutex_unlock(&d->mutex);

	// JNI_ABORT free the buffer without copying back the possible changes
	env->ReleaseByteArrayElements(frame, jinternal_buff, JNI_ABORT);
}

#ifdef __cplusplus
}
#endif

static int compute_image_rotation_correction(AndroidReaderContext* d) {
	AndroidWebcamConfig* conf = (AndroidWebcamConfig*)(AndroidWebcamConfig*)d->webcam->data;

	int result;
	if (conf->frontFacing) {
		ms_debug("%s: %d + %d\n", __FUNCTION__, ((AndroidWebcamConfig*)d->webcam->data)->orientation, d->rotation);
	 	result = ((AndroidWebcamConfig*)d->webcam->data)->orientation + d->rotation;
	} else {
		ms_debug("%s: %d - %d\n", __FUNCTION__, ((AndroidWebcamConfig*)d->webcam->data)->orientation, d->rotation);
	 	result = ((AndroidWebcamConfig*)d->webcam->data)->orientation - d->rotation;
	}
	while(result < 0)
		result += 360;
	return result % 360;
}

static void compute_cropping_offsets(MSVideoSize hwSize, MSVideoSize outputSize, int* yoff, int* cbcroff) {
	// if hw <= out => return
	if (hwSize.width * hwSize.height <= outputSize.width * outputSize.height) {
		*yoff = 0;
		*cbcroff = 0;
		return;
	}

	int halfDiffW = (hwSize.width - ((outputSize.width>outputSize.height)?outputSize.width:outputSize.height)) / 2;
	int halfDiffH = (hwSize.height - ((outputSize.width<outputSize.height)?outputSize.width:outputSize.height)) / 2;

	*yoff = hwSize.width * halfDiffH + halfDiffW;
	*cbcroff = hwSize.width * halfDiffH * 0.5 + halfDiffW;
}

static jclass getHelperClass(JNIEnv *env) {
	if (android_sdk_version >= 9)
		return (jclass) env->FindClass(AndroidApi9WrapperPath);
	else if (android_sdk_version >= 8)
		return (jclass) env->FindClass(AndroidApi8WrapperPath);
	else
		return (jclass) env->FindClass(AndroidApi5WrapperPath);
}

static AndroidReaderContext *getContext(MSFilter *f) {
	return (AndroidReaderContext*) f->data;
}
