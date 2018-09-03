/*
 Mediastreamer2 h26x-encoder-filter.cpp
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

#include "h26x-encoder-filter.h"

using namespace std;

namespace mediastreamer {

H26xEncoderFilter::H26xEncoderFilter(MSFilter *f, VideoEncoder *encoder, NalPacker *packer, const MSVideoConfiguration *defaultVConfList):
	EncoderFilter(f), _encoder(encoder), _packer(packer), _defaultVConfList(defaultVConfList) {

	setVideoConfigurations(nullptr);
	_vconf = ms_video_find_best_configuration_for_size(_vconfList, MS_VIDEO_SIZE_CIF, ms_factory_get_cpu_count(f->factory));
	ms_video_starter_init(&_starter);

	_packer->setPacketizationMode(NalPacker::NonInterleavedMode);
	_packer->enableAggregation(false);
}

void H26xEncoderFilter::preprocess() {
	_encoder->start();
	ms_video_starter_init(&_starter);
	ms_iframe_requests_limiter_init(&_iframeLimiter, 1000);
}

void H26xEncoderFilter::process() {
	/*First queue input image*/
	if (mblk_t *im = ms_queue_peek_last(getInput(0))) {
		bool requestIFrame = false;
		if (ms_iframe_requests_limiter_iframe_requested(&_iframeLimiter, getTime()) ||
				(!_avpfEnabled && ms_video_starter_need_i_frame(&_starter, getTime()))) {
			ms_message("MediaCodecEncoder: requesting I-frame to the encoder.");
			requestIFrame = true;
			ms_iframe_requests_limiter_notify_iframe_sent(&_iframeLimiter, getTime());
		}
		_encoder->feed(dupmsg(im), getTime(), requestIFrame);
	}
	ms_queue_flush(getInput(0));

	/*Second, dequeue possibly pending encoded frames*/
	MSQueue nalus;
	ms_queue_init(&nalus);
	while (_encoder->fetch(&nalus)) {
		if (!_firstFrameDecoded) {
			_firstFrameDecoded = true;
			ms_video_starter_first_frame(&_starter, getTime());
		}
		_packer->pack(&nalus, getOutput(0), static_cast<uint32_t>(getTime() * 90LL));
	}
}

void H26xEncoderFilter::postprocess() {
	_packer->flush();
	_encoder->stop();
	_firstFrameDecoded = false;
}

int H26xEncoderFilter::getBitrate() const {
	return _vconf.required_bitrate;
}

void H26xEncoderFilter::setBitrate(int br) {
	if (_encoder->isRunning()) {
		/* Encoding is already ongoing, do not change video size, only bitrate. */
		_vconf.required_bitrate = br;
		/* apply the new bitrate request to the running MediaCodec*/
		setVideoConfiguration(&_vconf);
	} else {
		MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_size_and_bitrate(_vconfList, _vconf.vsize, ms_factory_get_cpu_count(getFactory()),  br);
		setVideoConfiguration(&best_vconf);
	}
}

void H26xEncoderFilter::setVideoConfiguration(const MSVideoConfiguration *vconf) {
	if (vconf != &_vconf) memcpy(&_vconf, vconf, sizeof(MSVideoConfiguration));
	ms_message("MediaCodecEncoder: video configuration set: bitrate=%d bits/s, fps=%f, vsize=%dx%d", _vconf.required_bitrate, _vconf.fps, _vconf.vsize.width, _vconf.vsize.height);
	_encoder->setVideoSize(_vconf.vsize);
	_encoder->setFps(_vconf.fps);
	_encoder->setBitrate(_vconf.required_bitrate);
}

void H26xEncoderFilter::setFps(float  fps) {
	_vconf.fps = fps;
	setVideoConfiguration(&_vconf);
}

float H26xEncoderFilter::getFps() const {
	return _vconf.fps;
}

MSVideoSize H26xEncoderFilter::getVideoSize() const {
	return _vconf.vsize;
}

void H26xEncoderFilter::enableAvpf(bool enable) {
	ms_message("MediaCodecEncoder: AVPF %s", enable ? "enabled" : "disabled");
	_avpfEnabled = enable;
}

void H26xEncoderFilter::setVideoSize(const MSVideoSize &vsize) {
	MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_size_and_bitrate(_vconfList, vsize, ms_factory_get_cpu_count(getFactory()), _vconf.required_bitrate);
	_vconf.vsize = vsize;
	_vconf.fps = best_vconf.fps;
	_vconf.bitrate_limit = best_vconf.bitrate_limit;
	setVideoConfiguration(&_vconf);
}

void H26xEncoderFilter::requestVfu() {
	ms_message("MediaCodecEncoder: VFU requested");
	ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
}

void H26xEncoderFilter::notifyPli() {
	ms_message("MediaCodecEncoder: PLI requested");
	ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
}

void H26xEncoderFilter::notifyFir() {
	ms_message("MediaCodecEncoder: FIR requested");
	ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
}

void H26xEncoderFilter::notifySli() {
	ms_message("MediaCodecEncoder: SLI requested");
	ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
}

const MSVideoConfiguration *H26xEncoderFilter::getVideoConfigurations() const {
	return _vconfList;
}

void H26xEncoderFilter::setVideoConfigurations(const MSVideoConfiguration *vconfs) {
	_vconfList = vconfs ? vconfs : _defaultVConfList;
}

} // namespace mediastreamer
