/*
mediastreamer2 library - modular sound and video processing and streaming
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

#include <exception>

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/rfc3984.h"

#include "h264utils.h"
#include "rfc3984.hpp"

using namespace std;

namespace mediastreamer2 {


// ======
// Packer
// ======

Packer::Packer(SpliterInterface *spliter, AggregatorInterface *aggregator, MSFactory *factory): _spliter(spliter), _aggregator(aggregator) {
	setMaxPayloadSize(ms_factory_get_payload_max_size(factory));
}

void Packer::setMaxPayloadSize(size_t size) {
	_maxSize = size;
	_spliter->setMaxSize(size);
	_aggregator->setMaxSize(size);
}

void Packer::pack(MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
	switch (_mode) {
		case SingleNalUnitMode:
			packInSingleNalUnitMode(naluq, rtpq, ts);
			break;
		case NonInterleavedMode:
			packInNonInterleavedMode(naluq, rtpq, ts);
			break;
	}
}

// Private methods
void Packer::packInSingleNalUnitMode(MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
	while (mblk_t *m = ms_queue_get(naluq)) {
		bool end = ms_queue_empty(naluq);
		size_t size = msgdsize(m);
		if (size > _maxSize) {
			ms_warning("This H264 packet does not fit into MTU: size=%u", static_cast<unsigned int>(size));
		}
		sendPacket(rtpq, ts, m, end);
	}
}

void Packer::packInNonInterleavedMode(MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
	while (mblk_t *m = ms_queue_get(naluq)) {
		bool end = ms_queue_empty(naluq);
		size_t sz = msgdsize(m);
		if (_aggregationEnabled) {
			if (_aggregator->isAggregating()) {
				mblk_t *stapPacket = _aggregator->feedNalu(m);
				if (stapPacket) {
					sendPacket(rtpq, ts, stapPacket, false);
				} else continue;
			}
			if (sz < (_maxSize / 2)) {
				_aggregator->feedNalu(m);
			} else {
				/*send as single NAL or FU-A*/
				if (sz > _maxSize) {
					ms_debug("Sending FU-A packets");
					fragNaluAndSend(rtpq, ts, m, end);
				} else {
					ms_debug("Sending Single NAL");
					sendPacket(rtpq, ts, m, end);
				}
			}
		} else {
			if (sz > _maxSize) {
				ms_debug("Sending FU-A packets");
				fragNaluAndSend(rtpq, ts, m, end);
			} else {
				ms_debug("Sending Single NAL");
				sendPacket(rtpq, ts, m, end);
			}
		}
	}
	if (_aggregator->isAggregating()) {
		ms_debug("Sending Single NAL (2)");
		sendPacket(rtpq, ts, _aggregator->completeAggregation(), true);
	}
}

void Packer::fragNaluAndSend(MSQueue *rtpq, uint32_t ts, mblk_t *nalu, bool_t marker) {
	_spliter->feedNalu(nalu);
	MSQueue *nalus = _spliter->getNalus();
	while (mblk_t *m = ms_queue_get(nalus)) {
		sendPacket(rtpq, ts, m, ms_queue_empty(nalus) ? marker : false);
	}
}

void Packer::sendPacket(MSQueue *rtpq, uint32_t ts, mblk_t *m, bool_t marker) {
	mblk_set_timestamp_info(m, ts);
	mblk_set_marker_info(m, marker);
	mblk_set_cseq(m, _refCSeq++);
	ms_queue_put(rtpq, m);
}

// ========================
// H264NaluToStapAggregator
// ========================

void H264NaluToStapAggregator::setMaxSize(size_t maxSize) {
	if (isAggregating()) {
		throw logic_error("changing payload size while aggregating NALus into a STAP-A");
	}
	_maxsize = maxSize;
}

mblk_t *H264NaluToStapAggregator::feedNalu(mblk_t *nalu) {
	size_t size = msgdsize(nalu);
	if (_stap == nullptr) {
		_stap = nalu;
		_size = size + 3; /* STAP-A header + size */
	} else {
		if ((_size + size) < (_maxsize - 2)) {
			_stap = concatNalus(_stap, nalu);
			_size += (size + 2); /* +2 for the STAP-A size field */
		} else {
			return completeAggregation();
		}
	}
	return nullptr;
}

void H264NaluToStapAggregator::reset() {
	if (_stap) freemsg(_stap);
	_size = 0;
}

mblk_t *H264NaluToStapAggregator::completeAggregation() {
	mblk_t *res = _stap;
	_stap = nullptr;
	reset();
	return res;
}


