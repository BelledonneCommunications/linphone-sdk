#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "opengles_display.h"

#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#import <QuartzCore/CATransaction.h>

@interface CAMsGLLayer : CAOpenGLLayer {
@public
    struct opengles_display* display_helper;
    CGSize sourceSize;
    NSWindow *window;
	CGLPixelFormatObj cglPixelFormat;
	CGLContextObj cglContext;
    NSRecursiveLock *lock;
    CGRect prevBounds;
}

- (void)resizeWindow;

@property (assign) CGRect prevBounds;
@property (assign) CGSize sourceSize;
@property (readonly) NSRecursiveLock* lock;

@end

@implementation CAMsGLLayer

@synthesize prevBounds;
@synthesize sourceSize;
@synthesize lock;

- (id)init {
    self = [super init];
    if(self != nil) {
        self->window = nil;
        self->sourceSize = CGSizeMake(0, 0);
        self->prevBounds = CGRectMake(0, 0, 0, 0);
        self->lock = [[NSRecursiveLock alloc] init];
        self->display_helper = ogl_display_new();
        
        [self setOpaque:YES];
        [self setAsynchronous:NO];
        [self setAutoresizingMask: kCALayerWidthSizable | kCALayerHeightSizable];
        //[self setNeedsDisplayOnBoundsChange:YES];
        
		// FBO Support
		GLint numPixelFormats = 0;
		CGLPixelFormatAttribute attributes[] =
		{
			kCGLPFAAccelerated,
			kCGLPFANoRecovery,
			kCGLPFADoubleBuffer,
			0
		};
        
		CGLChoosePixelFormat(attributes, &cglPixelFormat, &numPixelFormats);
        assert(cglPixelFormat);
        
		cglContext = [super copyCGLContextForPixelFormat:cglPixelFormat];
		assert(cglContext);
    }
    return self;
}

- (void)dealloc {
    ogl_display_uninit(display_helper, TRUE);
    ogl_display_free(display_helper);
    
    [self releaseCGLContext:cglContext];
    [self releaseCGLPixelFormat:cglPixelFormat];
    [lock release];
    
    [super dealloc];
}

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
	CGLRetainPixelFormat(cglPixelFormat);
	return cglPixelFormat;
}

- (void)releaseCGLPixelFormat:(CGLPixelFormatObj)pixelFormat {
	CGLReleasePixelFormat(cglPixelFormat);
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat {
	CGLRetainContext(cglContext);
	return cglContext;
}

- (void)releaseCGLContext:(CGLContextObj)glContext {
	CGLReleaseContext(cglContext);
}

- (void)drawInCGLContext:(CGLContextObj)glContext 
             pixelFormat:(CGLPixelFormatObj)pixelFormat 
            forLayerTime:(CFTimeInterval)timeInterval 
             displayTime:(const CVTimeStamp *)timeStamp { 
    if([lock tryLock]) {
        CGLContextObj savedContext = CGLGetCurrentContext();
        CGLSetCurrentContext(cglContext);
        CGLLockContext(cglContext);
    
        if (!NSEqualRects(prevBounds, [self bounds])) {
            prevBounds = [self bounds];
            ogl_display_init(display_helper, prevBounds.size.width, prevBounds.size.height);
        }
        
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        ogl_display_render(display_helper, 0);
        
        CGLUnlockContext(cglContext);
        CGLSetCurrentContext(savedContext);
        CGLFlushDrawable(cglContext);
        
        [super drawInCGLContext:glContext 
                    pixelFormat:pixelFormat 
                   forLayerTime:timeInterval 
                    displayTime:timeStamp];
        [lock unlock];
    }
}

- (void)layoutSublayers {
    self.frame = [self superlayer].bounds;
    [self setNeedsDisplay];
}

- (void)resizeWindow {
    if(window != nil) {
        // Centred resize
        NSRect rect = [window frameRectForContentRect:NSMakeRect(0, 0, sourceSize.width, sourceSize.height)];
        NSRect windowRect = [window frame];
        windowRect.origin.x -= (rect.size.width - windowRect.size.width)/2;
        windowRect.origin.y -= (rect.size.height - windowRect.size.height)/2;
        windowRect.size.width = rect.size.width;
        windowRect.size.height = rect.size.height;
        [window setFrame:windowRect display:YES animate:YES];
    }
}

@end

typedef struct GLOSXState {
    NSWindow* window;
    CALayer* layer;
    CAMsGLLayer *glLayer;
} GLOSXState;


#include <OpenGL/CGLRenderers.h>
static void osx_gl_init(MSFilter* f) {
   GLOSXState* s = (GLOSXState*) ms_new0(GLOSXState, 1);
	f->data = s;
}

static void init_for_real(GLOSXState* s) {
    // Init window
    if(s->window == nil && s->layer == nil) {
        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100) styleMask:(NSTitledWindowMask | NSResizableWindowMask | NSClosableWindowMask) backing:NSBackingStoreBuffered defer:NO];
        [window setBackgroundColor: [NSColor blueColor]];
        [window makeKeyAndOrderFront:NSApp];
        [window setTitle: @"Video"];
        [window setMovable:YES];
        [window setMovableByWindowBackground:YES];
        [window setReleasedWhenClosed:NO];
        CGFloat xPos = NSWidth([[window screen] frame])/2 - NSWidth([window frame])/2;
        CGFloat yPos = NSHeight([[window screen] frame])/2 - NSHeight([window frame])/2;
        [window setFrame:NSMakeRect(xPos, yPos, NSWidth([window frame]), NSHeight([window frame])) display:YES];
        s->window = window;
        
        // Init view
        NSView *view = [[[NSView alloc] initWithFrame:[s->window frame]] autorelease];
        [view setWantsLayer:YES];
        [view.layer setAutoresizingMask: kCALayerWidthSizable | kCALayerHeightSizable];
        [view.layer setNeedsDisplayOnBoundsChange: YES];
        [s->window setContentView: view];
    }
    
    s->glLayer = [[CAMsGLLayer alloc] init];
    if(s->window != nil) {
        s->glLayer->window = s->window;
        [s->glLayer retain];
        [s->window retain];
        // Force resize
        dispatch_async(dispatch_get_main_queue(), ^{
            [s->glLayer setFrame:[[s->window.contentView layer] bounds]];
            [[s->window.contentView layer] addSublayer: s->glLayer];
            [s->window release];
            [s->glLayer release];
        });
    } else {
        // Resize and add the layout
        [s->layer retain];
        [s->glLayer retain];
        dispatch_async(dispatch_get_main_queue(), ^{
            [s->glLayer setFrame:[s->layer bounds]];
            [s->layer addSublayer: s->glLayer];
            [s->layer release];
            [s->glLayer release];
        });
    }
}

