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

#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mswebcam.h"
#include "mediastreamer2/msjava.h"


struct AndroidReaderContext {
	AndroidReaderContext():frame(0),fps(5){
		ms_message("Creating AndroidReaderContext for Android VIDEO capture filter");

		ms_mutex_init(&mutex,NULL);

		JNIEnv *env = ms_get_jni_env();
		managerClass = env->FindClass("org/linphone/core/AndroidCameraRecordManager");
		managerClass = (jclass) env->NewGlobalRef(managerClass);
		if (managerClass == 0) {
			ms_fatal("cannot register android video record manager class\n");
			return;
		}

		jmethodID getInstanceMethod = env->GetStaticMethodID(managerClass,"getInstance", "()Lorg/linphone/core/AndroidCameraRecordManager;");
		if (getInstanceMethod == 0) {
			ms_fatal("cannot find  singleton getter method\n");
			return;
		}

		// Get singleton AndroidCameraRecordManager for the default camera
		recorder = env->CallStaticObjectMethod(managerClass, getInstanceMethod);
		if (recorder == 0) {
			ms_fatal("cannot instantiate  %s\n", recorder);
			return;
		}

		recorder = env->NewGlobalRef(recorder);
		if (recorder == 0) {
			ms_fatal("cannot register  %s\n", recorder);
			return;
		}

	};

	~AndroidReaderContext(){
		ms_mutex_destroy(&mutex);
		JNIEnv *env = ms_get_jni_env();
		env->DeleteGlobalRef(recorder);
		env->DeleteGlobalRef(managerClass);

		if (frame != 0) {
			freeb(frame);
		}
	};

	mblk_t *frame;
	float fps;
	MSVideoSize vsize;
	jobject recorder;
	jclass managerClass;
	ms_mutex_t mutex;
};

