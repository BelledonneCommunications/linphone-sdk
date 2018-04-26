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

#include <memory>
#include <ortp/str_utils.h>
#include <mediastreamer2/msfactory.h>

namespace mediastreamer {

class NalPacker {
public:
	enum PacketizationMode {
		SingleNalUnitMode,
		NonInterleavedMode
	};

	class NaluAggregatorInterface {
	public:
		virtual ~NaluAggregatorInterface() = default;

		virtual size_t getMaxSize() const = 0;
		virtual void setMaxSize(size_t maxSize) = 0;

		virtual mblk_t *feed(mblk_t *nalu) = 0;
		virtual bool isAggregating() const = 0;
		virtual void reset() = 0;
		virtual mblk_t *completeAggregation() = 0;
	};

	class NaluSpliterInterface {
	public:
		virtual ~NaluSpliterInterface() = default;

		virtual size_t getMaxSize() const = 0;
		virtual void setMaxSize(size_t maxSize) = 0;

		virtual void feed(mblk_t *nalu) = 0;
		virtual MSQueue *getPackets() = 0;
	};

	NalPacker(NaluSpliterInterface *naluSpliter, NaluAggregatorInterface *naluAggregator): _naluSpliter(naluSpliter), _naluAggregator(naluAggregator) {}
	NalPacker(NaluSpliterInterface *naluSpliter, NaluAggregatorInterface *naluAggregator, MSFactory *factory);

	void setPacketizationMode(PacketizationMode packMode) {_packMode = packMode;}
	PacketizationMode getPacketizationMode() const {return _packMode;}

	// some stupid phones don't decode STAP-A packets ...
	void enableAggregation(bool yesno) {_aggregationEnabled = yesno;}
	bool aggregationEnabled() const {return _aggregationEnabled;}

	void setMaxPayloadSize(size_t size);
	size_t getMaxPayloadSize() {return _maxSize;}

	// process NALus and pack them into RTP payloads
	void pack(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);

protected:
	void packInSingleNalUnitMode(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);
	void packInNonInterleavedMode(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);
	void fragNaluAndSend(MSQueue *rtpq, uint32_t ts, mblk_t *nalu, bool_t marker);
	void sendPacket(MSQueue *rtpq, uint32_t ts, mblk_t *m, bool_t marker);

	size_t _maxSize = MS_DEFAULT_MAX_PAYLOAD_SIZE;
	uint16_t _refCSeq = 0;
	PacketizationMode _packMode = SingleNalUnitMode;
	bool _aggregationEnabled = false;
	std::unique_ptr<NaluSpliterInterface> _naluSpliter;
	std::unique_ptr<NaluAggregatorInterface> _naluAggregator;
};

};
