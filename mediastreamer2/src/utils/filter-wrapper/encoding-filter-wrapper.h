/*
 M ediastreamer2 decoding-filter-*wrapper.h
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
	static int onGetBitrateCall(MSFilter *f, void *arg) {
		int *bitrate = static_cast<int *>(arg);
		*bitrate = static_cast<EncodingFilterImpl *>(f->data)->getBitrate();
		return 0;
	}

	static int onSetConfigurationCall(MSFilter *f, void *arg) {
		const MSVideoConfiguration *vconf = static_cast<MSVideoConfiguration *>(arg);
		static_cast<EncodingFilterImpl *>(f->data)->setVideoConfiguration(vconf);
		return 0;
	}

	static int onSetBitrateCall(MSFilter *f, void *arg) {
		int br = *static_cast<int *>(arg);
		static_cast<EncodingFilterImpl *>(f->data)->setBitrate(br);
		return 0;
	}

	static int onSetFpsCall(MSFilter *f, void *arg) {
		float fps = *static_cast<float *>(arg);
		static_cast<EncodingFilterImpl *>(f->data)->setFps(fps);
		return 0;
	}

	static int onGetFpsCall(MSFilter *f, void *arg) {
		float *fps = static_cast<float *>(arg);
		*fps = static_cast<EncodingFilterImpl *>(f->data)->getFps();
		return 0;
	}

	static int onGetVideoSizeCall(MSFilter *f, void *arg) {
		MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		*vsize = static_cast<EncodingFilterImpl *>(f->data)->getVideoSize();
		return 0;
	}

	static int onEnableAvpfCall(MSFilter *f, void *data) {
		bool_t enable = *static_cast<bool_t *>(data);
		static_cast<EncodingFilterImpl *>(f->data)->enableAvpf(enable);
		return 0;
	}

	static int onSetVideoSizeCall(MSFilter *f, void *arg) {
		const MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		static_cast<EncodingFilterImpl *>(f->data)->setVideoSize(*vsize);
		return 0;
	}

	static int onNotifyPliCall(MSFilter *f, void *data) {
		static_cast<EncodingFilterImpl *>(f->data)->notifyPli();
		return 0;
	}

	static int onNotifyFirCall(MSFilter *f, void *data) {
		static_cast<EncodingFilterImpl *>(f->data)->notifyFir();
		return 0;
	}

	static int onGetVideoConfigurationsCall(MSFilter *f, void *data) {
		const MSVideoConfiguration **vconfs = static_cast<const MSVideoConfiguration **>(data);
		*vconfs = static_cast<EncodingFilterImpl *>(f->data)->getVideoConfiguratons();
		return 0;
	}

	static int onSetVideoConfigurationsCall(MSFilter *f, void *data) {
		const MSVideoConfiguration * const *vconfs = static_cast<const MSVideoConfiguration * const *>(data);
		static_cast<EncodingFilterImpl *>(f->data)->setVideoConfigurations(*vconfs);
		return 0;
	}
};

} // namespace mediastreamer

#define MS_ENCODING_FILTER_WRAPPER_METHODS_DECLARATION(base_name) \
static MSFilterMethod  MS_FILTER_WRAPPER_METHODS_NAME(base_name)[] = { \
	{ MS_FILTER_SET_FPS                       , EncodingFilterWrapper::onSetFpsCall                 }, \
	{ MS_FILTER_SET_BITRATE                   , EncodingFilterWrapper::onSetBitrateCall             }, \
	{ MS_FILTER_GET_BITRATE                   , EncodingFilterWrapper::onGetBitrateCall             }, \
	{ MS_FILTER_GET_FPS                       , EncodingFilterWrapper::onGetFpsCall                 }, \
	{ MS_FILTER_GET_VIDEO_SIZE                , EncodingFilterWrapper::onGetVideoSizeCall           }, \
	{ MS_VIDEO_ENCODER_NOTIFY_PLI             , EncodingFilterWrapper::onNotifyPliCall              }, \
	{ MS_VIDEO_ENCODER_NOTIFY_FIR             , EncodingFilterWrapper::onNotifyFirCall              }, \
	{ MS_FILTER_SET_VIDEO_SIZE                , EncodingFilterWrapper::onSetVideoSizeCall           }, \
	{ MS_VIDEO_ENCODER_ENABLE_AVPF            , EncodingFilterWrapper::onEnableAvpfCall             }, \
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST , EncodingFilterWrapper::onGetVideoConfigurationsCall }, \
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION_LIST , EncodingFilterWrapper::onSetVideoConfigurationsCall }, \
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION      , EncodingFilterWrapper::onSetConfigurationCall       }, \
	{ 0                                       , nullptr                                             } \
}

#define MS_ENCODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(base_name, id, text, enc_fmt, flags) \
	MS_FILTER_WRAPPER_FILTER_DESCRIPTION_BASE(base_name, id, text, MS_FILTER_ENCODER, enc_fmt, 1, 1, flags)
