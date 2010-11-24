/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "ffmpeg-priv.h"
#include "layouts.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/XShm.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#define SCALE_FACTOR 4.0f

static bool_t x11_error=FALSE;

static int x11error_handler(Display *d, XErrorEvent*ev){
	ms_error("X11 error reported.");
	x11_error=TRUE;
	return 0;
}

static void x11video_unprepare(MSFilter *f);

typedef struct X11Video
{
	MSPicture fbuf;
	MSPicture local_pic;
	mblk_t *local_msg;
	MSVideoSize wsize; /*wished window size */
	MSVideoSize vsize;
	MSVideoSize lsize;
	int corner; /*for selfview*/
	float scale_factor; /*for selfview*/
	uint8_t background_color[4];
	Display *display;
	Window window_id;
	XvPortID port;
	XShmSegmentInfo shminfo;
	XvImage *xv_image;
	GC gc;
	struct ms_SwsContext *sws1;
	struct ms_SwsContext *sws2;
	bool_t own_window;
	bool_t ready;
	bool_t autofit;
	bool_t mirror;
} X11Video;



static Display *init_display(){
	const char *display;
	Display *ret;
	display=getenv("DISPLAY");
	if (display==NULL) display=":0";
	ret=XOpenDisplay(display);
	if (ret==NULL){
		ms_error("Could not open display %s",display);
	}
	return ret;
}

static void x11video_init(MSFilter  *f){
	X11Video *obj=(X11Video*)ms_new0(X11Video,1);
	MSVideoSize def_size;
	def_size.width=MS_VIDEO_SIZE_CIF_W;
	def_size.height=MS_VIDEO_SIZE_CIF_H;
	obj->local_msg=NULL;
	obj->corner=0;
	obj->scale_factor=SCALE_FACTOR;
	obj->background_color[0]=obj->background_color[1]=obj->background_color[2]=0;
	obj->sws1=NULL;
	obj->sws2=NULL;
	obj->own_window=FALSE;
	obj->ready=FALSE;
	obj->autofit=FALSE;
	obj->mirror=FALSE;
	obj->display=init_display();
	obj->vsize=def_size;
	obj->lsize=def_size;
	obj->wsize=def_size;
	f->data=obj;
}


static void x11video_uninit(MSFilter *f){
	X11Video *obj=(X11Video*)f->data;
	
	x11video_unprepare(f);
	if (obj->own_window){
		XDestroyWindow(obj->display,obj->window_id);
	}
	if (obj->display){
		XCloseDisplay(obj->display);
		obj->display=NULL;
	}
	ms_free(obj);
}



static Window createX11Window(X11Video *s){
	Window w;
	XSetWindowAttributes wa;
	/*
	w=XCreateSimpleWindow(s->display,DefaultRootWindow(s->display),200,200,
	                      s->fbuf.w, s->fbuf.h,0,0,0);

	*/
	memset(&wa,0,sizeof(wa));
	wa.event_mask=StructureNotifyMask;
	w=XCreateWindow(s->display,DefaultRootWindow(s->display),200,200,
	                      s->wsize.width, s->wsize.height,0,CopyFromParent,CopyFromParent,CopyFromParent,
	                CWEventMask|CWBackPixel,&wa);
	
	if (w==0){
		ms_error("Could not create X11 window.");
		return 0;
	}
	XMapWindow(s->display,w);
	return w;
}

static void x11video_fill_background(MSFilter *f){
	X11Video *s=(X11Video*)f->data;
	uint8_t rgb[3]={0,0,0};
	uint8_t yuv[3]={0,0,0};
	int ysize=s->fbuf.h*s->fbuf.strides[0];
	int usize=s->fbuf.h*s->fbuf.strides[1]/2;
	int vsize=s->fbuf.h*s->fbuf.strides[2]/2;
	ms_rgb_to_yuv(rgb,yuv);
	memset(s->fbuf.planes[0],yuv[0],ysize);
	memset(s->fbuf.planes[1],yuv[1],usize);
	memset(s->fbuf.planes[2],yuv[2],vsize);
}

