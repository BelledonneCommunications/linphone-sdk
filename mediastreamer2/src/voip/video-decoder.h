/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>

#include <ortp/str_utils.h>

#include "mediastreamer2/msqueue.h"

namespace mediastreamer {

class VideoDecoder {
public:
	enum Status {
		noError,
		noFrameAvailable,
		decodingFailure
	};

	virtual ~VideoDecoder() = default;

	virtual void waitForKeyFrame() = 0;

	virtual bool feed(MSQueue *encodedFrame, uint64_t timestamp) = 0;
	virtual Status fetch(mblk_t *&frame) = 0;
};

} // namespace mediastreamer
