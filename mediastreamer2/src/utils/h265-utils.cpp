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

void H265NaluHeader::parse(const uint8_t *header) {
	uint16_t header2 = ntohs(*reinterpret_cast<const uint16_t *>(header));
	_tid = header2 & 0x07;
	header2 >>= 3;
	_layerId = header2 & 0x3f;
	header2 >>= 6;
	_type = header2 & 0x3f;
}

mblk_t *H265NaluHeader::forge() const {
	uint16_t header = _type;
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

	if (start && end) throw runtime_error("parsing an FU header with both start and end flags enabled");

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
	*newHeader->b_wptr = header;
	newHeader++;
	return newHeader;
}

}
