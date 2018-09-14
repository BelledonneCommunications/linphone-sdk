/*
 Mediastreamer2 media-codec-encoder.h
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

#pragma once

#include <memory>

#include "h26x-utils.h"
#include "media/NdkMediaCodec.h"

#include "h26x-encoder.h"

namespace mediastreamer {

class MediaCodecEncoder: public H26xEncoder {
public:
	~MediaCodecEncoder();

	const std::string &getMime() const {return _mime;}

	MSVideoSize getVideoSize() const override {return _vsize;}
	void setVideoSize(const MSVideoSize &vsize) override {_vsize = vsize;}

	float getFps() const override {return _fps;}
	void setFps(float fps) override;

	int getBitrate() const override {return _bitrate;}
	void setBitrate(int bitrate) override;

	bool isRunning() override {return _isRunning;}
	void start() override;
	void stop() override;

	void feed(mblk_t *rawData, uint64_t time, bool requestIFrame = false) override;
	bool fetch(MSQueue *encodedData) override;

protected:
	MediaCodecEncoder(const std::string &mime);
	void createImpl();
	void configureImpl();
	virtual AMediaFormat *createMediaFormat() const;
	virtual std::ostringstream getMediaForamtAsString() const;

	std::unique_ptr<H26xParameterSetsInserter> _psInserter;
	MSVideoSize _vsize;
	float _fps = 0;
	int _bitrate = 0;
	AMediaCodec *_impl = nullptr;
	int _pendingFrames = 0;
	bool _isRunning = false;
	bool _recoveryMode = false;

	static const int _timeoutUs = 0;
	static const int32_t _colorFormat = 0x7f420888; // COLOR_FormatYUV420Flexible
	static const int32_t _bitrateMode = 1; // VBR mode
	static const int32_t _iFrameInterval = 20; // 20 seconds
	static const int32_t _encodingLatency = 1;
	static const int32_t _priority = 0; // real-time priority
};

}

