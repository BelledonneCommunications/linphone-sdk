/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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
#include "mediastreamer2/msticker.h"
#include "ortp/str_utils.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "api/audio/audio_processing.h"
#include "api/audio/echo_canceller3_config.h"
#include "mediastreamer2/msfilter.h"
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/audio_buffer.h"
#include "mswebrtc_aec3.h"

namespace mswebrtc_aec3 {

mswebrtc_aec3::mswebrtc_aec3(MSFilter *filter) {
	mSampleRateInHz = 16000;
	mDelayInMs = 0;
	mEchoReturnLoss = 0.0;
	mEchoReturnLossEnhancement = 0.0;
	mNumSamples = 160;
	mNbytes = mNumSamples * sizeof(int16_t);
	mStateStr = nullptr;
	ms_bufferizer_init(&mDelayedRef);
	ms_bufferizer_init(&mEcho);
	ms_flow_controlled_bufferizer_init(&mRef, filter, mSampleRateInHz, kNumChannels);
	mEchoStarted = false;
	mBypassMode = false;
	mUsingZeroes = false;
}

void mswebrtc_aec3::uninit() {
	if (mStateStr) ms_free(mStateStr);
	ms_bufferizer_uninit(&mDelayedRef);
}

void mswebrtc_aec3::configureFlowControlledBufferizer() {
	ms_flow_controlled_bufferizer_set_samplerate(&mRef, mSampleRateInHz);
	ms_flow_controlled_bufferizer_set_max_size_ms(&mRef, 50);
	ms_flow_controlled_bufferizer_set_granularity_ms(&mRef, kFramesizeMs);
}

void mswebrtc_aec3::preprocess() {
	if (!webrtc::ValidFullBandRate(mSampleRateInHz)) {
		ms_error(
		    "WebRTC echo canceller 3 does not support %d sample rate. Accepted values are 16000, 32000 or 48000 Hz.",
		    mSampleRateInHz);
		mBypassMode = true;
		ms_error("Entering bypass mode");
		return;
	}

	ms_message("Initializing WebRTC echo canceler 3 with sample rate = %i Hz, frame %i ms, %i samples, initial delay "
	           "is %i ms, %i channel",
	           mSampleRateInHz, kFramesizeMs, rtc::CheckedDivExact(mSampleRateInHz, 100), mDelayInMs, kNumChannels);
	configureFlowControlledBufferizer();
	const webrtc::EchoCanceller3Config aecConfig = webrtc::EchoCanceller3Config();
	mEchoCanceller3Inst =
	    std::make_unique<webrtc::EchoCanceller3>(aecConfig, std::nullopt, mSampleRateInHz, kNumChannels, kNumChannels);
	if (mDelayInMs != 0) {
		mEchoCanceller3Inst->SetAudioBufferDelay(mDelayInMs);
	}

	if (!mEchoCanceller3Inst) {
		mBypassMode = true;
		ms_error("EchoCanceller3 error at creation, entering bypass mode");
		return;
	}

	mNumSamples = rtc::CheckedDivExact(mSampleRateInHz, 100);
	mNbytes = mNumSamples * sizeof(int16_t);

	// Initialize audio buffers
	mCaptureBuffer = std::make_unique<webrtc::AudioBuffer>(mSampleRateInHz, kNumChannels, mSampleRateInHz, kNumChannels,
	                                                       mSampleRateInHz, kNumChannels);
	mRenderBuffer = std::make_unique<webrtc::AudioBuffer>(mSampleRateInHz, kNumChannels, mSampleRateInHz, kNumChannels,
	                                                      mSampleRateInHz, kNumChannels);
	mStreamConfig = webrtc::StreamConfig(mSampleRateInHz, kNumChannels);

	return;
}

/* Process audio streams of MSFilter with EchoCanceller3
 *	inputs[0]= render, reference signal from far end (sent to soundcard)
 *	inputs[1]= capture, near speech & echo signal (read from soundcard)
 *	outputs[0]= is a copy of inputs[0] to be sent to soundcard
 *	outputs[1]= near end speech, echo removed - towards far end
 */
void mswebrtc_aec3::process(MSFilter *filter) {
	mblk_t *refm;

	if (mBypassMode) {
		while ((refm = ms_queue_get(filter->inputs[0])) != NULL) {
			ms_queue_put(filter->outputs[0], refm);
		}
		while ((refm = ms_queue_get(filter->inputs[1])) != NULL) {
			ms_queue_put(filter->outputs[1], refm);
		}
		return;
	}

	if (filter->inputs[0] != nullptr) {
		if (mEchoStarted) {
			while ((refm = ms_queue_get(filter->inputs[0])) != NULL) {
				mblk_t *cp = dupmsg(refm);
				ms_bufferizer_put(&mDelayedRef, cp);
				ms_flow_controlled_bufferizer_put(&mRef, refm);
			}
		} else {
			ms_warning("Getting reference signal but no echo to synchronize on.");
			ms_queue_flush(filter->inputs[0]);
		}
	}

	ms_bufferizer_put_from_queue(&mEcho, filter->inputs[1]);

	int16_t *refData, *echoData;
	refData = (int16_t *)alloca(mNbytes);
	echoData = (int16_t *)alloca(mNbytes);

	while (ms_bufferizer_read(&mEcho, (uint8_t *)echoData, (size_t)mNbytes) >= static_cast<size_t>(mNbytes)) {
		mblk_t *oEcho = allocb(mNbytes, 0);
		int avail;

		if (!mEchoStarted) mEchoStarted = TRUE;

		if ((avail = static_cast<int>(ms_bufferizer_get_avail(&mDelayedRef))) < mNbytes) {
			/*we don't have enough to read in a reference signal buffer, inject
			 * silence instead*/
			refm = allocb(mNbytes, 0);
			memset(refm->b_wptr, 0, mNbytes);
			refm->b_wptr += mNbytes;
			ms_bufferizer_put(&mDelayedRef, refm);
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
			if (!mUsingZeroes) {
				ms_warning("Not enough ref samples, using zeroes");
				mUsingZeroes = true;
			}
		} else {
			if (mUsingZeroes) {
				ms_message("Samples are back.");
				mUsingZeroes = false;
			}
			/* read from our no-delay buffer and output */
			refm = allocb(mNbytes, 0);
			if (ms_flow_controlled_bufferizer_read(&mRef, refm->b_wptr, mNbytes) == 0) {
				MSBufferizer *obj = (MSBufferizer *)&mRef;
				ms_message("ref flow controlled bufferizer size is %d but mNbytes is %d", (int)obj->size, (int)mNbytes);
				ms_fatal("Should never happen, read error on ref flow controlled bufferizer in AEC");
			}
			refm->b_wptr += mNbytes;
			ms_queue_put(filter->outputs[0], refm);
		}

		/*now read a valid buffer of delayed ref samples*/
		if (ms_bufferizer_read(&mDelayedRef, (uint8_t *)refData, mNbytes) == 0) {
			MSBufferizer *obj = (MSBufferizer *)&mRef;
			ms_message("delayed ref bufferizer size is %d but mNbytes is %d", (int)obj->size, (int)mNbytes);
			ms_fatal("Should never happen, read error on delayed ref flow controlled bufferizer in AEC");
		}
		avail -= mNbytes;

		// fill audio buffer
		mCaptureBuffer->webrtc::AudioBuffer::CopyFrom(echoData, mStreamConfig);
		mRenderBuffer->webrtc::AudioBuffer::CopyFrom(refData, mStreamConfig);

		if (mSampleRateInHz > webrtc::AudioProcessing::kSampleRate16kHz) {
			mCaptureBuffer->SplitIntoFrequencyBands();
			mRenderBuffer->SplitIntoFrequencyBands();
		}

		// apply echo cancellation
		mEchoCanceller3Inst->AnalyzeCapture(mCaptureBuffer.get());
		mEchoCanceller3Inst->AnalyzeRender(mRenderBuffer.get());
		mEchoCanceller3Inst->ProcessCapture(
		    mCaptureBuffer.get(),
		    false); // the echo path gain level change is set to false by default but it could be improved

		webrtc::EchoCanceller3::Metrics aecMetrics = mEchoCanceller3Inst->GetMetrics();
		mDelayInMs = aecMetrics.delay_ms;
		mEchoReturnLoss = aecMetrics.echo_return_loss;
		mEchoReturnLossEnhancement = aecMetrics.echo_return_loss_enhancement;
		if (filter->ticker->time % 5000 == 0) {
			ms_message("AEC3 current metrics : delay = %d ms, ERL = %f, ERLE = %f", aecMetrics.delay_ms,
			           aecMetrics.echo_return_loss, aecMetrics.echo_return_loss_enhancement);
		}

		// get processed capture
		if (mSampleRateInHz > webrtc::AudioProcessing::kSampleRate16kHz) {
			mCaptureBuffer->MergeFrequencyBands();
		}

		mCaptureBuffer->CopyTo(mStreamConfig, (int16_t *)oEcho->b_wptr);
		oEcho->b_wptr += mNbytes;
		ms_queue_put(filter->outputs[1], oEcho);
	}
}

void mswebrtc_aec3::postprocess() {
	ms_bufferizer_flush(&mDelayedRef);
	ms_bufferizer_flush(&mEcho);
	ms_flow_controlled_bufferizer_flush(&mRef);
	if (mEchoCanceller3Inst != nullptr) {
		mEchoCanceller3Inst = nullptr;
	}
	if (mCaptureBuffer != nullptr) {
		mCaptureBuffer.reset();
	}
	if (mRenderBuffer != nullptr) {
		mRenderBuffer.reset();
	}
}

int mswebrtc_aec3::setSampleRate(int requestedRateInHz) {
	if (requestedRateInHz >= 48000) {
		mSampleRateInHz = 48000;
	} else if (requestedRateInHz >= 32000) {
		mSampleRateInHz = 32000;
	} else {
		mSampleRateInHz = 16000;
	}
	if (mSampleRateInHz != requestedRateInHz)
		ms_message("Webrtc AEC3 does not support sampling rate %i, using %i instead", requestedRateInHz,
		           mSampleRateInHz);
	configureFlowControlledBufferizer();
	ms_message("sampling rate: %d - %d Hz", requestedRateInHz, mSampleRateInHz);
	return mSampleRateInHz;
}

} // namespace mswebrtc_aec3
