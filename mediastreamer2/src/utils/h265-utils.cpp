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

#include <stdexcept>
#include "mediastreamer2/mscommon.h"
#include "h265-utils.h"

using namespace std;

namespace mediastreamer {

H265NaluType::H265NaluType(uint8_t value) {
	if (0xc0 & value) throw out_of_range("H265 NALu type higher than 63");
	_value = value;
}

bool H265NaluType::isParameterSet() const {
	return *this == Vps || *this == Sps || *this == Pps;
}

const H265NaluType H265NaluType::IdrWRadl = 19;
const H265NaluType H265NaluType::IdrNLp = 20;
const H265NaluType H265NaluType::Vps = 32;
const H265NaluType H265NaluType::Sps = 33;
const H265NaluType H265NaluType::Pps = 34;
const H265NaluType H265NaluType::Ap = 48;
const H265NaluType H265NaluType::Fu = 49;

void H265NaluHeader::setLayerId(uint8_t layerId) {
	if (layerId & 0xc0) throw out_of_range("H265 layer ID wider than 6 bits");
	_layerId = layerId;
}

void H265NaluHeader::setTid(uint8_t tid) {
	if (tid & 0xf8) throw out_of_range("H265 layer ID wider than 3 bits");
	_tid = tid;
}

bool H265NaluHeader::operator==(const H265NaluHeader &h2) const {
	return _fBit == h2._fBit && _type == h2._type && _tid == h2._tid && _layerId == h2._layerId;
}

void H265NaluHeader::parse(const uint8_t *header) {
	uint16_t header2 = ntohs(*reinterpret_cast<const uint16_t *>(header));
	_tid = header2 & 0x07;
	header2 >>= 3;
	_layerId = header2 & 0x3f;
	header2 >>= 6;
	_type = header2 & 0x3f;
	header2 >>= 6;
	_fBit = (header2 != 0);
}

mblk_t *H265NaluHeader::forge() const {
	uint16_t header = _fBit ? 1 : 0;
	header <<= 1;
	header |= _type;
	header <<= 6;
	header |= _layerId;
	header <<= 3;
	header |= _tid;
	header = htons(header);

	mblk_t *newHeader = allocb(2, 0);
	*reinterpret_cast<uint16_t *>(newHeader->b_wptr) = header;
	newHeader->b_wptr += 2;
	return newHeader;
}

void H265FuHeader::parse(const uint8_t *header) {
	uint8_t header2 = *header;
	_type = header2 & 0x3f;
	header2 >>= 6;
	bool end = ((header2 & 0x01) != 0);
	header2 >>= 1;
	bool start = ((header2 & 0x01) != 0);

	if (start && end) throw invalid_argument("parsing an FU header with both start and end flags enabled");

	if (start) {
		_pos = Position::Start;
	} else if (end) {
		_pos = Position::End;
	} else {
		_pos = Position::Middle;
	}
}

mblk_t *H265FuHeader::forge() const {
	uint8_t header = (_pos == Position::Start ? 1 : 0);
	header <<= 1;
	header |= (_pos == Position::End ? 1 : 0);
	header <<= 6;
	header |= _type;

	mblk_t *newHeader = allocb(1, 0);
	*newHeader->b_wptr++ = header;
	return newHeader;
}

void H265ParameterSetsInserter::process(MSQueue *in, MSQueue *out) {
	bool psBeforeIdr = false;
	H265NaluHeader header;
	while (mblk_t *m = ms_queue_get(in)) {
		header.parse(m->b_rptr);
		if (header.getType() == H265NaluType::Vps) {
			psBeforeIdr = true;
			replaceParameterSet(_vps, m);
		} else if (header.getType() == H265NaluType::Sps) {
			psBeforeIdr = true;
			replaceParameterSet(_sps, m);
		} else if (header.getType() == H265NaluType::Pps) {
			psBeforeIdr = true;
			replaceParameterSet(_pps, m);
		} else {
			if (_vps && _sps && _pps) {
				if ((header.getType() == H265NaluType::IdrWRadl || header.getType() == H265NaluType::IdrNLp) && !psBeforeIdr) {
					ms_queue_put(out, dupmsg(_vps));
					ms_queue_put(out, dupmsg(_sps));
					ms_queue_put(out, dupmsg(_pps));
				}
				ms_queue_put(out, m);
			} else {
				freemsg(m);
			}
			psBeforeIdr = false;
		}
	}
}

void H265ParameterSetsInserter::flush() {
	replaceParameterSet(_vps, nullptr);
	replaceParameterSet(_sps, nullptr);
	replaceParameterSet(_pps, nullptr);
}

int H265ParameterSetsStore::getNaluType(const mblk_t *nalu) const {
	H265NaluHeader header(nalu->b_rptr);
	return header.getType();
}

} // namespace mediastreamer
