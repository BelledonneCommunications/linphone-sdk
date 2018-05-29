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

#include <cstring>
#include <stdexcept>

#include <jni.h>
#include <ortp/b64.h>

#include "mediastreamer2/msjava.h"
#include "mediastreamer2/msticker.h"

#include "android_mediacodec.h"
#include "h26x-utils.h"
#include "media-codec-encoder.h"

using namespace mediastreamer;
using namespace std;

namespace mediastreamer {

MediaCodecEncoder::MediaCodecEncoder(const std::string &mime, int profile, int level, H26xParameterSetsInserter *psInserter):
	_mime(mime), _profile(profile), _level(level), _psInserter(psInserter) {

	try {
		_vsize.width = 0;
		_vsize.height = 0;
		// We shall allocate the MediaCodec encoder the sooner as possible and before the decoder, because
		// some phones hardware encoder and decoders can't be allocated at the same time.
		createImpl();
	} catch (const runtime_error &e) {
		ms_error("MSMediaCodecH264Enc: %s", e.what());
	}
}

MediaCodecEncoder::~MediaCodecEncoder() {
	if (_impl) AMediaCodec_delete(_impl);
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
		if (_isRunning) {
			ms_warning("MediaCodecEncoder: encoder already started");
			return;
		}
		configureImpl();
		if (AMediaCodec_start(_impl) != AMEDIA_OK) {
			throw runtime_error("could not start encoder.");
		}
		_firstBufferQueued = false;
		_isRunning = true;
		ms_message("MSMediaCodecH264Enc: encoder successfully started");
	} catch (const runtime_error &e) {
		ms_error("MSMediaCodecH264Enc: %s", e.what());
	}
}

void MediaCodecEncoder::stop() {
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
}

void MediaCodecEncoder::feed(mblk_t *rawData, uint64_t time) {
	if (!_isRunning) {
		ms_error("MSMediaCodecH264Enc: encoder not running. Dropping buffer.");
		freemsg(rawData);
		return;
	}

	if (_recoveryMode && time - _lastTryTime >= 5000) {
		try {
			if (_impl == nullptr) createImpl();
			configureImpl();
			_recoveryMode = false;
		} catch (const runtime_error &e) {
			ms_error("MSMediaCodecH264Enc: %s", e.what());
			ms_error("MSMediaCodecH264Enc: AMediaCodec_reset() was not sufficient, will recreate the encoder in a moment...");
			AMediaCodec_delete(_impl);
			_lastTryTime = time;
		}
	}

	MSPicture pic;
	ms_yuv_buf_init_from_mblk(&pic, rawData);

	ssize_t ibufidx = AMediaCodec_dequeueInputBuffer(_impl, _timeoutUs);
	if (ibufidx < 0) {
		if (ibufidx == AMEDIA_ERROR_UNKNOWN) ms_error("MSMediaCodecH264Enc: AMediaCodec_dequeueInputBuffer() had an exception");
		return;
	}

	size_t bufsize;
	uint8_t *buf;
	if ((buf = AMediaCodec_getInputBuffer(_impl, ibufidx, &bufsize)) == nullptr) {
		ms_error("MSMediaCodecH264Enc: obtained InputBuffer, but no address.");
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
			ms_error("MSMediaCodecH264Enc: encoder requires non YUV420 format");
		}
		AMediaImage_close(&image);
	}

	AMediaCodec_queueInputBuffer(_impl, ibufidx, 0, bufsize, time, 0);
	if (!_firstBufferQueued) {
		_firstBufferQueued = true;
		ms_message("MSMediaCodecH264Enc: first frame to encode queued (size: %ix%i)", pic.w, pic.h);
	}
}

void MediaCodecEncoder::fetch(MSQueue *encodedData) {
	if (!_isRunning || _recoveryMode) return;

	MSQueue outq;
	ms_queue_init(&outq);

	AMediaCodecBufferInfo info;
	ssize_t obufidx;
	while ((obufidx = AMediaCodec_dequeueOutputBuffer(_impl, &info, _timeoutUs)) >= 0) {
		uint8_t *buf;
		size_t bufsize;
		if ((buf = AMediaCodec_getOutputBuffer(_impl, obufidx, &bufsize)) == nullptr) {
			ms_error("MSMediaCodecH264Enc: AMediaCodec_getOutputBuffer() returned nullptr");
			break;
		}
		byteStreamToNalus(buf + info.offset, info.size, &outq);
		AMediaCodec_releaseOutputBuffer(_impl, obufidx, FALSE);
	}
	if (obufidx == AMEDIA_ERROR_UNKNOWN) {
		ms_error("MSMediaCodecH264Enc: AMediaCodec_dequeueOutputBuffer() had an exception, MediaCodec is lost");
		// MediaCodec need to be reset  at this point because it may have become irrevocably crazy.
		AMediaCodec_reset(_impl);
		_recoveryMode = true;
		_lastTryTime = 0;
	}

	_psInserter->process(&outq, encodedData);
}

void MediaCodecEncoder::createImpl() {
	_impl = AMediaCodec_createEncoderByType(_mime.c_str());
	if (_impl == nullptr) {
		throw runtime_error("could not create MediaCodec");
	}
}

