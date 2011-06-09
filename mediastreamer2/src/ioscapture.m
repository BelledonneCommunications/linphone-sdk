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


struct v4IosState;

@interface IOSMsWebCam :NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> {
@private
    AVCaptureDeviceInput *input;
	AVCaptureSession *session;
	AVCaptureVideoDataOutput * output;
	ms_mutex_t mutex;
	queue_t rq;
};


-(void) dealloc;
-(int) start;
-(int) stop;
-(void) setSize:(MSVideoSize) size;
-(MSVideoSize) getSize;
-(void) openDevice:(const char*) deviceId;


-(AVCaptureSession*) session;
-(queue_t*) rq;
-(ms_mutex_t *) mutex;

@end





@implementation IOSMsWebCam 

- (void)captureOutput:(AVCaptureOutput *)captureOutput 
didOutputSampleBuffer:(CMSampleBufferRef) sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	ms_mutex_lock(&mutex);	
    CVImageBufferRef frame = CMSampleBufferGetImageBuffer(sampleBuffer); 

	MSPicture pict;
	size_t w = CVPixelBufferGetWidth(frame);
	size_t h = CVPixelBufferGetHeight(frame);
    mblk_t *yuv_block = ms_yuv_buf_alloc(&pict, w, h);
    size_t numberOfPlanes = CVPixelBufferGetPlaneCount(frame);
	CVReturn status = CVPixelBufferLockBaseAddress(frame, 0);
	if (kCVReturnSuccess != status) {
		ms_error("Error locking base address: %i", status);
		return;
	}
    int p;
    for (p=0; p < numberOfPlanes; p++) {
        size_t fullrow_width = CVPixelBufferGetBytesPerRowOfPlane(frame, p);
        size_t plane_width = CVPixelBufferGetWidthOfPlane(frame, p);
        size_t plane_height = CVPixelBufferGetHeightOfPlane(frame, p);
        uint8_t *dst_plane = pict.planes[p];
        uint8_t *src_plane = CVPixelBufferGetBaseAddressOfPlane(frame, p);
        //			ms_message("CVPixelBuffer %ix%i; Plane %i %ix%i (%i)", w, h, p, plane_width, plane_height, fullrow_width);
        int l;
        for (l=0; l<plane_height; l++) {
            memcpy(dst_plane, src_plane, plane_width);
            src_plane += fullrow_width;
            dst_plane += plane_width;
        }
    }
    CVPixelBufferUnlockBaseAddress(frame, 0);   
    putq(&rq, yuv_block);
	ms_mutex_unlock(&mutex);
	[myPool drain];
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
	[pool drain];
}

-(id) init {
	qinit(&rq);
	ms_mutex_init(&mutex,NULL);
	session = [[AVCaptureSession alloc] init];
    [[AVCaptureVideoDataOutput  alloc] init];
    dispatch_queue_t queue = dispatch_queue_create("myQueue", NULL);
    [output setSampleBufferDelegate:self queue:queue];
    dispatch_release(queue);
	return self;
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
	return 0;
}

-(int) stop {
	[session stopRunning];
	return 0;
}





-(void) setSize:(MSVideoSize) size {
	/*
     Currently, the only supported key is kCVPixelBufferPixelFormatTypeKey. Supported pixel formats are kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, kCVPixelFormatType_420YpCbCr8BiPlanarFullRange and kCVPixelFormatType_32BGRA, except on iPhone 3G, where the supported pixel formats are kCVPixelFormatType_422YpCbCr8 and kCVPixelFormatType_32BGRA..     
     */
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSDictionary* dic = [NSDictionary dictionaryWithObjectsAndKeys:
		 [NSNumber numberWithInteger:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange], (id)kCVPixelBufferPixelFormatTypeKey, nil];
	[output setVideoSettings:dic];
	[pool drain];
    return;
}

-(MSVideoSize) getSize
{
	MSVideoSize size;
	
	size.width = MS_VIDEO_SIZE_QCIF_W;
	size.height = MS_VIDEO_SIZE_QCIF_H;
	//size is unknown
	return size;
}

-(AVCaptureSession *) session {
	return	session;
}

-(queue_t*) rq {
	return &rq;
}

-(ms_mutex_t *) mutex {
	return &mutex;
}

@end

typedef struct v4IosState{
	IOSMsWebCam * webcam;
	int frame_ind;
	float fps;
	float start_time;
	int frame_count;
}v4IosState;



static void v4Ios_init(MSFilter *f){
	v4IosState *s=ms_new0(v4IosState,1);
	s->webcam= [[IOSMsWebCam alloc] init];
	s->start_time=0;
	s->frame_count=-1;
	s->fps=15;
	f->data=s;
}

static int v4Ios_start(MSFilter *f, void *arg) {
	v4IosState *s=(v4IosState*)f->data;
	[s->webcam start];
	ms_message("v4Ios video device opened.");
	return 0;
}

