/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

} // namespace mediastreamer
