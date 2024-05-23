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

#include "msscreensharing_private.h"
#if HAVE_X11_XLIB_H
#include "msscreensharing_x11.h"
#elif defined(_WIN32)
#include "msscreensharing_win.h"
#include <windows.h>
#elif defined(__APPLE__)
#include "msscreensharing_mac.h"
#endif

#include <sys/stat.h>

#include <algorithm>
#include <map>
#include <mutex>

#ifndef MIN
#define MIN(a, b) a <= b ? a : b
#endif

class SIData {
public:
	SIData() {
		vsize.width = MS_VIDEO_SIZE_CIF_W;
		vsize.height = MS_VIDEO_SIZE_CIF_H;
		lasttime = 0;
		fps = 10.0;
	}

	MSVideoSize vsize;
	uint64_t lasttime;
	float fps;

#if HAVE_X11_XLIB_H
	MsScreenSharing_x11 mScreenSharing;
#elif defined(_WIN32)
	MsScreenSharing_win mScreenSharing;
#elif defined(__APPLE__)
	MsScreenSharing_mac mScreenSharing;
#else
	MsScreenSharing mScreenSharing;
#endif
};

std::condition_variable MsScreenSharing::mThreadIterator;

MsScreenSharing::MsScreenSharing() {
	mSourceDesc.type = MS_SCREEN_SHARING_EMPTY;
	mSourceDesc.native_data = NULL;
}

MsScreenSharing::~MsScreenSharing() {
	stop();
	ms_message("[MsScreenSharing] Destroyed");
}

void MsScreenSharing::setSource(MSScreenSharingDesc sourceDesc, FormatData formatData) {
	stop();
	mSourceDesc = sourceDesc;
	mLastFormat = formatData;
}

void MsScreenSharing::init() {
	int windowX = 0, windowY = 0, windowW = 100, windowH = 100;
	int screenIndex = 0;
	if (mSourceDesc.type != MS_SCREEN_SHARING_EMPTY) getWindowSize(&windowX, &windowY, &windowW, &windowH);
	mLastFormat.mPosition = getCroppedArea(windowX, windowY, windowW, windowH, &screenIndex);
	mLastFormat.mSizeChanged = false;
	mLastFormat.mScreenIndex = screenIndex;
	mLastFormat.mLastTimeSizeChanged = std::chrono::system_clock::now();
	ms_video_init_framerate_controller(&mIdleFramerateController, 3.0);
}

void MsScreenSharing::uninit() {
}

bool_t MsScreenSharing::isTimeToSend(uint64_t tickerTime) {
	return ms_video_capture_new_frame(&mFramerateController, tickerTime);
}

bool MsScreenSharing::isRunning() const {
	return mThread.joinable();
}

double MsScreenSharing::getFps() {
	return mFps;
}

MSPixFmt MsScreenSharing::getPixFormat() const {
	return mLastFormat.mPixelFormat;
}

void MsScreenSharing::setFps(const float &pFps) {
	ms_message("[MsScreenSharing] Setting target fps to %f", pFps);
	mFps = pFps;
	ms_video_init_framerate_controller(
	    &mFramerateController,
	    pFps); // Set the controller to the target FPS and then try to find a format to fit the configuration
	ms_average_fps_init(&mAvgFps, "[MsScreenSharing] fps=%f");
}

void MsScreenSharing::start() {
	ms_message("[MsScreenSharing] Starting");
	mToStop = false;
	mFrameLock.lock(); // Just in case of multiple start
	if (mFrameData) {
		freemsg(mFrameData);
		mFrameData = nullptr;
	}
	if (mFrameToSend) {
		freemsg(mFrameToSend);
		mFrameToSend = nullptr;
	}
	if (mAllocator) {
		ms_yuv_buf_allocator_free(mAllocator);
		mAllocator = nullptr;
	}
	mAllocator = ms_yuv_buf_allocator_new();
	mThread = std::thread(&MsScreenSharing::inputThread, this);
	mFrameLock.unlock();
}

