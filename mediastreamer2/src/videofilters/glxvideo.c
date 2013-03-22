/* indent-tabs-mode: t
 * vi: set noexpandtab:
 * :noTabs=false:
 */
/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006	Simon MORLAT (simon.morlat@linphone.org)

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

#include <X11/Xlib.h>

#include "opengles_display.h"
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>

static void glxvideo_unprepare(MSFilter *f);

static bool_t createX11GLWindow(Display* display, MSVideoSize size, GLXContext* ctx, Window* win);

typedef struct GLXVideo
{
	MSVideoSize vsize;
	MSVideoSize wsize; /*wished window size */
	Display *display;
	Window window_id;
	GLXContext glContext;
	struct opengles_display *glhelper;
	bool_t show;
	bool_t own_window;
	bool_t ready;
	bool_t mirror;
	bool_t autofit;
} GLXVideo;



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

static void glxvideo_init(MSFilter	*f){
	GLXVideo *obj=(GLXVideo*)ms_new0(GLXVideo,1);
	MSVideoSize def_size;
	def_size.width=MS_VIDEO_SIZE_CIF_W;
	def_size.height=MS_VIDEO_SIZE_CIF_H;
	obj->display=init_display();
	obj->own_window=FALSE;
	obj->ready=FALSE;
	obj->vsize=def_size; /* the size of the main video*/
	obj->wsize=def_size; /* the size of the window*/
	obj->show=TRUE;
	obj->autofit=TRUE;
	f->data=obj;
}


static void glxvideo_uninit(MSFilter *f){
	GLXVideo *obj=(GLXVideo*)f->data;

	glxvideo_unprepare(f);
	if (obj->own_window){
		XDestroyWindow(obj->display,obj->window_id);
	}
	if (obj->display){
		XCloseDisplay(obj->display);
		obj->display=NULL;
	}
	ms_free(obj);
}

static void glxvideo_prepare(MSFilter *f){
	GLXVideo *s=(GLXVideo*)f->data;

	XWindowAttributes wa;

	if (s->display==NULL) return;
	if (s->window_id==0){
		if (createX11GLWindow(s->display, s->wsize, &s->glContext, &s->window_id)) {
			GLenum err;
			s->glhelper = ogl_display_new();
			glXMakeCurrent( s->display, s->window_id, s->glContext );
			err = glewInit();
			if (err != GLEW_OK) {
				ms_error("Failed to initialize GLEW");
				return;
			} else if (!GLEW_VERSION_2_0) {
				ms_error("Need OpenGL 2.0+");
				return;
			} else {
				ogl_display_init(s->glhelper, s->wsize.width, s->wsize.height);
			}
		}
		if (s->window_id==0) return;
		s->own_window=TRUE;
	}else if (s->own_window==FALSE){
		/*we need to register for resize events*/
		XSelectInput(s->display,s->window_id,StructureNotifyMask);
	}

	XGetWindowAttributes(s->display,s->window_id,&wa);
	ms_message("glxvideo_prepare(): Window has size %ix%i, received video is %ix%i",wa.width,wa.height,s->vsize.width,s->vsize.height);

	if (wa.width<MS_LAYOUT_MIN_SIZE || wa.height<MS_LAYOUT_MIN_SIZE){
		return;
	}

	s->wsize.width=wa.width;
	s->wsize.height=wa.height;
	s->ready=TRUE;
}

static void glxvideo_unprepare(MSFilter *f){
	GLXVideo *s=(GLXVideo*)f->data;
	s->ready=FALSE;
}

static void glxvideo_preprocess(MSFilter *f){
	GLXVideo *obj=(GLXVideo*)f->data;
	if (obj->show) {
		if (obj->ready) glxvideo_unprepare(f);
		glxvideo_prepare(f);
	}
}


