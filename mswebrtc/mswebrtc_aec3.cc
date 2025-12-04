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

namespace mswebrtcaec3 {

MSWebrtcAEC3::MSWebrtcAEC3(MSFilter *filter) {
	mSampleRateInHz = 16000;
	mDelayInMs = 0;
	mEchoReturnLoss = 0.0;
	mEchoReturnLossEnhancement = 0.0;
	mNumSamples = 160;
	mNbytes = static_cast<size_t>(mNumSamples * sizeof(int16_t));
	mStateStr = nullptr;
	ms_bufferizer_init(&mEcho);
	ms_flow_controlled_bufferizer_init(&mRef, filter, mSampleRateInHz, kNumChannels);
	mEchoStarted = false;
	mBypassMode = false;
	mWaitingRef = false;
}

void MSWebrtcAEC3::uninit() {
	if (mStateStr) ms_free(mStateStr);
}

void MSWebrtcAEC3::configureFlowControlledBufferizer() {
	ms_flow_controlled_bufferizer_set_samplerate(&mRef, mSampleRateInHz);
	ms_flow_controlled_bufferizer_set_max_size_ms(&mRef, 50);
	ms_flow_controlled_bufferizer_set_granularity_ms(&mRef, kFramesizeMs);
}

void MSWebrtcAEC3::preprocess() {
	if (!webrtc::ValidFullBandRate(mSampleRateInHz)) {
		ms_error("WebRTCAEC[%p]: echo canceller 3 does not support %d sample rate. Accepted values are 16000, 32000 "
		         "or 48000 Hz.",
		         this, mSampleRateInHz);
		mBypassMode = true;
		ms_error("Entering bypass mode");
		return;
	}

	ms_message("WebRTCAEC[%p]: initializing echo canceler 3 with sample rate = %i Hz, frame %i ms, %i samples, "
	           "initial delay "
	           "is %i ms, %i channel",
	           this, mSampleRateInHz, kFramesizeMs, rtc::CheckedDivExact(mSampleRateInHz, 100), mDelayInMs,
	           kNumChannels);
	configureFlowControlledBufferizer();
	const webrtc::EchoCanceller3Config aecConfig = webrtc::EchoCanceller3Config();
	mEchoCanceller3Inst =
	    std::make_unique<webrtc::EchoCanceller3>(aecConfig, std::nullopt, mSampleRateInHz, kNumChannels, kNumChannels);
	if (mDelayInMs != 0) {
		mEchoCanceller3Inst->SetAudioBufferDelay(mDelayInMs);
	}

	if (!mEchoCanceller3Inst) {
		mBypassMode = true;
		ms_error("WebRTCAEC[%p]: error at creation, entering bypass mode", this);
		return;
	}

	mEchoStarted = false;
	mNumSamples = rtc::CheckedDivExact(mSampleRateInHz, 100);
	mNbytes = static_cast<size_t>(mNumSamples * sizeof(int16_t));

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
void MSWebrtcAEC3::process(MSFilter *filter) {
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

	ms_bufferizer_put_from_queue(&mEcho, filter->inputs[1]);
	if (!mEchoStarted) {
		if (ms_bufferizer_get_avail(&mEcho) >= mNbytes) mEchoStarted = true;
	}

	if (filter->inputs[0] != nullptr) {
		if (mEchoStarted) {
			while ((refm = ms_queue_get(filter->inputs[0])) != NULL) {
				ms_flow_controlled_bufferizer_put(&mRef, refm);
			}
		} else {
			ms_warning("WebRTCAEC[%p]: getting reference signal but no echo to synchronize on.", this);
			ms_queue_flush(filter->inputs[0]);
		}
	}

	std::vector<int16_t> refData(mNumSamples, 0);
	std::vector<int16_t> echoData(mNumSamples, 0);

	while (ms_bufferizer_read(&mEcho, reinterpret_cast<uint8_t *>(echoData.data()), mNbytes) >= mNbytes) {
		mblk_t *oEcho = allocb(mNbytes, 0);
		if (ms_flow_controlled_bufferizer_get_avail(&mRef) >= mNbytes) {
			if (mWaitingRef) {
				ms_message("WebRTCAEC[%p]: samples are back.", this);
				mWaitingRef = false;
			}
			/* read from reference buffer to AEC3 render buffer and output */
			refm = allocb(mNbytes, 0);
			ms_flow_controlled_bufferizer_read(&mRef, refm->b_wptr, mNbytes);
			memcpy(refData.data(), refm->b_wptr, mNbytes);
			refm->b_wptr += mNbytes;
			ms_queue_put(filter->outputs[0], refm);
		} else {
			/*we don't have enough to read in a reference signal buffer, send nothing to ref output nor render buffer*/
			if (!mWaitingRef) {
				ms_warning("WebRTCAEC[%p]: not enough ref samples, waiting.", this);
				mWaitingRef = true;
			}
		}

		// fill audio buffer
		mCaptureBuffer->webrtc::AudioBuffer::CopyFrom(echoData.data(), mStreamConfig);
		mRenderBuffer->webrtc::AudioBuffer::CopyFrom(refData.data(), mStreamConfig);

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
		if ((filter->ticker->time % 5000) == 0) {
			ms_message("WebRTCAEC[%p] AEC3 current metrics: delay = %d ms, ERL = %f, ERLE = %f", this,
			           aecMetrics.delay_ms, aecMetrics.echo_return_loss, aecMetrics.echo_return_loss_enhancement);
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

void MSWebrtcAEC3::postprocess() {
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

int MSWebrtcAEC3::setSampleRate(int requestedRateInHz) {
	if (requestedRateInHz >= 48000) {
		mSampleRateInHz = 48000;
	} else if (requestedRateInHz >= 32000) {
		mSampleRateInHz = 32000;
	} else {
		mSampleRateInHz = 16000;
	}
	if (mSampleRateInHz != requestedRateInHz)
		ms_message("WebRTCAEC[%p]: AEC3 does not support sampling rate %i, using %i instead", this, requestedRateInHz,
		           mSampleRateInHz);
	configureFlowControlledBufferizer();
	ms_message("WebRTCAEC[%p]: sampling rate: %d - %d Hz", this, requestedRateInHz, mSampleRateInHz);
	return mSampleRateInHz;
}

} // namespace mswebrtcaec3