void MsScreenSharing::stop() {
	if (isRunning()) {
		ms_message("[MsScreenSharing] Stopping input thread ...");
		mThreadLock.lock();
		mToStop = true;
		mThreadLock.unlock();
		mThreadIterator.notify_all();
		ms_message("[MsScreenSharing] Waiting for input thread ...");
		mThread.join();
		mFrameLock.lock(); // Just in case of multiple start
		if (mFrameData) {
			freemsg(mFrameData);
			mFrameData = nullptr;
		}
		if (mFrameToSend) {
			freemsg(mFrameToSend);
			mFrameToSend = nullptr;
		}
		if (mAllocator) {
			ms_yuv_buf_allocator_free(mAllocator);
			mAllocator = nullptr;
		}
		mFrameLock.unlock();
		ms_message("[MsScreenSharing] Input thread stopped");
	}
}

// Compute mScreenRects before calling it
void MsScreenSharing::updateScreenConfiguration(const std::vector<Rect> &screenRects) {
	// make sure that we have at least one monitor
	if (screenRects.size() == 0) {
		ms_warning("[MsScreenSharing] No monitors detected, multi-monitor support may not work properly.");
		return;
	}

	// log the screen rectangles
	for (size_t i = 0; i < screenRects.size(); ++i) {
		auto &rect = screenRects[i];
		ms_message("[MsScreenSharing] Screen %zu: (%d, %d) x (%d, %d)", i, rect.mX1, rect.mY1, rect.mX2, rect.mY2);
	}

	// calculate bounding box
	Rect screenBbox = screenRects[0];
	for (size_t i = 1; i < screenRects.size(); ++i) {
		auto &rect = screenRects[i];
		if (rect.mX1 < screenBbox.mX1) screenBbox.mX1 = rect.mX1;
		if (rect.mY1 < screenBbox.mY1) screenBbox.mY1 = rect.mY1;
		if (rect.mX2 > screenBbox.mX2) screenBbox.mX2 = rect.mX2;
		if (rect.mY2 > screenBbox.mY2) screenBbox.mY2 = rect.mY2;
	}
	if (screenBbox.mX1 >= screenBbox.mX2 || screenBbox.mY1 >= screenBbox.mY2) {
		ms_error("[MsScreenSharing] Invalid screen bounding box!\n(%d,%d)x(%d,%d)", screenBbox.mX1, screenBbox.mY1,
		         screenBbox.mX2, screenBbox.mY2);
		return;
	}

	// calculate dead space
	mScreenDeadSpace.clear();
	mScreenDeadSpace.push_back(screenBbox);
	for (size_t i = 0; i < screenRects.size(); ++i) {
		auto &subtract = screenRects[i];
		std::vector<Rect> result;
		for (Rect &rect : mScreenDeadSpace) {
			if (rect.mX1 < subtract.mX2 && rect.mY1 < subtract.mY2 && subtract.mX1 < rect.mX2 &&
			    subtract.mY1 < rect.mY2) {
				int mid_y1 = MAX(rect.mY1, subtract.mY1);
				int mid_y2 = MIN(rect.mY2, subtract.mY2);
				if (rect.mY1 < subtract.mY1) result.emplace_back(rect.mX1, rect.mY1, rect.mX2, subtract.mY1);
				if (rect.mX1 < subtract.mX1) result.emplace_back(rect.mX1, mid_y1, subtract.mX1, mid_y2);
				if (subtract.mX2 < rect.mX2) result.emplace_back(subtract.mX2, mid_y1, rect.mX2, mid_y2);
				if (subtract.mY2 < rect.mY2) result.emplace_back(rect.mX1, subtract.mY2, rect.mX2, rect.mY2);
			} else {
				result.emplace_back(rect);
			}
		}
		mScreenDeadSpace = std::move(result);
	}

	// log the dead space rectangles
	for (size_t i = 0; i < mScreenDeadSpace.size(); ++i) {
		Rect &rect = mScreenDeadSpace[i];
		ms_message("[MsScreenSharing] Dead space  %zu: (%d, %d) x (%d, %d)", i, rect.mX1, rect.mY1, rect.mX2, rect.mY2);
	}
}

