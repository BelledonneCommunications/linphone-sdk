/*
 Mediastreamer2 video-decoder-interface.h
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

#include <cstdint>

#include <ortp/str_utils.h>

#include "mediastreamer2/msqueue.h"

namespace mediastreamer {

class VideoDecoderInterface {
public:
	enum Status {
		noError,
		noFrameAvailable,
		decodingFailure
	};

	virtual ~VideoDecoderInterface() = default;

	virtual void waitForKeyFrame() = 0;

	virtual bool feed(MSQueue *encodedFrame, uint64_t timestamp) = 0;
	virtual Status fetch(mblk_t *&frame) = 0;
};

} // namespace mediastreamer
