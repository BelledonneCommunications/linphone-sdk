/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2016  Belledonne Communications SARL

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

#ifdef __APPLE__

#include "mediastreamer-config.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msv4l.h"
#include "mediastreamer2/mswebcam.h"
#include "nowebcam.h"

#import <AVFoundation/AVFoundation.h>
struct v4mState;

@interface NsMsWebCam : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
{
	AVCaptureDeviceInput *input;
	AVCaptureVideoDataOutput * output;
	AVCaptureSession *session;
	MSYuvBufAllocator *allocator;
	ms_mutex_t mutex;
	queue_t rq;
	BOOL isStretchingCamera;
};

- (id)init;
- (void)dealloc;
- (int)start;
- (int)stop;
- (void)setSize:(MSVideoSize) size;
- (MSVideoSize)getSize;
- (AVCaptureDevice*)initDevice:(NSString*)deviceId;
- (void)openDevice:(NSString*) deviceId;
- (int)getPixFmt;


- (AVCaptureSession *)session;
- (queue_t*)rq;
- (ms_mutex_t *)mutex;

@end

static void capture_queue_cleanup(void* p) {
    NsMsWebCam *capture = (NsMsWebCam *)p;
    [capture release];
}



@implementation NsMsWebCam 

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:( CMSampleBufferRef)sampleBuffer
	   fromConnection:(AVCaptureConnection *)connection  {	//ms_message("AVCapture: callback is working before");
	ms_mutex_lock(&mutex);
	CVImageBufferRef frame = CMSampleBufferGetImageBuffer(sampleBuffer);
	//OSType pixelFormat = CVPixelBufferGetPixelFormatType(frame);
	if (rq.q_mcount >= 5){
		/*don't let too much buffers to be queued here, it makes no sense for a real time processing and would consume too much memory*/
		ms_warning("AVCapture: dropping %i frames", rq.q_mcount);
		flushq(&rq, 0);
	}
	
	size_t numberOfPlanes = CVPixelBufferGetPlaneCount(frame);
	MSPicture pict;
	size_t w = CVPixelBufferGetWidth(frame);
	size_t h = CVPixelBufferGetHeight(frame);
	mblk_t *yuv_block = ms_yuv_buf_allocator_get(allocator, &pict, w, h);
	
	CVReturn status = CVPixelBufferLockBaseAddress(frame, 0);
	if (kCVReturnSuccess != status) {
		ms_error("Error locking base address: %i", status);
		return;
	}
	size_t p;
	for (p=0; p < numberOfPlanes; p++) {
		size_t fullrow_width = CVPixelBufferGetBytesPerRowOfPlane(frame, p);
		size_t plane_width = CVPixelBufferGetWidthOfPlane(frame, p);
		size_t plane_height = CVPixelBufferGetHeightOfPlane(frame, p);
		uint8_t *dst_plane = pict.planes[p];
		uint8_t *src_plane = CVPixelBufferGetBaseAddressOfPlane(frame, p);
		size_t l;
		
		for (l=0; l<plane_height; l++) {
			memcpy(dst_plane, src_plane, plane_width);
			src_plane += fullrow_width;
			dst_plane += plane_width;
		}
	}
	CVPixelBufferUnlockBaseAddress(frame, 0);
	putq(&rq, yuv_block);
	
	//ms_message("AVCapture: callback is working after");
	ms_mutex_unlock(&mutex);
 
}

- (id)init {
	qinit(&rq);
	ms_mutex_init(&mutex,NULL);
	session = [[AVCaptureSession alloc] init];
    output = [[AVCaptureVideoDataOutput alloc] init];
    [output alwaysDiscardsLateVideoFrames];
	allocator = ms_yuv_buf_allocator_new();
	isStretchingCamera = FALSE;
	return self;
}

- (void)dealloc {
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
	ms_yuv_buf_allocator_free(allocator);
	ms_mutex_destroy(&mutex);

	[super dealloc];
}