static void glxvideo_process(MSFilter *f){
	GLXVideo *obj=(GLXVideo*)f->data;
	mblk_t *inm;
	MSPicture src={0};
	bool_t precious=FALSE;
	
	XWindowAttributes wa;
	XGetWindowAttributes(obj->display,obj->window_id,&wa);
	if (wa.width!=obj->wsize.width || wa.height!=obj->wsize.height){
		ms_warning("Resized to %ix%i", wa.width,wa.height);
		obj->wsize.width=wa.width;
		obj->wsize.height=wa.height;
		ogl_display_init(obj->glhelper, wa.width, wa.height);
	}
	
	ms_filter_lock(f);
	if (!obj->show) {
		goto end;
	}
	if (!obj->ready) glxvideo_prepare(f);
	if (!obj->ready){
		goto end;
	}

	glXMakeCurrent( obj->display, obj->window_id, obj->glContext );
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
				glxvideo_unprepare(f);
				glxvideo_prepare(f);
				if (!obj->ready) goto end;
			}
			if (obj->mirror && !precious) ms_yuv_buf_mirror(&src);
			ogl_display_set_yuv_to_display(obj->glhelper, inm);
		}
	}
	if (f->inputs[1]!=NULL && (inm=ms_queue_peek_last(f->inputs[1]))!=0) {
		if (ms_yuv_buf_init_from_mblk(&src,inm)==0){
			ogl_display_set_preview_yuv_to_display(obj->glhelper, inm);
		}
	}
	ogl_display_render(obj->glhelper, 0);
	glXSwapBuffers ( obj->display, obj->window_id );

	end:
		ms_filter_unlock(f);
		if (f->inputs[0]!=NULL)
			ms_queue_flush(f->inputs[0]);
		if (f->inputs[1]!=NULL)
			ms_queue_flush(f->inputs[1]);
}

static int glxvideo_set_vsize(MSFilter *f,void *arg){
	GLXVideo *s=(GLXVideo*)f->data;
	ms_filter_lock(f);
	s->wsize=*(MSVideoSize*)arg;
	ms_filter_unlock(f);
	return 0;
}

static int glxvideo_show_video(MSFilter *f, void *arg){
	GLXVideo *s=(GLXVideo*)f->data;
	bool_t show=*(bool_t*)arg;
	s->show=show?TRUE:FALSE;
	if (s->show==FALSE) {
		ms_filter_lock(f);
		glxvideo_unprepare(f);
		ms_filter_unlock(f);
	}

	return 0;
}

static int glxvideo_zoom(MSFilter *f, void *arg){
	GLXVideo *s=(GLXVideo*)f->data;

	ms_filter_lock(f);
	ogl_display_zoom(s->glhelper, arg);
	
	ms_filter_unlock(f);
	return 0;
}

static int glxvideo_get_native_window_id(MSFilter *f, void*arg){
	GLXVideo *s=(GLXVideo*)f->data;
	unsigned long *id=(unsigned long*)arg;
	*id=s->window_id;
	return 0;
}

static int glxvideo_set_native_window_id(MSFilter *f, void*arg){
	ms_error("MSGLXVideo: cannot change native window");
	return 0;
}

