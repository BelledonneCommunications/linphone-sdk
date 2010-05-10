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

/*required for dllexport of win_display_desc */
#define INVIDEOUT_C 1
#include "mediastreamer2/msvideoout.h"

#include "ffmpeg-priv.h"

#define SCALE_FACTOR 4.0f
#define SELVIEW_POS_INACTIVE -100.0
#include <Vfw.h>

typedef struct Yuv2RgbCtx{
	uint8_t *rgb;
	size_t rgblen;
	MSVideoSize dsize;
	struct ms_SwsContext *sws;
}Yuv2RgbCtx;

static void yuv2rgb_init(Yuv2RgbCtx *ctx){
	ctx->rgb=NULL;
	ctx->rgblen=0;
	ctx->dsize.width=0;
	ctx->dsize.height=0;
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
}

static void yuv2rgb_prepare(Yuv2RgbCtx *ctx, MSVideoSize src, MSVideoSize dst){
	if (ctx->sws!=NULL) yuv2rgb_uninit(ctx);
	ctx->sws=ms_sws_getContext(src.width,src.height,PIX_FMT_YUV420P,
			dst.width,dst.height, PIX_FMT_BGR24,
			SWS_FAST_BILINEAR, NULL, NULL, NULL);
	ctx->dsize=dst;
	ctx->rgblen=dst.width*dst.height*3;
	ctx->rgb=ms_malloc0(ctx->rgblen);
}


static void yuv2rgb_process(Yuv2RgbCtx *ctx, MSPicture *src, MSVideoSize dstsize){
	if (!ms_video_size_equal(dstsize,ctx->dsize)){
		MSVideoSize srcsize;
		srcsize.width=src->w;
		srcsize.height=src->h;
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
		DrawDibDraw(ddh,hdc,dstx,dsty,ctx->dsize.width,ctx->dsize.height,&bi,ctx->rgb,
			0,0,ctx->dsize.width,ctx->dsize.height,0);
	}
}

typedef struct _DDDisplay{
	HWND window;
	HDRAWDIB ddh;
	MSVideoSize defsize;
	Yuv2RgbCtx mainview;
	Yuv2RgbCtx locview;
}DDDisplay;

static LRESULT CALLBACK window_proc(
    HWND hwnd,        // handle to window
    UINT uMsg,        // message identifier
    WPARAM wParam,    // first message parameter
    LPARAM lParam)    // second message parameter
{
	switch(uMsg){
		case WM_DESTROY:
		break;
		case WM_SIZE:
			if (wParam==SIZE_RESTORED){
				int h=(lParam>>16) & 0xffff;
				int w=lParam & 0xffff;
				DDDisplay *wd;
				ms_message("Resized to %i,%i",w,h);
				wd=(DDDisplay*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
				if (wd!=NULL){
					//wd->window_size.width=w;
					//wd->window_size.height=h;
				}else{
					ms_error("Could not retrieve DDDisplay from window !");
				}
			}
		break;
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
	obj->defsize.width=MS_VIDEO_SIZE_CIF_W;
	obj->defsize.height=MS_VIDEO_SIZE_CIF_H;
	yuv2rgb_init(&obj->mainview);
	yuv2rgb_init(&obj->locview);
	f->data=obj;
}


static void dd_display_uninit(MSFilter *f){
	DDDisplay *obj=(DDDisplay*)f->data;
	yuv2rgb_uninit(&obj->mainview);
	yuv2rgb_uninit(&obj->locview);
	ms_free(obj);
}

static void dd_display_prepare(MSFilter *f){
	DDDisplay *dd=(DDDisplay*)f->data;
	if (dd->window==NULL){
		dd->window=create_window(dd->defsize.width,dd->defsize.height);
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
	}
}

static void dd_display_preprocess(MSFilter *f){
	dd_display_prepare(f);
}

static void dd_display_process(MSFilter *f){
	DDDisplay *obj=(DDDisplay*)f->data;
	mblk_t *inm;
	RECT rect;
	MSVideoSize wsize;
	MSVideoSize vsize;
	MSVideoSize psize; /*preview size*/
	HDC hdc;
	MSRect localrect;
	bool_t update_local=FALSE;
	bool_t update_main=FALSE;

	GetClientRect(obj->window,&rect);
	wsize.width=rect.right;
	wsize.height=rect.bottom;
	vsize.width=rect.right;
	vsize.height=rect.bottom;
	psize.width=vsize.width*SCALE_FACTOR;
	psize.height=vsize.height*SCALE_FACTOR;

	localrect.x=wsize.width-(wsize.width*SCALE_FACTOR);
	localrect.y=wsize.height-(wsize.height*SCALE_FACTOR);

	/*get most recent message and draw it*/
	if (f->inputs[1]!=NULL && (inm=ms_queue_peek_last(f->inputs[1]))!=0) {
		MSPicture src;
		if (yuv_buf_init_from_mblk(&src,inm)==0){	
			yuv2rgb_process(&obj->locview,&src,psize);
			update_local=TRUE;
		}
		ms_queue_flush(f->inputs[1]);
	}
	
	if (f->inputs[0]!=NULL && (inm=ms_queue_peek_last(f->inputs[0]))!=0) {
		MSPicture src;
		if (yuv_buf_init_from_mblk(&src,inm)==0){
			yuv2rgb_process(&obj->mainview,&src,vsize);
			update_main=TRUE;
			update_local=TRUE;
		}
		ms_queue_flush(f->inputs[0]);
		ms_message("Got new image of size %ix%i to display on a window of size %ix%i",
			src.w,src.h,vsize.width,vsize.height);
	}
	hdc=GetDC(obj->window);
	if (hdc==NULL) {
		ms_error("Could not get window dc");
		return;
	}
	if (update_main){
		yuv2rgb_draw(&obj->mainview,obj->ddh,hdc,0,0);
	}
	if (update_local){
		yuv2rgb_draw(&obj->locview,obj->ddh,hdc,localrect.x,localrect.y);
	}
	ReleaseDC(NULL,hdc);
}
static MSFilterMethod methods[]={
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