- (int)start {
	if (!session.running) {
		// Init queue
		dispatch_queue_t queue = dispatch_queue_create("CaptureQueue", NULL);
		dispatch_set_context(queue, [self retain]);
		dispatch_set_finalizer_f(queue, capture_queue_cleanup);
		[output setSampleBufferDelegate:self queue:queue];
		dispatch_release(queue);
		
		
		// At the time of macosx 10.11, it's mandatory to keep old settings, otherwise pixel buffer size is no preserved
		NSDictionary *old_dic = [output videoSettings];
		NSDictionary *dic = [NSDictionary dictionaryWithObjectsAndKeys:
							 [NSNumber numberWithInteger:[[old_dic objectForKey:(id)kCVPixelBufferWidthKey] integerValue]], (id)kCVPixelBufferWidthKey,
							 [NSNumber numberWithInteger:[[old_dic objectForKey:(id)kCVPixelBufferHeightKey] integerValue]],(id)kCVPixelBufferHeightKey,
							 [NSNumber numberWithUnsignedInteger:kCVPixelFormatType_420YpCbCr8Planar], (id)kCVPixelBufferPixelFormatTypeKey,
							 nil];
		[output setVideoSettings:dic];
		
		
		
		[session startRunning];
		ms_message("AVCapture: Engine started");
	}
	return 0;
}

- (int)stop {
       if (session.running) {
           [session removeInput:input];
           [session removeOutput:output];

            [session stopRunning];
    
           [output setSampleBufferDelegate:nil queue:nil];
            ms_message("AVCapture: Engine stopped");
}
	return 0;
}

- (int)getPixFmt {
  	return MS_YUV420P;
}

- (AVCaptureDevice*)initDevice:(NSString*)deviceId {
	unsigned int i = 0;
	AVCaptureDevice * device = NULL;

    NSArray * array = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];

	for (i = 0 ; i < [array count]; i++) {
		AVCaptureDevice * currentDevice = [array objectAtIndex:i];
		if([[currentDevice uniqueID] isEqualToString:deviceId]) {
			device = currentDevice;
			break;
		}
	}
	if (device == NULL) {
		ms_error("Error: camera %s not found, using default one", [deviceId UTF8String]);
        device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];

	}
	return device;
}

- (void)openDevice:(NSString*)deviceId {
	NSError *error = nil;
	AVCaptureDevice * device = [self initDevice:deviceId];
    input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
        if (error )
        {
            ms_error("%s", [[error localizedDescription] UTF8String]);
            return;
        }
    if ( input && [session canAddInput:input] ){
        [input retain]; // keep reference on an externally allocated object
        [session addInput:input];
    } else {
        ms_error("Error: input nil or cannot be added: %p", input);
    }
    if( output && [session canAddOutput:output] ){
        [session addOutput:output];
    } else {
        ms_error("Error: output nil or cannot be added: %p", output);
    }
}



- (NSString *) videoSizeToPreset:(MSVideoSize)size {
	NSString *preset;
	
	if (size.height*size.width >=MS_VIDEO_SIZE_720P_H*MS_VIDEO_SIZE_720P_W) {
		preset = AVCaptureSessionPreset1280x720;
	} else if (size.height*size.width >= MS_VIDEO_SIZE_VGA_H*MS_VIDEO_SIZE_VGA_H) {
		preset = AVCaptureSessionPreset640x480;
	} else if (size.height*size.width >= MS_VIDEO_SIZE_CIF_H*MS_VIDEO_SIZE_CIF_W) {
		preset = AVCaptureSessionPreset352x288;
	} else if (size.height*size.width >= MS_VIDEO_SIZE_QVGA_H*MS_VIDEO_SIZE_QVGA_H) {
		preset = AVCaptureSessionPreset320x240;
	} else {
		// Default case
		preset = AVCaptureSessionPreset352x288;
	}
	return preset;
}

