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

#include "videotoolbox-h264-utilities.h"

using namespace std;

namespace mediastreamer {

CMVideoCodecType VideoToolboxH264Utilities::getCodecType() const {
	return kCMVideoCodecType_H264;
}

CFStringRef VideoToolboxH264Utilities::getDefaultProfileLevel() const {
	return kVTProfileLevel_H264_Baseline_AutoLevel;
}

void VideoToolboxH264Utilities::getParameterSet(const CMFormatDescriptionRef format, size_t offset, const uint8_t *&parameterSet, size_t &parameterSetSize, size_t &parameterSetsCount) const {
	OSStatus status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, offset, &parameterSet, &parameterSetSize, &parameterSetsCount, nullptr);
	if (status != noErr) throw AppleOSError(status);
}

CMFormatDescriptionRef VideoToolboxH264Utilities::createFormatDescription(size_t parameterSetsCount, const uint8_t *parameterSets[], const size_t parameterSetSizes[]) const {
	CMFormatDescriptionRef formatDesc;
	OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(nullptr, parameterSetsCount, parameterSets, parameterSetSizes, 4, &formatDesc);
	if (status != noErr) {
		throw AppleOSError(status);
	}
	return formatDesc;
}

}
