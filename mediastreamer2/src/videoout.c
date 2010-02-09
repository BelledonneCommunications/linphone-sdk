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

static int video_out_set_vsize(MSFilter *f,void *arg);

bool_t ms_display_poll_event(MSDisplay *d, MSDisplayEvent *ev){
	if (d->desc->pollevent)
		return d->desc->pollevent(d,ev);
	else return FALSE;
}

#ifdef HAVE_SDL

#include <SDL/SDL.h>
#include <SDL/SDL_video.h>

typedef struct _SdlDisplay{
	MSFilter *filter;
	bool_t sdl_initialized;
	ms_mutex_t sdl_mutex;
	SDL_Surface *sdl_screen;
	SDL_Overlay *lay;

	float sv_scalefactor;
} SdlDisplay;

#ifdef HAVE_X11_XLIB_H

#include <SDL/SDL_syswm.h>

static long sdl_get_native_window_id(){
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if ( SDL_GetWMInfo(&info) ) {
		if ( info.subsystem == SDL_SYSWM_X11 ) {
			return (long) info.info.x11.wmwindow;
		}
	}
	return 0;
}

static void sdl_show_window(bool_t show){
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if ( SDL_GetWMInfo(&info) ) {
		if ( info.subsystem == SDL_SYSWM_X11 ) {
			Display *display;
			Window window;
		
			info.info.x11.lock_func();
			display = info.info.x11.display;
			window = info.info.x11.wmwindow;
			if (show)
				XMapWindow(display,window);
			else
				XUnmapWindow(display,window);
			info.info.x11.unlock_func();
		}
	}
}

#else

static void sdl_show_window(bool_t show){
	ms_warning("SDL window show/hide not implemented");
}

static long sdl_get_native_window_id(){
	ms_warning("sdl_get_native_window_id not implemented");
	return 0;
}

#endif

static void sdl_display_uninit(MSDisplay *obj);

static int sdl_create_window(SdlDisplay *wd, int w, int h){
	static bool_t once=TRUE;
	
	wd->sdl_screen = SDL_SetVideoMode(w,h, 0,SDL_HWSURFACE|SDL_RESIZABLE);
	if (wd->sdl_screen == NULL ) {
		ms_warning("no hardware for video mode: %s\n",
						SDL_GetError());
	}
	if (wd->sdl_screen == NULL )
		wd->sdl_screen = SDL_SetVideoMode(w,h, 0,SDL_SWSURFACE|SDL_RESIZABLE);
	if (wd->sdl_screen == NULL ) {
		ms_warning("Couldn't set video mode: %s\n",
						SDL_GetError());
		return -1;
	}
	if (wd->sdl_screen->flags & SDL_HWSURFACE) ms_message("SDL surface created in hardware");
	if (once) {
		SDL_WM_SetCaption("Video window", NULL);
		once=FALSE;
	}
	ms_message("Using yuv overlay.");
	wd->lay=SDL_CreateYUVOverlay(w , h ,SDL_YV12_OVERLAY,wd->sdl_screen);
	if (wd->lay==NULL){
		ms_warning("Couldn't create yuv overlay: %s\n",
						SDL_GetError());
		return -1;
	}else{
		ms_message("%i x %i YUV overlay created: hw_accel=%i, pitches=%i,%i,%i",wd->lay->w,wd->lay->h,wd->lay->hw_overlay,
			wd->lay->pitches[0],wd->lay->pitches[1],wd->lay->pitches[2]);
		ms_message("planes= %p %p %p  %i %i",wd->lay->pixels[0],wd->lay->pixels[1],wd->lay->pixels[2],
			wd->lay->pixels[1]-wd->lay->pixels[0],wd->lay->pixels[2]-wd->lay->pixels[1]);
	}
	return 0;
}

static bool_t sdl_display_init(MSDisplay *obj, MSFilter *f, MSPicture *fbuf, MSPicture *fbuf_selfview){
	SdlDisplay *wd = (SdlDisplay*)obj->data;
	int i;
	if (wd==NULL){
		/* Initialize the SDL library */
		wd=(SdlDisplay*)ms_new0(SdlDisplay,1);
		wd->filter = f;
		obj->data=wd;
		
		if( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
			ms_error("Couldn't initialize SDL: %s", SDL_GetError());
			return FALSE;
		}
		/* Clean up on exit */
		atexit(SDL_Quit);
		wd->sdl_initialized=TRUE;
		ms_mutex_init(&wd->sdl_mutex,NULL);
		ms_mutex_lock(&wd->sdl_mutex);
	}else {
		ms_mutex_lock(&wd->sdl_mutex);

		if (wd->lay!=NULL)
			SDL_FreeYUVOverlay(wd->lay);
		if (wd->sdl_screen!=NULL)
			SDL_FreeSurface(wd->sdl_screen);
		wd->lay=NULL;
		wd->sdl_screen=NULL;
	}
	wd->filter = f;
	
	i=sdl_create_window(wd, fbuf->w, fbuf->h);
	if (i==0){
		fbuf->planes[0]=wd->lay->pixels[0];
		fbuf->planes[1]=wd->lay->pixels[2];
		fbuf->planes[2]=wd->lay->pixels[1];
		fbuf->planes[3]=NULL;
		fbuf->strides[0]=wd->lay->pitches[0];
		fbuf->strides[1]=wd->lay->pitches[2];
		fbuf->strides[2]=wd->lay->pitches[1];
		fbuf->strides[3]=0;
		fbuf->w=wd->lay->w;
		fbuf->h=wd->lay->h;
		sdl_show_window(TRUE);
		obj->window_id=sdl_get_native_window_id();
		ms_mutex_unlock(&wd->sdl_mutex);
		return TRUE;
	}
	ms_mutex_unlock(&wd->sdl_mutex);
	return FALSE;
}

