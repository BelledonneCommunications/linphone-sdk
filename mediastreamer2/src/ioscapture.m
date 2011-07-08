/*
 ioscapture.m
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


@interface IOSMsWebCam :NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> {
@private
    AVCaptureDeviceInput *input;
	AVCaptureSession *session;
	AVCaptureVideoDataOutput * output;
	ms_mutex_t mutex;
	queue_t rq;
	int frame_ind;
	float fps;
	float start_time;
	int frame_count;
	MSVideoSize mCaptureSize;
};


-(void) dealloc;
-(int) start;
-(int) stop;
-(void) setSize:(MSVideoSize) size;
-(MSVideoSize*) getSize;
-(void) openDevice:(const char*) deviceId;

@end


@implementation IOSMsWebCam 

- (void)captureOutput:(AVCaptureOutput *)captureOutput 
didOutputSampleBuffer:(CMSampleBufferRef) sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
	ms_mutex_lock(&mutex);	
    CVImageBufferRef frame = CMSampleBufferGetImageBuffer(sampleBuffer); 
	
	MSPicture pict;
    mblk_t *yuv_block = ms_yuv_buf_alloc(&pict, mCaptureSize.width, mCaptureSize.height);
	CVReturn status = CVPixelBufferLockBaseAddress(frame, 0);
	if (kCVReturnSuccess != status) {
		ms_error("Error locking base address: %i", status);
		return;
	}
    //FIXME center image before cropping
	/*kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange*/
   
    size_t plane_width = MIN(CVPixelBufferGetWidthOfPlane(frame, 0),mCaptureSize.width);
    size_t plane_height = MIN(CVPixelBufferGetHeightOfPlane(frame, 0),mCaptureSize.height);
	
	yuv_block = copy_ycbcrbiplanar_to_true_yuv_portrait(CVPixelBufferGetBaseAddressOfPlane(frame, 0)
														, CVPixelBufferGetBaseAddressOfPlane(frame, 1)
														, 90
														, plane_width
														, plane_height
														, CVPixelBufferGetBytesPerRowOfPlane(frame, 0)
														,CVPixelBufferGetBytesPerRowOfPlane(frame, 1));
	
    CVPixelBufferUnlockBaseAddress(frame, 0);  
    putq(&rq, yuv_block);
	ms_mutex_unlock(&mutex);
	
}
-(void) openDevice:(const char*) deviceId {
	NSError *error = nil;
	unsigned int i = 0;
	AVCaptureDevice * device = NULL;
    
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];	
	NSArray * array = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
	for (i = 0 ; i < [array count]; i++) {
		AVCaptureDevice * currentDevice = [array objectAtIndex:i];
		if(!strcmp([[currentDevice uniqueID] UTF8String], deviceId)) {
			device = currentDevice;
			break;
		}
	}
	if (device == NULL) {
		ms_error("Error: camera %s not found, using default one", deviceId);
		device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
	}
	input = [AVCaptureDeviceInput deviceInputWithDevice:device
                                                  error:&error];
	
	[session addInput:input];
	[session addOutput:output ];
	NSArray *connections = output.connections;
	if ([connections count] > 0 && [[connections objectAtIndex:0] isVideoOrientationSupported]) {
		[[connections objectAtIndex:0] setVideoOrientation:AVCaptureVideoOrientationPortrait];
		ms_message("Configuring camera in portrait mode");
	}
	[pool drain];
}

-(id) init {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	qinit(&rq);
	ms_mutex_init(&mutex,NULL);
	session = [[AVCaptureSession alloc] init];
    output = [[AVCaptureVideoDataOutput  alloc] init];
	/*
     Currently, the only supported key is kCVPixelBufferPixelFormatTypeKey. Supported pixel formats are kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, kCVPixelFormatType_420YpCbCr8BiPlanarFullRange and kCVPixelFormatType_32BGRA, except on iPhone 3G, where the supported pixel formats are kCVPixelFormatType_422YpCbCr8 and kCVPixelFormatType_32BGRA..     
     */
	NSDictionary* dic = [NSDictionary dictionaryWithObjectsAndKeys:
						 [NSNumber numberWithInteger:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange], (id)kCVPixelBufferPixelFormatTypeKey, nil];
	[output setVideoSettings:dic];
	
    dispatch_queue_t queue = dispatch_queue_create("myQueue", NULL);
    [output setSampleBufferDelegate:self queue:queue];
    dispatch_release(queue);
	start_time=0;
	frame_count=-1;
	fps=15;
	return self;
	[pool drain];
	
}

-(void) dealloc {
	[self stop];
	if (session) {		
		[session release];
		session = nil;
	}
	if (input) {
		[input release];
		input = nil;
	}
    if (output) {
		[output release];
		output = nil;
	}
	flushq(&rq,0);
	ms_mutex_destroy(&mutex);
	[super dealloc];
}

-(int) start {
	[session startRunning];
	ms_message("v4ios video device started.");
	return 0;
}

-(int) stop {
	[session stopRunning];
	ms_message("v4ios video device closed.");
	return 0;
}





