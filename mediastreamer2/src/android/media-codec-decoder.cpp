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
		size_t size = _bitstream.size();
		//Initialize the video size
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

			if (size > bufsize) {
				ms_error("Cannot copy the all the bitstream into the input buffer size : %zu and bufsize %zu", size, bufsize);
				size = MIN(size, bufsize);
			}
			memcpy(buf, _bitstream.data(), _bitstream.size());

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
		size_t bufsize;
		mblk_t *om = nullptr;

		_pendingFrames--;

		uint8_t *buf = AMediaCodec_getOutputBuffer(_codec, oBufidx, &bufsize);
		if (buf == nullptr) {
			ms_error("MSMediaCodecH264Dec: AMediaCodec_getOutputBuffer() returned NULL");
			continue;
		}

		AMediaImage image;
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
	AMediaFormat_setInt32(format, "width", 1920);
	AMediaFormat_setInt32(format, "height", 1080);

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
