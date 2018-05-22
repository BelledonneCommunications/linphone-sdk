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

static void  push_nalu(const uint8_t *begin, const uint8_t *end, MSQueue *nalus) {
	unsigned ecount = 0;
	const uint8_t *src = begin;
	size_t nalu_len = (end - begin);
	uint8_t nalu_byte  = *src++;

	mblk_t *m = allocb(nalu_len, 0);

	// Removal of the 3 in a 003x sequence
	// This emulation prevention byte is normally part of a NAL unit.
	/* H.264 standard sys in par 7.4.1 page 58
	 * emulation_prevention_three_byte is a byte equal to 0x03.
	 * When an emulation_prevention_three_byte is present in a NAL unit, it shall be discarded by the decoding process.
	 * Within the NAL unit, the following three-byte sequence shall not occur at any byte-aligned position: 0x000000, 0x000001, 0x00002
	 */
	*m->b_wptr++ = nalu_byte;
	while (src < end - 3) {
		if (src[0] == 0 && src[1] == 0 && src[2] == 3) {
			*m->b_wptr++ = 0;
			*m->b_wptr++ = 0;
			// drop the emulation_prevention_three_byte
			src += 3;
			++ecount;
			continue;
		}
		*m->b_wptr++ = *src++;
	}
	*m->b_wptr++ = *src++;
	*m->b_wptr++ = *src++;
	*m->b_wptr++ = *src++;

	ms_queue_put(nalus, m);
}

void byteStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out) {
	size_t i;
	const uint8_t *p, *begin = NULL;
	int zeroes = 0;

	for (i = 0, p = byteStream; i < size; ++i) {
		if (*p == 0) {
			++zeroes;
		} else if (zeroes >= 2 && *p == 1) {
			if (begin) {
				push_nalu(begin, p - zeroes, out);
			}
			begin = p + 1;
		} else zeroes = 0;
		++p;
	}
	if (begin) push_nalu(begin, p, out);
}

void nalusToByteStream(MSQueue *nalus, std::vector<uint8_t> &bytestream) {
	bool start_picture = true;
	bytestream.resize(0);
	while (mblk_t *im = ms_queue_get(nalus)) {
		const uint8_t *src = im->b_rptr;
		if (src[0] == 0 && src[1] == 0 && src[2] == 0 && src[3] == 1) {
			while (src != im->b_wptr) {
				bytestream.push_back(*src++);
			}
		} else {
			if (start_picture) {
				bytestream.push_back(0);
				start_picture = false;
			}

			/*prepend nal marker*/
			bytestream.push_back(0);
			bytestream.push_back(0);
			bytestream.push_back(1);
			bytestream.push_back(*src++);

			while (src < (im->b_wptr - 3)) {
				if (src[0] == 0 && src[1] == 0 && src[2] < 3) {
					bytestream.push_back(0);
					bytestream.push_back(0);
					bytestream.push_back(3);
					src += 2;
				}

				bytestream.push_back(*src++);
			}

			bytestream.push_back(*src++);
			bytestream.push_back(*src++);
			bytestream.push_back(*src++);
		}

		freemsg(im);
	}
}

}
