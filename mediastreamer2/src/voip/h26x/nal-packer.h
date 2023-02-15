/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>

#include <ortp/str_utils.h>

#include "mediastreamer2/msqueue.h"

namespace mediastreamer {

class NalPacker {
public:
	enum PacketizationMode { SingleNalUnitMode, NonInterleavedMode };

	class NaluAggregatorInterface {
	public:
		NaluAggregatorInterface(size_t maxSize) : _maxSize(maxSize) {
		}
		virtual ~NaluAggregatorInterface() = default;

		size_t getMaxSize() const {
			return _maxSize;
		}
		void setMaxSize(size_t maxSize);

		virtual mblk_t *feed(mblk_t *nalu) = 0;
		virtual bool isAggregating() const = 0;
		virtual void reset() = 0;
		virtual mblk_t *completeAggregation() = 0;

	protected:
		size_t _maxSize;
	};

	class NaluSpliterInterface {
	public:
		NaluSpliterInterface(size_t maxSize) : _maxSize(maxSize) {
			ms_queue_init(&_q);
		}
		virtual ~NaluSpliterInterface() {
			ms_queue_flush(&_q);
		}

		size_t getMaxSize() const {
			return _maxSize;
		}
		void setMaxSize(size_t maxSize) {
			_maxSize = maxSize;
		}

		virtual void feed(mblk_t *nalu) = 0;
		MSQueue *getPackets() {
			return &_q;
		}

	protected:
		size_t _maxSize;
		MSQueue _q;
	};

	void setPacketizationMode(PacketizationMode packMode) {
		_packMode = packMode;
	}
	PacketizationMode getPacketizationMode() const {
		return _packMode;
	}

	// some stupid phones don't decode STAP-A packets ...
	void enableAggregation(bool yesno) {
		_aggregationEnabled = yesno;
	}
	bool aggregationEnabled() const {
		return _aggregationEnabled;
	}

	void setMaxPayloadSize(size_t size);
	size_t getMaxPayloadSize() {
		return _maxSize;
	}

	// process NALus and pack them into RTP payloads
	MS2_PUBLIC void pack(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);
	void flush();

protected:
	NalPacker(NaluAggregatorInterface *naluAggregator, NaluSpliterInterface *naluSpliter, size_t maxPayloadSize);

	void packInSingleNalUnitMode(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);
	void packInNonInterleavedMode(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);
	void fragNaluAndSend(MSQueue *rtpq, uint32_t ts, mblk_t *nalu, bool_t marker);
	void sendPacket(MSQueue *rtpq, uint32_t ts, mblk_t *m, bool_t marker);

	size_t _maxSize;
	uint16_t _refCSeq = 0;
	PacketizationMode _packMode = SingleNalUnitMode;
	bool _aggregationEnabled = false;
	std::unique_ptr<NaluSpliterInterface> _naluSpliter;
	std::unique_ptr<NaluAggregatorInterface> _naluAggregator;
};

}; // namespace mediastreamer
