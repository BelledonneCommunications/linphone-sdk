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

#include "av1-decoder.h"

using namespace mediastreamer;
using namespace std;

namespace mediastreamer {

Av1Decoder::Av1Decoder() : mKeyFrameIndicator() {
	dav1d_default_settings(&mSettings);

	mSettings.n_threads = 2;
	mSettings.max_frame_delay = 1;

	// Disable dav1d internal logging
	mSettings.logger.callback = nullptr;

	int res = 0;
	if ((res = dav1d_open(&mContext, &mSettings))) {
		ms_message("Av1Decoder: failed opening dav1d decoder %d", res);
	}

	mYuvBufAllocator = ms_yuv_buf_allocator_new();
}

Av1Decoder::~Av1Decoder() {
	while (!mPendingFrames.empty()) {
		Dav1dPicture &p = mPendingFrames.front();
		dav1d_picture_unref(&p);
		mPendingFrames.pop();
	}

	if (mContext) {
		dav1d_flush(mContext);
		dav1d_close(&mContext);
		mContext = nullptr;
	}

	if (mYuvBufAllocator) ms_yuv_buf_allocator_free(mYuvBufAllocator);
}

static void releaseDecoderBuffer([[maybe_unused]] const uint8_t *buffer, void *opaque) {
	if (opaque) freemsg(static_cast<mblk_t *>(opaque));
}

bool Av1Decoder::feed(MSQueue *encodedFrame, uint64_t timestamp) {
	if (mContext == nullptr) {
		ms_message("Av1Decoder: Trying to feed frames to decoder but decoder is not started");
		ms_queue_flush(encodedFrame);
		return true;
	}

	Dav1dData data{};
	mblk_t *im;

	// Keep going in case data cannot be consumed so we won't loose it
	while (data.sz != 0 || (im = ms_queue_get(encodedFrame))) {
		if (data.sz == 0) {
			if (mKeyFrameNeeded) {
				if (mKeyFrameIndicator.parseFrame(im)) {
					ms_message("Av1Decoder: key frame received");
					mKeyFrameNeeded = false;
					mKeyFrameIndicator.reset();
				} else {
					ms_error("Av1Decoder: waiting for key frame");
					freemsg(im);
					continue;
				}
			}

			if (dav1d_data_wrap(&data, im->b_rptr, msgdsize(im), releaseDecoderBuffer, im)) {
				ms_message("Av1Decoder: Failed to wrap data");
				freemsg(im);
				return false;
			}
			data.m.timestamp = (int64_t)timestamp;
		}

		if (data.sz) {
			int res = dav1d_send_data(mContext, &data);

			if (res < 0) {
				// EAGAIN means data cannot be consumed and we should retrieve frames
				if (res == DAV1D_ERR(EAGAIN)) {
					Dav1dPicture p{};

					while ((res = dav1d_get_picture(mContext, &p)) == 0) {
						mPendingFrames.push(std::move(p));
						p = {};
					}

					if (res != DAV1D_ERR(EAGAIN)) {
						ms_error("Av1Decoder: error during decoding");
					}
				} else {
					ms_message("Av1Decoder: Error while sending data");
					dav1d_data_unref(&data);
					return false;
				}
			}
		}
	}

	return !mKeyFrameNeeded;
}

VideoDecoder::Status Av1Decoder::fetch(mblk_t *&frame) {
	Dav1dPicture p{};

	if (!mPendingFrames.empty()) {
		p = std::move(mPendingFrames.front());
		mPendingFrames.pop();
	} else {
		int res = dav1d_get_picture(mContext, &p);

		if (res < 0) {
			if (res == DAV1D_ERR(EAGAIN)) {
				return VideoDecoder::Status::NoFrameAvailable;
			} else {
				ms_error("Av1Decoder: error during decoding");
				return VideoDecoder::Status::DecodingFailure;
			}
		}
	}

	if (p.p.bpc != 8) {
		ms_error("Av1Decoder: bpc is not 8, which is not supported yet");
		dav1d_picture_unref(&p);
		return VideoDecoder::Status::DecodingFailure;
	}

	int rowStrides[3] = {static_cast<int>(p.stride[0]), static_cast<int>(p.stride[1]), static_cast<int>(p.stride[1])};
	MSPicture pic;

	frame = ms_yuv_buf_allocator_get(mYuvBufAllocator, &pic, p.p.w, p.p.h);
	ms_yuv_buf_copy((uint8_t **)(&p.data), rowStrides, pic.planes, pic.strides, {p.p.w, p.p.h});

	dav1d_picture_unref(&p);

	return VideoDecoder::Status::NoError;
}

void Av1Decoder::flush() {
	while (!mPendingFrames.empty()) {
		Dav1dPicture &p = mPendingFrames.front();
		dav1d_picture_unref(&p);
		mPendingFrames.pop();
	}

	dav1d_flush(mContext);
	mKeyFrameIndicator.reset();
}

} // namespace mediastreamer
