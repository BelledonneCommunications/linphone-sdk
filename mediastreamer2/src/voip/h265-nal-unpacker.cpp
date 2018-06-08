/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2015  Belledonne Communications <info@belledonne-communications.com>

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

#include "h265-nal-unpacker.h"
#include "h265-utils.h"

using namespace std;

namespace mediastreamer {

mblk_t *H265NalUnpacker::FuAggregator::feed(mblk_t *packet) {
	if (packet->b_wptr - packet->b_rptr < 3) {
		ms_error("Dropping H265 FU packet smaller than 3 bytes");
		freemsg(packet);
		return nullptr;
	}

	H265NaluHeader naluHeader(packet->b_rptr);
	packet->b_rptr += 2;

	H265FuHeader fuHeader(packet->b_rptr++);
	naluHeader.setType(fuHeader.getType());

	if (fuHeader.getPosition() == H265FuHeader::Position::Start && isAggregating()) {
		ms_error("receiving start FU packet while aggregating. Dropping the under construction NALu");
		reset();
		_m = packet;
		return nullptr;
	}

	if (fuHeader.getPosition() != H265FuHeader::Position::Start && !isAggregating()) {
		ms_error("receiving continuation FU packet while aggregation hasn't been started. Doping packet");
		freemsg(packet);
		return nullptr;
	}

	if (fuHeader.getPosition() == H265FuHeader::Position::Start) {
		_m = naluHeader.forge();
	}

	concatb(_m, packet);

	if (fuHeader.getPosition() == H265FuHeader::Position::End) {
		return completeAggregation();
	} else return nullptr;
}

void H265NalUnpacker::FuAggregator::reset() {
	if (_m != nullptr) freemsg(_m);
	_m = nullptr;
}

mblk_t *H265NalUnpacker::FuAggregator::completeAggregation() {
	if (!isAggregating()) return nullptr;
	mblk_t *res = _m;
	msgpullup(res, -1);
	_m = nullptr;
	return res;
}

void H265NalUnpacker::ApSpliter::feed(mblk_t *packet) {
	ms_queue_flush(&_q);

	if (packet->b_wptr - packet->b_rptr < 2) {
		ms_error("Dropping H265 aggregation packet smaller than 2 bytes");
		freemsg(packet);
		return;
	}

	const uint8_t *it;
	for (it = packet->b_rptr; it < packet->b_wptr;) {
		if (packet->b_wptr - it < 2) break;
		uint16_t naluSize = ntohs(*reinterpret_cast<const uint16_t *>(it));
		it += 2;

		if (it + naluSize > packet->b_wptr) break;
		mblk_t *m = allocb(naluSize, 0);
		memcpy(m->b_wptr, it, naluSize);
		m->b_wptr += naluSize;
		ms_queue_put(&_q, m);

		it += naluSize;
	}

	if (it != packet->b_wptr) {
		ms_error("Dropping H265 aggregation packet containing truncated NALus");
		ms_queue_flush(&_q);
	}
}

NalUnpacker::PacketType H265NalUnpacker::getNaluType(const mblk_t *nalu) const {
	H265NaluHeader header(nalu->b_rptr);
	if (header.getType() == H265NaluType::Ap) {
		return PacketType::AggregationPacket;
	} else if (header.getType() == H265NaluType::Fu) {
		return PacketType::FragmentationUnit;
	} else {
		return PacketType::SingleNalUnit;
	}
}

} // namespace mediastreamer
