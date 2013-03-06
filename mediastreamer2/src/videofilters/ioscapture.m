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
#import <UIKit/UIKit.h>
#import <QuartzCore/CALayer.h>

#if !TARGET_IPHONE_SIMULATOR

// AVCaptureVideoPreviewLayer with AVCaptureSession creation
@interface AVCaptureVideoPreviewLayerEx : AVCaptureVideoPreviewLayer

@end

@implementation AVCaptureVideoPreviewLayerEx


- (id)init {
	return [super initWithSession:[[[AVCaptureSession alloc] init] autorelease]];
}

@end

@interface IOSCapture : UIView<AVCaptureVideoDataOutputSampleBufferDelegate> {
@private
	AVCaptureDeviceInput *input;
	AVCaptureVideoDataOutput * output;
	ms_mutex_t mutex;
	mblk_t *msframe;
	int frame_ind;
	float fps;
	float start_time;
	int frame_count;
	MSVideoSize mOutputVideoSize;
	MSVideoSize mCameraVideoSize; //required size in portrait mode
	Boolean mDownScalingRequired;
	int mDeviceOrientation;
	MSAverageFPS averageFps;
	char fps_context[64];
};

- (void)initIOSCapture;
- (int)start;
- (int)stop;
- (void)setSize:(MSVideoSize) size;
- (MSVideoSize*)getSize;
- (void)openDevice:(const char*) deviceId;
- (void)setFps:(float) value;
+ (Class)layerClass;

@property (nonatomic, retain) UIView* parentView;

@end

static void capture_queue_cleanup(void* p) {
	IOSCapture *capture = (IOSCapture *)p;
	[capture release];
}

@implementation IOSCapture

@synthesize parentView;

- (id)init {
	self = [super init];
	if (self) {
		[self initIOSCapture];
	}
	return self;
}

- (id)initWithCoder:(NSCoder *)coder {
	self = [super initWithCoder:coder];
	if (self) {
		[self initIOSCapture];
	}
	return self;
}

- (id)initWithFrame:(CGRect)frame {
	self = [super initWithFrame:frame];
	if (self) {
		[self initIOSCapture];
	}
	return self;
}

