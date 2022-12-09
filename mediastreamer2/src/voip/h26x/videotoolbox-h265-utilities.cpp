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

#include <dlfcn.h>

#include "videotoolbox-h265-utilities.h"

using namespace std;

namespace mediastreamer {

CFStringRef VideoToolboxH265Utilities::_kVTProfileLevel_HEVC_Main_AutoLevel = nullptr;
VideoToolboxH265Utilities::GetParameterSetNativeFunc VideoToolboxH265Utilities::_CMVideoFormatDescriptionGetHEVCParameterSetAtIndex = nullptr;
VideoToolboxH265Utilities::CreateFormatDescriptionNativeFunc VideoToolboxH265Utilities::_CMVideoFormatDescriptionCreateFromHEVCParameterSets = nullptr;

VideoToolboxH265Utilities::VideoToolboxH265Utilities() {
	if (_kVTProfileLevel_HEVC_Main_AutoLevel == nullptr) {
		_kVTProfileLevel_HEVC_Main_AutoLevel = *reinterpret_cast<CFStringRef *>(dlsym(RTLD_DEFAULT, "kVTProfileLevel_HEVC_Main_AutoLevel"));
	}
	if (_CMVideoFormatDescriptionGetHEVCParameterSetAtIndex == nullptr) {
		void *symPtr = dlsym(RTLD_DEFAULT, "CMVideoFormatDescriptionGetHEVCParameterSetAtIndex");
		_CMVideoFormatDescriptionGetHEVCParameterSetAtIndex = reinterpret_cast<GetParameterSetNativeFunc>(symPtr);
	}
	if (_CMVideoFormatDescriptionCreateFromHEVCParameterSets == nullptr) {
		void *symPtr = dlsym(RTLD_DEFAULT, "CMVideoFormatDescriptionCreateFromHEVCParameterSets");
		_CMVideoFormatDescriptionCreateFromHEVCParameterSets = reinterpret_cast<CreateFormatDescriptionNativeFunc>(symPtr);
	}
}

CMVideoCodecType VideoToolboxH265Utilities::getCodecType() const {
	return kCMVideoCodecType_HEVC;
}

CFStringRef VideoToolboxH265Utilities::getDefaultProfileLevel() const {
	return _kVTProfileLevel_HEVC_Main_AutoLevel;
}

void VideoToolboxH265Utilities::getParameterSet(const CMFormatDescriptionRef format, size_t offset, const uint8_t *&parameterSet, size_t &parameterSetSize, size_t &parameterSetsCount) const {
	OSStatus status = _CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(format, offset, &parameterSet, &parameterSetSize, &parameterSetsCount, nullptr);
	if (status != noErr) throw AppleOSError(status);
}

CMFormatDescriptionRef VideoToolboxH265Utilities::createFormatDescription(size_t paramterSetsCount, const uint8_t *parameterSets[], const size_t parameterSetSizes[]) const {
	CMFormatDescriptionRef formatDesc;
	OSStatus status = _CMVideoFormatDescriptionCreateFromHEVCParameterSets(nullptr, paramterSetsCount, parameterSets, parameterSetSizes, 4, nullptr, &formatDesc);
	if (status != noErr) {
		throw AppleOSError(status);
	}
	return formatDesc;
}

}
