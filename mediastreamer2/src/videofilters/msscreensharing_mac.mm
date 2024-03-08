/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <bctoolbox/defs.h>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/mswebcam.h"

#include "msscreensharing_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <dispatch/dispatch.h>

#include <algorithm>
#include <list>
#include <map>
#include <mutex>
#include <sstream>

@interface StreamOutput : NSObject <SCStreamOutput> {
	MsScreenSharing_mac *mScreenSharing;
    @public
    BOOL mProcessing;
}
- (id)initWithScreensharing:(MsScreenSharing_mac *)screenSharing;
- (void)stream:(SCStream *)stream
	didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
				   ofType:(SCStreamOutputType)type;
+ (SCContentFilter *)getFilter:(CGWindowID)windowId displayId:(CGDirectDisplayID)displayId;
+ (void)getWindowSize:(CGWindowID)windowId x:(int*)x y:(int*)y height:(int*)height width:(int*)width;
+ (void)getDisplaySize:(CGDirectDisplayID)displayId x:(int*)x y:(int*)y height:(int*)height width:(int*)width;
@end

template<typename... T>
void doRelease(T && ... args) {
#if !__has_feature(objc_arc)
	([&]{
		if(args) [args release];
	} (), ...);
#endif
}

MsScreenSharing_mac::MsScreenSharing_mac() : MsScreenSharing(){
	mLastFormat.mPixelFormat = MS_YUV420P;
}

MsScreenSharing_mac::~MsScreenSharing_mac() {
	stop();
	MsScreenSharing_mac::uninit();
}

void MsScreenSharing_mac::setSource(MSScreenSharingDesc sourceDesc, FormatData formatData){
	MsScreenSharing::setSource(sourceDesc, formatData);
	if (mLastFormat.mPixelFormat == MS_PIX_FMT_UNKNOWN) mLastFormat.mPixelFormat = MS_YUV420P;
}

void MsScreenSharing_mac::init() {
	if(mSourceDesc.type != MS_SCREEN_SHARING_EMPTY)
		mRunnable = initDisplay();
	else
		mRunnable = false;
	MsScreenSharing::init();
}


bool MsScreenSharing_mac::initDisplay() {
	mScreenRects.clear();
	int x, y, width = 0, height = 0;
	getWindowSize(&x,&y,&width,&height);
	if(width > 0 && height > 0)
		mScreenRects.push_back(Rect(x, y, width, height));
	else
		ms_error("[MsScreenSharing_mac] Window ID size is not available %dx%d, %dx%d from %x", x, y, width, height, mSourceDesc.native_data);

	MsScreenSharing::updateScreenConfiguration(mScreenRects);

	return mScreenRects.size() > 0;
}

void MsScreenSharing_mac::getWindowSize(int *windowX,
										int *windowY,
										int *windowWidth,
										int *windowHeight) const {

	if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_DISPLAY) {
		CGDirectDisplayID displayId = *(CGDirectDisplayID *) (&mSourceDesc.native_data);
		[StreamOutput getDisplaySize:displayId x:windowX y:windowY height:windowHeight width:windowWidth];
		*windowX = 0;
		*windowY = 0;
	} else if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_WINDOW) {
		CGWindowID windowId = *(CGWindowID *) (&mSourceDesc.native_data);
		[StreamOutput getWindowSize:windowId x:windowX y:windowY height:windowHeight width:windowWidth];
		*windowX = 0;
		*windowY = 0;
	}
}

@interface StreamDelegate : NSObject <SCStreamDelegate> {
}
//- (id)init;
- (void)outputVideoEffectDidStartForStream:(SCStream *)stream;
- (void)outputVideoEffectDidStopForStream:(SCStream *)stream;
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error;
@end

@implementation StreamDelegate

- (void)outputVideoEffectDidStartForStream:(SCStream *)stream
{}
- (void)outputVideoEffectDidStopForStream:(SCStream *)stream
{}
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error
{}
@end

