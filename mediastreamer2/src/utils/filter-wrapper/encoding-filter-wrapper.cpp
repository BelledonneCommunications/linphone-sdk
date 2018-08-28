/*
 Mediastreamer2 encoding-filter-wrapper.cpp
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

#include "encoding-filter-wrapper.h"

using namespace std;

namespace mediastreamer {

int EncodingFilterWrapper::onGetBitrateCall(MSFilter *f, void *arg) {
	try {
		int *bitrate = static_cast<int *>(arg);
		*bitrate = static_cast<EncodingFilterImpl *>(f->data)->getBitrate();
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onSetBitrateCall(MSFilter *f, void *arg) {
	try {
		int br = *static_cast<int *>(arg);
		static_cast<EncodingFilterImpl *>(f->data)->setBitrate(br);
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onGetFpsCall(MSFilter *f, void *arg) {
	try {
		float *fps = static_cast<float *>(arg);
		*fps = static_cast<EncodingFilterImpl *>(f->data)->getFps();
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onSetFpsCall(MSFilter *f, void *arg) {
	try {
		float fps = *static_cast<float *>(arg);
		static_cast<EncodingFilterImpl *>(f->data)->setFps(fps);
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onGetVideoSizeCall(MSFilter *f, void *arg) {
	try {
		MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		*vsize = static_cast<EncodingFilterImpl *>(f->data)->getVideoSize();
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onSetVideoSizeCall(MSFilter *f, void *arg) {
	try {
		const MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		static_cast<EncodingFilterImpl *>(f->data)->setVideoSize(*vsize);
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onGetVideoConfigurationsCall(MSFilter *f, void *arg) {
	try {
		const MSVideoConfiguration **vconfs = static_cast<const MSVideoConfiguration **>(arg);
		*vconfs = static_cast<EncodingFilterImpl *>(f->data)->getVideoConfigurations();
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onSetVideoConfigurationsCall(MSFilter *f, void *arg) {
	try {
		const MSVideoConfiguration * const *vconfs = static_cast<const MSVideoConfiguration * const *>(arg);
		static_cast<EncodingFilterImpl *>(f->data)->setVideoConfigurations(*vconfs);
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onSetConfigurationCall(MSFilter *f, void *arg) {
	try {
		const MSVideoConfiguration *vconf = static_cast<MSVideoConfiguration *>(arg);
		static_cast<EncodingFilterImpl *>(f->data)->setVideoConfiguration(vconf);
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onEnableAvpfCall(MSFilter *f, void *arg) {
	try {
		bool_t enable = *static_cast<bool_t *>(arg);
		static_cast<EncodingFilterImpl *>(f->data)->enableAvpf(enable);
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onRequestVfuCall(MSFilter *f, void *arg) {
	try {
		static_cast<EncodingFilterImpl *>(f->data)->requestVfu();
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onNotifyPliCall(MSFilter *f, void *arg) {
	try {
		static_cast<EncodingFilterImpl *>(f->data)->notifyPli();
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onNotifyFirCall(MSFilter *f, void *arg) {
	try {
		static_cast<EncodingFilterImpl *>(f->data)->notifyFir();
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onNotifySliCall(MSFilter *f, void *arg) {
	try {
		static_cast<EncodingFilterImpl *>(f->data)->notifySli();
		return 0;
	} catch (const EncodingFilterImpl::MethodCallFailed &) {
		return -1;
	}
}

}