static void x11video_prepare(MSFilter *f){
	X11Video *s=(X11Video*)f->data;
	unsigned int n;
	unsigned int nadaptors;
	int i;
	XvAdaptorInfo *xai=NULL;
	XvPortID port=-1;
	int imgfmt_id=0;
	XShmSegmentInfo *shminfo=&s->shminfo;
	XWindowAttributes wa;
	
	if (s->display==NULL) return;
	if (s->window_id==0){
		s->window_id=createX11Window(s);
		if (s->window_id==0) return;
		s->own_window=TRUE;
	}else if (s->own_window==FALSE){
		/*we need to register for resize events*/
		XSelectInput(s->display,s->window_id,StructureNotifyMask);
	}
	XGetWindowAttributes(s->display,s->window_id,&wa);
	ms_message("Window has size %i,%i",wa.width,wa.height);

	if (wa.width<MS_LAYOUT_MIN_SIZE || wa.height<MS_LAYOUT_MIN_SIZE){
		return;
	}
	
	s->fbuf.w=wa.width & ~0x1;
	s->fbuf.h=wa.height  & ~0x1;
	/* we might want to resize it */
	XResizeWindow(s->display,s->window_id,s->fbuf.w,s->fbuf.h);
	XSync(s->display,TRUE);
	
	
	s->port=-1;
	if (XvQueryExtension(s->display, &n, &n, &n, &n, &n)!=0){
		ms_error("Fail to query xv extension");
		return;
	}
	
	if (XShmQueryExtension(s->display)==0){
		ms_error("Fail to query xshm extension");
		return;
	}

	if (XvQueryAdaptors(s->display,DefaultRootWindow(s->display),
	                                 &nadaptors, &xai)!=0){
		ms_error("XvQueryAdaptors failed.");
		return;
	}
	XSetErrorHandler(x11error_handler);
	for (n=0;n<nadaptors && port==-1;++n){
		XvAdaptorInfo *ai=&xai[n];
		XvImageFormatValues *imgfmt;
		int nimgfmt=0;
		
		ms_message("Found output adaptor; name=%s num_ports=%i, with formats:",
		           ai->name,ai->num_ports,ai->num_formats);
		imgfmt=XvListImageFormats(s->display,ai->base_id,&nimgfmt);
		for(i=0;i<nimgfmt;++i){
			char fcc[5]={0};
			memcpy(fcc,&imgfmt[i].id,4);
			ms_message("type=%s/%s id=%s",
			           imgfmt[i].type == XvYUV ? "YUV" : "RGB",
			            imgfmt[i].format==XvPlanar ? "Planar" : "Packed",fcc);
			if (port==-1 && imgfmt[i].format==XvPlanar && strcasecmp(fcc,"YV12")==0){
				int k;
				/*we found a format interesting to us*/
				for(k=0;k<ai->num_ports;++k){
					if (XvGrabPort(s->display,ai->base_id+k,CurrentTime)==0){
						ms_message("Grabbed port %i",ai->base_id+k);
						port=ai->base_id+k;
						imgfmt_id=imgfmt[i].id;
						break;
					}
				}
			}
		}
	}
	XvFreeAdaptorInfo(xai);
	if (port==-1){
		ms_error("Could not find suitable format or Xv port to work with.");
		return;
	}
	s->port=port;

	/*create the shared memory XvImage*/
	memset(shminfo,0,sizeof(*shminfo));
	s->xv_image=XvShmCreateImage(s->display,s->port,imgfmt_id,NULL,s->fbuf.w,s->fbuf.h,shminfo);
	if (s->xv_image==NULL){
		ms_error("XvShmCreateImage failed.");
		x11video_unprepare(f);
		return;
	}
	/*allocate some shared memory to receive the pixel data */
	shminfo->shmid=shmget(IPC_PRIVATE, s->xv_image->data_size,IPC_CREAT | 0777);
	if (shminfo->shmid==-1){
		ms_error("Could not allocate %i bytes of shared memory: %s",
		         s->xv_image->data_size,
		         strerror(errno));
		x11video_unprepare(f);
		return;
	}
	shminfo->shmaddr=shmat(shminfo->shmid,NULL,0);
	if (shminfo->shmaddr==(void*)-1){
		ms_error("shmat() failed: %s",strerror(errno));
		shminfo->shmaddr=NULL;
		x11video_unprepare(f);
		return;
	}
	/*ask the x-server to attach this shared memory segment*/
	x11_error=FALSE;
	if (!XShmAttach(s->display,shminfo)){
		ms_error("XShmAttach failed !");
		x11video_unprepare(f);
		return ;
	}
	s->xv_image->data=s->shminfo.shmaddr;
	s->fbuf.planes[0]=(void*)s->xv_image->data;
	s->fbuf.planes[2]=s->fbuf.planes[0]+(s->xv_image->height*s->xv_image->pitches[0]);
	s->fbuf.planes[1]=s->fbuf.planes[2]+((s->xv_image->height/2)*s->xv_image->pitches[1]);
	s->fbuf.strides[0]=s->xv_image->pitches[0];
	s->fbuf.strides[2]=s->xv_image->pitches[1];
	s->fbuf.strides[1]=s->xv_image->pitches[2];

	/* set picture black */
	x11video_fill_background(f);

	/*Create a GC*/
	s->gc=XCreateGC(s->display,s->window_id,0,NULL);
	if (s->gc==NULL){
		ms_error("XCreateGC() failed.");
		x11video_unprepare(f);
		return ;
	}
	
	s->ready=TRUE;
}