@implementation StreamOutput
+ (SCContentFilter *)getFilter:(CGWindowID)windowId displayId:(CGDirectDisplayID)displayId{
	std::condition_variable condition;
	std::mutex lock;
	BOOL ended = FALSE;
	__block SCContentFilter *filter = nil;
	__block BOOL *_ended = &ended;
	__block std::condition_variable *_condition = &condition;
	ms_message("[MsScreenSharing_mac] Getting Filter");
	[SCShareableContent
		getShareableContentWithCompletionHandler:^(SCShareableContent *shareableContent,
												   NSError *error) {
			if (!error || error.code == 0) {
				if(windowId) {
					for (int i = 0; i < shareableContent.windows.count && !filter; ++i)
						if (shareableContent.windows[i].windowID == windowId) {
							ms_message("[MsScreenSharing_mac] Got a Window");
							filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:shareableContent.windows[i]];
						}
				}else if( displayId) {
					for (int i = 0; i < shareableContent.displays.count && !filter; ++i)
						if( shareableContent.displays[i].displayID == displayId) {
							ms_message("[MsScreenSharing_mac] Got a Display");
							filter = [[SCContentFilter alloc] initWithDisplay:shareableContent.displays[i] excludingWindows:@[]];
						}
				}
			}
			*_ended = TRUE;
			_condition->notify_all();
		}];
	std::unique_lock<std::mutex> locker(lock);
	condition.wait(locker, [&ended]{ return ended; });
	return filter;
}

+ (void)getWindowSize:(CGWindowID)windowId x:(int*)x y:(int*)y height:(int*)height width:(int*)width {
	CFArrayRef descriptions = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, windowId);
	if(CFArrayGetCount(descriptions) > 0) {
		CFDictionaryRef description = (CFDictionaryRef)CFArrayGetValueAtIndex ((CFArrayRef)descriptions, 0);
		if(CFDictionaryContainsKey(description, kCGWindowBounds)) {
			CFDictionaryRef bounds = (CFDictionaryRef)CFDictionaryGetValue (description, kCGWindowBounds);
			if(bounds) {
				CGRect windowRect;
				CGRectMakeWithDictionaryRepresentation(bounds, &windowRect);
				*x = windowRect.origin.x;
				*y = windowRect.origin.y;
				*height = windowRect.size.height;
				*width = windowRect.size.width;
			}else
				ms_warning("[MsScreenSharing_mac] Bounds found be cannot be parsed for Window ID : %x", windowId);
		}else
			ms_warning("[MsScreenSharing_mac] No bounds specified in Apple description for Window ID : %x", windowId);
	}else
		ms_warning("[MsScreenSharing_mac] No description found for Window ID : %x", windowId);
	CFRelease(descriptions);
}

+ (void)getDisplaySize:(CGDirectDisplayID)displayId x:(int*)x y:(int*)y height:(int*)height width:(int*)width {
	CGRect displayRect = CGDisplayBounds(displayId);
	*x = displayRect.origin.x;
	*y = displayRect.origin.y;
	*height = displayRect.size.height;
	*width = displayRect.size.width;
}


