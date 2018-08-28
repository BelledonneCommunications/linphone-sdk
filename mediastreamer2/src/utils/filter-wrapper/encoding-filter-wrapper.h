/*
 Mediastreamer2 encoding-filter-wrapper.h
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

#pragma once

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"

#include "encoding-filter-impl.h"
#include "filter-wrapper-base.h"

namespace mediastreamer {

class EncodingFilterWrapper {
public:
	static int onGetBitrateCall(MSFilter *f, void *arg);
	static int onSetBitrateCall(MSFilter *f, void *arg);

	static int onGetFpsCall(MSFilter *f, void *arg);
	static int onSetFpsCall(MSFilter *f, void *arg);

	static int onGetVideoSizeCall(MSFilter *f, void *arg);
	static int onSetVideoSizeCall(MSFilter *f, void *arg);

	static int onGetVideoConfigurationsCall(MSFilter *f, void *arg);
	static int onSetVideoConfigurationsCall(MSFilter *f, void *arg);
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
	{ MS_FILTER_GET_BITRATE                   , EncodingFilterWrapper::onGetBitrateCall             }, \
	{ MS_FILTER_SET_BITRATE                   , EncodingFilterWrapper::onSetBitrateCall             }, \
	{ MS_FILTER_GET_FPS                       , EncodingFilterWrapper::onGetFpsCall                 }, \
	{ MS_FILTER_SET_FPS                       , EncodingFilterWrapper::onSetFpsCall                 }, \
	{ MS_FILTER_GET_VIDEO_SIZE                , EncodingFilterWrapper::onGetVideoSizeCall           }, \
	{ MS_FILTER_SET_VIDEO_SIZE                , EncodingFilterWrapper::onSetVideoSizeCall           }, \
	{ MS_FILTER_REQ_VFU                       , EncodingFilterWrapper::onRequestVfuCall             }, \
	{ MS_VIDEO_ENCODER_REQ_VFU                , EncodingFilterWrapper::onRequestVfuCall             }, \
	{ MS_VIDEO_ENCODER_NOTIFY_PLI             , EncodingFilterWrapper::onNotifyPliCall              }, \
	{ MS_VIDEO_ENCODER_NOTIFY_FIR             , EncodingFilterWrapper::onNotifyFirCall              }, \
	{ MS_VIDEO_ENCODER_NOTIFY_SLI             , EncodingFilterWrapper::onNotifySliCall              }, \
	{ MS_VIDEO_ENCODER_ENABLE_AVPF            , EncodingFilterWrapper::onEnableAvpfCall             }, \
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST , EncodingFilterWrapper::onGetVideoConfigurationsCall }, \
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION_LIST , EncodingFilterWrapper::onSetVideoConfigurationsCall }, \
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION      , EncodingFilterWrapper::onSetConfigurationCall       }, \
	{ 0                                       , nullptr                                             } \
}

#define MS_ENCODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(base_name, id, text, enc_fmt, flags) \
	MS_FILTER_WRAPPER_FILTER_DESCRIPTION_BASE(base_name, id, text, MS_FILTER_ENCODER, enc_fmt, 1, 1, flags)