static void x11video_unprepare(MSFilter *f){
	X11Video *s=(X11Video*)f->data;
	if (s->port!=-1){
		XvUngrabPort(s->display,s->port,CurrentTime);
	}
	if (s->shminfo.shmaddr!=NULL){
		XShmDetach(s->display,&s->shminfo);
		shmdt(s->shminfo.shmaddr);
		shmctl(s->shminfo.shmid,IPC_RMID,NULL);
		memset(&s->shminfo,0,sizeof(s->shminfo));
	}
	if (s->gc){
		XFreeGC(s->display,s->gc);
		s->gc=NULL;
	}
	if (s->xv_image) {
		XFree(s->xv_image);
		s->xv_image=NULL;
	}
	if (s->sws1){
		ms_sws_freeContext (s->sws1);
		s->sws1=NULL;
	}
	if (s->sws2){
		ms_sws_freeContext (s->sws2);
		s->sws2=NULL;
	}
	if (s->local_msg){
		freemsg(s->local_msg);
		s->local_msg=NULL;
	}
	s->ready=FALSE;
}

static void x11video_preprocess(MSFilter *f){
	x11video_prepare(f);
}


static void x11video_process(MSFilter *f){
	X11Video *obj=(X11Video*)f->data;
	mblk_t *inm;
	int update=0;
	MSVideoSize wsize;
	MSPicture lsrc={0};
	MSPicture src={0};
	MSRect mainrect,localrect;
	bool_t resized=FALSE;
	bool_t precious=FALSE;
	
	while (XPending(obj->display)>0){
		XEvent ev;
		
		XNextEvent(obj->display,&ev);
		if (ev.type==ConfigureNotify){
			if (ev.xconfigure.width!=obj->fbuf.w || ev.xconfigure.height!=obj->fbuf.h){
				ms_message("We are resized to %i,%i",ev.xconfigure.width,ev.xconfigure.height);
				resized=TRUE;
			}
		}
	}
	if (resized) x11video_unprepare(f);
	ms_filter_lock(f);
	if (!obj->ready) x11video_prepare(f);
	if (!obj->ready){
		goto end;
	}

	
	if (f->inputs[0]!=NULL && (inm=ms_queue_peek_last(f->inputs[0]))!=0) {
		if (ms_yuv_buf_init_from_mblk(&src,inm)==0){
			MSVideoSize newsize;
			newsize.width=src.w;
			newsize.height=src.h;
			precious=mblk_get_precious_flag(inm);
			if (!ms_video_size_equal(newsize,obj->vsize) ) {
				if (obj->sws1){
					ms_sws_freeContext (obj->sws1);
					obj->sws1=NULL;
				}
				if (obj->autofit){
					MSVideoSize qvga_size;
					MSVideoSize new_window_size;
					qvga_size.width=MS_VIDEO_SIZE_QVGA_W;
					qvga_size.height=MS_VIDEO_SIZE_QVGA_H;
					
					ms_message("received size is %ix%i",newsize.width,newsize.height);
					/*don't resize less than QVGA, it is too small*/
					if (ms_video_size_greater_than(qvga_size,newsize)){
						new_window_size.width=MS_VIDEO_SIZE_QVGA_W;
						new_window_size.height=MS_VIDEO_SIZE_QVGA_H;
					}else new_window_size=newsize;
					obj->vsize=newsize;
					ms_message("autofit: new window size is %ix%i",new_window_size.width,new_window_size.height);
					XResizeWindow(obj->display,obj->window_id,new_window_size.width,new_window_size.height);
					XSync(obj->display,FALSE);
					x11video_unprepare(f);
					x11video_prepare(f);
					if (!obj->ready) goto end;
				}
			}
		}
		update=1;
	}
	/*process last video message for local preview*/
	if (f->inputs[1]!=NULL && (inm=ms_queue_peek_last(f->inputs[1]))!=0) {
		if (ms_yuv_buf_init_from_mblk(&lsrc,inm)==0){
			obj->lsize.width=lsrc.w;
			obj->lsize.height=lsrc.h;
			update=1;
		}
	}
	
	wsize.width=obj->fbuf.w;
	wsize.height=obj->fbuf.h;
	ms_layout_compute(wsize, obj->vsize,obj->lsize,obj->corner,obj->scale_factor,&mainrect,&localrect);

	if (lsrc.w!=0){
		/* first reduce the local preview image into a temporary image*/
		if (obj->local_msg==NULL){
			obj->local_msg=ms_yuv_buf_alloc(&obj->local_pic,localrect.w,localrect.h);
		}
		if (obj->sws2==NULL){
			obj->sws2=ms_sws_getContext (lsrc.w,lsrc.h,PIX_FMT_YUV420P,localrect.w,localrect.h,PIX_FMT_YUV420P,
			                             SWS_FAST_BILINEAR,NULL,NULL,NULL);
		}
		ms_sws_scale (obj->sws2,lsrc.planes,lsrc.strides,0,lsrc.h,obj->local_pic.planes,obj->local_pic.strides);
	}
	
	if (src.w!=0){
		MSPicture mainpic;
		mainpic.w=mainrect.w;
		mainpic.h=mainrect.h;
		mainpic.planes[0]=obj->fbuf.planes[0]+(mainrect.y*obj->fbuf.strides[0])+mainrect.x;
		mainpic.planes[1]=obj->fbuf.planes[1]+((mainrect.y*obj->fbuf.strides[1])/2)+(mainrect.x/2);
		mainpic.planes[2]=obj->fbuf.planes[2]+((mainrect.y*obj->fbuf.strides[2])/2)+(mainrect.x/2);
		mainpic.planes[3]=NULL;
		mainpic.strides[0]=obj->fbuf.strides[0];
		mainpic.strides[1]=obj->fbuf.strides[1];
		mainpic.strides[2]=obj->fbuf.strides[2];
		mainpic.strides[3]=0;
		/*scale the main video */
		if (obj->sws1==NULL){
			obj->sws1=ms_sws_getContext(src.w,src.h,PIX_FMT_YUV420P,
			mainrect.w,mainrect.h,PIX_FMT_YUV420P,
			SWS_FAST_BILINEAR, NULL, NULL, NULL);
		}
		if (ms_sws_scale(obj->sws1,src.planes,src.strides, 0,
					src.h, mainpic.planes, mainpic.strides)<0){
			ms_error("Error in ms_sws_scale().");
		}
		if (obj->mirror && !precious) ms_yuv_buf_mirror(&mainpic);
	}

	/*copy resized local view into a corner:*/
	if (update && obj->local_msg!=NULL && obj->corner!=-1){
		MSPicture corner=obj->fbuf;
		MSVideoSize roi;
		roi.width=obj->local_pic.w;
		roi.height=obj->local_pic.h;
		corner.w=obj->local_pic.w;
		corner.h=obj->local_pic.h;
		corner.planes[0]+=localrect.x+(localrect.y*corner.strides[0]);
		corner.planes[1]+=(localrect.x/2)+((localrect.y/2)*corner.strides[1]);
		corner.planes[2]+=(localrect.x/2)+((localrect.y/2)*corner.strides[2]);
		corner.planes[3]=0;
		ms_yuv_buf_copy(obj->local_pic.planes,obj->local_pic.strides,
				corner.planes,corner.strides,roi);
	}
	if (update){
		XvShmPutImage(obj->display,obj->port,obj->window_id,obj->gc, obj->xv_image,
		              0,0,obj->fbuf.w,obj->fbuf.h,
		              0,0,obj->fbuf.w,obj->fbuf.h,FALSE);
		XSync(obj->display,FALSE);
	}
	end:
		ms_filter_unlock(f);
		if (f->inputs[0]!=NULL)
			ms_queue_flush(f->inputs[0]);
		if (f->inputs[1]!=NULL)
			ms_queue_flush(f->inputs[1]);
}

