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
#import "shaders.h"

enum {
    UNIFORM_MATRIX,
    UNIFORM_TEXTURE_Y,
    UNIFORM_TEXTURE_U,
    UNIFORM_TEXTURE_V,
    NUM_UNIFORMS
};
GLint uniforms[NUM_UNIFORMS];

enum {
    ATTRIB_VERTEX,
    ATTRIB_UV,
    NUM_ATTRIBS
};

enum {
    Y,
    U,
    V
};

@interface IOSDisplay (PrivateMethods)
- (BOOL) loadShaders;
- (void) initGlRendering;
@end

@implementation IOSDisplay

void checkGlErrors() {
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR)
    {
        
        switch(error)
        {
            case GL_INVALID_ENUM:  ms_error("GL error: GL_INVALID_ENUM\n"); break;
            case GL_INVALID_VALUE: ms_error("GL error: GL_INVALID_VALUE\n"); break;
            case GL_INVALID_OPERATION: ms_error("GL error: GL_INVALID_OPERATION\n"); break;
            case GL_OUT_OF_MEMORY: ms_error("GL error: GL_OUT_OF_MEMORY\n"); break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: ms_error("GL error: GL_INVALID_FRAMEBUFFER_OPERATION\n"); break;
            default:
                ms_error("GL error: %x\n", error);
        }
    }
}

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
    latestYuv = nil;
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
    
    glDisable(GL_DEPTH_TEST);
    checkGlErrors();
    /* multisampling */
#if 0
    glGenFramebuffers(1, &sampleFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sampleFrameBuffer);
    glGenRenderbuffers(1, &sampleColorRenderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, sampleColorRenderBuffer);
    glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, 4, GL_RGBA8_OES, 320, 460);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sampleColorRenderBuffer);
    /*
    GLuint drb;
    glGenRenderbuffers(1, &drb);
    glBindRenderbuffer(GL_RENDERBUFFER, drb);
    glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, 16, GL_DEPTH_COMPONENT16, 320, 460);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, drb);
    */
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        ms_error("Failed to make complete FB : %x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));
    
    checkGlErrors();
#endif

    // init textures
    glGenTextures(3, textures);
    for(int i=Y; i<=V; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        int blockShift = (i==Y)?0:1;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 240 >> blockShift, 320 >> blockShift, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nil);
    }
    
    [self loadShaders];

    animating = FALSE; 
    checkGlErrors();
    
    // release GL context for this thread
    [EAGLContext setCurrentContext:nil];
}

void loadOrtho(float left, float right, float bottom, float top, float near, float far, float* mat)
{
    float r_l = right - left;
    float t_b = top - bottom;
    float f_n = far - near;
    float tx = - (right + left) / (right - left);
    float ty = - (top + bottom) / (top - bottom);
    float tz = - (far + near) / (far - near);
    
    mat[0] = 2.0f / r_l;
    mat[1] = mat[2] = mat[3] = 0.0f;
    
    mat[4] = 0.0f;
    mat[5] = 2.0f / t_b;
    mat[6] = mat[7] = 0.0f;
    
    mat[8] = mat[9] = 0.0f;
    mat[10] = -2.0f / f_n;
    mat[11] = 0.0f;
    
    mat[12] = tx;
    mat[13] = ty;
    mat[14] = tz;
    mat[15] = 1.0f;
}

- (void) drawView:(id)sender
{
    if (latestYuv == nil)
        return;
    
    if (![EAGLContext setCurrentContext:context])
    {
        ms_error("Failed to bind GL context");
        return;
    }
    
    checkGlErrors();
    
#if 0
    glBindFramebuffer(GL_FRAMEBUFFER, sampleFrameBuffer);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFrameBuffer);