- (id)initWithScreensharing:(MsScreenSharing_mac *)screenSharing {
	mScreenSharing = screenSharing;
    mProcessing = FALSE;
	return self;
}
- (void)stream:(SCStream *)stream
	didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
				   ofType:(SCStreamOutputType)type {
    mProcessing = TRUE;
	mScreenSharing->mFrameLock.lock();
	if (mScreenSharing->mLastFormat.mSizeChanged) {
		mScreenSharing->mFrameLock.unlock();
        mProcessing = FALSE;
		mScreenSharing->mAppleThreadIterator.notify_all();
		return;
	} else
		mScreenSharing->mFrameLock.unlock();
	if (type == SCStreamOutputTypeScreen) {
		CVImageBufferRef frame = CMSampleBufferGetImageBuffer(sampleBuffer);
		size_t w = CVPixelBufferGetWidth(frame);
		size_t h = CVPixelBufferGetHeight(frame);
		int rotation = 0;

		CVReturn status = CVPixelBufferLockBaseAddress(frame, 0);
		if (kCVReturnSuccess != status) {
			frame = nil;
            mProcessing = FALSE;
            mScreenSharing->mAppleThreadIterator.notify_all();
			return;
		}

		//size_t plane_width = CVPixelBufferGetWidthOfPlane(frame, 0);
		//size_t plane_height = CVPixelBufferGetHeightOfPlane(frame, 0);
		size_t y_bytePer_row = CVPixelBufferGetBytesPerRowOfPlane(frame, 0);
		//size_t y_plane_height = CVPixelBufferGetHeightOfPlane(frame, 0);
		//size_t y_plane_width = CVPixelBufferGetWidthOfPlane(frame, 0);
		//size_t cbcr_plane_height = CVPixelBufferGetHeightOfPlane(frame, 1);
		//size_t cbcr_plane_width = CVPixelBufferGetWidthOfPlane(frame, 1);
		size_t cbcr_bytePer_row = CVPixelBufferGetBytesPerRowOfPlane(frame, 1);
		uint8_t *y_src = static_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(frame, 0));
		uint8_t *cbcr_src = static_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(frame, 1));

        mblk_t * frameCopy = copy_ycbcrbiplanar_to_true_yuv_with_rotation(mScreenSharing->mAllocator,
                                                                     y_src,
                                                                     cbcr_src,
                                                                     rotation,
                                                                     w,
                                                                     h,
                                                                     (unsigned int) y_bytePer_row,
                                                                     (unsigned int) cbcr_bytePer_row,
                                                                     TRUE);
        CVPixelBufferUnlockBaseAddress(frame, kCVPixelBufferLock_ReadOnly);

		mScreenSharing->mFrameLock.lock();
		if (mScreenSharing->mFrameData)
			freemsg(mScreenSharing->mFrameData);
		if (!mScreenSharing->mLastFormat.mSizeChanged) {
            mScreenSharing->mFrameData = frameCopy;
		}else {
            freemsg(frameCopy);
			mScreenSharing->mFrameData = nullptr;
		}
		mScreenSharing->mFrameLock.unlock();
	}
    mProcessing = FALSE;
    mScreenSharing->mAppleThreadIterator.notify_all();
}
@end

bool MsScreenSharing_mac::getPermission() {
	ms_message("[MsScreenSharing_mac] Getting permissions");
	__block bool haveAccess = false;
	// Must be call from main thread! If not, you may be in deadlock.
	dispatch_sync(dispatch_get_main_queue(), ^{
	// Checks whether the current process already has screen capture access
		haveAccess = CGPreflightScreenCaptureAccess();
	//Requests event listening access if absent, potentially prompting
		if(!haveAccess) haveAccess = CGRequestScreenCaptureAccess();
	});

	return haveAccess;
}

void MsScreenSharing_mac::inputThread() {
	if(mSourceDesc.type == MS_SCREEN_SHARING_EMPTY) return;
	if(!getPermission()){
		ms_error("[MsScreenSharing_mac] Permission denied for screen sharing");
		return;
	}else
		ms_message("[MsScreenSharing_mac] Permission granted");
	
	NSError *error = nil;
	SCContentFilter *filter = nil;
	SCStreamConfiguration *streamConfig = nil;
	StreamDelegate *streamDelegate = [[StreamDelegate alloc] init];
	if (!streamDelegate) {
		ms_error("[MsScreenSharing_mac] Cannot instantiate stream delegate");
		return;
	}
	StreamOutput *streamOutput = [[StreamOutput alloc] initWithScreensharing:this];
	
	if (!streamOutput) {
		ms_error("[MsScreenSharing_mac] Cannot instantiate stream output");
		doRelease(streamDelegate);
		return;
	}
	// FILTER
	dispatch_queue_t videoSampleBufferQueue = dispatch_queue_create("MSScreenSharing Queue", NULL);
	streamConfig = [[SCStreamConfiguration alloc] init];
	if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_DISPLAY) {
		CGDirectDisplayID displayId = *(CGDirectDisplayID *) (&mSourceDesc.native_data);
		filter = [StreamOutput getFilter:0 displayId:displayId];
	} else if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_WINDOW) {
		CGWindowID windowId = *(CGWindowID *) (&mSourceDesc.native_data);
		filter = [StreamOutput getFilter:windowId displayId:0];
	}
	if(!filter) {
		ms_error("[MsScreenSharing_mac] Cannot instantiate a SCContentFilter");
		dispatch_release(videoSampleBufferQueue);
		doRelease(streamConfig,filter, streamOutput, streamDelegate);
		return;
	}
	[streamConfig setWidth:mLastFormat.mPosition.getWidth()];
	[streamConfig setHeight:mLastFormat.mPosition.getHeight()];
	// Should be more accurate on quantize the signal values. Equivalent to NV12
	[streamConfig setPixelFormat:kCVPixelFormatType_420YpCbCr8BiPlanarFullRange]; 

	CMTime fps;
	fps.value = 1;
	fps.timescale = mFps;
	[streamConfig setMinimumFrameInterval:fps];
	ms_message("[MsScreenSharing_mac] Creating stream");
	// Start Capturing session
	SCStream *stream = [[SCStream alloc] initWithFilter:filter
												configuration:streamConfig
												delegate:streamDelegate];
	if (!stream) {
		ms_error("[MsScreenSharing_mac] Cannot instantiate stream");
		dispatch_release(videoSampleBufferQueue);
		doRelease(streamConfig,filter, streamOutput, streamDelegate);
		return;
	}
	ms_message("[MsScreenSharing_mac] Adding output");
	[stream addStreamOutput:streamOutput
						type:SCStreamOutputTypeScreen
						sampleHandlerQueue:videoSampleBufferQueue
						error:&error];
	if (error) {
		ms_error("[MsScreenSharing_mac] Cannot add stream output %x", (int)error.code);
		dispatch_release(videoSampleBufferQueue);
		doRelease(streamConfig,filter, stream, streamOutput, streamDelegate);
		return;
	}

	__block BOOL started = FALSE;
	BOOL ended = false;
	__block BOOL *_ended = &ended;