mblk_t *H264NaluToStapAggregator::concatNalus(mblk_t *m1, mblk_t *m2) {
       mblk_t *l = allocb(2, 0);
       /*eventually append a STAP-A header to m1, if not already done*/
       if (ms_h264_nalu_get_type(m1) != MSH264NaluTypeSTAPA) {
               m1 = prependStapA(m1);
       }
       putNalSize(l, msgdsize(m2));
       l->b_cont = m2;
       concatb(m1, l);
       return m1;
}

mblk_t *H264NaluToStapAggregator::prependStapA(mblk_t *m) {
       mblk_t *hm = allocb(3, 0);
       H264Tools::nalHeaderInit(hm->b_wptr, ms_h264_nalu_get_nri(m), MSH264NaluTypeSTAPA);
       hm->b_wptr += 1;
       putNalSize(hm, msgdsize(m));
       hm->b_cont = m;
       return hm;
}

void H264NaluToStapAggregator::putNalSize(mblk_t *m, size_t sz) {
       uint16_t size = htons((uint16_t)sz);
       *(uint16_t *)m->b_wptr = size;
       m->b_wptr += 2;
}

// =========================
// H264NalToFuaSpliter class
// =========================

void H264NaluToFuaSpliter::feedNalu(mblk_t *nalu) {
	mblk_t *m;
	int payload_max_size = _maxsize - 2; /*minus FU-A header*/
	uint8_t fu_indicator;
	uint8_t type = ms_h264_nalu_get_type(nalu);
	uint8_t nri = ms_h264_nalu_get_nri(nalu);
	bool start = true;

	H264Tools::nalHeaderInit(&fu_indicator, nri, MSH264NaluTypeFUA);
	while (nalu->b_wptr - nalu->b_rptr > payload_max_size) {
		m = dupb(nalu);
		nalu->b_rptr += payload_max_size;
		m->b_wptr = nalu->b_rptr;
		m = H264Tools::prependFuIndicatorAndHeader(m, fu_indicator, start, FALSE, type);
		ms_queue_put(&_q, m);
		start = false;
	}
	/*send last packet */
	m = H264Tools::prependFuIndicatorAndHeader(nalu, fu_indicator, FALSE, TRUE, type);
	ms_queue_put(&_q, m);
}

// ========
// Unpacker
// ========

Unpacker::Unpacker(AggregatorInterface *aggregator, SpliterInterface *spliter): _naluAggregator(aggregator), _naluSpliter(spliter) {
	ms_queue_init(&_q);
}

Unpacker::Status Unpacker::unpack(mblk_t *im, MSQueue *out) {
	PacketType type = getNaluType(im);
	int marker = mblk_get_marker_info(im);
	uint32_t ts = mblk_get_timestamp_info(im);
	uint16_t cseq = mblk_get_cseq(im);
	Status ret;

	if (_lastTs != ts) {
		/*a new frame is arriving, in case the marker bit was not set in previous frame, output it now,
		 *	 unless it is a FU-A packet (workaround for buggy implementations)*/
		_lastTs = ts;
		if (!_naluAggregator->isAggregating() && !ms_queue_empty(&_q)) {
			Status status;
			status.set(StatusFlag::FrameAvailable).set(StatusFlag::FrameCorrupted);
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
			_status.set(StatusFlag::FrameCorrupted);
		}
	}

	switch (type) {
		case PacketType::SingleNalUnit:
			_naluAggregator->reset();
			/*single nal unit*/
			ms_debug("Receiving single NAL");
			storeNal(im);
			break;
		case PacketType::FragmentationUnit: {
			ms_debug("Receiving FU-A");
			mblk_t *o = _naluAggregator->feedNalu(im);
			if (o) storeNal(o);
			break;
		}
		case PacketType::AggregationPacket:
			ms_debug("Receiving STAP-A");
			_naluSpliter->feedNalu(im);
			while ((im = ms_queue_get(_naluSpliter->getNalus()))) {
				storeNal(im);
			}
			break;
	}

	if (marker) {
		_lastTs = ts;
		ms_debug("Marker bit set");
		Status status;
		status.set(StatusFlag::FrameAvailable);
		ret = outputFrame(out, status);
	}

	return ret;
}

Unpacker::Status Unpacker::outputFrame(MSQueue *out, const Status &flags) {
	Status res = _status;
	if (!ms_queue_empty(out)) {
		ms_warning("rfc3984_unpack: output_frame invoked several times in a row, this should not happen");
	}
	res |= flags;
	while (!ms_queue_empty(&_q)) {
		ms_queue_put(out, ms_queue_get(&_q));
	}
	_status = 0;
	return res;
}

void Unpacker::storeNal(mblk_t *nal) {
	ms_queue_put(&_q, nal);
}

