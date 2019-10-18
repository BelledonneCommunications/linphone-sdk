/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "filter-wrapper/encoding-filter-wrapper.h"
#include "h26x-encoder-filter.h"
#include "media-codec-encoder.h"

#define MS_MEDIACODECH265_CONF(required_bitrate, bitrate_limit, resolution, fps, ncpus) \
{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H }, fps, ncpus, nullptr }

static const MSVideoConfiguration _media_codec_h265_conf_list[] = {
	/*
	 * Formats above 720P are disabled. Indeed, there are not supported in baseline profile of H264, and it was observed
	 * that when we ask a MediaCodec to output a 1080P stream with baseline profile, we get interoperability issues:
	 * the remote decoder decodes it improperly, even on iOS.
	 * TODO: enable use of higher profiles to use formats above 720P.
	 */
#if 0
	MS_MEDIACODECH265_CONF(2048000, 5000000,       UXGA, 25,  2),
	MS_MEDIACODECH265_CONF(1500000, 3000000, SXGA_MINUS, 25,  2),
#endif
	MS_MEDIACODECH265_CONF(1024000, 2048000,       720P, 30,  2),
	MS_MEDIACODECH265_CONF( 850000, 2048000,        XGA, 25,  2),
	MS_MEDIACODECH265_CONF( 750000, 1500000,       SVGA, 30,  2),
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

class MediaCodecH265EncoderFilterImpl: public H26xEncoderFilter {
public:
	MediaCodecH265EncoderFilterImpl(MSFilter *f): H26xEncoderFilter(f, new MediaCodecH265Encoder(), _media_codec_h265_conf_list) {
		SoundDeviceDescription *info = ms_devices_info_get_sound_device_description(f->factory->devices_info);
		auto &encoder = static_cast<MediaCodecEncoder &>(*_encoder);
		encoder.enableOutbufferDequeueLimit(!!(info->flags & DEVICE_MCH265_LIMIT_DEQUEUE_OF_OUTPUT_BUFFERS));
	}
};

}

using namespace mediastreamer;

MS_ENCODING_FILTER_WRAPPER_METHODS_DECLARATION(MediaCodecH265Encoder);
MS_ENCODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(MediaCodecH265Encoder, MS_MEDIACODEC_H265_ENC_ID, "A H265 encoder based on MediaCodec API.", "H265", MS_FILTER_IS_PUMP);
