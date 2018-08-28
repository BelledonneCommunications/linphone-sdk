/*
 Mediastreamer2 decoding-filter-wrapper.cpp
 Copyright (C) 2018 Belledonne Communications SARL

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "decoding-filter-wrapper.h"

using namespace std;

namespace mediastreamer {

int DecodingFilterWrapper::onAddFmtpCall(MSFilter *f, void *arg) {
	try {
		const char *fmtp = static_cast<const char *>(arg);
		static_cast<DecodingFilterImpl *>(f->data)->addFmtp(fmtp);
		return 0;
	} catch (const DecodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onResetFirstImageCall(MSFilter *f, void *arg) {
	try {
		static_cast<DecodingFilterImpl *>(f->data)->resetFirstImage();
		return 0;
	} catch (const DecodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onGetVideoSizeCall(MSFilter *f, void *arg) {
	try {
		MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		*vsize = static_cast<DecodingFilterImpl *>(f->data)->getVideoSize();
		return 0;
	} catch (const DecodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onGetFpsCall(MSFilter *f, void *arg) {
	try {
		float *fps = static_cast<float *>(arg);
		*fps = static_cast<DecodingFilterImpl *>(f->data)->getFps();
		return 0;
	} catch (const DecodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onGetOutFmtCall(MSFilter *f, void *arg) {
	try {
		MSPinFormat *pinFormat = static_cast<MSPinFormat *>(arg);
		pinFormat->fmt = static_cast<DecodingFilterImpl *>(f->data)->getOutFmt();
		return 0;
	} catch (const DecodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onEnableAvpfCall(MSFilter *f, void *arg) {
	try {
		const bool_t *enable = static_cast<bool_t *>(arg);
		static_cast<DecodingFilterImpl *>(f->data)->enableAvpf(enable);
		return 0;
	} catch (const DecodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onEnableFreezeOnErrorCall(MSFilter *f, void *arg) {
	try {
		const bool_t *enable = static_cast<bool_t *>(arg);
		static_cast<DecodingFilterImpl *>(f->data)->enableFreezeOnError(enable);
		return 0;
	} catch (const DecodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

}
