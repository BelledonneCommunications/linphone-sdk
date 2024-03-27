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

#include "av1-encoder.h"

#include <memory>

using namespace mediastreamer;
using namespace std;

namespace mediastreamer {

Av1Encoder::Av1Encoder(MSFactory *factory) : mFactory(factory) {
	mEncoder = aom_codec_av1_cx();

	aom_codec_err_t ret = aom_codec_enc_config_default(mEncoder, &mConfig, AOM_USAGE_REALTIME);
	if (ret != AOM_CODEC_OK) {
		ms_error("Av1Encoder: Cannot retrieve encoder default settings (%d).", ret);
	}

	ms_queue_init(&mToEncode);
	ms_queue_init(&mEncodedFrames);
}

Av1Encoder::~Av1Encoder() {
	if (mIsRunning) Av1Encoder::stop();
}

void Av1Encoder::setVideoSize(const MSVideoSize &vsize) {
	mVsize = vsize;

	mConfig.g_w = vsize.width;
	mConfig.g_h = vsize.height;
}

void Av1Encoder::setFps(float fps) {
	mFps = fps;

	mConfig.g_timebase.num = 1;
	mConfig.g_timebase.den = (int)fps;

	if (mIsRunning) {
		lock_guard<mutex> guard(mCodecMutex);
		if (aom_codec_enc_config_set(&mCodec, &mConfig)) {
			ms_error("Av1Encoder: Failed to set fps while encoder is running.");
		}
	}
}

void Av1Encoder::setBitrate(int bitrate) {
	mBitrate = bitrate;

	// 0.92=take into account IP/UDP/RTP overhead, in average.
	mConfig.rc_target_bitrate = (unsigned int)(((float)mBitrate) * 0.92f / 1024.0f);

	if (mIsRunning) {
		lock_guard<mutex> guard(mCodecMutex);
		if (aom_codec_enc_config_set(&mCodec, &mConfig)) {
			ms_error("Av1Encoder: Failed to set bitrate while encoder is running.");
		}
	}
}

void Av1Encoder::configure(bool avpfEnabled) {
	// This initial configuration is for now the same as VP8
#if TARGET_IPHONE_SIMULATOR
	mConfig.g_threads = 1;
#elif TARGET_OS_OSX
	mConfig.g_threads = std::max<int>(ms_factory_get_cpu_count(mFactory) - 2, 1);
#else
	mConfig.g_threads = ms_factory_get_cpu_count(mFactory);
#endif

	mConfig.g_error_resilient = AOM_ERROR_RESILIENT_DEFAULT;

	if (avpfEnabled) {
		mConfig.kf_mode = AOM_KF_DISABLED;
	} else {
		mConfig.kf_mode = AOM_KF_AUTO;                     // encoder automatically places keyframes
		mConfig.kf_max_dist = 10 * mConfig.g_timebase.den; // 1 keyframe each 10s.
	}
}

void Av1Encoder::start() {
	if (mIsRunning) {
		ms_warning("Av1Encoder: Trying to start encode while already running.");
		return;
	}

	if (aom_codec_enc_init(&mCodec, mEncoder, &mConfig, 0)) {
		ms_error("Av1Encoder: Failed to initialize encoder.");
	} else {
		aom_codec_control(&mCodec, AOME_SET_CPUUSED, 10);

		mIsRunning = true;
		mEncodeThread = std::thread(&Av1Encoder::encoderThread, this);
	}
}

void Av1Encoder::stop() {
	if (mIsRunning) {
		mIsRunning = false;

		// Ensure the encoding thread can finish
		unique_lock<mutex> lk(mToEncodeMutex);
		mFramesAvailable = true;
		lk.unlock();
		mToEncodeCondition.notify_one();

		mEncodeThread.join();

		flush();

		if (aom_codec_destroy(&mCodec)) {
			ms_error("Av1Encoder: Failed to destroy encoder.");
		}
	}
}

void Av1Encoder::feed(mblk_t *rawData, [[maybe_unused]] uint64_t time, bool requestIFrame) {
	if (!mIsRunning) {
		ms_error("Av1Encoder: encoder not running. Dropping buffer.");
		return;
	}

	unique_lock<mutex> lk(mToEncodeMutex);

	ms_queue_put(&mToEncode, rawData);

	// Notify the encoding thread that frames are available
	mFramesAvailable = true;

	if (requestIFrame) mIframeRequested = true;

	lk.unlock();
	mToEncodeCondition.notify_one();
}

bool Av1Encoder::fetch(MSQueue *encodedData) {
	if (!mIsRunning) return false;

	lock_guard<mutex> guard(mEncodedFramesMutex);
	if (ms_queue_empty(&mEncodedFrames)) return false;

	mblk_t *out;
	while ((out = ms_queue_get(&mEncodedFrames)) != nullptr) {
		ms_queue_put(encodedData, out);
	}

	return true;
}

void Av1Encoder::encoderThread() {
	while (mIsRunning) {
		// Wait for available frames to pass to the encoder
		unique_lock lk(mToEncodeMutex);
		mToEncodeCondition.wait(lk, [this] { return this->mFramesAvailable; });

		mblk_t *data = nullptr;
		int skippedCount = 0;

		mblk_t *previous = nullptr;
		while ((data = ms_queue_get(&mToEncode)) != nullptr) {
			if (previous) {
				freemsg(previous);
				skippedCount++;
			}
			previous = data;
		}

		if (data == nullptr) data = previous;

		mFramesAvailable = false;
		lk.unlock();

		if (data == nullptr) continue;

		if (skippedCount > 0) ms_warning("Av1Encoder: %i frames skipped by async encoding process", skippedCount);

		MSPicture pic;
		ms_yuv_buf_init_from_mblk(&pic, data);

		aom_image_t img;
		aom_img_wrap(&img, AOM_IMG_FMT_I420, mVsize.width, mVsize.height, 1, pic.planes[0]);

		aom_enc_frame_flags_t flags = 0;

		if (mFrameCount == 0) flags |= AOM_EFLAG_FORCE_KF;

		lk.lock();
		if (mIframeRequested) {
			flags |= AOM_EFLAG_FORCE_KF;
			mIframeRequested = false;
		}
		lk.unlock();

		unique_lock codecLk(mCodecMutex);
		aom_codec_err_t ret = aom_codec_encode(&mCodec, &img, mFrameCount, 1, flags);

		if (ret != AOM_CODEC_OK) {
			ms_error("Av1Encoder: encode failed: %s (%s)", aom_codec_err_to_string(ret),
			         aom_codec_error_detail(&mCodec));
		}

		const aom_codec_cx_pkt_t *pkt;
		aom_codec_iter_t iter = nullptr;

		while ((pkt = aom_codec_get_cx_data(&mCodec, &iter))) {
			if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
				mblk_t *out = allocb(pkt->data.frame.sz, 0);
				memcpy(out->b_wptr, pkt->data.frame.buf, pkt->data.frame.sz);
				out->b_wptr += pkt->data.frame.sz;
				mblk_set_timestamp_info(out, mblk_get_timestamp_info(data));
				mblk_set_independent_flag(out, (pkt->data.frame.flags & AOM_FRAME_IS_KEY ||
				                                        pkt->data.frame.flags & AOM_FRAME_IS_INTRAONLY ||
				                                        pkt->data.frame.flags & AOM_FRAME_IS_SWITCH
				                                    ? 0x1
				                                    : 0x0));
				mblk_set_discardable_flag(out, (pkt->data.frame.flags & AOM_FRAME_IS_DROPPABLE ? 0x1 : 0x0));

				lock_guard<mutex> guard(mEncodedFramesMutex);
				ms_queue_put(&mEncodedFrames, out);
			}
		}
		codecLk.unlock();

		mFrameCount++;

		freemsg(data);
	}
}

void Av1Encoder::flush() {
	aom_codec_iter_t iter = nullptr;
	while (aom_codec_get_cx_data(&mCodec, &iter))
		continue;

	ms_queue_flush(&mToEncode);
	ms_queue_flush(&mEncodedFrames);
}

} // namespace mediastreamer
