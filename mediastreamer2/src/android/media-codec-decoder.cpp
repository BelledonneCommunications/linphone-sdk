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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include <jni.h>
#include <media/NdkMediaFormat.h>
#include <ortp/b64.h>
#include <ortp/str_utils.h>

#include "mediastreamer2/formats.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"

#include "android_mediacodec.h"
#include "h26x-utils.h"
#include "media-codec-decoder.h"

using namespace b64;
using namespace mediastreamer;
using namespace std;

namespace mediastreamer {

MediaCodecDecoder::MediaCodecDecoder(const std::string &mime): _vsize({0, 0}) {
	try {
		_bufAllocator = ms_yuv_buf_allocator_new();
		createImpl(mime);
	} catch (const runtime_error &e) {
		ms_error("MSMediaCodecH264Dec: %s", e.what());
	}
}

MediaCodecDecoder::~MediaCodecDecoder() {
	if (_impl) AMediaCodec_delete(_impl);
	ms_yuv_buf_allocator_free(_bufAllocator);
}

void MediaCodecDecoder::flush() {
	if (_impl) AMediaCodec_flush(_impl);
	_pendingFrames = 0;
}

bool MediaCodecDecoder::feed(const std::vector<uint8_t> &encodedFrame, uint64_t timestamp) {
	if (_impl == nullptr) return false;

	ssize_t iBufidx = AMediaCodec_dequeueInputBuffer(_impl, _timeoutUs);
	if (iBufidx < 0) {
		ms_error("MSMediaCodecH264Dec: %s.", iBufidx == -1 ? "no buffer available for queuing this frame ! Decoder is too slow" : "AMediaCodec_dequeueInputBuffer() had an exception");
		return false;
	}

	size_t bufsize;
	uint8_t *buf = AMediaCodec_getInputBuffer(_impl, iBufidx, &bufsize);
	if (buf == nullptr) {
		ms_error("MSMediaCodecH264Dec: AMediaCodec_getInputBuffer() returned NULL");
		return false;
	}

	size_t size = encodedFrame.size();
	if (size > bufsize) {
		ms_error("Cannot copy the all the bitstream into the input buffer size : %zu and bufsize %zu", size, bufsize);
		size = min(size, bufsize);
	}
	memcpy(buf, encodedFrame.data(), size);

	if (AMediaCodec_queueInputBuffer(_impl, iBufidx, 0, size, timestamp, 0) != 0) {
		ms_error("MSMediaCodecH264Dec: AMediaCodec_queueInputBuffer() had an exception");
		return false;
	}
	_pendingFrames++;

	return true;
}

mblk_t *MediaCodecDecoder::fetch() {
	mblk_t *om = nullptr;
	AMediaImage image = {0};
	int dst_pix_strides[4] = {1, 1, 1, 1};
	MSRect dst_roi = {0};
	AMediaCodecBufferInfo info;
	ssize_t oBufidx = -1;

	if (_impl == nullptr || _pendingFrames <= 0) goto end;

	oBufidx = AMediaCodec_dequeueOutputBuffer(_impl, &info, _timeoutUs);
	if (oBufidx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
		ms_message("MSMediaCodecH264Dec: output format has changed.");
		oBufidx = AMediaCodec_dequeueOutputBuffer(_impl, &info, _timeoutUs);
	}

	if (oBufidx < 0) {
		if (oBufidx == AMEDIA_ERROR_UNKNOWN) {
			ms_error("MSMediaCodecH264Dec: AMediaCodec_dequeueOutputBuffer() had an exception");
		} else if (oBufidx != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
			ms_error("MSMediaCodecH264Dec: unknown error while dequeueing an output buffer (oBufidx=%zd)", oBufidx);
		}
		goto end;
	}

	_pendingFrames--;

	if (AMediaCodec_getOutputImage(_impl, oBufidx, &image) <= 0) {
		ms_error("AMediaCodec_getOutputImage() failed");
		goto end;
	}

	_vsize.width = dst_roi.w = image.crop_rect.w;
	_vsize.height = dst_roi.h = image.crop_rect.h;

	MSPicture pic;
	om = ms_yuv_buf_allocator_get(_bufAllocator, &pic, _vsize.width, _vsize.height);
	ms_yuv_buf_copy_with_pix_strides(image.buffers, image.row_strides, image.pixel_strides, image.crop_rect,
										pic.planes, pic.strides, dst_pix_strides, dst_roi);
	AMediaImage_close(&image);

end:
	if (oBufidx >= 0) AMediaCodec_releaseOutputBuffer(_impl, oBufidx, FALSE);
	return om;
}

void MediaCodecDecoder::createImpl(const std::string &mime) {
	media_status_t status = AMEDIA_OK;
	ostringstream errMsg;

	if ((_impl = AMediaCodec_createDecoderByType(mime.c_str())) == nullptr) {
		throw runtime_error("could not create MediaCodec");
	}

	AMediaFormat *format = AMediaFormat_new();
	AMediaFormat_setString(format, "mime", mime.c_str());
	AMediaFormat_setInt32(format, "color-format", 0x7f420888);
	AMediaFormat_setInt32(format, "max-width", 1920);
	AMediaFormat_setInt32(format, "max-height", 1920);

	if ((status = AMediaCodec_configure(_impl, format, nullptr, nullptr, 0)) != AMEDIA_OK) {
		errMsg << "configuration failure: " << int(status);
		goto end;
	}

	if ((status = AMediaCodec_start(_impl)) != AMEDIA_OK) {
		errMsg << "starting failure: " << int(status);
		goto end;
	}

end:
	AMediaFormat_delete(format);
	if (status != AMEDIA_OK) {
		AMediaCodec_delete(_impl);
		_impl = nullptr;
		throw runtime_error(errMsg.str());
	}
}

MediaCodecDecoderFilterImpl::MediaCodecDecoderFilterImpl(MSFilter *f, const std::string &mimeType, NalUnpacker *unpacker, H26xParameterSetsStore *psStore):
	_f(f), _mimeType(mimeType), _unpacker(unpacker), _psStore(psStore) {

	ms_message("MSMediaCodecH264Dec initialization");
	_vsize.width = 0;
	_vsize.height = 0;
	ms_average_fps_init(&_fps, " H264 decoder: FPS: %f");
	_bufAllocator = ms_yuv_buf_allocator_new();
}

MediaCodecDecoderFilterImpl::~MediaCodecDecoderFilterImpl() {
	if (_codec) {
		AMediaCodec_stop(_codec);
		AMediaCodec_delete(_codec);
	}
	ms_yuv_buf_allocator_free(_bufAllocator);
}

void MediaCodecDecoderFilterImpl::preprocess() {
	_firstImageDecoded = false;
}

void MediaCodecDecoderFilterImpl::process() {
	bool request_pli = false;
	MSQueue nalus;
	ms_queue_init(&nalus);

	while (mblk_t *im = ms_queue_get(_f->inputs[0])) {
		uint8_t *buf = nullptr;
		ssize_t iBufidx;

		NalUnpacker::Status unpacking_ret = _unpacker->unpack(im, &nalus);

		if (!unpacking_ret.frameAvailable) continue;

		if (unpacking_ret.frameCorrupted) {
			ms_warning("MSMediaCodecH264Dec: corrupted frame");
			request_pli = true;
			if (_freezeOnError) {
				ms_queue_flush(&nalus);
				_needKeyFrame = true;
				continue;
			}
		}

		H26xUtils::nalusToByteStream(&nalus, _bitstream);

		if (_codec == nullptr) {
			initMediaCodec();
		}

		if (_codec == nullptr) continue;

		/*First put our H264 bitstream into the decoder*/
		iBufidx = AMediaCodec_dequeueInputBuffer(_codec, _timeoutUs);

		if (iBufidx >= 0) {
			struct timespec ts;
			size_t bufsize;
			buf = AMediaCodec_getInputBuffer(_codec, iBufidx, &bufsize);

			if (buf == nullptr) {
				ms_error("MSMediaCodecH264Dec: AMediaCodec_getInputBuffer() returned NULL");
				continue;
			}
			clock_gettime(CLOCK_MONOTONIC, &ts);

			size_t size = _bitstream.size();
			if (size > bufsize) {
				ms_error("Cannot copy the all the bitstream into the input buffer size : %zu and bufsize %zu", size, bufsize);
				size = MIN(size, bufsize);
			}
			memcpy(buf, _bitstream.data(), size);

			if (_needKeyFrame) {
				ms_message("MSMediaCodecH264Dec: fresh I-frame submitted to the decoder");
				_needKeyFrame = false;
			}

			if (AMediaCodec_queueInputBuffer(_codec, iBufidx, 0, size, (ts.tv_nsec / 1000) + 10000LL, 0) == 0) {
				_pendingFrames++;
			} else {
				ms_error("MSMediaCodecH264Dec: AMediaCodec_queueInputBuffer() had an exception");
				flush(false);
				request_pli = true;
				continue;
			}
		} else if (iBufidx == -1) {
			ms_error("MSMediaCodecH264Dec: no buffer available for queuing this frame ! Decoder is too slow.");
			/*
			 * This is a problematic case because we can't wait the decoder to be ready, otherwise we'll freeze the entire
			 * video MSTicker thread.
			 * We have no other option to drop the frame, and retry later, but with an I-frame of course.
			 **/
			request_pli = true;
			_needKeyFrame = true;
			continue;
		} else {
			ms_error("MSMediaCodecH264Dec: AMediaCodec_dequeueInputBuffer() had an exception");
			flush(false);
			request_pli = true;
			continue;
		}
		_packetNum++;
	}

	/*secondly try to get decoded frames from the decoder, this is performed every tick*/
	ssize_t oBufidx = -1;
	AMediaCodecBufferInfo info;

	if (_pendingFrames <= 0) goto end;

	while ((oBufidx = AMediaCodec_dequeueOutputBuffer(_codec, &info, _timeoutUs)) >= 0) {
		mblk_t *om = nullptr;

		_pendingFrames--;

		AMediaImage image = {0};
		if (AMediaCodec_getOutputImage(_codec, oBufidx, &image)) {
			int dst_pix_strides[4] = {1, 1, 1, 1};
			MSRect dst_roi = {0, 0, image.crop_rect.w, image.crop_rect.h};

			_vsize.width = image.crop_rect.w;
			_vsize.height = image.crop_rect.h;

			MSPicture pic;
			om = ms_yuv_buf_allocator_get(_bufAllocator, &pic, _vsize.width, _vsize.height);
			ms_yuv_buf_copy_with_pix_strides(image.buffers, image.row_strides, image.pixel_strides, image.crop_rect,
			                                 pic.planes, pic.strides, dst_pix_strides, dst_roi);
			AMediaImage_close(&image);
		} else {
			ms_error("AMediaCodec_getOutputImage() failed");
		}

		if (om) {

			if (!_firstImageDecoded) {
				ms_message("First frame decoded %ix%i", _vsize.width, _vsize.height);
				_firstImageDecoded = true;
				ms_filter_notify_no_arg(_f, MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
			}

			ms_average_fps_update(&_fps, _f->ticker->time);
			ms_queue_put(_f->outputs[0], om);
		}

		AMediaCodec_releaseOutputBuffer(_codec, oBufidx, FALSE);
	}

	if (oBufidx == AMEDIA_ERROR_UNKNOWN) {
		ms_error("MSMediaCodecH264Dec: AMediaCodec_dequeueOutputBuffer() had an exception");
		flush(false);
		request_pli = true;
	}

end:
	if (_avpfEnabled && request_pli) {
		ms_filter_notify_no_arg(_f, MS_VIDEO_DECODER_SEND_PLI);
	}
}

void MediaCodecDecoderFilterImpl::postprocess() {
	_unpacker->reset();
}

void MediaCodecDecoderFilterImpl::resetFirstImage() {
	_firstImageDecoded = false;
}

MSVideoSize MediaCodecDecoderFilterImpl::getVideoSize() const {
	return _firstImageDecoded ? _vsize : MS_VIDEO_SIZE_UNKNOWN;
}

float MediaCodecDecoderFilterImpl::getFps() const {
	return ms_average_fps_get(&_fps);
}

const MSFmtDescriptor *MediaCodecDecoderFilterImpl::getOutFmt() const {
	return ms_factory_get_video_format(_f->factory, "YUV420P", ms_video_size_make(_vsize.width, _vsize.height), 0, nullptr);
}

void MediaCodecDecoderFilterImpl::enableAvpf(bool enable) {
	_avpfEnabled = enable;
}

void MediaCodecDecoderFilterImpl::enableFreezeOnError(bool enable) {
	_freezeOnError = enable;
	ms_message("MSMediaCodecH264Dec: freeze on error %s", _freezeOnError ? "enabled" : "disabled");
}

media_status_t MediaCodecDecoderFilterImpl::initMediaCodec() {
	AMediaFormat *format;
	media_status_t status = AMEDIA_OK;

	if (_codec == nullptr) {
		_codec = AMediaCodec_createDecoderByType(_mimeType.c_str());
		if (_codec == nullptr) {
			ms_error("MSMediaCodecH264Dec: could not create MediaCodec");
			return AMEDIA_ERROR_UNKNOWN;
		}
	}

	format = AMediaFormat_new();
	AMediaFormat_setString(format, "mime", _mimeType.c_str());
	AMediaFormat_setInt32(format, "color-format", 0x7f420888);
	AMediaFormat_setInt32(format, "max-width", 1920);
	AMediaFormat_setInt32(format, "max-height", 1920);

	if ((status = AMediaCodec_configure(_codec, format, nullptr, nullptr, 0)) != AMEDIA_OK) {
		ms_error("MSMediaCodecH264Dec: configuration failure: %i", (int)status);
		goto end;
	}

	if ((status = AMediaCodec_start(_codec)) != AMEDIA_OK) {
		ms_error("MSMediaCodecH264Dec: starting failure: %i", (int)status);
		goto end;
	}
	_needKeyFrame = true;

end:
	AMediaFormat_delete(format);
	return status;
}

void MediaCodecDecoderFilterImpl::flush(bool with_reset) {
	if (with_reset || (AMediaCodec_flush(_codec) != 0)) {
		AMediaCodec_reset(_codec);
		initMediaCodec();
	}
	_needKeyFrame = true;
	_pendingFrames = 0;
}

} // namespace mediastreamer
