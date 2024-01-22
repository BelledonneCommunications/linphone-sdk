/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
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

#include "packet-router.h"

#include <algorithm>
#include <map>

#ifdef AV1_ENABLED
#include "av1/obu/obu-key-frame-indicator.h"
#endif
#include "filter-wrapper/filter-wrapper-base.h"
#include "key-frame-indicator/header-extension-key-frame-indicator.h"
#include "key-frame-indicator/vp8-key-frame-indicator.h"

using namespace std;

namespace mediastreamer {

RouterInput::RouterInput(PacketRouter *router, int inputNumber) : mRouter(router), mPin(inputNumber) {
}

int RouterInput::getPin() const {
	return mPin;
}

RouterAudioInput::RouterAudioInput(PacketRouter *router, int inputNumber) : RouterInput(router, inputNumber) {
}

void RouterAudioInput::update() {
	MSQueue *queue = mRouter->getInputQueue(mPin);
	if (queue == nullptr) return;

	for (mblk_t *m = ms_queue_peek_first(queue); !ms_queue_end(queue, m); m = ms_queue_peek_next(queue, m)) {
		mSsrc = rtp_get_ssrc(m);

		bool_t voiceActivity = FALSE;
		int volume = rtp_get_client_to_mixer_audio_level(m, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL, &voiceActivity);

		if (volume != RTP_AUDIO_LEVEL_NO_VOLUME) {
			mVolume = volume;
			mIsSpeaking = voiceActivity;

			// Delete the extension, so it won't interfere with the mixer to client volumes that will be injected later.
			rtp_delete_extension_header(m, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL);
		}
	}
}

#ifdef VIDEO_ENABLED
RouterVideoInput::RouterVideoInput(PacketRouter *router,
                                   int inputNumber,
                                   const std::string &encoding,
                                   bool fullPacketMode)
    : RouterInput(router, inputNumber) {
	if (fullPacketMode) {
		mKeyFrameIndicator = make_unique<HeaderExtensionKeyFrameIndicator>();
	} else if (encoding == "VP8") {
		mKeyFrameIndicator = make_unique<VP8KeyFrameIndicator>();
	} else if (encoding == "AV1") {
#ifdef AV1_ENABLED
		mKeyFrameIndicator = make_unique<ObuKeyFrameIndicator>();
#else
		ms_error("PacketRouter: Trying to create a video input in AV1 while it is disabled");
#endif
	}
}

void RouterVideoInput::update() {
	MSQueue *queue = mRouter->getInputQueue(mPin);
	if (queue == nullptr) return;

	mKeyFrameStart = nullptr;

	for (mblk_t *m = ms_queue_peek_first(queue); !ms_queue_end(queue, m); m = ms_queue_peek_next(queue, m)) {
		uint32_t newTimestamp = mblk_get_timestamp_info(m);
		uint16_t newSeqNumber = mblk_get_cseq(m);

		if (!mSeqNumberSet) {
			mState = State::Stopped;
			mKeyFrameRequested = true;
		} else if (!mLocal && (newSeqNumber != mCurrentSeqNumber + 1)) {
			ms_warning("PacketRouter: Sequence discontinuity detected on pin %i, key-frame requested", mPin);
			mState = State::Stopped;
			mKeyFrameRequested = true;
		}

		if (mKeyFrameRequested) {
			if (!mSeqNumberSet || mCurrentTimestamp != newTimestamp) {
				// Possibly a beginning of frame !
				if (mKeyFrameIndicator->isKeyFrame(m)) {
					ms_message("PacketRouter: Key frame detected on pin %i", mPin);
					mState = State::Running;
					mKeyFrameStart = m;
					mKeyFrameRequested = false;
				}
			}
		}

		mCurrentTimestamp = newTimestamp;
		mCurrentSeqNumber = newSeqNumber;
		mSeqNumberSet = true;
	}

	if (!ms_queue_empty(queue) && mKeyFrameRequested) {
		if (mState == State::Stopped) {
			mRouter->notifyPli(mPin);
		} else {
			mRouter->notifyFir(mPin);
		}
	}
}
#endif

// =============================================================================

RouterOutput::RouterOutput(PacketRouter *router, int pin) : mRouter(router), mPin(pin) {
}

RouterAudioOutput::RouterAudioOutput(PacketRouter *router, int pin) : RouterOutput(router, pin) {
}

void RouterAudioOutput::configure(const MSPacketRouterPinData *pinData) {
	RouterOutput::configure(pinData);
}

void RouterAudioOutput::transfer() {
	MSQueue *outputQueue = mRouter->getOutputQueue(mPin);

	if (outputQueue != nullptr) {
		const auto selector = dynamic_cast<RouterInputAudioSelector *>(mRouter->getRouterInputSelector());

		if (selector) {
			auto &volumes = mRouter->getVolumesToSend();

			for (const auto input : selector->getSelectedInputs()) {
				// Don't send audio to ourselves
				if (input == nullptr || mPin == input->getPin()) continue;

				MSQueue *inputQueue = mRouter->getInputQueue(input->getPin());

				if (inputQueue == nullptr || ms_queue_empty(inputQueue)) continue;

				bool volumesSent = false;
				for (mblk_t *m = ms_queue_peek_first(inputQueue); !ms_queue_end(inputQueue, m);
				     m = ms_queue_peek_next(inputQueue, m)) {

					if (!volumes.empty() && !volumesSent) {
						// Small optimization here, we inject the volumes into the input's first packet and not the
						// duplication. So only the first output transfer() will inject the volumes then it will already
						// be there.
						if (uint8_t * data;
						    rtp_get_extension_header(m, RTP_EXTENSION_MIXER_TO_CLIENT_AUDIO_LEVEL, &data) == -1) {
							// If we don't find it then we must be in the first output transfer(), inject it.
							rtp_add_mixer_to_client_audio_level(m, RTP_EXTENSION_MIXER_TO_CLIENT_AUDIO_LEVEL,
							                                    volumes.size(), volumes.data());
						}

						volumesSent = true;
					}

					ms_queue_put(outputQueue, copymsg(m));
				}
			}
		}
	}
}

#ifdef VIDEO_ENABLED
RouterVideoOutput::RouterVideoOutput(PacketRouter *router, int pin) : RouterOutput(router, pin) {
}

void RouterVideoOutput::configure(const MSPacketRouterPinData *pinData) {
	RouterOutput::configure(pinData);

	mCurrentSource = pinData->input;
	mActiveSpeakerEnabled = pinData->active_speaker_enabled;

	if (mActiveSpeakerEnabled) {
		mCurrentSource = -1;

		if (int focus = mRouter->getFocusPin(); focus != mSelfSource) {
			mNextSource = focus;
		} else {
			// We will elect another source in process() function
			mNextSource = -1;
		}
	}

	ms_message("PacketRouter: Configure active_speaker[%d] pin output %d with input %d, next_source %d",
	           pinData->active_speaker_enabled, pinData->output, pinData->input, mNextSource);
}

void RouterVideoOutput::transfer() {
	MSQueue *outputQueue = mRouter->getOutputQueue(mPin);

	if (outputQueue != nullptr && mCurrentSource != -1) {
		const auto input = dynamic_cast<RouterVideoInput *>(mRouter->getRouterInput(mCurrentSource));

		if (input && input->mState == RouterVideoInput::State::Running) {
			MSQueue *inputQueue = mRouter->getInputQueue(mCurrentSource);

			if (inputQueue == nullptr || ms_queue_empty(inputQueue)) return;

			mblk_t *start = input->mKeyFrameStart ? input->mKeyFrameStart : ms_queue_peek_first(inputQueue);

			for (mblk_t *m = start; !ms_queue_end(inputQueue, m); m = ms_queue_peek_next(inputQueue, m)) {
				mblk_t *o = dupmsg(m);

				// Only re-write packet information if full packet mode is disabled
				if (!mRouter->isFullPacketModeEnabled()) {
					if (mblk_get_timestamp_info(m) != mOutTimestamp) {
						// Each time we observe a new input timestamp, we must select a new output timestamp
						mOutTimestamp = mblk_get_timestamp_info(m);
						mAdjustedOutTimestamp = static_cast<uint32_t>(mRouter->getTime() * 90LL);
					}

					/* We need to set sequence number for what we send out, otherwise the VP8 decoder won't be able
					 * to verify the integrity of the stream */

					mblk_set_timestamp_info(o, mAdjustedOutTimestamp);
					mblk_set_cseq(o, mOutSeqNumber++);
					mblk_set_marker_info(o, mblk_get_marker_info(m));
				}

				ms_queue_put(outputQueue, o);
			}
		}
	}
}
#endif

// =============================================================================

RouterInputSelector::RouterInputSelector(PacketRouter *router) : mRouter(router) {
}

RouterInputAudioSelector::RouterInputAudioSelector(PacketRouter *router) : RouterInputSelector(router) {
}

void RouterInputAudioSelector::select() {
	mSelected = {};

	// If only two, select them anyway to always have audio.
	bool onlyTwo = mRouter->getRouterInputsSize() == 2;

	for (int i = 0; i < mRouter->getRouterInputsSize(); ++i) {
		auto audioInput = dynamic_cast<RouterAudioInput *>(mRouter->getRouterInput(i));

		if (audioInput != nullptr && (onlyTwo || audioInput->mIsSpeaking)) mSelected.push_back(audioInput);
	}

	if (!onlyTwo) {
		// Sort by descending volume order
		std::sort(mSelected.begin(), mSelected.end(), [](auto a, auto b) { return a->mVolume > b->mVolume; });

		// Keep the first MAX_SPEAKER_AT_ONCE volumes
		if (mSelected.size() > PacketRouter::MAX_SPEAKER_AT_ONCE) mSelected.resize(PacketRouter::MAX_SPEAKER_AT_ONCE);
	}
}

const std::vector<RouterAudioInput *> &RouterInputAudioSelector::getSelectedInputs() const {
	return mSelected;
}

#ifdef VIDEO_ENABLED
RouterInputVideoSelector::RouterInputVideoSelector(PacketRouter *router) : RouterInputSelector(router) {
}

void RouterInputVideoSelector::select() {
	for (int i = 0; i < mRouter->getRouterOutputsSize(); ++i) {
		auto videoOutput = dynamic_cast<RouterVideoOutput *>(mRouter->getRouterOutput(i));

		if (videoOutput != nullptr && videoOutput->mActiveSpeakerEnabled) {
			if (videoOutput->mNextSource != -1 && mRouter->getInputQueue(videoOutput->mNextSource) == nullptr) {
				ms_warning("PacketRouter: Next source %i disappeared, choosing another one", videoOutput->mNextSource);
				videoOutput->mNextSource = -1;
			}

			if (videoOutput->mCurrentSource != -1 && mRouter->getInputQueue(videoOutput->mCurrentSource) == nullptr) {
				ms_warning("PacketRouter: Current source %i disappeared", videoOutput->mCurrentSource);
				// Invalidate the current source until the next switch
				videoOutput->mCurrentSource = -1;
			}

			if (videoOutput->mNextSource == -1) {
				if (electNewSource(videoOutput) != -1) {
					ms_message(
					    "PacketRouter: New source automatically selected for output pin [%d]: next source = [%i]", i,
					    videoOutput->mNextSource);
				}
			}

			if (videoOutput->mCurrentSource != videoOutput->mNextSource && videoOutput->mNextSource != -1) {
				// This output is waiting for a key-frame to start
				auto videoInput = dynamic_cast<RouterVideoInput *>(mRouter->getRouterInput(videoOutput->mNextSource));
				if (videoInput) {
					if (videoInput->mKeyFrameStart != nullptr) {
						MSPacketRouterSwitchedEventData eventData;

						eventData.output = i;
						eventData.input = videoOutput->mNextSource;

						// The input just got a key frame, we can switch !
						videoOutput->mCurrentSource = videoOutput->mNextSource;
						mRouter->notifyOutputSwitched(eventData);
					} else {
						// Else request a key frame
						if (!videoInput->mKeyFrameRequested) {
							ms_message("PacketRouter: Need key-frame for pin %i", videoOutput->mNextSource);
							videoInput->mKeyFrameRequested = true;
						}
					}
				}
			}
		}
	}
}

int RouterInputVideoSelector::electNewSource(mediastreamer::RouterVideoOutput *output) {
	if (!output->mActiveSpeakerEnabled) {
		ms_error("PacketRouter: electNewSource should only be called for active speaker case");
		return -1;
	}

	if (int focus = mRouter->getFocusPin();
	    output->mSelfSource != focus && focus != -1 && mRouter->getInputQueue(focus) != nullptr) {
		// Show the active speaker
		output->mNextSource = focus;
		return output->mNextSource;
	}

	int current;
	if (output->mNextSource != -1) {
		current = output->mNextSource;
	} else if (output->mCurrentSource != -1) {
		current = output->mCurrentSource;
	} else {
		current = output->mSelfSource;
	}

	// Show somebody else, but not us
	int inputs = static_cast<int>(mRouter->getRouterInputsSize());
	for (int k = current + 1; k < current + 1 + inputs; k++) {
		int nextPin = k % inputs;
		if (mRouter->getInputQueue(nextPin) != nullptr && nextPin != output->mSelfSource &&
		    mRouter->getRouterInput(nextPin) != nullptr) {
			output->mNextSource = nextPin;
			return output->mNextSource;
		}
	}

	// Otherwise, show nothing
	output->mNextSource = -1;
	return output->mNextSource;
}
#endif

// =============================================================================

PacketRouter::PacketRouter(MSFilter *f) : FilterBase(f) {
}

void PacketRouter::preprocess() {
	// Make sure all connected inputs are created
	for (int i = 0; i < ROUTER_MAX_INPUT_CHANNELS; ++i) {
		MSQueue *q = getInput(i);
		if (q) {
			createInputIfNotExists(i);
		}
	}

#ifdef VIDEO_ENABLED
	if (mRoutingMode == RoutingMode::Video && mFocusPin != -1) {
		// Re-apply the focus selection in case of reconnection.
		// Indeed, some new contributors may appear, in which case the placeholder is no longer needed
		for (const auto &output : mOutputs) {
			auto videoOutput = dynamic_cast<RouterVideoOutput *>(output.get());
			if (videoOutput != nullptr) {
				setFocus(mFocusPin);
				break;
			}
		}
	}
#endif
}

void PacketRouter::process() {
	lock();

	// Update all current inputs.
	for (const auto &input : mInputs) {
		if (input != nullptr) input->update();
	}

	// Update the volumes to send
	if (mRoutingMode == RoutingMode::Audio) {
		updateVolumesToSend();
	}

	// Select which one(s) we will use.
	mSelector->select();

	// If the output's linked input is selected, then transfer the stream.
	for (const auto &output : mOutputs) {
		if (output != nullptr) output->transfer();
	}

	// Flush the rest.
	for (int i = 0; i < ROUTER_MAX_INPUT_CHANNELS; ++i) {
		MSQueue *q = getInput(i);
		if (q) ms_queue_flush(q);
	}

	unlock();
}

void PacketRouter::postprocess() {
}

void PacketRouter::setRoutingMode(PacketRouter::RoutingMode mode) {
	mRoutingMode = mode;

	switch (mRoutingMode) {
		case PacketRouter::RoutingMode::Audio:
			mFullPacketMode = true; // This does nothing for audio mode because it always uses full packets.
			mSelector = make_unique<RouterInputAudioSelector>(this);
			break;
		case PacketRouter::RoutingMode::Video:
#ifdef VIDEO_ENABLED
			mSelector = make_unique<RouterInputVideoSelector>(this);
#else
			ms_error("PacketRouter: Cannot set mode to video as it is disabled");
#endif
			break;
		default:
			break;
	}
}

PacketRouter::RoutingMode PacketRouter::getRoutingMode() const {
	return mRoutingMode;
}

void PacketRouter::enableFullPacketMode(bool enable) {
	mFullPacketMode = enable;
}

bool PacketRouter::isFullPacketModeEnabled() const {
	return mFullPacketMode;
}

RouterInput *PacketRouter::getRouterInput(int index) const {
	if (index < 0 || (size_t)index > ROUTER_MAX_INPUT_CHANNELS) {
		return nullptr;
	}

	try {
		const auto &input = mInputs.at(index);
		return input != nullptr ? input.get() : nullptr;
	} catch (const std::out_of_range &) {
		ms_error("PacketRouter: Trying to get router input on un-existing pin");
	}

	return nullptr;
}

RouterOutput *PacketRouter::getRouterOutput(int index) const {
	if (index < 0 || (size_t)index > mOutputs.size()) {
		return nullptr;
	}

	try {
		const auto &output = mOutputs.at(index);
		return output != nullptr ? output.get() : nullptr;
	} catch (const std::out_of_range &) {
		ms_error("PacketRouter: Trying to get router output on un-existing pin");
	}

	return nullptr;
}

RouterInputSelector *PacketRouter::getRouterInputSelector() const {
	return mSelector.get();
}

int PacketRouter::getRouterInputsSize() const {
	return static_cast<int>(mInputs.size());
}

int PacketRouter::getRouterOutputsSize() const {
	return static_cast<int>(mOutputs.size());
}

MSQueue *PacketRouter::getInputQueue(int pin) const {
	return getInput(pin);
}

MSQueue *PacketRouter::getOutputQueue(int pin) const {
	return getOutput(pin);
}

uint64_t PacketRouter::getTime() const {
	return FilterBase::getTime();
}

void PacketRouter::configureOutput(const MSPacketRouterPinData *pinData) {
	if (mRoutingMode == RoutingMode::Unknown) {
		ms_error("PacketRouter: Trying to configure an output while mode has not yet been set");
		return;
	}

	lock();

	// Resize if output pin exceeds the outputs size
	if (static_cast<size_t>(pinData->output) + 1 > mOutputs.size()) {
		mOutputs.resize(pinData->output + 1);
	}

	// Create it if not already exists
	if (mOutputs[pinData->output] == nullptr) {
		unique_ptr<RouterOutput> output;
		if (mRoutingMode == RoutingMode::Audio) {
			output = make_unique<RouterAudioOutput>(this, pinData->output);
		} else {
#ifdef VIDEO_ENABLED
			output = make_unique<RouterVideoOutput>(this, pinData->output);
#endif
		}

		mOutputs[pinData->output] = std::move(output);
	}

	mOutputs[pinData->output]->configure(pinData);

	if (pinData->input != -1) {
		createInputIfNotExists(pinData->input);
	}

	unlock();
}

void PacketRouter::unconfigureOutput(int pin) {
	if (static_cast<size_t>(pin) > mOutputs.size()) return;

	lock();

	try {
		mOutputs.at(pin) = nullptr;
	} catch (const std::out_of_range &) {
		ms_error("PacketRouter: Trying to unconfigure output on un-existing pin");
	}

	unlock();

	ms_message("PacketRouter: Unconfigure output pin %i", pin);
}

void PacketRouter::setAsLocalMember(const MSPacketRouterPinControl *pinControl) {
	ms_message("PacketRouter: Pin #%i local member attribute: %i", pinControl->pin, pinControl->enabled);

	// If the input does not exist create it first, so we can configure it
	createInputIfNotExists(pinControl->pin);

	mInputs[pinControl->pin]->mLocal = pinControl->enabled;
}

const std::vector<rtp_audio_level_t> &PacketRouter::getVolumesToSend() const {
	return mVolumesToSend;
}

#ifdef VIDEO_ENABLED
void PacketRouter::setFocus(int pin) {
	if (mRoutingMode != RoutingMode::Video) {
		ms_error("PacketRouter: Trying to set focus while not in video mode");
		return;
	}

	int requestedPin = pin;

	ms_message("PacketRouter: Set focus %d", pin);

	for (size_t i = 0; i < mOutputs.size(); ++i) {
		auto output = dynamic_cast<RouterVideoOutput *>(mOutputs[i].get());
		if (output != nullptr && output->mActiveSpeakerEnabled) {
			// Never send back self contribution.
			if (requestedPin != output->mSelfSource) {
				output->mNextSource = pin;
			}
			ms_message("PacketRouter: This pin %zu self_source[%d], current_source %d and next_source %d", i,
			           output->mSelfSource, output->mCurrentSource, output->mNextSource);
		}
	}

	mFocusPin = pin;
}

int PacketRouter::getFocusPin() const {
	return mFocusPin;
}

void PacketRouter::notifyPli(int pin) {
	if (mRoutingMode != RoutingMode::Video) {
		ms_error("PacketRouter: Trying to notify PLI while not in video mode");
		return;
	}

	notify(MS_PACKET_ROUTER_SEND_PLI, &pin);
}

void PacketRouter::notifyFir(int pin) {
	if (mRoutingMode != RoutingMode::Video) {
		ms_error("PacketRouter: Trying to notify FIR while not in video mode");
		return;
	}

	notify(MS_PACKET_ROUTER_SEND_FIR, &pin);
}

void PacketRouter::notifyOutputSwitched(MSPacketRouterSwitchedEventData event) {
	notify(MS_PACKET_ROUTER_OUTPUT_SWITCHED, &event);
}

void PacketRouter::setInputFmt(const MSFmtDescriptor *format) {
	if (mRoutingMode != RoutingMode::Video) {
		ms_error("PacketRouter: Trying to set input format while not in video mode");
		return;
	}

	if (!mInputs.empty()) {
		ms_error("PacketRouter: Input format has to be set before configuring any inputs");
		return;
	}

	mEncoding = format->encoding;
}
#endif

void PacketRouter::createInputIfNotExists(int index) {
	if (index < 0 || (size_t)index > ROUTER_MAX_INPUT_CHANNELS) {
		return;
	}

	// Resize the vector if the input pin needed exceeds the number of current inputs
	if (static_cast<size_t>(index) + 1 > mInputs.size()) {
		mInputs.resize(index + 1);
	}

	// Create the input if needed
	if (mInputs[index] == nullptr) {
		unique_ptr<RouterInput> input;
		if (mRoutingMode == RoutingMode::Audio) {
			input = make_unique<RouterAudioInput>(this, index);
		} else {
#ifdef VIDEO_ENABLED
			input = make_unique<RouterVideoInput>(this, index, mEncoding, mFullPacketMode);
#endif
		}

		mInputs[index] = std::move(input);
	}
}

void PacketRouter::updateVolumesToSend() {
	// Clear and reserve is more efficient to keep this vector updated.
	// Order is guaranteed to be the same as we iterate through inputs and reserve will allocate what is needed one time
	// and then will do nothing if already at the good capacity.
	mVolumes.clear();
	mVolumes.reserve(mInputs.size());

	for (const auto &input : mInputs) {
		if (input == nullptr) continue;

		auto audioInput = dynamic_cast<RouterAudioInput *>(input.get());
		if (audioInput) mVolumes.push_back({audioInput->mSsrc, audioInput->mVolume});
	}

	if (mVolumesSentIndex > 0) {
		int size = static_cast<int>(mVolumes.size());

		// In case of mass disconnections. If we are past the size then stop, and we will send volumes again next time.
		if (mVolumesSentIndex >= size) {
			mVolumesSentIndex = 0;
			mVolumesToSend.clear();
			return;
		}

		if (mVolumesSentIndex + RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL >= size) {
			mVolumesToSend.assign(mVolumes.begin() + mVolumesSentIndex, mVolumes.end());
			mVolumesSentIndex = 0;
		} else {
			auto begin = mVolumes.begin() + mVolumesSentIndex;
			mVolumesToSend.assign(begin, begin + RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL);
			mVolumesSentIndex += RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL;
		}
	} else if (uint64_t time = getTime(); time - mLastVolumesSentTime > mVolumesSentInterval) {
		if (static_cast<int>(mVolumes.size()) <= RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL) {
			mVolumesToSend = mVolumes;
		} else {
			mVolumesToSend.assign(mVolumes.begin(), mVolumes.begin() + RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL);
			mVolumesSentIndex = RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL;
		}

		mLastVolumesSentTime = time;
	} else if (!mVolumesToSend.empty()) {
		// If we are here, it means we just sent the last volumes, and we have to wait for the timer.
		mVolumesToSend.clear();
	}
}

// =============================================================================

int PacketRouterFilterWrapper::onSetRoutingMode(MSFilter *f, void *arg) {
	try {
		PacketRouter::RoutingMode mode;

		switch (auto desiredMode = *static_cast<MSPacketRouterMode *>(arg)) {
			case MS_PACKET_ROUTER_MODE_AUDIO:
				mode = PacketRouter::RoutingMode::Audio;
				break;
			case MS_PACKET_ROUTER_MODE_VIDEO:
				mode = PacketRouter::RoutingMode::Video;
				break;
			default:
				ms_error("PacketRouter: Unknown routing mode (%d)", static_cast<int>(desiredMode));
				return -1;
		}

		static_cast<PacketRouter *>(f->data)->setRoutingMode(mode);
		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}

int PacketRouterFilterWrapper::onEnableFullPacketMode(MSFilter *f, void *arg) {
	try {
		static_cast<PacketRouter *>(f->data)->enableFullPacketMode(*static_cast<bool_t *>(arg));
		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}

int PacketRouterFilterWrapper::onConfigureOutput(MSFilter *f, void *arg) {
	try {
		const auto pd = static_cast<MSPacketRouterPinData *>(arg);

		if (pd->output < 0 || pd->output >= ROUTER_MAX_OUTPUT_CHANNELS) {
			ms_warning("PacketRouter: Cannot configure output with invalid pin");
			return -1;
		}

		static_cast<PacketRouter *>(f->data)->configureOutput(pd);
		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}

int PacketRouterFilterWrapper::onUnconfigureOutput(MSFilter *f, void *arg) {
	try {
		int pin = *static_cast<int *>(arg);

		if (pin < 0 || pin >= ROUTER_MAX_OUTPUT_CHANNELS) {
			ms_warning("PacketRouter: Cannot unconfigure output with invalid pin");
			return -1;
		}

		static_cast<PacketRouter *>(f->data)->unconfigureOutput(pin);
		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}

int PacketRouterFilterWrapper::onSetAsLocalMember(MSFilter *f, void *arg) {
	try {
		const MSPacketRouterPinControl *pc = static_cast<MSPacketRouterPinControl *>(arg);
		if (pc->pin < 0 || pc->pin >= f->desc->ninputs) {
			ms_error("PacketRouter: Invalid argument to MS_PACKET_ROUTER_SET_AS_LOCAL_MEMBER");
			return -1;
		}

		static_cast<PacketRouter *>(f->data)->setAsLocalMember(pc);
		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}

#ifdef VIDEO_ENABLED
int PacketRouterFilterWrapper::onSetFocus(MSFilter *f, void *arg) {
	try {
		int pin = *static_cast<int *>(arg);
		if (pin < 0 || pin >= f->desc->ninputs) {
			ms_warning("PacketRouter: set_focus invalid pin number %i", pin);
			return -1;
		}

		auto router = static_cast<PacketRouter *>(f->data);
		if (pin != router->getFocusPin()) {
			ms_message("PacketRouter: Focus requested on pin %i", pin);
			ms_filter_lock(f);
			router->setFocus(pin);
			ms_filter_unlock(f);
		}

		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}

int PacketRouterFilterWrapper::onNotifyPli(MSFilter *f, void *arg) {
	try {
		auto router = static_cast<PacketRouter *>(f->data);
		if (router->getRoutingMode() != PacketRouter::RoutingMode::Video) {
			ms_error("PacketRouter: Trying to call MS_PACKET_ROUTER_NOTIFY_PLI while not in video mode");
			return -1;
		}

		int pin = *static_cast<int *>(arg);
		if (pin < 0 || pin >= ROUTER_MAX_OUTPUT_CHANNELS) {
			ms_error("PacketRouter: Invalid argument to MS_PACKET_ROUTER_NOTIFY_PLI");
			return -1;
		}

		// Propagate the PLI to the current input source.
		auto output = dynamic_cast<RouterVideoOutput *>(router->getRouterOutput(pin));
		if (output == nullptr) {
			ms_error("PacketRouter: Cannot notify PLI, output on pin %d does not exist", pin);
			return -1;
		}

		if (int sourcePin = output->getCurrentSource(); sourcePin != -1) router->notifyPli(sourcePin);

		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}

int PacketRouterFilterWrapper::onNotifyFir(MSFilter *f, void *arg) {
	try {
		auto router = static_cast<PacketRouter *>(f->data);
		if (router->getRoutingMode() != PacketRouter::RoutingMode::Video) {
			ms_error("PacketRouter: Trying to call MS_PACKET_ROUTER_NOTIFY_FIR while not in video mode");
			return -1;
		}

		int pin = *static_cast<int *>(arg);
		if (pin < 0 || pin >= ROUTER_MAX_OUTPUT_CHANNELS) {
			ms_error("PacketRouter: Invalid argument to MS_PACKET_ROUTER_NOTIFY_FIR");
			return -1;
		}

		// Propagate the FIR to the current input source.
		auto output = dynamic_cast<RouterVideoOutput *>(router->getRouterOutput(pin));
		if (output == nullptr) {
			ms_error("PacketRouter: Cannot notify FIR, output on pin %d does not exist", pin);
			return -1;
		}

		if (int sourcePin = output->getCurrentSource(); sourcePin != -1) router->notifyFir(sourcePin);

		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}

int PacketRouterFilterWrapper::onSetInputFmt(MSFilter *f, void *arg) {
	try {
		const MSFmtDescriptor *format = static_cast<MSFmtDescriptor *>(arg);
		if (format) {
			if (strcasecmp(format->encoding, "VP8") != 0 && strcasecmp(format->encoding, "H264") != 0
#ifdef AV1_ENABLED
			    && strcasecmp(format->encoding, "AV1") != 0
#endif
			) {
				ms_error("PacketRouter: Unsupported format %s", format->encoding);
				return -1;
			}

			static_cast<PacketRouter *>(f->data)->setInputFmt(format);
		}

		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}
#endif

} // namespace mediastreamer

using namespace mediastreamer;

static MSFilterMethod MS_FILTER_WRAPPER_METHODS_NAME(PacketRouter)[] = {
    {MS_PACKET_ROUTER_SET_ROUTING_MODE, PacketRouterFilterWrapper::onSetRoutingMode},
    {MS_PACKET_ROUTER_ENABLE_FULL_PACKET_MODE, PacketRouterFilterWrapper::onEnableFullPacketMode},
    {MS_PACKET_ROUTER_CONFIGURE_OUTPUT, PacketRouterFilterWrapper::onConfigureOutput},
    {MS_PACKET_ROUTER_UNCONFIGURE_OUTPUT, PacketRouterFilterWrapper::onUnconfigureOutput},
    {MS_PACKET_ROUTER_SET_AS_LOCAL_MEMBER, PacketRouterFilterWrapper::onSetAsLocalMember},
#ifdef VIDEO_ENABLED
    {MS_PACKET_ROUTER_SET_FOCUS, PacketRouterFilterWrapper::onSetFocus},
    {MS_PACKET_ROUTER_NOTIFY_PLI, PacketRouterFilterWrapper::onNotifyPli},
    {MS_PACKET_ROUTER_NOTIFY_FIR, PacketRouterFilterWrapper::onNotifyFir},
    {MS_FILTER_SET_INPUT_FMT, PacketRouterFilterWrapper::onSetInputFmt},
#endif
    {0, nullptr}};

MS_FILTER_WRAPPER_FILTER_DESCRIPTION_BASE(PacketRouter,
                                          MS_PACKET_ROUTER_ID,
                                          "A filter that route streams from several inputs, used for conferencing.",
                                          MS_FILTER_OTHER,
                                          nullptr,
                                          ROUTER_MAX_INPUT_CHANNELS,
                                          ROUTER_MAX_OUTPUT_CHANNELS,
                                          PacketRouter,
                                          0)

MS_FILTER_DESC_EXPORT(ms_PacketRouter_desc)
