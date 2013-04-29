/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL, Grenoble France.

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

#include "layouts.h"

#define SCALE_FACTOR 4.0f
#define SELVIEW_POS_INACTIVE -100.0
#define LOCAL_BORDER_SIZE 2

#include <vfw.h>

static void draw_background(HDC hdc, MSVideoSize wsize, MSRect mainrect, int color[3]);
static void erase_window(HWND window, int color[3]);

typedef struct Yuv2RgbCtx{
	uint8_t *rgb;
	size_t rgblen;
	MSVideoSize dsize;
	MSVideoSize ssize;
	MSScalerContext *sws;
}Yuv2RgbCtx;

static void yuv2rgb_init(Yuv2RgbCtx *ctx){
	ctx->rgb=NULL;
	ctx->rgblen=0;
	ctx->dsize.width=0;
	ctx->dsize.height=0;
	ctx->ssize.width=0;
	ctx->ssize.height=0;
	ctx->sws=NULL;
}

static void yuv2rgb_uninit(Yuv2RgbCtx *ctx){
	if (ctx->rgb){
		ms_free(ctx->rgb);
		ctx->rgb=NULL;
		ctx->rgblen=0;
	}
	if (ctx->sws){
		ms_scaler_context_free(ctx->sws);
		ctx->sws=NULL;
	}
	ctx->dsize.width=0;
	ctx->dsize.height=0;
	ctx->ssize.width=0;
	ctx->ssize.height=0;
}

static void yuv2rgb_prepare(Yuv2RgbCtx *ctx, MSVideoSize src, MSVideoSize dst){
	if (ctx->sws!=NULL) yuv2rgb_uninit(ctx);
	ctx->sws=ms_scaler_create_context(src.width,src.height,MS_YUV420P,
			dst.width,dst.height, MS_RGB24_REV,
			MS_SCALER_METHOD_BILINEAR);
	ctx->dsize=dst;
	ctx->ssize=src;
	ctx->rgblen=dst.width*dst.height*3;
	ctx->rgb=(uint8_t*)ms_malloc0(ctx->rgblen+dst.width);
}


/*
 this function resizes the original pictures to the destination size and converts to rgb.
 It takes care of reallocating a new SwsContext and rgb buffer if the source/destination sizes have 
 changed.
*/
static void yuv2rgb_process(Yuv2RgbCtx *ctx, MSPicture *src, MSVideoSize dstsize, bool_t mirroring){
	MSVideoSize srcsize;
	
	srcsize.width=src->w;
	srcsize.height=src->h;
	if (!ms_video_size_equal(dstsize,ctx->dsize) || !ms_video_size_equal(srcsize,ctx->ssize)){	
		yuv2rgb_prepare(ctx,srcsize,dstsize);
	}
	{
		int rgb_stride=-dstsize.width*3;
		uint8_t *p;

		p=ctx->rgb+(dstsize.width*3*(dstsize.height-1));
		if (ms_scaler_process(ctx->sws,src->planes,src->strides, &p, &rgb_stride)<0){
			ms_error("Error in 420->rgb ms_scaler_process().");
		}
		if (mirroring) rgb24_mirror(ctx->rgb,dstsize.width,dstsize.height,dstsize.width*3);
	}
}

static void yuv2rgb_draw(Yuv2RgbCtx *ctx, HDRAWDIB ddh, HDC hdc, int dstx, int dsty){
	if (ctx->rgb){
		BITMAPINFOHEADER bi;
		memset(&bi,0,sizeof(bi));
		bi.biSize=sizeof(bi);
		bi.biWidth=ctx->dsize.width;
		bi.biHeight=ctx->dsize.height;
		bi.biPlanes=1;
		bi.biBitCount=24;
		bi.biCompression=BI_RGB;
		bi.biSizeImage=ctx->rgblen;

		DrawDibDraw(ddh,hdc,dstx,dsty,-1,-1,&bi,ctx->rgb,
			0,0,ctx->dsize.width,ctx->dsize.height,0);
	}
}

typedef struct _DDDisplay{
	HWND window;
	HDRAWDIB ddh;
	MSVideoSize wsize; /*the initial requested window size*/
	MSVideoSize vsize; /*the video size received for main input*/
	MSVideoSize lsize; /*the video size received for local display */
	Yuv2RgbCtx mainview;
	Yuv2RgbCtx locview;
	int sv_corner;
	float sv_scalefactor;
	float sv_posx,sv_posy;
	int background_color[3];
	bool_t need_repaint;
	bool_t autofit;
	bool_t mirroring;
	bool_t own_window;
}DDDisplay;

