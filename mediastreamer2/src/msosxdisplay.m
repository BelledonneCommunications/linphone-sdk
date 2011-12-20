#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "opengles_display.h"

#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include "opengles_display.h"

static void drawAnObject ()
{
    glColor3f(1.0f, 0.85f, 0.35f);
    glBegin(GL_TRIANGLES);
    {
        glVertex3f(  0.0,  0.6, 0.0);
        glVertex3f( -0.2, -0.3, 0.0);
        glVertex3f(  0.2, -0.3 ,0.0);
    }
    glEnd();
}

@interface NsMsGLDisplay : NSOpenGLView
{
@public
    struct opengles_display* display_helper;
    int w, h;
    
}
- (void) drawRect: (NSRect) bounds;
-(void) reshape;
@end

typedef struct GLOSXState {
    NSWindow* window;
    NsMsGLDisplay* disp;
} GLOSXState;

@implementation NsMsGLDisplay


-(void) reshape {
CGLContextObj obj = CGLGetCurrentContext();
ms_message("Current ctx: %p", obj);

	ms_message("Set GL context");
	NSOpenGLContext* ctx = [self openGLContext];
    [ctx makeCurrentContext];
    ogl_display_init(display_helper, self.bounds.size.width, self.bounds.size.height); 
    [NSOpenGLContext clearCurrentContext];
}

-(void) drawRect: (NSRect) bounds
{
CGLContextObj obj = CGLGetCurrentContext();
ms_message("Current ctx: %p", obj);

	 NSOpenGLContext* ctx = [self openGLContext];
	 [ctx makeCurrentContext];
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    //drawAnObject();
    ogl_display_render(display_helper);
    glFlush();
    [NSOpenGLContext clearCurrentContext];
}

-(void) resizeWindow: (id) window {
    
    [window setFrame:NSMakeRect(0, 0, w, h) display:YES];
}

@end

#include <OpenGL/CGLRenderers.h>
static void osx_gl_init(MSFilter* f) {
	CGLContextObj obj = CGLGetCurrentContext();
ms_message("Current ctx: %p", obj);

    GLOSXState* s = (GLOSXState*) ms_new0(GLOSXState, 1);
	f->data = s;
}

static void init_for_real(GLOSXState* s) {
    NSWindow* window = [[[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100) styleMask:(NSTitledWindowMask | NSResizableWindowMask | NSClosableWindowMask) backing:NSBackingStoreBuffered defer:NO] autorelease];
    [window setBackgroundColor: [NSColor blueColor]];
    [window makeKeyAndOrderFront:NSApp];
    [window setTitle: @"Mediastreamer"];
    [window setMovable:YES];
    [window setMovableByWindowBackground:YES];
    s->window = [window retain];

	NSOpenGLPixelFormatAttribute attrs[] = {
		//NSOpenGLPFARendererID, kCGLRendererGenericID,
		NSOpenGLPFAAccelerated,
		//NSOpenGLPFADoubleBuffer,
		//NSOpenGLPFADepthSize, 32,
		0
	};
	 NSOpenGLPixelFormat * fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    s->disp = [[NsMsGLDisplay alloc] initWithFrame:[s->window frame] pixelFormat:fmt];
    s->disp->display_helper = ogl_display_new();
    s->disp->w = 100;
    s->disp->h = 100;
	[s->window setContentView: s->disp];

}

static void osx_gl_preprocess(MSFilter* f) {
    
}

static void osx_gl_process(MSFilter* f) {
    GLOSXState* s = (GLOSXState*) f->data;
    mblk_t* m = 0;
    MSPicture pic;
    
    if (!s->window)
    	init_for_real(s);
    
    if ((m=ms_queue_peek_last(f->inputs[0]))!=NULL){
        if (ms_yuv_buf_init_from_mblk (&pic,m)==0){
            if (pic.w != s->disp->w || pic.h != s->disp->h) {
                s->disp->w = pic.w;
                s->disp->h = pic.h;
                id w = s->window;
                [s->disp performSelectorOnMainThread:@selector(resizeWindow:) withObject:w waitUntilDone:NO];
            }
            
            ogl_display_set_yuv_to_display(s->disp->display_helper, m);
            // ogl_display_set_preview_yuv_to_display(s->disp->display_helper, m);
            [s->disp setNeedsDisplay:YES];
            ms_queue_remove(f->inputs[0], m);
        }
    }
    ms_queue_flush(f->inputs[0]);
    
    if (f->inputs[1] != NULL) {
        if (0 && (m=ms_queue_peek_last(f->inputs[1]))!=NULL){
            if (ms_yuv_buf_init_from_mblk (&pic,m)==0){
                ogl_display_set_preview_yuv_to_display(s->disp->display_helper, m);
                [s->disp setNeedsDisplay:YES];
                ms_queue_remove(f->inputs[1], m);
            }
        }
        ms_queue_flush(f->inputs[1]);        
    }
}

static void osx_gl_uninit(MSFilter* f) {
   GLOSXState* s = (GLOSXState*) f->data;
    NSLog(@"todo");
    return;
    if (s->disp) {
        [s->disp dealloc];
        s->disp = nil;
    }
    if (s->window) {
        [s->window close];
        [s->window release];
    }
}

static int osx_gl_set_vsize(MSFilter* f, void* arg) {
    return -1;
}

static int osx_gl_get_native_window_id(MSFilter* f, void* arg) {
    GLOSXState* s = (GLOSXState*) f->data;
    unsigned long *winId=(unsigned long*)arg;
    *winId = (unsigned long)0;//s->window;
    return 0;
}

static int osx_gl_enable_mirroring(MSFilter* f, void* arg) {
    return -1;
}

static int osx_gl_set_local_view_mode(MSFilter* f, void* arg) {
    return -1;
}

static MSFilterMethod methods[]={
    {MS_FILTER_SET_VIDEO_SIZE, osx_gl_set_vsize},
    {MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, osx_gl_get_native_window_id},
	{MS_VIDEO_DISPLAY_ENABLE_MIRRORING, osx_gl_enable_mirroring },
    {MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE, osx_gl_set_local_view_mode},
    { 0, NULL }
};


MSFilterDesc ms_osx_gl_display_desc = {
    .id=MS_OSX_GL_DISPLAY_ID,
    .name="MSOSXGLDisplay",
    .text="MacOSX GL-based display",
    .category=MS_FILTER_OTHER,
    .ninputs=2,
    .noutputs=0,
    .init=osx_gl_init,
    .preprocess=osx_gl_preprocess,
    .process=osx_gl_process,
    .uninit=osx_gl_uninit,
    .methods=methods
};

MS_FILTER_DESC_EXPORT(ms_osx_gl_display_desc);