static int x11video_set_vsize(MSFilter *f,void *arg){
	X11Video *s=(X11Video*)f->data;
	ms_filter_lock(f);
	s->wsize=*(MSVideoSize*)arg;
	ms_filter_unlock(f);
	return 0;
}


static int x11video_auto_fit(MSFilter *f, void *arg){
	X11Video *s=(X11Video*)f->data;
	s->autofit=*(int*)arg;
	return 0;
}

static int x11video_set_corner(MSFilter *f,void *arg){
	X11Video *s=(X11Video*)f->data;
	ms_filter_lock(f);
	s->corner= *(int*)arg;
	ms_filter_unlock(f);
	return 0;
}

static int x11video_set_scalefactor(MSFilter *f,void *arg){
	X11Video *s=(X11Video*)f->data;
	s->scale_factor = *(float*)arg;
	if (s->scale_factor<0.5f)
		s->scale_factor = 0.5f;
	return 0;
}

static int x11video_enable_mirroring(MSFilter *f,void *arg){
	X11Video *s=(X11Video*)f->data;
	s->mirror=*(int*)arg;
	return 0;
}

static int x11video_get_native_window_id(MSFilter *f, void*arg){
	X11Video *s=(X11Video*)f->data;
	unsigned long *id=(unsigned long*)arg;
	*id=s->window_id;
	return 0;
}