MsScreenSharing::Rect
MsScreenSharing::getCroppedArea(int windowX, int windowY, int windowWidth, int windowHeight, int *screenIndex) const {
	Rect cropArea;
	cropArea.mX1 = 0;
	cropArea.mY1 = 0;
	cropArea.mX2 = cropArea.mX1 + windowWidth;
	cropArea.mY2 = cropArea.mY1 + windowHeight;

	int windowX2 = windowX + windowWidth;
	int windowY2 = windowY + windowHeight;
	// clear the dead space
	std::map<int, std::pair<Rect, size_t>> crops;
	for (size_t i = 0; i < mScreenRects.size(); ++i) {
		Rect rect = mScreenRects[i];
		Rect crop;
		crop.mX1 = MAX(rect.mX1, windowX);
		crop.mY1 = MAX(rect.mY1, windowY);
		crop.mX2 = MIN(rect.mX2, windowX2);
		crop.mY2 = MIN(rect.mY2, windowY2);
		if (crop.mX1 < crop.mX2 && crop.mY1 < crop.mY2) {
			crops[(crop.mX2 - crop.mX1) * (crop.mY2 - crop.mY1)] = {crop, i};
		}
	}
	if (crops.size() > 0) {
		cropArea = crops.rbegin()->second.first;
		if (screenIndex) *screenIndex = crops.rbegin()->second.second;
	}
	int scaleFit = cropArea.getWidth() % 8;
	if (scaleFit != 0 && cropArea.mX2 > 0) {
		cropArea.mX2 -= scaleFit / 2;
		cropArea.mX1 += scaleFit / 2 + scaleFit % 2;
	}
	scaleFit = cropArea.getHeight() % 8;
	if (scaleFit != 0 && cropArea.mY2 > 0) {
		cropArea.mY2 -= scaleFit / 2;
		cropArea.mY1 += scaleFit / 2 + scaleFit % 2;
	}
	return cropArea;
}

void MsScreenSharing::getWindowSize(BCTBX_UNUSED(int *windowX),
                                    BCTBX_UNUSED(int *windowY),
                                    BCTBX_UNUSED(int *windowWidth),
                                    BCTBX_UNUSED(int *windowHeight)) const {
}

void MsScreenSharing::inputThread() {
	ms_message("[MsScreenSharing] Input thread started. %d", (int)mToStop);
	int grabX, grabY, grabWidth, grabHeight;
	if (mRunnable) {
		auto startTime = std::chrono::system_clock::now();
		while (!mToStop) {
			int windowX, windowY, windowWidth, windowHeight;
			int screenIndex = 0;
			getWindowSize(&windowX, &windowY, &windowWidth, &windowHeight);
			Rect cropArea = getCroppedArea(windowX, windowY, windowWidth, windowHeight, &screenIndex);
			grabWidth = cropArea.mX2 - cropArea.mX1;
			grabHeight = cropArea.mY2 - cropArea.mY1;
			grabX = cropArea.mX1;
			grabY = cropArea.mY1;
			if (screenIndex != mLastFormat.mScreenIndex || grabWidth != mLastFormat.mPosition.getWidth() ||
			    grabHeight != mLastFormat.mPosition.getHeight()) {
				mLastFormat.mScreenIndex = screenIndex;
				mLastFormat.mPosition = cropArea;
				ms_message("[MsScreenSharing] New window size detected (%d,%d,%d,%d) at %d", grabX, grabY, grabWidth,
				           grabHeight, screenIndex);
				mLastFormat.mSizeChanged = true;
				mLastFormat.mLastTimeSizeChanged = std::chrono::system_clock::now();
			} else mLastFormat.mPosition = cropArea;
			if (!mLastFormat.mSizeChanged) {
				if (prepareImage()) {
					finalizeImage();
				}
			}

			auto targetTimeFrame = std::chrono::milliseconds(
			    MIN((int)(1000.0 / mFps), 333)); // Get potential new value. Cap to around 3fps
			auto workTimeFrame = (std::chrono::system_clock::now() - startTime);

			if (targetTimeFrame > workTimeFrame) {
				std::unique_lock<std::mutex> lock(mThreadLock);
				mThreadIterator.wait_for(lock, targetTimeFrame - workTimeFrame, [this] { return this->mToStop; });
			}
			startTime = std::chrono::system_clock::now();
		}
	}
	ms_message("[MsScreenSharing] Input thread stopped.%s", (mRunnable ? "" : " It was not Runnable"));
}

