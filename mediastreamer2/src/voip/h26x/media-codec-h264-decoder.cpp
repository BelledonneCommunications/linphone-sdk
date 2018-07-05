/*
 Mediastreamer2 media-codec-h264-decoder.cpp
 Copyright (C) 2015 Belledonne Communications SARL

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

#include <ortp/b64.h>

#include "h264-nal-unpacker.h"
#include "h264-utils.h"
#include "media-codec-decoder.h"

using namespace b64;

namespace mediastreamer {

class MediaCodecH264DecoderFilterImpl: public MediaCodecDecoderFilterImpl {
public:
	MediaCodecH264DecoderFilterImpl(MSFilter *f): MediaCodecDecoderFilterImpl(f, "video/avc") {}
	~MediaCodecH264DecoderFilterImpl() {
		if (_sps) freemsg(_sps);
		if (_pps) freemsg(_pps);
	}

	void process() {
		if (_sps && _pps) {
			static_cast<H264NalUnpacker &>(*_unpacker).setOutOfBandSpsPps(_sps, _pps);
			_sps = nullptr;
			_pps = nullptr;
		}
		MediaCodecDecoderFilterImpl::process();
	}

	void addFmtp(const char *fmtp) {
		char value[256];
		if (fmtp_get_value(fmtp, "sprop-parameter-sets", value, sizeof(value))) {
			char *b64_sps = value;
			char *b64_pps = strchr(value, ',');

			if (b64_pps) {
				*b64_pps = '\0';
				++b64_pps;
				ms_message("Got sprop-parameter-sets : sps=%s , pps=%s", b64_sps, b64_pps);
				_sps = allocb(sizeof(value), 0);
				_sps->b_wptr += b64_decode(b64_sps, strlen(b64_sps), _sps->b_wptr, sizeof(value));
				_pps = allocb(sizeof(value), 0);
				_pps->b_wptr += b64_decode(b64_pps, strlen(b64_pps), _pps->b_wptr, sizeof(value));
			}
		}
	}

	static void onFilterInit(MSFilter *f) {
		f->data = new MediaCodecH264DecoderFilterImpl(f);
	}

	static void onFilterUninit(MSFilter *f) {
		delete static_cast<MediaCodecH264DecoderFilterImpl *>(f->data);
	}

	static void onFilterPreProcess(MSFilter *f) {
		static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->preprocess();
	}

	static void onFilterPostProcess(MSFilter *f) {
		static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->postprocess();
	}

	static void onFilterProcces(MSFilter *f) {
		static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->process();
	}

	static int onAddFmtpCall(MSFilter *f, void *arg) {
		const char *fmtp = static_cast<const char *>(arg);
		static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->addFmtp(fmtp);
		return 0;
	}

	static int onResetFirstImageCall(MSFilter *f, void *arg) {
		static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->resetFirstImage();
		return 0;
	}

	static int onGetVideoSizeCall(MSFilter *f, void *arg) {
		MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		*vsize = static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->getVideoSize();
		return 0;
	}

	static int onGetFpsCall(MSFilter *f, void *arg) {
		float *fps = static_cast<float *>(arg);
		*fps = static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->getFps();
		return 0;
	}

	static int onGetOutFmtCall(MSFilter *f, void *arg) {
		MSPinFormat *pinFormat = static_cast<MSPinFormat *>(arg);
		pinFormat->fmt = static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->getOutFmt();
		return 0;
	}

	static int onEnableAvpfCall(MSFilter *f, void *arg) {
		const bool_t *enable = static_cast<bool_t *>(arg);
		static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->enableAvpf(enable);
		return 0;
	}

	static int onEnableFreezeOnErrorCall(MSFilter *f, void *arg) {
		const bool_t *enable = static_cast<bool_t *>(arg);
		static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->enableFreezeOnError(enable);
		return 0;
	}

private:
	bool isKeyFrame(const MSQueue *frame) const override {
		H264FrameAnalyser analyser;
		H264FrameAnalyser::Info info = analyser.analyse(frame);
		return info.hasIdr && info.hasSps && info.hasPps;
	}

	void updateSps(mblk_t *sps) {
		if (_sps) freemsg(_sps);
		_sps = dupb(sps);
	}

	void updatePps(mblk_t *pps) {
		if (_pps) freemsg(_pps);
		if (pps) _pps = dupb(pps);
		else _pps = nullptr;
	}

	bool checkSpsChange(mblk_t *sps) {
		bool ret = false;
		if (_sps) {
			ret = (msgdsize(sps) != msgdsize(_sps)) || (memcmp(_sps->b_rptr, sps->b_rptr, msgdsize(sps)) != 0);

			if (ret) {
				ms_message("SPS changed ! %i,%i", (int)msgdsize(sps), (int)msgdsize(_sps));
				updateSps(sps);
				updatePps(nullptr);
			}
		} else {
			ms_message("Receiving first SPS");
			updateSps(sps);
		}
		return ret;
	}

	bool checkPpsChange(mblk_t *pps) {
		bool ret = false;
		if (_pps) {
			ret = (msgdsize(pps) != msgdsize(_pps)) || (memcmp(_pps->b_rptr, pps->b_rptr, msgdsize(pps)) != 0);

			if (ret) {
				ms_message("PPS changed ! %i,%i", (int)msgdsize(pps), (int)msgdsize(_pps));
				updatePps(pps);
			}
		} else {
			ms_message("Receiving first PPS");
			updatePps(pps);
		}
		return ret;
	}

	mblk_t *_sps = nullptr;
	mblk_t *_pps = nullptr;
};

}

using namespace mediastreamer;

static MSFilterMethod  mediacodec_h264_dec_methods[] = {
	{	MS_FILTER_ADD_FMTP                                 , MediaCodecH264DecoderFilterImpl::onAddFmtpCall             },
	{	MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION    , MediaCodecH264DecoderFilterImpl::onResetFirstImageCall     },
	{	MS_FILTER_GET_VIDEO_SIZE                           , MediaCodecH264DecoderFilterImpl::onGetVideoSizeCall        },
	{	MS_FILTER_GET_FPS                                  , MediaCodecH264DecoderFilterImpl::onGetFpsCall              },
	{	MS_FILTER_GET_OUTPUT_FMT                           , MediaCodecH264DecoderFilterImpl::onGetOutFmtCall           },
	{ 	MS_VIDEO_DECODER_ENABLE_AVPF                       , MediaCodecH264DecoderFilterImpl::onEnableAvpfCall          },
	{	MS_VIDEO_DECODER_FREEZE_ON_ERROR                   , MediaCodecH264DecoderFilterImpl::onEnableFreezeOnErrorCall },
	{	0                                                  , nullptr                                                    }
};


extern "C" MSFilterDesc ms_mediacodec_h264_dec_desc = {
	.id = MS_MEDIACODEC_H264_DEC_ID,
	.name = "MSMediaCodecH264Dec",
	.text = "A H264 decoder based on MediaCodec API.",
	.category = MS_FILTER_DECODER,
	.enc_fmt = "H264",
	.ninputs = 1,
	.noutputs = 1,
	.init = MediaCodecH264DecoderFilterImpl::onFilterInit,
	.preprocess = MediaCodecH264DecoderFilterImpl::onFilterPreProcess,
	.process = MediaCodecH264DecoderFilterImpl::onFilterProcces,
	.postprocess = MediaCodecH264DecoderFilterImpl::onFilterPostProcess,
	.uninit = MediaCodecH264DecoderFilterImpl::onFilterUninit,
	.methods = mediacodec_h264_dec_methods,
	.flags = MS_FILTER_IS_PUMP
};
