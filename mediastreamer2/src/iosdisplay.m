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

#import <AVFoundation/AVFoundation.h>

#import "iosdisplay.h"
#include "mediastreamer2/msfilter.h"
#include "scaler.h"


@interface IOSDisplay (PrivateMethods)
- (BOOL) loadShaders;
- (void) initGlRendering;
@end

@implementation IOSDisplay

@synthesize imageView;

- (id)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self initGlRendering];
    }
    return self;
}

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self initGlRendering];
    }
    return self;
}

- (void)initGlRendering
{
    self->helper = ogl_display_new();
    
    // Initialization code
    CAEAGLLayer *eaglLayer = (CAEAGLLayer*) self.layer;
    eaglLayer.opaque = TRUE;
    
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
    
    glClear(GL_COLOR_BUFFER_BIT);
    
    // release GL context for this thread
    [EAGLContext setCurrentContext:nil];
    
    glInitDone = FALSE;
    allocatedW = allocatedH = 0;
    deviceRotation = 0;
}

- (void) drawView:(id)sender
{    
    /* no opengl es call made when in background */ 
    if ([UIApplication sharedApplication].applicationState ==  UIApplicationStateBackground)
        return;

    @synchronized(self) {
    if (![EAGLContext setCurrentContext:context])
    {
        ms_error("Failed to bind GL context");
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFrameBuffer);

    if (!glInitDone) {
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        ogl_display_render(helper, deviceRotation);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderBuffer);

    [context presentRenderbuffer:GL_RENDERBUFFER];
    }
}

- (void) layoutSubviews
{
    @synchronized(self) {
    if (!(allocatedW == self.superview.frame.size.width && allocatedH == self.superview.frame.size.height)) {
        if (![EAGLContext setCurrentContext:context]) {
            ms_error("Failed to set EAGLContext - expect issues");
        }
        glFinish();
        glBindRenderbuffer(GL_RENDERBUFFER, colorRenderBuffer);
        CAEAGLLayer* layer = (CAEAGLLayer*)self.layer;
        
        if (allocatedW != 0 || allocatedH != 0) {
            // release previously allocated storage
            [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:nil];
            allocatedW = allocatedH = 0;
        }
        // allocate storage
        if (![context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer]) {
            ms_error("Error in renderbufferStorage (layer %p frame size: %f x %f)", layer, layer.frame.size.width, layer.frame.size.height);
        } else {
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &allocatedW);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &allocatedH);
            ms_message("GL renderbuffer allocation size: %dx%d (layer frame size: %f x %f)", allocatedW, allocatedH, layer.frame.size.width, layer.frame.size.height);
            ogl_display_init(helper, self.superview.frame.size.width, self.superview.frame.size.height);
        
            glClear(GL_COLOR_BUFFER_BIT);
        }
    } else {
        ogl_display_init(helper, self.superview.frame.size.width, self.superview.frame.size.height);
    }
    }
    glInitDone = TRUE;
}

- (void) startRendering: (id)ignore
{
    if (!animating)
    {
        if (self.superview != self.imageView) {
            // remove from old parent
            [self removeFromSuperview];
            // add to new parent
            [self.imageView addSubview:self];
        }
        // handle GL/view interaction
        [self layoutSubviews];
        // schedule rendering
        displayLink = [self.window.screen displayLinkWithTarget:self selector:@selector(drawView:)];
        [displayLink setFrameInterval:4];
        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        animating = TRUE;
    }
}

- (void) stopRendering: (id)ignore
{
    if (animating)
    {
        [displayLink release];
        displayLink = nil;
        animating = FALSE;
        
        [self removeFromSuperview];
    }
}

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

static void iosdisplay_init(MSFilter *f){
    //IOSDisplay* thiz = [[IOSDisplay alloc] init];
    //[thiz initGlRendering];
    //f->data = thiz;
    //f->data = nil;
}
-(void) dealloc {
    [EAGLContext setCurrentContext:context];
    glFinish();
    ogl_display_uninit(helper, TRUE);
    ogl_display_free(helper);
    helper = NULL;
    [EAGLContext setCurrentContext:0];

    [context release];
    [imageView release];
    
    [super dealloc];
}

static void iosdisplay_process(MSFilter *f){
	IOSDisplay* thiz=(IOSDisplay*)f->data;
	mblk_t *m=ms_queue_peek_last(f->inputs[0]);
    
    if (thiz != nil && m != nil) {
        ogl_display_set_yuv_to_display(thiz->helper, m);
    }
    
    ms_queue_flush(f->inputs[0]);
    if (f->inputs[1])
        ms_queue_flush(f->inputs[1]);
}

static void iosdisplay_unit(MSFilter *f){
    IOSDisplay* thiz=(IOSDisplay*)f->data;

    [thiz performSelectorOnMainThread:@selector(stopRendering:) withObject:nil waitUntilDone:YES];
    
    [thiz release];
}

/*filter specific method*/
/*  This methods declare the PARENT window of the opengl view.
    We'll create on gl view for once, and then simply change its parent. 
    This works only if parent size is the size in all possible orientation.
*/
static int iosdisplay_set_native_window(MSFilter *f, void *arg) {
    UIView* parentView = *(UIView**)arg;
    IOSDisplay* thiz;
    
    if (f->data != nil) {
        NSLog(@"OpenGL view parent changed.");
        thiz = f->data;
        thiz.frame = CGRectMake(0, 0, parentView.frame.size.width, parentView.frame.size.height);
        [thiz performSelectorOnMainThread:@selector(stopRendering:) withObject:nil waitUntilDone:NO];
    } else if (parentView == nil) {
        return 0;
    } else {
        thiz = f->data = [[IOSDisplay alloc] initWithFrame:CGRectMake(0, 0, parentView.frame.size.width, parentView.frame.size.height)];
    }
    thiz.imageView = parentView;
    [thiz performSelectorOnMainThread:@selector(startRendering:) withObject:nil waitUntilDone:NO];

    return 0;
}

static int iosdisplay_get_native_window(MSFilter *f, void *arg) {
    IOSDisplay* thiz=(IOSDisplay*)f->data;
    arg = &thiz->imageView;
    return 0;
}

static int iosdisplay_set_device_orientation(MSFilter* f, void* arg) {
    IOSDisplay* thiz=(IOSDisplay*)f->data;
    if (!thiz)
        return 0;
    thiz->deviceRotation = *((int*)arg);
    return 0;
}


static MSFilterMethod iosdisplay_methods[]={
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , iosdisplay_set_native_window },
    {	MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID , iosdisplay_get_native_window },
    {	MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION,        iosdisplay_set_device_orientation },
	{	0, NULL}
};
@end

MSFilterDesc ms_iosdisplay_desc={
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
