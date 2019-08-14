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

#include "filter-wrapper/decoding-filter-wrapper.h"
#include "h26x-decoder-filter.h"
#include "media-codec-h265-decoder.h"

namespace mediastreamer {

class MediaCodecH265DecoderFilterImpl: public H26xDecoderFilter {
public:
	MediaCodecH265DecoderFilterImpl(MSFilter *f): H26xDecoderFilter(f, new MediaCodecH265Decoder()) {}
};

}

using namespace mediastreamer;

MS_DECODING_FILTER_WRAPPER_METHODS_DECLARATION(MediaCodecH265Decoder);
MS_DECODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(MediaCodecH265Decoder, MS_MEDIACODEC_H265_DEC_ID, "A H265 decoder based on MediaCodec API.", "H265", MS_FILTER_IS_PUMP);
