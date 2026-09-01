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

using namespace std;

namespace mediastreamer {

namespace {

constexpr uint8_t kObuTypeTemporalDelimiter = 2;

// leb128 as defined by AV1 §4.10.5: at most 8 bytes.
bool readLeb128(const uint8_t *data, size_t size, uint64_t &value, size_t &length) {
	value = 0;
	for (size_t i = 0; i < 8 && i < size; i++) {
		value |= (uint64_t)(data[i] & 0x7f) << (i * 7);
		if ((data[i] & 0x80) == 0) {
			length = i + 1;
			return true;
		}
	}
	return false;
}

size_t writeLeb128(uint8_t *out, uint64_t value) {
	size_t length = 0;
	do {
		out[length] = value & 0x7f;
		value >>= 7;
		if (value) out[length] |= 0x80;
		length++;
	} while (value);
	return length;
}

} // namespace

ObuUnpacker::~ObuUnpacker() {
	if (mFrame) freemsg(mFrame);
	if (mPendingFragment) freemsg(mPendingFragment);
}

void ObuUnpacker::reset() {
	discardFrame();
	mInitializedRefCSeq = false;
}

void ObuUnpacker::discardFrame() {
	if (mFrame) freemsg(mFrame);
	mFrame = nullptr;
	if (mPendingFragment) freemsg(mPendingFragment);
	mPendingFragment = nullptr;
	mFrameCorrupted = false;
}

ObuUnpacker::Status ObuUnpacker::unpack(mblk_t *im, MSQueue *output) {
	uint16_t cseq = mblk_get_cseq(im);
	bool marker = mblk_get_marker_info(im) != 0;
	bool wasCorrupted = mFrameCorrupted;

	if (im->b_cont) msgpullup(im, -1);

	if (!mInitializedRefCSeq) {
		mInitializedRefCSeq = true;
		mRefCSeq = cseq;
	} else {
		mRefCSeq++;
		if (mRefCSeq != cseq) {
			ms_message("ObuUnpacker: Sequence inconsistency detected (diff=%i)", (int)(cseq - mRefCSeq));
			mRefCSeq = cseq;
			// Packets are missing: neither the OBU under reassembly nor the
			// current temporal unit can be completed.
			if (mPendingFragment) {
				freemsg(mPendingFragment);
				mPendingFragment = nullptr;
			}
			mFrameCorrupted = true;
		}
	}

	processPacket(im);
	freemsg(im);

	if (marker && mPendingFragment) {
		ms_warning("ObuUnpacker: the temporal unit ends on an open OBU fragment. Dropping the frame.");
		mFrameCorrupted = true;
	}

	// Report a corrupted frame once, on the packet that condemned it.
	Status ret = (mFrameCorrupted && !wasCorrupted) ? FrameCorrupted : NoFrame;

	if (marker) {
		if (!mFrameCorrupted && mFrame) {
			msgpullup(mFrame, -1);
			ms_queue_put(output, mFrame);
			mFrame = nullptr;
			ret = FrameAvailable;
		}
		discardFrame();
	}

	return ret;
}

void ObuUnpacker::processPacket(mblk_t *packet) {
	uint8_t *cur = packet->b_rptr;
	uint8_t *end = packet->b_wptr;

	if (cur >= end) {
		ms_warning("ObuUnpacker: empty payload. Dropping the frame.");
		mFrameCorrupted = true;
		return;
	}

	const uint8_t aggregationHeader = *cur++;
	const bool zBit = (aggregationHeader & 0x80) != 0;  // first element continues an OBU of the previous packet
	const bool yBit = (aggregationHeader & 0x40) != 0;  // last element continues in the next packet
	const int wField = (aggregationHeader >> 4) & 0x03; // element count, 0 = every element is length-prefixed
	// N (0x08) flags a new coded video sequence; reassembly does not depend on it.

	if (cur >= end) {
		ms_warning("ObuUnpacker: packet carries no OBU element. Dropping the frame.");
		mFrameCorrupted = true;
		return;
	}

	int index = 0;
	while (cur < end) {
		index++;
		size_t elementSize;
		if (wField == 0 || index < wField) {
			uint64_t length;
			size_t lengthBytes;
			if (!readLeb128(cur, (size_t)(end - cur), length, lengthBytes) || length == 0 ||
			    length > (size_t)(end - cur) - lengthBytes) {
				ms_warning("ObuUnpacker: malformed OBU element length. Dropping the frame.");
				mFrameCorrupted = true;
				return;
			}
			cur += lengthBytes;
			elementSize = (size_t)length;
		} else {
			// The last of the W announced elements extends to the end of the payload.
			elementSize = (size_t)(end - cur);
		}

		const bool first = (index == 1);
		const bool last = (cur + elementSize == end);
		processObuElement(packet, cur, elementSize, first && zBit, last && yBit);
		cur += elementSize;
	}

	if (wField != 0 && index != wField) {
		ms_warning("ObuUnpacker: expected %i OBU elements, got %i. Dropping the frame.", wField, index);
		mFrameCorrupted = true;
	}
}

void ObuUnpacker::processObuElement(
    mblk_t *packet, uint8_t *start, size_t size, bool continuesPrevious, bool willContinue) {
	if (continuesPrevious && !mPendingFragment) {
		// The head of this OBU was in a packet we never received (loss, or
		// joining mid-stream): the element cannot be reassembled.
		mFrameCorrupted = true;
		return;
	}
	if (!continuesPrevious && mPendingFragment) {
		// Defensive: an open fragment this element does not continue. It cannot
		// happen without a sequence number gap, which already condemned the frame.
		freemsg(mPendingFragment);
		mPendingFragment = nullptr;
		mFrameCorrupted = true;
	}

	// Reference the element bytes without copying them.
	mblk_t *element = dupb(packet);
	element->b_rptr = start;
	element->b_wptr = start + size;

	if (mPendingFragment) concatb(mPendingFragment, element);
	else mPendingFragment = element;

	if (!willContinue) appendCompleteObu();
}

void ObuUnpacker::appendCompleteObu() {
	msgpullup(mPendingFragment, -1);
	if (!rewriteObu(mPendingFragment->b_rptr, (size_t)(mPendingFragment->b_wptr - mPendingFragment->b_rptr))) {
		mFrameCorrupted = true;
	}
	freemsg(mPendingFragment);
	mPendingFragment = nullptr;
}

bool ObuUnpacker::rewriteObu(const uint8_t *data, size_t size) {
	const uint8_t obuHeader = data[0];
	const uint8_t obuType = (obuHeader >> 3) & 0x0f;
	const bool hasExtension = (obuHeader & 0x04) != 0;
	const bool hasSizeField = (obuHeader & 0x02) != 0;
	const size_t headerSize = hasExtension ? 2 : 1;

	if (size < headerSize) {
		ms_warning("ObuUnpacker: OBU shorter than its own header. Dropping the frame.");
		return false;
	}

	const uint8_t *payload = data + headerSize;
	size_t payloadSize = size - headerSize;

	// The sender may have kept obu_size on the wire (the specification
	// recommends omitting it but allows it). Strip it: the rewritten OBU
	// carries its own.
	if (hasSizeField) {
		uint64_t declaredSize;
		size_t lengthBytes;
		if (!readLeb128(payload, payloadSize, declaredSize, lengthBytes)) {
			ms_warning("ObuUnpacker: malformed obu_size field. Dropping the frame.");
			return false;
		}
		payload += lengthBytes;
		payloadSize -= lengthBytes;
		if (declaredSize > payloadSize) {
			ms_warning("ObuUnpacker: obu_size larger than the OBU element. Dropping the frame.");
			return false;
		}
		// A smaller obu_size flags trailing padding: follow it.
		payloadSize = (size_t)declaredSize;
	}

	// Forbidden on the wire and rebuilt by no one: a temporal delimiter that
	// arrives anyway is dropped, not treated as an error.
	if (obuType == kObuTypeTemporalDelimiter) return true;

	mblk_t *obu = allocb(headerSize + 8 + payloadSize, 0);
	*obu->b_wptr++ = obuHeader | 0x02;
	if (hasExtension) *obu->b_wptr++ = data[1];
	obu->b_wptr += writeLeb128(obu->b_wptr, payloadSize);
	memcpy(obu->b_wptr, payload, payloadSize);
	obu->b_wptr += payloadSize;

	if (mFrame) concatb(mFrame, obu);
	else mFrame = obu;

	return true;
}

} // namespace mediastreamer
