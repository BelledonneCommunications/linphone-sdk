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

#include <cstring>

#include <ortp/b64.h>

#include "filter-wrapper/decoding-filter-wrapper.h"
#include "h264-nal-unpacker.h"
#include "h264-utils.h"
#include "media-codec-decoder.h"
#include "media-codec-h264-decoder.h"

using namespace b64;

namespace mediastreamer {

MediaCodecH264Decoder::~MediaCodecH264Decoder() {
	if (_lastSps) freemsg(_lastSps);
}

bool MediaCodecH264Decoder::setParameterSets(MSQueue *parameterSet, uint64_t timestamp) {
	for (mblk_t *m = ms_queue_peek_first(parameterSet); !ms_queue_end(parameterSet, m); m = ms_queue_next(parameterSet, m)) {
		MSH264NaluType type = ms_h264_nalu_get_type(m);
		if (type == MSH264NaluTypeSPS && isNewPps(m)) {
			int32_t curWidth, curHeight;
			AMediaFormat_getInt32(_format, "width", &curWidth);
			AMediaFormat_getInt32(_format, "height", &curHeight);
			MSVideoSize vsize = ms_h264_sps_get_video_size(m);
			if (vsize.width != curWidth || vsize.height != curHeight) {
				ms_message("MediaCodecDecoder: restarting decoder because the video size has changed (%dx%d->%dx%d)",
					curWidth,
					curHeight,
					vsize.width,
					vsize.height
				);
				AMediaFormat_setInt32(_format, "width", vsize.width);
				AMediaFormat_setInt32(_format, "height", vsize.height);
				stopImpl();
				startImpl();
			}
		}
	}
	return MediaCodecDecoder::setParameterSets(parameterSet, timestamp);
}

bool MediaCodecH264Decoder::isNewPps(mblk_t *sps) {
	if (_lastSps == nullptr) {
		_lastSps = dupmsg(sps);
		return true;
	}
	const size_t spsSize = size_t(sps->b_wptr - sps->b_rptr);
	const size_t lastSpsSize = size_t(_lastSps->b_wptr - _lastSps->b_rptr);
	if (spsSize != lastSpsSize || memcmp(_lastSps->b_rptr, sps->b_rptr, spsSize) != 0) {
		freemsg(_lastSps);
		_lastSps = dupmsg(sps);
		return true;
	}
	return false;
}

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

private:
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
				ms_message("MediaCodecDecoder: SPS changed ! %i,%i", (int)msgdsize(sps), (int)msgdsize(_sps));
				updateSps(sps);
				updatePps(nullptr);
			}
		} else {
			ms_message("MediaCodecDecoder: receiving first SPS");
			updateSps(sps);
		}
		return ret;
	}

	bool checkPpsChange(mblk_t *pps) {
		bool ret = false;
		if (_pps) {
			ret = (msgdsize(pps) != msgdsize(_pps)) || (memcmp(_pps->b_rptr, pps->b_rptr, msgdsize(pps)) != 0);

			if (ret) {
				ms_message("MediaCodecDecoder: PPS changed ! %i,%i", (int)msgdsize(pps), (int)msgdsize(_pps));
				updatePps(pps);
			}
		} else {
			ms_message("MediaCodecDecoder: receiving first PPS");
			updatePps(pps);
		}
		return ret;
	}

	mblk_t *_sps = nullptr;
	mblk_t *_pps = nullptr;
};

}

using namespace mediastreamer;

MS_DECODING_FILTER_WRAPPER_METHODS_DECLARATION(MediaCodecH264Decoder);
MS_DECODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(MediaCodecH264Decoder, MS_MEDIACODEC_H264_DEC_ID, "A H264 decoder based on MediaCodec API.", "H264", MS_FILTER_IS_PUMP);
