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

#include "../obu/obu-packer.h"
#include "av1-encoder.h"
#include "filter-interface/encoder-filter.h"

namespace mediastreamer {

class Av1EncoderFilter : public EncoderFilter {
public:
	Av1EncoderFilter(MSFilter *f, Av1Encoder *encoder, const MSVideoConfiguration *vconfList);

	void preprocess() override;
	void process() override;
	void postprocess() override;

	const MSVideoConfiguration *getVideoConfigurations() const override;
	const MSVideoConfiguration &getVideoConfiguration() const override {
		return mVconf;
	}
	void setVideoConfiguration(MSVideoConfiguration vconf) override;

	void enableAvpf(bool enable) override;

	void requestVfu() override;
	void notifyPli() override;
	void notifyFir() override;
	void notifySli() override;

protected:
	MSVideoConfiguration mVconf;
	bool mAvpfEnabled = false;
	bool mFirstFrameEncoded = false;

	MSVideoStarter mStarter;
	MSIFrameRequestsLimiterCtx mIframeLimiter;

	std::unique_ptr<Av1Encoder> mEncoder;
	ObuPacker mPacker;

	const MSVideoConfiguration *mVconfList = nullptr;
};

} // namespace mediastreamer
