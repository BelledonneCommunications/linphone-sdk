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
+ (SCWindow *)findWindow:(CGWindowID)windowId;
+ (SCDisplay *)findDisplay:(CGDirectDisplayID)displayId;
@end

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
	switch (mSourceDesc.type) {
	case MSScreenSharingType::MS_SCREEN_SHARING_DISPLAY: {
		CGDirectDisplayID displayId = *(CGDirectDisplayID *) (&mSourceDesc.native_data);
		SCDisplay *display = [StreamOutput findDisplay:displayId];
		if(display) {
			if (display.width > 0 && display.height > 0)
				mScreenRects.push_back(Rect(0, 0, display.width, display.height));
			else
				ms_error("[MsScreenSharing_mac] display size not available %dx%d, %dx%d",
						 (int)display.width,
						 (int)display.height,
						 (int)display.frame.size.width,
						 (int)display.frame.size.height);
			[display release];
		}else
			ms_error("[MsScreenSharing_mac] Display ID is not available: %x", displayId);
		}
		break;
	case MSScreenSharingType::MS_SCREEN_SHARING_WINDOW: {
		CGWindowID windowId = *(CGWindowID *) (&mSourceDesc.native_data);
		SCWindow *window = [StreamOutput findWindow:windowId];
		if(window){
			if (window.frame.size.width > 0 && window.frame.size.height > 0)
				mScreenRects.push_back(Rect(0, 0, window.frame.size.width, window.frame.size.height));
			else
				ms_error("[MsScreenSharing_mac] window size not available %dx%d",
					 (int)window.frame.size.width,
					 (int)window.frame.size.height);
			[window release];
		}else
			ms_error("[MsScreenSharing_mac] Window ID is not available: %x", windowId);
		}
		break;
	case MSScreenSharingType::MS_SCREEN_SHARING_AREA:
		break;
	default: {
	}
	}

	MsScreenSharing::updateScreenConfiguration(mScreenRects);

	return mScreenRects.size() > 0;
}

