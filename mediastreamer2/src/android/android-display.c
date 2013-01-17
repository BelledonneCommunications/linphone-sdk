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

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msjava.h"
#include "layouts.h"

#include <android/bitmap.h>

#include <dlfcn.h>


typedef struct AndroidDisplay{
	jobject android_video_window;
	jobject jbitmap;
	jmethodID get_bitmap_id;
	jmethodID update_id;
	jmethodID request_orientation_id;
	AndroidBitmapInfo bmpinfo;
	MSScalerContext *sws;
	MSVideoSize vsize;
	bool_t orientation_change_pending;
}AndroidDisplay;


static int (*sym_AndroidBitmap_getInfo)(JNIEnv *env,jobject bitmap, AndroidBitmapInfo *bmpinfo)=NULL;
static int (*sym_AndroidBitmap_lockPixels)(JNIEnv *env, jobject bitmap, void **pixels)=NULL;
static int (*sym_AndroidBitmap_unlockPixels)(JNIEnv *env, jobject bitmap)=NULL;

static void android_display_init(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)ms_new0(AndroidDisplay,1);
	JNIEnv *jenv=NULL;
	jclass wc;

	jenv=ms_get_jni_env();
	wc=(*jenv)->FindClass(jenv,"org/linphone/mediastream/video/AndroidVideoWindowImpl");
	if (wc==0){
		ms_fatal("Could not find org/linphone/mediastream/video/AndroidVideoWindowImpl class !");
	}
	ad->get_bitmap_id=(*jenv)->GetMethodID(jenv,wc,"getBitmap", "()Landroid/graphics/Bitmap;");
	ad->update_id=(*jenv)->GetMethodID(jenv,wc,"update","()V");
	ad->request_orientation_id=(*jenv)->GetMethodID(jenv,wc,"requestOrientation","(I)V");

	f->data=ad;
}

static void android_display_uninit(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	if (ad->sws){
		ms_scaler_context_free (ad->sws);
		ad->sws=NULL;
	}
	ms_free(ad);
}

static void android_display_preprocess(MSFilter *f){

}

#define LANDSCAPE 0
#define PORTRAIT 1

static int vsize_get_orientation(MSVideoSize vs){
	return vs.width>=vs.height ? LANDSCAPE : PORTRAIT;
}

static void select_orientation(AndroidDisplay *ad, MSVideoSize wsize, MSVideoSize vsize){
	int wo,vo;
	JNIEnv *jenv=ms_get_jni_env();
	wo=vsize_get_orientation(wsize);
	vo=vsize_get_orientation(vsize);
	if (wo!=vo){
		ms_message("Requesting orientation change !");
		(*jenv)->CallVoidMethod(jenv,ad->android_video_window,ad->request_orientation_id,vo);
		ad->orientation_change_pending=TRUE;
	}
}


static void android_display_process(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	MSPicture pic;
	mblk_t *m;

	ms_filter_lock(f);
	if (ad->jbitmap!=0 && !ad->orientation_change_pending){
		if ((m=ms_queue_peek_last(f->inputs[0]))!=NULL){
			if (ms_yuv_buf_init_from_mblk (&pic,m)==0){
				MSVideoSize wsize={ad->bmpinfo.width,ad->bmpinfo.height};
				MSVideoSize vsize={pic.w, pic.h};
				MSRect vrect;
				MSPicture dest={0};
				void *pixels=NULL;
				JNIEnv *jenv=ms_get_jni_env();

				if (!ms_video_size_equal(vsize,ad->vsize)){
					ms_message("Video to display has size %ix%i",vsize.width,vsize.height);
					ad->vsize=vsize;
					if (ad->sws){
						ms_scaler_context_free(ad->sws);
						ad->sws=NULL;
					}
					/*select_orientation(ad,wsize,vsize);*/
				}

				ms_layout_compute(wsize,vsize,vsize,-1,0,&vrect, NULL);

				if (ad->sws==NULL){
					ad->sws=ms_scaler_create_context (vsize.width,vsize.height,MS_YUV420P,
					                           vrect.w,vrect.h,MS_RGB565,MS_SCALER_METHOD_BILINEAR);
					if (ad->sws==NULL){
						ms_fatal("Could not obtain sws context !");
					}
				}

				if (sym_AndroidBitmap_lockPixels(jenv,ad->jbitmap,&pixels)==0){

					if (pixels!=NULL){
						dest.planes[0]=(uint8_t*)pixels+(vrect.y*ad->bmpinfo.stride)+(vrect.x*2);
						dest.strides[0]=ad->bmpinfo.stride;
						ms_scaler_process(ad->sws,pic.planes,pic.strides,dest.planes,dest.strides);
					}else ms_warning("Pixels==NULL in android bitmap !");

					sym_AndroidBitmap_unlockPixels(jenv,ad->jbitmap);
				}else{
					ms_error("AndroidBitmap_lockPixels() failed !");
				}

				(*jenv)->CallVoidMethod(jenv,ad->android_video_window,ad->update_id);

			}
		}
	}
	ms_filter_unlock(f);

	ms_queue_flush(f->inputs[0]);
	ms_queue_flush(f->inputs[1]);
}

