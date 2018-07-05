/*
 Mediastreamer2 media-codec-h265-encoder.cpp
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

#include "h265-nal-packer.h"
#include "media-codec-encoder.h"

#define MS_MEDIACODECH265_CONF(required_bitrate, bitrate_limit, resolution, fps, ncpus) \
{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H }, fps, ncpus, nullptr }

static const MSVideoConfiguration _media_codec_h265_conf_list[] = {
	MS_MEDIACODECH265_CONF(2048000, 1000000,       UXGA, 25,  2),
	MS_MEDIACODECH265_CONF(1024000, 5000000, SXGA_MINUS, 25,  2),
	MS_MEDIACODECH265_CONF(1024000, 5000000,       720P, 30,  2),
	MS_MEDIACODECH265_CONF( 750000, 2048000,        XGA, 25,  2),
	MS_MEDIACODECH265_CONF( 500000, 1024000,       SVGA, 15,  2),
	MS_MEDIACODECH265_CONF( 600000, 3000000,        VGA, 30,  2),
	MS_MEDIACODECH265_CONF( 400000,  800000,        VGA, 15,  2),
	MS_MEDIACODECH265_CONF( 128000,  512000,        CIF, 15,  1),
	MS_MEDIACODECH265_CONF( 100000,  380000,       QVGA, 15,  1),
	MS_MEDIACODECH265_CONF(      0,  170000,       QCIF, 10,  1),
};

namespace mediastreamer {

class MediaCodecH265Encoder: public MediaCodecEncoder {
public:
	MediaCodecH265Encoder(): MediaCodecEncoder("video/hevc") {}
};

class MediaCodecH265EncoderFilterImpl: public MediaCodecEncoderFilterImpl {
public:
	MediaCodecH265EncoderFilterImpl(MSFilter *f): MediaCodecEncoderFilterImpl(
		f,
		new MediaCodecH265Encoder(),
		new H265NalPacker(f->factory),
		_media_codec_h265_conf_list) {}

	static void onFilterInit(MSFilter *f) {
		f->data = new MediaCodecH265EncoderFilterImpl(f);
	}

	static void onFilterPreprocess(MSFilter *f) {
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->preprocess();
	}

	static void onFilterPostprocess(MSFilter *f) {
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->postprocess();
	}

	static void onFilterUninit(MSFilter *f) {
		delete static_cast<MediaCodecH265EncoderFilterImpl *>(f->data);
	}

	static void onFilterProcess(MSFilter *f) {
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->process();
	}

	static int onGetBitrateCall(MSFilter *f, void *arg) {
		int *bitrate = static_cast<int *>(arg);
		*bitrate = static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->getBitrate();
		return 0;
	}

	static int onSetConfigurationCall(MSFilter *f, void *arg) {
		const MSVideoConfiguration *vconf = static_cast<MSVideoConfiguration *>(arg);
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->setVideoConfiguration(vconf);
		return 0;
	}

	static int onSetBitrateCall(MSFilter *f, void *arg) {
		int br = *static_cast<int *>(arg);
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->setBitrate(br);
		return 0;
	}

	static int onSetFpsCall(MSFilter *f, void *arg) {
		float fps = *static_cast<float *>(arg);
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->setFps(fps);
		return 0;
	}

	static int onGetFpsCall(MSFilter *f, void *arg) {
		float *fps = static_cast<float *>(arg);
		*fps = static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->getFps();
		return 0;
	}

	static int onGetVideoSizeCall(MSFilter *f, void *arg) {
		MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		*vsize = static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->getVideoSize();
		return 0;
	}

	static int onEnableAvpfCall(MSFilter *f, void *data) {
		bool_t enable = *static_cast<bool_t *>(data);
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->enableAvpf(enable);
		return 0;
	}

	static int onSetVideoSizeCall(MSFilter *f, void *arg) {
		const MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->setVideoSize(*vsize);
		return 0;
	}

	static int onNotifyPliCall(MSFilter *f, void *data) {
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->notifyPli();
		return 0;
	}

	static int onNotifyFirCall(MSFilter *f, void *data) {
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->notifyFir();
		return 0;
	}

	static int onGetVideoConfigurationsCall(MSFilter *f, void *data) {
		const MSVideoConfiguration **vconfs = static_cast<const MSVideoConfiguration **>(data);
		*vconfs = static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->getVideoConfiguratons();
		return 0;
	}

	static int onSetVideoConfigurationsCall(MSFilter *f, void *data) {
		const MSVideoConfiguration * const *vconfs = static_cast<const MSVideoConfiguration * const *>(data);
		static_cast<MediaCodecH265EncoderFilterImpl *>(f->data)->setVideoConfigurations(*vconfs);
		return 0;
	}
};

}

using namespace mediastreamer;

static MSFilterMethod  mediacodec_h265_enc_methods[] = {
	{ MS_FILTER_SET_FPS                       , MediaCodecH265EncoderFilterImpl::onSetFpsCall                 },
	{ MS_FILTER_SET_BITRATE                   , MediaCodecH265EncoderFilterImpl::onSetBitrateCall             },
	{ MS_FILTER_GET_BITRATE                   , MediaCodecH265EncoderFilterImpl::onGetBitrateCall             },
	{ MS_FILTER_GET_FPS                       , MediaCodecH265EncoderFilterImpl::onGetFpsCall                 },
	{ MS_FILTER_GET_VIDEO_SIZE                , MediaCodecH265EncoderFilterImpl::onGetVideoSizeCall           },
	{ MS_VIDEO_ENCODER_NOTIFY_PLI             , MediaCodecH265EncoderFilterImpl::onNotifyPliCall              },
	{ MS_VIDEO_ENCODER_NOTIFY_FIR             , MediaCodecH265EncoderFilterImpl::onNotifyFirCall              },
	{ MS_FILTER_SET_VIDEO_SIZE                , MediaCodecH265EncoderFilterImpl::onSetVideoSizeCall           },
	{ MS_VIDEO_ENCODER_ENABLE_AVPF            , MediaCodecH265EncoderFilterImpl::onEnableAvpfCall             },
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST , MediaCodecH265EncoderFilterImpl::onGetVideoConfigurationsCall },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION_LIST , MediaCodecH265EncoderFilterImpl::onSetVideoConfigurationsCall },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION      , MediaCodecH265EncoderFilterImpl::onSetConfigurationCall       },
	{ 0                                       , nullptr                                                       }
};


extern "C" MSFilterDesc ms_mediacodec_h265_enc_desc = {
	.id = MS_MEDIACODEC_H265_ENC_ID,
	.name = "MSMediaCodecH265Enc",
	.text = "A H265 encoder based on MediaCodec API.",
	.category = MS_FILTER_ENCODER,
	.enc_fmt = "H265",
	.ninputs = 1,
	.noutputs = 1,
	.init = MediaCodecH265EncoderFilterImpl::onFilterInit,
	.preprocess = MediaCodecH265EncoderFilterImpl::onFilterPreprocess,
	.process = MediaCodecH265EncoderFilterImpl::onFilterProcess,
	.postprocess = MediaCodecH265EncoderFilterImpl::onFilterPostprocess,
	.uninit = MediaCodecH265EncoderFilterImpl::onFilterUninit,
	.methods = mediacodec_h265_enc_methods,
	.flags = MS_FILTER_IS_PUMP
};
