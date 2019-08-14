/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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
		ms_error("MediaCodecDecoder: %s. Dumping decoding context", e.what());
		if (_impl) AMediaCodec_delete(_impl);
		_impl = nullptr;
	}
}

MediaCodecDecoder::~MediaCodecDecoder() {
	if (_impl) AMediaCodec_delete(_impl);
	if (_format) AMediaFormat_delete(_format);
	if (_bufAllocator) ms_yuv_buf_allocator_free(_bufAllocator);
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

	if (_impl == nullptr) {
		ms_queue_flush(encodedFrame);
		return true;
	}

	_psStore->extractAllPs(encodedFrame);
	if (_psStore->hasNewParameters()) {
		ms_message("MediaCodecDecoder: new paramter sets received");
		_needParameters = true;
		_psStore->acknowlege();
	}

	if (_needParameters && _psStore->psGatheringCompleted()) {
		MSQueue parameters;
		ms_queue_init(&parameters);
		_psStore->fetchAllPs(&parameters);
		if (!setParameterSets(&parameters, timestamp)) {
			ms_error("MediaCodecDecoder: waiting for parameter sets.");
			goto clean;
		}
	}

	if (_needParameters) {
		ms_error("MediaCodecDecoder: missing parameter sets");
		goto clean;
	}

	if (_needKeyFrame) {
		if (!isKeyFrame(encodedFrame)) {
			ms_error("MediaCodecDecoder: waiting for key frame.");
			goto clean;
		}
		ms_message("MediaCodecDecoder: key frame received");
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
	while (oBufidx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED || oBufidx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
		ms_message("MediaCodecDecoder: %s", codecInfoToString(oBufidx).c_str());
		if (oBufidx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
			AMediaFormat *format = AMediaCodec_getOutputFormat(_impl);
			ms_message("MediaCodecDecoder: new format:\n%s", AMediaFormat_toString(format));
			AMediaFormat_delete(format);
		}
		oBufidx = AMediaCodec_dequeueOutputBuffer(_impl, &info, _timeoutUs);
	}

	if (oBufidx < 0) {
		if (oBufidx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
			return noFrameAvailable;
		} else {
			ms_error("MediaCodecDecoder: error while dequeueing an output buffer: %s", codecInfoToString(oBufidx).c_str());
			if (oBufidx == AMEDIA_ERROR_UNKNOWN) {
				try {
					resetImpl();
					startImpl();
				} catch (const runtime_error &e) {
					ms_error("MediaCodecDecoder: decoding session couldn't been started. The session is definitively lost !");
				}
			}
			return decodingFailure;
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
	ms_message("MediaCodecDecoder: starting decoder with following parameters:\n%s", AMediaFormat_toString(_format));
	if ((status = AMediaCodec_configure(_impl, _format, nullptr, nullptr, 0)) != AMEDIA_OK) {
		errMsg << "configuration failure: " << int(status);
		throw runtime_error(errMsg.str());
	}

	if ((status = AMediaCodec_start(_impl)) != AMEDIA_OK) {
		errMsg << "starting failure: " << int(status);
		throw runtime_error(errMsg.str());
	}

	ms_message("MediaCodecDecoder: decoder successfully started. In-force parameters:\n%s", AMediaFormat_toString(_format));
}

void MediaCodecDecoder::stopImpl() noexcept {
	ms_message("MediaCodecDecoder: stopping decoder");
	AMediaCodec_stop(_impl);
}

void MediaCodecDecoder::resetImpl() noexcept {
	ms_message("MediaCodecDecoder: reseting decoder");
	if (AMediaCodec_reset(_impl) != AMEDIA_OK) {
		ms_error("MediaCodecDecoder: decoder couldn't been reset. Throwing decoding session out");
		AMediaCodec_delete(_impl);
		_impl = AMediaCodec_createDecoderByType(_mime.c_str());
		if (_impl == nullptr) ms_error("MediaCodecDecoder: couldn't recreate decoding session. The decoding session is definitively lost !");
	}
}

bool MediaCodecDecoder::feed(MSQueue *encodedFrame, uint64_t timestamp, bool isPs) {
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
		ms_error("MediaCodecDecoder: cannot copy the all the bitstream into the input buffer size : %zu and bufsize %zu", size, bufsize);
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

std::string MediaCodecDecoder::codecInfoToString(ssize_t codecStatusCode) {
	switch (codecStatusCode) {
		case AMEDIA_ERROR_UNKNOWN:
			return "MediaCodec had an exception";
		case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
			return "output buffers has changed";
		case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED:
			return "output format has changed";
		case AMEDIACODEC_INFO_TRY_AGAIN_LATER:
			return "no output buffer available";
		default:
			break;
	}
	ostringstream os;
	if (codecStatusCode >= 0) {
		os << "unqueued buffer (index=" << codecStatusCode << ")";
	} else {
		os << "unknown error (" << codecStatusCode << ")";
	}
	return os.str();
}

} // namespace mediastreamer
