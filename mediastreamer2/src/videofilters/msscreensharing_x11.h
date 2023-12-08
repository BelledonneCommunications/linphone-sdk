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

#ifndef _MS_SHARED_SCREEN_X11_H
#define _MS_SHARED_SCREEN_X11_H

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "msscreensharing_private.h"

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <atomic>
#include <thread>
#include <vector>

class MsScreenSharing_x11 : public MsScreenSharing {
public:
	Display *mDisplay;
	int mScreen;
	Window mRootWindow;
	Visual *mVisual;
	int mDepth;
	bool mUseShm;
	XImage *mImage;
	XShmSegmentInfo mShmInfo;
	bool mShmServerAttached;

public:
	MsScreenSharing_x11(MSScreenSharingDesc sourceDesc, FormatData formatData);

	virtual ~MsScreenSharing_x11();
	MsScreenSharing_x11(const MsScreenSharing_x11 &) = delete;

	void initDisplay();
	void clean();

	virtual void getWindowSize(int *windowX, int *windowY, int *windowWidth, int *windowHeight) const override;
	virtual bool prepareImage() override;
	virtual void finalizeImage() override;

private:
	void allocateImage(int width, int height);
	void cleanImage();
	void updateScreenConfiguration();
};

#endif
