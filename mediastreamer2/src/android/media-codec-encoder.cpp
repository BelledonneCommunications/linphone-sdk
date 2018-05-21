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

#include <jni.h>
#include <ortp/b64.h>

#include "mediastreamer2/msjava.h"
#include "mediastreamer2/msticker.h"

#include "android_mediacodec.h"
#include "h26x-utils.h"
#include "media-codec-encoder.h"

#define MS_MEDIACODECH264_CONF(required_bitrate, bitrate_limit, resolution, fps, ncpus) \
{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H }, fps, ncpus, nullptr }

static const MSVideoConfiguration mediaCodecH264_conf_list[] = {
	MS_MEDIACODECH264_CONF(2048000, 1000000,       UXGA, 25,  2),
	MS_MEDIACODECH264_CONF(1024000, 5000000, SXGA_MINUS, 25,  2),
	MS_MEDIACODECH264_CONF(1024000, 5000000,       720P, 30,  2),
	MS_MEDIACODECH264_CONF( 750000, 2048000,        XGA, 25,  2),
	MS_MEDIACODECH264_CONF( 500000, 1024000,       SVGA, 15,  2),
	MS_MEDIACODECH264_CONF( 600000, 3000000,        VGA, 30,  2),
	MS_MEDIACODECH264_CONF( 400000,  800000,        VGA, 15,  2),
	MS_MEDIACODECH264_CONF( 128000,  512000,        CIF, 15,  1),
	MS_MEDIACODECH264_CONF( 100000,  380000,       QVGA, 15,  1),
	MS_MEDIACODECH264_CONF(      0,  170000,       QCIF, 10,  1),
};

using namespace mediastreamer;

