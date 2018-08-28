/*
 * Mediastreamer2 decoding-filter-impl.h
 * Copyright (C) 2018 Belledonne Communications SARL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include "mediastreamer2/msvideo.h"

#include "filter-impl-base.h"

namespace mediastreamer {

	class DecodingFilterImpl: public FilterImplBase {
	public:
		DecodingFilterImpl(MSFilter *f): FilterImplBase(f) {}

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
