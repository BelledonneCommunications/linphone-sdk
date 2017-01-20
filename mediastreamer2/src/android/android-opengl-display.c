/*
mediastreamer2 android video display filter
Copyright (C) 2010 Belledonne Communications SARL (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a/Users/pepp/workspace/mediastreamer2/src/msosxdisplay.m copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msjava.h"
#include "layouts.h"
#include "opengles_display.h"

#include <android/bitmap.h>

#include <dlfcn.h>


typedef struct AndroidDisplay{
	jobject android_video_window;
	MSVideoSize vsize;
	struct opengles_display* ogl;
	jmethodID set_opengles_display_id;
	jmethodID request_render_id;
}AndroidDisplay;


static void android_display_init(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)ms_new0(AndroidDisplay,1);
	JNIEnv *jenv=NULL;
	jclass wc;

	jenv=ms_get_jni_env();
	wc=(*jenv)->FindClass(jenv,"org/linphone/mediastream/video/AndroidVideoWindowImpl");
	if (wc==0){
		ms_fatal("Could not find org/linphone/mediastream/video/AndroidVideoWindowImpl class !");
	}
	ad->set_opengles_display_id=(*jenv)->GetMethodID(jenv,wc,"setOpenGLESDisplay","(J)V");
	ad->request_render_id=(*jenv)->GetMethodID(jenv,wc,"requestRender","()V");
	if (ad->set_opengles_display_id == 0)
		ms_error("Could not find 'setOpenGLESDisplay' method\n");
	if (ad->request_render_id == 0)
		ms_error("Could not find 'requestRender' method\n");
	ad->ogl = ogl_display_new();

	f->data=ad;
	ms_message("%s %p %p", __FUNCTION__, f, ad);
	(*jenv)->DeleteLocalRef(jenv,wc);
}

static void android_display_uninit(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	JNIEnv *jenv=ms_get_jni_env();
	ms_message("%s %p %p", __FUNCTION__, f, ad->ogl);
	
	if (ad->ogl) {
		/* clear native ptr, to prevent rendering to occur now that ptr is invalid */
		if (ad->android_video_window)
			(*jenv)->CallVoidMethod(jenv,ad->android_video_window,ad->set_opengles_display_id, 0);
		ogl_display_uninit(ad->ogl,FALSE);
		ms_free(ad->ogl);
	}
	if (ad->android_video_window) (*jenv)->DeleteGlobalRef(jenv, ad->android_video_window);

	ms_free(ad);
}

static void android_display_preprocess(MSFilter *f){
}

static void android_display_process(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	MSPicture pic;
	mblk_t *m;

	ms_filter_lock(f);
	if (ad->android_video_window){
		if ((m=ms_queue_peek_last(f->inputs[0]))!=NULL){
			if (ms_yuv_buf_init_from_mblk (&pic,m)==0){
				JNIEnv *jenv;

				/* schedule display of frame */
				if (ad->ogl) {
					/* m is dupb'ed inside ogl_display */
					ogl_display_set_yuv_to_display(ad->ogl, m);
				} else {
					ms_warning("%s: opengldisplay not ready (%p)", __FUNCTION__, ad->ogl);
				}
				
				jenv=ms_get_jni_env();
				(*jenv)->CallVoidMethod(jenv,ad->android_video_window,ad->request_render_id);
			}
		}
	}
	ms_filter_unlock(f);

	ms_queue_flush(f->inputs[0]);
	if (f->inputs[1] != NULL)
		ms_queue_flush(f->inputs[1]);
}

static int android_display_set_window(MSFilter *f, void *arg){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	unsigned long id=*(unsigned long*)arg;
	JNIEnv *jenv=ms_get_jni_env();
	jobject window=(jobject)id;
	jobject old_window;

	if (window == ad->android_video_window) return 0;
	
	ms_filter_lock(f);
	
	old_window=ad->android_video_window;
	
	if (ad->android_video_window) {
		ms_message("Clearing old opengles_display (%p)", ad->ogl);
		/* clear native ptr, to prevent rendering to occur now that ptr is invalid */
		(*jenv)->CallVoidMethod(jenv,ad->android_video_window,ad->set_opengles_display_id, 0);
		/* when context is lost GL resources are freed by Android */
		ogl_display_uninit(ad->ogl, FALSE);
		ms_free(ad->ogl);
		ad->ogl = ogl_display_new();
		
	}
	
	if (window) {
		ad->android_video_window=(*jenv)->NewGlobalRef(jenv, window);
		ms_message("Sending opengles_display pointer (%p)", ad->ogl);
		(*jenv)->CallVoidMethod(jenv,window,ad->set_opengles_display_id, (jlong)ad->ogl);
	}else ad->android_video_window=NULL;
	
	if (old_window)
		(*jenv)->DeleteGlobalRef(jenv, old_window);

	ms_filter_unlock(f);

	return 0;
}

static int android_display_set_zoom(MSFilter* f, void* arg) {
	AndroidDisplay* thiz=(AndroidDisplay*)f->data;
	ogl_display_zoom(thiz->ogl, arg);
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , android_display_set_window },
    	{  	MS_VIDEO_DISPLAY_ZOOM, android_display_set_zoom},
	{	0, NULL}
};

MSFilterDesc ms_android_opengl_display_desc={
	.id=MS_ANDROID_DISPLAY_ID,
	.name="MSAndroidDisplay",
	.text="OpenGL-ES2 video display filter for Android.",
	.category=MS_FILTER_OTHER,
	.ninputs=2, /*number of inputs*/
	.noutputs=0, /*number of outputs*/
	.init=android_display_init,
	.preprocess=android_display_preprocess,
	.process=android_display_process,
	.uninit=android_display_uninit,
	.methods=methods
};

void libmsandroidopengldisplay_init(MSFactory *factory){
	ms_factory_register_filter(factory,&ms_android_opengl_display_desc);
	ms_message("MSAndroidDisplay (OpenGL ES2) registered (id=%d).", MS_ANDROID_DISPLAY_ID);
}
