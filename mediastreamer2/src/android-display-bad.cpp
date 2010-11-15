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

#include "ffmpeg-priv.h"

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "layouts.h"
#include <android/bitmap.h>
#include <jni.h>
#include <dlfcn.h>

/*defined in msandroid.cpp*/
extern JavaVM *ms_andsnd_jvm;


#if 0
struct SurfaceInfo {
	uint32_t    w;
	uint32_t    h;
	uint32_t    bpr;
	PixelFormat format;
	void*       bits;
	void*       base;
	uint32_t    reserved[2];
	};
#else
	struct SurfaceInfo {
	uint32_t    w;
        uint32_t    h;
        uint32_t    s;
        uint32_t    usage;
        PixelFormat format;
        void*       bits;
        uint32_t    reserved[2];
	};
#endif


class RefBase{
};

class Surface : public RefBase{
};


typedef int (*Android_Surface_lock)(Surface *surf, SurfaceInfo *info,bool blocking);
typedef int (*Android_Surface_unlockAndPost)(Surface *surf);
typedef int (*Android_RefBase_incStrong)(RefBase *obj, const void *);
typedef int (*Android_RefBase_decStrong)(RefBase *obj, const void *);

static Android_Surface_lock sym_Android_Surface_lock=NULL;
static Android_Surface_unlockAndPost sym_Android_Surface_unlockAndPost=NULL;
static Android_RefBase_incStrong sym_Android_RefBase_incStrong=NULL;
static Android_RefBase_decStrong sym_Android_RefBase_decStrong=NULL;

typedef struct AndroidDisplay{
	JavaVM *jvm;
	Surface *surf;
	jfieldID surface_id;	/* private mSurface field of android.view.Surface */
	jmethodID get_surface_id; /* AndroidVideoWindowImpl.getSurface() method */
	struct ms_SwsContext *sws;
	MSVideoSize vsize;
	MSVideoSize wsize;
}AndroidDisplay;


static void android_display_init(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)ms_new0(AndroidDisplay,1);
	JNIEnv *jenv=NULL;
	jclass wc;

	ad->jvm=ms_andsnd_jvm;

	if (ad->jvm->AttachCurrentThread(&jenv,NULL)!=0){
		ms_error("Could not get JNIEnv");
		return ;
	}
	wc=jenv->FindClass("org/linphone/core/AndroidVideoWindowImpl");
	if (wc==0){
		ms_fatal("Could not find org.linphone.core.AndroidVideoWindowImpl class !");
	}
	ad->get_surface_id=jenv->GetMethodID(wc,"getSurface", "()Landroid/view/Surface;");
	if (ad->get_surface_id==NULL){
		ms_fatal("Could not find getSurface() method of AndroidVideoWindowImpl");
		return;
	}

	wc=jenv->FindClass("android/view/Surface");
	if (wc==0){
		ms_fatal("Could not find android/view/Surface class !");
	}
	ad->surface_id=jenv->GetFieldID(wc,"mSurface","I");
	if (ad->surface_id==NULL){
		ms_fatal("Could not find mSurface field of android.view.Surface");
		return;
	}

	MS_VIDEO_SIZE_ASSIGN(ad->vsize,CIF);
	MS_VIDEO_SIZE_ASSIGN(ad->wsize,CIF);
	f->data=ad;
}

static void android_display_uninit(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	if (ad->sws){
		ms_sws_freeContext (ad->sws);
		ad->sws=NULL;
	}
	ms_free(ad);
}

static void android_display_preprocess(MSFilter *f){
	
}

