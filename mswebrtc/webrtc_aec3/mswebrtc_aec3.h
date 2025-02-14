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
 * @brief Class to apply the AEC3 filter of webRTC to the capture and the render
 * audio streams in order to suppress the echo.
 *
 * The filter has 2 inputs and 2 outputs :
 * inputs[0]= render, reference signal from far end (sent to soundcard)
 * inputs[1]= capture, near speech & echo signal (read from soundcard)
 * outputs[0]=  is a copy of inputs[0] to be sent to soundcard
 * outputs[1]=  near end speech, echo removed - towards far end
 * All signals have the same sampling rate, that must be divisible by 100, and
 * only 1 channel. The supported sampling rates are 16000 Hz, 32000 Hz and 48000
 * Hz.
 */
class mswebrtc_aec3 {
private:
	void configure_flow_controlled_bufferizer();

	const int framesize_ms = 100;
	const int num_channels = 1;
	int sample_rate_Hz;
	int delay_ms;
	double echo_return_loss;
	double echo_return_loss_enhancement;
	int num_samples;
	int nbytes;
	char *state_str;
	std::unique_ptr<webrtc::EchoCanceller3> EchoCanceller3Inst;
	MSBufferizer delayed_ref;
	MSFlowControlledBufferizer ref;
	MSBufferizer echo;
	std::unique_ptr<webrtc::EchoCanceller3Config> aec_config;
	std::unique_ptr<webrtc::AudioBuffer> capture_buffer;
	std::unique_ptr<webrtc::AudioBuffer> render_buffer;
	webrtc::StreamConfig stream_config;
	bool echostarted;
	bool bypass_mode;
	bool using_zeroes;

public:
	mswebrtc_aec3(MSFilter *f);
	~mswebrtc_aec3(){};
	void uninit();
	void preprocess();
	void process(MSFilter *f);
	void postprocess();
	int set_sample_rate(int requested_rate);
	int get_sample_rate() {
		return sample_rate_Hz;
	}
	void set_delay(int requested_delay) {
		delay_ms = requested_delay;
	}
	int get_delay() {
		return delay_ms;
	}
	int get_erl() {
		return echo_return_loss;
	}
	int get_erle() {
		return echo_return_loss_enhancement;
	}
	void set_bypass_mode(bool bypass) {
		bypass_mode = bypass;
		ms_message("set EC bypass mode to [%i]", bypass_mode);
	}
	bool get_bypass_mode() {
		return bypass_mode;
	}
	void set_state(char *state) {
		state_str = state;
	}
	char *get_state() {
		return state_str;
	}
};

} // namespace mswebrtc_aec3

#endif // MSWEBRTC_AEC3_H_