// Start
	ms_message("[MsScreenSharing_mac] Starting capture");
	[stream startCaptureWithCompletionHandler:^(NSError *_Nullable error) {
		if (error != nil && error.code != 0) {
            ms_error("[MsScreenSharing_mac] Failed to start capture : %s, (%x), %s"
				, [[error localizedFailureReason] cStringUsingEncoding:NSUTF8StringEncoding]
                , error.code
				,  [[error localizedRecoverySuggestion] cStringUsingEncoding:NSUTF8StringEncoding]);
		}else
			started = TRUE;
		*_ended = TRUE;
		mAppleThreadIterator.notify_all();
	}];
    
    {
		std::unique_lock<std::mutex> lock(mAppleThreadLock);
		mAppleThreadIterator.wait(lock, [&ended]{ return ended;});
	}
	if(started && !mToStop) ms_message("[MsScreenSharing_mac] Capturing");
	while (!mToStop) {
		int width = mLastFormat.mPosition.getWidth();
		int height = mLastFormat.mPosition.getHeight();
		int x, y;
		getWindowSize(&x,&y,&width, &height);
		width -= width % 8;
		height -= height % 8;
		if (mLastFormat.mPosition.getHeight() != height
			|| mLastFormat.mPosition.getWidth() != width) {
			mFrameLock.lock();
			mLastFormat.mSizeChanged = true;
			mLastFormat.mLastTimeSizeChanged = std::chrono::system_clock::now();
			mLastFormat.mPosition.mX2 = mLastFormat.mPosition.mX1 + width;
			mLastFormat.mPosition.mY2 = mLastFormat.mPosition.mY1 + height;
			mFrameLock.unlock();
		}else {
			std::unique_lock<std::mutex> lock(mThreadLock);
			mThreadIterator.wait_for(lock, std::chrono::milliseconds( MIN((int)(1000.0 / mFps), 333)), [this]{return this->mToStop;});
		}
	}
// Stop
	if(started) {
		ms_message("[MsScreenSharing_mac] Stopping thread");
		ended = FALSE;
		[stream stopCaptureWithCompletionHandler:^(NSError *_Nullable error) {
			if (error != nil && error.code != 0) {
				ms_error("[MsScreenSharing_mac] Failed to stop capture with error: %s"
					, [[error localizedFailureReason] cStringUsingEncoding:NSUTF8StringEncoding]);
			}
			*_ended = TRUE;
			mAppleThreadIterator.notify_all();
		}];
		// stopCaptureWithCompletionHandler doesn't take account of didOutputSampleBuffer calls : a frame can still be in processing while the stream stop..
		std::unique_lock<std::mutex> lock(mAppleThreadLock);
		mAppleThreadIterator.wait(lock, [&ended, streamOutput](){return ended && !streamOutput->mProcessing;});
	}
	dispatch_release(videoSampleBufferQueue);
	doRelease(streamConfig,filter, stream, streamOutput, streamDelegate);
	ms_message("[MsScreenSharing_mac] Thread stopped");
}
