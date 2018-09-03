/*
 Mediastreamer2 video-encoder.h
 Copyright (C) 2018 Belledonne Communications SARL

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

#pragma once

#include "mediastreamer2/msvideo.h"

namespace mediastreamer {

class VideoEncoder {
public:
	virtual ~VideoEncoder() = default;

	virtual MSVideoSize getVideoSize() const = 0;
	virtual void setVideoSize(const MSVideoSize &vsize) = 0;

	virtual float getFps() const = 0;
	virtual void setFps(float fps) = 0;

	virtual int getBitrate() const = 0;
	virtual void setBitrate(int bitrate) = 0;

	virtual bool isRunning() = 0;
	virtual void start() = 0;
	virtual void stop() = 0;

	virtual void feed(mblk_t *rawData, uint64_t time, bool requestIFrame = false) = 0;
	virtual bool fetch(MSQueue *encodedData) = 0;
};

}
