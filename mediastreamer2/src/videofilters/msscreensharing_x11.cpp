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
#include "mediastreamer2/x11_helper.h"
#include "msscreensharing_x11.h"

#include <sys/stat.h>

#include <X11/Xutil.h>
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
#include <algorithm>
#include <map>
#include <mutex>
#include <sys/shm.h>

MsScreenSharing_x11::MsScreenSharing_x11() : MsScreenSharing() {
}

MsScreenSharing_x11::~MsScreenSharing_x11() {
	stop();
	cleanImage();
}

void MsScreenSharing_x11::setSource(MSScreenSharingDesc sourceDesc, FormatData formatData) {
	MsScreenSharing::setSource(sourceDesc, formatData);
	if (mLastFormat.mPixelFormat == MS_PIX_FMT_UNKNOWN) mLastFormat.mPixelFormat = MS_RGBA32_REV;
	mDisplay = NULL;
	mImage = NULL;

	mShmInfo.shmseg = 0;
	mShmInfo.shmid = -1;
	mShmInfo.shmaddr = (char *)-1;
	mShmInfo.readOnly = false;
	mShmServerAttached = false;
	switch (mSourceDesc.type) {
		case MSScreenSharingType::MS_SCREEN_SHARING_DISPLAY:
			mLastFormat.mScreenIndex = *(uintptr_t *)(&mSourceDesc.native_data);
			mRootWindow = 0;
			break;
		case MSScreenSharingType::MS_SCREEN_SHARING_WINDOW:
			mRootWindow = (Window)mSourceDesc.native_data;
			break;
		case MSScreenSharingType::MS_SCREEN_SHARING_AREA:
			ms_error("[MSScreenSharing] Sharing an area is not supported.");
			break;
		default:
			mRootWindow = 0;
			mLastFormat.mScreenIndex = 01;
	}
	initDisplay();
	init();
}

// It must return global coordinate and not window relative.
void MsScreenSharing_x11::getWindowSize(int *windowX, int *windowY, int *windowWidth, int *windowHeight) const {
	XWindowAttributes windowAttributes;
	if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_DISPLAY &&
	    mLastFormat.mScreenIndex < static_cast<int>(mScreenRects.size())) {
		auto rect = mScreenRects[mLastFormat.mScreenIndex];
		*windowX = rect.mX1;
		*windowY = rect.mY1;
		*windowWidth = rect.getWidth();
		*windowHeight = rect.getHeight();
	} else if (mDisplay) {
		XGetWindowAttributes(mDisplay, mRootWindow,
							 &windowAttributes); // Coordinates are not global but relative to the root window.
		*windowWidth = windowAttributes.width;
		*windowHeight = windowAttributes.height;
		int x, y;
		Window w;
		XTranslateCoordinates(mDisplay, mRootWindow, windowAttributes.root, 0, 0, &x, &y,
							  &w); // Convert origin coordinates to coordinate in root which should be the screen.

		*windowX = x;
		*windowY = y;
	}
}

void MsScreenSharing_x11::initDisplay() {
	const char *display = getenv("DISPLAY");
	if (display == NULL) display = ":0";
	mDisplay = XOpenDisplay(display);
	if (mDisplay == NULL) {
		ms_error("[MsScreenSharing_x11] Can't open X display!");
		return;
	}
	mScreen = DefaultScreen(mDisplay);
	if (mRootWindow == 0) mRootWindow = RootWindow(mDisplay, mScreen);
	mVisual = DefaultVisual(mDisplay, mScreen);
	mDepth = DefaultDepth(mDisplay, mScreen);
	mUseShm = XShmQueryExtension(mDisplay);
	if (mUseShm) {
		ms_message("[MsScreenSharing_x11] Using X11 shared memory.");
	} else {
		ms_message("[MsScreenSharing_x11] Not using X11 shared memory.");
	}

	// showing the cursor requires XFixes (which should be supported on any modern X server, but let's check it anyway)
	if (mLastFormat.mRecordCursor) {
#ifdef HAVE_XFIXES
		int event, error;
		if (!XFixesQueryExtension(mDisplay, &event, &error)) {
			ms_warning("[MsScreenSharing_x11] Warning: XFixes is not supported by X server, the cursor has been "
			           "hidden.");
			mLastFormat.mRecordCursor = false;
		}
#else
		ms_warning("[MsScreenSharing_x11] Warning: XFixes is not supported, the cursor has been hidden.");
		mLastFormat.mRecordCursor = false;
#endif
	}

	updateScreenConfiguration();
	mToStop = false;
}

