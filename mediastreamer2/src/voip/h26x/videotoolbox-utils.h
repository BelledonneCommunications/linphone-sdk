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

#include <exception>
#include <string>

#include <VideoToolbox/VideoToolbox.h>

#include "mediastreamer2/msqueue.h"

#include "h26x-utils.h"

namespace mediastreamer {

std::string toString(OSStatus status);

class AppleOSError: public std::exception {
public:
	AppleOSError(OSStatus status): _desc(toString(status)) {}
	const char *what() const noexcept override {return _desc.c_str();}

private:
	std::string _desc;
};

class VideoToolboxUtilities {
public:
	virtual ~VideoToolboxUtilities() = default;

	virtual CMVideoCodecType getCodecType() const = 0;
	virtual CFStringRef getDefaultProfileLevel() const = 0;

	bool encoderIsAvailable() const;
	void getParameterSets(const CMFormatDescriptionRef format, MSQueue *outPs) const;
	CMFormatDescriptionRef createFormatDescription(const H26xParameterSetsStore &psStore) const;

	static VideoToolboxUtilities *create(const std::string &mime);

protected:
	virtual void getParameterSet(const CMFormatDescriptionRef format, size_t offset, const uint8_t *&parameterSet, size_t &parameterSetSize, size_t &parameterSetsCount) const = 0;
	virtual CMFormatDescriptionRef createFormatDescription(size_t parameterSetsCount, const uint8_t *parameterSets[], const size_t parameterSetSizes[]) const = 0;

	static void loadCodecAvailability();

	static std::map<CMVideoCodecType,bool> _codecAvailability;
};

} // namespace mediastreamer