static int v4Ios_stop(MSFilter *f, void *arg){
	v4IosState *s=(v4IosState*)f->data;
	[s->webcam stop];
	ms_message("v4Ios video device closed.");
	
	return 0;
}

static void v4Ios_uninit(MSFilter *f){
	v4IosState *s=(v4IosState*)f->data;
	v4Ios_stop(f,NULL);
	
	[s->webcam release];
	ms_free(s);
}

static void v4Ios_process(MSFilter * obj){
	v4IosState *s=(v4IosState*)obj->data;
	uint32_t timestamp;
	int cur_frame;
	if (s->frame_count==-1){
		s->start_time=obj->ticker->time;
		s->frame_count=0;
	}

	ms_mutex_lock([s->webcam mutex]);

	cur_frame=((obj->ticker->time-s->start_time)*s->fps/1000.0);
	if (cur_frame>=s->frame_count)
	{
		mblk_t *om=NULL;
		/*keep the most recent frame if several frames have been captured */
		if ([[s->webcam session] isRunning])
		{
			om=getq([s->webcam rq]);
		}

		if (om!=NULL)
		{
			timestamp=obj->ticker->time*90;/* rtp uses a 90000 Hz clockrate for video*/
			mblk_set_timestamp_info(om,timestamp);
			mblk_set_marker_info(om,TRUE);	
			ms_queue_put(obj->outputs[0],om);
			s->frame_count++;
		}
	}
	else 
		flushq([s->webcam rq],0);

	ms_mutex_unlock([s->webcam mutex]);
}

static void v4Ios_preprocess(MSFilter *f){
	v4Ios_start(f,NULL);
        
}

static void v4Ios_postprocess(MSFilter *f){
	v4Ios_stop(f,NULL);
}

static int v4Ios_set_fps(MSFilter *f, void *arg){
	v4IosState *s=(v4IosState*)f->data;
	s->fps=*((float*)arg);
	s->frame_count=-1;
	return 0;
}

static int v4Ios_get_pix_fmt(MSFilter *f,void *arg){
    *(MSPixFmt*)arg=MS_YUV420P;
	return 0;
}

static int v4Ios_set_vsize(MSFilter *f, void *arg){
	v4IosState *s=(v4IosState*)f->data;
	[s->webcam setSize:*((MSVideoSize*)arg)];
	return 0;
}

static int v4Ios_get_vsize(MSFilter *f, void *arg){
	v4IosState *s=(v4IosState*)f->data;
	*(MSVideoSize*)arg = [s->webcam getSize];
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_FPS		,	v4Ios_set_fps		},
	{	MS_FILTER_GET_PIX_FMT	,	v4Ios_get_pix_fmt	},
	{	MS_FILTER_SET_VIDEO_SIZE, 	v4Ios_set_vsize	},
	{	MS_V4L_START			,	v4Ios_start		},
	{	MS_V4L_STOP				,	v4Ios_stop		},
	{	MS_FILTER_GET_VIDEO_SIZE,	v4Ios_get_vsize	},
	{	0						,	NULL			}
};

MSFilterDesc ms_v4Ios_desc={
	.id=MS_V4L_ID,
	.name="MSv4Ios",
	.text="A video for IOS compatible source filter to stream pictures.",
	.ninputs=0,
	.noutputs=1,
	.category=MS_FILTER_OTHER,
	.init=v4Ios_init,
	.preprocess=v4Ios_preprocess,
	.process=v4Ios_process,
	.postprocess=v4Ios_postprocess,
	.uninit=v4Ios_uninit,
	.methods=methods
};

MS_FILTER_DESC_EXPORT(ms_v4Ios_desc)
        
static void ms_v4Ios_detect(MSWebCamManager *obj);

static void ms_v4Ios_cam_init(MSWebCam *cam)
{
}

static int v4Ios_open_device(MSFilter *f, void *arg)
{	
	v4IosState *s=(v4IosState*)f->data;
	[s->webcam openDevice:(char*)arg];
	return 0;
}


static MSFilter *ms_v4Ios_create_reader(MSWebCam *obj)
{	
	MSFilter *f= ms_filter_new_from_desc(&ms_v4Ios_desc); 
	v4Ios_open_device(f,obj->data);
	return f;
}

MSWebCamDesc ms_v4Ios_cam_desc={
	"AV Capture",
	&ms_v4Ios_detect,
	&ms_v4Ios_cam_init,
	&ms_v4Ios_create_reader,
	NULL
};


static void ms_v4Ios_detect(MSWebCamManager *obj){
  
	unsigned int i = 0;
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	
	NSArray * array = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
	
	for(i = 0 ; i < [array count]; i++)
	{
		AVCaptureDevice * device = [array objectAtIndex:i];
		MSWebCam *cam=ms_web_cam_new(&ms_v4Ios_cam_desc);
		cam->name= ms_strdup([[device localizedName] UTF8String]);
		cam->data = ms_strdup([[device uniqueID] UTF8String]);
		ms_web_cam_manager_add_cam(obj,cam);
	}
	[myPool drain];
}
            
