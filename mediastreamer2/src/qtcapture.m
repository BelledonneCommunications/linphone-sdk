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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
static OSType forcedPixelFormat=kCVPixelFormatType_420YpCbCr8Planar;
//static OSType forcedPixelFormat=0;

static MSPixFmt ostype_to_pix_fmt(OSType pixelFormat, bool printFmtName){
	// ms_message("OSType= %i", pixelFormat);
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
                default:
                	if (printFmtName) ms_message("Format unknown: %i", (UInt32) pixelFormat);
                	return MS_PIX_FMT_UNKNOWN;
        }
}

@interface NsMsWebCam :NSObject
{
	QTCaptureDeviceInput *input;
	QTCaptureDecompressedVideoOutput * output;
	QTCaptureSession *session;
	
	ms_mutex_t mutex;
	queue_t rq;
};

- (id)init;
- (void)dealloc;
- (int)start;
- (int)stop;
- (void)setSize:(MSVideoSize) size;
- (MSVideoSize)getSize;
- (void)openDevice:(NSString*) deviceId;
- (int)getPixFmt;


- (QTCaptureSession *)session;
- (queue_t*)rq;
- (ms_mutex_t *)mutex;

@end




@implementation NsMsWebCam 

- (void)captureOutput:(QTCaptureOutput *)captureOutput didOutputVideoFrame:(CVImageBufferRef)frame withSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection
{
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	ms_mutex_lock(&mutex);	

    	OSType pixelFormat = CVPixelBufferGetPixelFormatType(frame);
        MSPixFmt msfmt = ostype_to_pix_fmt(pixelFormat, false);

	if (CVPixelBufferIsPlanar(frame)) {
		size_t numberOfPlanes = CVPixelBufferGetPlaneCount(frame);
		MSPicture pict;
	    	size_t w = CVPixelBufferGetWidth(frame);
		size_t h = CVPixelBufferGetHeight(frame);
		mblk_t *yuv_block = ms_yuv_buf_alloc(&pict, w, h);

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

	[myPool drain];
}

- (id)init {
	qinit(&rq);
	ms_mutex_init(&mutex,NULL);
	session = [[QTCaptureSession alloc] init];
	output = [[QTCaptureDecompressedVideoOutput alloc] init];
	[output automaticallyDropsLateVideoFrames];
	[output setDelegate: self];
	
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
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];	 
		NSArray * array = [device formatDescriptions];
	
		NSEnumerator *enumerator = [array objectEnumerator];
		QTFormatDescription *desc;
		while ((desc = [enumerator nextObject])) {
			if ([desc mediaType] == QTMediaTypeVideo) {
				UInt32 fmt = [desc formatType];
				MSPixFmt msfmt = ostype_to_pix_fmt(fmt, true);
				if (msfmt != MS_PIX_FMT_UNKNOWN) {
			       		[pool drain];
					return msfmt;
				}
            		}
        	}
        	[pool drain];
    	} else {
    		ms_error("The camera wasn't opened when asking for pixel format");
    	}

	ms_warning("No compatible format found, using MS_YUV420P pixel format");
	// Configure the output to convert the uncompatible hardware pixel format to MS_YUV420P
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSDictionary *old_dic = [output pixelBufferAttributes];
	if ([[old_dic objectForKey:(id)kCVPixelBufferPixelFormatTypeKey] integerValue] != kCVPixelFormatType_420YpCbCr8Planar) {
		NSDictionary *dic = [NSDictionary dictionaryWithObjectsAndKeys:
		 [NSNumber numberWithInteger:[[old_dic objectForKey:(id)kCVPixelBufferWidthKey] integerValue]], (id)kCVPixelBufferWidthKey,
		 [NSNumber numberWithInteger:[[old_dic objectForKey:(id)kCVPixelBufferHeightKey] integerValue]],(id)kCVPixelBufferHeightKey,
		 [NSNumber numberWithInteger:kCVPixelFormatType_420YpCbCr8Planar], (id)kCVPixelBufferPixelFormatTypeKey,
		  nil];
		  [output setPixelBufferAttributes:dic];
	}

	[pool drain];
	return MS_YUV420P;
}



- (void)openDevice:(NSString*)deviceId {
	NSError *error = nil;
	unsigned int i = 0;
	QTCaptureDevice * device = NULL;

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];	 
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
	
	bool success = [device open:&error];
	if (success) ms_message("Device opened");
	else {
		ms_error("Error while opening camera: %s", [[error localizedDescription] UTF8String]);
		[pool drain];
		return;
	}

	input = [[QTCaptureDeviceInput alloc] initWithDevice:device];
	
	success = [session addInput:input error:&error];
	if (!success) ms_error("%s", [[error localizedDescription] UTF8String]);
	

	success = [session addOutput:output error:&error];
	if (!success) ms_error("%s", [[error localizedDescription] UTF8String]);
	[pool drain];
}