#endif
    
    glViewport(0, 0, backingWidth, backingHeight);

    const GLfloat squareVertices[] = {
        0, 0,
        1, 0,
        0, 1,
        1, 1
    };
    const GLfloat squareUvs[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f
    };
    
    MSPicture yuvbuf;
    ms_yuv_buf_init_from_mblk(&yuvbuf, latestYuv);
    if (yuvbuf.w == 0 && yuvbuf.h == 0) {
        ms_warning("Incoherent image size: %dx%d\n", yuvbuf.w, yuvbuf.h);
        return;
    }

    glUseProgram(program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[Y]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, yuvbuf.w, yuvbuf.h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvbuf.planes[Y]);
    glUniform1i(uniforms[UNIFORM_TEXTURE_Y], 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[U]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, yuvbuf.w >> 1, yuvbuf.h >> 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvbuf.planes[U]);
    glUniform1i(uniforms[UNIFORM_TEXTURE_U], 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textures[V]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, yuvbuf.w >> 1, yuvbuf.h >> 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvbuf.planes[V]);
    glUniform1i(uniforms[UNIFORM_TEXTURE_V], 2);

    GLfloat mat[16];    
    loadOrtho(0, 1.0f, 0, 1, 0, 1, mat);
    glUniformMatrix4fv(uniforms[UNIFORM_MATRIX], 1, GL_FALSE, mat);

    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    glVertexAttribPointer(ATTRIB_UV, 2, GL_FLOAT, 1, 0, squareUvs);
    glEnableVertexAttribArray(ATTRIB_UV);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#if 0
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE, defaultFrameBuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_APPLE, sampleFrameBuffer);
    
    glResolveMultisampleFramebufferAPPLE();
#endif

    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderBuffer);

    [context presentRenderbuffer:GL_RENDERBUFFER];
    
    latestYuv = nil;
}

- (void) layoutSubviews
{
    [EAGLContext setCurrentContext:context];
    
    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderBuffer);
    CAEAGLLayer* layer = (CAEAGLLayer*)self.layer;
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);
    
    [self drawView:nil];
}

- (void) startRendering: (id)ignore
{
    if (!animating)
    {
        [self->imageView addSubview:self];
        [self layoutSubviews];
        
        displayLink = [self.window.screen displayLinkWithTarget:self selector:@selector(drawView:)];
        [displayLink setFrameInterval:4];

        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        animating = TRUE;
    }
}

- (void) stopRendering
{
    if (animating)
    {
        [displayLink release];
        displayLink = nil;
        animating = TRUE;
        
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
	[super dealloc];
	[imageView release];
	imageView = nil;
}

-(BOOL) loadShaders {
#include "yuv2rgb.vs.h"
#include "yuv2rgb.fs.h"
    GLuint vertShader, fragShader;
    program = glCreateProgram();

    if (!compileShader(&vertShader, GL_VERTEX_SHADER, YUV2RGB_VERTEX_SHADER))
        return NO;
    if (!compileShader(&fragShader, GL_FRAGMENT_SHADER, YUV2RGB_FRAGMENT_SHADER))
        return NO;
    
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    
    glBindAttribLocation(program, ATTRIB_VERTEX, "position");
    glBindAttribLocation(program, ATTRIB_UV, "uv");
    
    if (!linkProgram(program))
        return NO;
    
    uniforms[UNIFORM_MATRIX] = glGetUniformLocation(program, "matrix");
    uniforms[UNIFORM_TEXTURE_Y] = glGetUniformLocation(program, "t_texture_y");
    uniforms[UNIFORM_TEXTURE_U] = glGetUniformLocation(program, "t_texture_u");
    uniforms[UNIFORM_TEXTURE_V] = glGetUniformLocation(program, "t_texture_v");
    
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    
    return YES;
}

static void iosdisplay_process(MSFilter *f){
	IOSDisplay* thiz=(IOSDisplay*)f->data;
	mblk_t *m=ms_queue_peek_last(f->inputs[0]);
    
    if (thiz != nil && m != nil) {
        /* keep 'm' from future use 
         => remove it from queue before flush */
        ms_queue_remove(f->inputs[0], m);
        if (thiz->latestYuv)
            ms_free(thiz->latestYuv);
        thiz->latestYuv = m;
    }
    
    
    ms_queue_flush(f->inputs[0]);
	ms_queue_flush(f->inputs[1]);
}

static void iosdisplay_unit(MSFilter *f){
    [(IOSDisplay*)(f->data) release];
}

/*filter specific method*/

static int iosdisplay_set_native_window(MSFilter *f, void *arg) {
    UIView* parentView = *(UIView**)arg;
    IOSDisplay* thiz = [[IOSDisplay alloc] initWithFrame:[parentView bounds]];

    f->data = thiz;
    thiz->imageView = parentView;
    
    [thiz performSelectorOnMainThread:@selector(startRendering:) withObject:nil waitUntilDone:NO];

    return 0;
}

static int iosdisplay_get_native_window(MSFilter *f, void *arg) {
    IOSDisplay* thiz=(IOSDisplay*)f->data;
    arg = &thiz->imageView;
    return 0;
}


static MSFilterMethod iosdisplay_methods[]={
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , iosdisplay_set_native_window },
    {	MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID , iosdisplay_get_native_window },
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
