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

#include "android_mediacodec.h"

#include "media-codec-encoder.h"

#include <algorithm>
#include <limits>

using namespace mediastreamer;
using namespace std;

namespace mediastreamer {

MediaCodecEncoder::MediaCodecEncoder(const std::string &mime): H26xEncoder(mime), _psInserter(H26xToolFactory::get(mime).createParameterSetsInserter()) {
	try {
		_vsize.width = 0;
		_vsize.height = 0;
		// We shall allocate the MediaCodec encoder the sooner as possible and before the decoder, because
		// some phones hardware encoder and decoders can't be allocated at the same time.
		createImpl();
	} catch (const runtime_error &e) {
		ms_error("MediaCodecEncoder: %s", e.what());
	}
}

MediaCodecEncoder::~MediaCodecEncoder() {
	if (_impl) AMediaCodec_delete(_impl);
}

void MediaCodecEncoder::enableOutbufferDequeueLimit(bool enable){
	if (enable){
		ms_warning("Enabling the DEVICE_MCH265_LIMIT_DEQUEUE_OF_OUTPUT_BUFFERS hack on this device.");
	}
	_hasOutbufferDequeueLimit = enable;
}

void MediaCodecEncoder::setFps(float fps) {
	_fps = fps;
	if (isRunning() && _impl) {
		AMediaFormat *afmt = AMediaFormat_new();
		AMediaFormat_setInt32(afmt, "frame-rate", int32_t(_fps));
		AMediaCodec_setParams(_impl, afmt);
		AMediaFormat_delete(afmt);
	}
}

void MediaCodecEncoder::setBitrate(int bitrate) {
	_bitrate = bitrate;
	if (isRunning() && _impl) {
		AMediaFormat *afmt = AMediaFormat_new();
		// update the output bitrate
		AMediaFormat_setInt32(afmt, "video-bitrate", (_bitrate * 9)/10);
		AMediaCodec_setParams(_impl, afmt);
		AMediaFormat_delete(afmt);
	}
}

void MediaCodecEncoder::start() {
	try {
		if (_impl == nullptr) {
			ms_error("MediaCodecEncoder: starting failed. No MediaCodec instance.");
			return;
		}
		if (_isRunning) {
			ms_warning("MediaCodecEncoder: encoder already started");
			return;
		}
		configureImpl();
		if (AMediaCodec_start(_impl) != AMEDIA_OK) {
			throw runtime_error("could not start encoder.");
		}
		_isRunning = true;
		ms_message("MediaCodecEncoder: encoder successfully started");
	} catch (const runtime_error &e) {
		ms_error("MediaCodecEncoder: %s", e.what());
	}
}

void MediaCodecEncoder::stop() {
	if (_impl == nullptr) return;
	if (!_isRunning) {
		ms_warning("MediaCodecEncoder: encoder already stopped");
		return;
	}
	if (_impl) {
		AMediaCodec_flush(_impl);
		AMediaCodec_stop(_impl);
		// It is preferable to reset the encoder, otherwise it may not accept a new configuration while returning in preprocess().
		// This was observed at least on Moto G2, with qualcomm encoder.
		AMediaCodec_reset(_impl);
	}
	_psInserter->flush();
	_isRunning = false;
	_pendingFrames = 0;
	_firstImageQueued = false;
}

void MediaCodecEncoder::feed(mblk_t *rawData, uint64_t time, bool requestIFrame) {
	// ensure that rawData is destroyed on function return
	unique_ptr<mblk_t, void(*)(mblk_t *)> rawDataPtr(rawData, freemsg);

	
	if (_recoveryMode && time % 5000 == 0) {
		try {
			if (_impl == nullptr) createImpl();
			configureImpl();
			_recoveryMode = false;
		} catch (const runtime_error &e) {
			ms_error("MediaCodecEncoder: %s", e.what());
			ms_error("MediaCodecEncoder: AMediaCodec_reset() was not sufficient, will recreate the encoder in a moment...");
			AMediaCodec_delete(_impl);
			_impl = nullptr;
		}
	}
	
	if (_impl == nullptr) return;

	if (!_isRunning) {
		ms_error("MediaCodecEncoder: encoder not running. Dropping buffer.");
		return;
	}

	MSPicture pic;
	ms_yuv_buf_init_from_mblk(&pic, rawData);

	if (requestIFrame) {
		AMediaFormat *afmt = AMediaFormat_new();
		/*Force a key-frame*/
		AMediaFormat_setInt32(afmt, "request-sync", 0);
		AMediaCodec_setParams(_impl, afmt);
		AMediaFormat_delete(afmt);
		ms_error("MediaCodecEncoder: I-frame requested to MediaCodec");
	}

	ssize_t ibufidx = AMediaCodec_dequeueInputBuffer(_impl, _timeoutUs);
	if (ibufidx < 0) {
		if (ibufidx == AMEDIA_ERROR_UNKNOWN) {
			ms_error("MediaCodecEncoder: AMediaCodec_dequeueInputBuffer() had an exception");
		} else if (ibufidx == -1) {
			ms_error("MediaCodecEncoder: no input buffer available.");
		} else {
			ms_error("MediaCodecEncoder: unknown error while requesting an input buffer (%zd)", ibufidx);
		}
		return;
	}

	size_t bufsize = 0;
	size_t bufsize_max = numeric_limits<size_t>::max();

	if (_pixelFormatConvertionEnabled) {
		if (ms2_android_get_sdk_version() >= 31) {
			// There is a size problem in Android 12
			// We call getInputBuffer once before getting the image to know the max available size
			// And use that size when queuing the buffer.
			AMediaCodec_getInputBuffer(_impl, ibufidx, &bufsize_max);
		}

		AMediaImage image;
		if (AMediaCodec_getInputImage(_impl, ibufidx, &image)) {
			if (image.format == 35 /* YUV_420_888 */) {
				MSRect src_roi = {0, 0, pic.w, pic.h};
				int src_pix_strides[4] = {1, 1, 1, 1};
				ms_yuv_buf_copy_with_pix_strides(pic.planes, pic.strides, src_pix_strides, src_roi, image.buffers, image.row_strides, image.pixel_strides, image.crop_rect);
				bufsize = image.row_strides[0] * image.height * 3 / 2;
			} else {
				ms_error("MediaCodecEncoder: encoder requires non YUV420 format");
			}
			AMediaImage_close(&image);
		}
	} else {
		uint8_t *inputBuffer = AMediaCodec_getInputBuffer(_impl, ibufidx, &bufsize);
		size_t dataSize = rawData->b_wptr-rawData->b_rptr;
		if (dataSize <= bufsize) {
			memcpy(inputBuffer, rawData->b_rptr, dataSize);
		} else {
			ms_error("MediaCodecEncoder: not enough space in the input buffer (required=%zu, availabel=%zu)", dataSize, bufsize);
			bufsize = 0;
		}
	}

	if (AMediaCodec_queueInputBuffer(_impl, ibufidx, 0, min(bufsize, bufsize_max), time * 1000, 0) == AMEDIA_ERROR_BASE) {
		ms_error("MediaCodecEncoder: error while queuing input buffer");
		return;
	}
	if (!_firstImageQueued) _firstImageQueued = true;
	_pendingFrames++;
}

bool MediaCodecEncoder::fetch(MSQueue *encodedData) {
	MSQueue outq;
	AMediaCodecBufferInfo info;
	uint8_t *buf;
	size_t bufsize;

	if (_impl == nullptr || !_isRunning || _recoveryMode || !_firstImageQueued) return false;
	
	if (_hasOutbufferDequeueLimit && _pendingFrames <= 0) return false;

	ms_queue_init(&outq);

	ssize_t obufidx = AMediaCodec_dequeueOutputBuffer(_impl, &info, _timeoutUs);
	if (obufidx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
		ms_message("MediaCodecEncoder: output format has changed.");
		AMediaFormat *format = AMediaCodec_getOutputFormat(_impl);
		ms_message("MediaCodecEncoder: new output format:\n%s", AMediaFormat_toString(format));
		AMediaFormat_delete(format);
		obufidx = AMediaCodec_dequeueOutputBuffer(_impl, &info, _timeoutUs);
	}
	if (obufidx < 0) {
		if (obufidx == AMEDIA_ERROR_UNKNOWN) {
			ms_error("MediaCodecEncoder: AMediaCodec_dequeueOutputBuffer() had an exception, MediaCodec is lost");
			// MediaCodec need to be reset  at this point because it may have become irrevocably crazy.
			AMediaCodec_reset(_impl);
			_recoveryMode = true;
		} else if (obufidx != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
			ms_error("MediaCodecEncoder: unknown error while requesting an output buffer (%zd)", obufidx);
		}
		return false;
	}

	_pendingFrames--;

	if ((buf = AMediaCodec_getOutputBuffer(_impl, obufidx, &bufsize)) == nullptr) {
		ms_error("MediaCodecEncoder: AMediaCodec_getOutputBuffer() returned nullptr");
		AMediaCodec_releaseOutputBuffer(_impl, obufidx, FALSE);
		return false;
	}
	/*
	 * Split bitstream into NAL units. removePreventionBytes (start code emulation prevention byte) is willingly set to false, which is not obvious.
	 * Indeed, we should expect that the encoder has inserted prevention bytes (0x03) to escape 0x000001 start codes that could be present
	 * within slices. When splitting into NAL units, these 0x03 bytes should be removed, since it is allowed to have 0x000001 sequences within a NAL unit.
	 * However, the practice shows that if we do that, we get random decoding errors at the other end, whatever the other end is an Android or an iOS device.
	 * If we do not do that, the decoding goes perfectly well. How to explain this ?
	 * - this could mean that MediaCodec does not insert prevention bytes, but the encoder guarantees by some other means that no 0x000001 sequences can occur.
	 *   however 0x00000301 may happen but this is not a prevention byte.
	 * - this could mean that we have another bug elsewhere in H26xUtils routines, but then why decoding goes perfectly well when not removing prevention bytes ?
	 */
	H26xUtils::byteStreamToNalus(buf + info.offset, info.size, &outq, false);
	_psInserter->process(&outq, encodedData);

	AMediaCodec_releaseOutputBuffer(_impl, obufidx, FALSE);
	return true;
}

void MediaCodecEncoder::createImpl() {
	_impl = AMediaCodec_createEncoderByType(_mime.c_str());
	if (_impl == nullptr) {
		throw runtime_error("could not create MediaCodec");
	}
}

void MediaCodecEncoder::configureImpl() {
	AMediaFormat *format = createMediaFormat();

	ms_message("MediaCodecEncoder: configuring MediaCodec with the following parameters:\n%s", AMediaFormat_toString(format));

	media_status_t status = AMediaCodec_configure(_impl, format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
	AMediaFormat_delete(format);

	if (status != 0) {
		throw runtime_error("could not configure encoder.");
	}

	format = AMediaCodec_getOutputFormat(_impl);
	ms_message("MediaCodecEncoder: encoder successfully configured. In-force parameters:\n%s", AMediaFormat_toString(format));
	AMediaFormat_delete(format);
}

AMediaFormat *MediaCodecEncoder::createMediaFormat() const {
	AMediaFormat *format = AMediaFormat_new();
	AMediaFormat_setString(format, "mime", _mime.c_str());
	AMediaFormat_setInt32(format, "color-format", _colorFormat);
	AMediaFormat_setInt32(format, "width", _vsize.width);
	AMediaFormat_setInt32(format, "height", _vsize.height);
	AMediaFormat_setInt32(format, "frame-rate", _fps);
	AMediaFormat_setInt32(format, "bitrate", (_bitrate * 9)/10); // take a margin
	AMediaFormat_setInt32(format, "bitrate-mode", _bitrateMode);
	AMediaFormat_setInt32(format, "i-frame-interval", _iFrameInterval);
	AMediaFormat_setInt32(format, "latency", _encodingLatency);
	AMediaFormat_setInt32(format, "priority", _priority);
	return format;
}

} // namespace mediastreamer
