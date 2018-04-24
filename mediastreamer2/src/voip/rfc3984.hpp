/*
mediastreamer2 library - modular sound and video processing and streaming
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

#include <bitset>
#include <memory>

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msqueue.h"
#include "mediastreamer2/msfactory.h"

/*
 * This file declares an API useful to pack/unpack H264 nals as described in RFC3984
 * It is part of the public API to allow external H264 plugins use this api.
 */

namespace mediastreamer2 {

class NaluAggregatorInterface {
public:
	virtual ~NaluAggregatorInterface() = default;
	virtual mblk_t *feedNalu(mblk_t *nalu) = 0;
	virtual bool isAggregating() const = 0;
	virtual void reset() = 0;
};

class NaluSpliterInterface {
public:
	virtual ~NaluSpliterInterface() = default;
	virtual void feedNalu(mblk_t *nalu) = 0;
	virtual MSQueue *getNalus() = 0;
};

class H264NalToFuaSpliter: public NaluSpliterInterface {
public:
	H264NalToFuaSpliter(size_t maxsize): _maxsize(maxsize) {ms_queue_init(&_q);}
	~H264NalToFuaSpliter() {ms_queue_flush(&_q);}

	void feedNalu(mblk_t *nalu) override;
	MSQueue *getNalus() override {return &_q;};

private:
	size_t _maxsize;
	MSQueue _q;
};

class Rfc3984Packer {
public:
	enum PacketizationMode {
		SingleNalUnitMode,
		NonInterleavedMode
	};

	Rfc3984Packer(): _spliter(new H264NalToFuaSpliter(MS_DEFAULT_MAX_PAYLOAD_SIZE)) {}
	Rfc3984Packer(MSFactory *factory);

	void setMode(PacketizationMode mode) {_mode = mode;}
	PacketizationMode getMode() const {return _mode;}

	// some stupid phones don't decode STAP-A packets ...
	void enableAggregation(bool yesno) {_aggregationEnabled = yesno;}
	bool aggregationEnabled() const {return _aggregationEnabled;}

	void setMaxPayloadSize(int size) {_maxSize = size;}
	int getMaxPayloadSize() {return _maxSize;}

	// process NALUs and pack them into rtp payloads
	void pack(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);

private:
	void packInSingleNalUnitMode(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);
	void packInNonInterleavedMode(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);
	void fragNaluAndSend(MSQueue *rtpq, uint32_t ts, mblk_t *nalu, bool_t marker, int maxsize);
	void sendPacket(MSQueue *rtpq, uint32_t ts, mblk_t *m, bool_t marker);

	static mblk_t *concatNalus(mblk_t *m1, mblk_t *m2);
	static mblk_t *prependStapA(mblk_t *m);
	static void putNalSize(mblk_t *m, size_t sz);

	int _maxSize = MS_DEFAULT_MAX_PAYLOAD_SIZE;
	uint16_t _refCSeq = 0;
	PacketizationMode _mode = SingleNalUnitMode;
	bool _aggregationEnabled = false;
	std::unique_ptr<H264NalToFuaSpliter> _spliter;
};

class Unpacker {
public:
	class StatusFlag {
	public:
		static const size_t FrameAvailable = 0;
		static const size_t FrameCorrupted = 1;
		static const size_t IsKeyFrame = 2;
	};
	typedef std::bitset<3> Status;

	Unpacker(NaluAggregatorInterface *aggregator, NaluSpliterInterface *spliter);
	virtual ~Unpacker() {ms_queue_flush(&_q);}

	/**
	 * Process incoming rtp data and output NALUs, whenever possible.
	 * @param ctx the Rfc3984Context object
	 * @param im a new H264 packet to process
	 * @param naluq a MSQueue into which a frame ready to be decoded will be output, in the form of a sequence of NAL units.
	 * @return a bitmask of Rfc3984Status values.
	 * The return value is a bitmask of the #Rfc3984Status enum.
	 **/
	Status unpack(mblk_t *im, MSQueue *out);

protected:
	enum class PacketType {
		SingleNalUnit,
		AggregationPacket,
		FragmentationUnit
	};

	virtual Status outputFrame(MSQueue *out, const Status &flags);
	virtual void storeNal(mblk_t *nal);

	virtual PacketType getNaluType(const mblk_t *nalu) const = 0;

	MSQueue _q;
	Status _status = 0;
	uint32_t _lastTs = 0x943FEA43;
	bool _initializedRefCSeq = false;
	uint16_t _refCSeq = 0;
	std::unique_ptr<NaluAggregatorInterface> _naluAggregator;
	std::unique_ptr<NaluSpliterInterface> _naluSpliter;
};

class H264Tools {
public:
	static void nalHeaderInit(uint8_t *h, uint8_t nri, uint8_t type) {*h=((nri&0x3)<<5) | (type & ((1<<5)-1));}
	static mblk_t *prependFuIndicatorAndHeader(mblk_t *m, uint8_t indicator, bool_t start, bool_t end, uint8_t type);
};

class H264FUAAggregator: public NaluAggregatorInterface {
public:
	~H264FUAAggregator() {if (_m) freemsg(_m);}
	mblk_t *feedNalu(mblk_t *im) override;
	bool isAggregating() const override {return _m != nullptr;}
	void reset() override;

private:
	mblk_t *_m = nullptr;
};

class H264StapASpliter: public NaluSpliterInterface {
public:
	H264StapASpliter() {ms_queue_init(&_q);}
	~H264StapASpliter() {ms_queue_flush(&_q);}
	void feedNalu(mblk_t *im) override;
	MSQueue *getNalus() override {return &_q;}

private:
	MSQueue _q;
};

class Rfc3984Unpacker: public Unpacker {
public:
	Rfc3984Unpacker(): Unpacker(new H264FUAAggregator(), new H264StapASpliter()) {}
	~Rfc3984Unpacker();

	void setOutOfBandSpsPps(mblk_t *sps, mblk_t *pps);

private:
	Unpacker::PacketType getNaluType(const mblk_t *nalu) const override;
	Status outputFrame(MSQueue *out, const Status &flags) override;

	mblk_t *_sps = nullptr;
	mblk_t *_pps = nullptr;
};

}; // end of mediastreamer2 namespace
