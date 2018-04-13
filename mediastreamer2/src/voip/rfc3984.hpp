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

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msqueue.h"
#include "mediastreamer2/msfactory.h"

/*
 T h*is file declares an API useful to pack/unpack H264 nals as described in RFC3984
 It is part of the public API to allow external H264 plugins use this api.
 */

namespace mediastreamer2 {

class Rfc3984Context {
public:
	enum class Status {
		FrameAvailable = 1,
		FrameCorrupted = 1<<1,
		IsKeyFrame = 1<<2, // set when a frame has SPS + PPS or IDR (possibly both)
		NewSPS = 1<<3,
		NewPPS = 1<<4,
		HasSPS = 1<<5,
		HasPPS = 1<<6,
		HasIDR = 1<<7,
	};

	Rfc3984Context();
	Rfc3984Context(MSFactory *factory);
	~Rfc3984Context();

	void setMode(int mode) {this->mode = mode;}
	int getMode() const {return this->mode;}

	// some stupid phones don't decode STAP-A packets ...
	void enableStapA(bool yesno) {this->stap_a_allowed = yesno;}
	bool stapAEnabled() const {return this->stap_a_allowed;}

	void setMaxPayloadSize(int size) {this->maxsz = size;}
	int getMaxPayloadSize() {return this->maxsz;}

	void setOutOfBandSpsPps(mblk_t *sps, mblk_t *pps);

	// process NALUs and pack them into rtp payloads
	void pack(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);

	/**
	 * Process incoming rtp data and output NALUs, whenever possible.
	 * @param ctx the Rfc3984Context object
	 * @param im a new H264 packet to process
	 * @param naluq a MSQueue into which a frame ready to be decoded will be output, in the form of a sequence of NAL units.
	 * @return a bitmask of Rfc3984Status values.
	 * The return value is a bitmask of the #Rfc3984Status enum.
	 **/
	unsigned int unpack(mblk_t *im, MSQueue *naluq);

private:
	void packMode0(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);
	void packMode1(MSQueue *naluq, MSQueue *rtpq, uint32_t ts);
	void fragNaluAndSend(MSQueue *rtpq, uint32_t ts, mblk_t *nalu, bool_t marker, int maxsize);
	void sendPacket(MSQueue *rtpq, uint32_t ts, mblk_t *m, bool_t marker);
	unsigned int outputFrame(MSQueue *out, unsigned int flags);
	void storeNal(mblk_t *nal);
	mblk_t *aggregateFUA(mblk_t *im);

	static mblk_t *concatNalus(mblk_t *m1, mblk_t *m2);
	static mblk_t *prependStapA(mblk_t *m);
	static uint8_t nalHeaderGetNri(const uint8_t *h) {return ((*h) >> 5) & 0x3;}
	static void nalHeaderInit(uint8_t *h, uint8_t nri, uint8_t type) {*h=((nri&0x3)<<5) | (type & ((1<<5)-1));}
	static void putNalSize(mblk_t *m, size_t sz);
	static mblk_t *prependFuIndicatorAndHeader(mblk_t *m, uint8_t indicator, bool_t start, bool_t end, uint8_t type);
	static int isUniqueISlice(const uint8_t *slice_header);
	static bool_t updateParameterSet(mblk_t **last_parameter_set, mblk_t *new_parameter_set);

	MSQueue q;
	mblk_t *m = nullptr;
	int maxsz = MS_DEFAULT_MAX_PAYLOAD_SIZE;
	unsigned int status = 0; // bitmask of Rfc3984Status values
	mblk_t *sps;
	mblk_t *pps;
	mblk_t *last_sps = nullptr;
	mblk_t *last_pps = nullptr;
	uint32_t last_ts = 0x943FEA43;
	uint16_t ref_cseq;
	uint8_t mode = 0;
	bool stap_a_allowed = false;
	bool initialized_ref_cseq;
};

unsigned int operator&(unsigned int val1, Rfc3984Context::Status val2);
unsigned int &operator&=(unsigned int &val1, Rfc3984Context::Status val2);
unsigned int operator|(unsigned int val1, Rfc3984Context::Status val2);
unsigned int &operator|=(unsigned int &val1, Rfc3984Context::Status val2);

};
