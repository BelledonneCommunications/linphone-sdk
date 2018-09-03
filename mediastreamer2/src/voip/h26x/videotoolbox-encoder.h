/*
 Mediastreamer2 videotoolbox-encoder.h
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

#include <list>
#include <string>

#include <VideoToolbox/VTCompressionSession.h>

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msqueue.h"

#include "h26x-encoder.h"

namespace mediastreamer {

class VideoToolboxEncoder: public H26xEncoder {
public:
	VideoToolboxEncoder();
	~VideoToolboxEncoder() {if (_session) CFRelease(_session);}

	MSVideoSize getVideoSize() const override {return _vsize;}
	void setVideoSize(const MSVideoSize &vsize) override {_vsize = vsize;}

	float getFps() const override {return _framerate;}
	void setFps(float fps) override;

	int getBitrate() const override {return _bitrate;}
	void setBitrate(int bitrate) override;

	bool isRunning() override {return _session != nullptr;}
	void start() override;
	void stop() override;

	void feed(mblk_t *rawData, uint64_t time, bool requestIFrame = false) override;
	bool fetch(MSQueue *encodedData) override;

private:
	class Frame {
	public:
		Frame() {ms_queue_init(&_nalus);}
		Frame(Frame &&src);
		~Frame() {ms_queue_flush(&_nalus);}

		void put(mblk_t *m) {ms_queue_put(&_nalus, m);}
		mblk_t *get() {return ms_queue_get(&_nalus);}

		MSQueue *getQueue() {return &_nalus;}

	private:
		MSQueue _nalus;
	};

	void applyFramerate();
	void applyBitrate();
	static void outputCb(void *outputCallbackRefCon, void *sourceFrameRefCon, OSStatus status, VTEncodeInfoFlags infoFlags, CMSampleBufferRef sampleBuffer);

	MSVideoSize _vsize;
	float _framerate = 0.0f;
	int _bitrate = 0;
	VTCompressionSessionRef _session = nullptr;
	ms_mutex_t _mutex;
	std::list<Frame> _encodedFrames;
};

}
