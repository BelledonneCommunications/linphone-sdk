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

#include <memory>

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msqueue.h"
#include "mediastreamer2/msfactory.h"

/*
 * This file declares an API useful to pack/unpack H264 nals as described in RFC3984
 * It is part of the public API to allow external H264 plugins use this api.
 */

namespace mediastreamer2 {

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

class H264NaluAggregator: public NalPacker::NaluAggregatorInterface {
public:
	H264NaluAggregator() {}
	~H264NaluAggregator() {reset();}

	size_t getMaxSize() const override {return _maxsize;}
	void setMaxSize(size_t maxSize) override;

	mblk_t *feed(mblk_t *nalu) override;
	bool isAggregating() const override {return bool(_stap);}
	void reset() override;
	mblk_t *completeAggregation() override;

private:
	static mblk_t *concatNalus(mblk_t *m1, mblk_t *m2);
	static mblk_t *prependStapA(mblk_t *m);
	static void putNalSize(mblk_t *m, size_t sz);

	mblk_t *_stap = nullptr;
	size_t _size = 0;
	size_t _maxsize = MS_DEFAULT_MAX_PAYLOAD_SIZE;
};

class H264NaluSpliter: public NalPacker::NaluSpliterInterface {
public:
	H264NaluSpliter() {ms_queue_init(&_q);}
	~H264NaluSpliter() {ms_queue_flush(&_q);}

	size_t getMaxSize() const override {return _maxsize;}
	void setMaxSize(size_t maxSize) override {_maxsize = maxSize;}

	void feed(mblk_t *nalu) override;
	MSQueue *getPackets() override {return &_q;};

private:
	size_t _maxsize = MS_DEFAULT_MAX_PAYLOAD_SIZE;
	MSQueue _q;
};

class H264NalPacker: public NalPacker {
public:
	enum PacketizationMode {
		SingleNalUnitMode,
		NonInterleavedMode
	};

	H264NalPacker(): NalPacker(new H264NaluSpliter(), new H264NaluAggregator()) {}
	H264NalPacker(MSFactory *factory): NalPacker(new H264NaluSpliter(), new H264NaluAggregator(), factory) {}
};

class NalUnpacker {
public:
	struct Status {
		bool frameAvailable = false;
		bool frameCorrupted = false;
		bool isKeyFrame = false;

		Status &operator|=(const Status &s2);
		unsigned int toUInt() const;
	};

	class FuAggregatorInterface {
	public:
		virtual ~FuAggregatorInterface() = default;
		virtual mblk_t *feed(mblk_t *packet) = 0;
		virtual bool isAggregating() const = 0;
		virtual void reset() = 0;
		virtual mblk_t *completeAggregation() = 0;
	};

	class ApSpliterInterface {
	public:
		virtual ~ApSpliterInterface() = default;
		virtual void feed(mblk_t *packet) = 0;
		virtual MSQueue *getNalus() = 0;
	};

	NalUnpacker(FuAggregatorInterface *aggregator, ApSpliterInterface *spliter);
	virtual ~NalUnpacker() {ms_queue_flush(&_q);}

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
	Status _status;
	uint32_t _lastTs = 0x943FEA43;
	bool _initializedRefCSeq = false;
	uint16_t _refCSeq = 0;
	std::unique_ptr<FuAggregatorInterface> _fuAggregator;
	std::unique_ptr<ApSpliterInterface> _apSpliter;
};

class H264Tools {
public:
	static void nalHeaderInit(uint8_t *h, uint8_t nri, uint8_t type) {*h=((nri&0x3)<<5) | (type & ((1<<5)-1));}
	static mblk_t *prependFuIndicatorAndHeader(mblk_t *m, uint8_t indicator, bool_t start, bool_t end, uint8_t type);
};

class H264FuaAggregator: public NalUnpacker::FuAggregatorInterface {
public:
	~H264FuaAggregator() {if (_m) freemsg(_m);}
	mblk_t *feed(mblk_t *im) override;
	bool isAggregating() const override {return _m != nullptr;}
	void reset() override;
	mblk_t *completeAggregation() override;

private:
	mblk_t *_m = nullptr;
};

class H264StapaSpliter: public NalUnpacker::ApSpliterInterface {
public:
	H264StapaSpliter() {ms_queue_init(&_q);}
	~H264StapaSpliter() {ms_queue_flush(&_q);}
	void feed(mblk_t *im) override;
	MSQueue *getNalus() override {return &_q;}

private:
	MSQueue _q;
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

}; // end of mediastreamer2 namespace
