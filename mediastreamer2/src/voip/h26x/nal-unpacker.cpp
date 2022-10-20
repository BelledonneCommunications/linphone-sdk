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

#include <bitset>

#include "nal-unpacker.h"

using namespace std;

namespace mediastreamer {

NalUnpacker::Status &NalUnpacker::Status::operator|=(const Status &s2) {
	this->frameAvailable = (this->frameAvailable || s2.frameAvailable);
	this->frameCorrupted = (this->frameCorrupted || s2.frameCorrupted);
	this->isKeyFrame = (this->isKeyFrame || s2.isKeyFrame);
	return *this;
}

unsigned int NalUnpacker::Status::toUInt() const {
	bitset<3> flags;
	if (frameAvailable) flags.set(0);
	if (frameCorrupted) flags.set(1);
	if (isKeyFrame) flags.set(2);
	return flags.to_ulong();
}

NalUnpacker::NalUnpacker(FuAggregatorInterface *aggregator, ApSpliterInterface *spliter): _fuAggregator(aggregator), _apSpliter(spliter) {
	ms_queue_init(&_q);
}

NalUnpacker::Status NalUnpacker::unpack(mblk_t *im, MSQueue *out) {
	PacketType type = getNaluType(im);
	int marker = mblk_get_marker_info(im);
	uint32_t ts = mblk_get_timestamp_info(im);
	uint16_t cseq = mblk_get_cseq(im);
	Status ret;

	if (_lastTs != ts) {
		/*a new frame is arriving, in case the marker bit was not set in previous frame, output it now,
		 *	 unless it is a FU-A packet (workaround for buggy implementations)*/
		_lastTs = ts;
		if (!_fuAggregator->isAggregating() && !ms_queue_empty(&_q)) {
			Status status;
			status.frameAvailable = true;
			status.frameCorrupted = true;
			ret = outputFrame(out, status);
			ms_warning("Incomplete H264 frame (missing marker bit after seq number %u)",
			           mblk_get_cseq(ms_queue_peek_last(out)));
		}
	}

	if (im->b_cont) msgpullup(im, -1);

	if (!_initializedRefCSeq) {
		_initializedRefCSeq = TRUE;
		_refCSeq = cseq;
	} else {
		_refCSeq++;
		if (_refCSeq != cseq) {
			ms_message("sequence inconsistency detected (diff=%i)", (int)(cseq - _refCSeq));
			_refCSeq = cseq;
			_status.frameCorrupted = true;
		}
	}

	switch (type) {
		case PacketType::SingleNalUnit:
			_fuAggregator->reset();
			/*single nal unit*/
			ms_debug("Receiving single NAL");
			storeNal(im);
			break;
		case PacketType::FragmentationUnit: {
			try {
				ms_debug("Receiving FU-A");
				mblk_t *o = _fuAggregator->feed(im);
				if (o) storeNal(o);
			} catch (const invalid_argument &e) {
				ms_error("%s", e.what());
				_fuAggregator->reset();
				_status.frameCorrupted = true;
			}
			break;
		}
		case PacketType::AggregationPacket:
			ms_debug("Receiving STAP-A");
			_apSpliter->feed(im);
			while ((im = ms_queue_get(_apSpliter->getNalus()))) {
				storeNal(im);
			}
			break;
	}

	if (marker) {
		_lastTs = ts;
		ms_debug("Marker bit set");
		Status status;
		status.frameAvailable = true;
		ret = outputFrame(out, status);
	}

	return ret;
}

void NalUnpacker::reset() {
	ms_queue_flush(&_q);
	_status = Status();
	_initializedRefCSeq = false;
	_fuAggregator->reset();
}

NalUnpacker::Status NalUnpacker::outputFrame(MSQueue *out, const Status &flags) {
	Status res = _status;
	if (!ms_queue_empty(out)) {
		ms_warning("rfc3984_unpack: output_frame invoked several times in a row, this should not happen");
	}
	res |= flags;
	while (!ms_queue_empty(&_q)) {
		ms_queue_put(out, ms_queue_get(&_q));
	}
	_status = Status();
	return res;
}

void NalUnpacker::storeNal(mblk_t *nal) {
	ms_queue_put(&_q, nal);
}

} // namespace mediastreamer
