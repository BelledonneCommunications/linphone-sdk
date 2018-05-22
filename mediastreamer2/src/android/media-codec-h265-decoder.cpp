/*
 Mediastreamer2 media-codec-h265-decoder.cpp
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

#include "h265-nal-unpacker.h"
#include "media-codec-decoder.h"

namespace mediastreamer {

class MediaCodecH265DecoderFilterImpl: public MediaCodecDecoderFilterImpl {
public:
	MediaCodecH265DecoderFilterImpl(MSFilter *f): MediaCodecDecoderFilterImpl(f, "video/hevc", new H265NalUnpacker()) {}

	static void onFilterInit(MSFilter *f) {
		f->data = new MediaCodecH265DecoderFilterImpl(f);
	}

	static void onFilterUninit(MSFilter *f) {
		delete static_cast<MediaCodecH265DecoderFilterImpl *>(f->data);
	}

	static void onFilterPreProcess(MSFilter *f) {
		static_cast<MediaCodecH265DecoderFilterImpl *>(f->data)->preprocess();
	}

	static void onFilterPostProcess(MSFilter *f) {
		static_cast<MediaCodecH265DecoderFilterImpl *>(f->data)->postprocess();
	}

	static void onFilterProcces(MSFilter *f) {
		static_cast<MediaCodecH265DecoderFilterImpl *>(f->data)->process();
	}

	static int onAddFmtpCall(MSFilter *f, void *arg) {
		const char *fmtp = static_cast<const char *>(arg);
		static_cast<MediaCodecH265DecoderFilterImpl *>(f->data)->addFmtp(fmtp);
		return 0;
	}

	static int onResetFirstImageCall(MSFilter *f, void *arg) {
		static_cast<MediaCodecH265DecoderFilterImpl *>(f->data)->resetFirstImage();
		return 0;
	}

	static int onGetVideoSizeCall(MSFilter *f, void *arg) {
		MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		*vsize = static_cast<MediaCodecH265DecoderFilterImpl *>(f->data)->getVideoSize();
		return 0;
	}

	static int onGetFpsCall(MSFilter *f, void *arg) {
		float *fps = static_cast<float *>(arg);
		*fps = static_cast<MediaCodecH265DecoderFilterImpl *>(f->data)->getFps();
		return 0;
	}

	static int onGetOutFmtCall(MSFilter *f, void *arg) {
		MSPinFormat *pinFormat = static_cast<MSPinFormat *>(arg);
		pinFormat->fmt = static_cast<MediaCodecH265DecoderFilterImpl *>(f->data)->getOutFmt();
		return 0;
	}

	static int onEnableAvpfCall(MSFilter *f, void *arg) {
		const bool_t *enable = static_cast<bool_t *>(arg);
		static_cast<MediaCodecH265DecoderFilterImpl *>(f->data)->enableAvpf(enable);
		return 0;
	}

	static int onEnableFreezeOnErrorCall(MSFilter *f, void *arg) {
		const bool_t *enable = static_cast<bool_t *>(arg);
		static_cast<MediaCodecH265DecoderFilterImpl *>(f->data)->enableFreezeOnError(enable);
		return 0;
	}
};

}

using namespace mediastreamer;

static MSFilterMethod  mediacodec_h265_dec_methods[] = {
	{	MS_FILTER_ADD_FMTP                                 , MediaCodecH265DecoderFilterImpl::onAddFmtpCall              },
	{	MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION    , MediaCodecH265DecoderFilterImpl::onResetFirstImageCall      },
	{	MS_FILTER_GET_VIDEO_SIZE                           , MediaCodecH265DecoderFilterImpl::onGetVideoSizeCall         },
	{	MS_FILTER_GET_FPS                                  , MediaCodecH265DecoderFilterImpl::onGetFpsCall               },
	{	MS_FILTER_GET_OUTPUT_FMT                           , MediaCodecH265DecoderFilterImpl::onGetOutFmtCall            },
	{ 	MS_VIDEO_DECODER_ENABLE_AVPF                       , MediaCodecH265DecoderFilterImpl::onEnableAvpfCall           },
	{	MS_VIDEO_DECODER_FREEZE_ON_ERROR                   , MediaCodecH265DecoderFilterImpl::onEnableFreezeOnErrorCall  },
	{	0                                                  , nullptr                                                     }
};


extern "C" MSFilterDesc ms_mediacodec_h265_dec_desc = {
	.id = MS_MEDIACODEC_H265_DEC_ID,
	.name = "MSMediaCodecH265Dec",
	.text = "A H265 decoder based on MediaCodec API.",
	.category = MS_FILTER_DECODER,
	.enc_fmt = "H265",
	.ninputs = 1,
	.noutputs = 1,
	.init = MediaCodecH265DecoderFilterImpl::onFilterInit,
	.preprocess = MediaCodecH265DecoderFilterImpl::onFilterPreProcess,
	.process = MediaCodecH265DecoderFilterImpl::onFilterProcces,
	.postprocess = MediaCodecH265DecoderFilterImpl::onFilterPostProcess,
	.uninit = MediaCodecH265DecoderFilterImpl::onFilterUninit,
	.methods = mediacodec_h265_dec_methods,
	.flags = MS_FILTER_IS_PUMP
};
