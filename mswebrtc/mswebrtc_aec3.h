#ifndef MSWEBRTC_AEC3_H_
#define MSWEBRTC_AEC3_H_

#include <memory>

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msqueue.h"

#include "api/audio/audio_processing.h"
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/high_pass_filter.h"

namespace mswebrtc_aec3 {

/** @class mswebrtc_aec3
 * @brief Class to apply the AEC3 filter of WebRTC to the capture and the render
 * audio streams in order to suppress the echo.
 *
 * The filter has 2 inputs and 2 outputs:
 * inputs[0] = render, reference signal from far end (sent to soundcard)
 * inputs[1] = capture, near speech & echo signal (read from soundcard)
 * outputs[0] = is a copy of inputs[0] to be sent to soundcard
 * outputs[1] = near end speech, echo removed - towards far end
 * All signals have the same sampling rate, that must be divisible by 100, and
 * only 1 channel. The supported sampling rates are 16000 Hz, 32000 Hz, and 48000
 * Hz.
 */
class mswebrtc_aec3 {
private:
	void configureFlowControlledBufferizer();

	const int kFramesizeMs = 100;
	const int kNumChannels = 1;
	int mSampleRateInHz;
	int mDelayInMs;
	double mEchoReturnLoss;
	double mEchoReturnLossEnhancement;
	int mNumSamples;
	int mNbytes;
	char *mStateStr;
	std::unique_ptr<webrtc::EchoCanceller3> mEchoCanceller3Inst;
	MSBufferizer mDelayedRef;
	MSFlowControlledBufferizer mRef;
	MSBufferizer mEcho;
	std::unique_ptr<webrtc::EchoCanceller3Config> mAecConfig;
	std::unique_ptr<webrtc::AudioBuffer> mCaptureBuffer;
	std::unique_ptr<webrtc::AudioBuffer> mRenderBuffer;
	webrtc::StreamConfig mStreamConfig;
	bool mEchoStarted;
	bool mBypassMode;
	bool mUsingZeroes;

public:
	mswebrtc_aec3(MSFilter *filter);
	~mswebrtc_aec3(){};
	void uninit();
	void preprocess();
	void process(MSFilter *filter);
	void postprocess();
	int setSampleRate(int requestedRateInHz);
	int getSampleRate() {
		return mSampleRateInHz;
	}
	void setDelay(int requestedDelayInMs) {
		mDelayInMs = requestedDelayInMs;
	}
	int getDelay() {
		return mDelayInMs;
	}
	int getErl() {
		return mEchoReturnLoss;
	}
	int getErle() {
		return mEchoReturnLossEnhancement;
	}
	void setBypassMode(bool bypass) {
		mBypassMode = bypass;
		ms_message("set EC bypass mode to [%i]", mBypassMode);
	}
	bool getBypassMode() {
		return mBypassMode;
	}
	void setState(char *state) {
		mStateStr = state;
	}
	char *getState() {
		return mStateStr;
	}
};

} // namespace mswebrtc_aec3

#endif // MSWEBRTC_AEC3_H_
