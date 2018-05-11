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

#pragma once

#include "nal-unpacker.h"

namespace mediastreamer {

class H265NalUnpacker: public NalUnpacker {
public:
	H265NalUnpacker(): NalUnpacker(new FuAggregator(), new ApSpliter()) {}

private:
	class FuAggregator: public NalUnpacker::FuAggregatorInterface {
	public:
		~FuAggregator() {if (_m != nullptr) freemsg(_m);}
		mblk_t *feed(mblk_t *packet) override;
		bool isAggregating() const override {return _m != nullptr;}
		void reset() override;
		mblk_t *completeAggregation() override;

	private:
		mblk_t *_m = nullptr;
	};

	class ApSpliter: public NalUnpacker::ApSpliterInterface {
	public:
		ApSpliter() {ms_queue_init(&_q);}
		~ApSpliter() {ms_queue_flush(&_q);}

		void feed(mblk_t *packet) override;
		MSQueue *getNalus() override {return &_q;}

	private:
		MSQueue _q;
	};
};

} // namespace mediastreamer