namespace mediastreamer {

// Public methods
MediaCodecEncoderFilterImpl::MediaCodecEncoderFilterImpl(MSFilter *f, NalPacker *packer): _f(f), _packer(packer) {
	_vconfList = mediaCodecH264_conf_list;
	_vconf = ms_video_find_best_configuration_for_size(_vconfList, MS_VIDEO_SIZE_CIF, ms_factory_get_cpu_count(f->factory));
	ms_video_starter_init(&_starter);

	_packer->setPacketizationMode(NalPacker::NonInterleavedMode);
	_packer->enableAggregation(false);

	/*we shall allocate the MediaCodec encoder the sooner as possible and before the decoder, because
		* on some phones hardware encoder and decoders can't be allocated at the same time.
		* */
	allocEncoder();
}

MediaCodecEncoderFilterImpl::~MediaCodecEncoderFilterImpl() {
	if (_codec) AMediaCodec_delete(_codec);
}

void MediaCodecEncoderFilterImpl::preprocess() {
	encConfigure();
	ms_video_starter_init(&_starter);
	ms_iframe_requests_limiter_init(&_iframeLimiter, 1000);
}

void MediaCodecEncoderFilterImpl::process() {
	if (_codecLost && (_f->ticker->time % 5000 == 0)){
		if (encConfigure() != 0){
			ms_error("MSMediaCodecH264Enc: AMediaCodec_reset() was not sufficient, will recreate the encoder in a moment...");
			AMediaCodec_delete(_codec);
			_codec = nullptr;
			_codecLost = true;
		}
	}

	if (!_codecStarted || _codecLost) {
		ms_queue_flush(_f->inputs[0]);
		return;
	}

	/*First queue input image*/
	if (mblk_t *im = ms_queue_peek_last(_f->inputs[0])) {
		MSPicture pic;
		if (ms_yuv_buf_init_from_mblk(&pic, im) == 0) {
			if (ms_iframe_requests_limiter_iframe_requested(&_iframeLimiter, _f->ticker->time) ||
					(!_avpfEnabled && ms_video_starter_need_i_frame(&_starter, _f->ticker->time))) {
				AMediaFormat *afmt = AMediaFormat_new();
				/*Force a key-frame*/
				AMediaFormat_setInt32(afmt, "request-sync", 0);
				AMediaCodec_setParams(_codec, afmt);
				AMediaFormat_delete(afmt);
				ms_error("MSMediaCodecH264Enc: I-frame requested to MediaCodec");
				ms_iframe_requests_limiter_notify_iframe_sent(&_iframeLimiter, _f->ticker->time);
			}

			ssize_t ibufidx = AMediaCodec_dequeueInputBuffer(_codec, _timeoutUs);
			if (ibufidx >= 0) {
				size_t bufsize;
				if (uint8_t *buf = AMediaCodec_getInputBuffer(_codec, ibufidx, &bufsize)){
					AMediaImage image;
					if (AMediaCodec_getInputImage(_codec, ibufidx, &image)) {
						if (image.format == 35 /* YUV_420_888 */) {
							MSRect src_roi = {0, 0, pic.w, pic.h};
							int src_pix_strides[4] = {1, 1, 1, 1};
							ms_yuv_buf_copy_with_pix_strides(pic.planes, pic.strides, src_pix_strides, src_roi, image.buffers, image.row_strides, image.pixel_strides, image.crop_rect);
							bufsize = image.row_strides[0] * image.height * 3 / 2;
						} else {
							ms_error("%s: encoder requires non YUV420 format", _f->desc->name);
						}
						AMediaImage_close(&image);
					}
					AMediaCodec_queueInputBuffer(_codec, ibufidx, 0, bufsize, _f->ticker->time * 1000, 0);
					if (!_firstBufferQueued){
						_firstBufferQueued = true;
						ms_message("MSMediaCodecH264Enc: first frame to encode queued (size: %ix%i)", pic.w, pic.h);
					}
				}else{
					ms_error("MSMediaCodecH264Enc: obtained InputBuffer, but no address.");
				}
			} else if (ibufidx == AMEDIA_ERROR_UNKNOWN) {
				ms_error("MSMediaCodecH264Enc: AMediaCodec_dequeueInputBuffer() had an exception");
			}
		}
	}
	ms_queue_flush(_f->inputs[0]);

	if (!_firstBufferQueued)
		return;

	/*Second, dequeue possibly pending encoded frames*/
	AMediaCodecBufferInfo info;
	ssize_t obufidx;
	while ((obufidx = AMediaCodec_dequeueOutputBuffer(_codec, &info, _timeoutUs)) >= 0) {
		size_t bufsize;
		if (uint8_t *buf = AMediaCodec_getOutputBuffer(_codec, obufidx, &bufsize)) {
			MSQueue nalus;

			ms_queue_init(&nalus);
			byteStreamToNalus(buf + info.offset, info.size, &nalus);

			if (!ms_queue_empty(&nalus)) {
				onFrameEncodedHook(&nalus);
				_packer->pack(&nalus, _f->outputs[0], _f->ticker->time * 90LL);

				if (_framenum == 0) {
					ms_video_starter_first_frame(&_starter, _f->ticker->time);
				}
				_framenum++;
			}else{
				ms_error("MSMediaCodecH264Enc: no NALUs in buffer obtained from MediaCodec");
			}
		}else{
			ms_error("MSMediaCodecH264Enc: AMediaCodec_getOutputBuffer() returned nullptr");
		}
		AMediaCodec_releaseOutputBuffer(_codec, obufidx, FALSE);
	}

	if (obufidx == AMEDIA_ERROR_UNKNOWN) {
		ms_error("MSMediaCodecH264Enc: AMediaCodec_dequeueOutputBuffer() had an exception, MediaCodec is lost");
		/* TODO: the MediaCodec is irrecoverabely crazy at this point. We should probably use AMediacCodec_reset() but this method is not wrapped yet.*/
		_codecLost = true;
		AMediaCodec_reset(_codec);
	}
}

void MediaCodecEncoderFilterImpl::postprocess() {
	_packer->flush();

	if (_codec) {
		if (_codecStarted){
			AMediaCodec_flush(_codec);
			AMediaCodec_stop(_codec);
			//It is preferable to reset the encoder, otherwise it may not accept a new configuration while returning in preprocess().
			//This was observed at least on Moto G2, with qualcomm encoder.
			AMediaCodec_reset(_codec);
			_codecStarted = false;
		}
	}
}

int MediaCodecEncoderFilterImpl::getBitrate() const {
	return _vconf.required_bitrate;
}

void MediaCodecEncoderFilterImpl::setBitrate(int br) {
	if (_codecStarted) {
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

	if (_codecStarted){
		AMediaFormat *afmt = AMediaFormat_new();
		/*Update the output bitrate*/
		ms_filter_lock(_f);
		AMediaFormat_setInt32(afmt, "video-bitrate", _vconf.required_bitrate);
		AMediaCodec_setParams(_codec, afmt);
		AMediaFormat_delete(afmt);
		ms_filter_unlock(_f);
	}
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
	_vconfList = vconfs ? vconfs : mediaCodecH264_conf_list;
}




// Private methods
media_status_t MediaCodecEncoderFilterImpl::allocEncoder(){
	if (!_codec){
		_codec = AMediaCodec_createEncoderByType("video/avc");
		if (!_codec) {
			ms_error("MSMediaCodecH264Enc: could not create MediaCodec");
			return AMEDIA_ERROR_UNKNOWN;
		}
	}
	return AMEDIA_OK;
}

media_status_t MediaCodecEncoderFilterImpl::tryColorFormat(AMediaFormat *format, unsigned value) {
	AMediaFormat_setInt32(format, "color-format", value);
	media_status_t status = AMediaCodec_configure(_codec, format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
	if (status != 0){
		ms_message("AMediaCodec_configure() failed with error %i for format %u", (int)status, value);
	}
	return status;
}

int MediaCodecEncoderFilterImpl::encConfigure() {
	media_status_t status = allocEncoder();
	if (status != 0) return status;

	_codecLost = false;
	_codecStarted = false;

	AMediaFormat *format = AMediaFormat_new();
	AMediaFormat_setString(format, "mime", "video/avc");
	AMediaFormat_setInt32(format, "width", _vconf.vsize.width);
	AMediaFormat_setInt32(format, "height", _vconf.vsize.height);
	AMediaFormat_setInt32(format, "i-frame-interval", 20);
	AMediaFormat_setInt32(format, "bitrate", (_vconf.required_bitrate * 9)/10); /*take a margin*/
	AMediaFormat_setInt32(format, "frame-rate", _vconf.fps);
	AMediaFormat_setInt32(format, "bitrate-mode", 1);
	AMediaFormat_setInt32(format, "profile", 1); // AVCProfileBaseline
	AMediaFormat_setInt32(format, "level", 1024); // AVCLevel32

	ms_message("MSMediaCodecH264Enc: AMediaImage is available.");
	status = tryColorFormat(format, 0x7f420888);/*the new "flexible YUV", appeared in API23*/

	if (status != 0) {
		ms_error("MSMediaCodecH264Enc: Could not configure encoder.");
	} else {
		int32_t color_format;

		if (!AMediaFormat_getInt32(format, "color-format", &color_format)) {
			color_format = -1;
		}
		ms_message("MSMediaCodecH264Enc: encoder successfully configured. size=%ix%i, color-format=%d",
					_vconf.vsize.width,  _vconf.vsize.height, color_format);
		if ((status = AMediaCodec_start(_codec)) != AMEDIA_OK) {
			ms_error("MSMediaCodecH264Enc: Could not start encoder.");
		} else {
			ms_message("MSMediaCodecH264Enc: encoder successfully started");
			_codecStarted = true;
		}
	}

	AMediaFormat_delete(format);
	_firstBufferQueued = false;
	return status;
}

}
