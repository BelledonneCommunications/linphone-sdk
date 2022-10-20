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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"

#include "filter-interface/encoder-filter.h"
#include "filter-wrapper-base.h"

namespace mediastreamer {

class EncodingFilterWrapper {
public:
	static int onGetVideoConfigurationsCall(MSFilter *f, void *arg);

	static int onGetConfigurationCall(MSFilter *f, void *arg);
	static int onSetConfigurationCall(MSFilter *f, void *arg);

	static int onEnableAvpfCall(MSFilter *f, void *arg);

	static int onRequestVfuCall(MSFilter *f, void *arg);
	static int onNotifyPliCall(MSFilter *f, void *arg);
	static int onNotifyFirCall(MSFilter *f, void *arg);
	static int onNotifySliCall(MSFilter *f, void *arg);
};

} // namespace mediastreamer

#define MS_ENCODING_FILTER_WRAPPER_METHODS_DECLARATION(base_name) \
static MSFilterMethod  MS_FILTER_WRAPPER_METHODS_NAME(base_name)[] = { \
	{ MS_FILTER_REQ_VFU                       , EncodingFilterWrapper::onRequestVfuCall             }, \
	{ MS_VIDEO_ENCODER_REQ_VFU                , EncodingFilterWrapper::onRequestVfuCall             }, \
	{ MS_VIDEO_ENCODER_NOTIFY_PLI             , EncodingFilterWrapper::onNotifyPliCall              }, \
	{ MS_VIDEO_ENCODER_NOTIFY_FIR             , EncodingFilterWrapper::onNotifyFirCall              }, \
	{ MS_VIDEO_ENCODER_NOTIFY_SLI             , EncodingFilterWrapper::onNotifySliCall              }, \
	{ MS_VIDEO_ENCODER_ENABLE_AVPF            , EncodingFilterWrapper::onEnableAvpfCall             }, \
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST , EncodingFilterWrapper::onGetVideoConfigurationsCall }, \
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION      , EncodingFilterWrapper::onGetConfigurationCall       }, \
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION      , EncodingFilterWrapper::onSetConfigurationCall       }, \
	{ 0                                       , nullptr                                             } \
}

#define MS_ENCODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(base_name, id, text, enc_fmt, flags) \
	MS_FILTER_WRAPPER_FILTER_DESCRIPTION_BASE(base_name, id, text, MS_FILTER_ENCODER, enc_fmt, 1, 1, flags)
