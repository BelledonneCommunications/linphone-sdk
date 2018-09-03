/*
 Mediastreamer2 h26x-decoder-filter.h
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

#include <memory>
#include <string>

#include "mediastreamer2/msvideo.h"

#include "nal-unpacker.h"
#include "video-decoder.h"

#include "filter-interface/decoder-filter.h"

namespace mediastreamer {

class H26xDecoderFilter: public DecoderFilter {
public:
	H26xDecoderFilter(MSFilter *f, const std::string &mime, VideoDecoder *decoder);

	void preprocess() override;
	void process() override;
	void postprocess() override;

	MSVideoSize getVideoSize() const override;
	float getFps() const override;
	const MSFmtDescriptor *getOutputFmt() const override;
	void addFmtp(const char *fmtp)  override {}

	void enableAvpf(bool enable) override;

	bool freezeOnErrorEnabled() const override {return _freezeOnError;}
	void enableFreezeOnError(bool enable) override;

	void resetFirstImage() override;

protected:
	MSVideoSize _vsize;
	MSAverageFPS _fps;
	bool _avpfEnabled = false;
	bool _freezeOnError = true;

	std::unique_ptr<NalUnpacker> _unpacker;
	std::unique_ptr<VideoDecoder> _codec;
	bool _firstImageDecoded = false;

	static const unsigned int _timeoutUs = 0;
};

}