// =======================
// H264Tools class
// =======================
mblk_t *H264Tools::prependFuIndicatorAndHeader(mblk_t *m, uint8_t indicator, bool_t start, bool_t end, uint8_t type) {
	mblk_t *h = allocb(2, 0);
	h->b_wptr[0] = indicator;
	h->b_wptr[1] = ((start & 0x1) << 7) | ((end & 0x1) << 6) | type;
	h->b_wptr += 2;
	h->b_cont = m;
	if (start) m->b_rptr++;/*skip original nalu header */
		return h;
}

// =======================
// H264FUAAggregator class
// =======================
mblk_t *H264FUAAggregator::feedNalu(mblk_t *im) {
	mblk_t *om = nullptr;
	uint8_t fu_header;
	uint8_t nri, type;
	bool_t start, end;
	bool_t marker = mblk_get_marker_info(im);

	fu_header = im->b_rptr[1];
	type = ms_h264_nalu_get_type(im);
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

void H264FUAAggregator::reset() {
	if (_m) {
		freemsg(_m);
		_m = nullptr;
	}
}

mblk_t *H264FUAAggregator::completeAggregation() {
	mblk_t *res = _m;
	_m = nullptr;
	return res;
}

// =====================
// H264StapASlicer class
// =====================

void H264StapASpliter::feedNalu(mblk_t *im) {
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

Rfc3984Unpacker::~Rfc3984Unpacker() {
	if (_sps != nullptr) freemsg(_sps);
	if (_pps != nullptr) freemsg(_pps);
}

void Rfc3984Unpacker::setOutOfBandSpsPps(mblk_t *sps, mblk_t *pps) {
	if (_sps) freemsg(_sps);
	if (_pps) freemsg(_pps);
	_sps = sps;
	_pps = pps;
}


// Private methods
// ---------------
Unpacker::PacketType Rfc3984Unpacker::getNaluType(const mblk_t *nalu) const {
	switch (ms_h264_nalu_get_type(nalu)) {
		case MSH264NaluTypeFUA: return PacketType::FragmentationUnit;
		case MSH264NaluTypeSTAPA: return PacketType::AggregationPacket;
		default: return PacketType::SingleNalUnit;
	}
}

Rfc3984Unpacker::Status Rfc3984Unpacker::outputFrame(MSQueue *out, const Status &flags) {
	Status res = _status;
	if (res.test(Unpacker::StatusFlag::IsKeyFrame) && _sps && _pps) {
		/*prepend out of band provided sps and pps*/
		ms_queue_put(out, _sps);
		ms_queue_put(out, _pps);
		_sps = NULL;
		_pps = NULL;
	}
	Unpacker::outputFrame(out, flags);
	return res;
}

}; // end of mediastreamer2 namespace


//==================================================
// C wrapper implementation
//==================================================


struct _Rfc3984Context {
	mediastreamer2::Rfc3984Packer packer;
	mediastreamer2::Rfc3984Unpacker unpacker;
	mediastreamer2::H264FrameAnalyser analyser;

	_Rfc3984Context() = default;
	_Rfc3984Context(MSFactory *factory): packer(factory), unpacker() {}
};

extern "C" {

	Rfc3984Context *rfc3984_new(void) {
		return new _Rfc3984Context();
	}

	Rfc3984Context *rfc3984_new_with_factory(MSFactory *factory) {
		return new _Rfc3984Context(factory);
	}

	void rfc3984_destroy(Rfc3984Context *ctx) {
		delete ctx;
	}

	void rfc3984_set_mode(Rfc3984Context *ctx, int mode) {
		if (mode < 0 || mode > 1) {
			ms_error("invalid RFC3984 packetization mode [%d]", mode);
			return;
		}
		ctx->packer.setMode(mode == 0 ? mediastreamer2::Packer::SingleNalUnitMode : mediastreamer2::Packer::SingleNalUnitMode);
	}

	void rfc3984_enable_stap_a(Rfc3984Context *ctx, bool_t yesno) {
		ctx->packer.enableAggregation(yesno);
	}

	void rfc3984_pack(Rfc3984Context *ctx, MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
		ctx->packer.pack(naluq, rtpq, ts);
	}

	void rfc3984_unpack_out_of_band_sps_pps(Rfc3984Context *ctx, mblk_t *sps, mblk_t *pps) {
		ctx->unpacker.setOutOfBandSpsPps(sps, pps);
	}

	unsigned int rfc3984_unpack2(Rfc3984Context *ctx, mblk_t *im, MSQueue *naluq) {
		MSQueue q;
		ms_queue_init(&q);
		unsigned int status = ctx->unpacker.unpack(im, &q).to_ulong();
		if (status & Rfc3984FrameAvailable) {
			status |= ctx->analyser.analyse(&q).toUInt();
			while(mblk_t *m = ms_queue_get(&q)) {
				ms_queue_put(naluq, m);
			}
		}
		return status;
	}
}
