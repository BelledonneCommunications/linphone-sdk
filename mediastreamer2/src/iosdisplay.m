/*
 iosdisplay.m
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
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msv4l.h"
#include "mediastreamer2/mswebcam.h"
#include "mediastreamer2/mscommon.h"
#include "nowebcam.h"
#include "mediastreamer2/msfilter.h"
#include "scaler.h"

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <OpenGLES/ES2/gl.h>

#include "opengles_display.h"

@interface IOSDisplay : UIView {
@private
    UIView* imageView;
    
    EAGLContext* context;
    
    GLuint defaultFrameBuffer, colorRenderBuffer;
    struct opengles_display* helper;
    
    id displayLink;
    BOOL animating;
    int deviceRotation;
    CGRect prevBounds;
}

- (void)drawView:(id)sender;
- (BOOL)loadShaders;
- (void)initIOSDisplay;

@property (nonatomic, retain) UIView* parentView;

@end

@implementation IOSDisplay

@synthesize parentView;

- (id)init {
    self = [super init];
    if (self) {
        [self initIOSDisplay];
    }
    return self;
}

- (id)initWithCoder:(NSCoder *)coder {
    self = [super initWithCoder:coder];
    if (self) {
        [self initIOSDisplay];
    }
    return self;
}

- (id)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self initIOSDisplay];
    }
    return self;
}

- (void)initIOSDisplay {
    self->deviceRotation = 0;
    self->helper = ogl_display_new();
    self->prevBounds = CGRectMake(0, 0, 0, 0);
    
    // Init view
    [self setOpaque:YES];
    [self setAutoresizingMask: UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    
    // Init layer
    CAEAGLLayer *eaglLayer = (CAEAGLLayer*) self.layer;
    [eaglLayer setOpaque:YES];
    
    
    // Init OpenGL context
    context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (!context || ![EAGLContext setCurrentContext:context]) {
        ms_error("Opengl context failure");
        return;
    }
    
    glGenFramebuffers(1, &defaultFrameBuffer);
    
    glGenRenderbuffers(1, &colorRenderBuffer);    
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFrameBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRenderBuffer);
    
    // release GL context for this thread
    [EAGLContext setCurrentContext:nil];
}

- (void)drawView:(id)sender {    
    /* no opengl es call made when in background */ 
    if ([UIApplication sharedApplication].applicationState ==  UIApplicationStateBackground)
        return;

    @synchronized(self) {
        if (![EAGLContext setCurrentContext:context]) {
            ms_error("Failed to bind GL context");
            return;
        }
        
        if (!CGRectEqualToRect(prevBounds, [self bounds])) {
            glBindRenderbuffer(GL_RENDERBUFFER, colorRenderBuffer);
            CAEAGLLayer* layer = (CAEAGLLayer*)self.layer;
            
            if (prevBounds.size.width != 0 || prevBounds.size.height != 0) {
                // release previously allocated storage
                [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:nil];
            }
            
            prevBounds = [self bounds];
            
            // allocate storage
            if (![context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer]) {
                ms_error("Error in renderbufferStorage (layer %p frame size: %f x %f)", layer, layer.frame.size.width, layer.frame.size.height);
            } else {
                ms_message("GL renderbuffer allocation size (layer %p frame size: %f x %f)", layer, layer.frame.size.width, layer.frame.size.height);
                ogl_display_init(helper, prevBounds.size.width, prevBounds.size.height);
                
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        } 

        if (!animating) {
            glClear(GL_COLOR_BUFFER_BIT);
        } else {
            ogl_display_render(helper, 0); 
        }

        [context presentRenderbuffer:GL_RENDERBUFFER];
    }
}

- (void)setParentView:(UIView*)aparentView{
    if (parentView == aparentView) {
        return;
    }
    
    if(parentView != nil) {
        animating = FALSE;
        
        // stop schedule rendering
        [displayLink invalidate];
        displayLink = nil;
        
        [self drawView:0];
        
        // remove from parent
        [self removeFromSuperview];
        
        [parentView release];
        parentView = nil;
    }
    
    parentView = aparentView;
    
    if(parentView != nil) {
        [parentView retain];
        animating = TRUE;
        
        // add to new parent
        [self setFrame: [parentView bounds]];
        [parentView addSubview:self];
        
        // schedule rendering
        displayLink = [self.window.screen displayLinkWithTarget:self selector:@selector(drawView:)];
        [displayLink setFrameInterval:1];
        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    }
}

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

