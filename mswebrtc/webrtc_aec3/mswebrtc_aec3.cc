/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#include "aec3_common.h"
#include "audio_buffer.h"
#include "echo_control.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msqueue.h"
#include "ortp/str_utils.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "mediastreamer2/msfilter.h"

#include "api/audio/audio_processing.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/audio_buffer.h"
#include "mswebrtc_aec3.h"

#include "modules/audio_processing/aec3/echo_canceller3.h"

#define EC_DUMP 0

namespace mswebrtc_aec3 {

mswebrtc_aec3::mswebrtc_aec3(MSFilter *f) {
	sample_rate_Hz = 16000;
	delay_ms = 0;
	echo_return_loss = 0.;
	echo_return_loss_enhancement = 0.;
	num_samples = 160;
	nbytes = num_samples * sizeof(int16_t);
	state_str = nullptr;
	ms_bufferizer_init(&delayed_ref);
	ms_bufferizer_init(&echo);
	ms_flow_controlled_bufferizer_init(&ref, f, sample_rate_Hz, num_channels);
	echostarted = false;
	bypass_mode = false;
	using_zeroes = false;
}

void mswebrtc_aec3::uninit() {
	if (state_str) ms_free(state_str);
	if (delayed_ref.size > 0) {
		ms_bufferizer_uninit(&delayed_ref);
	}
}

void mswebrtc_aec3::configure_flow_controlled_bufferizer() {
	ms_flow_controlled_bufferizer_set_samplerate(&ref, sample_rate_Hz);
	ms_flow_controlled_bufferizer_set_max_size_ms(&ref, 50);
	ms_flow_controlled_bufferizer_set_granularity_ms(&ref, framesize_ms);
}

void mswebrtc_aec3::preprocess() {

	if (!webrtc::ValidFullBandRate(sample_rate_Hz)) {
		ms_error(
		    "WebRTC echo canceller 3 does not support %d sample rate. Accepted values are 16000, 32000 or 48000 Hz.",
		    sample_rate_Hz);
		bypass_mode = true;
		ms_error("Entering bypass mode");
		return;
	}

	ms_message("Initializing WebRTC echo canceler 3 with sample rate = %i Hz, frame %i ms, %i samples, initial delay "
	           "is %i ms, %i channel",
	           sample_rate_Hz, framesize_ms, rtc::CheckedDivExact(sample_rate_Hz, 100), delay_ms, num_channels);
	configure_flow_controlled_bufferizer();
	const webrtc::EchoCanceller3Config aec_config = webrtc::EchoCanceller3Config();
	EchoCanceller3Inst =
	    std::make_unique<webrtc::EchoCanceller3>(aec_config, std::nullopt, sample_rate_Hz, num_channels, num_channels);
	if (delay_ms != 0) {
		EchoCanceller3Inst->SetAudioBufferDelay(delay_ms);
	}

	if (!EchoCanceller3Inst) {
		bypass_mode = true;
		ms_error("EchoCanceller3 error at creation, entering bypass mode");
		return;
	}

	num_samples = rtc::CheckedDivExact(sample_rate_Hz, 100);
	nbytes = num_samples * sizeof(int16_t);

	// initialize audio buffers
	capture_buffer = std::make_unique<webrtc::AudioBuffer>(sample_rate_Hz, num_channels, sample_rate_Hz, num_channels,
	                                                       sample_rate_Hz, num_channels);
	render_buffer = std::make_unique<webrtc::AudioBuffer>(sample_rate_Hz, num_channels, sample_rate_Hz, num_channels,
	                                                      sample_rate_Hz, num_channels);
	stream_config = webrtc::StreamConfig(sample_rate_Hz, num_channels);

	return;
}

/* Process audio streams of MSFilter with EchoCanceller3
 *	inputs[0]= render, reference signal from far end (sent to soundcard)
 *	inputs[1]= capture, near speech & echo signal (read from soundcard)
 *	outputs[0]= is a copy of inputs[0] to be sent to soundcard
 *	outputs[1]= near end speech, echo removed - towards far end
 */
void mswebrtc_aec3::process(MSFilter *f) {

	mblk_t *refm;

	if (bypass_mode) {
		while ((refm = ms_queue_get(f->inputs[0])) != NULL) {
			ms_queue_put(f->outputs[0], refm);
		}
		while ((refm = ms_queue_get(f->inputs[1])) != NULL) {
			ms_queue_put(f->outputs[1], refm);
		}
		return;
	}

	if (f->inputs[0] != nullptr) {
		if (echostarted) {
			while ((refm = ms_queue_get(f->inputs[0])) != NULL) {
				mblk_t *cp = dupmsg(refm);
				ms_bufferizer_put(&delayed_ref, cp);
				ms_flow_controlled_bufferizer_put(&ref, refm);
			}
		} else {
			ms_warning("Getting reference signal but no echo to synchronize on.");
			ms_queue_flush(f->inputs[0]);
		}
	}

	ms_bufferizer_put_from_queue(&echo, f->inputs[1]);

	int16_t *ref_data, *echo_data;
	ref_data = (int16_t *)alloca(nbytes);
	echo_data = (int16_t *)alloca(nbytes);

	while (ms_bufferizer_read(&echo, (uint8_t *)echo_data, (size_t)nbytes) >= (size_t)nbytes) {
		mblk_t *oecho = allocb(nbytes, 0);
		int avail;

		if (!echostarted) echostarted = TRUE;

		if ((avail = ms_bufferizer_get_avail(&delayed_ref)) < nbytes) {
			/*we don't have enough to read in a reference signal buffer, inject
			 * silence instead*/
			refm = allocb(nbytes, 0);
			memset(refm->b_wptr, 0, nbytes);
			refm->b_wptr += nbytes;
			ms_bufferizer_put(&delayed_ref, refm);
			/*
			 * However, we don't inject this silence buffer to the sound card, in
			 * order to break the following bad loop:
			 * - the sound playback filter detects it has too many pending samples,
			 * then triggers an event to request samples to be dropped upstream.
			 * - the upstream MSFlowControl filter is requested to drop samples, which
			 * it starts to do.
			 * - necessarily shortly after the AEC goes into a situation where it has
			 * not enough reference samples while processing an audio buffer from mic.
			 * - if the AEC injects a silence buffer as output, then it will RECREATE
			 * a situation where the sound playback filter has too many pending
			 * samples. That's why we should not do this. By not doing this, we will
			 * create a discrepancy between what we really injected to the soundcard,
			 * and what we told to the echo canceller about the samples we injected.
			 * This shifts the echo. The echo canceller will re-converge quickly to
			 * take into account the situation.
			 *
			 */
			if (!using_zeroes) {
				ms_warning("Not enough ref samples, using zeroes");
				using_zeroes = true;
			}
		} else {
			if (using_zeroes) {
				ms_message("Samples are back.");
				using_zeroes = false;
			}
			/* read from our no-delay buffer and output */
			refm = allocb(nbytes, 0);
			if (ms_flow_controlled_bufferizer_read(&ref, refm->b_wptr, nbytes) == 0) {
				// FIXME FHA: crash happens here
				MSBufferizer *obj = (MSBufferizer *)&ref;
				ms_message("ref flow controlled bufferizer size is %d but nbytes is %d", (int)obj->size, (int)nbytes);
				ms_fatal("Should never happen, read error on ref flow controlled bufferizer in AEC");
			}
			refm->b_wptr += nbytes;
			ms_queue_put(f->outputs[0], refm);
		}

		/*now read a valid buffer of delayed ref samples*/
		if (ms_bufferizer_read(&delayed_ref, (uint8_t *)ref_data, nbytes) == 0) {
			// FIXME FHA
			MSBufferizer *obj = (MSBufferizer *)&ref;
			ms_message("delayed ref bufferizer size is %d but nbytes is %d", (int)obj->size, (int)nbytes);
			ms_fatal("Should never happen, read error on delayed ref flow controlled bufferizer in AEC");
		}
		avail -= nbytes;

		// fill audio buffer
		capture_buffer->webrtc::AudioBuffer::CopyFrom(echo_data, stream_config);
		render_buffer->webrtc::AudioBuffer::CopyFrom(ref_data, stream_config);

		if (sample_rate_Hz > webrtc::AudioProcessing::kSampleRate16kHz) {
			capture_buffer->SplitIntoFrequencyBands();
			render_buffer->SplitIntoFrequencyBands();
		}

		// apply echo cancellation
		EchoCanceller3Inst->AnalyzeCapture(capture_buffer.get());
		EchoCanceller3Inst->AnalyzeRender(render_buffer.get());
		EchoCanceller3Inst->ProcessCapture(capture_buffer.get(),
		                                   false); // FIXME FHA set variable instead of false?

		webrtc::EchoCanceller3::Metrics aec_metrics = EchoCanceller3Inst->GetMetrics();
		delay_ms = aec_metrics.delay_ms;
		echo_return_loss = aec_metrics.echo_return_loss;
		echo_return_loss_enhancement = aec_metrics.echo_return_loss_enhancement;
#if defined(EC_DUMP) && EC_DUMP
		ms_message("current metrics : delay = %d ms, ERL = %f, ERLE = %f", aec_metrics.delay_ms,
		           aec_metrics.echo_return_loss, aec_metrics.echo_return_loss_enhancement);
#endif

		// get processed capture
		if (sample_rate_Hz > webrtc::AudioProcessing::kSampleRate16kHz) {
			capture_buffer->MergeFrequencyBands();
		}

		capture_buffer->CopyTo(stream_config, (int16_t *)oecho->b_wptr);
		oecho->b_wptr += nbytes;
		ms_queue_put(f->outputs[1], oecho);
	}
}

void mswebrtc_aec3::postprocess() {
	ms_bufferizer_flush(&delayed_ref);
	ms_bufferizer_flush(&echo);
	ms_flow_controlled_bufferizer_flush(&ref);
	if (EchoCanceller3Inst != nullptr) {
		EchoCanceller3Inst = nullptr;
	}
	if (capture_buffer != nullptr) {
		capture_buffer.reset();
	}
	if (render_buffer != nullptr) {
		render_buffer.reset();
	}
}

int mswebrtc_aec3::set_sample_rate(int requested_rate) {
	if (requested_rate >= 48000) {
		sample_rate_Hz = 48000;
	} else if (requested_rate >= 32000) {
		sample_rate_Hz = 32000;
	} else {
		sample_rate_Hz = 16000;
	}
	if (sample_rate_Hz != requested_rate)
		ms_message("Webrtc AEC3 does not support sampling rate %i, using %i instead", requested_rate, sample_rate_Hz);
	configure_flow_controlled_bufferizer();
	ms_message("sampling rate: %d - %d Hz", requested_rate, sample_rate_Hz);
	return sample_rate_Hz;
}

} // namespace mswebrtc_aec3
