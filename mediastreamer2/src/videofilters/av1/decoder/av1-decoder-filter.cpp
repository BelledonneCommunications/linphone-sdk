/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
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

#include "av1-decoder-filter.h"

#include "filter-wrapper/decoding-filter-wrapper.h"

using namespace std;

namespace mediastreamer {

Av1DecoderFilter::Av1DecoderFilter(MSFilter *f, Av1Decoder *codec)
    : DecoderFilter(f), mVsize{0, 0}, mCodec(codec), mUnpacker() {
	ms_average_fps_init(&mFps, " Av1DecoderFilter: FPS: %f");
}

void Av1DecoderFilter::preprocess() {
}

void Av1DecoderFilter::process() {
	bool requestPli = false;

	if (mCodec == nullptr) {
		ms_queue_flush(getInput(0));
		return;
	}

	MSQueue frame;
	ms_queue_init(&frame);

	while (mblk_t *im = ms_queue_get(getInput(0))) {
		ObuUnpacker::Status ret = mUnpacker.unpack(im, &frame);

		if (ret == ObuUnpacker::Status::NoFrame) continue;
		else if (ret == ObuUnpacker::Status::FrameCorrupted) {
			ms_warning("Av1DecoderFilter: corrupted frame.");
			requestPli = true;
			if (mFreezeOnError) {
				ms_queue_flush(&frame);
				mCodec->waitForKeyFrame();
				continue;
			}
		}

		if (!mCodec->feed(&frame, ms_get_cur_time_ms())) requestPli = true;

		if (requestPli && mFreezeOnError) {
			/* In freeze on error mode, regardless of the decoding failure cause, we must restart with a key-frame. */
			mCodec->waitForKeyFrame();
		}

		ms_queue_flush(&frame);
	}

	mblk_t *om;
	VideoDecoder::Status status;
	while ((status = mCodec->fetch(om)) != VideoDecoder::Status::NoFrameAvailable) {
		if (status == VideoDecoder::DecodingFailure) {
			ms_error("Av1DecoderFilter: decoding failure");
			requestPli = true;
			continue;
		}
		if (!om) {
			ms_warning("Av1DecoderFilter: no frame.");
			break;
		}

		if (!mFirstImageDecoded) {
			MSPicture pic;

			ms_yuv_buf_init_from_mblk(&pic, om);
			mVsize.width = pic.w;
			mVsize.height = pic.h;

			ms_message("Av1DecoderFilter: first frame decoded %ix%i", mVsize.width, mVsize.height);

			mFirstImageDecoded = true;
			notify(MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
		}

		ms_average_fps_update(&mFps, getTime());
		ms_queue_put(getOutput(0), om);
	}

	if (requestPli) {
		notify(mAvpfEnabled ? MS_VIDEO_DECODER_SEND_PLI : MS_VIDEO_DECODER_DECODING_ERRORS);
	}
}

void Av1DecoderFilter::postprocess() {
}

void Av1DecoderFilter::resetFirstImage() {
	mFirstImageDecoded = false;
}

MSVideoSize Av1DecoderFilter::getVideoSize() const {
	return mFirstImageDecoded ? mVsize : MS_VIDEO_SIZE_UNKNOWN;
}

float Av1DecoderFilter::getFps() const {
	return ms_average_fps_get(&mFps);
}

const MSFmtDescriptor *Av1DecoderFilter::getOutputFmt() const {
	return ms_factory_get_video_format(getFactory(), "YUV420P", ms_video_size_make(mVsize.width, mVsize.height), 0,
	                                   nullptr);
}

void Av1DecoderFilter::enableAvpf(bool enable) {
	mAvpfEnabled = enable;
	ms_message("Av1DecoderFilter: %s AVPF mode", mAvpfEnabled ? "enabling" : "disabling");
}

void Av1DecoderFilter::enableFreezeOnError(bool enable) {
	mFreezeOnError = enable;
	ms_message("Av1DecoderFilter: freeze on error %s", mFreezeOnError ? "enabled" : "disabled");
}

class Av1DecoderFilterImpl : public Av1DecoderFilter {
public:
	Av1DecoderFilterImpl(MSFilter *f) : Av1DecoderFilter(f, new Av1Decoder()) {
	}
};

} // namespace mediastreamer

using namespace mediastreamer;

MS_DECODING_FILTER_WRAPPER_METHODS_DECLARATION(Av1Decoder);
MS_DECODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(
    Av1Decoder, MS_AV1_DEC_ID, "An Av1 decoder based on dav1d.", "AV1", MS_FILTER_IS_PUMP);

MS_FILTER_DESC_EXPORT(ms_Av1Decoder_desc)
