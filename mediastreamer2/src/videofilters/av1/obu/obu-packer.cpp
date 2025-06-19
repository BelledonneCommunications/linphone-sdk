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

#include "obu-packer.h"

#include <algorithm>
#include <stdexcept>

using namespace std;

namespace mediastreamer {

ObuPacker::ObuPacker(size_t maxPayloadSize) : mMaxPayloadSize(maxPayloadSize) {
}

void ObuPacker::setMaxPayloadSize(size_t size) {
	mMaxPayloadSize = size;
}

void ObuPacker::enableAggregation(bool enable) {
	mAggregationEnabled = enable;
}

void ObuPacker::pack(MSQueue *input, MSQueue *output, uint32_t ts) {
	while (mblk_t *m = ms_queue_get(input)) {
		size_t size = msgdsize(m);

		vector<ParsedObu> obus;

		size_t currentPos = 0;
		while (currentPos < size) {
			try {
				auto obu = parseNextObu(m->b_rptr + currentPos, size - currentPos);

				// TD Obus are dropped
				if (obu.type != OBP_OBU_TEMPORAL_DELIMITER) {
					obus.push_back(obu);
				}

				currentPos += obu.size;
			} catch (runtime_error &e) {
				ms_error("ObuPacker: Failed to parse OBU header: %s", e.what());
				obus.clear();
				break;
			}
		}

		if (!obus.empty()) sendObus(obus, output, ts, m);

		freemsg(m);
	}
}

void ObuPacker::enableDivideIntoEqualSize(bool enable) {
	mEqualSizeEnabled = enable;
}

ObuPacker::ParsedObu ObuPacker::parseNextObu(uint8_t *buf, size_t size) {
	char errBuf[1024];
	ptrdiff_t offset;
	size_t obuSize;
	int temporalId, spatialId;
	OBPOBUType obuType;
	OBPError err = {&errBuf[0], 1024};

	if (obp_get_next_obu(buf, size, &obuType, &offset, &obuSize, &temporalId, &spatialId, &err) < 0) {
		throw runtime_error(err.error);
	}
	//  else {
	// 	ms_message("ObuPacker [%p]: type = %d, size = %d, temporal = %d, spatial = %d", this, obuType,
	// 	           (int)(obuSize + offset), temporalId, spatialId);
	// }

	return {obuType, buf, obuSize + offset};
}

void ObuPacker::sendObus(vector<ParsedObu> &obus, MSQueue *output, uint32_t ts, mblk_t *source) {
	for (size_t i = 0, e = obus.size(); i != e; ++i) {
		const auto &obu = obus[i];
		int packetsNumber = 0;
		size_t maxPacketSize = ms_video_payload_sizes(obu.size, mMaxPayloadSize, mEqualSizeEnabled, &packetsNumber);
		size_t remainingSize = obu.size;

		if (obu.size + 1 < mMaxPayloadSize) {
			mblk_t *m = makePacket(obu.start, obu.size + 1, false, false, (i == 0), (i == (e - 1)), ts, source);
			ms_queue_put(output, m);
		} else {
			bool firstPacket = true;

			for (int j = 0; j < packetsNumber; j++) {
				size_t minSize = std::min<size_t>(remainingSize + 1, maxPacketSize);
				mblk_t *m = makePacket(obu.start + obu.size - remainingSize, minSize, !firstPacket,
				                       (remainingSize + 1 > maxPacketSize), (firstPacket && i == 0),
				                       (i == (e - 1) && j == (packetsNumber - 1)), ts, source);
				ms_queue_put(output, m);

				if (firstPacket) firstPacket = false;

				remainingSize -= minSize - 1;
			}
		}
	}
}

mblk_t *
ObuPacker::makePacket(uint8_t *buf, size_t size, bool Z, bool Y, bool N, bool marker, uint32_t ts, mblk_t *source) {
	mblk_t *m = allocb(size, -1);

	memcpy(m->b_rptr + 1, buf, size - 1);
	m->b_wptr += size;

	uint8_t *agHdr = m->b_rptr;
	*agHdr = 0;

	*agHdr |= (Z ? 0x1 : 0x0) << 7; // Z
	*agHdr |= (Y ? 0x1 : 0x0) << 6; // Y
	*agHdr |= 0x1 << 4;             // W
	*agHdr |= (N ? 0x1 : 0x0) << 3; // N

	mblk_set_timestamp_info(m, ts);
	mblk_set_independent_flag(m, mblk_get_independent_flag(source));
	mblk_set_discardable_flag(m, mblk_get_discardable_flag(source));
	mblk_set_marker_info(m, marker ? 1 : 0);
	mblk_set_cseq(m, mSeq++);
	return m;
}

} // namespace mediastreamer
