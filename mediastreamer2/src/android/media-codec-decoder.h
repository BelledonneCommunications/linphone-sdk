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

	void flush();

	bool feed(const std::vector<uint8_t> &encodedFrame, uint64_t timestamp);
	mblk_t *fetch();

private:
	void createImpl(const std::string &mime);

	AMediaCodec *_impl = nullptr;
	int _pendingFrames = 0;
	MSVideoSize _vsize;
	MSYuvBufAllocator *_bufAllocator = nullptr;

	static const unsigned int _timeoutUs = 0;
};

class MediaCodecDecoderFilterImpl {
public:
	MediaCodecDecoderFilterImpl(MSFilter *f, const std::string &mimeType, NalUnpacker *unpacker, H26xParameterSetsStore *psStore);
	virtual ~MediaCodecDecoderFilterImpl();

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
	media_status_t initMediaCodec();
	void flush(bool with_reset);

	virtual bool isKeyFrame(const MSQueue *frame) const = 0;

	MSVideoSize _vsize;
	MSAverageFPS _fps;
	bool _avpfEnabled = false;
	bool _freezeOnError = true;

	MSFilter *_f = nullptr;
	std::string _mimeType;
	std::unique_ptr<NalUnpacker> _unpacker;
	std::unique_ptr<H26xParameterSetsStore> _psStore;
	AMediaCodec *_codec = nullptr;
	unsigned int _packetNum = 0;
	std::vector<uint8_t> _bitstream;
	MSYuvBufAllocator *_bufAllocator = nullptr;
	int _pendingFrames = 0;
	bool _firstImageDecoded = false;
	bool _needKeyFrame = true;


	static const unsigned int _timeoutUs = 0;
};

}