static void sdl_display_lock(MSDisplay *obj){
	SdlDisplay *wd = (SdlDisplay*)obj->data;
	ms_mutex_lock(&wd->sdl_mutex);
	SDL_LockYUVOverlay(wd->lay);
	ms_mutex_unlock(&wd->sdl_mutex);
}

static void sdl_display_unlock(MSDisplay *obj){
	SdlDisplay *wd = (SdlDisplay*)obj->data;
	ms_mutex_lock(&wd->sdl_mutex);
	SDL_UnlockYUVOverlay(wd->lay);
	ms_mutex_unlock(&wd->sdl_mutex);
}

static void sdl_display_update(MSDisplay *obj){
	SdlDisplay *wd = (SdlDisplay*)obj->data;
	SDL_Rect rect;
	rect.x=0;
	rect.y=0;
	ms_mutex_lock(&wd->sdl_mutex);
	rect.w=wd->lay->w;
	rect.h=wd->lay->h;
	SDL_DisplayYUVOverlay(wd->lay,&rect);
	ms_mutex_unlock(&wd->sdl_mutex);
}

static bool_t sdl_poll_event(MSDisplay *obj, MSDisplayEvent *ev){
	SdlDisplay *wd = (SdlDisplay*)obj->data;
	SDL_Event event;
	bool_t ret=FALSE;
	if (wd->sdl_screen==NULL) return FALSE;
	ms_mutex_lock(&wd->sdl_mutex);
	if (SDL_PollEvent(&event)){
		ms_mutex_unlock(&wd->sdl_mutex);
		switch(event.type){
			case SDL_VIDEORESIZE:
				ev->evtype=MS_DISPLAY_RESIZE_EVENT;
				ev->w=event.resize.w;
				ev->h=event.resize.h;
				return TRUE;
			break;
			default:
			break;
		}
	}else ms_mutex_unlock(&wd->sdl_mutex);
	return ret;
}

static void sdl_display_uninit(MSDisplay *obj){
	SdlDisplay *wd = (SdlDisplay*)obj->data;
	SDL_Event event;
	int i;
	if (wd==NULL)
		return;
	if (wd->lay!=NULL)
		SDL_FreeYUVOverlay(wd->lay);
	if (wd->sdl_screen!=NULL){
		SDL_FreeSurface(wd->sdl_screen);
		wd->sdl_screen=NULL;
	}
	wd->lay=NULL;
	wd->sdl_screen=NULL;
#ifdef __linux
	/*purge the event queue before leaving*/
	for(i=0;SDL_PollEvent(&event) && i<100;++i){
	}
#endif
	sdl_show_window(FALSE);
}

MSDisplayDesc ms_sdl_display_desc={
	.init=sdl_display_init,
	.lock=sdl_display_lock,
	.unlock=sdl_display_unlock,
	.update=sdl_display_update,
	.uninit=sdl_display_uninit,
	.pollevent=sdl_poll_event,
};

#elif defined(WIN32)

#include <Vfw.h>

