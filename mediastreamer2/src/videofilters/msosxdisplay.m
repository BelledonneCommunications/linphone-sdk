/*
 msosxdisplay.m
 Copyright (C) 2011 Belledonne Communications, Grenoble, France

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
#include "opengles_display.h"

#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#import <QuartzCore/CATransaction.h>

@interface CAMsGLLayer : CAOpenGLLayer {
@public
	struct opengles_display* display_helper;
@private
	CGLPixelFormatObj cglPixelFormat;
	CGLContextObj cglContext;
	NSRecursiveLock* lock;
	CGRect prevBounds;
	CGSize sourceSize;
}

- (void)resizeToWindow:(NSWindow *)window;

@property (assign) CGSize sourceSize;

@end

@implementation CAMsGLLayer

@synthesize sourceSize;

- (id)init {
	self = [super init];
	if(self != nil) {
		self->sourceSize = CGSizeMake(0, 0);
		self->prevBounds = CGRectMake(0, 0, 0, 0);
		self->lock = [[NSRecursiveLock alloc] init];
		self->display_helper = ogl_display_new();

		[self setOpaque:YES];
		[self setAsynchronous:NO];
		[self setAutoresizingMask: kCALayerWidthSizable | kCALayerHeightSizable];
		[self setNeedsDisplayOnBoundsChange:YES];

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

		CGLContextObj savedContext = CGLGetCurrentContext();
		CGLSetCurrentContext(cglContext);
		CGLLockContext(cglContext);

		ogl_display_init(display_helper, prevBounds.size.width, prevBounds.size.height);

		CGLUnlockContext(cglContext);
		CGLSetCurrentContext(savedContext);
	}
	return self;
}

- (void)dealloc {
	CGLContextObj savedContext = CGLGetCurrentContext();
	CGLSetCurrentContext(cglContext);
	CGLLockContext(cglContext);

	ogl_display_uninit(display_helper, TRUE);
	ogl_display_free(display_helper);

	CGLUnlockContext(cglContext);
	CGLSetCurrentContext(savedContext);

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

		if (!CGRectEqualToRect(prevBounds, [self bounds])) {
			prevBounds = [self bounds];
			ogl_display_set_size(display_helper, prevBounds.size.width, prevBounds.size.height);
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

- (void)resizeToWindow:(NSWindow *)window {
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

@interface OSXDisplay : NSObject {
@private
	BOOL closeWindow;
	BOOL autoWindow;
	NSWindow* window;
	NSView* view;
	CALayer* layer;
	CAMsGLLayer* glLayer;
	int corner;
}
@property (assign) BOOL closeWindow;
@property (assign) BOOL autoWindow;
@property (nonatomic, retain) NSWindow* window;
@property (nonatomic, retain) NSView* view;
@property (nonatomic, retain) CALayer* layer;
@property (nonatomic, retain) CAMsGLLayer* glLayer;
@property (assign) int corner;

- (void)createWindowIfNeeded;
- (void)resetContainers;

@end

@implementation OSXDisplay

@synthesize closeWindow;
@synthesize autoWindow;
@synthesize window;
@synthesize view;
@synthesize layer;
@synthesize glLayer;
@synthesize corner;

- (id)init {
	self = [super init];
	if(self != nil) {
		self.glLayer = [[CAMsGLLayer alloc] init];
		window = nil;
		view = nil;
		layer = nil;
		closeWindow = FALSE;
		autoWindow = TRUE;
		corner = 0;
	}
	return self;
}

- (void)resetContainers {
	[glLayer removeFromSuperlayer];

	if(window != nil) {
		if(closeWindow) {
			[window close];
		}
		[window release];
		window = nil;
	}
	if(view != nil) {
		[view release];
		view = nil;
	}
	if(layer != nil) {
		[layer release];
		layer = nil;
	}
}

- (void)setWindow:(NSWindow*)awindow {
	if(window == awindow) {
		return;
	}

	[self resetContainers];

	if(awindow != nil) {
		window = [awindow retain];
		[glLayer setFrame:[[window.contentView layer] bounds]];
		[[window.contentView layer] addSublayer: glLayer];

		glLayer.sourceSize = CGSizeMake(0, 0); // Force window resize
	}
}

- (void)setView:(NSView*)aview {
	if(view == aview) {
		return;
	}

	[self resetContainers];

	if(aview != nil) {
		view = [aview retain];
		[view setWantsLayer:YES];
		[glLayer setFrame:[[view layer] bounds]];
		[[view layer] addSublayer: glLayer];
	}
}

- (void)setLayer:(CALayer*)alayer {
	if(layer == alayer) {
		return;
	}

	[self resetContainers];

	if(alayer != nil) {
		layer = [alayer retain];
		[glLayer setFrame:[layer bounds]];
		[layer addSublayer: glLayer];
	}
}

- (void)createWindowIfNeeded {
	if(autoWindow && window == nil && layer == nil && view == nil) {
		NSWindow *awindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100) styleMask:(NSTitledWindowMask | NSResizableWindowMask | NSClosableWindowMask) backing:NSBackingStoreBuffered defer:NO];
		[awindow setBackgroundColor: [NSColor blueColor]];
		[awindow makeKeyAndOrderFront:NSApp];
		[awindow setTitle: @"Video"];
		[awindow setMovable:YES];
		[awindow setMovableByWindowBackground:YES];
		[awindow setReleasedWhenClosed:NO];
		CGFloat xPos = NSWidth([[awindow screen] frame])/2 - NSWidth([awindow frame])/2;
		CGFloat yPos = NSHeight([[awindow screen] frame])/2 - NSHeight([awindow frame])/2;
		[awindow setFrame:NSMakeRect(xPos, yPos, NSWidth([awindow frame]), NSHeight([awindow frame])) display:YES];

		// Init view
		NSView *innerView = [[NSView alloc] initWithFrame:[window frame]];
		[innerView setWantsLayer:YES];
		[innerView.layer setAutoresizingMask: kCALayerWidthSizable | kCALayerHeightSizable];
		[innerView.layer setNeedsDisplayOnBoundsChange: YES];
		[awindow setContentView: innerView];
		[innerView release];

		self.window = awindow;
		self.closeWindow = TRUE;
	}
}

- (void)dealloc {
	[self resetContainers];
	[self.glLayer release];
	self.glLayer = nil;

	[super dealloc];
}

@end

static void osx_gl_init(MSFilter* f) {
	NSAutoreleasePool *loopPool = [[NSAutoreleasePool alloc] init];
	f->data = [[OSXDisplay alloc] init];
	[loopPool drain];
}

static void osx_gl_preprocess(MSFilter* f) {
	OSXDisplay* thiz = (OSXDisplay*) f->data;
	if (thiz != nil) {
		NSAutoreleasePool *loopPool = [[NSAutoreleasePool alloc] init];
		[thiz performSelectorOnMainThread:@selector(createWindowIfNeeded) withObject:nil waitUntilDone:FALSE];
		[loopPool drain];
	}
}

static void osx_gl_process(MSFilter* f) {
	OSXDisplay* thiz = (OSXDisplay*) f->data;
	mblk_t* m = 0;
	MSPicture pic;

	NSAutoreleasePool *loopPool = [[NSAutoreleasePool alloc] init];

	if ((m=ms_queue_peek_last(f->inputs[0])) != NULL) {
		if (ms_yuv_buf_init_from_mblk (&pic,m) == 0) {
			if (thiz != nil) {
				// Source size change?
				if (pic.w != thiz.glLayer.sourceSize.width || pic.h != thiz.glLayer.sourceSize.height) {
					thiz.glLayer.sourceSize = CGSizeMake(pic.w, pic.h);

					// Force window resize
					if(thiz.window != nil) {
						[thiz.glLayer performSelectorOnMainThread:@selector(resizeToWindow:) withObject:thiz.window waitUntilDone:FALSE];
					}
				}
				ogl_display_set_yuv_to_display(thiz.glLayer->display_helper, m);

				// Force redraw
				[thiz.glLayer performSelectorOnMainThread:@selector(setNeedsDisplay) withObject:nil waitUntilDone:FALSE];
			}
		}
	}
	ms_queue_flush(f->inputs[0]);

	if (f->inputs[1] != NULL) {
		if (thiz.corner != -1) {
			if ((m=ms_queue_peek_last(f->inputs[1])) != NULL) {
				if (ms_yuv_buf_init_from_mblk (&pic,m) == 0) {
					if (thiz != nil) {
						if (!mblk_get_precious_flag(m)) ms_yuv_buf_mirror(&pic);
						ogl_display_set_preview_yuv_to_display(thiz.glLayer->display_helper, m);

						// Force redraw
						[thiz.glLayer performSelectorOnMainThread:@selector(setNeedsDisplay) withObject:nil waitUntilDone:FALSE];
					}
				}
			}
		}else ogl_display_set_preview_yuv_to_display(thiz.glLayer->display_helper,NULL);
		ms_queue_flush(f->inputs[1]);
	}

	[loopPool drain];
}

static void osx_gl_uninit(MSFilter* f) {
	OSXDisplay* thiz = (OSXDisplay*) f->data;
	if (thiz != nil) {
		NSAutoreleasePool *loopPool = [[NSAutoreleasePool alloc] init];
		[thiz release];
		[loopPool drain];
	}
}

static int osx_gl_set_vsize(MSFilter* f, void* arg) {
	return -1;
}

static int osx_gl_get_native_window_id(MSFilter* f, void* arg) {
	OSXDisplay* thiz = (OSXDisplay*) f->data;
	unsigned long *winId = (unsigned long*)arg;
	int ret = -1;
	if(thiz != nil) {
		if(thiz.window != nil) {
			*winId = (unsigned long)thiz.window;
			ret = 0;
		} else if(thiz.view != nil) {
			*winId = (unsigned long)thiz.view;
			ret = 0;
		} else if(thiz.layer != nil) {
			*winId = (unsigned long)thiz.layer;
			ret = 0;
		} else if(thiz.autoWindow) {
			*winId = MS_FILTER_VIDEO_AUTO;
			ret = 0;
		} else {
			*winId = MS_FILTER_VIDEO_NONE;
			ret = 0;
		}
	}
	return ret;
}

static int osx_gl_set_native_window_id(MSFilter* f, void* arg) {
	OSXDisplay* thiz = (OSXDisplay*) f->data;
	unsigned long winId = *((unsigned long*)arg);
	NSObject *obj = *((NSObject **)arg);
	int ret = -1;
	if(thiz != nil) {
		NSAutoreleasePool *loopPool = [[NSAutoreleasePool alloc] init];
		if(winId != MS_FILTER_VIDEO_AUTO && winId != MS_FILTER_VIDEO_NONE) {
			if([obj isKindOfClass:[NSWindow class]]) {
				[thiz performSelectorOnMainThread:@selector(setWindow:) withObject:(NSWindow*)obj waitUntilDone:NO];
				ret = 0;
			} else if([obj isKindOfClass:[NSView class]]) {
				[thiz performSelectorOnMainThread:@selector(setView:) withObject:(NSView*)obj waitUntilDone:NO];
				ret = 0;
			} else if([obj isKindOfClass:[CALayer class]]) {
				[thiz performSelectorOnMainThread:@selector(setLayer:) withObject:(CALayer*)obj waitUntilDone:NO];
				ret = 0;
			}
		} else {
			if(winId == MS_FILTER_VIDEO_NONE) {
				thiz.autoWindow = FALSE;
			} else {
				thiz.autoWindow = TRUE;
			}
			[thiz performSelectorOnMainThread:@selector(resetContainers) withObject:nil waitUntilDone:NO];
			ret = 0;
		}
		[loopPool drain];
	}
	return ret;
}

static int osx_gl_enable_mirroring(MSFilter* f, void* arg) {
	return -1;
}

static int osx_gl_set_local_view_mode(MSFilter* f, void* arg) {
	OSXDisplay* thiz = (OSXDisplay*) f->data;
	thiz.corner = *(int*)arg;
	return 0;
}

static MSFilterMethod methods[]={
	{MS_FILTER_SET_VIDEO_SIZE, osx_gl_set_vsize},
	{MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, osx_gl_get_native_window_id},
	{MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, osx_gl_set_native_window_id},
	{MS_VIDEO_DISPLAY_ENABLE_MIRRORING, osx_gl_enable_mirroring},
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
