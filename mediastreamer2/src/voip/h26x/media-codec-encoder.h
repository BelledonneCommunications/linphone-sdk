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
#include <sstream>
#include <string>

#include <media/NdkMediaCodec.h>

#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"

#include "encoding-filter-impl.h"
#include "h26x-utils.h"
#include "nal-packer.h"
#include "video-encoder-interface.h"

namespace mediastreamer {

class MediaCodecEncoder: public VideoEncoderInterface {
public:
	~MediaCodecEncoder();

	const std::string &getMime() const {return _mime;}

	MSVideoSize getVideoSize() const override {return _vsize;}
	void setVideoSize(const MSVideoSize &vsize) override {_vsize = vsize;}

	float getFps() const override {return _fps;}
	void setFps(float fps) override {_fps = fps;}

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

	std::string _mime;
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

class MediaCodecEncoderFilterImpl: public EncodingFilterImpl {
public:
	void preprocess() override;
	void process() override;
	void postprocess() override;

	const MSVideoConfiguration *getVideoConfiguratons() const override;
	void setVideoConfigurations(const MSVideoConfiguration *vconfs) override;
	int setVideoConfiguration(const MSVideoConfiguration *vconf) override;

	int getBitrate() const override;
	void setBitrate(int br) override;

	float getFps() const override;
	void setFps(float  fps) override;

	MSVideoSize getVideoSize() const override;
	void setVideoSize(const MSVideoSize &vsize) override;

	void enableAvpf(bool enable) override;

	void notifyPli() override;
	void notifyFir() override;

protected:
	MediaCodecEncoderFilterImpl(MSFilter *f, MediaCodecEncoder *encoder, NalPacker *packer, const MSVideoConfiguration *defaultVConfList);

	std::unique_ptr<MediaCodecEncoder> _encoder;
	std::unique_ptr<NalPacker> _packer;
	const MSVideoConfiguration *_vconfList = nullptr;
	const MSVideoConfiguration *_defaultVConfList = nullptr;
	MSVideoConfiguration _vconf;
	bool _avpfEnabled = false;
	bool _firstFrameDecoded = false;

	MSVideoStarter _starter;
	MSIFrameRequestsLimiterCtx _iframeLimiter;
};

}

