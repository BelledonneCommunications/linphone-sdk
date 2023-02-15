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

#include <vector>

#include "media-codec-decoder.h"

namespace mediastreamer {

class MediaCodecH264Decoder : public MediaCodecDecoder {
public:
	MediaCodecH264Decoder();
	~MediaCodecH264Decoder();
	bool setParameterSets(MSQueue *parameterSet, uint64_t timestamp) override;

private:
	struct DeviceInfo {
		std::string manufacturer;
		std::string model;
		std::string platform;

		bool operator==(const DeviceInfo &info) const;
		bool weakEquals(const DeviceInfo &info) const;
		std::string toString() const;
	};

	bool isNewPps(mblk_t *sps);
	static DeviceInfo getDeviceInfo();

	mblk_t *_lastSps = nullptr;
	bool _resetOnPsReceiving = false;
	static const std::vector<const DeviceInfo>
	    _tvDevices; // List of devices whose H264 hardware decoder needs to be initialized with the right definition
};

} // namespace mediastreamer
