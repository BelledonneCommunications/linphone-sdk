/*
 Mediastreamer2 media-codec-decoder.h
 Copyright (C) 2018 Belledonne Communications SARL

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

#include <vector>

#include "h26x-utils.h"
#include "media/NdkMediaCodec.h"

#include "h26x-decoder.h"

namespace mediastreamer {

class MediaCodecDecoder: public H26xDecoder {
public:
	virtual ~MediaCodecDecoder();

	void waitForKeyFrame()  override {_needKeyFrame = true;}

	bool feed(MSQueue *encodedFrame, uint64_t timestamp) override;
	Status fetch(mblk_t *&frame) override;

protected:
	class BufferFlag {
	public:
		static const uint32_t None = 0;
		static const uint32_t KeyFrame = 1;
		static const uint32_t CodecConfig = 1<<1;
		static const uint32_t EndOfStream = 1<<2;
		static const uint32_t PartialFrame = 1<<3;
	};

	MediaCodecDecoder(const std::string &mime);
	virtual bool setParameterSets(MSQueue *parameterSet, uint64_t timestamp);
	AMediaFormat *createFormat(const std::string &mime) const;
	void startImpl();
	void stopImpl();
	bool feed(MSQueue *encodedFrame, uint64_t timestamp, bool isPs);
	bool isKeyFrame(const MSQueue *frame) const;

	AMediaCodec *_impl = nullptr;
	AMediaFormat *_format = nullptr;
	MSYuvBufAllocator *_bufAllocator = nullptr;
	std::vector<uint8_t> _bitstream;
	std::unique_ptr<H26xNaluHeader> _naluHeader;
	std::unique_ptr<H26xParameterSetsStore> _psStore;
	int _pendingFrames = 0;
	bool _needKeyFrame = true;
	bool _needParameters = true;

	static const unsigned int _timeoutUs = 0;
};

}
