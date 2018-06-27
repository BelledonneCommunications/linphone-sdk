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

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include <media/NdkMediaCodec.h>

#include "mediastreamer2/msvideo.h"

#include "nal-unpacker.h"

namespace mediastreamer {

class MediaCodecDecoder {
public:
	MediaCodecDecoder(const std::string &mime);
	~MediaCodecDecoder();

	void setParameterSets(MSQueue *paramterSets);
	void waitForKeyFrame() {_needKeyFrame = true;}
	void flush();

	bool feed(MSQueue *encodedFrame, uint64_t timestamp);
	mblk_t *fetch();

private:
	enum class State {
		Reset,
		Ready
	};

	class BufferFlag {
	public:
		static const uint32_t None = 0;
		static const uint32_t KeyFrame = 1;
		static const uint32_t CodecConfig = 1<<1;
		static const uint32_t EndOfStream = 1<<2;
		static const uint32_t PartialFrame = 1<<3;
	};

	void createImpl(const std::string &mime);
	bool feed(MSQueue *encodedFrame, uint64_t timestamp, bool isPs);
	bool isKeyFrame(const MSQueue *frame) const;
	void setState(State state);
	static const char *toString(State state);

	AMediaCodec *_impl = nullptr;
	int _pendingFrames = 0;
	MSVideoSize _vsize;
	MSYuvBufAllocator *_bufAllocator = nullptr;
	uint64_t _lastTs = 0;
	State _state = State::Reset;
	std::vector<uint8_t> _bitstream;
	std::unique_ptr<H26xNaluHeader> _naluHeader;
	bool _needKeyFrame = true;

	static const unsigned int _timeoutUs = 0;
};

class MediaCodecDecoderFilterImpl {
public:
	MediaCodecDecoderFilterImpl(MSFilter *f, const std::string &mime);
	virtual ~MediaCodecDecoderFilterImpl() = default;

	void preprocess();
	void process();
	void postprocess();

	MSVideoSize getVideoSize() const;
	float getFps() const;
	const MSFmtDescriptor *getOutFmt() const;
	void addFmtp(const char *fmtp) {}

	void enableAvpf(bool enable);
	void enableFreezeOnError(bool enable);
	void resetFirstImage();

protected:
	void extractParameterSets(MSQueue *frame, MSQueue *paramterSets);
	virtual bool isKeyFrame(const MSQueue *frame) const = 0;

	MSVideoSize _vsize;
	MSAverageFPS _fps;
	bool _avpfEnabled = false;
	bool _freezeOnError = true;

	MSFilter *_f = nullptr;
	std::unique_ptr<NalUnpacker> _unpacker;
	std::unique_ptr<H26xParameterSetsStore> _psStore;
	std::unique_ptr<H26xNaluHeader> _naluHeader;
	MediaCodecDecoder _codec;
	bool _firstImageDecoded = false;


	static const unsigned int _timeoutUs = 0;
};

}