static void android_display_process(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	MSPicture pic;
	mblk_t *m;
	JNIEnv *jenv=NULL;

	jint result = ad->jvm->AttachCurrentThread(&jenv,NULL);
	if (result != 0) {
		ms_error("android_display_process(): cannot attach VM");
		goto end;
	}

	ms_filter_lock(f);
	if (ad->surf!=NULL){
		if ((m=ms_queue_peek_last(f->inputs[0]))!=NULL){
			if (ms_yuv_buf_init_from_mblk (&pic,m)==0){
				SurfaceInfo info;
				if (sym_Android_Surface_lock(ad->surf,&info,true)==0){
					MSVideoSize wsize={info.w,info.h};
					MSVideoSize vsize={pic.w, pic.h};
					MSRect vrect;
					MSPicture dest={0};
			
					//ms_message("Got surface with w=%i, h=%i, s=%i",info.w, info.h, info.s);

					if (!ms_video_size_equal(vsize,ad->vsize) 
						|| !ms_video_size_equal(wsize,ad->wsize) ){
						ad->vsize=vsize;
						ad->wsize=wsize;
						if (ad->sws){
							ms_sws_freeContext (ad->sws);
							ad->sws=NULL;
						}
					}
				
					ms_layout_compute(wsize,vsize,vsize,-1,0,&vrect, NULL);

					if (ad->sws==NULL){
						ad->sws=ms_sws_getContext (vsize.width,vsize.height,PIX_FMT_YUV420P,
							                   vrect.w,vrect.h,PIX_FMT_RGB565,SWS_BILINEAR,NULL,NULL,NULL);
						if (ad->sws==NULL){
							ms_fatal("Could not obtain sws context !");
						}
					}
				
					dest.planes[0]=(uint8_t*)info.bits+(vrect.y*info.s*2)+(vrect.x*2);
					dest.strides[0]=info.s*2;
					ms_sws_scale (ad->sws,pic.planes,pic.strides,0,pic.h,dest.planes,dest.strides);
					sym_Android_Surface_unlockAndPost(ad->surf);
				}else{
					ms_error("AndroidBitmap_lockPixels() failed !");
				}
			}
		}
	}
	ms_filter_unlock(f);
	
end:
	ms_queue_flush(f->inputs[0]);
	ms_queue_flush(f->inputs[1]);

	if (jenv!=NULL){
		jint result = ad->jvm->DetachCurrentThread();
		if (result != 0) {
			ms_error("android_display_process(): cannot detach VM");
		}
	}
}

static int android_display_set_window(MSFilter *f, void *arg){
	AndroidDisplay *ad=(AndroidDisplay*)f->data;
	unsigned long id=*(unsigned long*)arg;
	int err;
	JNIEnv *jenv=NULL;
	jobject jsurface=NULL;
	jobject android_window=(jobject)id;
	Surface *oldsurf;
	
	if (ad->jvm->AttachCurrentThread(&jenv,NULL)!=0){
		ms_error("Could not get JNIEnv");
		return -1;
	}
	if (android_window!=NULL)
		jsurface=jenv->CallObjectMethod(android_window,ad->get_surface_id);
	
	ms_filter_lock(f);
	oldsurf=ad->surf;
	if (jsurface!=NULL) ad->surf=(Surface*)jenv->GetIntField(jsurface,ad->surface_id);
	else ad->surf=NULL;
	if (ad->surf)
		sym_Android_RefBase_incStrong(ad->surf,NULL);
	if (oldsurf)
		sym_Android_RefBase_decStrong(oldsurf,NULL);
	ms_filter_unlock(f);

	ms_message("Got new surface to draw (%p)",ad->surf);
}

static MSFilterMethod methods[]={
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , android_display_set_window },
	{	0, NULL	}
};

static MSFilterDesc ms_android_display_bad_desc={
	MS_ANDROID_DISPLAY_ID,
	"MSAndroidDisplay",
	"Video display filter for Android (unofficial)",
	MS_FILTER_OTHER,
	NULL,	
	2, /*number of inputs*/
	0, /*number of outputs*/
	android_display_init,
	android_display_preprocess,
	android_display_process,
	NULL,
	android_display_uninit,
	methods
};

static void *loadSymbol(void *handle, const char *symbol, int *error){
	void *ret=dlsym(handle,symbol);
	if (ret==NULL){
		ms_warning("Could not load symbol %s",symbol);
		*error=1;
	}
	return ret;
}

#define LIBSURFACE_SO "libsurfaceflinger_client.so"

extern "C" void libmsandroiddisplaybad_init(void){
	void *handle=dlopen(LIBSURFACE_SO,RTLD_LAZY);
	if (handle!=NULL){
		int error=0;
		sym_Android_Surface_lock=(Android_Surface_lock)loadSymbol(handle,"_ZN7android7Surface4lockEPNS0_11SurfaceInfoEb", &error);
		sym_Android_Surface_unlockAndPost=(Android_Surface_unlockAndPost)loadSymbol(handle,"_ZN7android7Surface13unlockAndPostEv",&error);
		
		handle=dlopen("libutils.so",RTLD_LAZY);
		if (handle!=NULL){
			sym_Android_RefBase_decStrong=(Android_RefBase_decStrong)loadSymbol(handle,"_ZNK7android7RefBase9decStrongEPKv",&error);
			sym_Android_RefBase_incStrong=(Android_RefBase_incStrong)loadSymbol(handle,"_ZNK7android7RefBase9incStrongEPKv",&error);
		}else{
			ms_warning("Could not load libutils.so");
			error=1;
		}
		if (error==0){
			ms_filter_register(&ms_android_display_bad_desc);
			ms_message("Android display filter (the bad one) loaded."); 
		}
	}else ms_message("Could not load "LIBSURFACE_SO);
}



