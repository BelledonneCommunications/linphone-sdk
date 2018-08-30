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

#include "filter-wrapper/encoding-filter-wrapper.h"
#include "h26x/h26x-encoder-impl.h"
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

class MediaCodecH265EncoderFilterImpl: public H26xEncoderFilterImpl {
public:
	MediaCodecH265EncoderFilterImpl(MSFilter *f): H26xEncoderFilterImpl(
		f,
		new MediaCodecH265Encoder(),
		new H265NalPacker(f->factory),
		_media_codec_h265_conf_list) {}
};

}

using namespace mediastreamer;

MS_ENCODING_FILTER_WRAPPER_METHODS_DECLARATION(MediaCodecH265Encoder);
MS_ENCODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(MediaCodecH265Encoder, MS_MEDIACODEC_H265_ENC_ID, "A H265 encoder based on MediaCodec API.", "H265", MS_FILTER_IS_PUMP);
