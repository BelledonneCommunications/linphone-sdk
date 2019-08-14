/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>

#include "h26x-encoder.h"
#include "nal-packer.h"

#include "filter-interface/encoder-filter.h"

namespace mediastreamer {

class H26xEncoderFilter: public EncoderFilter {
public:
	void preprocess() override;
	void process() override;
	void postprocess() override;

	const MSVideoConfiguration *getVideoConfigurations() const override;
	const MSVideoConfiguration &getVideoConfiguration() const override {return _vconf;}
	void setVideoConfiguration(MSVideoConfiguration vconf) override;

	void enableAvpf(bool enable) override;

	void requestVfu() override;
	void notifyPli() override;
	void notifyFir() override;
	void notifySli() override;

protected:
	H26xEncoderFilter(MSFilter *f, H26xEncoder *encoder, const MSVideoConfiguration *vconfList);

	std::unique_ptr<H26xEncoder> _encoder;
	std::unique_ptr<NalPacker> _packer;
	const MSVideoConfiguration *_vconfList = nullptr;
	MSVideoConfiguration _vconf;
	bool _avpfEnabled = false;
	bool _firstFrameDecoded = false;

	MSVideoStarter _starter;
	MSIFrameRequestsLimiterCtx _iframeLimiter;
};

}
