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

#include "h26x-utils.h"

#include "h26x-encoder-filter.h"

using namespace std;

namespace mediastreamer {

H26xEncoderFilter::H26xEncoderFilter(MSFilter *f, H26xEncoder *encoder, const MSVideoConfiguration *vconfList):
	EncoderFilter(f), _encoder(encoder), _vconfList(vconfList) {

	_vconf = ms_video_find_best_configuration_for_size(_vconfList, MS_VIDEO_SIZE_CIF, ms_factory_get_cpu_count(f->factory));
	ms_video_starter_init(&_starter);

	_packer.reset(H26xToolFactory::get(_encoder->getMime()).createNalPacker(ms_factory_get_payload_max_size(f->factory)));
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
			ms_message("H26xEncoder: requesting I-frame to the encoder.");
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

void H26xEncoderFilter::setVideoConfiguration(MSVideoConfiguration vconf) {
	char videoSettingsStr[255];
	snprintf(videoSettingsStr, sizeof(videoSettingsStr), "bitrate=%db/s, fps=%f, vsize=%dx%d", _vconf.required_bitrate, _vconf.fps, _vconf.vsize.width, _vconf.vsize.height);
	try {
		if (_encoder->isRunning()) {
			ms_warning("H26xEncoderFilter: ignoring video size change because the encoder is started");
			vconf.vsize = _encoder->getVideoSize();
		} else {
			_encoder->setVideoSize(vconf.vsize);
		}
		_encoder->setFps(vconf.fps);
		_encoder->setBitrate(vconf.required_bitrate);
		_vconf = vconf;
		ms_message("H26xEncoder: video configuration set (%s)", videoSettingsStr);
	} catch (const runtime_error &e) {
		ms_error("H26xEncoder: video configuration failed (%s): %s", videoSettingsStr, e.what());
	}
}

void H26xEncoderFilter::enableAvpf(bool enable) {
	ms_message("H26xEncoder: AVPF %s", enable ? "enabled" : "disabled");
	_avpfEnabled = enable;
}

void H26xEncoderFilter::requestVfu() {
	ms_message("H26xEncoder: VFU requested");
	ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
}

void H26xEncoderFilter::notifyPli() {
	ms_message("H26xEncoder: PLI requested");
	ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
}

void H26xEncoderFilter::notifyFir() {
	ms_message("H26xEncoder: FIR requested");
	ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
}

void H26xEncoderFilter::notifySli() {
	ms_message("H26xEncoder: SLI requested");
	ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
}

const MSVideoConfiguration *H26xEncoderFilter::getVideoConfigurations() const {
	return _vconfList;
}

} // namespace mediastreamer