static bool_t createX11GLWindow(Display* display, MSVideoSize size, GLXContext* ctx, Window* win) {
	static int visual_attribs[] =
	{
	GLX_X_RENDERABLE	, True,
	GLX_DRAWABLE_TYPE	, GLX_WINDOW_BIT,
	GLX_RENDER_TYPE , GLX_RGBA_BIT,
	GLX_X_VISUAL_TYPE	, GLX_TRUE_COLOR,
	GLX_RED_SIZE	  , 8,
	GLX_GREEN_SIZE	, 8,
	GLX_BLUE_SIZE	 , 8,
	GLX_DOUBLEBUFFER	, True,
	//GLX_SAMPLE_BUFFERS  , 1,
	//GLX_SAMPLES	   , 4,
	None
	};
	int glx_major, glx_minor;
 
	// FBConfigs were added in GLX version 1.3.
	if ( !glXQueryVersion( display, &glx_major, &glx_minor ) ||		 ( ( glx_major == 1 ) && ( glx_minor < 3 ) ) || ( glx_major < 1 ) ) {
		ms_error( "Invalid GLX version" );
		return FALSE;
	}

	ms_message( "Getting matching framebuffer configs" );
	int fbcount;
	GLXFBConfig *fbc = glXChooseFBConfig( display, DefaultScreen( display ), 
							visual_attribs, &fbcount );
	if ( !fbc ) {
		ms_error( "Failed to retrieve a framebuffer config" );
		return FALSE;
	}
	ms_message( "Found %d matching FB configs.", fbcount );
	// Pick the FB config/visual with the most samples per pixel
	ms_message( "Getting XVisualInfos" );
	int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;
 
	int i;
	for ( i = 0; i < fbcount; i++ ) {
		XVisualInfo *vi = glXGetVisualFromFBConfig( display, fbc[i] );
		if ( vi ) {
			int samp_buf, samples;
			glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf );
			glXGetFBConfigAttrib( display, fbc[i], GLX_SAMPLES	 , &samples  );
 
			ms_message( "  Matching fbconfig %d, visual ID 0x%lu: SAMPLE_BUFFERS = %d,"
		  " SAMPLES = %d", 
		  i, vi -> visualid, samp_buf, samples );
 
			if ( best_fbc < 0 || (samp_buf && samples > best_num_samp) )
				best_fbc = i, best_num_samp = samples;
			if ( worst_fbc < 0 || (!samp_buf || samples < worst_num_samp) )
				worst_fbc = i, worst_num_samp = samples;
		}
		XFree( vi );
	}
	GLXFBConfig bestFbc = fbc[ best_fbc ];
 
	// Be sure to free the FBConfig list allocated by glXChooseFBConfig()
	XFree( fbc );

	// Get a visual
	XVisualInfo *vi = glXGetVisualFromFBConfig( display, bestFbc );
	ms_message( "Chosen visual ID = 0x%lu", vi->visualid );
 
	ms_message( "Creating colormap" );
	XSetWindowAttributes swa;
	Colormap cmap;
	swa.colormap = cmap = XCreateColormap( display,
							 RootWindow( display, vi->screen ), 
							 vi->visual, AllocNone );
	swa.background_pixmap = None ;
	swa.border_pixel	= 0;
	swa.event_mask	  = StructureNotifyMask;
	ms_message( "Creating window" );
	*win = XCreateWindow( display, RootWindow( display, vi->screen ), 
					200, 200, size.width, size.height, 0, vi->depth, InputOutput, 
					vi->visual, 
					CWBorderPixel|CWColormap|CWEventMask, &swa );
	if ( !(*win) ) {
		ms_error( "Failed to create window." );
		return FALSE;
	}
	// Done with the visual info data
	XFree( vi );
 
	XStoreName( display, *win, "Video" );
 
	ms_message( "Mapping window" );
	XMapWindow( display, *win );
 
	// Get the default screen's GLX extension list
	*ctx = glXCreateNewContext( display, bestFbc, GLX_RGBA_TYPE, 0, True );

	// Sync to ensure any errors generated are processed.
	XSync( display, False );

	if (!(*ctx)) {
		ms_error("GL context creation failed");
		return FALSE;
	}

	return TRUE;
}

static int glxvideo_enable_mirroring(MSFilter *f,void *arg){
	GLXVideo *s=(GLXVideo*)f->data;
	s->mirror=(bool_t)*(int*)arg;
	return 0;
}

static int glxvideo_enable_autofit(MSFilter *f,void *arg){
	GLXVideo *s=(GLXVideo*)f->data;
	s->autofit=(bool_t)*(int*)arg;
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_VIDEO_SIZE		, glxvideo_set_vsize },
	{	MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID	, glxvideo_get_native_window_id },
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID	, glxvideo_set_native_window_id },
	{	MS_VIDEO_DISPLAY_SHOW_VIDEO		, glxvideo_show_video },
	{	MS_VIDEO_DISPLAY_ZOOM			, glxvideo_zoom },
	{	MS_VIDEO_DISPLAY_ENABLE_MIRRORING	, glxvideo_enable_mirroring},
	{	MS_VIDEO_DISPLAY_ENABLE_AUTOFIT		, glxvideo_enable_autofit	},
	{	0	,NULL}
};


MSFilterDesc ms_glxvideo_desc={
	.id=MS_GLXVIDEO_ID,
	.name="MSGLXVideo",
	.text="A video display using GL (glx)",
	.category=MS_FILTER_OTHER,
	.ninputs=2,
	.noutputs=0,
	.init=glxvideo_init,
	.preprocess=glxvideo_preprocess,
	.process=glxvideo_process,
	.uninit=glxvideo_uninit,
	.methods=methods
};


MS_FILTER_DESC_EXPORT(ms_glxvideo_desc)