- (void)initIOSCapture {
	msframe = NULL;
	ms_mutex_init(&mutex, NULL);
	output = [[AVCaptureVideoDataOutput  alloc] init];
	
	[self setOpaque:YES];
	[self setAutoresizingMask: UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
	
	/*
	 Currently, the only supported key is kCVPixelBufferPixelFormatTypeKey. Supported pixel formats are kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, kCVPixelFormatType_420YpCbCr8BiPlanarFullRange and kCVPixelFormatType_32BGRA, except on iPhone 3G, where the supported pixel formats are kCVPixelFormatType_422YpCbCr8 and kCVPixelFormatType_32BGRA..
	 */
	NSDictionary* dic = [NSDictionary dictionaryWithObjectsAndKeys:
						 [NSNumber numberWithInteger:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange], (id)kCVPixelBufferPixelFormatTypeKey, nil];
	[output setVideoSettings:dic];
	
	/* Set the layer */
	AVCaptureVideoPreviewLayer *previewLayer = (AVCaptureVideoPreviewLayer *)self.layer;
	[previewLayer setOrientation:AVCaptureVideoOrientationPortrait];
	[previewLayer setBackgroundColor:[[UIColor clearColor] CGColor]];
	[previewLayer setOpaque:YES];
	start_time=0;
	frame_count=-1;
	fps=0;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
	   fromConnection:(AVCaptureConnection *)connection {	
	CVImageBufferRef frame = nil;
	@synchronized(self) { 
		@try {
			CVImageBufferRef frame = CMSampleBufferGetImageBuffer(sampleBuffer); 
			CVReturn status = CVPixelBufferLockBaseAddress(frame, 0);
			if (kCVReturnSuccess != status) {
				ms_error("Error locking base address: %i", status);
				frame=nil;
				return;
			}
			
			/*kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange*/
			size_t plane_width = CVPixelBufferGetWidthOfPlane(frame, 0);
			size_t plane_height = CVPixelBufferGetHeightOfPlane(frame, 0);
			//sanity check
			/*size_t y_bytePer_row = CVPixelBufferGetBytesPerRowOfPlane(frame, 0);
			size_t cbcr_plane_height = CVPixelBufferGetHeightOfPlane(frame, 1);
			size_t cbcr_plane_width = CVPixelBufferGetWidthOfPlane(frame, 1);
			size_t cbcr_bytePer_row = CVPixelBufferGetBytesPerRowOfPlane(frame, 1);		 
			*/
			uint8_t* y_src= CVPixelBufferGetBaseAddressOfPlane(frame, 0);
			uint8_t* cbcr_src= CVPixelBufferGetBaseAddressOfPlane(frame, 1);
			int rotation=0;
			if (![connection isVideoOrientationSupported]) {
				switch (mDeviceOrientation) {
					case 0: {
						rotation = 90;
						break;
					}
					case 90: {
						if ([(AVCaptureDevice*)input.device position] == AVCaptureDevicePositionBack) {
							rotation = 180;
						} else {
							rotation = 0;
						}
						break;
					}
					case 270: {
						if ([(AVCaptureDevice*)input.device position] == AVCaptureDevicePositionBack) {
							rotation = 0;
						} else {
							rotation = 180;
						}
						break;
					}
					default: ms_error("Unsupported device orientation [%i]",mDeviceOrientation);
				}
			}
			/*check if buffer size are compatible with downscaling or rotation*/
			int factor =mDownScalingRequired?2:1;
			switch (rotation) {
				case 0:
				case 180:
					if (mOutputVideoSize.width*factor>plane_width || mOutputVideoSize.height*factor>plane_height) {
						ms_warning("[1]IOS capture discarding frame because wrong dimensions (%d > %d || %d > %d)",
								   mOutputVideoSize.width*factor, plane_width,
								   mOutputVideoSize.height*factor, plane_height);
						return;
					}
					break;
				case 90:
				case 270:
					if (mOutputVideoSize.width*factor>plane_height || mOutputVideoSize.height*factor>plane_width) {
						ms_warning("[2]	IOS capture discarding frame because wrong dimensions (%d > %d || %d > %d)",
								   mOutputVideoSize.width*factor, plane_height,
								   mOutputVideoSize.height*factor, plane_width);
						return;
					}
					break;

					default: ms_error("Unsupported device orientation [%i]",mDeviceOrientation);
			}
			
			mblk_t * yuv_block2 = copy_ycbcrbiplanar_to_true_yuv_with_rotation_and_down_scale_by_2(y_src
																								   , cbcr_src
																								   , rotation
																								   , mOutputVideoSize.width
																								   , mOutputVideoSize.height
																								   , CVPixelBufferGetBytesPerRowOfPlane(frame, 0)
																								   , CVPixelBufferGetBytesPerRowOfPlane(frame, 1)
																								   , TRUE
																								   , mDownScalingRequired); 
			  
			ms_mutex_lock(&mutex);
			if (msframe!=NULL) {
				freemsg(msframe);
			}
			msframe = yuv_block2;
		} @finally {
			if (frame) CVPixelBufferUnlockBaseAddress(frame, 0);
			ms_mutex_unlock(&mutex);
		}
	}
}

- (void)openDevice:(const char*) deviceId {
	NSError *error = nil;
	unsigned int i = 0;
	AVCaptureDevice * device = NULL;
	
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
	[input retain]; // keep reference on an externally allocated object
	
	AVCaptureSession *session = [(AVCaptureVideoPreviewLayer *)self.layer session];
	[session addInput:input];
	[session addOutput:output];
}

- (void)dealloc {
	AVCaptureSession *session = [(AVCaptureVideoPreviewLayer *)self.layer session];
	[session removeInput:input];
	[session removeOutput:output];
	[output release];
	[parentView release];
	
	if (msframe) {
		freemsg(msframe);
	}
	ms_mutex_destroy(&mutex);
	[super dealloc];
}

+ (Class)layerClass {
	return [AVCaptureVideoPreviewLayerEx class];
}

- (int)start {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	@synchronized(self) {
		AVCaptureSession *session = [(AVCaptureVideoPreviewLayer *)self.layer session];
		if (!session.running) {
			// Init queue
			dispatch_queue_t queue = dispatch_queue_create("CaptureQueue", NULL);
			dispatch_set_context(queue, [self retain]);
			dispatch_set_finalizer_f(queue, capture_queue_cleanup);	
			[output setSampleBufferDelegate:self queue:queue];
			dispatch_release(queue);
			
			[session startRunning]; //warning can take around 1s before returning
			snprintf(fps_context, sizeof(fps_context), "Captured mean fps=%%f, expected=%f", fps);
			ms_video_init_average_fps(&averageFps, fps_context);
			
			ms_message("ioscapture video device started.");
		}
	}
	[myPool drain];
	return 0;
}

- (int)stop {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	@synchronized(self) {
		AVCaptureSession *session = [(AVCaptureVideoPreviewLayer *)self.layer session];
		if (session.running) {
			[session stopRunning];
			
			// Will free the queue
			[output setSampleBufferDelegate:nil queue:nil];
		}
	}
	[myPool drain];
	return 0;
}

static AVCaptureVideoOrientation deviceOrientation2AVCaptureVideoOrientation(int deviceOrientation) {
	switch (deviceOrientation) {
		case 0: return AVCaptureVideoOrientationPortrait;
		case 90: return AVCaptureVideoOrientationLandscapeLeft;	
		case -180:
		case 180: return AVCaptureVideoOrientationPortraitUpsideDown;
		case -90:
		case 270: return AVCaptureVideoOrientationLandscapeRight;
		default:
		ms_error("Unexpected device orientation [%i] expected value are 0, 90, 180, 270",deviceOrientation);
		break;
	}
	return AVCaptureVideoOrientationPortrait;
}

- (void)setSize:(MSVideoSize) size {
	@synchronized(self) {
		AVCaptureSession *session = [(AVCaptureVideoPreviewLayer *)self.layer session];
		[session beginConfiguration];
		if (size.width*size.height == MS_VIDEO_SIZE_QVGA_W  * MS_VIDEO_SIZE_QVGA_H) {
			[session setSessionPreset: AVCaptureSessionPreset640x480];	
			mCameraVideoSize.width=MS_VIDEO_SIZE_VGA_W;
			mCameraVideoSize.height=MS_VIDEO_SIZE_VGA_H;
			mOutputVideoSize.width=MS_VIDEO_SIZE_QVGA_W;
			mOutputVideoSize.height=MS_VIDEO_SIZE_QVGA_H;
			mDownScalingRequired=true;
		} else if (size.width*size.height == MS_VIDEO_SIZE_VGA_W  * MS_VIDEO_SIZE_VGA_H) {
			[session setSessionPreset: AVCaptureSessionPreset640x480];	
			mCameraVideoSize.width=MS_VIDEO_SIZE_VGA_W;
			mCameraVideoSize.height=MS_VIDEO_SIZE_VGA_H;
			mOutputVideoSize=mCameraVideoSize;
			mDownScalingRequired=false;
		} else {
			//default case
			[session setSessionPreset: AVCaptureSessionPresetMedium];	
			mCameraVideoSize.width=MS_VIDEO_SIZE_IOS_MEDIUM_W;
			mCameraVideoSize.height=MS_VIDEO_SIZE_IOS_MEDIUM_H;	
			mOutputVideoSize=mCameraVideoSize;
			mDownScalingRequired=false;
		}
		
		NSArray *connections = output.connections;
		if ([connections count] > 0 && [[connections objectAtIndex:0] isVideoOrientationSupported]) {
			switch (mDeviceOrientation) {
				case 0:
					[[connections objectAtIndex:0] setVideoOrientation:AVCaptureVideoOrientationPortrait];
					ms_message("Configuring camera in AVCaptureVideoOrientationPortrait mode ");
					break;
				case 180:
					[[connections objectAtIndex:0] setVideoOrientation:AVCaptureVideoOrientationPortraitUpsideDown];
					ms_message("Configuring camera in AVCaptureVideoOrientationPortraitUpsideDown mode ");
					break;
				case 90:	
					[[connections objectAtIndex:0] setVideoOrientation:AVCaptureVideoOrientationLandscapeLeft];
					ms_message("Configuring camera in AVCaptureVideoOrientationLandscapeLeft mode ");
					break;
				case 270:	
					[[connections objectAtIndex:0] setVideoOrientation:AVCaptureVideoOrientationLandscapeRight];
					ms_message("Configuring camera in AVCaptureVideoOrientationLandscapeRight mode ");
				default:
					break;
			}
		}


		if (mDeviceOrientation == 0 || mDeviceOrientation == 180) { 
			MSVideoSize tmpSize = mOutputVideoSize;
			mOutputVideoSize.width=tmpSize.height;
			mOutputVideoSize.height=tmpSize.width;
		}  
		
		[session commitConfiguration];
		return;
	}
}

- (MSVideoSize*)getSize {
	return &mOutputVideoSize;
}

- (void)setFps:(float) value {
	@synchronized(self) {
		AVCaptureSession *session = [(AVCaptureVideoPreviewLayer *)self.layer session];
		[session beginConfiguration];
		if ([[[UIDevice currentDevice] systemVersion] floatValue] < 5) { 
			[output setMinFrameDuration:CMTimeMake(1, value)];
		} else {
			NSArray *connections = output.connections;
			if ([connections count] > 0) {
				[[connections objectAtIndex:0] setVideoMinFrameDuration:CMTimeMake(1, value)];
				[[connections objectAtIndex:0] setVideoMaxFrameDuration:CMTimeMake(1, value)];
			} 
			
		}
		fps=value;
		snprintf(fps_context, sizeof(fps_context), "Captured mean fps=%%f, expected=%f", fps);
		ms_video_init_average_fps(&averageFps, fps_context);
		[session commitConfiguration];
	}
}

- (void)setParentView:(UIView*)aparentView{
	if (parentView == aparentView) {
		return;
	}
	
	if(parentView != nil) {
		[self removeFromSuperview];
		[parentView release];
		parentView = nil;
	}
	
	parentView = aparentView;
	
	if(parentView != nil) {
		[parentView retain];
		AVCaptureVideoPreviewLayer *previewLayer = (AVCaptureVideoPreviewLayer *)self.layer;
		if([parentView contentMode] == UIViewContentModeScaleAspectFit) {
			previewLayer.videoGravity = AVLayerVideoGravityResizeAspect;
		} else if([parentView contentMode] == UIViewContentModeScaleAspectFill) {
			previewLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;
		} else {
			previewLayer.videoGravity = AVLayerVideoGravityResize;
		}
		[parentView insertSubview:self atIndex:0];
		[self setFrame: [parentView bounds]];
	}
}

//filter methods

static void ioscapture_init(MSFilter *f) {
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	f->data = [[IOSCapture alloc] initWithFrame:CGRectMake(0, 0, 0, 0)];
	[myPool drain];
}

static void ioscapture_uninit(MSFilter *f) {
	IOSCapture *thiz = (IOSCapture*)f->data;
	
	if(thiz != nil) {
		NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
		[thiz performSelectorInBackground:@selector(stop) withObject:nil];
		
		[thiz performSelectorOnMainThread:@selector(setParentView:) withObject:nil waitUntilDone:NO];
		[thiz release];
		[myPool drain];
	}
}

static void ioscapture_process(MSFilter * obj) {
	IOSCapture *thiz = (IOSCapture*)obj->data;
	
	if(thiz != NULL) {
		ms_mutex_lock(&thiz->mutex);
		if (thiz->msframe) {
			// keep only the latest image
			ms_queue_flush(obj->outputs[0]);
			ms_queue_put(obj->outputs[0],thiz->msframe);
			ms_video_update_average_fps(&thiz->averageFps, obj->ticker->time);
			thiz->msframe=0;
		}	
		ms_mutex_unlock(&thiz->mutex);
	}
}

static void ioscapture_preprocess(MSFilter *f) {
	IOSCapture *thiz = (IOSCapture*)f->data;
	if (thiz != NULL) {
		NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
		[thiz performSelectorInBackground:@selector(start) withObject:nil];
		[myPool drain];
	}
}

static void ioscapture_postprocess(MSFilter *f) {
}

static int ioscapture_get_fps(MSFilter *f, void *arg) {
	IOSCapture *thiz = (IOSCapture*)f->data;
	if (thiz != NULL) {
		*((float*)arg) = thiz->fps;
	}
	return 0;
}

static int ioscapture_set_fps(MSFilter *f, void *arg) {
	IOSCapture *thiz = (IOSCapture*)f->data;
	if (thiz != NULL) {
		[thiz setFps:*(float*)arg];
	}
	return 0;
}

static int ioscapture_get_pix_fmt(MSFilter *f,void *arg) {
	*(MSPixFmt*)arg = MS_YUV420P;
	return 0;
}

static int ioscapture_set_vsize(MSFilter *f, void *arg) {
	IOSCapture *thiz = (IOSCapture*)f->data;
	if (thiz != NULL) {
		[thiz setSize:*((MSVideoSize*)arg)];
	}
	return 0;
}

static int ioscapture_get_vsize(MSFilter *f, void *arg) {
	IOSCapture *thiz = (IOSCapture*)f->data;
	if (thiz != NULL) {
		*(MSVideoSize*)arg = *[thiz getSize];
	}
	return 0;
}

/*filter specific method*/

static int ioscapture_set_native_window(MSFilter *f, void *arg) {
	UIView* parentView = *(UIView**)arg;
	IOSCapture *thiz = (IOSCapture*)f->data;
	if (thiz != nil) {
		// set curent parent view
		[thiz performSelectorOnMainThread:@selector(setParentView:) withObject:parentView waitUntilDone:NO];
	}
	return 0;
}

static int ioscapture_get_native_window(MSFilter *f, void *arg) {
	IOSCapture *thiz = (IOSCapture*)f->data;
	if (thiz != NULL) {
		arg = &thiz->parentView;
	}
	return 0;
}

static int ioscapture_set_device_orientation (MSFilter *f, void *arg) {
	IOSCapture *thiz = (IOSCapture*)f->data;
	if (thiz != NULL) {
		if (thiz->mDeviceOrientation != *(int*)(arg)) {
			thiz->mDeviceOrientation = *(int*)(arg);
			[thiz setSize:thiz->mOutputVideoSize]; //to update size from orientation
			
			// delete frame if any
			ms_mutex_lock(&thiz->mutex);
			if (thiz->msframe) {
				freemsg(thiz->msframe);
				thiz->msframe = 0;
			}
			ms_mutex_unlock(&thiz->mutex);
		}
	}
	return 0;
}

/* this method is used to display the preview with correct orientation */
static int ioscapture_set_device_orientation_display (MSFilter *f, void *arg) {
	IOSCapture *thiz=(IOSCapture*)f->data;
	if (thiz != NULL) {
		AVCaptureVideoPreviewLayer *previewLayer = (AVCaptureVideoPreviewLayer *)thiz.layer;
		if ([previewLayer isOrientationSupported])
			previewLayer.orientation = deviceOrientation2AVCaptureVideoOrientation(*(int*)(arg));
	}
	return 0;
}

static MSFilterMethod methods[] = {
	{ MS_FILTER_SET_FPS, ioscapture_set_fps },
	{ MS_FILTER_GET_FPS, ioscapture_get_fps },
	{ MS_FILTER_GET_PIX_FMT, ioscapture_get_pix_fmt },
	{ MS_FILTER_SET_VIDEO_SIZE, ioscapture_set_vsize	},
	{ MS_FILTER_GET_VIDEO_SIZE, ioscapture_get_vsize	},
	{ MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, ioscapture_set_native_window },//preview is managed by capture filter
	{ MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, ioscapture_get_native_window },
	{ MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION, ioscapture_set_device_orientation },
	{ MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION, ioscapture_set_device_orientation_display },
	{ 0, NULL }
};

MSFilterDesc ms_ioscapture_desc = {
	.id=MS_V4L_ID,
	.name="MSioscapture",
	.text="A video for IOS compatible source filter to stream pictures.",
	.ninputs=0,
	.noutputs=1,
	.category=MS_FILTER_OTHER,
	.init=ioscapture_init,
	.preprocess=ioscapture_preprocess,
	.process=ioscapture_process,
	.postprocess=ioscapture_postprocess,
	.uninit=ioscapture_uninit,
	.methods=methods
};

MS_FILTER_DESC_EXPORT(ms_ioscapture_desc)

/*
 
 MSWebCamDesc for iOS
 
 */

static void ms_v4ios_detect(MSWebCamManager *obj);
static void ms_v4ios_cam_init(MSWebCam *cam);
static MSFilter *ms_v4ios_create_reader(MSWebCam *obj);

MSWebCamDesc ms_v4ios_cam_desc = {
	"AV Capture",
	&ms_v4ios_detect,
	&ms_v4ios_cam_init,
	&ms_v4ios_create_reader,
	NULL
};

static void ms_v4ios_detect(MSWebCamManager *obj) {
	
	if (kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber_iOS_4_0) {
		ms_error("No capture support for IOS version below 4");
		return;
	}
	
	unsigned int i = 0;
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	
	NSArray * array = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
	
	for(i = 0 ; i < [array count]; i++)
	{
		AVCaptureDevice * device = [array objectAtIndex:i];
		MSWebCam *cam=ms_web_cam_new(&ms_v4ios_cam_desc);
		cam->name= ms_strdup([[device modelID] UTF8String]);
		cam->data = ms_strdup([[device uniqueID] UTF8String]);
		ms_web_cam_manager_add_cam(obj,cam);
	}
	[myPool drain];
}

static void ms_v4ios_cam_init(MSWebCam *cam) {
}

static MSFilter *ms_v4ios_create_reader(MSWebCam *obj) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];	
	MSFilter *f= ms_filter_new_from_desc(&ms_ioscapture_desc);
	[((IOSCapture*)f->data) openDevice:obj->data];
	[pool drain];
	return f;
}

@end

#else

MSFilterDesc ms_ioscapture_desc={
	.id=MS_V4L_ID,
	.name="MSioscapture dummy",
	.text="Dummy capture filter for ios simulator",
	.ninputs=0,
	.noutputs=0,
	.category=MS_FILTER_OTHER,
};

#endif /*TARGET_IPHONE_SIMULATOR*/
