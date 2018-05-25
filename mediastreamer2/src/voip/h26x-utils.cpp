/*
 Mediastreamer2 h26x-utils.h
 Copyright (C) 2018 Belledonne Communications SARL

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

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "h26x-utils.h"

using namespace std;

namespace mediastreamer {

void naluStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out) {
	naluStreamToNalus(byteStream.data(), byteStream.size(), out);
}

void naluStreamToNalus(const uint8_t *bytestream, size_t size, MSQueue *out) {
	const uint8_t *ptr = bytestream;
	while (ptr < bytestream + size) {
		uint32_t nalu_size;
		memcpy(&nalu_size, ptr, 4);
		nalu_size = ntohl(nalu_size);

		mblk_t *nalu = allocb(nalu_size, 0);
		memcpy(nalu->b_wptr, ptr + 4, nalu_size);
		ptr += nalu_size + 4;
		nalu->b_wptr += nalu_size;

		ms_queue_put(out, nalu);
	}
}

void byteStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out) {
	byteStreamToNalus(byteStream.data(), byteStream.size(), out);
}

void byteStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out) {
	vector<uint8_t> buffer;
	const uint8_t *end = byteStream + size;
	for (const uint8_t *it = byteStream; it != end;) {
		buffer.resize(0);

		int leadingZero = 0;
		while (it != end && *it == 0) {
			leadingZero++;
			it++;
		}
		if (it == end) break;
		if (leadingZero < 2 || *it++ != 1) throw invalid_argument("no starting sequence found in H26x byte stream");

		while (it != end) {
			if (it + 2 < end && it[0] == 0 && it[1] == 0) {
				if (it[2] == 0 || it[2] == 1) break;
				else if (it[2] == 3) {
					buffer.push_back(0);
					buffer.push_back(0);
					it += 3;
					continue;
				}
			}
			buffer.push_back(*it++);
		}

		mblk_t *nalu = allocb(buffer.size(), 0);
		memcpy(nalu->b_wptr, buffer.data(), buffer.size());
		nalu->b_wptr += buffer.size();
		ms_queue_put(out, nalu);
	}
}

void nalusToByteStream(MSQueue *nalus, std::vector<uint8_t> &byteStream) {
	bool startPicture = true;
	byteStream.resize(0);
	while (mblk_t *im = ms_queue_get(nalus)) {
		if (startPicture) {
			// starting picture extra zero byte
			byteStream.push_back(0);
			startPicture = false;
		}

		// starting NALu marker
		byteStream.push_back(0);
		byteStream.push_back(0);
		byteStream.push_back(1);

		// copy NALu content
		for (const uint8_t *src = im->b_rptr; src < im->b_wptr;) {
			if (src+2 < im->b_wptr && src[0] == 0 && src[1] == 0 && (src[2] == 0 || src[2] == 1)) {
				byteStream.push_back(0);
				byteStream.push_back(0);
				byteStream.push_back(3); // emulation prevention three byte
				byteStream.push_back(src[2]);
				src += 3;
			} else {
				byteStream.push_back(*src++);
			}
		}

		freemsg(im);
	}
}

}
