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

#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <aom/aom_encoder.h>
#include <aom/aomcx.h>

#include "mediastreamer2/msqueue.h"
#include "video-encoder.h"

namespace mediastreamer {

class Av1Encoder : public VideoEncoder {
public:
	explicit Av1Encoder(MSFactory *factory);
	virtual ~Av1Encoder();

	MSVideoSize getVideoSize() const override {
		return mVsize;
	};
	void setVideoSize(const MSVideoSize &vsize) override;

	float getFps() const override {
		return mFps;
	};
	void setFps(float fps) override;

	int getBitrate() const override {
		return mBitrate;
	};
	void setBitrate(int bitrate) override;

	bool isRunning() override {
		return mIsRunning;
	};

	void configure(bool avpfEnabled);

	void start() override;
	void stop() override;

	void feed(mblk_t *rawData, uint64_t time, bool requestIFrame) override;
	bool fetch(MSQueue *encodedData) override;

	void flush();

protected:
	void encoderThread();

	MSFactory *mFactory;

	aom_codec_iface_t *mEncoder = nullptr;
	aom_codec_enc_cfg_t mConfig;
	aom_codec_ctx_t mCodec;

	MSVideoSize mVsize = MS_VIDEO_SIZE_CIF;
	float mFps = 0;
	int mBitrate = 0;
	bool mIsRunning = false;
	bool mIframeRequested = false;

	unsigned int mFrameCount = 0;

	// IN frames
	MSQueue mToEncode;
	std::mutex mToEncodeMutex;
	std::condition_variable mToEncodeCondition;
	bool mFramesAvailable = false;

	// OUT frames
	MSQueue mEncodedFrames;
	std::mutex mEncodedFramesMutex;

	std::mutex mCodecMutex;

	std::thread mEncodeThread{};
};

} // namespace mediastreamer