static int android_display_set_window(MSFilter *f, void *arg){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	unsigned long id=*(unsigned long*)arg;
	int err;
	JNIEnv *jenv=ms_get_jni_env();
	jobject window=(jobject)id;

	ms_filter_lock(f);
	if (window!=NULL)
		ad->jbitmap=(*jenv)->CallObjectMethod(jenv,window,ad->get_bitmap_id);
	else
		ad->jbitmap=NULL;
	ad->android_video_window=window;
	if (ad->jbitmap!=NULL){
		err=sym_AndroidBitmap_getInfo(jenv,ad->jbitmap,&ad->bmpinfo);
		if (err!=0){
			ms_error("AndroidBitmap_getInfo() failed.");
			ad->jbitmap=0;
			ms_filter_unlock(f);
			return -1;
		}
	}
	if (ad->sws){
		ms_scaler_context_free(ad->sws);
		ad->sws=NULL;
	}
	ad->orientation_change_pending=FALSE;
	ms_filter_unlock(f);
	if (ad->jbitmap!=NULL) ms_message("New java bitmap given with w=%i,h=%i,stride=%i,format=%i",
	           ad->bmpinfo.width,ad->bmpinfo.height,ad->bmpinfo.stride,ad->bmpinfo.format);
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , android_display_set_window },
	{	0, NULL}
};

MSFilterDesc ms_android_display_desc={
	.id=MS_ANDROID_DISPLAY_ID,
	.name="MSAndroidDisplay",
	.text="Video display filter for Android.",
	.category=MS_FILTER_OTHER,
	.ninputs=2, /*number of inputs*/
	.noutputs=0, /*number of outputs*/
	.init=android_display_init,
	.preprocess=android_display_preprocess,
	.process=android_display_process,
	.uninit=android_display_uninit,
	.methods=methods
};


bool_t libmsandroiddisplay_init(void){
	/*See if we can use AndroidBitmap_* symbols (only since android 2.2 normally)*/
	void *handle=NULL;
	handle=dlopen("libjnigraphics.so",RTLD_LAZY);
	if (handle!=NULL){
		sym_AndroidBitmap_getInfo=dlsym(handle,"AndroidBitmap_getInfo");
		sym_AndroidBitmap_lockPixels=dlsym(handle,"AndroidBitmap_lockPixels");
		sym_AndroidBitmap_unlockPixels=dlsym(handle,"AndroidBitmap_unlockPixels");

		if (sym_AndroidBitmap_getInfo==NULL || sym_AndroidBitmap_lockPixels==NULL
			|| sym_AndroidBitmap_unlockPixels==NULL){
			ms_warning("AndroidBitmap not available.");
		}else{
			ms_filter_register(&ms_android_display_desc);
			ms_message("MSAndroidDisplay registered.");
			return TRUE;
		}
	}else{
		ms_warning("libjnigraphics.so cannot be loaded.");
	}
	return FALSE;
}