- (void)setSize:(MSVideoSize)size {	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSDictionary *dic;
	if (forcedPixelFormat != 0) {
		ms_message("QTCapture set size w=%i, h=%i fmt=%i", size.width, size.height, forcedPixelFormat);
		dic = [NSDictionary dictionaryWithObjectsAndKeys:
		 [NSNumber numberWithInteger:size.width], (id)kCVPixelBufferWidthKey,
		 [NSNumber numberWithInteger:size.height],(id)kCVPixelBufferHeightKey,
		 [NSNumber numberWithInteger:forcedPixelFormat], (id)kCVPixelBufferPixelFormatTypeKey, // force pixel format to plannar
		  nil];
	} else {
		dic = [NSDictionary dictionaryWithObjectsAndKeys:
		 [NSNumber numberWithInteger:size.width], (id)kCVPixelBufferWidthKey,
		 [NSNumber numberWithInteger:size.height],(id)kCVPixelBufferHeightKey,
		  nil];
	}
	
	[output setPixelBufferAttributes:dic];
	[pool drain];
}

- (MSVideoSize)getSize {
	MSVideoSize size;
	
	size.width = MS_VIDEO_SIZE_QCIF_W;
	size.height = MS_VIDEO_SIZE_QCIF_H;
	
	if(output) {
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		NSDictionary * dic = [output pixelBufferAttributes];
		size.width = [[dic objectForKey:(id)kCVPixelBufferWidthKey] integerValue];
		size.height = [[dic objectForKey:(id)kCVPixelBufferHeightKey] integerValue];
		[pool drain];
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
} v4mState;



static void v4m_init(MSFilter *f) {
	v4mState *s = ms_new0(v4mState,1);
	s->webcam = [[NsMsWebCam alloc] init];
	s->start_time = 0;
	s->frame_count = -1;
	s->fps = 15;
	f->data = s;
}

static int v4m_start(MSFilter *f, void *arg) {
	v4mState *s = (v4mState*)f->data;
	[s->webcam performSelectorOnMainThread:@selector(start) withObject:nil waitUntilDone:NO];
	ms_message("v4m video device opened.");
	return 0;
}

static int v4m_stop(MSFilter *f, void *arg) {
	v4mState *s = (v4mState*)f->data;
	[s->webcam performSelectorOnMainThread:@selector(stop) withObject:nil waitUntilDone:NO];
	ms_message("v4m video device closed.");
	return 0;
}

static void v4m_uninit(MSFilter *f) {
	v4mState *s = (v4mState*)f->data;
	v4m_stop(f,NULL);
	
	[s->webcam release];
	ms_free(s);
}

static void v4m_process(MSFilter * obj){
	v4mState *s = (v4mState*)obj->data;
	uint32_t timestamp;
	int cur_frame;
	if (s->frame_count == -1){
		s->start_time=obj->ticker->time;
		s->frame_count=0;
	}

	ms_mutex_lock([s->webcam mutex]);

	cur_frame = ((obj->ticker->time-s->start_time)*s->fps/1000.0);
	if (cur_frame >= s->frame_count) {
		mblk_t *om=NULL;
		/*keep the most recent frame if several frames have been captured */
		if ([[s->webcam session] isRunning]) {
			om=getq([s->webcam rq]);
		}

		if (om != NULL) {
			timestamp=obj->ticker->time*90;/* rtp uses a 90000 Hz clockrate for video*/
			mblk_set_timestamp_info(om,timestamp);
			mblk_set_marker_info(om,TRUE);	
			ms_queue_put(obj->outputs[0],om);
			s->frame_count++;
		}
	} else {
		flushq([s->webcam rq],0);
    }

	ms_mutex_unlock([s->webcam mutex]);
}

static void v4m_preprocess(MSFilter *f) {
	v4m_start(f,NULL);
        
}

static void v4m_postprocess(MSFilter *f) {
	v4m_stop(f,NULL);
}

static int v4m_set_fps(MSFilter *f, void *arg) {
	v4mState *s = (v4mState*)f->data;
	s->fps = *((float*)arg);
	s->frame_count = -1;
	return 0;
}

static int v4m_get_pix_fmt(MSFilter *f,void *arg) {
	v4mState *s = (v4mState*)f->data;
	*((MSPixFmt*)arg) = [s->webcam getPixFmt];
	return 0;
}

static int v4m_set_vsize(MSFilter *f, void *arg) {
	v4mState *s = (v4mState*)f->data;
	[s->webcam setSize:*((MSVideoSize*)arg)];
	return 0;
}

static int v4m_get_vsize(MSFilter *f, void *arg) {
	v4mState *s = (v4mState*)f->data;
	*(MSVideoSize*)arg = [s->webcam getSize];
	return 0;
}

static MSFilterMethod methods[] = {
	{	MS_FILTER_SET_FPS		,	v4m_set_fps		},
	{	MS_FILTER_GET_PIX_FMT	,	v4m_get_pix_fmt	},
	{	MS_FILTER_SET_VIDEO_SIZE, 	v4m_set_vsize	},
	{	MS_V4L_START			,	v4m_start		},
	{	MS_V4L_STOP				,	v4m_stop		},
	{	MS_FILTER_GET_VIDEO_SIZE,	v4m_get_vsize	},
	{	0						,	NULL			}
};

MSFilterDesc ms_v4m_desc={
	.id=MS_V4L_ID,
	.name="MSV4m",
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
	v4mState *s = (v4mState*)f->data;
	[s->webcam performSelectorOnMainThread:@selector(openDevice:) withObject:[NSString stringWithUTF8String:(char*)arg] waitUntilDone:NO];
	return 0;
}


static MSFilter *ms_v4m_create_reader(MSWebCam *obj) {	
	MSFilter *f = ms_filter_new_from_desc(&ms_v4m_desc); 
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
