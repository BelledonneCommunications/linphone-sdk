/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
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

#include "../obu/obu-unpacker.h"
#include "av1-decoder.h"
#include "filter-interface/decoder-filter.h"
#include "mediastreamer2/msvideo.h"

namespace mediastreamer {

class Av1DecoderFilter : public DecoderFilter {
public:
	Av1DecoderFilter(MSFilter *f, Av1Decoder *codec);

	void preprocess() override;
	void process() override;
	void postprocess() override;

	MSVideoSize getVideoSize() const override;
	float getFps() const override;
	const MSFmtDescriptor *getOutputFmt() const override;
	void addFmtp([[maybe_unused]] const char *fmtp) override {
	}

	void enableAvpf(bool enable) override;

	bool freezeOnErrorEnabled() const override {
		return mFreezeOnError;
	}
	void enableFreezeOnError(bool enable) override;

	void resetFirstImage() override;

protected:
	MSVideoSize mVsize;
	MSAverageFPS mFps{};
	bool mAvpfEnabled = false;
	bool mFreezeOnError = true;
	bool mFirstImageDecoded = false;

	std::unique_ptr<Av1Decoder> mCodec;
	ObuUnpacker mUnpacker;
};

} // namespace mediastreamer
