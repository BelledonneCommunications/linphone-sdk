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
#include <string>

#include <media/NdkMediaCodec.h>

#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"

#include "nal-packer.h"

namespace mediastreamer {

class MediaCodecEncoderFilterImpl {
public:
	MediaCodecEncoderFilterImpl(MSFilter *f, const std::string &mime, NalPacker *packer);
	virtual ~MediaCodecEncoderFilterImpl();

	virtual void preprocess();
	virtual void process();
	virtual void postprocess();

	int getBitrate() const;
	void setBitrate(int br);

	const MSVideoConfiguration *getVideoConfiguratons() const;
	void setVideoConfigurations(const MSVideoConfiguration *vconfs);
	int setVideoConfiguration(const MSVideoConfiguration *vconf);

	float getFps() const;
	void setFps(float  fps);

	MSVideoSize getVideoSize() const;
	void setVideoSize(const MSVideoSize &vsize);

	void enableAvpf(bool enable);
	void notifyPli();
	void notifyFir();

protected:
	virtual void onFrameEncodedHook(MSQueue *frame) {};

	media_status_t allocEncoder();
	media_status_t tryColorFormat(AMediaFormat *format, unsigned value);
	int encConfigure();

	const MSVideoConfiguration *_vconfList;
	MSVideoConfiguration _vconf;
	bool _avpfEnabled = false;
	int _profile = 0;
	int _level = 0;

	MSFilter *_f = nullptr;
	std::string _mime;
	std::unique_ptr<NalPacker> _packer;
	AMediaCodec *_codec = nullptr;
	uint64_t _framenum = 0;
	MSVideoStarter _starter;
	MSIFrameRequestsLimiterCtx _iframeLimiter;
	bool _firstBufferQueued = false;
	bool _codecStarted = false;
	bool _codecLost = false;

	static const int _timeoutUs = 0;
};

}
