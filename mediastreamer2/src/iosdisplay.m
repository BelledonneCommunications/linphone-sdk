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
#include "nowebcam.h"

#import <AVFoundation/AVFoundation.h>

#import "iosdisplay.h"
#include "mediastreamer2/msfilter.h"
#include "scaler.h"


@implementation IOSDisplay


-(void) updateImage:(UIImage*) image {
	[imageView setImage:image];
	[image release];
}
static void iosdisplay_init(MSFilter *f){
    f->data = [[IOSDisplay alloc] init]  ;
}
-(void) dealloc {
	[super dealloc];
	[imageView release];
	imageView = nil;
}

static void iosdisplay_process(MSFilter *f){
	IOSDisplay* thiz=(IOSDisplay*)f->data;
	MSPicture yuvbuf;
	mblk_t *m=ms_queue_peek_last(f->inputs[0]);
	if (m && ms_yuv_buf_init_from_mblk(&yuvbuf,m)==0){
		CVPixelBufferRef imageBuffer = 0;
		// create pixel buffer
		int ret = CVPixelBufferCreate(kCFAllocatorDefault
							, yuvbuf.w
							, yuvbuf.h
							, kCVPixelFormatType_32BGRA
							, nil
							, &imageBuffer);		
	if (ret != kCVReturnSuccess) {
			ms_error("CVPixelBufferCreateWithBytes() failed : %d", ret);
			goto end;
		}
		CVPixelBufferLockBaseAddress(imageBuffer, 0); 
		uint32_t* buff = CVPixelBufferGetBaseAddress(imageBuffer);
		img_ycrcb420p_to_bgra(yuvbuf.planes,yuvbuf.w,yuvbuf.h,buff);
		// Unlock the pixel buffer
		CVPixelBufferUnlockBaseAddress(imageBuffer,0);
		// Create a device-dependent RGB color space
       CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB(); 
        // Lock the base address of the pixel buffer
		CVPixelBufferLockBaseAddress(imageBuffer, 0); 
		// Get the number of bytes per row for the pixel buffer
		void *baseAddress = CVPixelBufferGetBaseAddress(imageBuffer); 
       // Create a bitmap graphics context with the sample buffer data
       CGContextRef context = CGBitmapContextCreate(baseAddress
													, yuvbuf.w 
													, yuvbuf.h
													,8
													,yuvbuf.w*4
													,colorSpace
													, kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst); 
        // Create a Quartz image from the pixel data in the bitmap graphics context
        CGImageRef quartzImage = CGBitmapContextCreateImage(context); 
		
		// Unlock the pixel buffer
		CVPixelBufferUnlockBaseAddress(imageBuffer,0);
		
		// Free up the context and color space
		CGContextRelease(context); 
		CGColorSpaceRelease(colorSpace);
		
		// Create an image object from the Quartz image
		UIImage *image = [[UIImage alloc ]initWithCGImage:quartzImage] ;
		
		[thiz performSelectorOnMainThread:@selector(updateImage:) withObject:image waitUntilDone:NO];
		//[thiz updateImage:image];
		// Release the Quartz image
		CGImageRelease(quartzImage);
		CVPixelBufferRelease(imageBuffer);
    }
end:	
    ms_queue_flush(f->inputs[0]);
	ms_queue_flush(f->inputs[1]);
}

static void iosdisplay_unit(MSFilter *f){
    [(IOSDisplay*)(f->data) release];
}


/*filter specific method*/

static int iosdisplay_set_native_window(MSFilter *f, void *arg) {
    IOSDisplay* thiz=(IOSDisplay*)f->data;
    thiz->imageView = *(UIImageView**)(arg);
	[thiz->imageView retain];
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
