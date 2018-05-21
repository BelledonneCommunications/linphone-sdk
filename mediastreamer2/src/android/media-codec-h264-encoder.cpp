/*
 Mediastreamer2 media-codec-h264-encoder.cpp
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

#include "h264-nal-packer.h"
#include "h264utils.h"
#include "media-codec-encoder.h"

namespace mediastreamer {

class MediaCodecH264EncoderFilterImpl: public MediaCodecEncoderFilterImpl {
public:

	MediaCodecH264EncoderFilterImpl(MSFilter *f): MediaCodecEncoderFilterImpl(f, new H264NalPacker()) {}
	~MediaCodecH264EncoderFilterImpl() {
		if (_sps) freemsg(_sps);
		if (_pps) freemsg(_pps);
	}

	void postprocess() override {
		setMblk(&_sps, nullptr);
		setMblk(&_pps, nullptr);
	}

	static void onFilterInit(MSFilter *f) {
		f->data = new MediaCodecH264EncoderFilterImpl(f);
	}

	static void onFilterPreprocess(MSFilter *f) {
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->preprocess();
	}

	static void onFilterPostprocess(MSFilter *f) {
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->postprocess();
	}

	static void onFilterUninit(MSFilter *f) {
		delete static_cast<MediaCodecH264EncoderFilterImpl *>(f->data);
	}

	static void onFilterProcess(MSFilter *f) {
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->process();
	}

	static int onGetBitrateCall(MSFilter *f, void *arg) {
		int *bitrate = static_cast<int *>(arg);
		*bitrate = static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->getBitrate();
		return 0;
	}

	static int onSetConfigurationCall(MSFilter *f, void *arg) {
		const MSVideoConfiguration *vconf = static_cast<MSVideoConfiguration *>(arg);
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->setVideoConfiguration(vconf);
		return 0;
	}

	static int onSetBitrateCall(MSFilter *f, void *arg) {
		int br = *static_cast<int *>(arg);
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->setBitrate(br);
		return 0;
	}

	static int onSetFpsCall(MSFilter *f, void *arg) {
		float fps = *static_cast<float *>(arg);
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->setFps(fps);
		return 0;
	}

	static int onGetFpsCall(MSFilter *f, void *arg) {
		float *fps = static_cast<float *>(arg);
		*fps = static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->getFps();
		return 0;
	}

	static int onGetVideoSizeCall(MSFilter *f, void *arg) {
		MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		*vsize = static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->getVideoSize();
		return 0;
	}

	static int onEnableAvpfCall(MSFilter *f, void *data) {
		bool_t enable = *static_cast<bool_t *>(data);
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->enableAvpf(enable);
		return 0;
	}

	static int onSetVideoSizeCall(MSFilter *f, void *arg) {
		const MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->setVideoSize(*vsize);
		return 0;
	}

	static int onNotifyPliCall(MSFilter *f, void *data) {
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->notifyPli();
		return 0;
	}

	static int onNotifyFirCall(MSFilter *f, void *data) {
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->notifyFir();
		return 0;
	}

	static int onGetVideoConfigurationsCall(MSFilter *f, void *data) {
		const MSVideoConfiguration **vconfs = static_cast<const MSVideoConfiguration **>(data);
		*vconfs = static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->getVideoConfiguratons();
		return 0;
	}

	static int onSetVideoConfigurationsCall(MSFilter *f, void *data) {
		const MSVideoConfiguration *vconfs = static_cast<const MSVideoConfiguration *>(data);
		static_cast<MediaCodecH264EncoderFilterImpl *>(f->data)->setVideoConfigurations(vconfs);
		return 0;
	}

private:
	void onFrameEncodedHook(MSQueue *frame) override {
		bool have_seen_sps_pps = false;
		mblk_t *m = ms_queue_peek_first(frame);
		switch (ms_h264_nalu_get_type(m)) {
			case MSH264NaluTypeIDR:
				if (!have_seen_sps_pps) {
					ms_message("MSMediaCodecH264Enc: seeing IDR without prior SPS/PPS, so manually adding them.");

					if (_sps && _pps) {
						ms_queue_insert(frame, m, copyb(_sps));
						ms_queue_insert(frame, m, copyb(_pps));
					} else {
						ms_error("MSMediaCodecH264Enc: SPS or PPS are not known !");
					}
				}
				break;

			case MSH264NaluTypeSPS:
				ms_message("MSMediaCodecH264Enc: seeing SPS");
				have_seen_sps_pps = true;
				setMblk(&_sps, m);
				m = ms_queue_next(&nalus, m);

				if (!ms_queue_end(frame, m) && ms_h264_nalu_get_type(m) == MSH264NaluTypePPS) {
					ms_message("MSMediaCodecH264Enc: seeing PPS");
					setMblk(&_pps, m);
				}
				break;

			case MSH264NaluTypePPS:
				ms_warning("MSMediaCodecH264Enc: unexpecting starting PPS");
				break;
			default:
				break;
		}
	}

	static void setMblk(mblk_t **packet, mblk_t *newone) {
		if (newone) newone = copyb(newone);
		if (*packet) freemsg(*packet);
		*packet = newone;
	}

	mblk_t *_sps = nullptr, *_pps = nullptr; /*lastly generated SPS, PPS, in case we need to repeat them*/
};

} // mamespace mediastreamer

