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

#ifndef _MS_SCREEN_SHARING_PRIVATE_H
#define _MS_SCREEN_SHARING_PRIVATE_H

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msscreensharing.h"
#include "mediastreamer2/msvideo.h"

#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>

class MsScreenSharing {
public:
	struct Rect {
		int mX1 = 0, mY1 = 0, mX2 = 0, mY2 = 0;
		inline Rect() {
		}
		inline Rect(int x1, int y1, int x2, int y2) : mX1(x1), mY1(y1), mX2(x2), mY2(y2) {
		}
		inline int getWidth() const {
			return mX2 - mX1;
		}
		inline int getHeight() const {
			return mY2 - mY1;
		}
	};
	struct FormatData {
		Rect mPosition;
		int mScreenIndex = 0;
		bool mRecordCursor = true;
		bool mSizeChanged = false;
		std::chrono::system_clock::time_point mLastTimeSizeChanged;
		MSPixFmt mPixelFormat = MS_PIX_FMT_UNKNOWN;
	};

	bool mRunning = false;
	bool mRunnable = true;
	std::vector<Rect> mScreenRects;
	std::vector<Rect> mScreenDeadSpace;

	std::thread mThread;
	static std::condition_variable mThreadIterator;
	std::mutex mThreadLock;
	FormatData mLastFormat;
	bool mToStop = false;

public:
	MsScreenSharing();
	virtual ~MsScreenSharing();
	MsScreenSharing(const MsScreenSharing &) = delete;
	
	virtual void setSource(MSScreenSharingDesc sourceDesc, FormatData formatData);

	virtual void init();
	virtual void uninit();
	void start();
	void stop();
	// screenRects: Global coordinates
	void updateScreenConfiguration(const std::vector<Rect> &screenRects);

	virtual MSPixFmt getPixFormat() const;

	// Return the position of the current Window in global coordinates.
	virtual void getWindowSize(int *windowX, int *windowY, int *windowWidth, int *windowHeight) const;
	virtual bool prepareImage() {
		return true;
	};
	virtual void finalizeImage(){};
	// Get cropped rectangle in Window from all screen rectangles.
	// When the area is splitted in multiple screens, the algo choose the crop with the highest area.
	// @return Global coordinate
	Rect getCroppedArea(int windowX, int windowY, int windowWidth, int windowHeight, int *screenIndex) const;

	bool isRunning() const;

	double getFps();
	virtual void setFps(const float &pFps);

	virtual void inputThread();
	// Client API will change the mFrameData. The feed() implementation use mFrameToSend.
	mblk_t *mFrameData = nullptr, *mFrameToSend = nullptr;
	std::mutex mFrameLock;
	MSFrameRateController mFramerateController, mIdleFramerateController;
	MSAverageFPS mAvgFps;
	float mFps = 10.0;
	bool_t isTimeToSend(uint64_t tickerTime);
	void feed(MSFilter *filter);
	MSFilter *mFilter = nullptr;
	MSYuvBufAllocator *mAllocator = nullptr;
	MSScreenSharingDesc mSourceDesc;
};

#endif
