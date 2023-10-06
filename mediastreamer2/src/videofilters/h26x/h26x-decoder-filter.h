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

#include <bctoolbox/defs.h>

#include "mediastreamer2/msvideo.h"

#include "h26x-decoder.h"
#include "nal-unpacker.h"
#include "stream_regulator.h"

#include "filter-interface/decoder-filter.h"

namespace mediastreamer {

class H26xDecoderFilter : public DecoderFilter {
public:
	H26xDecoderFilter(MSFilter *f, H26xDecoder *decoder);

	void preprocess() override;
	void process() override;
	void postprocess() override;

	MSVideoSize getVideoSize() const override;
	float getFps() const override;
	const MSFmtDescriptor *getOutputFmt() const override;
	void addFmtp(BCTBX_UNUSED(const char *fmtp)) override {
	}

	void enableAvpf(bool enable) override;

	bool freezeOnErrorEnabled() const override {
		return _freezeOnError;
	}
	void enableFreezeOnError(bool enable) override;

	void resetFirstImage() override;

protected:
	MSVideoSize _vsize;
	MSAverageFPS _fps;
	MSStreamRegulator *_regulator = nullptr;
	bool _avpfEnabled = false;
	bool _freezeOnError = true;
	bool _firstImageDecoded = false;
	bool _useRegulator = false;

	std::unique_ptr<NalUnpacker> _unpacker;
	std::unique_ptr<H26xDecoder> _codec;

	static const unsigned int _timeoutUs = 0;
};

} // namespace mediastreamer