static LRESULT CALLBACK window_proc(
    HWND hwnd,        // handle to window
    UINT uMsg,        // message identifier
    WPARAM wParam,    // first message parameter
    LPARAM lParam)    // second message parameter
{
	DDDisplay *wd=(DDDisplay*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
	switch(uMsg){
		case WM_DESTROY:
			if (wd){
				wd->window=NULL;
			}
		break;
		case WM_SIZE:
			if (wParam==SIZE_RESTORED){
				int h=(lParam>>16) & 0xffff;
				int w=lParam & 0xffff;
				
				ms_message("Resized to %i,%i",w,h);
				
				if (wd!=NULL){
					wd->need_repaint=TRUE;
					//wd->window_size.width=w;
					//wd->window_size.height=h;
				}else{
					ms_error("Could not retrieve DDDisplay from window !");
				}
			}
		break;
		case WM_PAINT:
			if (wd!=NULL){
				wd->need_repaint=TRUE;
			}
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

static HWND create_window(int w, int h)
{
	WNDCLASS wc;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	HWND hwnd;
	RECT rect;
	wc.style = 0 ;
	wc.lpfnWndProc = window_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = NULL;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(hInstance, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName =  NULL;
	wc.lpszClassName = "Video Window";
	
	if(!RegisterClass(&wc))
	{
		/* already registred! */
	}
	rect.left=100;
	rect.top=100;
	rect.right=rect.left+w;
	rect.bottom=rect.top+h;
	if (!AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW|WS_VISIBLE /*WS_CAPTION WS_TILED|WS_BORDER*/,FALSE)){
		ms_error("AdjustWindowRect failed.");
	}
	ms_message("AdjustWindowRect: %li,%li %li,%li",rect.left,rect.top,rect.right,rect.bottom);
	hwnd=CreateWindow("Video Window", "Video window", 
		WS_OVERLAPPEDWINDOW /*WS_THICKFRAME*/ | WS_VISIBLE ,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right-rect.left,rect.bottom-rect.top,
													NULL, NULL, hInstance, NULL);
	if (hwnd==NULL){
		ms_error("Fail to create video window");
	}
	return hwnd;
}

static void dd_display_init(MSFilter  *f){
	DDDisplay *obj=(DDDisplay*)ms_new0(DDDisplay,1);
	obj->wsize.width=MS_VIDEO_SIZE_CIF_W;
	obj->wsize.height=MS_VIDEO_SIZE_CIF_H;
	obj->vsize.width=MS_VIDEO_SIZE_CIF_W;
	obj->vsize.height=MS_VIDEO_SIZE_CIF_H;
	obj->lsize.width=MS_VIDEO_SIZE_CIF_W;
	obj->lsize.height=MS_VIDEO_SIZE_CIF_H;
	yuv2rgb_init(&obj->mainview);
	yuv2rgb_init(&obj->locview);
	obj->sv_corner=0; /* bottom right*/
	obj->sv_scalefactor=SCALE_FACTOR;
	obj->sv_posx=obj->sv_posy=SELVIEW_POS_INACTIVE;
	obj->background_color[0]=obj->background_color[1]=obj->background_color[2]=0;
	obj->need_repaint=FALSE;
	obj->autofit=TRUE;
	obj->mirroring=FALSE;
	obj->own_window=TRUE;
	f->data=obj;
}

static void dd_display_prepare(MSFilter *f){
	DDDisplay *dd=(DDDisplay*)f->data;
	
	if (dd->window==NULL){
		dd->window=create_window(dd->wsize.width,dd->wsize.height);
		SetWindowLong(dd->window,GWL_USERDATA,(long)dd);
	}
	if (dd->ddh==NULL)
		dd->ddh=DrawDibOpen();
	//do not automatically resize video window if we don't created it.
	if (dd->own_window==FALSE)
		dd->autofit=FALSE;
}

static void dd_display_unprepare(MSFilter *f){
	DDDisplay *dd=(DDDisplay*)f->data;
	if(dd->window!=NULL) {
		erase_window(dd->window, dd->background_color);
	}
	if (dd->own_window && dd->window!=NULL){
		DestroyWindow(dd->window);
		dd->window=NULL;
	}
	if (dd->ddh!=NULL){
		DrawDibClose(dd->ddh);
		dd->ddh=NULL;
	}
}

static void dd_display_uninit(MSFilter *f){
	DDDisplay *obj=(DDDisplay*)f->data;
	dd_display_unprepare(f);
	yuv2rgb_uninit(&obj->mainview);
	yuv2rgb_uninit(&obj->locview);
	ms_free(obj);
}

static void dd_display_preprocess(MSFilter *f){
	dd_display_prepare(f);
}



static void draw_local_view_frame(HDC hdc, MSVideoSize wsize, MSRect localrect){
	Rectangle(hdc, localrect.x-LOCAL_BORDER_SIZE, localrect.y-LOCAL_BORDER_SIZE,
		localrect.x+localrect.w+LOCAL_BORDER_SIZE, localrect.y+localrect.h+LOCAL_BORDER_SIZE);
}

static void erase_window(HWND window, int color[3]) {
	HDC hdc=GetDC(window);
	RECT rect;
	MSVideoSize wsize;
	MSRect mainrect;
	if (hdc==NULL) {
		ms_error("Could not get window dc");
		return;
	}
	
	// Force to draw background
	GetClientRect(window, &rect);
	wsize.width = rect.right;
	wsize.height = rect.bottom;
	memset(&mainrect, 0, sizeof(MSRect));
	draw_background(hdc, wsize, mainrect, color);
}

/*
* Draws a background, that is the black rectangles at top, bottom or left right sides of the video display.
* It is normally invoked only when a full redraw is needed (notified by Windows).
*/
static void draw_background(HDC hdc, MSVideoSize wsize, MSRect mainrect, int color[3]){
	HBRUSH brush;
	RECT brect;

	brush = CreateSolidBrush(RGB(color[0],color[1],color[2]));
	if (mainrect.x>0){	
		brect.left=0;
		brect.top=0;
		brect.right=mainrect.x;
		brect.bottom=wsize.height;
		FillRect(hdc, &brect, brush);
		brect.left=mainrect.x+mainrect.w;
		brect.top=0;
		brect.right=wsize.width;
		brect.bottom=wsize.height;
		FillRect(hdc, &brect, brush);
	}
	if (mainrect.y>0){
		brect.left=0;
		brect.top=0;
		brect.right=wsize.width;
		brect.bottom=mainrect.y;
		FillRect(hdc, &brect, brush);
		brect.left=0;
		brect.top=mainrect.y+mainrect.h;
		brect.right=wsize.width;
		brect.bottom=wsize.height;
		FillRect(hdc, &brect, brush);
	}
	if (mainrect.w==0 && mainrect.h==0){
		/*no image yet, black everything*/
		brect.left=brect.top=0;
		brect.right=wsize.width;
		brect.bottom=wsize.height;
		FillRect(hdc,&brect,brush);
	}
	DeleteObject(brush);
}

static void dd_display_process(MSFilter *f){
	DDDisplay *obj=(DDDisplay*)f->data;
	RECT rect;
	MSVideoSize wsize; /* the window size*/
	MSVideoSize vsize;
	MSVideoSize lsize; /*local preview size*/
	HDC hdc;
	MSRect mainrect;
	MSRect localrect;
	MSPicture mainpic;
	MSPicture localpic;
	mblk_t *main_im=NULL;
	mblk_t *local_im=NULL;
	HDC hdc2;
	HBITMAP tmp_bmp=NULL;
	HGDIOBJ old_object=NULL;
	bool_t repainted=FALSE;
	int corner=obj->sv_corner;
	float scalefactor=obj->sv_scalefactor;

	if (obj->window==NULL){
		goto end;
	}

	if (GetClientRect(obj->window,&rect)==0
	    || rect.right<=32 || rect.bottom<=32) goto end;

	wsize.width=rect.right;
	wsize.height=rect.bottom;
	obj->wsize=wsize;
	/*get most recent message and draw it*/
	if (corner!=-1 && f->inputs[1]!=NULL && (local_im=ms_queue_peek_last(f->inputs[1]))!=NULL) {
		if (ms_yuv_buf_init_from_mblk(&localpic,local_im)==0){
			obj->lsize.width=localpic.w;
			obj->lsize.height=localpic.h;
		}
	}
	
	if (f->inputs[0]!=NULL && (main_im=ms_queue_peek_last(f->inputs[0]))!=NULL) {
		if (ms_yuv_buf_init_from_mblk(&mainpic,main_im)==0){
			if (obj->autofit && (obj->vsize.width!=mainpic.w || obj->vsize.height!=mainpic.h)
				&& (mainpic.w>wsize.width || mainpic.h>wsize.height)){
				RECT cur;
				ms_message("Detected video resolution changed, resizing window");
				GetWindowRect(obj->window,&cur);
				wsize.width=mainpic.w;
				wsize.height=mainpic.h;
				MoveWindow(obj->window,cur.left, cur.top, wsize.width, wsize.height,TRUE);
				obj->need_repaint=TRUE;
			}
			obj->vsize.width=mainpic.w;
			obj->vsize.height=mainpic.h;
		}
	}

	if (main_im!=NULL || local_im!=NULL || obj->need_repaint){
		ms_layout_compute(wsize,obj->vsize,obj->lsize,corner,scalefactor,&mainrect,&localrect);
		vsize.width=mainrect.w;
		vsize.height=mainrect.h;
		lsize.width=localrect.w;
		lsize.height=localrect.h;
		
		if (local_im!=NULL)
			yuv2rgb_process(&obj->locview,&localpic,lsize,!mblk_get_precious_flag(local_im));
	
		if (main_im!=NULL)
			yuv2rgb_process(&obj->mainview,&mainpic,vsize,obj->mirroring && !mblk_get_precious_flag(main_im));
	
		hdc=GetDC(obj->window);
		if (hdc==NULL) {
			ms_error("Could not get window dc");
			return;
		}
		/*handle the case where local view is disabled*/
		if (corner==-1 && obj->locview.rgb!=NULL){
			yuv2rgb_uninit(&obj->locview);
		}
		if (obj->locview.rgb==NULL){
			 /*One layer: we can draw directly on the displayed surface*/
			hdc2=hdc;
			if (obj->need_repaint)
				draw_background(hdc2,wsize,mainrect, obj->background_color);
		}else{
			/* in this case we need to stack several layers*/
			/*Create a second DC and bitmap to draw to a buffer that will be blitted to screen
			once all drawing is finished. This avoids some blinking while composing the image*/
			hdc2=CreateCompatibleDC(hdc);
			tmp_bmp=CreateCompatibleBitmap(hdc,wsize.width,wsize.height);
			old_object = SelectObject(hdc2, tmp_bmp);
			draw_background(hdc2,wsize,mainrect, obj->background_color);
		}

		if (obj->need_repaint){
			repainted=TRUE;
			obj->need_repaint=FALSE;
		}
		if (main_im!=NULL || obj->locview.rgb!=NULL){
			yuv2rgb_draw(&obj->mainview,obj->ddh,hdc2,mainrect.x,mainrect.y);
		}
		if (obj->locview.rgb!=NULL){
			draw_local_view_frame(hdc2,wsize,localrect);
			yuv2rgb_draw(&obj->locview,obj->ddh,hdc2,localrect.x,localrect.y);
		}
		if (hdc!=hdc2){
			if (main_im==NULL && !repainted){
				/* Blitting local rect only */
				BitBlt(hdc,localrect.x-LOCAL_BORDER_SIZE,localrect.y-LOCAL_BORDER_SIZE,
					localrect.w+LOCAL_BORDER_SIZE,localrect.h+LOCAL_BORDER_SIZE,hdc2,
					localrect.x-LOCAL_BORDER_SIZE,localrect.y-LOCAL_BORDER_SIZE,SRCCOPY);
			}else{
				/*Blitting the entire window */
				BitBlt(hdc, 0, 0, wsize.width, wsize.height, hdc2, 0, 0, SRCCOPY);
			}
			SelectObject(hdc2,old_object);
			DeleteObject(tmp_bmp);
			DeleteDC(hdc2);
		}
		/*else using direct blitting to screen*/

		ReleaseDC(NULL,hdc);
	}
	
	end:
		
	if (f->inputs[0]!=NULL)
		ms_queue_flush(f->inputs[0]);
	if (f->inputs[1]!=NULL)
		ms_queue_flush(f->inputs[1]);
}

static int get_native_window_id(MSFilter *f, void *data){
	DDDisplay *obj=(DDDisplay*)f->data;
	*(long*)data=(long)obj->window;
	return 0;
}

static int set_native_window_id(MSFilter *f, void *data){
	DDDisplay *obj=(DDDisplay*)f->data;
	obj->window=(HWND)(*(long*)data);
	obj->own_window=FALSE;
	return 0;
}

static int enable_autofit(MSFilter *f, void *data){
	DDDisplay *obj=(DDDisplay*)f->data;
	obj->autofit=*(int*)data;
	return 0;
}

static int enable_mirroring(MSFilter *f, void *data){
	DDDisplay *obj=(DDDisplay*)f->data;
	obj->mirroring=*(int*)data;
	return 0;
}

static int set_corner(MSFilter *f, void *data){
	DDDisplay *obj=(DDDisplay*)f->data;
	obj->sv_corner=*(int*)data;
	obj->need_repaint=TRUE;
	return 0;
}

static int get_vsize(MSFilter *f, void *data){
	DDDisplay *obj=(DDDisplay*)f->data;
	*(MSVideoSize*)data=obj->wsize;
	return 0;
}

static int set_vsize(MSFilter *f, void *data){
	DDDisplay *obj=(DDDisplay*)f->data;
	obj->wsize=*(MSVideoSize*)data;
	return 0;
}

static int set_scalefactor(MSFilter *f,void *arg){
	DDDisplay *obj=(DDDisplay*)f->data;
	ms_filter_lock(f);
	obj->sv_scalefactor = *(float*)arg;
	if (obj->sv_scalefactor<0.5f)
		obj->sv_scalefactor = 0.5f;
	ms_filter_unlock(f);
	return 0;
}

#if 0
static int set_selfview_pos(MSFilter *f,void *arg){
	DDDisplay *s=(DDDisplay*)f->data;
	s->sv_posx=((float*)arg)[0];
	s->sv_posy=((float*)arg)[1];
	s->sv_scalefactor=(float)100.0/((float*)arg)[2];
	return 0;
}

static int get_selfview_pos(MSFilter *f,void *arg){
	DDDisplay *s=(DDDisplay*)f->data;
	((float*)arg)[0]=s->sv_posx;
	((float*)arg)[1]=s->sv_posy;
	((float*)arg)[2]=(float)100.0/s->sv_scalefactor;
	return 0;
}
#endif

static int set_background_color(MSFilter *f,void *arg){
	DDDisplay *s=(DDDisplay*)f->data;
	s->background_color[0]=((int*)arg)[0];
	s->background_color[1]=((int*)arg)[1];
	s->background_color[2]=((int*)arg)[2];
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_GET_VIDEO_SIZE			, get_vsize	},
	{	MS_FILTER_SET_VIDEO_SIZE			, set_vsize	},
	{	MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, get_native_window_id },
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, set_native_window_id },
	{	MS_VIDEO_DISPLAY_ENABLE_AUTOFIT		,	enable_autofit	},
	{	MS_VIDEO_DISPLAY_ENABLE_MIRRORING	,	enable_mirroring},
	{	MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE	, set_corner },
	{	MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_SCALEFACTOR	, set_scalefactor },
	{	MS_VIDEO_DISPLAY_SET_BACKGROUND_COLOR    ,  set_background_color},
	{	0	,NULL}
};

#ifdef _MSC_VER

MSFilterDesc ms_dd_display_desc={
	MS_DRAWDIB_DISPLAY_ID,
	"MSDrawDibDisplay",
	N_("A video display based on windows DrawDib api"),
	MS_FILTER_OTHER,
	NULL,
	2,
	0,
	dd_display_init,
	dd_display_preprocess,
	dd_display_process,
	NULL,
	dd_display_uninit,
	methods
};

#else

MSFilterDesc ms_dd_display_desc={
	.id=MS_DRAWDIB_DISPLAY_ID,
	.name="MSDrawDibDisplay",
	.text=N_("A video display based on windows DrawDib api"),
	.category=MS_FILTER_OTHER,
	.ninputs=2,
	.noutputs=0,
	.init=dd_display_init,
	.preprocess=dd_display_preprocess,
	.process=dd_display_process,
	.uninit=dd_display_uninit,
	.methods=methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_dd_display_desc)
