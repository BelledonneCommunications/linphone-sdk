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

#define SCALE_FACTOR 0.16f
#define SELVIEW_POS_INACTIVE -100.0
#include <Vfw.h>

typedef struct Yuv2RgbCtx{
	uint8_t *rgb;
	size_t rgblen;
	MSVideoSize dsize;
	MSVideoSize ssize;
	struct ms_SwsContext *sws;
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
		ms_sws_freeContext(ctx->sws);
		ctx->sws=NULL;
	}
	ctx->dsize.width=0;
	ctx->dsize.height=0;
	ctx->ssize.width=0;
	ctx->ssize.height=0;
}

static void yuv2rgb_prepare(Yuv2RgbCtx *ctx, MSVideoSize src, MSVideoSize dst){
	if (ctx->sws!=NULL) yuv2rgb_uninit(ctx);
	ctx->sws=ms_sws_getContext(src.width,src.height,PIX_FMT_YUV420P,
			dst.width,dst.height, PIX_FMT_BGR24,
			SWS_FAST_BILINEAR, NULL, NULL, NULL);
	ctx->dsize=dst;
	ctx->ssize=src;
	ctx->rgblen=dst.width*dst.height*3;
	ctx->rgb=ms_malloc0(ctx->rgblen+dst.width);
}


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
		if (ms_sws_scale(ctx->sws,src->planes,src->strides, 0,
           				src->h, &p, &rgb_stride)<0){
			ms_error("Error in 420->rgb ms_sws_scale().");
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
	MSVideoSize vsize;
	MSVideoSize lsize;
	Yuv2RgbCtx mainview;
	Yuv2RgbCtx locview;
	bool_t need_repaint;
	bool_t autofit;
	bool_t mirroring;
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
	obj->vsize.width=MS_VIDEO_SIZE_CIF_W;
	obj->vsize.height=MS_VIDEO_SIZE_CIF_H;
	obj->lsize.width=MS_VIDEO_SIZE_CIF_W;
	obj->lsize.height=MS_VIDEO_SIZE_CIF_H;
	yuv2rgb_init(&obj->mainview);
	yuv2rgb_init(&obj->locview);
	obj->need_repaint=FALSE;
	obj->autofit=TRUE;
	obj->mirroring=FALSE;
	f->data=obj;
}

static void dd_display_prepare(MSFilter *f){
	DDDisplay *dd=(DDDisplay*)f->data;
	if (dd->window==NULL){
		dd->window=create_window(dd->vsize.width,dd->vsize.height);
		SetWindowLong(dd->window,GWL_USERDATA,(long)dd);
		dd->ddh=DrawDibOpen();
	}
}

