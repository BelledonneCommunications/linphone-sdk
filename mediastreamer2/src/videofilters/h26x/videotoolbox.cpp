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

#include <bctoolbox/defs.h>

#include "filter-wrapper/decoding-filter-wrapper.h"
#include "filter-wrapper/encoding-filter-wrapper.h"
#include "h26x-decoder-filter.h"
#include "h26x-encoder-filter.h"
#include "videotoolbox-decoder.h"
#include "videotoolbox-encoder.h"
#include "videotoolbox-utils.h"

using namespace std;

namespace mediastreamer {

const MSVideoConfiguration vth264enc_video_confs[] = {
/*
 * Formats above 720P are disabled. Indeed, there are not supported in baseline profile of H264, and it was observed
 * that when we ask a MediaCodec to output a 1080P stream with baseline profile, we get interoperability issues:
 * the remote decoder decodes it improperly, even on iOS.
 * TODO: enable use of higher profiles to use formats above 720P.
 */
#if 0
	MS_VIDEO_CONF(1536000,  3000000, SXGA_MINUS, 30, 2),
#endif
    MS_VIDEO_CONF(1024000, 2048000, 720P, 30, 2), MS_VIDEO_CONF(850000, 2048000, XGA, 30, 2),
    MS_VIDEO_CONF(750000, 1500000, SVGA, 30, 2),  MS_VIDEO_CONF(600000, 2000000, VGA, 30, 2),
    MS_VIDEO_CONF(400000, 800000, VGA, 20, 1),    MS_VIDEO_CONF(200000, 350000, CIF, 18, 1),
    MS_VIDEO_CONF(150000, 200000, QVGA, 15, 1),   MS_VIDEO_CONF(100000, 150000, QVGA, 10, 1),
    MS_VIDEO_CONF(64000, 100000, QCIF, 12, 1),    MS_VIDEO_CONF(0, 64000, QCIF, 5, 1)};

class VideoToolboxH264EncoderFilterImpl : public H26xEncoderFilter {
public:
	VideoToolboxH264EncoderFilterImpl(MSFilter *f)
	    : H26xEncoderFilter(f,
	                        new VideoToolboxEncoder("video/avc", ms_factory_get_payload_max_size(f->factory)),
	                        vth264enc_video_confs) {
	}
};

class VideoToolboxH264DecoderFilterImpl : public H26xDecoderFilter {
public:
	VideoToolboxH264DecoderFilterImpl(MSFilter *f) : H26xDecoderFilter(f, new VideoToolboxDecoder("video/avc")) {
	}
};

class VideoToolboxH265EncoderFilterImpl : public H26xEncoderFilter {
public:
	VideoToolboxH265EncoderFilterImpl(MSFilter *f)
	    : H26xEncoderFilter(f,
	                        new VideoToolboxEncoder("video/hevc", ms_factory_get_payload_max_size(f->factory)),
	                        vth264enc_video_confs) {
	}
};

class VideoToolboxH265DecoderFilterImpl : public H26xDecoderFilter {
public:
	VideoToolboxH265DecoderFilterImpl(MSFilter *f) : H26xDecoderFilter(f, new VideoToolboxDecoder("video/hevc")) {
	}
};

} // namespace mediastreamer

using namespace mediastreamer;

MS_ENCODING_FILTER_WRAPPER_METHODS_DECLARATION(VideoToolboxH264Encoder);
MS_ENCODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(VideoToolboxH264Encoder,
                                                   MS_VT_H264_ENC_ID,
                                                   "H264 hardware encoder for iOS and MacOSX",
                                                   "H264",
                                                   MS_FILTER_IS_PUMP | MS_FILTER_IS_HW_ACCELERATED);

MS_DECODING_FILTER_WRAPPER_METHODS_DECLARATION(VideoToolboxH264Decoder);
MS_DECODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(VideoToolboxH264Decoder,
                                                   MS_VT_H264_DEC_ID,
                                                   "H264 hardware decoder for iOS and MacOSX",
                                                   "H264",
                                                   MS_FILTER_IS_PUMP | MS_FILTER_IS_HW_ACCELERATED);

MS_ENCODING_FILTER_WRAPPER_METHODS_DECLARATION(VideoToolboxH265Encoder);
MS_ENCODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(VideoToolboxH265Encoder,
                                                   MS_VT_H265_ENC_ID,
                                                   "H265 hardware encoder for iOS and MacOSX",
                                                   "H265",
                                                   MS_FILTER_IS_PUMP | MS_FILTER_IS_HW_ACCELERATED);

MS_DECODING_FILTER_WRAPPER_METHODS_DECLARATION(VideoToolboxH265Decoder);
MS_DECODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(VideoToolboxH265Decoder,
                                                   MS_VT_H265_DEC_ID,
                                                   "H265 hardware decoder for iOS and MacOSX",
                                                   "H265",
                                                   MS_FILTER_IS_PUMP | MS_FILTER_IS_HW_ACCELERATED);

extern "C" void _register_videotoolbox_if_supported(BCTBX_UNUSED(MSFactory *factory)) {
#if TARGET_OS_SIMULATOR
	(void)factory;
	ms_message("VideoToolbox H264 codec is not supported on simulators");
#else
	if (kCFCoreFoundationVersionNumber >= 744.00) { // MacOS >= 10.8 or iOS >= 8.0
		unique_ptr<VideoToolboxUtilities> codecInfo(VideoToolboxUtilities::create("video/avc"));
		if (codecInfo->encoderIsAvailable()) {
			ms_message("Registering VideoToolbox H264 codec");
			ms_factory_register_filter(factory, &ms_VideoToolboxH264Encoder_desc);
			ms_factory_register_filter(factory, &ms_VideoToolboxH264Decoder_desc);
		} else {
			ms_message("No H264 encoder found on this device");
		}
	} else {
		ms_message("Cannot register VideoToolbox H264 codec. That "
		           "requires iOS 8 or MacOS 10.8");
	}

	if (kCFCoreFoundationVersionNumber >= 1400) { // MacOS >= 10.13 or iOS >= 11.0
		unique_ptr<VideoToolboxUtilities> codecInfo(VideoToolboxUtilities::create("video/hevc"));
		if (codecInfo->encoderIsAvailable()) {
			ms_message("Registering VideoToolbox H265 codec");
			ms_factory_register_filter(factory, &ms_VideoToolboxH265Encoder_desc);
			ms_factory_register_filter(factory, &ms_VideoToolboxH265Decoder_desc);
		} else {
			ms_message("No H265 encoder found on this device");
		}
	} else {
		ms_message("Cannot register VideoToolbox H265 codec. That "
		           "requires iOS 11.0 or MacOS 10.13");
	}

#endif // !TARGET_OS_SIMULATOR
}
