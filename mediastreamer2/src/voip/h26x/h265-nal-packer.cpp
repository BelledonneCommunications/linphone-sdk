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

#include "h265-nal-packer.h"

using namespace std;

namespace mediastreamer {

H265NalPacker::NaluAggregator::~NaluAggregator() {
	if (_ap) freemsg(_ap);
}

mblk_t *H265NalPacker::NaluAggregator::feed(mblk_t *nalu) {
	H265NaluHeader header(nalu->b_rptr);
	if (_ap == nullptr) {
		placeFirstNalu(nalu);
	} else {
		if (H265NaluHeader::length + _size + 2 + msgdsize(nalu) > _maxSize) {
			mblk_t *m = completeAggregation();
			placeFirstNalu(nalu);
			return m;
		} else {
			aggregate(nalu);
		}
	}
	return nullptr;
}

void H265NalPacker::NaluAggregator::reset() {
	if (_ap) freemsg(_ap);
}

mblk_t *H265NalPacker::NaluAggregator::completeAggregation() {
	mblk_t *m;

	if (_ap == nullptr) return nullptr;

	if (_apHeader.getType() != H265NaluType::Ap) {
		m = _ap;
	} else {
		m = _apHeader.forge();
		concatb(m, _ap);
		msgpullup(m, -1);
	}
	_ap = nullptr;
	return m;
}

void H265NalPacker::NaluAggregator::placeFirstNalu(mblk_t *nalu) {
	H265NaluHeader header(nalu->b_rptr);
	_ap = nalu;
	_apHeader = header;
	_size = msgdsize(nalu);
}

void H265NalPacker::NaluAggregator::aggregate(mblk_t *nalu) {
	H265NaluHeader header(nalu->b_rptr);
	_apHeader.setFBit(_apHeader.getFBit() || header.getFBit());
	_apHeader.setType(H265NaluType::Ap);
	_apHeader.setLayerId(min(_apHeader.getLayerId(), header.getLayerId()));
	_apHeader.setTid(min(_apHeader.getTid(), header.getTid()));

	mblk_t *size = allocb(2, 0);
	uint16_t *sizePtr = reinterpret_cast<uint16_t *>(size->b_wptr);
	*sizePtr = htons(uint16_t(msgdsize(nalu)));
	size->b_wptr += 2;

	_size += (msgdsize(size) + msgdsize(nalu)); // warning: this line must remain before 'concatb()' invocations.

	concatb(_ap, size);
	concatb(_ap, nalu);
}

void H265NalPacker::NaluSpliter::feed(mblk_t *nalu) {
	if (msgdsize(nalu) <= _maxSize) return;

	H265NaluHeader naluHeader(nalu->b_rptr);
	nalu->b_rptr += H265NaluHeader::length;

	H265FuHeader fuHeader;
	fuHeader.setType(naluHeader.getType());
	naluHeader.setType(H265NaluType::Fu);

	const size_t maxFuPayloadSize = _maxSize - H265NaluHeader::length - H265FuHeader::length;
	while (msgdsize(nalu) > maxFuPayloadSize) {
		ms_queue_put(&_q, makeFu(naluHeader, fuHeader, nalu->b_rptr, maxFuPayloadSize));
		fuHeader.setPosition(H265FuHeader::Position::Middle);
		nalu->b_rptr += maxFuPayloadSize;
	}
	fuHeader.setPosition(H265FuHeader::Position::End);
	ms_queue_put(&_q, makeFu(naluHeader, fuHeader, nalu->b_rptr, msgdsize(nalu)));

	freemsg(nalu);
}

mblk_t *H265NalPacker::NaluSpliter::makeFu(const H265NaluHeader &naluHeader, const H265FuHeader &fuHeader, const uint8_t *payload, size_t length) {
	mblk_t *naluHeaderBuf = naluHeader.forge();
	mblk_t *fuHeaderBuf = fuHeader.forge();
	mblk_t *payloadBuf = allocb(length, 0);
	memcpy(payloadBuf->b_wptr, payload, length);
	payloadBuf->b_wptr += length;

	mblk_t *fu = naluHeaderBuf;
	concatb(fu, fuHeaderBuf);
	concatb(fu, payloadBuf);
	msgpullup(fu, -1);
	return fu;
}

}
