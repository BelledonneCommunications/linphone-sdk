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

int EncodingFilterWrapper::onGetVideoConfigurationsCall(MSFilter *f, void *arg) {
	try {
		const MSVideoConfiguration **vconfs = static_cast<const MSVideoConfiguration **>(arg);
		*vconfs = static_cast<EncoderFilter *>(f->data)->getVideoConfigurations();
		return 0;
	} catch (const EncoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onGetConfigurationCall(MSFilter *f, void *arg) {
	try {
		MSVideoConfiguration *vconf = static_cast<MSVideoConfiguration *>(arg);
		*vconf = static_cast<const EncoderFilter *>(f->data)->getVideoConfiguration();
		return 0;
	} catch (const EncoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onSetConfigurationCall(MSFilter *f, void *arg) {
	try {
		const MSVideoConfiguration *vconf = static_cast<MSVideoConfiguration *>(arg);
		static_cast<EncoderFilter *>(f->data)->setVideoConfiguration(*vconf);
		return 0;
	} catch (const EncoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onEnableAvpfCall(MSFilter *f, void *arg) {
	try {
		const bool_t *enable = static_cast<bool_t *>(arg);
		static_cast<EncoderFilter *>(f->data)->enableAvpf(*enable);
		return 0;
	} catch (const EncoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onRequestVfuCall(MSFilter *f, void *arg) {
	try {
		static_cast<EncoderFilter *>(f->data)->requestVfu();
		return 0;
	} catch (const EncoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onNotifyPliCall(MSFilter *f, void *arg) {
	try {
		static_cast<EncoderFilter *>(f->data)->notifyPli();
		return 0;
	} catch (const EncoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onNotifyFirCall(MSFilter *f, void *arg) {
	try {
		static_cast<EncoderFilter *>(f->data)->notifyFir();
		return 0;
	} catch (const EncoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int EncodingFilterWrapper::onNotifySliCall(MSFilter *f, void *arg) {
	try {
		static_cast<EncoderFilter *>(f->data)->notifySli();
		return 0;
	} catch (const EncoderFilter::MethodCallFailed &) {
		return -1;
	}
}

}
