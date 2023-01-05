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

#include <memory>

#include "mediastreamer2/mediastream.h"

namespace mediastreamer {

class ObuUnpacker {
public:
	enum Status { NoFrame, FrameAvailable, FrameCorrupted };

	virtual ~ObuUnpacker();

	Status unpack(mblk_t *im, MSQueue *output);
	void reset();

protected:
	mblk_t *feed(mblk_t *packet);
	bool isAggregating() const;
	mblk_t *completeAggregation();

	mblk_t *mFrame = nullptr;
	bool mInitializedRefCSeq = false;
	uint16_t mRefCSeq = 0;
};

} // namespace mediastreamer
