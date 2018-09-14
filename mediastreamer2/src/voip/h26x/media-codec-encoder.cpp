/*
mediastreamer2 mediacodech264enc.c
Copyright (C) 2015 Belledonne Communications SARL

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

#include "android_mediacodec.h"

#include "media-codec-encoder.h"

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
}

void MediaCodecEncoder::feed(mblk_t *rawData, uint64_t time, bool requestIFrame) {
	if (_impl == nullptr) {
		freemsg(rawData);
		return;
	}

	if (!_isRunning) {
		ms_error("MediaCodecEncoder: encoder not running. Dropping buffer.");
		freemsg(rawData);
		return;
	}

	if (_recoveryMode && time % 5000 == 0) {
		try {
			if (_impl == nullptr) createImpl();
			configureImpl();
			_recoveryMode = false;
		} catch (const runtime_error &e) {
			ms_error("MediaCodecEncoder: %s", e.what());
			ms_error("MediaCodecEncoder: AMediaCodec_reset() was not sufficient, will recreate the encoder in a moment...");
			AMediaCodec_delete(_impl);
		}
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

	size_t bufsize;
	uint8_t *buf = AMediaCodec_getInputBuffer(_impl, ibufidx, &bufsize);
	if (buf == nullptr) {
		ms_error("MediaCodecEncoder: obtained InputBuffer, but no address.");
		return;
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

	if (AMediaCodec_queueInputBuffer(_impl, ibufidx, 0, bufsize, time * 1000, 0) == AMEDIA_ERROR_BASE) {
		ms_error("MediaCodecEncoder: error while queuing input buffer");
		return;
	}

	_pendingFrames++;
	freemsg(rawData);
}

bool MediaCodecEncoder::fetch(MSQueue *encodedData) {
	MSQueue outq;
	AMediaCodecBufferInfo info;
	uint8_t *buf;
	size_t bufsize;

	if (_impl == nullptr || !_isRunning || _recoveryMode || _pendingFrames <= 0) return false;

	ms_queue_init(&outq);

	ssize_t obufidx = AMediaCodec_dequeueOutputBuffer(_impl, &info, _timeoutUs);
	if (obufidx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
		ms_message("MediaCodecEncoder: output format has changed.");
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

	H26xUtils::byteStreamToNalus(buf + info.offset, info.size, &outq);
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

	ms_message("MediaCodecEncoder: configuring MediaCodec with the following parameters:");
	ms_message("%s", getMediaForamtAsString().str().c_str());

	media_status_t status = AMediaCodec_configure(_impl, format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
	AMediaFormat_delete(format);

	if (status != 0) {
		throw runtime_error("could not configure encoder.");
	}

	ms_message("MediaCodecEncoder: encoder successfully configured.");
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

std::ostringstream MediaCodecEncoder::getMediaForamtAsString() const {
	ostringstream os;
	os << "\tmime: " << _mime << endl;

	os.unsetf(ios::basefield);
	os.setf(ios::hex);
	os.setf(ios::showbase);
	os << "\tcolor-format: " << _colorFormat << endl;
	os.unsetf(ios::basefield);
	os.setf(ios::dec);
	os.unsetf(ios::showbase);

	os << "\tvideo size: " << _vsize.width << "x" << _vsize.height << endl;
	os << "\tframe-rate: " << _fps << " fps" << endl;
	os << "\tbitrate: " << _bitrate << " b/s" << endl;
	os << "\tbitrate-mode: " << _bitrateMode << endl;
	os << "\ti-frame-intervale: " << _iFrameInterval << endl;
	os << "\tlatency: " << _encodingLatency << endl;
	os << "\tpriority: " << _priority << endl;
	return os;
}

} // namespace mediastreamer