- (MSVideoSize) presetTovideoSize:(const NSString *)preset {
	
	if (preset ==  AVCaptureSessionPreset1280x720) return MS_VIDEO_SIZE_720P;
	if (preset ==  AVCaptureSessionPreset640x480) return MS_VIDEO_SIZE_VGA;
	if (preset ==  AVCaptureSessionPreset352x288) return MS_VIDEO_SIZE_CIF;
	if (preset ==  AVCaptureSessionPreset320x240) return MS_VIDEO_SIZE_QVGA;
	
	ms_error("Unknon format %s, returnig CIF",[preset UTF8String]);
	return MS_VIDEO_SIZE_CIF;
}



- (void)setSize:(MSVideoSize)size {
	session.sessionPreset = [self videoSizeToPreset:size];
}
- (MSVideoSize)getSize {
	return [self presetTovideoSize:session.sessionPreset];
}

- (AVCaptureSession *)session {
	return	session;
}

- (queue_t*)rq {
	return &rq;
}

- (ms_mutex_t *)mutex {
	return &mutex;
}

@end

typedef struct v4mState {
	NsMsWebCam * webcam;
	int frame_ind;
	float fps;
	float start_time;
	int frame_count;
	MSAverageFPS afps;
} v4mState;



static void v4m_init(MSFilter *f) {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	v4mState *s = ms_new0(v4mState,1);
	s->webcam = [[NsMsWebCam alloc] init];
	s->start_time = 0;
	s->frame_count = -1;
	s->fps = 15;
	f->data = s;
	[myPool drain];
}

static int v4m_start(MSFilter *f, void *arg) {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	v4mState *s = (v4mState*)f->data;
	[s->webcam start];
	ms_message("AVCapture video device opened.");
	[myPool drain];
	return 0;
}

static int v4m_stop(MSFilter *f, void *arg) {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	v4mState *s = (v4mState*)f->data;
	[s->webcam stop];
	ms_message("AVCapture video device closed.");
	[myPool drain];
	return 0;
}

static void v4m_uninit(MSFilter *f) {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	v4mState *s = (v4mState*)f->data;
	v4m_stop(f,NULL);
	
	[s->webcam release];
	ms_free(s);
	[myPool drain];
}

static void v4m_process(MSFilter * obj){
	v4mState *s = (v4mState*)obj->data;
	uint32_t timestamp;
	int cur_frame;
	if (s->frame_count == -1){
		s->start_time=obj->ticker->time;
		s->frame_count=0;
	}
	
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];

	ms_mutex_lock([s->webcam mutex]);

	cur_frame = ((obj->ticker->time-s->start_time)*s->fps/1000.0);
	if (cur_frame >= s->frame_count) {
		mblk_t *om=NULL;
		mblk_t *m;
		/*keep the most recent frame if several frames have been captured */
		if ([[s->webcam session] isRunning]) {
			while ((m=getq([s->webcam rq])) != NULL){
				if (om) freemsg(om);
				om = m;
			}
		}

		if (om != NULL) {
			timestamp=obj->ticker->time*90;/* rtp uses a 90000 Hz clockrate for video*/
			mblk_set_timestamp_info(om,timestamp);
			mblk_set_marker_info(om,TRUE);	
			ms_queue_put(obj->outputs[0],om);
			s->frame_count++;
			ms_average_fps_update(&s->afps, obj->ticker->time);
		}
	} else {
		flushq([s->webcam rq],0);
	}

	ms_mutex_unlock([s->webcam mutex]);
	
	[myPool drain];
}

static void v4m_preprocess(MSFilter *f) {
	v4mState *s = (v4mState *)f->data;
	ms_average_fps_init(&s->afps, "AV capture average fps = %f");
	v4m_start(f,NULL);    
}

static void v4m_postprocess(MSFilter *f) {
	v4mState *s = (v4mState *)f->data;
	v4m_stop(f,NULL);
	ms_average_fps_init(&s->afps, "AV capture average fps = %f");
}