using namespace mediastreamer;

static MSFilterMethod  mediacodec_h264_enc_methods[] = {
	{ MS_FILTER_SET_FPS                       , MediaCodecH264EncoderFilterImpl::onSetFpsCall                 },
	{ MS_FILTER_SET_BITRATE                   , MediaCodecH264EncoderFilterImpl::onSetBitrateCall             },
	{ MS_FILTER_GET_BITRATE                   , MediaCodecH264EncoderFilterImpl::onGetBitrateCall             },
	{ MS_FILTER_GET_FPS                       , MediaCodecH264EncoderFilterImpl::onGetFpsCall                 },
	{ MS_FILTER_GET_VIDEO_SIZE                , MediaCodecH264EncoderFilterImpl::onGetVideoSizeCall           },
	{ MS_VIDEO_ENCODER_NOTIFY_PLI             , MediaCodecH264EncoderFilterImpl::onNotifyPliCall              },
	{ MS_VIDEO_ENCODER_NOTIFY_FIR             , MediaCodecH264EncoderFilterImpl::onNotifyFirCall              },
	{ MS_FILTER_SET_VIDEO_SIZE                , MediaCodecH264EncoderFilterImpl::onSetVideoSizeCall           },
	{ MS_VIDEO_ENCODER_ENABLE_AVPF            , MediaCodecH264EncoderFilterImpl::onEnableAvpfCall             },
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST , MediaCodecH264EncoderFilterImpl::onGetVideoConfigurationsCall },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION_LIST , MediaCodecH264EncoderFilterImpl::onSetVideoConfigurationsCall },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION      , MediaCodecH264EncoderFilterImpl::onSetConfigurationCall       },
	{ 0                                       , nullptr                                                       }
};


extern "C" MSFilterDesc ms_mediacodec_h264_enc_desc = {
	.id = MS_MEDIACODEC_H264_ENC_ID,
	.name = "MSMediaCodecH264Enc",
	.text = "A H264 encoder based on MediaCodec API.",
	.category = MS_FILTER_ENCODER,
	.enc_fmt = "H264",
	.ninputs = 1,
	.noutputs = 1,
	.init = MediaCodecH264EncoderFilterImpl::onFilterInit,
	.preprocess = MediaCodecH264EncoderFilterImpl::onFilterPreprocess,
	.process = MediaCodecH264EncoderFilterImpl::onFilterProcess,
	.postprocess = MediaCodecH264EncoderFilterImpl::onFilterPostprocess,
	.uninit = MediaCodecH264EncoderFilterImpl::onFilterUninit,
	.methods = mediacodec_h264_enc_methods,
	.flags = MS_FILTER_IS_PUMP
};
