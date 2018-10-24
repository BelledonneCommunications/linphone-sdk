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

#include "nal-unpacker.h"

namespace mediastreamer {

class H264FuaAggregator: public NalUnpacker::FuAggregatorInterface {
public:
	mblk_t *feed(mblk_t *im) override;
	bool isAggregating() const override {return _m != nullptr;}
	void reset() override;
	mblk_t *completeAggregation() override;
};

class H264StapaSpliter: public NalUnpacker::ApSpliterInterface {
public:
	void feed(mblk_t *im) override;
	MSQueue *getNalus() override {return &_q;}
};

class H264NalUnpacker: public NalUnpacker {
public:
	H264NalUnpacker(): NalUnpacker(new H264FuaAggregator(), new H264StapaSpliter()) {}
	~H264NalUnpacker();

	void setOutOfBandSpsPps(mblk_t *sps, mblk_t *pps);

private:
	NalUnpacker::PacketType getNaluType(const mblk_t *nalu) const override;
	Status outputFrame(MSQueue *out, const Status &flags) override;

	mblk_t *_sps = nullptr;
	mblk_t *_pps = nullptr;
};

} // namespace mediastreamer