void MsScreenSharing_x11::clean() {
	cleanImage();
	if (mDisplay != NULL) {
		XCloseDisplay(mDisplay);
		mDisplay = NULL;
	}
}

void MsScreenSharing_x11::allocateImage(int width, int height) {
	if (mShmServerAttached && mImage && mImage->width == width && mImage->height == height) {
		return; // reuse existing image
	}
	cleanImage();
	mImage = XShmCreateImage(mDisplay, mVisual, mDepth, ZPixmap, NULL, &mShmInfo, width, height);
	if (mImage == NULL) {
		ms_error("[MsScreenSharing_x11] Can't create shared image!");
		return;
	}
	mShmInfo.shmid = shmget(IPC_PRIVATE, mImage->bytes_per_line * mImage->height, IPC_CREAT | 0700);
	if (mShmInfo.shmid == -1) {
		ms_error("[MsScreenSharing_x11] Can't get shared memory!");
		return;
	}
	mShmInfo.shmaddr = (char *)shmat(mShmInfo.shmid, NULL, SHM_RND);
	if (mShmInfo.shmaddr == (char *)-1) {
		ms_error("[MsScreenSharing_x11] Can't attach to shared memory!");
		return;
	}
	mImage->data = mShmInfo.shmaddr;
	if (!XShmAttach(mDisplay, &mShmInfo)) {
		ms_error("[MsScreenSharing_x11] Can't attach server to shared memory!");
		return;
	}
	mShmServerAttached = true;
}

void MsScreenSharing_x11::cleanImage() {
	if (mShmServerAttached) {
		XShmDetach(mDisplay, &mShmInfo);
		mShmServerAttached = false;
	}
	if (mShmInfo.shmaddr != (char *)-1) {
		shmdt(mShmInfo.shmaddr);
		mShmInfo.shmaddr = (char *)-1;
	}
	if (mShmInfo.shmid != -1) {
		shmctl(mShmInfo.shmid, IPC_RMID, NULL);
		mShmInfo.shmid = -1;
	}
	if (mImage != NULL) {
		XDestroyImage(mImage);
		mImage = NULL;
	}
}

void MsScreenSharing_x11::updateScreenConfiguration() {
	ms_message("[MsScreenSharing_x11] Detecting screen configuration ...");
	// get screen rectangles
	mScreenRects.clear();
#ifdef HAVE_XINERAMA
	int event_base, error_base;
	if (XineramaQueryExtension(mDisplay, &event_base, &error_base)) {
		int num_screens;
		XineramaScreenInfo *screens = XineramaQueryScreens(mDisplay, &num_screens);
		try {
			for (int i = 0; i < num_screens; ++i) {
				mScreenRects.emplace_back(screens[i].x_org, screens[i].y_org, screens[i].x_org + screens[i].width,
				                          screens[i].y_org + screens[i].height);
			}
		} catch (...) {
			XFree(screens);
			throw;
		}
		XFree(screens);
	} else {
		ms_warning("[MsScreenSharing_x11] Xinerama is not supported by X server, multi-monitor support "
		           "may not work properly.");
		return;
	}
#else
	ms_warning(
	    "[MsScreenSharing_x11] Warning: Xinerama is not supported, multi-monitor support may not work properly.");
	return;
#endif
	MsScreenSharing::updateScreenConfiguration(mScreenRects);
}