static void osx_gl_preprocess(MSFilter* f) {
	init_for_real((GLOSXState*) f->data);
}

static void osx_gl_process(MSFilter* f) {
    GLOSXState* s = (GLOSXState*) f->data;
    mblk_t* m = 0;
    MSPicture pic;
    
    NSAutoreleasePool *loopPool = [[NSAutoreleasePool alloc] init];

    if ((m=ms_queue_peek_last(f->inputs[0])) != NULL) {
        if (ms_yuv_buf_init_from_mblk (&pic,m) == 0) {
            // Source size change?
            if (pic.w != s->glLayer->sourceSize.width || pic.h != s->glLayer->sourceSize.height) {
                s->glLayer.sourceSize = CGSizeMake(pic.w, pic.h);
                
                // Force window resize
                if(s->glLayer->window != nil) {
                    [s->glLayer performSelectorOnMainThread:@selector(resizeWindow) withObject:nil waitUntilDone:FALSE];
                }
            }
            ogl_display_set_yuv_to_display(s->glLayer->display_helper, m);
            
            // Force redraw
            [s->glLayer setNeedsDisplay];
        }
    }
    ms_queue_flush(f->inputs[0]);

    if (f->inputs[1] != NULL) {
        if ((m=ms_queue_peek_last(f->inputs[1])) != NULL) {
            if (ms_yuv_buf_init_from_mblk (&pic,m) == 0) {
                ogl_display_set_preview_yuv_to_display(s->glLayer->display_helper, m);
                
                // Force redraw
                [s->glLayer setNeedsDisplay];
            }
        }
        ms_queue_flush(f->inputs[1]);
    }
    
    // From Apple's doc: "An autorelease pool should always be drained in the same context (such as the invocation of a method or function, or the body of a loop) in which it was created." So we cannot create on autorelease pool in init and drain it in uninit.
    
    [loopPool drain];
}

static void osx_gl_uninit(MSFilter* f) {
    GLOSXState* s = (GLOSXState*) f->data;
    
    if(s->glLayer != nil) {
        [s->glLayer removeFromSuperlayer];
        [s->glLayer release];
    }
    
    if(s->layer != nil) {
        [s->layer release];
    }
    
    if(s->window != nil) {
        [s->window release];
    }
}

static int osx_gl_set_vsize(MSFilter* f, void* arg) {
    return -1;
}

static int osx_gl_get_native_window_id(MSFilter* f, void* arg) {
    GLOSXState* s = (GLOSXState*) f->data;
    unsigned long *winId=(unsigned long*)arg;
    *winId = (unsigned long)s->window;
    return 0;
}

static int osx_gl_set_native_window_id(MSFilter* f, void* arg) {
    GLOSXState* s = (GLOSXState*) f->data;
    NSObject *obj = *((NSObject **)arg);
    if(obj != nil) {
        if([obj isKindOfClass:[NSWindow class]]) {
            s->window = [(NSWindow*)obj retain];
            return 0;
        } else if([obj isKindOfClass:[CALayer class]]) {
            s->layer = [(CALayer*)obj retain];
            return 0;
        }
    }
    return -1;
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
    {MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , osx_gl_set_native_window_id },
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
