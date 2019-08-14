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

#include "mediastreamer2/msvideo.h"

#include "filter-base.h"

namespace mediastreamer {

	class DecoderFilter: public FilterBase {
	public:
		DecoderFilter(MSFilter *f): FilterBase(f) {}

		virtual MSVideoSize getVideoSize() const = 0;
		virtual float getFps() const = 0;
		virtual const MSFmtDescriptor *getOutputFmt() const = 0;
		virtual void addFmtp(const char *fmtp) = 0;

		virtual void enableAvpf(bool enable) = 0;
		virtual bool freezeOnErrorEnabled() const = 0;
		virtual void enableFreezeOnError(bool enable) = 0;
		virtual void resetFirstImage() = 0;
	};

} // namespace mediastreamer
