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

#pragma once

#include "nal-unpacker.h"

namespace mediastreamer {

class H265NalUnpacker: public NalUnpacker {
public:
	H265NalUnpacker(): NalUnpacker(new FuAggregator(), new ApSpliter()) {}

private:
	class FuAggregator: public NalUnpacker::FuAggregatorInterface {
	public:
		mblk_t *feed(mblk_t *packet) override;
		bool isAggregating() const override {return _m != nullptr;}
		void reset() override;
		mblk_t *completeAggregation() override;
	};

	class ApSpliter: public NalUnpacker::ApSpliterInterface {
	public:
		void feed(mblk_t *packet) override;
		MSQueue *getNalus() override {return &_q;}
	};

	PacketType getNaluType(const mblk_t *nalu) const override;
};

} // namespace mediastreamer
