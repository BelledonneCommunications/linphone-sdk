/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011  Belledonne Communications SARL

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

#ifdef __APPLE__

#include "mediastreamer-config.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msv4l.h"
#include "mediastreamer2/mswebcam.h"
#include "nowebcam.h"

#import <QTKit/QTKit.h>

struct v4mState;

// Define != NULL to have QT Framework convert hardware device pixel format to another one.
static OSType forcedPixelFormat=kCVPixelFormatType_422YpCbCr8_yuvs;
//static OSType forcedPixelFormat=0;

static MSPixFmt ostype_to_pix_fmt(OSType pixelFormat, bool printFmtName){
        ms_message("OSType= %i", (int)pixelFormat);
        switch(pixelFormat){
                case kCVPixelFormatType_420YpCbCr8Planar:
                	if (printFmtName) ms_message("FORMAT = MS_YUV420P");
                	return MS_YUV420P;
                case kYUVSPixelFormat:
                	if (printFmtName) ms_message("FORMAT = MS_YUY2");
                	return MS_YUY2;
                case kUYVY422PixelFormat:
                	if (printFmtName) ms_message("FORMAT = MS_UYVY");
                	return MS_UYVY;
                case k32RGBAPixelFormat:
                	if (printFmtName) ms_message("FORMAT = MS_RGBA32");
                	return MS_RGBA32;
                case kCVPixelFormatType_24RGB:
                	if (printFmtName) ms_message("FORMAT = MS_RGB24");
                	return MS_RGB24;
                default:
                	if (printFmtName) ms_message("Format unknown: %ui", (unsigned int) pixelFormat);
                	return MS_PIX_FMT_UNKNOWN;
        }
}