void MsScreenSharing::feed(MSFilter *filter) {
	if (mLastFormat.mSizeChanged) {
		if (std::chrono::system_clock::now() - mLastFormat.mLastTimeSizeChanged < std::chrono::milliseconds(200))
			return; // Avoid to burst replumbing (exemple of main issue if not : replumbing on each pixel change when
			        // moving window outside current screen)
		ms_message("[MsScreenSharing] New format to notify.");
		stop();
		ms_message("[MsScreenSharing] Notify new format (%d,%d,%d,%d)", mLastFormat.mPosition.mX1,
		           mLastFormat.mPosition.mY1, mLastFormat.mPosition.getWidth(), mLastFormat.mPosition.getHeight());
		ms_filter_notify_no_arg(filter, MS_FILTER_OUTPUT_FMT_CHANGED);
		mLastFormat.mSizeChanged = false;
	} else {
		mFrameLock.lock();
		// The main goal of the "Frame to Send" implementation is to avoid memory copy if not needed.
		// If there is no frame in queue:  Use the current frame for sending.
		if (!mFrameToSend) { // After the first set, mFrameToSend should be never empty.
			mFrameToSend = mFrameData;
			mFrameData = nullptr;
		}
		if (mFrameToSend) { // Repeat check in case of framedata was empty on first check
			mblk_t *newFrame = nullptr;
			if (isTimeToSend(filter->ticker->time)) {
				// Replace frame only on regular FPS
				if (mFrameData) { // We got one. Send old and replace it.
					newFrame = mFrameToSend;
					mFrameToSend = mFrameData;
					mFrameData = nullptr;
				}
			}
			if (newFrame || ms_video_capture_new_frame(&mIdleFramerateController, filter->ticker->time)) {
				uint32_t timestamp = (uint32_t)(filter->ticker->time * 90); // rtp uses a 90000 Hz clockrate for video
				if (!newFrame) newFrame = dupmsg(mFrameToSend); // Copy case: no new frame have arrived or idle FPS.
				mblk_set_precious_flag(newFrame, 1);
				mblk_set_timestamp_info(newFrame, timestamp); // prevent mirroring at the output
				ms_queue_put(filter->outputs[0], newFrame);
				ms_average_fps_update(&mAvgFps, filter->ticker->time);
			}
		}
		mFrameLock.unlock();
	}
}
//------------------------------------------------------------------------------------------------------------
//											C INTERFACE
//------------------------------------------------------------------------------------------------------------

void ms_screensharing_init(MSFilter *f) {
	SIData *d = new SIData();
	f->data = d;

	d->mScreenSharing.mFilter = f;
	d->mScreenSharing.init(); // need to init to have frame sizes on creation.
}

void ms_screensharing_uninit(MSFilter *f) {
	SIData *d = (SIData *)f->data;
	delete d;
}

void ms_screensharing_preprocess(BCTBX_UNUSED(MSFilter *f)) {
	ms_filter_lock(f);
	SIData *d = (SIData *)f->data;
	d->mScreenSharing.start();
	ms_filter_unlock(f);
}

void ms_screensharing_process(MSFilter *filter) {
	ms_filter_lock(filter);
	SIData *d = (SIData *)filter->data;
	if (d->mScreenSharing.mSourceDesc.type != MS_SCREEN_SHARING_EMPTY) d->mScreenSharing.feed(filter);
	d->lasttime = filter->ticker->time;
	ms_filter_unlock(filter);
}

void ms_screensharing_postprocess(MSFilter *f) {
	ms_filter_lock(f);
	SIData *d = (SIData *)f->data;
	d->mScreenSharing.stop();
	ms_filter_unlock(f);
}

static int ms_screensharing_set_fps(MSFilter *f, void *arg) {
	ms_filter_lock(f);
	SIData *d = (SIData *)f->data;
	d->mScreenSharing.setFps(*(float *)arg);
	ms_filter_unlock(f);
	return 0;
}

static int ms_screensharing_get_fps(MSFilter *filter, void *arg) {
	ms_filter_lock(filter);
	SIData *d = (SIData *)filter->data;
	if (filter->ticker) {
		*((float *)arg) = ms_average_fps_get(&d->mScreenSharing.mAvgFps);
	} else {
		*((float *)arg) = d->mScreenSharing.getFps();
	}
	ms_filter_unlock(filter);
	return 0;
}

