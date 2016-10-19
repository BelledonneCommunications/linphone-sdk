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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"
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
	MSScalerContext *sws2;
	bool_t auto_window;
	bool_t own_window;
	bool_t ready;
	bool_t autofit;
	bool_t mirror;
	bool_t show;
} X11Video;



static Display *init_display(void){
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
	obj->sws2=NULL;
	obj->own_window=FALSE;
	obj->auto_window=TRUE;
	obj->ready=FALSE;
	obj->autofit=TRUE;
	obj->mirror=FALSE;
	obj->display=init_display();
	obj->vsize=def_size; /* the size of the main video*/
	obj->lsize=def_size; /* the size of the local preview*/
	obj->wsize=def_size; /* the size of the window*/
	obj->show=TRUE;
	obj->port=-1;
	f->data=obj;

	XSetErrorHandler(x11error_handler);
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
	XvPortID port=(XvPortID)-1;
	int imgfmt_id=0;
	XShmSegmentInfo *shminfo=&s->shminfo;
	XWindowAttributes wa = {0};

	if (s->display==NULL) return;
	if (s->window_id==0){
		if(s->auto_window) {
			s->window_id=createX11Window(s);
		}
		if (s->window_id==0) return;
		s->own_window=TRUE;
	}

	/* Make sure X11 window is ready to use*/
	XSync(s->display, False);

	if (s->own_window==FALSE){
		/*we need to register for resize events*/
		XSelectInput(s->display,s->window_id,StructureNotifyMask);
	}
	XGetWindowAttributes(s->display,s->window_id,&wa);
	XClearWindow(s->display,s->window_id);
	ms_message("x11video_prepare(): Window has size %ix%i, received video is %ix%i",wa.width,wa.height,s->vsize.width,s->vsize.height);

	if (wa.width<MS_LAYOUT_MIN_SIZE || wa.height<MS_LAYOUT_MIN_SIZE){
		return;
	}

	s->wsize.width=wa.width;
	s->wsize.height=wa.height;
	s->fbuf.w=s->vsize.width;
	s->fbuf.h=s->vsize.height;

	s->port=(XvPortID)-1;
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

	for (n=0;n<nadaptors && port==(XvPortID)-1;++n){
		XvAdaptorInfo *ai=&xai[n];
		XvImageFormatValues *imgfmt;
		int nimgfmt=0;

		ms_message("Found output adaptor; name=%s num_ports=%i, with %i formats:",
		           ai->name,(int)ai->num_ports,(int)ai->num_formats);
		imgfmt=XvListImageFormats(s->display,ai->base_id,&nimgfmt);
		for(i=0;i<nimgfmt;++i){
			char fcc[5]={0};
			memcpy(fcc,&imgfmt[i].id,4);
			ms_message("type=%s/%s id=%s",
			           imgfmt[i].type == XvYUV ? "YUV" : "RGB",
			            imgfmt[i].format==XvPlanar ? "Planar" : "Packed",fcc);
			if (port==(XvPortID)-1 && imgfmt[i].format==XvPlanar && strcasecmp(fcc,"YV12")==0){
				unsigned int k;
				/*we found a format interesting to us*/
				for(k=0;k<ai->num_ports;++k){
					if (XvGrabPort(s->display,ai->base_id+k,CurrentTime)==0){
						ms_message("Grabbed port %i",(int)ai->base_id+k);
						port=(XvPortID)(ai->base_id+k);
						imgfmt_id=imgfmt[i].id;
						break;
					}
				}
			}
		}
		if (imgfmt) XFree(imgfmt);
	}
	XvFreeAdaptorInfo(xai);
	if (port==(XvPortID)-1){
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
	if (s->port!=(XvPortID)-1){
		XvStopVideo(s->display,s->port, s->window_id);
		XvUngrabPort(s->display,s->port,CurrentTime);
		s->port=(XvPortID)-1;
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
	if (s->sws2){
		ms_scaler_context_free (s->sws2);
		s->sws2=NULL;
	}
	if (s->local_msg){
		freemsg(s->local_msg);
		s->local_msg=NULL;
	}
	s->ready=FALSE;
}

static void x11video_preprocess(MSFilter *f){
	X11Video *obj=(X11Video*)f->data;
	if (obj->show) {
		if (obj->ready) x11video_unprepare(f);
		x11video_prepare(f);
	}
}


static void x11video_process(MSFilter *f){
	X11Video *obj=(X11Video*)f->data;
	mblk_t *inm;
	int update=0;
	MSPicture lsrc={0};
	MSPicture src={0};
	MSRect mainrect,localrect;
	bool_t precious=FALSE;
	bool_t local_precious=FALSE;
	XWindowAttributes wa;
	MSTickerLateEvent late_info;

	ms_filter_lock(f);

	if ((obj->window_id == 0) || (x11_error == TRUE)) goto end;

	XGetWindowAttributes(obj->display,obj->window_id,&wa);
	if (x11_error == TRUE) {
		ms_error("Could not get window attributes for window %lu", obj->window_id);
		goto end;
	}
	if (wa.width!=obj->wsize.width || wa.height!=obj->wsize.height){
		ms_warning("Resized to %ix%i", wa.width,wa.height);
		obj->wsize.width=wa.width;
		obj->wsize.height=wa.height;
		XClearWindow(obj->display,obj->window_id);
	}

	ms_ticker_get_last_late_tick(f->ticker, &late_info);
	if(late_info.current_late_ms > 100) {
		ms_warning("Dropping frames because we're late");
		goto end;
	}

	if (!obj->show) {
		goto end;
	}
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
				ms_message("received size is %ix%i",newsize.width,newsize.height);
				obj->vsize=newsize;
				if (obj->autofit){
					MSVideoSize new_window_size;
					static const MSVideoSize min_size=MS_VIDEO_SIZE_QVGA;
					/*don't resize less than QVGA, it is too small*/
					if (min_size.width*min_size.height>newsize.width*newsize.height){
						new_window_size.width=newsize.width*2;
						new_window_size.height=newsize.height*2;
					}else new_window_size=newsize;
					obj->wsize=new_window_size;
					ms_message("autofit: new window size should be %ix%i",new_window_size.width,new_window_size.height);
					XResizeWindow(obj->display,obj->window_id,new_window_size.width,new_window_size.height);
					XSync(obj->display,FALSE);
				}
				x11video_unprepare(f);
				x11video_prepare(f);
				if (!obj->ready) goto end;
			}
		}
		update=1;
	}
	/*process last video message for local preview*/
	if (obj->corner!=-1 && f->inputs[1]!=NULL && (inm=ms_queue_peek_last(f->inputs[1]))!=0) {
		if (ms_yuv_buf_init_from_mblk(&lsrc,inm)==0){
			obj->lsize.width=lsrc.w;
			obj->lsize.height=lsrc.h;
			local_precious=mblk_get_precious_flag(inm);
			update=1;
		}
	}

	ms_layout_compute(obj->vsize, obj->vsize,obj->lsize,obj->corner,obj->scale_factor,&mainrect,&localrect);

	if (lsrc.w!=0 && obj->corner!=-1){
		/* first reduce the local preview image into a temporary image*/
		if (obj->local_msg==NULL){
			obj->local_msg=ms_yuv_buf_alloc(&obj->local_pic,localrect.w,localrect.h);
		}
		if (obj->sws2==NULL){
			obj->sws2=ms_scaler_create_context(lsrc.w,lsrc.h,MS_YUV420P,localrect.w,localrect.h,MS_YUV420P,
			                             MS_SCALER_METHOD_BILINEAR);
		}
		ms_scaler_process(obj->sws2,lsrc.planes,lsrc.strides,obj->local_pic.planes,obj->local_pic.strides);
		if (!local_precious) ms_yuv_buf_mirror(&obj->local_pic);
	}

	if (update && src.w!=0){
		ms_yuv_buf_copy(src.planes,src.strides,obj->fbuf.planes,obj->fbuf.strides,obj->vsize);
		if (obj->mirror && !precious) ms_yuv_buf_mirror(&obj->fbuf);
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
		MSRect rect;
		ms_layout_center_rectangle(obj->wsize,obj->vsize,&rect);
		//ms_message("XvShmPutImage() %ix%i --> %ix%i",obj->fbuf.w,obj->fbuf.h,obj->wsize.width,obj->wsize.height);

		XvShmPutImage(obj->display,obj->port,obj->window_id,obj->gc, obj->xv_image,
		              0,0,obj->fbuf.w,obj->fbuf.h,
		              rect.x,rect.y,rect.w,rect.h,TRUE);
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

static int x11video_show_video(MSFilter *f, void *arg){
	X11Video *s=(X11Video*)f->data;
	bool_t show=*(bool_t*)arg;
	s->show=show?TRUE:FALSE;
	if (s->show==FALSE) {
		ms_filter_lock(f);
		x11video_unprepare(f);
		ms_filter_unlock(f);
	}

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
	if(s->auto_window) {
		*id=s->window_id;
	} else {
		*id = MS_FILTER_VIDEO_NONE;
	}
	return 0;
}

static int x11video_set_native_window_id(MSFilter *f, void*arg){
	X11Video *s=(X11Video*)f->data;
	unsigned long id=*(unsigned long*)arg;
	ms_filter_lock(f);
	if(id != MS_FILTER_VIDEO_NONE) {
		x11video_unprepare(f);
		s->autofit=FALSE;
		s->auto_window=TRUE;
		s->window_id=id;
		x11video_prepare(f);
	} else {
		s->window_id=0;
		s->auto_window=FALSE;
	}
	ms_filter_unlock(f);
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
	{	MS_VIDEO_DISPLAY_SHOW_VIDEO			, x11video_show_video },
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
