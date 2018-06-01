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

#pragma once

#include <cstdint>
#include <vector>

#include "mediastreamer2/msqueue.h"

namespace mediastreamer {


class H26xUtils {
public:
	H26xUtils() = delete;

	static void naluStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out);
	static void naluStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out);

	static void byteStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out);
	static void byteStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out);

	static void nalusToByteStream(MSQueue *nalus, std::vector<uint8_t> &bytestream);
};


class H26xParameterSetsInserter {
public:
	virtual ~H26xParameterSetsInserter() = default;
	virtual void process(MSQueue *in, MSQueue *out) = 0;
	virtual void flush() = 0;

protected:
	static void replaceParameterSet(mblk_t *&ps, mblk_t *newPs);
};

}