@interface NsMsWebCam : NSObject
{
	QTCaptureDeviceInput *input;
	QTCaptureDecompressedVideoOutput * output;
	QTCaptureSession *session;
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
- (QTCaptureDevice*)initDevice:(NSString*)deviceId;
- (void)openDevice:(NSString*) deviceId;
- (int)getPixFmt;


- (QTCaptureSession *)session;
- (queue_t*)rq;
- (ms_mutex_t *)mutex;

@end




@implementation NsMsWebCam 

- (void)captureOutput:(QTCaptureOutput *)captureOutput didOutputVideoFrame:(CVImageBufferRef)frame withSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection
{
	ms_mutex_lock(&mutex);	

    	//OSType pixelFormat = CVPixelBufferGetPixelFormatType(frame);
    if (rq.q_mcount >= 5){
    	/*don't let too much buffers to be queued here, it makes no sense for a real time processing and would consume too much memory*/
    	ms_warning("QTCapture: dropping %i frames", rq.q_mcount);
    	flushq(&rq, 0);
    }

	if (CVPixelBufferIsPlanar(frame)) {
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
		int p;
		for (p=0; p < numberOfPlanes; p++) {
			size_t fullrow_width = CVPixelBufferGetBytesPerRowOfPlane(frame, p);
			size_t plane_width = CVPixelBufferGetWidthOfPlane(frame, p);
			size_t plane_height = CVPixelBufferGetHeightOfPlane(frame, p);
			uint8_t *dst_plane = pict.planes[p];
			uint8_t *src_plane = CVPixelBufferGetBaseAddressOfPlane(frame, p);
			int l;
			
			for (l=0; l<plane_height; l++) {
				memcpy(dst_plane, src_plane, plane_width);
				src_plane += fullrow_width;
				dst_plane += plane_width;
			}
		}
		CVPixelBufferUnlockBaseAddress(frame, 0);
		putq(&rq, yuv_block);
	} else {
		// Buffer doesn't contain a plannar image.
		uint8_t * data = (uint8_t *)[sampleBuffer bytesForAllSamples];
		int size = [sampleBuffer lengthForAllSamples];
		mblk_t *buf=allocb(size,0);
		memcpy(buf->b_wptr, data, size);
		buf->b_wptr+=size;
		putq(&rq, buf);
	}


	ms_mutex_unlock(&mutex);
}

- (id)init {
	qinit(&rq);
	ms_mutex_init(&mutex,NULL);
	session = [[QTCaptureSession alloc] init];
	output = [[QTCaptureDecompressedVideoOutput alloc] init];
	[output automaticallyDropsLateVideoFrames];
	[output setDelegate: self];
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
	[session startRunning];
	return 0;
}

- (int)stop {
	[session stopRunning];
	return 0;
}

- (int)getPixFmt {
	if (forcedPixelFormat != 0) {
		MSPixFmt msfmt=ostype_to_pix_fmt(forcedPixelFormat, true);
		ms_message("getPixFmt forced capture FMT: %i", msfmt);
		return msfmt;
	}

	QTCaptureDevice *device = [input device];
	// Return the first pixel format of the hardware device compatible with mediastreamer
	// Could be improved by choosing the best through all the formats supported by the hardware.
	if([device isOpen]) {
		NSArray * array = [device formatDescriptions];
	
		NSEnumerator *enumerator = [array objectEnumerator];
		QTFormatDescription *desc;
		while ((desc = [enumerator nextObject])) {
			if ([desc mediaType] == QTMediaTypeVideo) {
				UInt32 fmt = [desc formatType];
				MSPixFmt msfmt = ostype_to_pix_fmt(fmt, true);
				if (msfmt != MS_PIX_FMT_UNKNOWN) {
					return msfmt;
				}
            }
        }
    } else {
        ms_error("The camera wasn't opened when asking for pixel format");
    }

	ms_warning("No compatible format found, using MS_YUV420P pixel format");
	// Configure the output to convert the uncompatible hardware pixel format to MS_YUV420P
	NSDictionary *old_dic = [output pixelBufferAttributes];
	if ([[old_dic objectForKey:(id)kCVPixelBufferPixelFormatTypeKey] integerValue] != kCVPixelFormatType_420YpCbCr8Planar) {
		NSDictionary *dic = [NSDictionary dictionaryWithObjectsAndKeys:
		 [NSNumber numberWithInteger:[[old_dic objectForKey:(id)kCVPixelBufferWidthKey] integerValue]], (id)kCVPixelBufferWidthKey,
		 [NSNumber numberWithInteger:[[old_dic objectForKey:(id)kCVPixelBufferHeightKey] integerValue]],(id)kCVPixelBufferHeightKey,
		 [NSNumber numberWithUnsignedInteger:kCVPixelFormatType_420YpCbCr8Planar], (id)kCVPixelBufferPixelFormatTypeKey,
		  nil];
		  [output setPixelBufferAttributes:dic];
	}

	return MS_YUV420P;
}

static const char *stretchingCameras[]={
	"UVC Camera VendorID_1452 ProductID_34057", /*macbook pro 2011*/
	"UVC Camera VendorID_1452 ProductID_34065",  /*iMac*/
	NULL
};

static bool is_stretching_camera(const char *modelID){
	const char **it;
	for (it = stretchingCameras; *it != NULL; ++it){
		if (strcasecmp(*it, modelID) == 0) return true;
	}
	return false;
}

- (QTCaptureDevice*)initDevice:(NSString*)deviceId {
	unsigned int i = 0;
	QTCaptureDevice * device = NULL;

	NSArray * array = [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];
	for (i = 0 ; i < [array count]; i++) {
		QTCaptureDevice * currentDevice = [array objectAtIndex:i];
		if([[currentDevice uniqueID] isEqualToString:deviceId]) {
			device = currentDevice;
			break;
		}
	}
	if (device == NULL) {
		ms_error("Error: camera %s not found, using default one", [deviceId UTF8String]);
		device = [QTCaptureDevice defaultInputDeviceWithMediaType:QTMediaTypeVideo];
	}
	isStretchingCamera = is_stretching_camera([[device modelUniqueID] UTF8String]);
	return device;
}

- (void)openDevice:(NSString*)deviceId {
	NSError *error = nil;
	QTCaptureDevice * device = [self initDevice:deviceId];
	
	bool success = [device open:&error];
	if (success) ms_message("Device opened, model is %s", [[device modelUniqueID] UTF8String]);
	else {
		ms_error("Error while opening camera: %s", [[error localizedDescription] UTF8String]);
		return;
	}

	input = [[QTCaptureDeviceInput alloc] initWithDevice:device];
	
	success = [session addInput:input error:&error];
	if (!success) ms_error("%s", [[error localizedDescription] UTF8String]);
	

	success = [session addOutput:output error:&error];
	if (!success) ms_error("%s", [[error localizedDescription] UTF8String]);
	
}



- (void)setSize:(MSVideoSize)size {	
	NSDictionary *dic;
	MSVideoSize new_size = size;
	
	/*mac cameras are not able to capture between VGA and 720P without doing an ugly stretching.  Workaround this problem here for SVGA, which a common format
	used by mediastreamer2 encoders.
	*/
	const MSVideoSize svga = MS_VIDEO_SIZE_SVGA;

	if (isStretchingCamera){
		if (ms_video_size_equal(size, svga)){
			new_size.width = 960;
			new_size.height = 540;
		}
	}
	if (!ms_video_size_equal(new_size, size)){
		ms_message("QTCatpure video size requested is %ix%i, but adapted to %ix%i in order to avoid stretching", size.width, size.height,
					new_size.width, new_size.height);
		size = new_size;
	}
	
	if (forcedPixelFormat != 0) {
		ms_message("QTCapture set size w=%i, h=%i fmt=%i", size.width, size.height, (unsigned int)forcedPixelFormat);
		dic = [NSDictionary dictionaryWithObjectsAndKeys:
		 [NSNumber numberWithInteger:size.width], (id)kCVPixelBufferWidthKey,
		 [NSNumber numberWithInteger:size.height],(id)kCVPixelBufferHeightKey,
		 [NSNumber numberWithUnsignedInt:forcedPixelFormat], (id)kCVPixelBufferPixelFormatTypeKey, // force pixel format to plannar
		  nil];
	} else {
		dic = [NSDictionary dictionaryWithObjectsAndKeys:
		 [NSNumber numberWithInteger:size.width], (id)kCVPixelBufferWidthKey,
		 [NSNumber numberWithInteger:size.height],(id)kCVPixelBufferHeightKey,
		  nil];
	}
	
	[output setPixelBufferAttributes:dic];
}

- (MSVideoSize)getSize {
	MSVideoSize size;
	
	size.width = MS_VIDEO_SIZE_QCIF_W;
	size.height = MS_VIDEO_SIZE_QCIF_H;
	
	if(output) {
		NSDictionary * dic = [output pixelBufferAttributes];
		size.width = [[dic objectForKey:(id)kCVPixelBufferWidthKey] integerValue];
		size.height = [[dic objectForKey:(id)kCVPixelBufferHeightKey] integerValue];
	}
	return size;
}

- (QTCaptureSession *)session {
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
	[s->webcam performSelectorOnMainThread:@selector(start) withObject:nil waitUntilDone:NO];
	ms_message("qtcapture video device opened.");
	[myPool drain];
	return 0;
}

static int v4m_stop(MSFilter *f, void *arg) {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	v4mState *s = (v4mState*)f->data;
	[s->webcam performSelectorOnMainThread:@selector(stop) withObject:nil waitUntilDone:NO];
	ms_message("qtcapture video device closed.");
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
	ms_average_fps_init(&s->afps, "QuickTime capture average fps = %f");
	v4m_start(f,NULL);    
}

static void v4m_postprocess(MSFilter *f) {
	v4mState *s = (v4mState *)f->data;
	v4m_stop(f,NULL);
	ms_average_fps_init(&s->afps, "QuickTime capture average fps = %f");
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
	.name="MSQtCapture",
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
	[s->webcam performSelectorOnMainThread:@selector(openDevice:) withObject:[NSString stringWithUTF8String:(char*)arg] waitUntilDone:NO];
	[myPool drain];
	return 0;
}


static MSFilter *ms_v4m_create_reader(MSWebCam *obj) {
	MSFilter *f = ms_factory_create_filter_from_desc(ms_web_cam_get_factory(obj), &ms_v4m_desc);
	v4m_open_device(f,obj->data);
	return f;
}

MSWebCamDesc ms_v4m_cam_desc = {
	"QT Capture",
	&ms_v4m_detect,
	&ms_v4m_cam_init,
	&ms_v4m_create_reader,
	NULL
};


static void ms_v4m_detect(MSWebCamManager *obj) {
	unsigned int i = 0;
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	
	NSArray * array = [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];
	
	for(i = 0 ; i < [array count]; i++) {
		QTCaptureDevice * device = [array objectAtIndex:i];
		MSWebCam *cam = ms_web_cam_new(&ms_v4m_cam_desc);
		cam->name = ms_strdup([[device localizedDisplayName] UTF8String]);
		cam->data = ms_strdup([[device uniqueID] UTF8String]);
		ms_web_cam_manager_add_cam(obj,cam);
	}
	[myPool drain];
}
        
#endif        
