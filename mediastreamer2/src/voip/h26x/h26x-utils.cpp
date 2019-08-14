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

#include <stdexcept>

#include "h264-utils.h"
#include "h265-utils.h"

#include "h26x-utils.h"

using namespace std;

namespace mediastreamer {

void H26xUtils::naluStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out) {
	H26xUtils::naluStreamToNalus(byteStream.data(), byteStream.size(), out);
}

void H26xUtils::naluStreamToNalus(const uint8_t *bytestream, size_t size, MSQueue *out) {
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

void H26xUtils::byteStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out) {
	H26xUtils::byteStreamToNalus(byteStream.data(), byteStream.size(), out);
}

void H26xUtils::byteStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out) {
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

void H26xUtils::nalusToByteStream(MSQueue *nalus, std::vector<uint8_t> &byteStream) {
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
				src += 2;
			} else {
				byteStream.push_back(*src++);
			}
		}

		freemsg(im);
	}
}

void H26xParameterSetsInserter::replaceParameterSet(mblk_t *&ps, mblk_t *newPs) {
	if (ps) freemsg(ps);
	ps = newPs;
}

H26xParameterSetsStore::H26xParameterSetsStore(const std::string &mime, const std::initializer_list<int> &psCodes) {
	_naluHeader.reset(H26xToolFactory::get(mime).createNaluHeader());
	for (int psCode : psCodes) {
		_ps[psCode] = nullptr;
	}
}

H26xParameterSetsStore::~H26xParameterSetsStore() {
	for(auto it = _ps.begin(); it != _ps.end(); it++) {
		if (it->second) freemsg(it->second);
	}
}

bool H26xParameterSetsStore::psGatheringCompleted() const {
	for(const auto &item : _ps) {
		if (item.second == nullptr) return false;
	}
	return true;
}

void H26xParameterSetsStore::extractAllPs(MSQueue *frame) {
	for (mblk_t *nalu = ms_queue_peek_first(frame); !ms_queue_end(frame, nalu);) {
		_naluHeader->parse(nalu->b_rptr);
		int type = _naluHeader->getAbsType();
		if (_ps.find(type) != _ps.end()) {
			mblk_t *ps = nalu;
			nalu = ms_queue_next(frame, nalu);
			ms_queue_remove(frame, ps);
			addPs(type, ps);
			continue;
		}
		nalu = ms_queue_next(frame, nalu);
	}
}

void H26xParameterSetsStore::fetchAllPs(MSQueue *outq) const {
	MSQueue q;
	ms_queue_init(&q);
	for(const auto &item : _ps) {
		if (item.second) {
			ms_queue_put(outq, dupmsg(item.second));
		}
	}
}

void H26xParameterSetsStore::addPs(int naluType, mblk_t *nalu) {
	bool replaceParam = false;
	mblk_t *lastPs = _ps[naluType];

	if (lastPs == nullptr || nalu == nullptr) {
		replaceParam = true;
	} else {
		ssize_t naluSize = nalu->b_wptr - nalu->b_rptr;
		ssize_t lastPsSize = lastPs->b_wptr - lastPs->b_rptr;
		if (naluSize != lastPsSize || memcmp(nalu->b_rptr, lastPs->b_rptr, naluSize) != 0) {
			replaceParam = true;
		}
	}

	if (replaceParam) {
		if (lastPs) freemsg(lastPs);
		_ps[naluType] = nalu ? dupmsg(nalu) : nullptr;
		_newParameters = true;
	}
}

const H26xToolFactory &H26xToolFactory::get(const std::string &mime) {
	unique_ptr<H26xToolFactory> &instance = _instances[mime];
	if (instance == nullptr) {
		if (mime == "video/avc") instance.reset(new H264ToolFactory());
		else if (mime == "video/hevc") instance.reset(new H265ToolFactory());
		else throw invalid_argument("no H26xToolFactory class associated to '" + mime + "' mime");
	}
	return *instance;
}

std::unordered_map<std::string, std::unique_ptr<H26xToolFactory>> H26xToolFactory::_instances;

} // namespace mediastreamer