typedef struct _WinDisplay{
	MSFilter *filter;
	HWND window;
	HDRAWDIB ddh;
	MSPicture fb;
	MSPicture fb_selfview;
	uint8_t *rgb_selfview;
	int rgb_len_selfview;
	struct SwsContext *sws_selfview;
	MSDisplayEvent last_rsz;
	uint8_t *rgb;
	uint8_t *black;
	int last_rect_w;
	int last_rect_h;
	int rgb_len;
	struct SwsContext *sws;
	bool_t new_ev;
}WinDisplay;

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
				MSDisplay *obj;
				WinDisplay *wd;
				ms_message("Resized to %i,%i",w,h);
				obj=(MSDisplay*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
				if (obj!=NULL){
					wd=(WinDisplay*)obj->data;
					wd->last_rsz.evtype=MS_DISPLAY_RESIZE_EVENT;
					wd->last_rsz.w=w;
					wd->last_rsz.h=h;
					wd->new_ev=TRUE;
				}else{
					ms_error("Could not retrieve MSDisplay from window !");
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

static bool_t win_display_init(MSDisplay *obj, MSFilter *f, MSPicture *fbuf, MSPicture *fbuf_selfview){
	WinDisplay *wd=(WinDisplay*)obj->data;
	int ysize,usize;

	if (wd!=NULL)
	{
		wd->filter = NULL;
		if (wd->ddh) DrawDibClose(wd->ddh);
		wd->ddh=NULL;
		if (wd->fb.planes[0]) ms_free(wd->fb.planes[0]);
		wd->fb.planes[0]=NULL;
		wd->fb.planes[1]=NULL;
		wd->fb.planes[2]=NULL;
		wd->fb.planes[3]=NULL;
		if (wd->rgb) ms_free(wd->rgb);
		if (wd->black) ms_free(wd->black);
		wd->rgb=NULL;
		wd->rgb_len=0;
		sws_freeContext(wd->sws);
		wd->sws=NULL;
		if (wd->fb_selfview.planes[0]) ms_free(wd->fb_selfview.planes[0]);
		wd->fb_selfview.planes[0]=NULL;
		wd->fb_selfview.planes[1]=NULL;
		wd->fb_selfview.planes[2]=NULL;
		wd->fb_selfview.planes[3]=NULL;
		if (wd->rgb_selfview) ms_free(wd->rgb_selfview);
		wd->rgb_selfview=NULL;
		wd->rgb_len_selfview=0;
		sws_freeContext(wd->sws_selfview);
		wd->sws_selfview=NULL;
		wd->black=NULL;
		wd->last_rect_w=0;
		wd->last_rect_h=0;
	}
	else
		wd=(WinDisplay*)ms_new0(WinDisplay,1);
	
	wd->filter = f;
	obj->data=wd;
	
	wd->fb.w=fbuf->w;
	wd->fb.h=fbuf->h;
	wd->fb_selfview.w=fbuf_selfview->w;
	wd->fb_selfview.h=fbuf_selfview->h;

	if (wd->window==NULL){
		if (obj->use_external_window && obj->window_id!=0){
			void *p;
			wd->window=(HWND)obj->window_id;
			p=(void*)GetWindowLongPtr(wd->window,GWLP_USERDATA);
			if (p!=NULL){
				ms_error("Gulp: this externally supplied windows seems to "
					"already have a userdata ! resizing will crash !");
			}else SetWindowLongPtr(wd->window,GWLP_USERDATA,(LONG_PTR)obj);
		}else{
			wd->window=create_window(wd->fb.w,wd->fb.h);
			obj->window_id=(long)wd->window;
			if (wd->window!=NULL) SetWindowLongPtr(wd->window,GWLP_USERDATA,(LONG_PTR)obj);
			else return FALSE;
		}
	}else if (!obj->use_external_window){
		/* the window might need to be resized*/
		RECT cur;
		GetWindowRect(wd->window,&cur);
		MoveWindow(wd->window,cur.left, cur.top, wd->fb.w, wd->fb.h,TRUE);
	}
	
	if (wd->ddh==NULL) wd->ddh=DrawDibOpen();
	if (wd->ddh==NULL){
		ms_error("DrawDibOpen() failed.");
		return FALSE;
	}

	/*allocate yuv and rgb buffers*/
	if (wd->fb_selfview.planes[0]) ms_free(wd->fb_selfview.planes[0]);
	if (wd->rgb_selfview) ms_free(wd->rgb_selfview);
	ysize=wd->fb_selfview.w*wd->fb_selfview.h;
	usize=ysize/4;
	fbuf_selfview->planes[0]=wd->fb_selfview.planes[0]=(uint8_t*)ms_malloc0(ysize+2*usize);
	fbuf_selfview->planes[1]=wd->fb_selfview.planes[1]=wd->fb_selfview.planes[0]+ysize;
	fbuf_selfview->planes[2]=wd->fb_selfview.planes[2]=wd->fb_selfview.planes[1]+usize;
	fbuf_selfview->planes[3]=NULL;
	fbuf_selfview->strides[0]=wd->fb_selfview.strides[0]=wd->fb_selfview.w;
	fbuf_selfview->strides[1]=wd->fb_selfview.strides[1]=wd->fb_selfview.w/2;
	fbuf_selfview->strides[2]=wd->fb_selfview.strides[2]=wd->fb_selfview.w/2;
	fbuf_selfview->strides[3]=0;

	wd->rgb_len_selfview=ysize*3;
	wd->rgb_selfview=(uint8_t*)ms_malloc0(wd->rgb_len_selfview);

	if (wd->fb.planes[0]) ms_free(wd->fb.planes[0]);
	if (wd->rgb) ms_free(wd->rgb);
	if (wd->black) ms_free(wd->black);
	ysize=wd->fb.w*wd->fb.h;
	usize=ysize/4;
	fbuf->planes[0]=wd->fb.planes[0]=(uint8_t*)ms_malloc0(ysize+2*usize);
	fbuf->planes[1]=wd->fb.planes[1]=wd->fb.planes[0]+ysize;
	fbuf->planes[2]=wd->fb.planes[2]=wd->fb.planes[1]+usize;
	fbuf->planes[3]=NULL;
	fbuf->strides[0]=wd->fb.strides[0]=wd->fb.w;
	fbuf->strides[1]=wd->fb.strides[1]=wd->fb.w/2;
	fbuf->strides[2]=wd->fb.strides[2]=wd->fb.w/2;
	fbuf->strides[3]=0;

	wd->rgb_len=ysize*3;
	wd->rgb=(uint8_t*)ms_malloc0(wd->rgb_len);
	wd->black = (uint8_t*)ms_malloc0(wd->rgb_len);
	wd->last_rect_w=0;
	wd->last_rect_h=0;
	return TRUE;
}

typedef struct rgb{
	uint8_t r,g,b;
} rgb_t;

typedef struct yuv{
	uint8_t y,u,v;
} yuv_t;



static void yuv420p_to_rgb(WinDisplay *wd, MSPicture *src, uint8_t *rgb){
	int rgb_stride=-src->w*3;
	uint8_t *p;

	p=rgb+(src->w*3*(src->h-1));
	if (wd->sws==NULL){
		wd->sws=sws_getContext(src->w,src->h,PIX_FMT_YUV420P,
			src->w,src->h, PIX_FMT_BGR24,
			SWS_FAST_BILINEAR, NULL, NULL, NULL);
	}
	if (sws_scale(wd->sws,src->planes,src->strides, 0,
           			src->h, &p, &rgb_stride)<0){
		ms_error("Error in 420->rgb sws_scale().");
	}
}

static void yuv420p_to_rgb_selfview(WinDisplay *wd, MSPicture *src, uint8_t *rgb){
	int rgb_stride=-src->w*3;
	uint8_t *p;

	p=rgb+(src->w*3*(src->h-1));
	if (wd->sws_selfview==NULL){
		wd->sws_selfview=sws_getContext(src->w,src->h,PIX_FMT_YUV420P,
			src->w,src->h, PIX_FMT_BGR24,
			SWS_FAST_BILINEAR, NULL, NULL, NULL);
	}
	if (sws_scale(wd->sws_selfview,src->planes,src->strides, 0,
           			src->h, &p, &rgb_stride)<0){
		ms_error("Error in 420->rgb sws_scale().");
	}
}

static int gcd(int m, int n)
{
   if(n == 0)
     return m;
   else
     return gcd(n, m % n);
}
   
static void reduce(int *num, int *denom)
{
   int divisor = gcd(*num, *denom);
   *num /= divisor;
   *denom /= divisor;
}

static void win_display_update(MSDisplay *obj){
	WinDisplay *wd=(WinDisplay*)obj->data;
	HDC hdc;
	BITMAPINFOHEADER bi;
	RECT rect;
	bool_t ret;
	int ratiow;
	int ratioh;
	int w;
	int h;
	int corner;
	float sv_scalefactor;
	float sv_pos[3];

	HDC dd_hdc;
	HBITMAP dd_bmp;
	BOOL dont_draw;

	if (wd->window==NULL) return;
	hdc=GetDC(wd->window);
	if (hdc==NULL) {
		ms_error("Could not get window dc");
		return;
	}
	yuv420p_to_rgb(wd, &wd->fb, wd->rgb);
	memset(&bi,0,sizeof(bi));
	bi.biSize=sizeof(bi);
	GetClientRect(wd->window,&rect);

	bi.biWidth=wd->fb.w;
	bi.biHeight=wd->fb.h;
	bi.biPlanes=1;
	bi.biBitCount=24;
	bi.biCompression=BI_RGB;
	bi.biSizeImage=wd->rgb_len;

	ratiow=wd->fb.w;
	ratioh=wd->fb.h;
	reduce(&ratiow, &ratioh);
	w = rect.right/ratiow*ratiow;
	h = rect.bottom/ratioh*ratioh;

	if (h*ratiow>w*ratioh)
	{
		w = w;
		h = w*ratioh/ratiow;
	}
	else
	{
		h = h;
		w = h*ratiow/ratioh;
	}

	if (h*wd->fb.w!=w*wd->fb.h)
		ms_error("wrong ratio");

	dd_hdc = CreateCompatibleDC(hdc);
	if (dd_hdc==NULL) {
		ms_error("Could not get CreateCompatibleDC");
		return;
	}
	dd_bmp = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
	if (dd_bmp==NULL) {
		ms_error("Could not get CreateCompatibleBitmap");
		return;
	}

	HGDIOBJ old_object = SelectObject(dd_hdc, dd_bmp);

	dont_draw = DrawDibBegin(wd->ddh,dd_hdc, 0, 0, &bi, 0, 0, DDF_BUFFER);
	//full screen in black
	ret=DrawDibDraw(wd->ddh,dd_hdc,0,0,
		rect.right,rect.bottom,
		&bi,wd->black,
		0,0,bi.biWidth,bi.biHeight,dont_draw?DDF_DONTDRAW:0);

	corner = 0;
	sv_scalefactor = SCALE_FACTOR;
	sv_pos[0] = SELVIEW_POS_INACTIVE;
	sv_pos[1] = SELVIEW_POS_INACTIVE;
	sv_pos[2] = 0;
	if (wd->filter)
		ms_filter_call_method(wd->filter, MS_VIDEO_OUT_GET_CORNER, &corner);
	if (wd->filter)
		ms_filter_call_method(wd->filter, MS_VIDEO_OUT_GET_SCALE_FACTOR, &sv_scalefactor);
	if (wd->filter)
		ms_filter_call_method(wd->filter, MS_VIDEO_OUT_GET_SELFVIEW_POS, sv_pos);

	if (wd->rgb_selfview==NULL || corner==-1) {
		ret=DrawDibDraw(wd->ddh,dd_hdc,
			(rect.right-w)/2,
			(rect.bottom-h)/2,
			w,
			h,
			&bi,wd->rgb,
			0,0,bi.biWidth,bi.biHeight,dont_draw?DDF_DONTDRAW:0);
	
	} else {

		int w_selfview = rect.right - w;
		int h_selfview = rect.bottom - h;

		if ((h_selfview < h/sv_scalefactor && w_selfview < w/sv_scalefactor) || corner<=3 || sv_pos[0]!=SELVIEW_POS_INACTIVE)
		{
			ret=DrawDibDraw(wd->ddh,dd_hdc,
				(rect.right-w)/2,
				(rect.bottom-h)/2,
				w,
				h,
				&bi,wd->rgb,
				0,0,bi.biWidth,bi.biHeight,dont_draw?DDF_DONTDRAW:0);

			//preserve ratio
			ratiow=wd->fb_selfview.w;
			ratioh=wd->fb_selfview.h;
			reduce(&ratiow, &ratioh);

			w_selfview = (int)(w/sv_scalefactor);
			w_selfview = w_selfview/ratiow*ratiow;
			h_selfview = w_selfview*ratioh/ratiow;

			if (rect.right>100 && rect.bottom>100)
			{
				int x_sv;
				int y_sv;
				yuv420p_to_rgb_selfview(wd, &wd->fb_selfview, wd->rgb_selfview);

				//HPEN hpenDot;
				//hpenDot = CreatePen(PS_SOLID, 1, RGB(10, 10, 10));
				//SelectObject(dd_hdc, hpenDot);
				if (sv_pos[0]!=SELVIEW_POS_INACTIVE)
				{
					x_sv = (int)((rect.right*sv_pos[0])/100.0-w_selfview/2);
					y_sv = (int)((rect.bottom*sv_pos[1])/100.0-h_selfview/2);
				}
				else if (corner==1 || corner==4+1)
				{
					/* top left corner */
					x_sv = 20;
					y_sv = 20;
				}
				else if (corner==2 || corner==4+2)
				{
					/* top right corner */
					x_sv = (rect.right-w_selfview-20);
					y_sv = 20;
				}
				else if (corner==3 || corner==4+3)
				{
					/* bottom left corner */
					x_sv = 20;
					y_sv = (rect.bottom-h_selfview-20);
				}
				else /* corner = 0: default */
				{
					/* bottom right corner */
					x_sv = (rect.right-w_selfview-20);
					y_sv = (rect.bottom-h_selfview-20);
				}


				Rectangle(dd_hdc, x_sv-2, y_sv-2, x_sv+w_selfview+2, y_sv+h_selfview+2); 
				ret=DrawDibDraw(wd->ddh,dd_hdc,
					x_sv,
					y_sv,
					w_selfview,
					h_selfview,
					&bi,wd->rgb_selfview,
					0,0,bi.biWidth,bi.biHeight,dont_draw?DDF_DONTDRAW:0);
			}
		}
		else
		{
			//preserve ratio
			ratiow=wd->fb_selfview.w;
			ratioh=wd->fb_selfview.h;
			reduce(&ratiow, &ratioh);

			yuv420p_to_rgb_selfview(wd, &wd->fb_selfview, wd->rgb_selfview);
			if (w_selfview >= w/sv_scalefactor)
			{
				w_selfview = w_selfview/ratiow*ratiow;
				h_selfview = w_selfview*ratioh/ratiow;

				ret=DrawDibDraw(wd->ddh,dd_hdc,
					0,
					(rect.bottom-h)/2,
					w,
					h,
					&bi,wd->rgb,
					0,0,bi.biWidth,bi.biHeight,dont_draw?DDF_DONTDRAW:0);

				Rectangle(dd_hdc,
					(rect.right-w_selfview)-4-2,
					(rect.bottom-h_selfview)/2-2,
					(rect.right)-4+2,
					(rect.bottom+h_selfview)/2+2);

				ret=DrawDibDraw(wd->ddh,dd_hdc,
					(rect.right-w_selfview)-4,
					(rect.bottom-h_selfview)/2,
					w_selfview,
					h_selfview,
					&bi,wd->rgb_selfview,
					0,0,bi.biWidth,bi.biHeight,dont_draw?DDF_DONTDRAW:0);
			}
			else
			{
				h_selfview = h_selfview/ratioh*ratioh;
				w_selfview = h_selfview*ratiow/ratioh;

				ret=DrawDibDraw(wd->ddh,dd_hdc,
					(rect.right-w)/2,
					0,
					w,
					h,
					&bi,wd->rgb,
					0,0,bi.biWidth,bi.biHeight,dont_draw?DDF_DONTDRAW:0);

				Rectangle(dd_hdc,
					(rect.right-w_selfview)/2-2,
					(rect.bottom-h_selfview)-4-2,
					(rect.right+w_selfview)/2+2,
					(rect.bottom)-4+2);

				ret=DrawDibDraw(wd->ddh,dd_hdc,
					(rect.right-w_selfview)/2,
					(rect.bottom-h_selfview)-4,
					w_selfview,
					h_selfview,
					&bi,wd->rgb_selfview,
					0,0,bi.biWidth,bi.biHeight,dont_draw?DDF_DONTDRAW:0);
			}
		}
	}

	DrawDibEnd(wd->ddh);
	BitBlt(hdc, 0, 0, rect.right, rect.bottom, dd_hdc, 0, 0, SRCCOPY);
	SelectObject(dd_hdc, old_object);

	DeleteObject(dd_bmp);
	DeleteDC(dd_hdc);

	wd->last_rect_w=rect.right;
	wd->last_rect_h=rect.bottom;

  	if (!ret) ms_error("DrawDibDraw failed.");
	ReleaseDC(NULL,hdc);
}

static void win_display_uninit(MSDisplay *obj){
	WinDisplay *wd=(WinDisplay*)obj->data;
	if (wd==NULL)
		return;
	if (wd->window && !obj->use_external_window) DestroyWindow(wd->window);
	if (wd->ddh) DrawDibClose(wd->ddh);
	if (wd->fb_selfview.planes[0]) ms_free(wd->fb_selfview.planes[0]);
	if (wd->rgb_selfview) ms_free(wd->rgb_selfview);
	if (wd->sws_selfview) sws_freeContext(wd->sws_selfview);
	if (wd->fb.planes[0]) ms_free(wd->fb.planes[0]);
	if (wd->rgb) ms_free(wd->rgb);
	if (wd->black) ms_free(wd->black);
	if (wd->sws) sws_freeContext(wd->sws);
	ms_free(wd);
}

bool_t win_display_pollevent(MSDisplay *d, MSDisplayEvent *ev){
	return FALSE;
}

#ifdef _MSC_VER

extern MSDisplayDesc ms_win_display_desc={
	win_display_init,
	NULL,
	NULL,
	win_display_update,
	win_display_uninit,
	win_display_pollevent
};

#else

MSDisplayDesc ms_win_display_desc={
	.init=win_display_init,
	.update=win_display_update,
	.uninit=win_display_uninit,
	.pollevent=win_display_pollevent
};

#endif

#endif

MSDisplay *ms_display_new(MSDisplayDesc *desc){
	MSDisplay *obj=(MSDisplay *)ms_new0(MSDisplay,1);
	obj->desc=desc;
	obj->data=NULL;
	return obj;
}

void ms_display_set_window_id(MSDisplay *d, long id){
	d->window_id=id;
	d->use_external_window=TRUE;
}

void ms_display_destroy(MSDisplay *obj){
	obj->desc->uninit(obj);
	ms_free(obj);
}

#ifdef HAVE_SDL
static MSDisplayDesc *default_display_desc=&ms_sdl_display_desc;
#elif defined(WIN32)
static MSDisplayDesc *default_display_desc=&ms_win_display_desc;
#else
static MSDisplayDesc *default_display_desc=NULL;
#endif

void ms_display_desc_set_default(MSDisplayDesc *desc){
	default_display_desc=desc;
}

MSDisplayDesc * ms_display_desc_get_default(void){
	return default_display_desc;
}

void ms_display_desc_set_default_window_id(MSDisplayDesc *desc, long id){
	desc->default_window_id=id;
}

typedef struct VideoOut
{
	AVRational ratio;
	MSPicture fbuf;
	MSPicture fbuf_selfview;
	MSPicture local_pic;
	MSRect local_rect;
	mblk_t *local_msg;
	MSVideoSize prevsize;
	int corner; /*for selfview*/
	float scale_factor; /*for selfview*/
	float sv_posx,sv_posy;

	struct SwsContext *sws1;
	struct SwsContext *sws2;
	MSDisplay *display;
	bool_t own_display;
	bool_t ready;
	bool_t autofit;
	bool_t mirror;
} VideoOut;

static void set_corner(VideoOut *s, int corner)
{
	s->corner=corner;
	s->local_pic.w=((int)(s->fbuf.w/s->scale_factor)) & ~0x1;
	s->local_pic.h=((int)(s->fbuf.h/s->scale_factor)) & ~0x1;
	s->local_rect.w=s->local_pic.w;
	s->local_rect.h=s->local_pic.h;
	if (corner==1) {
		/* top left corner */
		s->local_rect.x=0;
		s->local_rect.y=0;
	} else if (corner==2) {
		/* top right corner */
		s->local_rect.x=s->fbuf.w-s->local_pic.w;
		s->local_rect.y=0;
	} else if (corner==3) {
		/* bottom left corner */
		s->local_rect.x=0;
		s->local_rect.y=s->fbuf.h-s->local_pic.h;
	} else {
		/* default: bottom right corner */
		/* corner can be set to -1: to disable the self view... */
		s->local_rect.x=s->fbuf.w-s->local_pic.w;
		s->local_rect.y=s->fbuf.h-s->local_pic.h;
	}
	s->fbuf_selfview.w=(s->fbuf.w/1) & ~0x1;
	s->fbuf_selfview.h=(s->fbuf.h/1) & ~0x1;
}

static void set_vsize(VideoOut *s, MSVideoSize *sz){
	s->fbuf.w=sz->width & ~0x1;
	s->fbuf.h=sz->height & ~0x1;
	set_corner(s,s->corner);
	ms_message("Video size set to %ix%i",s->fbuf.w,s->fbuf.h);
}

static void video_out_init(MSFilter  *f){
	VideoOut *obj=(VideoOut*)ms_new0(VideoOut,1);
	MSVideoSize def_size;
	obj->ratio.num=11;
	obj->ratio.den=9;
	def_size.width=MS_VIDEO_SIZE_CIF_W;
	def_size.height=MS_VIDEO_SIZE_CIF_H;
	obj->prevsize.width=0;
	obj->prevsize.height=0;
	obj->local_msg=NULL;
	obj->corner=0;
	obj->scale_factor=SCALE_FACTOR;
	obj->sv_posx=obj->sv_posy=SELVIEW_POS_INACTIVE;
	obj->sws1=NULL;
	obj->sws2=NULL;
	obj->display=NULL;
	obj->own_display=FALSE;
	obj->ready=FALSE;
	obj->autofit=FALSE;
	obj->mirror=FALSE;
	set_vsize(obj,&def_size);
	f->data=obj;
}


static void video_out_uninit(MSFilter *f){
	VideoOut *obj=(VideoOut*)f->data;
	if (obj->display!=NULL && obj->own_display)
		ms_display_destroy(obj->display);
	if (obj->sws1!=NULL){
		sws_freeContext(obj->sws1);
		obj->sws1=NULL;
	}
	if (obj->sws2!=NULL){
		sws_freeContext(obj->sws2);
		obj->sws2=NULL;
	}
	if (obj->local_msg!=NULL) {
		freemsg(obj->local_msg);
		obj->local_msg=NULL;
	}
	ms_free(obj);
}

static void video_out_prepare(MSFilter *f){
	VideoOut *obj=(VideoOut*)f->data;
	if (obj->display==NULL){
		if (default_display_desc==NULL){
			ms_error("No default display built in !");
			return;
		}
		obj->display=ms_display_new(default_display_desc);
		obj->own_display=TRUE;
	}
	if (!ms_display_init(obj->display,f,&obj->fbuf,&obj->fbuf_selfview)){
		if (obj->own_display) ms_display_destroy(obj->display);
		obj->display=NULL;
	}
	if (obj->sws1!=NULL){
		sws_freeContext(obj->sws1);
		obj->sws1=NULL;
	}
	if (obj->sws2!=NULL){
		sws_freeContext(obj->sws2);
		obj->sws2=NULL;
	}
	if (obj->local_msg!=NULL) {
		freemsg(obj->local_msg);
		obj->local_msg=NULL;
	}
	set_corner(obj,obj->corner);
	obj->ready=TRUE;
}

static int video_out_handle_resizing(MSFilter *f, void *data){
	VideoOut *s=(VideoOut*)f->data;
	MSDisplay *disp=s->display;
	if (disp!=NULL){
		MSDisplayEvent ev;
		if (ms_display_poll_event(disp,&ev)){
			if (ev.evtype==MS_DISPLAY_RESIZE_EVENT){
				MSVideoSize sz;
				sz.width=ev.w;
				sz.height=ev.h;
				ms_filter_lock(f);
				if (s->ready){
					set_vsize(s,&sz);
					s->ready=FALSE;
				}
				ms_filter_unlock(f);
			}
		}
	}
	return 0;
}

static void video_out_preprocess(MSFilter *f){
	video_out_prepare(f);
}


static void video_out_process(MSFilter *f){
	VideoOut *obj=(VideoOut*)f->data;
	mblk_t *inm;

	ms_filter_lock(f);
	if (!obj->ready) video_out_prepare(f);
	if (obj->display==NULL){
		ms_filter_unlock(f);
		if (f->inputs[0]!=NULL)
			ms_queue_flush(f->inputs[0]);
		if (f->inputs[1]!=NULL)
			ms_queue_flush(f->inputs[1]);
		return;
	}
	/*get most recent message and draw it*/
	if (f->inputs[1]!=NULL && (inm=ms_queue_peek_last(f->inputs[1]))!=0) {
		if (obj->corner==-1){
			if (obj->local_msg!=NULL) {
				freemsg(obj->local_msg);
				obj->local_msg=NULL;
			}
		}else if (obj->fbuf_selfview.planes[0]!=NULL) {
			MSPicture src;
			if (yuv_buf_init_from_mblk(&src,inm)==0){
				
				if (obj->sws2==NULL){
					obj->sws2=sws_getContext(src.w,src.h,PIX_FMT_YUV420P,
											 obj->fbuf_selfview.w,obj->fbuf_selfview.h,PIX_FMT_YUV420P,
											 SWS_FAST_BILINEAR, NULL, NULL, NULL);
				}
				ms_display_lock(obj->display);
				if (sws_scale(obj->sws2,src.planes,src.strides, 0,
							  src.h, obj->fbuf_selfview.planes, obj->fbuf_selfview.strides)<0){
					ms_error("Error in sws_scale().");
				}
				if (!mblk_get_precious_flag(inm)) yuv_buf_mirror(&obj->fbuf_selfview);
				ms_display_unlock(obj->display);
			}
		}else{
			MSPicture src;
			if (yuv_buf_init_from_mblk(&src,inm)==0){
				
				if (obj->sws2==NULL){
					obj->sws2=sws_getContext(src.w,src.h,PIX_FMT_YUV420P,
								obj->local_pic.w,obj->local_pic.h,PIX_FMT_YUV420P,
								SWS_FAST_BILINEAR, NULL, NULL, NULL);
				}
				if (obj->local_msg==NULL){
					obj->local_msg=yuv_buf_alloc(&obj->local_pic,
						obj->local_pic.w,obj->local_pic.h);
				}
				if (obj->local_pic.planes[0]!=NULL)
				{
					if (sws_scale(obj->sws2,src.planes,src.strides, 0,
						src.h, obj->local_pic.planes, obj->local_pic.strides)<0){
						ms_error("Error in sws_scale().");
					}
					if (!mblk_get_precious_flag(inm)) yuv_buf_mirror(&obj->local_pic);
				}
			}
		}
		ms_queue_flush(f->inputs[1]);
	}
	
	if (f->inputs[0]!=NULL && (inm=ms_queue_peek_last(f->inputs[0]))!=0) {
		MSPicture src;
		if (yuv_buf_init_from_mblk(&src,inm)==0){
			MSVideoSize cur,newsize;
			cur.width=obj->fbuf.w;
			cur.height=obj->fbuf.h;
			newsize.width=src.w;
			newsize.height=src.h;
			if (obj->autofit && !ms_video_size_equal(newsize,obj->prevsize) ) {
				MSVideoSize qvga_size;
				qvga_size.width=MS_VIDEO_SIZE_QVGA_W;
				qvga_size.height=MS_VIDEO_SIZE_QVGA_H;
				obj->prevsize=newsize;
				ms_message("received size is %ix%i",newsize.width,newsize.height);
				/*don't resize less than QVGA, it is too small*/
				if (ms_video_size_greater_than(qvga_size,newsize)){
					newsize.width=MS_VIDEO_SIZE_QVGA_W;
					newsize.height=MS_VIDEO_SIZE_QVGA_H;
				}
				if (!ms_video_size_equal(newsize,cur)){
					set_vsize(obj,&newsize);
					ms_message("autofit: new size is %ix%i",newsize.width,newsize.height);
					video_out_prepare(f);
				}
			}
			if (obj->sws1==NULL){
				obj->sws1=sws_getContext(src.w,src.h,PIX_FMT_YUV420P,
				obj->fbuf.w,obj->fbuf.h,PIX_FMT_YUV420P,
				SWS_FAST_BILINEAR, NULL, NULL, NULL);
			}
			ms_display_lock(obj->display);
			if (sws_scale(obj->sws1,src.planes,src.strides, 0,
            			src.h, obj->fbuf.planes, obj->fbuf.strides)<0){
				ms_error("Error in sws_scale().");
			}
			if (obj->mirror && !mblk_get_precious_flag(inm)) yuv_buf_mirror(&obj->fbuf);
			ms_display_unlock(obj->display);
		}
		ms_queue_flush(f->inputs[0]);
	}

	/*copy resized local view into main buffer, at bottom left corner:*/
	if (obj->local_msg!=NULL){
		MSPicture corner=obj->fbuf;
		MSVideoSize roi;
		roi.width=obj->local_pic.w;
		roi.height=obj->local_pic.h;
		corner.w=obj->local_pic.w;
		corner.h=obj->local_pic.h;
		corner.planes[0]+=obj->local_rect.x+(obj->local_rect.y*corner.strides[0]);
		corner.planes[1]+=(obj->local_rect.x/2)+((obj->local_rect.y/2)*corner.strides[1]);
		corner.planes[2]+=(obj->local_rect.x/2)+((obj->local_rect.y/2)*corner.strides[2]);
		corner.planes[3]=0;
		ms_display_lock(obj->display);
		yuv_buf_copy(obj->local_pic.planes,obj->local_pic.strides,
				corner.planes,corner.strides,roi);
		ms_display_unlock(obj->display);
	}

	ms_display_update(obj->display);
	ms_filter_unlock(f);
}

static int video_out_set_vsize(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	ms_filter_lock(f);
	set_vsize(s,(MSVideoSize*)arg);
	ms_filter_unlock(f);
	return 0;
}

static int video_out_set_display(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->display=(MSDisplay*)arg;
	return 0;
}

static int video_out_auto_fit(MSFilter *f, void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->autofit=*(int*)arg;
	return 0;
}

static int video_out_set_corner(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->sv_posx=s->sv_posy=SELVIEW_POS_INACTIVE;
	ms_filter_lock(f);
	set_corner(s, *(int*)arg);
	if (s->display){
		ms_display_lock(s->display);
		{
		int w=s->fbuf.w;
		int h=s->fbuf.h;
		int ysize=w*h;
		int usize=ysize/4;
		
		memset(s->fbuf.planes[0], 0, ysize);
		memset(s->fbuf.planes[1], 0, usize);
		memset(s->fbuf.planes[2], 0, usize);
		s->fbuf.planes[3]=NULL;
		}
		ms_display_unlock(s->display);
	}
	ms_filter_unlock(f);
	return 0;
}

static int video_out_get_corner(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	*((int*)arg)=s->corner;
	return 0;
}

static int video_out_set_scalefactor(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->scale_factor = *(float*)arg;
	if (s->scale_factor<0.5f)
		s->scale_factor = 0.5f;
	ms_filter_lock(f);
	set_corner(s, s->corner);
	if (s->display){
		ms_display_lock(s->display);
		{
		int w=s->fbuf.w;
		int h=s->fbuf.h;
		int ysize=w*h;
		int usize=ysize/4;
		
		memset(s->fbuf.planes[0], 0, ysize);
		memset(s->fbuf.planes[1], 0, usize);
		memset(s->fbuf.planes[2], 0, usize);
		s->fbuf.planes[3]=NULL;
		}
		ms_display_unlock(s->display);
	}
	ms_filter_unlock(f);
	return 0;
}

static int video_out_get_scalefactor(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	*((float*)arg)=(float)s->scale_factor;
	return 0;
}


static int video_out_enable_mirroring(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->mirror=*(int*)arg;
	return 0;
}

static int video_out_get_native_window_id(MSFilter *f, void*arg){
	VideoOut *s=(VideoOut*)f->data;
	unsigned long *id=(unsigned long*)arg;
	*id=0;
	if (s->display){
		*id=s->display->window_id;
		return 0;
	}
	return -1;
}

static int video_out_set_selfview_pos(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	s->sv_posx=((float*)arg)[0];
	s->sv_posy=((float*)arg)[1];
	s->scale_factor=(float)100.0/((float*)arg)[2];
	return 0;
}

static int video_out_get_selfview_pos(MSFilter *f,void *arg){
	VideoOut *s=(VideoOut*)f->data;
	((float*)arg)[0]=s->sv_posx;
	((float*)arg)[1]=s->sv_posy;
	((float*)arg)[2]=(float)100.0/s->scale_factor;
	return 0;
}
static MSFilterMethod methods[]={
	{	MS_FILTER_SET_VIDEO_SIZE	,	video_out_set_vsize },
	{	MS_VIDEO_OUT_SET_DISPLAY	,	video_out_set_display},
	{	MS_VIDEO_OUT_SET_CORNER 	,	video_out_set_corner},
	{	MS_VIDEO_OUT_AUTO_FIT		,	video_out_auto_fit},
	{	MS_VIDEO_OUT_HANDLE_RESIZING	,	video_out_handle_resizing},
	{	MS_VIDEO_OUT_ENABLE_MIRRORING	,	video_out_enable_mirroring},
	{	MS_VIDEO_OUT_GET_NATIVE_WINDOW_ID,	video_out_get_native_window_id},
	{	MS_VIDEO_OUT_GET_CORNER 	,	video_out_get_corner},
	{	MS_VIDEO_OUT_SET_SCALE_FACTOR 	,	video_out_set_scalefactor},
	{	MS_VIDEO_OUT_GET_SCALE_FACTOR 	,	video_out_get_scalefactor},
	{	MS_VIDEO_OUT_SET_SELFVIEW_POS 	 ,	video_out_set_selfview_pos},
	{	MS_VIDEO_OUT_GET_SELFVIEW_POS    ,  video_out_get_selfview_pos},
	{	0	,NULL}
};

#ifdef _MSC_VER

MSFilterDesc ms_video_out_desc={
	MS_VIDEO_OUT_ID,
	"MSVideoOut",
	N_("A generic video display"),
	MS_FILTER_OTHER,
	NULL,
	2,
	0,
	video_out_init,
	video_out_preprocess,
	video_out_process,
	NULL,
	video_out_uninit,
	methods
};

#else

MSFilterDesc ms_video_out_desc={
	.id=MS_VIDEO_OUT_ID,
	.name="MSVideoOut",
	.text=N_("A generic video display"),
	.category=MS_FILTER_OTHER,
	.ninputs=2,
	.noutputs=0,
	.init=video_out_init,
	.preprocess=video_out_preprocess,
	.process=video_out_process,
	.uninit=video_out_uninit,
	.methods=methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_video_out_desc)
