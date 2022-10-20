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
		static_cast<EncoderFilter *>(f->data)->enableAvpf(*static_cast<bool *>(arg));
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
