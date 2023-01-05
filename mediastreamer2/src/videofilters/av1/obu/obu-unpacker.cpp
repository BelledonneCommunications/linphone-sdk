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

#include "obu-unpacker.h"

#include <stdexcept>

using namespace std;

namespace mediastreamer {

ObuUnpacker::~ObuUnpacker() {
	if (mFrame) freemsg(mFrame);
}

ObuUnpacker::Status ObuUnpacker::unpack(mblk_t *im, MSQueue *output) {
	uint16_t cseq = mblk_get_cseq(im);
	Status ret = NoFrame;

	if (im->b_cont) msgpullup(im, -1);

	if (!mInitializedRefCSeq) {
		mInitializedRefCSeq = true;
		mRefCSeq = cseq;
	} else {
		mRefCSeq++;
		if (mRefCSeq != cseq) {
			ms_message("ObuUnpacker: Sequence inconsistency detected (diff=%i)", (int)(cseq - mRefCSeq));
			mRefCSeq = cseq;
			ret = FrameCorrupted;
		}
	}

	mblk_t *om = feed(im);
	if (om) {
		ret = FrameAvailable;
		ms_queue_put(output, om);
	}

	return ret;
}

void ObuUnpacker::reset() {
	if (mFrame) freemsg(mFrame);
	mFrame = nullptr;
	mInitializedRefCSeq = false;
}

mblk_t *ObuUnpacker::feed(mblk_t *packet) {
	uint8_t *header = packet->b_rptr;
	bool isFirst = (*header >> 3) & 0x1;
	int marker = mblk_get_marker_info(packet);

	// Remove aggregation header
	packet->b_rptr += 1;

	if (isAggregating() && isFirst) {
		ms_error("ObuUnpacker: Received the first packet of a video sequence while already aggregating. Dropping the "
		         "current frame.");
		if (mFrame) freemsg(mFrame);
		mFrame = packet;

		return marker ? completeAggregation() : nullptr;
	}

	if (!isAggregating() && !isFirst) {
		ms_error("ObuUnpacker: Received a continuation packet while aggregation is not started. Dropping packet.");
		freemsg(packet);

		return nullptr;
	}

	if (!isAggregating()) {
		mFrame = packet;
	} else {
		concatb(mFrame, packet);
	}

	return marker ? completeAggregation() : nullptr;
}

bool ObuUnpacker::isAggregating() const {
	return mFrame != nullptr;
}

mblk_t *ObuUnpacker::completeAggregation() {
	if (!isAggregating()) return nullptr;

	mblk_t *om = mFrame;
	msgpullup(om, -1);

	mFrame = nullptr;

	return om;
}

} // namespace mediastreamer