static AndroidReaderContext *getContext(MSFilter *f) {
	return (AndroidReaderContext*) f->data;
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
static void video_capture_uninit(MSFilter *f);

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

static void video_capture_detect(MSWebCamManager *obj);

static void video_capture_cam_init(MSWebCam *cam){
	ms_message("Android VIDEO capture filter cam init");
	cam->name=ms_strdup("Android video");
}

static MSFilter *video_capture_create_reader(MSWebCam *obj){
	ms_message("Instanciating Android VIDEO capture MS filter");

	MSFilter* lFilter = ms_filter_new_from_desc(&ms_video_capture_desc);
	lFilter->data = new AndroidReaderContext();
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
	// Only creates one camera object whatever the real number of cameras.

	ms_message("Detecting Android VIDEO cards");
	MSWebCam *cam=ms_web_cam_new(&ms_android_video_capture_desc);
	ms_web_cam_manager_add_cam(obj,cam);
	ms_message("Detection of Android VIDEO cards done");
}




// Set Video parameters to java recorder
void video_capture_preprocess(MSFilter *f){
	ms_message("Preprocessing of Android VIDEO capture filter");

	AndroidReaderContext *d = getContext(f);
	JNIEnv *env = ms_get_jni_env();
	jmethodID setParamMethod = env->GetMethodID(d->managerClass,"setParametersFromFilter", "(JIIF)V");
	if (setParamMethod == 0) {
		ms_message("cannot find  %s\n", setParamMethod);
		return;
	}

	ms_message("Android video capture setting parameters h=%i, w=%i fps=%f through JNI", d->vsize.height, d->vsize.width, d->fps);
	ms_mutex_lock(&d->mutex);
	env->CallVoidMethod(d->recorder, setParamMethod, (jlong) d, d->vsize.height, d->vsize.width, d->fps);
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


// Can rotate Y, U or V plane; use step=2 for interleaved UV planes otherwise step=1
static void rotate_plane(int wDest, int hDest, uint8_t* src, uint8_t* dst, int step, bool clockWise) {
	int hSrc = wDest;
	int wSrc = hDest;
	int src_stride = wSrc * step;

	int signed_dst_stride;
	int incr;



	if (clockWise) {
		// ms_warning("start writing destination buffer from top right");
		dst += wDest - 1;
		incr = 1;
		signed_dst_stride = wDest;
	} else {
		// ms_warning("start writing destination buffer from top right");
		dst += wDest * (hDest - 1);
		incr = -1;
		signed_dst_stride = -wDest;
	}

	for (int y=0; y<hSrc; y++) {
		uint8_t* dst2 = dst;
		for (int x=0; x<step*wSrc; x+=step) {
			// Copy a line in source buffer (left to right)
			// Clockwise: Store a column in destination buffer (top to bottom)
			// Not clockwise: Store a column in destination buffer (bottom to top)
			*dst2 = src[x];
			dst2 += signed_dst_stride;
		}
		dst -= incr;
		src += src_stride;
	}
}

/*
static void rotate_plane_with_stripes(int w, int h, uint8_t* src, int src_stride, uint8_t* dst, int dst_stride, int step) {
	int alpha = (w-h) / 2; // the stripe

	dst += alpha + h;
	src += step * alpha;
	int xmax = h*step;

	for (int y=0; y<h; y++) {
		uint8_t* dst2 = dst;
		for (int x=0; x<xmax; x+=step) {
			*dst2 = src[x];
			dst2 += dst_stride;
		}
		dst--;
		src += src_stride;
	}
}
*/
static mblk_t *copy_frame_to_true_yuv(jbyte* initial_frame, int rotation, int w, int h) {

	//ms_message("Orientation %i; width %i; heigth %i", orientation, w, h);
	MSPicture pict;
	mblk_t *yuv_block = ms_yuv_buf_alloc(&pict, w, h);

	int ysize = w * h;


	// Copying Y
	uint8_t* srcy = (uint8_t*) initial_frame;
	uint8_t* dsty = pict.planes[0];
	switch (rotation) {
	case 180: // -->
		for (int i=0; i < ysize; i++) {
			*(dsty+i) = *(srcy + ysize - i - 1);
		}
		break;
/*	case 90: // <--
		memset(dsty, 16, ysize); // background for stripes
		rotate_plane_with_stripes(w,h,srcy,w,dsty,w, 1);
		break;
*/	case 0: // ^^^
		memcpy(pict.planes[0],srcy,ysize);
		break;
	default:
		ms_error("msandroidvideo.cpp: bad rotation %i", rotation);
		break;
	}

	uint8_t* dstu = pict.planes[2];
	uint8_t* dstv = pict.planes[1];
	int uorvsize = ysize / 4;
	uint8_t* srcuv = (uint8_t*) initial_frame + ysize;
	switch (rotation) {
/*		case 1:
		{
			memset(dstu, 128, uorvsize);
			memset(dstv, 128, uorvsize);

			int uvw = w/2;
			int uvh = h/2;
			rotate_plane_with_stripes(uvw,uvh,srcuv,w,dstu,uvw, 2);
			rotate_plane_with_stripes(uvw,uvh,srcuv +1,w,dstv,uvw, 2);
			break;
		}*/
		case 0:
			for (int i = 0; i < uorvsize; i++) {
				*(dstu++) = *(srcuv++); // Copying U
				*(dstv++) = *(srcuv++); // Copying V
			}
			break;
		case 180:
			srcuv += 2 * uorvsize;
			for (int i = 0; i < uorvsize; i++) {
				*(dstu++) = *(srcuv--); // Copying U
				*(dstv++) = *(srcuv--); // Copying V
			}
			break;
		default:
			ms_error("msandroidvideo.cpp: bad rotation %i", rotation);
			break;
	}


	return yuv_block;
}

/*
static void drawGradient(int w, int h, uint8_t* dst, bool vertical) {
	for (int y=0; y<h; y++) {
		for (int x=0; x < w; x++) {
			*(dst++) = vertical ? x : y;
		}
	}
}*/

// Destination and source images have their dimensions inverted.
static mblk_t *copy_frame_to_true_yuv_portrait(jbyte* initial_frame, int rotation, int w, int h) {

//	ms_message("copy_frame_to_true_yuv_inverted : Orientation %i; width %i; height %i", orientation, w, h);
	MSPicture pict;
	mblk_t *yuv_block = ms_yuv_buf_alloc(&pict, w, h);

	bool clockwise = rotation == 90 ? true : false;

	// Copying Y
	uint8_t* dsty = pict.planes[0];
	uint8_t* srcy = (uint8_t*) initial_frame;
	rotate_plane(w,h,srcy,dsty,1, clockwise);



	int uv_w = w/2;
	int uv_h = h/2;
//	int uorvsize = uv_w * uv_h;

	// Copying U
	uint8_t* srcu = (uint8_t*) initial_frame + (w * h);
	uint8_t* dstu = pict.planes[2];
	rotate_plane(uv_w,uv_h,srcu,dstu, 2, clockwise);
//	memset(dstu, 128, uorvsize);

	// Copying V
	uint8_t* srcv = srcu + 1;
	uint8_t* dstv = pict.planes[1];
	rotate_plane(uv_w,uv_h,srcv,dstv, 2, clockwise);
//	memset(dstv, 128, uorvsize);

	return yuv_block;
}

extern "C" void Java_org_linphone_core_AndroidCameraRecordImpl_putImage(JNIEnv*  env,
		jobject  thiz,jlong nativePtr,jbyteArray jbadyuvframe, jint jorientation) {

	AndroidReaderContext* d = ((AndroidReaderContext*) nativePtr);

	// received buffer is always in landscape orientation
	bool portrait = d->vsize.width < d->vsize.height;
	//ms_warning("PUT IMAGE: bo=%i, inv=%s, filter w=%i/h=%i", (int) jorientation,
	//		portrait? "portrait" : "landscape", d->vsize.width, d->vsize.height);

	jboolean isCopied;
	jbyte* jinternal_buff = env->GetByteArrayElements(jbadyuvframe, &isCopied);
	if (isCopied) {
		ms_warning("The video frame received from Java has been copied");
	}


	// Get a copy of the frame, encoded in a non interleaved YUV format
	mblk_t *yuv_frame;
	if (portrait) {
		yuv_frame=copy_frame_to_true_yuv_portrait(jinternal_buff, (int) jorientation, d->vsize.width, d->vsize.height);
	} else {
		yuv_frame=copy_frame_to_true_yuv(jinternal_buff, (int) jorientation, d->vsize.width, d->vsize.height);
	}


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
	JNIEnv *env = ms_get_jni_env();
	jmethodID stopMethod = env->GetMethodID(d->managerClass,"invalidateParameters", "()V");
	env->CallVoidMethod(d->recorder, stopMethod);
}


static void video_capture_uninit(MSFilter *f) {
	ms_message("Uninit of Android VIDEO capture filter");
	AndroidReaderContext* d = getContext(f);
	delete d;
}
