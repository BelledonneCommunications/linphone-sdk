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

#pragma once

#include <memory>
#include <vector>

#include "h26x-decoder.h"
#include "h26x-utils.h"
#include "media/NdkMediaCodec.h"
#include "mediastreamer2/msvideo.h"

namespace mediastreamer {

class MediaCodecDecoder : public H26xDecoder {
public:
	virtual ~MediaCodecDecoder();

	void waitForKeyFrame() override {
		_needKeyFrame = true;
	}

	bool feed(MSQueue *encodedFrame, uint64_t timestamp) override;
	Status fetch(mblk_t *&frame) override;

protected:
	class BufferFlag {
	public:
		static const uint32_t None = 0;
		static const uint32_t KeyFrame = 1;
		static const uint32_t CodecConfig = 1 << 1;
		static const uint32_t EndOfStream = 1 << 2;
		static const uint32_t PartialFrame = 1 << 3;
	};

	MediaCodecDecoder(const std::string &mime);
	virtual bool setParameterSets(MSQueue *parameterSet, uint64_t timestamp);
	AMediaFormat *createFormat(bool withLowLatency) const;
	void startImpl();
	void stopImpl() noexcept;
	void resetImpl() noexcept;
	bool feed(MSQueue *encodedFrame, uint64_t timestamp, bool isPs);
	bool isKeyFrame(const MSQueue *frame) const;
	static std::string codecInfoToString(ssize_t codecStatusCode);

	AMediaCodec *_impl = nullptr;
	AMediaFormat *_format = nullptr;
	MSYuvBufAllocator *_bufAllocator = nullptr;
	std::unique_ptr<H26xNaluHeader> _naluHeader;
	std::unique_ptr<H26xParameterSetsStore> _psStore;
	int _pendingFrames = 0;
	int32_t _curWidth = 0, _curHeight = 0;
	bool _needKeyFrame = true;
	bool _needParameters = true;

	static const unsigned int _timeoutUs = 0;
};

} // namespace mediastreamer