static int v4m_set_fps(MSFilter *f, void *arg) {
	v4mState *s = (v4mState*)f->data;
	s->fps = *((float*)arg);
	s->frame_count = -1;
	return 0;
}

static int v4m_get_fps(MSFilter *f, void *arg) {
	v4mState *s = (v4mState *)f->data;
	*((float *)arg) = ms_average_fps_get(&s->afps);
	return 0;
}

static int v4m_get_pix_fmt(MSFilter *f,void *arg) {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	v4mState *s = (v4mState*)f->data;
	*((MSPixFmt*)arg) = [s->webcam getPixFmt];
	[myPool drain];
	return 0;
}

static int v4m_set_vsize(MSFilter *f, void *arg) {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	v4mState *s = (v4mState*)f->data;
	[s->webcam setSize:*((MSVideoSize*)arg)];
	[myPool drain];
	return 0;
}

static int v4m_get_vsize(MSFilter *f, void *arg) {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	v4mState *s = (v4mState*)f->data;
	*(MSVideoSize*)arg = [s->webcam getSize];
	[myPool drain];
	return 0;
}

static MSFilterMethod methods[] = {
	{	MS_FILTER_SET_FPS		,	v4m_set_fps		},
	{	MS_FILTER_GET_FPS		,	v4m_get_fps		},
	{	MS_FILTER_GET_PIX_FMT	,	v4m_get_pix_fmt	},
	{	MS_FILTER_SET_VIDEO_SIZE, 	v4m_set_vsize	},
	{	MS_V4L_START			,	v4m_start		},
	{	MS_V4L_STOP				,	v4m_stop		},
	{	MS_FILTER_GET_VIDEO_SIZE,	v4m_get_vsize	},
	{	0						,	NULL			}
};

MSFilterDesc ms_v4m_desc={
	.id=MS_V4L_ID,
	.name="MSAVCapture",
	.text="A video for macosx compatible source filter to stream pictures.",
	.ninputs=0,
	.noutputs=1,
	.category=MS_FILTER_OTHER,
	.init=v4m_init,
	.preprocess=v4m_preprocess,
	.process=v4m_process,
	.postprocess=v4m_postprocess,
	.uninit=v4m_uninit,
	.methods=methods
};

MS_FILTER_DESC_EXPORT(ms_v4m_desc)
        
static void ms_v4m_detect(MSWebCamManager *obj);

static void ms_v4m_cam_init(MSWebCam *cam) {
}

static int v4m_open_device(MSFilter *f, void *arg) {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	v4mState *s = (v4mState*)f->data;
	[s->webcam initDevice: [NSString stringWithUTF8String:(char*)arg]];
	[s->webcam openDevice:[NSString stringWithUTF8String:(char*)arg]];
	[myPool drain];
	return 0;
}


static MSFilter *ms_v4m_create_reader(MSWebCam *obj) {
	MSFilter *f = ms_factory_create_filter_from_desc(ms_web_cam_get_factory(obj), &ms_v4m_desc);
	v4m_open_device(f,obj->data);
	return f;
}

MSWebCamDesc ms_v4m_cam_desc = {
	"AV Capture",
	&ms_v4m_detect,
	&ms_v4m_cam_init,
	&ms_v4m_create_reader,
	NULL
};


static void ms_v4m_detect(MSWebCamManager *obj) {
	unsigned int i = 0;
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	
    NSArray * array = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
	
	for(i = 0 ; i < [array count]; i++) {
		AVCaptureDevice * device = [array objectAtIndex:i];
		MSWebCam *cam = ms_web_cam_new(&ms_v4m_cam_desc);
		cam->name = ms_strdup([[device localizedName] UTF8String]);
		//cam->name = ms_strdup([[device modelID] UTF8String]);
		cam->data = ms_strdup([[device uniqueID] UTF8String]);
		ms_web_cam_manager_add_cam(obj,cam);
	}
	[myPool drain];
}
        
#endif        
