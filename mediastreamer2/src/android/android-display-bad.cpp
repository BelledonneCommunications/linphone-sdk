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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msjava.h"

#include "layouts.h"
#include <android/bitmap.h>
#include <dlfcn.h>


static int android_version=0;

typedef unsigned int PixelFormat;

struct SurfaceInfo15{
	uint32_t    w;
	uint32_t    h;
	uint32_t    bpr;
	PixelFormat format;
	void*       bits;
	void*       base;
	uint32_t    reserved[2];
};

struct SurfaceInfo21 {
	uint32_t    w;
        uint32_t    h;
        uint32_t    s;
        uint32_t    usage;
        PixelFormat format;
        void*       bits;
        uint32_t    reserved[2];
};

union SurfaceInfo{
	SurfaceInfo15 info15;
	SurfaceInfo21 info21;
};

class SurfaceInfoAccess{
	public:
		SurfaceInfoAccess(SurfaceInfo *i){
			info=i;
		}
		int getWidth(){
			return info->info21.w;
		}
		int getHeight(){
			return info->info21.h;
		}
		int getStride(){
			return android_version<21 ? info->info15.bpr : (info->info21.s*2);
		}
		void *getBits(){
			return android_version<21 ? (info->info15.bits) : info->info21.bits;
		}
	private:
		SurfaceInfo *info;
};


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
	Surface *surf;
	jfieldID surface_id;	/* private mSurface field of android.view.Surface */
	jmethodID get_surface_id; /* AndroidVideoWindowImpl.getSurface() method */
	MSScalerContext *sws;
	MSVideoSize vsize;
	MSVideoSize wsize;
}AndroidDisplay;


static void android_display_init(MSFilter *f){
	AndroidDisplay *ad=(AndroidDisplay*)ms_new0(AndroidDisplay,1);
	JNIEnv *jenv=ms_get_jni_env();
	jclass wc;

	wc=jenv->FindClass("org/linphone/mediastream/video/AndroidVideoWindowImpl");
	if (wc==0){
		ms_fatal("Could not find org/linphone/mediastream/video/AndroidVideoWindowImpl class !");
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
		ms_scaler_context_free (ad->sws);
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

	ms_filter_lock(f);
	if (ad->surf!=NULL){
		if ((m=ms_queue_peek_last(f->inputs[0]))!=NULL){
			if (ms_yuv_buf_init_from_mblk (&pic,m)==0){
				SurfaceInfo info;
				if (sym_Android_Surface_lock(ad->surf,&info,true)==0){
					SurfaceInfoAccess infoAccess(&info);
					MSVideoSize wsize={infoAccess.getWidth(),infoAccess.getHeight()};
					MSVideoSize vsize={pic.w, pic.h};
					MSRect vrect;
					MSPicture dest={0};

					//ms_message("Got surface with w=%i, h=%i, s=%i",info.w, info.h, info.s);

					if (!ms_video_size_equal(vsize,ad->vsize)
						|| !ms_video_size_equal(wsize,ad->wsize) ){
						ad->vsize=vsize;
						ad->wsize=wsize;
						if (ad->sws){
							ms_scaler_context_free(ad->sws);
							ad->sws=NULL;
						}
					}

					ms_layout_compute(wsize,vsize,vsize,-1,0,&vrect, NULL);

					if (ad->sws==NULL){
						ad->sws=ms_scaler_create_context(vsize.width,vsize.height,MS_YUV420P,
							                   vrect.w,vrect.h,MS_RGB565,MS_SCALER_METHOD_BILINEAR);
						if (ad->sws==NULL){
							ms_fatal("Could not obtain sws context !");
						}
					}

					dest.planes[0]=(uint8_t*)infoAccess.getBits()+(vrect.y*infoAccess.getStride())+(vrect.x*2);
					dest.strides[0]=infoAccess.getStride();
					ms_scaler_process(ad->sws,pic.planes,pic.strides,dest.planes,dest.strides);
					sym_Android_Surface_unlockAndPost(ad->surf);
				}else{
					ms_error("AndroidBitmap_lockPixels() failed !");
				}
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
	JNIEnv *jenv=ms_get_jni_env();
	jobject jsurface=NULL;
	jobject android_window=(jobject)id;
	Surface *oldsurf;

	if (android_window!=NULL)
		jsurface=jenv->CallObjectMethod(android_window,ad->get_surface_id);

	ms_filter_lock(f);
	oldsurf=ad->surf;
	if (jsurface!=NULL) ad->surf=(Surface*)jenv->GetLongField(jsurface,ad->surface_id);
	else ad->surf=NULL;
	if (ad->surf)
		sym_Android_RefBase_incStrong(ad->surf,NULL);
	if (oldsurf)
		sym_Android_RefBase_decStrong(oldsurf,NULL);
	ms_filter_unlock(f);

	ms_message("Got new surface to draw (%p)",ad->surf);
	return 0;
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

#define LIBSURFACE22_SO "libsurfaceflinger_client.so"

#define LIBSURFACE21_SO "libui.so"

extern "C" void libmsandroiddisplaybad_init(MSFactory *factory){
	void *handle=dlopen(LIBSURFACE22_SO,RTLD_LAZY);
	if (handle==NULL){
		android_version=21;
		handle=dlopen(LIBSURFACE21_SO,RTLD_LAZY);
	}else android_version=22;
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
			ms_factory_register_filter(factory,&ms_android_display_bad_desc);
			ms_message("Android display filter (the bad one) loaded.");
		}
	} else ms_message("Could not load either " LIBSURFACE22_SO " or " LIBSURFACE21_SO);
}