static void dd_display_unprepare(MSFilter *f){
	DDDisplay *dd=(DDDisplay*)f->data;
	if (dd->window!=NULL){
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


/* compute the ideal placement of the video within a window of size wsize,
given that the original video has size vsize. Put the result in rect*/
static void center_with_ratio(MSVideoSize wsize, MSVideoSize vsize, MSRect *rect){
	int w,h;
	w=wsize.width & ~0x3;
	h=((w*vsize.height)/vsize.width) & ~0x1;
	if (h>wsize.height){
		/*the height doesn't fit, so compute the width*/
		h=wsize.height & ~0x1;
		w=((h*vsize.width)/vsize.height) & ~0x3;
	}
	rect->x=(wsize.width-w)/2;
	rect->y=(wsize.height-h)/2;
	rect->w=w;
	rect->h=h;
}

static void compute_layout(MSVideoSize wsize, MSVideoSize vsize, MSVideoSize orig_psize, MSRect *mainrect, MSRect *localrect){
	MSVideoSize psize;

	center_with_ratio(wsize,vsize,mainrect);
	psize.width=wsize.width*SCALE_FACTOR;
	psize.height=wsize.height*SCALE_FACTOR;
	center_with_ratio(psize,orig_psize,localrect);
	localrect->x=wsize.width-localrect->w-2;
	localrect->y=wsize.height-localrect->h-2;
/*
	ms_message("Compute layout result for\nwindow size=%ix%i\nvideo orig size=%ix%i\nlocal size=%ix%i\nlocal orig size=%ix%i\n"
		"mainrect=%i,%i,%i,%i\tlocalrect=%i,%i,%i,%i",
		wsize.width,wsize.height,vsize.width,vsize.height,psize.width,psize.height,orig_psize.width,orig_psize.height,
		mainrect->x,mainrect->y,mainrect->w,mainrect->h,
		localrect->x,localrect->y,localrect->w,localrect->h);
*/
}

static void draw_local_view_frame(HDC hdc, MSVideoSize wsize, MSRect localrect){
	HGDIOBJ old_object = SelectObject(hdc, GetStockObject(WHITE_BRUSH)); 
	Rectangle(hdc, localrect.x-2, localrect.y-2, localrect.x+localrect.w+2, localrect.y+localrect.h+2);
	SelectObject(hdc,old_object);
}

static void draw_background(HDC hdc, MSVideoSize wsize, MSRect mainrect){
	HBRUSH brush;
	RECT brect;

	brush = CreateSolidBrush(RGB(0,0,0));
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
	MSVideoSize wsize;
	MSVideoSize vsize;
	MSVideoSize lsize; /*local preview size*/
	HDC hdc;
	MSRect mainrect;
	MSRect localrect;
	MSPicture mainpic;
	MSPicture localpic;
	mblk_t *main_im=NULL;
	mblk_t *local_im=NULL;

	GetClientRect(obj->window,&rect);
	wsize.width=rect.right;
	wsize.height=rect.bottom;
	/*get most recent message and draw it*/
	if (f->inputs[1]!=NULL && (local_im=ms_queue_peek_last(f->inputs[1]))!=NULL) {
		if (yuv_buf_init_from_mblk(&localpic,local_im)==0){
			obj->lsize.width=localpic.w;
			obj->lsize.height=localpic.h;
		}
	}
	
	if (f->inputs[0]!=NULL && (main_im=ms_queue_peek_last(f->inputs[0]))!=NULL) {
		if (yuv_buf_init_from_mblk(&mainpic,main_im)==0){
			if (obj->autofit && (obj->vsize.width!=mainpic.w || obj->vsize.height!=mainpic.h)
				&& (mainpic.w>wsize.width || mainpic.h>wsize.height)){
				RECT cur;
				ms_message("Detected video resolution changed, resizing window");
				GetWindowRect(obj->window,&cur);
				wsize.width=mainpic.w;
				wsize.height=mainpic.h;
				MoveWindow(obj->window,cur.left, cur.top, wsize.width, wsize.height,TRUE);
			}
			obj->vsize.width=mainpic.w;
			obj->vsize.height=mainpic.h;
		}
	}
	compute_layout(wsize,obj->vsize,obj->lsize,&mainrect,&localrect);
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

	if (obj->need_repaint){
		draw_background(hdc,wsize,mainrect);
		obj->need_repaint=FALSE;
	}

	if (main_im!=NULL){
		yuv2rgb_draw(&obj->mainview,obj->ddh,hdc,mainrect.x,mainrect.y);
	}
	
	if (local_im!=NULL || main_im!=NULL){
		if (obj->locview.rgb!=NULL){
			draw_local_view_frame(hdc,wsize,localrect);
			yuv2rgb_draw(&obj->locview,obj->ddh,hdc,localrect.x,localrect.y);
		}
	}

	ReleaseDC(NULL,hdc);
	if (main_im!=NULL)
		ms_queue_flush(f->inputs[0]);
	if (local_im!=NULL)
		ms_queue_flush(f->inputs[1]);
}

static int get_native_window_id(MSFilter *f, void *data){
	DDDisplay *obj=(DDDisplay*)f->data;
	*(long*)data=(long)obj->window;
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

static MSFilterMethod methods[]={
	{	MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, get_native_window_id },
	{	MS_VIDEO_DISPLAY_ENABLE_AUTOFIT		,	enable_autofit	},
	{	MS_VIDEO_DISPLAY_ENABLE_MIRRORING	,	enable_mirroring},
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
