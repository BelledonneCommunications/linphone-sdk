/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
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

#include <queue>

#include <dav1d/dav1d.h>

#include "../obu/obu-key-frame-indicator.h"
#include "mediastreamer2/msvideo.h"
#include "video-decoder.h"

namespace mediastreamer {

class Av1Decoder : public VideoDecoder {
public:
	Av1Decoder();
	virtual ~Av1Decoder();

	void waitForKeyFrame() override {
		mKeyFrameNeeded = true;
	}

	bool feed(MSQueue *encodedFrame, uint64_t timestamp) override;
	Status fetch(mblk_t *&frame) override;

	void flush();

protected:
	Dav1dSettings mSettings{};
	Dav1dContext *mContext = nullptr;

	ObuKeyFrameIndicator mKeyFrameIndicator;

	MSYuvBufAllocator *mYuvBufAllocator = nullptr;

	std::queue<Dav1dPicture> mPendingFrames;

	bool mKeyFrameNeeded = true;
};

} // namespace mediastreamer
