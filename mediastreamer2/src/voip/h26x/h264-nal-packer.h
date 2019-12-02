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

#include "nal-packer.h"

namespace mediastreamer {

class H264NalPacker: public NalPacker {
public:
	enum PacketizationMode {
		SingleNalUnitMode,
		NonInterleavedMode
	};

	H264NalPacker(size_t maxPayloadSize): NalPacker(new NaluAggregator(maxPayloadSize), new NaluSpliter(maxPayloadSize), maxPayloadSize) {}

private:

	class NaluAggregator: public NaluAggregatorInterface {
	public:
		using NaluAggregatorInterface::NaluAggregatorInterface;
		~NaluAggregator() override {reset();}

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

	class NaluSpliter: public NaluSpliterInterface {
	public:
		using NaluSpliterInterface::NaluSpliterInterface;
		void feed(mblk_t *nalu) override;
	};

};

} // namespace mediastreamer