bool MsScreenSharing_x11::prepareImage() {
	int width = mLastFormat.mPosition.getWidth();
	int height = mLastFormat.mPosition.getHeight();
	int x, y;
	if (mSourceDesc.type == MSScreenSharingType::MS_SCREEN_SHARING_DISPLAY) {
		x = mLastFormat.mPosition.mX1;
		y = mLastFormat.mPosition.mY1;
	} else { // X,Y in XShmGetImage are relative to the window. We want all.
		x = 0;
		y = 0;
	}
	if (mUseShm) {
		allocateImage(width, height);
		if (!XShmGetImage(mDisplay, mRootWindow, mImage, x, y, AllPlanes)) {
			XWindowAttributes windowAttributes;
			XGetWindowAttributes(mDisplay, mRootWindow, &windowAttributes);
			x = windowAttributes.width - mLastFormat.mPosition.getWidth();
			y = windowAttributes.height - mLastFormat.mPosition.getHeight();
			if (!XShmGetImage(mDisplay, mRootWindow, mImage, x, y, AllPlanes)) {
				ms_error(
					"[MsScreenSharing_x11] Can't get image (using shared memory)! "
					"Grabber : "
					"(x,y,width,height) = (%i,%i,%i,%i).Window = (%i,%i,%i,%i)\nUsually this means the recording area "
					"is not "
					"completely "
					"inside the screen. Or did you change the screen resolution?",
					x, y, width, height, windowAttributes.x, windowAttributes.y, windowAttributes.width,
					windowAttributes.height);
				return false;
			} else {
				mLastFormat.mPosition.mX1 = x;
				mLastFormat.mPosition.mX2 = x + width;
				mLastFormat.mPosition.mY1 = y;
				mLastFormat.mPosition.mY2 = y + height;
			}
		}
	} else {
		if (mImage != NULL) {
			XDestroyImage(mImage);
			mImage = NULL;
		}
		mImage = XGetImage(mDisplay, mRootWindow, x, y, width, height, AllPlanes, ZPixmap);
		if (mImage == NULL) {
			ms_error("[MsScreenSharing_x11] Can't get image (not using shared memory)!\n "
			         "Usually this means the recording area is not completely inside the screen. Or did you change "
			         "the screen resolution?");
			return false;
		}
	}

	auto imagePixelFormat = ms_x11_image_get_pixel_format(mImage);
	if (mLastFormat.mPixelFormat != imagePixelFormat) {
		mLastFormat.mPixelFormat = imagePixelFormat;
		mLastFormat.mSizeChanged = true;
		mLastFormat.mLastTimeSizeChanged = std::chrono::system_clock::now();
		return false;
	}
	// draw the cursor.
	if (mLastFormat.mRecordCursor) {
		ms_x11_image_draw_cursor(mDisplay, mImage, mLastFormat.mPosition.mX1, mLastFormat.mPosition.mY1);
	}
	// clear the dead space. A crop was already. Algo is kept for further use, in case we want to erase some area.
	// applied.
	// x = mLastFormat.mPosition.mX1;
	// y = mLastFormat.mPosition.mY1;
	// for (size_t i = 0; i < mScreenDeadSpace.size(); ++i) {
	//	Rect rect = mScreenDeadSpace[i];
	//	if (rect.mX1 < x) rect.mX1 = x;
	//	if (rect.mY1 < y) rect.mY1 = y;
	//	if (rect.mX2 > x + width) rect.mX2 = x + width;
	//	if (rect.mY2 > y + height) rect.mY2 = y + height;
	//	if (rect.mX2 > rect.mX1 && rect.mY2 > rect.mY1)
	//		ms_x11_image_clear_rectangle(mImage, rect.mX1 - x, rect.mY1 - y, rect.mX2 - rect.mX1,
	//									 rect.mY2 - rect.mY1);
	//}
	return true;
}

void MsScreenSharing_x11::finalizeImage() {
	uint8_t *imageData = (uint8_t *)mImage->data;
	int imageStride = mImage->bytes_per_line;
	unsigned int imageSize = imageStride * mLastFormat.mPosition.getHeight();
	bool haveData = false;
	for (unsigned int i = 0;
	     !haveData && i < mLastFormat.mPosition.getHeight() * static_cast<unsigned int>(abs(imageStride)); ++i)
		if (imageData[i] != '\0') {
			haveData = true;
		}
	mFrameLock.lock();
	if (mFrameData) freemsg(mFrameData);
	mFrameData = nullptr;
	mFrameData = ms_yuv_allocator_get(mAllocator, imageSize, mLastFormat.mPosition.getWidth(),
	                                  mLastFormat.mPosition.getHeight());
	if (mFrameData) {
		memcpy(mFrameData->b_rptr, imageData, imageSize);
	}
	mFrameLock.unlock();
}
