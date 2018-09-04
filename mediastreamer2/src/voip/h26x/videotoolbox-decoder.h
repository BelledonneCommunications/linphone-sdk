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
#include <memory>
#include <string>

#include <VideoToolbox/VTDecompressionSession.h>

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msvideo.h"

#include "h26x-utils.h"

#include "h26x-decoder.h"

namespace mediastreamer {

class VideoToolboxDecoder: public H26xDecoder {
public:
	VideoToolboxDecoder(const std::string &mime);
	~VideoToolboxDecoder();

	bool feed(MSQueue *encodedFrame, uint64_t timestamp) override;
	Status fetch(mblk_t *&frame) override;

	void waitForKeyFrame() override {_freeze = true;}

private:
	class Frame {
	public:
		Frame(mblk_t *data = nullptr): _data(data) {}
		Frame(const Frame &src): _data(src._data ? dupmsg(src._data) : nullptr) {}
		~Frame() {if (_data) freemsg(_data);}
		mblk_t *getData() const {return _data ? dupmsg(_data) : nullptr;}

	private:
		mblk_t *_data = nullptr;
	};

	bool createDecoder();
	void destroyDecoder();
	bool decodeFrame(MSQueue *encodedFrame, uint64_t timestamp);
	bool formatDescFromSpsPps();

	static void outputCb(void *decompressionOutputRefCon, void *sourceFrameRefCon, OSStatus status,
						 VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer,
						 CMTime presentationTimeStamp, CMTime presentationDuration);

	VTDecompressionSessionRef _session = nullptr;
	CMFormatDescriptionRef _formatDesc = nullptr;
	MSYuvBufAllocator *_pixbufAllocator = nullptr;
	std::list<Frame> _queue;
	ms_mutex_t _mutex;
	std::unique_ptr<H26xParameterSetsStore> _psStore;
	std::unique_ptr<H26xNaluHeader> _naluHeader;
	bool _freeze = true;
	static const size_t _naluSizeLength = 4;
};

}