int ms_screensharing_set_vsize(BCTBX_UNUSED(MSFilter *f), BCTBX_UNUSED(void *data)) {
	ms_warning("[MsScreenSharing] forcing size is not supported");
	return -1;
}

int ms_screensharing_get_vsize(MSFilter *f, void *data) {
	SIData *d = (SIData *)f->data;
	ms_filter_lock(f);
	if (!d->mScreenSharing.mRunnable) {
		d->mScreenSharing.uninit();
		d->mScreenSharing.init();
	}
	d->vsize.width = d->mScreenSharing.mLastFormat.mPosition.getWidth();
	d->vsize.height = d->mScreenSharing.mLastFormat.mPosition.getHeight();
	ms_filter_unlock(f);
	*(MSVideoSize *)data = d->vsize;
	return 0;
}

int ms_screensharing_get_pix_fmt(BCTBX_UNUSED(MSFilter *f), void *data) {
	SIData *d = (SIData *)f->data;
	*(MSPixFmt *)data = d->mScreenSharing.getPixFormat();
	return 0;
}

static int ms_screensharing_set_source_descriptor(MSFilter *f, void *arg) {
	ms_filter_lock(f);
	SIData *d = (SIData *)f->data;
	bool wasRunning = false;
	MsScreenSharing::FormatData saveData;
	if (d->mScreenSharing.mSourceDesc.type != MS_SCREEN_SHARING_EMPTY) {
		ms_message("[MSScreenSharing] Resetting new source descriptor");
		wasRunning = d->mScreenSharing.isRunning();
		// Last format is saved only if sharing is running: If not, format is not refreshing on change.
		if (wasRunning) saveData = d->mScreenSharing.mLastFormat;

	} else ms_message("[MSScreenSharing] Setting new source descriptor");
	d->mScreenSharing.mFilter = f;
	MSScreenSharingDesc descriptor = *(MSScreenSharingDesc *)arg;
	d->mScreenSharing.setSource(descriptor, saveData);
	d->mScreenSharing.init(); // need to init to have frame sizes on creation.
	if (wasRunning) {
		d->mScreenSharing.mLastFormat.mSizeChanged =
		    d->mScreenSharing.mLastFormat.mSizeChanged ||
		    saveData.mPosition.getHeight() != d->mScreenSharing.mLastFormat.mPosition.getHeight() ||
		    saveData.mPosition.getWidth() != d->mScreenSharing.mLastFormat.mPosition.getWidth();
		d->mScreenSharing.start();
	}
	ms_filter_unlock(f);
	return 0;
}

static int ms_screensharing_get_source_descriptor(MSFilter *f, void *arg) {
	SIData *d = (SIData *)f->data;
	*(void **)arg = &d->mScreenSharing.mSourceDesc;
	return 0;
}

extern "C" {
MSFilterMethod ms_screensharing_methods[] = {
    {MS_FILTER_SET_FPS, ms_screensharing_set_fps},
    {MS_FILTER_GET_FPS, ms_screensharing_get_fps},
    {MS_FILTER_SET_VIDEO_SIZE, ms_screensharing_set_vsize},
    {MS_FILTER_GET_VIDEO_SIZE, ms_screensharing_get_vsize},
    {MS_FILTER_GET_PIX_FMT, ms_screensharing_get_pix_fmt},
    {MS_SCREEN_SHARING_SET_SOURCE_DESCRIPTOR, ms_screensharing_set_source_descriptor},
    {MS_SCREEN_SHARING_GET_SOURCE_DESCRIPTOR, ms_screensharing_get_source_descriptor},
    {0, 0}};

MSFilterDesc ms_screensharing_filter_desc = {MS_SCREEN_SHARING_ID,
                                             "MSScreenSharing",
                                             "Screen sharing output",
                                             MS_FILTER_OTHER,
                                             NULL,
                                             0,
                                             1,
                                             ms_screensharing_init,
                                             ms_screensharing_preprocess,
                                             ms_screensharing_process,
                                             ms_screensharing_postprocess,
                                             ms_screensharing_uninit,
                                             ms_screensharing_methods,
                                             0};
} // extern "C"

MS_FILTER_DESC_EXPORT(ms_screensharing_filter_desc)
