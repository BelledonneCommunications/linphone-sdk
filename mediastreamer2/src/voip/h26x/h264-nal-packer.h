/*
 Mediastreamer2 library - modular sound and video processing and streaming
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

#pragma once

#include "nal-packer.h"

namespace mediastreamer {

class H264NaluAggregator: public NalPacker::NaluAggregatorInterface {
public:
	H264NaluAggregator() {}
	~H264NaluAggregator() {reset();}

	mblk_t *feed(mblk_t *nalu) override;
	bool isAggregating() const override {return _stap != nullptr;}
	void reset() override;
	mblk_t *completeAggregation() override;

private:
	static mblk_t *concatNalus(mblk_t *m1, mblk_t *m2);
	static mblk_t *prependStapA(mblk_t *m);
	static void putNalSize(mblk_t *m, size_t sz);

	mblk_t *_stap = nullptr;
	size_t _size = 0;
};

class H264NaluSpliter: public NalPacker::NaluSpliterInterface {
public:
	void feed(mblk_t *nalu) override;
};

class H264NalPacker: public NalPacker {
public:
	enum PacketizationMode {
		SingleNalUnitMode,
		NonInterleavedMode
	};

	H264NalPacker(): NalPacker(new H264NaluAggregator(), new H264NaluSpliter()) {}
	H264NalPacker(MSFactory *factory): NalPacker(new H264NaluAggregator(), new H264NaluSpliter(), factory) {}
};

} // namespace mediastreamer