void MsScreenSharing_mac::getWindowSize(int *windowX,
										int *windowY,
										int *windowWidth,
										int *windowHeight) const {

	if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_DISPLAY) {
		CGDirectDisplayID displayId = *(CGDirectDisplayID *) (&mSourceDesc.native_data);
		SCDisplay *display = [StreamOutput findDisplay:displayId];
		if(display) {
			*windowX = 0;
			*windowY = 0;
			*windowWidth = display.width;
			*windowHeight = display.height;
			[display release];
		}
	} else if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_WINDOW) {
		CGWindowID windowId = *(CGWindowID *) (&mSourceDesc.native_data);
		SCWindow *window = [StreamOutput findWindow:windowId];
		if(window) {
			*windowX = 0;
			*windowY = 0;
			*windowWidth = window.frame.size.width;
			*windowHeight = window.frame.size.height;
			[window release];
		}
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
+ (SCWindow *)findWindow:(CGWindowID)windowId {
	std::condition_variable condition;
	std::mutex lock;
	BOOL ended = FALSE;
	__block SCWindow *window = nil;
	__block BOOL *_ended = &ended;
	__block std::condition_variable *_condition = &condition;
	
	[SCShareableContent
		getShareableContentWithCompletionHandler:^(SCShareableContent *shareableContent,
												   NSError *error) {
			if (!error || error.code == 0) {
				for (int i = 0; i < shareableContent.windows.count && !window; ++i)
					if (shareableContent.windows[i].windowID == windowId) {
						window = [shareableContent.windows[i] retain];
					}
			}
			*_ended = TRUE;
			_condition->notify_all();
		}];
	std::unique_lock<std::mutex> locker(lock);
	condition.wait(locker, [&ended]{ return ended; });
	return window;
}

+ (SCDisplay *)findDisplay:(CGDirectDisplayID)displayId {
	std::condition_variable condition;
	std::mutex lock;
	BOOL ended = FALSE;
	__block SCDisplay *display = nil;
	__block BOOL *_ended = &ended;
	__block std::condition_variable *_condition = &condition;
	[SCShareableContent
		getShareableContentWithCompletionHandler:^(SCShareableContent *shareableContent,
												   NSError *error) {
			if (!error || error.code == 0) {
				for (int i = 0; i < shareableContent.displays.count && !display; ++i)
					if (shareableContent.displays[i].displayID == displayId) {
						display = [shareableContent.displays[i] retain];
					}
			}
			*_ended = TRUE;
			_condition->notify_all();
		}];

	std::unique_lock<std::mutex> locker(lock);
	condition.wait(locker, [&ended]{ return ended; });
	return display;
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
	int tryCount = 0;
	while (!CGRequestScreenCaptureAccess() && ++tryCount <= 5) {// Let 5 seconds to get permissions.
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return CGRequestScreenCaptureAccess();
}

void MsScreenSharing_mac::inputThread() {
	if(mSourceDesc.type == MS_SCREEN_SHARING_EMPTY) return;
	if(!getPermission()){
		ms_error("[MsScreenSharing_mac] Permission denied for screen sharing");
		return;
	}
	
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
		[streamDelegate dealloc];
		return;
	}
	// FILTER
	dispatch_queue_t videoSampleBufferQueue = dispatch_queue_create("MSScreenSharing Queue", NULL);
	streamConfig = [[SCStreamConfiguration alloc] init];
	if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_DISPLAY) {
		CGDirectDisplayID displayId = *(CGDirectDisplayID *) (&mSourceDesc.native_data);
		SCDisplay *display = [StreamOutput findDisplay:displayId];
		filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];
		[display release];
	} else if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_WINDOW) {
		CGWindowID windowId = *(CGWindowID *) (&mSourceDesc.native_data);
		SCWindow *window = [StreamOutput findWindow:windowId];
		filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:window];
		[window release];
	}
	[streamConfig setWidth:mLastFormat.mPosition.getWidth()];
	[streamConfig setHeight:mLastFormat.mPosition.getHeight()];
	// Should be more accurate on quantize the signal values. Equivalent to NV12
	[streamConfig setPixelFormat:kCVPixelFormatType_420YpCbCr8BiPlanarFullRange]; 

	CMTime fps;
	fps.value = 1;
	fps.timescale = mFps;
	[streamConfig setMinimumFrameInterval:fps];

	// Start Capturing session
	SCStream *stream = [[SCStream alloc] initWithFilter:filter
												configuration:streamConfig
												delegate:streamDelegate];
	if (!stream) {
		ms_error("[MsScreenSharing_mac] Cannot instantiate stream");
		if (filter) [filter dealloc];
		if (streamConfig) [streamConfig dealloc];
		dispatch_release(videoSampleBufferQueue);
		[streamOutput dealloc];
		[streamDelegate dealloc];
		return;
	}
	[stream addStreamOutput:streamOutput
						type:SCStreamOutputTypeScreen
						sampleHandlerQueue:videoSampleBufferQueue
						error:&error];
	if (error) {
		ms_error("[MsScreenSharing_mac] Cannot add stream output %x", (int)error.code);
		dispatch_release(videoSampleBufferQueue);
		if (stream) [stream dealloc];
		if (filter) [filter dealloc];
		if (streamConfig) [streamConfig dealloc];
		[streamOutput dealloc];
		[streamDelegate dealloc];
		return;
	}

	__block BOOL started = FALSE;
	BOOL ended = false;
	__block BOOL *_ended = &ended;
// Start
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
			mThreadIterator.wait_for(lock, std::chrono::microseconds(100), [this]{return this->mToStop;});
		}
	}
// Stop
	ms_message("[MsScreenSharing_mac] Stopping thread");
	if(started) {
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
	if (streamConfig) [streamConfig dealloc];
	if (filter) [filter dealloc];
	if (stream) [stream dealloc];
	dispatch_release(videoSampleBufferQueue);
	[streamOutput dealloc];
	[streamDelegate dealloc];
	ms_message("[MsScreenSharing_mac] Thread stopped");
}
