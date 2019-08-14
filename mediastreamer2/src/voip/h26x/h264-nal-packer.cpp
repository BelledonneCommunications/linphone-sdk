/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "h264-utils.h"

#include "h264-nal-packer.h"

using namespace std;

namespace mediastreamer {

// ========================
// H264NaluToStapAggregator
// ========================

mblk_t *H264NaluAggregator::feed(mblk_t *nalu) {
	size_t size = msgdsize(nalu);
	if (_stap == nullptr) {
		_stap = nalu;
		_size = size + 3; /* STAP-A header + size */
	} else {
		if ((_size + size) < (_maxSize - 2)) {
			_stap = concatNalus(_stap, nalu);
			_size += (size + 2); /* +2 for the STAP-A size field */
		} else {
			return completeAggregation();
		}
	}
	return nullptr;
}

void H264NaluAggregator::reset() {
	if (_stap) freemsg(_stap);
	_size = 0;
}

mblk_t *H264NaluAggregator::completeAggregation() {
	mblk_t *res = _stap;
	_stap = nullptr;
	reset();
	return res;
}


mblk_t *H264NaluAggregator::concatNalus(mblk_t *m1, mblk_t *m2) {
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

mblk_t *H264NaluAggregator::prependStapA(mblk_t *m) {
	mblk_t *hm = allocb(3, 0);
	H264Tools::nalHeaderInit(hm->b_wptr, ms_h264_nalu_get_nri(m), MSH264NaluTypeSTAPA);
	hm->b_wptr += 1;
	putNalSize(hm, msgdsize(m));
	hm->b_cont = m;
	return hm;
}

void H264NaluAggregator::putNalSize(mblk_t *m, size_t sz) {
	uint16_t size = htons((uint16_t)sz);
	*(uint16_t *)m->b_wptr = size;
	m->b_wptr += 2;
}

// =========================
// H264NalToFuaSpliter class
// =========================

void H264NaluSpliter::feed(mblk_t *nalu) {
	mblk_t *m;
	int payload_max_size = _maxSize - 2; /*minus FU-A header*/
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

} // namespace mediastreamer