static int x11video_set_native_window_id(MSFilter *f, void*arg){
	X11Video *s=(X11Video*)f->data;
	unsigned long *id=(unsigned long*)arg;
	if (s->window_id!=0){
		ms_error("MSX11Video: Window id is already set, cannot change");
		return -1;
	}
	s->autofit=FALSE;
	s->window_id=*id;
	return 0;
}

static int x11video_set_background_color(MSFilter *f,void *arg){
	X11Video *s=(X11Video*)f->data;
	s->background_color[0]=((int*)arg)[0];
	s->background_color[1]=((int*)arg)[1];
	s->background_color[2]=((int*)arg)[2];
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_VIDEO_SIZE	,	x11video_set_vsize },
/* methods for compatibility with the MSVideoDisplay interface*/
	{	MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE , x11video_set_corner },
	{	MS_VIDEO_DISPLAY_ENABLE_AUTOFIT			, x11video_auto_fit },
	{	MS_VIDEO_DISPLAY_ENABLE_MIRRORING		, x11video_enable_mirroring },
	{	MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID	, x11video_get_native_window_id },
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID	, x11video_set_native_window_id },
	{	MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_SCALEFACTOR	, x11video_set_scalefactor },
	{	MS_VIDEO_DISPLAY_SET_BACKGROUND_COLOR    ,  x11video_set_background_color},
	{	0	,NULL}
};


MSFilterDesc ms_x11video_desc={
	.id=MS_X11VIDEO_ID,
	.name="MSX11Video",
	.text=N_("A video display using X11+Xv"),
	.category=MS_FILTER_OTHER,
	.ninputs=2,
	.noutputs=0,
	.init=x11video_init,
	.preprocess=x11video_preprocess,
	.process=x11video_process,
	.uninit=x11video_uninit,
	.methods=methods
};


MS_FILTER_DESC_EXPORT(ms_x11video_desc)
