/*
 Mediastreamer2 media-codec-decoder.cpp
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


#include "android_mediacodec.h"

#include "media-codec-decoder.h"

using namespace mediastreamer;
using namespace std;

namespace mediastreamer {

MediaCodecDecoder::MediaCodecDecoder(const std::string &mime): H26xDecoder(mime) {
	try {
		_impl = AMediaCodec_createDecoderByType(mime.c_str());
		if (_impl == nullptr) {
			ostringstream msg;
			msg << "could not create MediaCodec for '" << mime << "'";
			throw runtime_error(msg.str());
		}
		_format = createFormat(mime);
		_bufAllocator = ms_yuv_buf_allocator_new();
		_naluHeader.reset(H26xToolFactory::get(mime).createNaluHeader());
		_psStore.reset(H26xToolFactory::get(mime).createParameterSetsStore());
		startImpl();
	} catch (const runtime_error &e) {
		if (_impl) AMediaCodec_delete(_impl);
		if (_format) AMediaFormat_delete(_format);
		if (_bufAllocator) ms_yuv_buf_allocator_free(_bufAllocator);
		throw e;
	}
}

MediaCodecDecoder::~MediaCodecDecoder() {
	AMediaCodec_delete(_impl);
	ms_yuv_buf_allocator_free(_bufAllocator);
}

bool MediaCodecDecoder::setParameterSets(MSQueue *parameterSets, uint64_t timestamp) {
	if (!feed(parameterSets, timestamp, true)) {
		ms_error("MediaCodecDecoder: parameter sets has been refused by the decoder.");
		return false;
	}
	_needParameters = false;
	return true;
}

bool MediaCodecDecoder::feed(MSQueue *encodedFrame, uint64_t timestamp) {
	bool status = false;

	_psStore->extractAllPs(encodedFrame);
	if (_psStore->hasNewParameters()) {
		ms_message("MediaCodecDecoder: new paramter sets received");
		_needParameters = true;
		_psStore->acknowlege();
	}

	if (_needParameters) {
		MSQueue parameters;
		ms_queue_init(&parameters);
		_psStore->fetchAllPs(&parameters);
		if (!setParameterSets(&parameters, timestamp)) {
			ms_error("MediaCodecDecoder: waiting for parameter sets.");
			goto clean;
		}
	}

	if (_needKeyFrame) {
		if (!isKeyFrame(encodedFrame)) {
			ms_error("MediaCodecDecoder: waiting for key frame.");
			goto clean;
		}
		_needKeyFrame = false;
	}

	if (!feed(encodedFrame, timestamp, false)) {
		goto clean;
	}

	_pendingFrames++;
	status = true;

clean:
	ms_queue_flush(encodedFrame);
	return status;
}

MediaCodecDecoder::Status MediaCodecDecoder::fetch(mblk_t *&frame) {
	AMediaImage image = {0};
	int dst_pix_strides[4] = {1, 1, 1, 1};
	MSRect dst_roi = {0};
	AMediaCodecBufferInfo info;
	ssize_t oBufidx = -1;
	Status status = noError;

	frame = nullptr;

	if (_impl == nullptr || _pendingFrames <= 0) {
		status = noFrameAvailable;
		goto end;
	}

	oBufidx = AMediaCodec_dequeueOutputBuffer(_impl, &info, _timeoutUs);
	if (oBufidx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
		ms_message("MediaCodecDecoder: output format has changed.");
		oBufidx = AMediaCodec_dequeueOutputBuffer(_impl, &info, _timeoutUs);
	}

	if (oBufidx < 0) {
		if (oBufidx == AMEDIA_ERROR_UNKNOWN) {
			ms_error("MediaCodecDecoder: AMediaCodec_dequeueOutputBuffer() had an exception");
			status = decodingFailure;
		} else if (oBufidx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
			ms_error("MediaCodecDecoder: decoder isn't ready");
			status = noFrameAvailable;
		} else {
			ms_error("MediaCodecDecoder: unknown error while dequeueing an output buffer (oBufidx=%zd)", oBufidx);
			status = noFrameAvailable;
		}
		goto end;
	}

	_pendingFrames--;

	if (AMediaCodec_getOutputImage(_impl, oBufidx, &image) <= 0) {
		ms_error("MediaCodecDecoder: AMediaCodec_getOutputImage() failed");
		status = decodingFailure;
		goto end;
	}

	MSPicture pic;
	frame = ms_yuv_buf_allocator_get(_bufAllocator, &pic, image.crop_rect.w, image.crop_rect.h);
	ms_yuv_buf_copy_with_pix_strides(image.buffers, image.row_strides, image.pixel_strides, image.crop_rect,
										pic.planes, pic.strides, dst_pix_strides, dst_roi);
	AMediaImage_close(&image);

end:
	if (oBufidx >= 0) AMediaCodec_releaseOutputBuffer(_impl, oBufidx, FALSE);
	return status;
}

AMediaFormat *MediaCodecDecoder::createFormat(const std::string &mime) const {
	AMediaFormat *format = AMediaFormat_new();
	AMediaFormat_setString(format, "mime", mime.c_str());
	AMediaFormat_setInt32(format, "color-format", 0x7f420888);
	AMediaFormat_setInt32(format, "max-width", 1920);
	AMediaFormat_setInt32(format, "max-height", 1920);
	AMediaFormat_setInt32(format, "priority", 0);
	return format;
}

void MediaCodecDecoder::startImpl() {
	media_status_t status = AMEDIA_OK;
	ostringstream errMsg;
	ms_message("MediaCodecDecoder: starting decoder");
	if ((status = AMediaCodec_configure(_impl, _format, nullptr, nullptr, 0)) != AMEDIA_OK) {
		errMsg << "configuration failure: " << int(status);
		throw runtime_error(errMsg.str());
	}

	if ((status = AMediaCodec_start(_impl)) != AMEDIA_OK) {
		errMsg << "starting failure: " << int(status);
		throw runtime_error(errMsg.str());
	}
}

void MediaCodecDecoder::stopImpl() {
	ms_message("MediaCodecDecoder: stopping decoder");
	AMediaCodec_stop(_impl);
}

bool MediaCodecDecoder::feed(MSQueue *encodedFrame, uint64_t timestamp, bool isPs) {
	unique_ptr<H26xNaluHeader> header;
	header.reset(H26xToolFactory::get("video/avc").createNaluHeader());
	for(mblk_t *m = ms_queue_peek_first(encodedFrame); !ms_queue_end(encodedFrame, m); m = ms_queue_next(encodedFrame, m)) {
		header->parse(m->b_rptr);
		ms_message("MediaCodecDecoder: nalu type %d", int(header->getAbsType()));
	}

	H26xUtils::nalusToByteStream(encodedFrame, _bitstream);

	if (_impl == nullptr) return false;

	ssize_t iBufidx = AMediaCodec_dequeueInputBuffer(_impl, _timeoutUs);
	if (iBufidx < 0) {
		ms_error("MediaCodecDecoder: %s.", iBufidx == -1 ? "no buffer available for queuing this frame ! Decoder is too slow" : "AMediaCodec_dequeueInputBuffer() had an exception");
		return false;
	}

	size_t bufsize;
	uint8_t *buf = AMediaCodec_getInputBuffer(_impl, iBufidx, &bufsize);
	if (buf == nullptr) {
		ms_error("MediaCodecDecoder: AMediaCodec_getInputBuffer() returned NULL");
		return false;
	}

	size_t size = _bitstream.size();
	if (size > bufsize) {
		ms_error("Cannot copy the all the bitstream into the input buffer size : %zu and bufsize %zu", size, bufsize);
		size = min(size, bufsize);
	}
	memcpy(buf, _bitstream.data(), size);

	uint32_t flags = isPs ? BufferFlag::CodecConfig : BufferFlag::None;
	if (AMediaCodec_queueInputBuffer(_impl, iBufidx, 0, size, timestamp * 1000ULL, flags) != 0) {
		ms_error("MediaCodecDecoder: AMediaCodec_queueInputBuffer() had an exception");
		return false;
	}

	return true;
}

bool MediaCodecDecoder::isKeyFrame(const MSQueue *frame) const {
	for (const mblk_t *nalu = ms_queue_peek_first(frame); !ms_queue_end(frame, nalu); nalu = ms_queue_next(frame, nalu)) {
		_naluHeader->parse(nalu->b_rptr);
		if (_naluHeader->getAbsType().isKeyFramePart()) return true;
	}
	return false;
}

} // namespace mediastreamer
