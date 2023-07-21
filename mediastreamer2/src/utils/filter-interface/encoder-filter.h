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

#include "mediastreamer2/msvideo.h"

#include "filter-base.h"

namespace mediastreamer {

class EncoderFilter : public FilterBase {
public:
	EncoderFilter(MSFilter *f) : FilterBase(f) {
	}

	virtual const MSVideoConfiguration *getVideoConfigurations() const = 0;

	virtual const MSVideoConfiguration &getVideoConfiguration() const = 0;
	virtual void setVideoConfiguration(MSVideoConfiguration vconf) = 0;

	virtual void enableAvpf(bool enable) = 0;

	virtual void requestVfu() = 0;
	virtual void notifyPli() = 0;
	virtual void notifyFir() = 0;
	virtual void notifySli() = 0;

	virtual void enableDivideIntoPacketsEqualSize(bool enable) = 0;
};

} // namespace mediastreamer