-(void) setSize:(MSVideoSize) size {
	[session beginConfiguration];
	if (size.width >=(MS_VIDEO_SIZE_QVGA_H + MS_VIDEO_SIZE_HVGA_W)/2) {
		[session setSessionPreset: AVCaptureSessionPreset640x480];
		mCaptureSize.width=MS_VIDEO_SIZE_HVGA_W;
		mCaptureSize.height=MS_VIDEO_SIZE_HVGA_H;		
	} else 	 {
		[session setSessionPreset: AVCaptureSessionPresetMedium];//480x360 3GS
		mCaptureSize.width=MS_VIDEO_SIZE_QVGA_H;
		mCaptureSize.height=MS_VIDEO_SIZE_QVGA_W;
	} /*else {
	   mCaptureSize.width=MS_VIDEO_SIZE_HQQVGA_W;
	   mCaptureSize.height=MS_VIDEO_SIZE_HQQVGA_H;		
	   [session setSessionPreset: AVCaptureSessionPresetLow];	//192*144 3GS
	   }*/
	[session commitConfiguration];
    return;
}

-(MSVideoSize*) getSize {
	return &mCaptureSize;
}

//filter methods

static void v4ios_init(MSFilter *f){
	f->data=[[IOSMsWebCam alloc] init];
}

static void v4ios_uninit(MSFilter *f){
	IOSMsWebCam *webcam=(IOSMsWebCam*)f->data;
	[webcam stop];
	[webcam release];
}

static void v4ios_process(MSFilter * obj){
	IOSMsWebCam *webcam=(IOSMsWebCam*)obj->data;
	uint32_t timestamp;
	int cur_frame;
	if (webcam->frame_count==-1){
		webcam->start_time=obj->ticker->time;
		webcam->frame_count=0;
	}
	
	ms_mutex_lock(&webcam->mutex);
	
	cur_frame=((obj->ticker->time-webcam->start_time)*webcam->fps/1000.0);
	if (cur_frame>=webcam->frame_count)
	{
		mblk_t *om=NULL;
		/*keep the most recent frame if several frames have been captured */
		if ([webcam->session isRunning])
		{
			om=getq(&webcam->rq);
		}
		
		if (om!=NULL)
		{
			timestamp=obj->ticker->time*90;/* rtp uses a 90000 Hz clockrate for video*/
			mblk_set_timestamp_info(om,timestamp);
			mblk_set_marker_info(om,TRUE);	
			ms_queue_put(obj->outputs[0],om);
			webcam->frame_count++;
		}
	}
	else 
		flushq(&webcam->rq,0);
	
	ms_mutex_unlock(&webcam->mutex);
}

static void v4ios_preprocess(MSFilter *f){
	IOSMsWebCam *webcam=(IOSMsWebCam*)f->data;
	[webcam start];
}

static void v4ios_postprocess(MSFilter *f){
	IOSMsWebCam *webcam=(IOSMsWebCam*)f->data;
	[webcam stop];
}

/*static int v4ios_set_fps(MSFilter *f, void *arg){
 v4iosState *s=(v4iosState*)f->data;
 webcam->fps=*((float*)arg);
 webcam->frame_count=-1;
 return 0;
 }
 */
static int v4ios_get_pix_fmt(MSFilter *f,void *arg){
    *(MSPixFmt*)arg=MS_YUV420P;
	return 0;
}

static int v4ios_set_vsize(MSFilter *f, void *arg){
	IOSMsWebCam *webcam=(IOSMsWebCam*)f->data;
	[webcam setSize:*((MSVideoSize*)arg)];
	return 0;
}

static int v4ios_get_vsize(MSFilter *f, void *arg){
	IOSMsWebCam *webcam=(IOSMsWebCam*)f->data;
	*(MSVideoSize*)arg = *[webcam getSize];
	return 0;
}

static MSFilterMethod methods[]={
	//	{	MS_FILTER_SET_FPS		,	v4ios_set_fps		},
	{	MS_FILTER_GET_PIX_FMT	,	v4ios_get_pix_fmt	},
	{	MS_FILTER_SET_VIDEO_SIZE, 	v4ios_set_vsize	},
	{	MS_FILTER_GET_VIDEO_SIZE,	v4ios_get_vsize	},
	{	0						,	NULL			}
};

MSFilterDesc ms_v4ios_desc={
	.id=MS_V4L_ID,
	.name="MSv4ios",
	.text="A video for IOS compatible source filter to stream pictures.",
	.ninputs=0,
	.noutputs=1,
	.category=MS_FILTER_OTHER,
	.init=v4ios_init,
	.preprocess=v4ios_preprocess,
	.process=v4ios_process,
	.postprocess=v4ios_postprocess,
	.uninit=v4ios_uninit,
	.methods=methods
};

MS_FILTER_DESC_EXPORT(ms_v4ios_desc)

static void ms_v4ios_detect(MSWebCamManager *obj);

static void ms_v4ios_cam_init(MSWebCam *cam){
}


static MSFilter *ms_v4ios_create_reader(MSWebCam *obj)
{	
	MSFilter *f= ms_filter_new_from_desc(&ms_v4ios_desc); 
	[((IOSMsWebCam*)f->data) openDevice:obj->data];
	return f;
}

MSWebCamDesc ms_v4ios_cam_desc={
	"AV Capture",
	&ms_v4ios_detect,
	&ms_v4ios_cam_init,
	&ms_v4ios_create_reader,
	NULL
};


static void ms_v4ios_detect(MSWebCamManager *obj){
	
	unsigned int i = 0;
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	
	NSArray * array = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
	
	for(i = 0 ; i < [array count]; i++)
	{
		AVCaptureDevice * device = [array objectAtIndex:i];
		MSWebCam *cam=ms_web_cam_new(&ms_v4ios_cam_desc);
		cam->name= ms_strdup([[device localizedName] UTF8String]);
		cam->data = ms_strdup([[device uniqueID] UTF8String]);
		ms_web_cam_manager_add_cam(obj,cam);
	}
	[myPool drain];
}

@end