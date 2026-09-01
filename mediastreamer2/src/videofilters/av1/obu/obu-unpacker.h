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

/**
 * Reassembles temporal units from RTP packets carrying the "RTP Payload Format
 * For AV1" (AOMedia specification, v1.0).
 *
 * Each packet starts with the aggregation header:
 *
 *     0 1 2 3 4 5 6 7
 *    +-+-+-+-+-+-+-+-+
 *    |Z|Y| W |N|-|-|-|
 *    +-+-+-+-+-+-+-+-+
 *
 * and carries one or more OBU elements. When W is 0 every element is prefixed
 * by its leb128 length; when W > 0 the packet holds exactly W elements, the
 * first W-1 length-prefixed and the last one extending to the end of the
 * payload. Z flags the first element as the continuation of an OBU started in
 * the previous packet, Y flags the last element as continuing in the next
 * packet, and the RTP marker bit closes the temporal unit.
 *
 * Senders are recommended to omit obu_size fields (the element length already
 * carries the size), but may keep them. Whatever the input form, each
 * reassembled OBU is rewritten with obu_has_size_field set, so the delivered
 * temporal unit is a self-delimiting low-overhead bitstream that dav1d and
 * obuparse can consume directly.
 */
class ObuUnpacker {
public:
	enum Status { NoFrame, FrameAvailable, FrameCorrupted };

	virtual ~ObuUnpacker();

	Status unpack(mblk_t *im, MSQueue *output);
	void reset();

private:
	void processPacket(mblk_t *packet);
	void processObuElement(mblk_t *packet, uint8_t *start, size_t size, bool continuesPrevious, bool willContinue);
	void appendCompleteObu();
	bool rewriteObu(const uint8_t *data, size_t size);
	void discardFrame();

	mblk_t *mFrame = nullptr;           // completed OBUs of the current temporal unit, in low-overhead format
	mblk_t *mPendingFragment = nullptr; // OBU under reassembly, raw element bytes
	bool mFrameCorrupted = false;
	bool mInitializedRefCSeq = false;
	uint16_t mRefCSeq = 0;
};

} // namespace mediastreamer