static void iosdisplay_init(MSFilter *f) {
    f->data = [[IOSDisplay alloc] initWithFrame:CGRectMake(0, 0, 0, 0)];
}

- (void)dealloc {
    [EAGLContext setCurrentContext:context];
    glFinish();
    ogl_display_uninit(helper, TRUE);
    ogl_display_free(helper);
    helper = NULL;
    [EAGLContext setCurrentContext:0];

    [context release];
    self.parentView = nil;
    
    [super dealloc];
}

static void iosdisplay_process(MSFilter *f) {
	IOSDisplay* thiz = (IOSDisplay*)f->data;
	mblk_t *m = ms_queue_peek_last(f->inputs[0]);
    
    if (thiz != nil && m != nil) {
        ogl_display_set_yuv_to_display(thiz->helper, m);
    }
    
    ms_queue_flush(f->inputs[0]);
    if (f->inputs[1])
        ms_queue_flush(f->inputs[1]);
}

static void iosdisplay_unit(MSFilter *f) {
    IOSDisplay* thiz = (IOSDisplay*)f->data;
    
    if(thiz != nil) {
        [thiz performSelectorOnMainThread:@selector(setParentView:) withObject:nil waitUntilDone:NO];
        [thiz release];
        f->data = NULL;
    }
}

static int iosdisplay_set_native_window(MSFilter *f, void *arg) {
    IOSDisplay *thiz = (IOSDisplay*)f->data;
    UIView* parentView = *(UIView**)arg;
    if (thiz != nil) {       
        // set current parent view
        if (parentView) {
            [thiz performSelectorOnMainThread:@selector(setParentView:) withObject:parentView waitUntilDone:NO];
        }
    }
    return 0;
}

static int iosdisplay_get_native_window(MSFilter *f, void *arg) {
    IOSDisplay* thiz=(IOSDisplay*)f->data;
    arg = &thiz->parentView;
    return 0;
}

static int iosdisplay_set_device_orientation(MSFilter* f, void* arg) {
    IOSDisplay* thiz=(IOSDisplay*)f->data;
    if (!thiz)
        return 0;
    //thiz->deviceRotation = 0;//*((int*)arg);
    return 0;
}

static int iosdisplay_set_device_orientation_display(MSFilter* f, void* arg) {
    IOSDisplay* thiz=(IOSDisplay*)f->data;
    if (!thiz)
        return 0;
    thiz->deviceRotation = *((int*)arg);
    return 0;
}

static int iosdisplay_set_zoom(MSFilter* f, void* arg) {
    IOSDisplay* thiz=(IOSDisplay*)f->data;
    ogl_display_zoom(thiz->helper, arg);
}

static MSFilterMethod iosdisplay_methods[] = {
	{ MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, iosdisplay_set_native_window },
    { MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, iosdisplay_get_native_window },
    { MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION, iosdisplay_set_device_orientation },
    { MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION, iosdisplay_set_device_orientation_display },
    { MS_VIDEO_DISPLAY_ZOOM, iosdisplay_set_zoom },
	{ 0, NULL }
};

@end

MSFilterDesc ms_iosdisplay_desc = {
	.id=MS_IOS_DISPLAY_ID, /* from Allfilters.h*/
	.name="IOSDisplay",
	.text="IOS Display filter.",
	.category=MS_FILTER_OTHER,
	.ninputs=2, /*number of inputs*/
	.noutputs=0, /*number of outputs*/
	.init=iosdisplay_init,
	.preprocess=NULL,
	.process=iosdisplay_process,
	.postprocess=NULL,
	.uninit=iosdisplay_unit,
	.methods=iosdisplay_methods
};
MS_FILTER_DESC_EXPORT(ms_iosdisplay_desc)
