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

#include "h26x-utils.h"

#include "h26x-decoder-filter.h"

using namespace std;

namespace mediastreamer {

H26xDecoderFilter::H26xDecoderFilter(MSFilter *f, H26xDecoder *decoder):
	DecoderFilter(f),
	_vsize{ 0, 0 },
	_unpacker(H26xToolFactory::get(decoder->getMime()).createNalUnpacker()),
	_codec(decoder) {

	ms_average_fps_init(&_fps, " H26x decoder: FPS: %f");
}

void H26xDecoderFilter::preprocess() {
	_firstImageDecoded = false;
	if (_codec) _codec->waitForKeyFrame();
}

void H26xDecoderFilter::process() {
	bool requestPli = false;
	MSQueue frame;

	if (_codec == nullptr) {
		ms_queue_flush(getInput(0));
		return;
	}

	ms_queue_init(&frame);

	while (mblk_t *im = ms_queue_get(getInput(0))) {
		NalUnpacker::Status unpacking_ret = _unpacker->unpack(im, &frame);

		if (!unpacking_ret.frameAvailable) continue;

		if (unpacking_ret.frameCorrupted) {
			ms_warning("H26xDecoder: corrupted frame");
			requestPli = true;
			if (_freezeOnError){
				/*in freeze on error mode we must not supply a corrupted frame to the decoder. */
				ms_queue_flush(&frame);
				_codec->waitForKeyFrame();
				continue;
			}
		}
		/* 
		 * Feed the decoder implementation with the full frame.
		 * In case of feeding error (such too many buffers queued), we will request a PLI.
		 */
		if (!_codec->feed(&frame, ms_get_cur_time_ms())) requestPli = true;

		if (requestPli && _freezeOnError) {
			/* In freeze on error mode, regardless of the decoding failure cause, we must restart with a key-frame. */
			_codec->waitForKeyFrame();
		}
		
		ms_queue_flush(&frame);
	}

	mblk_t *om;
	VideoDecoder::Status status;
	while ((status = _codec->fetch(om)) != VideoDecoder::Status::noFrameAvailable) {
		if (status == VideoDecoder::decodingFailure) {
			ms_error("H26xDecoder: decoding failure");
			requestPli = true;
			continue;
		}

		MSPicture pic;
		ms_yuv_buf_init_from_mblk(&pic, om);
		_vsize.width = pic.w;
		_vsize.height = pic.h;

		if (!_firstImageDecoded) {
			ms_message("H26xDecoder: first frame decoded %ix%i", _vsize.width, _vsize.height);
			_firstImageDecoded = true;
			notify(MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
		}

		ms_average_fps_update(&_fps, getTime());
		ms_queue_put(getOutput(0), om);
	}

	if (requestPli) {
		notify(_avpfEnabled ? MS_VIDEO_DECODER_SEND_PLI : MS_VIDEO_DECODER_DECODING_ERRORS);
	}
}

void H26xDecoderFilter::postprocess() {
	_unpacker->reset();
}

void H26xDecoderFilter::resetFirstImage() {
	_firstImageDecoded = false;
}

MSVideoSize H26xDecoderFilter::getVideoSize() const {
	return _firstImageDecoded ? _vsize : MS_VIDEO_SIZE_UNKNOWN;
}

float H26xDecoderFilter::getFps() const {
	return ms_average_fps_get(&_fps);
}

const MSFmtDescriptor *H26xDecoderFilter::getOutputFmt() const {
	return ms_factory_get_video_format(getFactory(), "YUV420P", ms_video_size_make(_vsize.width, _vsize.height), 0, nullptr);
}

void H26xDecoderFilter::enableAvpf(bool enable) {
	ms_message("H26xDecoder: %s AVPF mode", enable ? "enabling" : "disabling");
	_avpfEnabled = enable;
}

void H26xDecoderFilter::enableFreezeOnError(bool enable) {
	_freezeOnError = enable;
	ms_message("H26xDecoder: freeze on error %s", _freezeOnError ? "enabled" : "disabled");
}

} // namespace mediastreamer