void MediaCodecEncoder::configureImpl() {
	int32_t colorFormat = 0x7f420888;

	AMediaFormat *format = AMediaFormat_new();
	AMediaFormat_setString(format, "mime", _mime.c_str());
	AMediaFormat_setInt32(format, "width", _vsize.width);
	AMediaFormat_setInt32(format, "height", _vsize.height);
	AMediaFormat_setInt32(format, "i-frame-interval", 20);
	AMediaFormat_setInt32(format, "bitrate", (_bitrate * 9)/10); // take a margin
	AMediaFormat_setInt32(format, "frame-rate", _fps);
	AMediaFormat_setInt32(format, "bitrate-mode", 1); // VBR mode
	AMediaFormat_setInt32(format, "profile", _profile);
	AMediaFormat_setInt32(format, "level", _level);
	AMediaFormat_setInt32(format, "color-format", colorFormat);

	media_status_t status = AMediaCodec_configure(_impl, format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
	AMediaFormat_delete(format);

	if (status != 0) {
		throw runtime_error("could not configure encoder.");
	}

	ms_message("MSMediaCodecH264Enc: encoder successfully configured. size=%ix%i, color-format=%d",
				_vsize.width,  _vsize.height, colorFormat);
}

// Public methods
MediaCodecEncoderFilterImpl::MediaCodecEncoderFilterImpl(MSFilter *f, MediaCodecEncoder *encoder, NalPacker *packer, const MSVideoConfiguration *vconfs):
	_f(f), _encoder(encoder), _packer(packer), _vconfList(vconfs) {

	_vconf = ms_video_find_best_configuration_for_size(_vconfList, MS_VIDEO_SIZE_CIF, ms_factory_get_cpu_count(f->factory));
	ms_video_starter_init(&_starter);

	_packer->setPacketizationMode(NalPacker::NonInterleavedMode);
	_packer->enableAggregation(false);
}

void MediaCodecEncoderFilterImpl::preprocess() {
	_encoder->start();
	ms_video_starter_init(&_starter);
	ms_iframe_requests_limiter_init(&_iframeLimiter, 1000);
}

void MediaCodecEncoderFilterImpl::process() {
	/*First queue input image*/
	if (mblk_t *im = ms_queue_peek_last(_f->inputs[0])) {
		_encoder->feed(dupmsg(im), _f->ticker->time);
	}
	ms_queue_flush(_f->inputs[0]);

	/*Second, dequeue possibly pending encoded frames*/
	_encoder->fetch(_f->outputs[0]);
}

void MediaCodecEncoderFilterImpl::postprocess() {
	_packer->flush();
	_encoder->stop();
}

int MediaCodecEncoderFilterImpl::getBitrate() const {
	return _vconf.required_bitrate;
}

void MediaCodecEncoderFilterImpl::setBitrate(int br) {
	if (_encoder->isRunning()) {
		/* Encoding is already ongoing, do not change video size, only bitrate. */
		_vconf.required_bitrate = br;
		/* apply the new bitrate request to the running MediaCodec*/
		setVideoConfiguration(&_vconf);
	} else {
		MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_size_and_bitrate(_vconfList, _vconf.vsize, ms_factory_get_cpu_count(_f->factory),  br);
		setVideoConfiguration(&best_vconf);
	}
}

int MediaCodecEncoderFilterImpl::setVideoConfiguration(const MSVideoConfiguration *vconf) {
	if (vconf != &_vconf) memcpy(&_vconf, vconf, sizeof(MSVideoConfiguration));

	if (_vconf.required_bitrate > _vconf.bitrate_limit)
		_vconf.required_bitrate = _vconf.bitrate_limit;

	ms_message("Video configuration set: bitrate=%d bits/s, fps=%f, vsize=%dx%d", _vconf.required_bitrate, _vconf.fps, _vconf.vsize.width, _vconf.vsize.height);

	_encoder->setVideoSize(_vconf.vsize);
	_encoder->setFps(_vconf.fps);
	_encoder->setBitrate(_vconf.required_bitrate);

	return 0;
}

void MediaCodecEncoderFilterImpl::setFps(float  fps) {
	_vconf.fps = fps;
	setVideoConfiguration(&_vconf);
}

float MediaCodecEncoderFilterImpl::getFps() const {
	return _vconf.fps;
}

MSVideoSize MediaCodecEncoderFilterImpl::getVideoSize() const {
	return _vconf.vsize;
}

void MediaCodecEncoderFilterImpl::enableAvpf(bool enable) {
	_avpfEnabled = enable;
}

void MediaCodecEncoderFilterImpl::setVideoSize(const MSVideoSize &vsize) {
	MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_size(_vconfList, vsize, ms_factory_get_cpu_count(_f->factory));
	_vconf.vsize = vsize;
	_vconf.fps = best_vconf.fps;
	_vconf.bitrate_limit = best_vconf.bitrate_limit;
	setVideoConfiguration(&_vconf);
}

void MediaCodecEncoderFilterImpl::notifyPli() {
	ms_message("MSMediaCodecH264Enc: PLI requested");
	ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
}

void MediaCodecEncoderFilterImpl::notifyFir() {
	ms_message("MSMediaCodecH264Enc: FIR requested");
	ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
}

const MSVideoConfiguration *MediaCodecEncoderFilterImpl::getVideoConfiguratons() const {
	return _vconfList;
}

void MediaCodecEncoderFilterImpl::setVideoConfigurations(const MSVideoConfiguration *vconfs) {
	_vconfList = vconfs;
}

}
