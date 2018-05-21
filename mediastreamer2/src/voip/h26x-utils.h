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

void naluStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out);
void naluStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out);

void byteStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out);
void byteStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out);

void nalusToByteStream(MSQueue *nalus, std::vector<uint8_t> &bytestream);

}
