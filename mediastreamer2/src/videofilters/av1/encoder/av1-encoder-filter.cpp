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

#include "av1-encoder-filter.h"

#include "filter-wrapper/encoding-filter-wrapper.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

using namespace std;

#define MS_AV1_CONF(required_bitrate, bitrate_limit, resolution, fps, ncpus)                                           \
	{                                                                                                                  \
		required_bitrate, bitrate_limit, {MS_VIDEO_SIZE_##resolution##_W, MS_VIDEO_SIZE_##resolution##_H}, fps, ncpus, \
		    nullptr                                                                                                    \
	}

static const MSVideoConfiguration _av1_conf_list[] = {
// High resolutions are for now too demanding for the encoding software on mobile phones.
#if !defined(ANDROID) && !defined(TARGET_OS_IPHONE)
    MS_AV1_CONF(1024000, 2048000, 720P, 30, 8),
#endif
    // MS_AV1_CONF(850000, 2048000, XGA, 25, 4),
    MS_AV1_CONF(400000, 1500000, VGA, 30, 2),
    MS_AV1_CONF(128000, 512000, CIF, 30, 1),
    MS_AV1_CONF(0, 170000, QVGA, 15, 1),
};

namespace mediastreamer {

Av1EncoderFilter::Av1EncoderFilter(MSFilter *f, Av1Encoder *encoder, const MSVideoConfiguration *vconfList)
    : EncoderFilter(f), mEncoder(encoder), mPacker(ms_factory_get_payload_max_size(f->factory)), mVconfList(vconfList) {

	mVconf =
	    ms_video_find_best_configuration_for_size(mVconfList, MS_VIDEO_SIZE_CIF, ms_factory_get_cpu_count(f->factory));
	ms_video_starter_init(&mStarter);
}

void Av1EncoderFilter::preprocess() {
	mEncoder->configure(mAvpfEnabled);
	mEncoder->start();
	ms_video_starter_init(&mStarter);
	ms_iframe_requests_limiter_init(&mIframeLimiter, 1000);
}

void Av1EncoderFilter::process() {
	/*First queue input image*/
	if (mblk_t *im = ms_queue_peek_last(getInput(0))) {
		bool requestIFrame = false;
		if (ms_iframe_requests_limiter_iframe_requested(&mIframeLimiter, getTime()) ||
		    (!mAvpfEnabled && ms_video_starter_need_i_frame(&mStarter, getTime()))) {
			ms_message("Av1EncoderFilter: requesting I-frame to the encoder.");
			requestIFrame = true;
			ms_iframe_requests_limiter_notify_iframe_sent(&mIframeLimiter, getTime());
		}
		mEncoder->feed(dupmsg(im), getTime(), requestIFrame);
	}
	ms_queue_flush(getInput(0));

	/*Second, dequeue possibly pending encoded frames*/
	MSQueue encodedFrames;
	ms_queue_init(&encodedFrames);
	while (mEncoder->fetch(&encodedFrames)) {
		if (!mFirstFrameEncoded) {
			mFirstFrameEncoded = true;
			ms_video_starter_first_frame(&mStarter, getTime());
		}

		mPacker.pack(&encodedFrames, getOutput(0), static_cast<uint32_t>(getTime() * 90LL));
	}
}

void Av1EncoderFilter::postprocess() {
	mEncoder->stop();
	mFirstFrameEncoded = false;
}

void Av1EncoderFilter::setVideoConfiguration(MSVideoConfiguration vconf) {
	ostringstream os{};
	os << "bitrate=" << vconf.required_bitrate << "b/s, fps=" << vconf.fps << ", vsize=" << vconf.vsize.width << "x"
	   << vconf.vsize.height;

	try {
		if (mEncoder->isRunning() && !ms_video_size_equal(mVconf.vsize, vconf.vsize)) {
			ms_warning("Av1EncoderFilter: ignoring video size change because the encoder is started");
			vconf.vsize = mEncoder->getVideoSize();
		} else {
			mEncoder->setVideoSize(vconf.vsize);
		}
		mEncoder->setFps(vconf.fps);
		mEncoder->setBitrate(vconf.required_bitrate);
		mVconf = vconf;
		ms_message("Av1EncoderFilter: video configuration set (%s)", os.str().c_str());
	} catch (const runtime_error &e) {
		ms_error("Av1EncoderFilter: video configuration failed (%s): %s", os.str().c_str(), e.what());
	}
}

void Av1EncoderFilter::enableAvpf(bool enable) {
	ms_message("Av1EncoderFilter: AVPF %s", enable ? "enabled" : "disabled");
	mAvpfEnabled = enable;
}

void Av1EncoderFilter::requestVfu() {
	ms_message("Av1EncoderFilter: VFU requested");
	ms_iframe_requests_limiter_request_iframe(&mIframeLimiter);
}

void Av1EncoderFilter::notifyPli() {
	ms_message("Av1EncoderFilter: PLI requested");
	ms_iframe_requests_limiter_request_iframe(&mIframeLimiter);
}

void Av1EncoderFilter::notifyFir() {
	ms_message("Av1EncoderFilter: FIR requested");
	ms_iframe_requests_limiter_request_iframe(&mIframeLimiter);
}

void Av1EncoderFilter::notifySli() {
	ms_message("Av1EncoderFilter: SLI requested");
	ms_iframe_requests_limiter_request_iframe(&mIframeLimiter);
}

void Av1EncoderFilter::enableDivideIntoPacketsEqualSize(bool enable) {
	ms_message("Av1EncoderFilter: divide OBU into packets of equal sizes %s", enable ? "enabled" : "disabled");
	mPacker.enableDivideIntoEqualSize(enable);
}

const MSVideoConfiguration *Av1EncoderFilter::getVideoConfigurations() const {
	return mVconfList;
}

class Av1EncoderFilterImpl : public Av1EncoderFilter {
public:
	Av1EncoderFilterImpl(MSFilter *f) : Av1EncoderFilter(f, new Av1Encoder(f->factory), _av1_conf_list) {
	}
};

} // namespace mediastreamer

using namespace mediastreamer;

MS_ENCODING_FILTER_WRAPPER_METHODS_DECLARATION(Av1Encoder);
MS_ENCODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(
    Av1Encoder, MS_AV1_ENC_ID, "An AV1 encoder based on aom.", "AV1", MS_FILTER_IS_PUMP);

MS_FILTER_DESC_EXPORT(ms_Av1Encoder_desc)
