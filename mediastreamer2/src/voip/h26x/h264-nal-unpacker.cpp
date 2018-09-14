/*
 Mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#include "h264-utils.h"

#include "h264-nal-unpacker.h"

using namespace std;

namespace mediastreamer {

// =======================
// H264FUAAggregator class
// =======================
mblk_t *H264FuaAggregator::feed(mblk_t *im) {
	mblk_t *om = nullptr;
	uint8_t fu_header;
	uint8_t nri, type;
	bool_t start, end;
	bool_t marker = mblk_get_marker_info(im);

	fu_header = im->b_rptr[1];
	type = fu_header & 0x17;
	start = fu_header >> 7;
	end = (fu_header >> 6) & 0x1;
	if (start) {
		mblk_t *new_header;
		nri = ms_h264_nalu_get_nri(im);
		if (_m != nullptr) {
			ms_error("receiving FU-A start while previous FU-A is not "
			"finished");
			freemsg(_m);
			_m = nullptr;
		}
		im->b_rptr += 2; /*skip the nal header and the fu header*/
		new_header = allocb(1, 0); /* allocate small fragment to put the correct nal header, this is to avoid to write on the buffer
		which can break processing of other users of the buffers */
		H264Tools::nalHeaderInit(new_header->b_wptr, nri, type);
		new_header->b_wptr++;
		mblk_meta_copy(im, new_header);
		concatb(new_header, im);
		_m = new_header;
	} else {
		if (_m != nullptr) {
			im->b_rptr += 2;
			concatb(_m, im);
		} else {
			ms_error("Receiving continuation FU packet but no start.");
			freemsg(im);
		}
	}
	if (end && _m) {
		msgpullup(_m, -1);
		om = _m;
		mblk_set_marker_info(om, marker); /*set the marker bit of this aggregated NAL as the last fragment received.*/
		_m = nullptr;
	}
	return om;
}

void H264FuaAggregator::reset() {
	if (_m) {
		freemsg(_m);
		_m = nullptr;
	}
}

mblk_t *H264FuaAggregator::completeAggregation() {
	mblk_t *res = _m;
	_m = nullptr;
	return res;
}

// =====================
// H264StapASlicer class
// =====================

void H264StapaSpliter::feed(mblk_t *im) {
	uint16_t sz;
	for (uint8_t *p = im->b_rptr + 1; p < im->b_wptr;) {
		memcpy(&sz, p, 2);
		sz = ntohs(sz);
		mblk_t *nal = dupb(im);
		p += 2;
		nal->b_rptr = p;
		p += sz;
		nal->b_wptr = p;
		if (p > im->b_wptr) {
			ms_error("Malformed STAP-A packet");
			freemsg(nal);
			break;
		}
		ms_queue_put(&_q, nal);
	}
	freemsg(im);
}

//==================================================
// Rfc3984Unpacker
//==================================================

// Public methods
// --------------

H264NalUnpacker::~H264NalUnpacker() {
	if (_sps != nullptr) freemsg(_sps);
	if (_pps != nullptr) freemsg(_pps);
}

void H264NalUnpacker::setOutOfBandSpsPps(mblk_t *sps, mblk_t *pps) {
	if (_sps) freemsg(_sps);
	if (_pps) freemsg(_pps);
	_sps = sps;
	_pps = pps;
}


// Private methods
// ---------------
NalUnpacker::PacketType H264NalUnpacker::getNaluType(const mblk_t *nalu) const {
	switch (ms_h264_nalu_get_type(nalu)) {
		case MSH264NaluTypeFUA: return PacketType::FragmentationUnit;
		case MSH264NaluTypeSTAPA: return PacketType::AggregationPacket;
		default: return PacketType::SingleNalUnit;
	}
}

H264NalUnpacker::Status H264NalUnpacker::outputFrame(MSQueue *out, const Status &flags) {
	if (_status.isKeyFrame && _sps && _pps) {
		/*prepend out of band provided sps and pps*/
		ms_queue_put(out, _sps);
		ms_queue_put(out, _pps);
		_sps = NULL;
		_pps = NULL;
	}
	return NalUnpacker::outputFrame(out, flags);
}

} // namespace mediastreamer
