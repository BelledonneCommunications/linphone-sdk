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
#include <sstream>

#ifdef AV1_ENABLED
#include "av1/obu/obu-key-frame-indicator.h"
#endif
#include "filter-wrapper/filter-wrapper-base.h"
#include "key-frame-indicator/header-extension-key-frame-indicator.h"
#include "key-frame-indicator/vp8-key-frame-indicator.h"

using namespace std;

namespace {
/* EKT message type as defined in RFC8870 - section 4.1 */
const uint8_t EKT_MsgType_SHORT = 0x00;
} // namespace

namespace mediastreamer {

class PackerRouterLogContextualizer {
public:
	explicit PackerRouterLogContextualizer(const PacketRouter *packetRouter) {
		ostringstream os;

		os << "PacketRouter ";
		if (packetRouter->getRoutingMode() == PacketRouter::RoutingMode::Audio) {
			os << "Audio";
		} else if (packetRouter->getRoutingMode() == PacketRouter::RoutingMode::Video) {
			os << "Video";
		} else {
			os << "Unknown";
		}
		os << ", " << packetRouter;

		bctbx_push_log_tag(sTagIdentifier, os.str().c_str());
	}

	~PackerRouterLogContextualizer() {
		bctbx_pop_log_tag(sTagIdentifier);
	}

private:
	static constexpr char sTagIdentifier[] = "packet-router";
};

// =============================================================================

RouterInput::RouterInput(PacketRouter *router, int inputNumber) : mRouter(router), mPin(inputNumber) {
}

void RouterInput::configure(const MSPacketRouterPinData *pinData) {
	std::copy(std::begin(pinData->extension_ids), std::end(pinData->extension_ids), std::begin(mExtensionIds));
}

int RouterInput::getPin() const {
	return mPin;
}

int RouterInput::getExtensionId(int defaultExtensionId) const {
	return mExtensionIds[defaultExtensionId] > 0 ? mExtensionIds[defaultExtensionId] : defaultExtensionId;
}

RouterAudioInput::RouterAudioInput(PacketRouter *router, int inputNumber) : RouterInput(router, inputNumber) {
}

bool isRTCP(const uint8_t *packet) {
	const uint8_t version = (packet[0] >> 6); // Version always 2.
	const uint8_t packetType = packet[1];     // For RTCP, packetType indicates the type of report.

	return (version == 2 && (packetType >= 72 && packetType <= 76));
}

bool queueContainsOnlyRtcp(MSQueue *queue) {
	for (mblk_t *m = ms_queue_peek_first(queue); !ms_queue_end(queue, m); m = ms_queue_peek_next(queue, m)) {
		if (!isRTCP(m->b_rptr)) {
			return false;
		}
	}
	return true;
}

void RouterAudioInput::update() {
	MSQueue *queue = mRouter->getInputQueue(mPin);
	if (queue == nullptr || ms_queue_empty(queue)) return;

	if (!mRouter->isFullPacketModeEnabled()) return;

	// Reset this state for the upcoming selection
	mNeedsToBeSent = false;

	// Always send RTCP
	if (queueContainsOnlyRtcp(queue)) {
		mNeedsToBeSent = true;
		return;
	}

	for (mblk_t *m = ms_queue_peek_first(queue); !ms_queue_end(queue, m); m = ms_queue_peek_next(queue, m)) {
		mSsrc = rtp_get_ssrc(m);

		bool_t voiceActivity = FALSE;
		int newVolume = rtp_get_client_to_mixer_audio_level(
		    m, getExtensionId(RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL), &voiceActivity);

		if (newVolume != RTP_AUDIO_LEVEL_NO_VOLUME) {
			newVolume = static_cast<int>(ms_volume_dbov_to_dbm0(newVolume));

			// First received packet is mute or we just muted ourselves
			if ((mVolume == -1 && newVolume == MS_VOLUME_DB_MUTED) ||
			    (mVolume != MS_VOLUME_DB_MUTED && newVolume == MS_VOLUME_DB_MUTED)) {
				mNeedsToBeSent = true;
				mLastMutedSentTime = mRouter->getTime();
			}

			// We are still muted
			if (mVolume == MS_VOLUME_DB_MUTED && newVolume == MS_VOLUME_DB_MUTED) {
				// Check if we have a short EKT tag (packet last byte is 0x00) when end to end ecryption is enabled.
				// In that case, do not mark the packet to be sent, if the timer expired,
				// we'll try again on the next one until we find a packet with a full EKT tag.
				if (!mRouter->isEndToEndEncryptionEnabled() || m->b_rptr[msgdsize(m) - 1] != EKT_MsgType_SHORT) {
					// Wait at least sMutedSentInterval before sending this information again
					// This is only needed for participants that joins after an input is muted to make sure they know
					if (const auto time = mRouter->getTime(); time - mLastMutedSentTime > sMutedSentInterval) {
						mNeedsToBeSent = true;
						mLastMutedSentTime = mRouter->getTime();
					}
				}
			}

			// We just unmuted ourselves
			if (mVolume == MS_VOLUME_DB_MUTED && newVolume != MS_VOLUME_DB_MUTED) {
				mNeedsToBeSent = true;
			}

			mVolume = newVolume;
			mIsSpeaking = voiceActivity;
		}
	}
}

#ifdef VIDEO_ENABLED
RouterVideoInput::RouterVideoInput(PacketRouter *router,
                                   int inputNumber,
                                   const std::string &encoding,
                                   bool endToEndEcryption)
    : RouterInput(router, inputNumber) {
	if (endToEndEcryption) {
		mKeyFrameIndicator = make_unique<HeaderExtensionKeyFrameIndicator>();
	} else if (encoding == "VP8") {
		mKeyFrameIndicator = make_unique<VP8KeyFrameIndicator>();
	} else if (encoding == "AV1") {
#ifdef AV1_ENABLED
		mKeyFrameIndicator = make_unique<ObuKeyFrameIndicator>();
#else
		PackerRouterLogContextualizer prlc(router);
		ms_error("Trying to create a video input in AV1 while it is disabled");
#endif
	}
}

void RouterVideoInput::configure(const MSPacketRouterPinData *pinData) {
	RouterInput::configure(pinData);

	// If we have an HeaderExtensionKeyFrameIndicator, update the extension id.
	if (auto headerExtensionKeyFrameIndicator =
	        dynamic_cast<HeaderExtensionKeyFrameIndicator *>(mKeyFrameIndicator.get());
	    headerExtensionKeyFrameIndicator != nullptr) {
		headerExtensionKeyFrameIndicator->setFrameMarkingExtensionId(getExtensionId(RTP_EXTENSION_FRAME_MARKING));
	}
}

void RouterVideoInput::update() {
	MSQueue *queue = mRouter->getInputQueue(mPin);
	if (queue == nullptr) return;

	mKeyFrameStart = nullptr;

	for (mblk_t *m = ms_queue_peek_first(queue); !ms_queue_end(queue, m); m = ms_queue_peek_next(queue, m)) {
		uint32_t newTimestamp;
		uint16_t newSeqNumber;

		if (mRouter->isFullPacketModeEnabled()) {
			newTimestamp = rtp_get_timestamp(m);
			newSeqNumber = rtp_get_seqnumber(m);
		} else {
			newTimestamp = mblk_get_timestamp_info(m);
			newSeqNumber = mblk_get_cseq(m);
		}

		if (!mSeqNumberSet) {
			mState = State::Stopped;
			mKeyFrameRequested = true;
		} else if (!mLocal && (newSeqNumber != mCurrentSeqNumber + 1)) {
			PackerRouterLogContextualizer prlc(mRouter);
			ms_warning("Sequence discontinuity detected on pin %i, key-frame requested", mPin);
			mState = State::Stopped;
			mKeyFrameRequested = true;
		}

		if (mKeyFrameRequested) {
			if (!mSeqNumberSet || mCurrentTimestamp != newTimestamp) {
				// Possibly a beginning of frame !
				if (mKeyFrameIndicator->isKeyFrame(m)) {
					PackerRouterLogContextualizer prlc(mRouter);
					ms_message("Key frame detected on pin %i", mPin);
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

void RouterOutput::configure(const MSPacketRouterPinData *pinData) {
	mSelfSource = pinData->self;
	std::copy(std::begin(pinData->extension_ids), std::end(pinData->extension_ids), std::begin(mExtensionIds));
}

void RouterOutput::rewritePacketInformation(mblk_t *source, mblk_t *output) {
	if (mblk_get_timestamp_info(source) != mOutTimestamp) {
		if (mRouter->getRoutingMode() == PacketRouter::RoutingMode::Video) {
			// Each time we observe a new input timestamp, we must select a new output timestamp
			mOutTimestamp = mblk_get_timestamp_info(source);
			mAdjustedOutTimestamp = static_cast<uint32_t>(mRouter->getTime() * 90LL);
		} else {
			// In audio as it can only work for 2 since client-side is not implemented
			// just set source timestamp
			mOutTimestamp = mAdjustedOutTimestamp = mblk_get_timestamp_info(source);
		}
	}

	// We need to set sequence number for what we send out, otherwise the decoder won't be able
	// to verify the integrity of the stream

	mblk_set_timestamp_info(output, mAdjustedOutTimestamp);
	mblk_set_cseq(output, mOutSeqNumber++);
	mblk_set_marker_info(output, mblk_get_marker_info(source));
}

void RouterOutput::rewriteExtensionIds(mblk_t *output, int inputIds[16], int outputIds[16]) {
	int mapping[16] = {};
	bool mappingRequired = false;

	// 0 is no extension and 15 is reserved by the RFC
	for (int i = 1; i < 15; i++) {
		if (inputIds[i] > 0 && outputIds[i] > 0 && inputIds[i] != outputIds[i]) {
			// If any is not the same put it in the mapping array
			mapping[inputIds[i]] = outputIds[i];
			mappingRequired = true;
		}
	}

	if (mappingRequired) rtp_remap_header_extension_ids(output, mapping);
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

		if (selector != nullptr) {
			// Send all selected inputs that are not us.
			for (const auto input : selector->getSelectedInputs()) {
				if (input != nullptr && mSelfSource != input->getPin()) {
					MSQueue *inputQueue = mRouter->getInputQueue(input->getPin());

					if (inputQueue == nullptr || ms_queue_empty(inputQueue)) continue;

					for (mblk_t *m = ms_queue_peek_first(inputQueue); !ms_queue_end(inputQueue, m);
					     m = ms_queue_peek_next(inputQueue, m)) {

						mblk_t *o = copymsg(m);

						if (!mRouter->isFullPacketModeEnabled()) {
							rewritePacketInformation(m, o);
						} else {
							rewriteExtensionIds(o, input->mExtensionIds, mExtensionIds);
						}

						ms_queue_put(outputQueue, o);
					}
				}
			}
		}
	}
}

#ifdef VIDEO_ENABLED
RouterVideoOutput::RouterVideoOutput(PacketRouter *router, int pin) : RouterOutput(router, pin) {
}

void RouterVideoOutput::configure(const MSPacketRouterPinData *pinData) {
	PackerRouterLogContextualizer prlc(mRouter);

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

	ms_message("Configure active_speaker[%d] pin output %d with input %d, next_source %d",
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
				mblk_t *o = copymsg(m);

				// Only re-write packet information if full packet mode is disabled
				if (!mRouter->isFullPacketModeEnabled()) {
					rewritePacketInformation(m, o);
				} else {
					rewriteExtensionIds(o, input->mExtensionIds, mExtensionIds);
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
	mSelected.clear();

	if (mRouter->isFullPacketModeEnabled()) {
		auto speakers = vector<RouterAudioInput *>{};

		// If only two, select them anyway to always have audio.
		const int activeParticipants = mRouter->getRouterActiveInputs();

		for (int i = 0; i < mRouter->getRouterInputsSize(); ++i) {
			auto audioInput = dynamic_cast<RouterAudioInput *>(mRouter->getRouterInput(i));

			if (audioInput != nullptr) {
				// No selection when we are only two
				if (activeParticipants == 2) mSelected.insert(audioInput);

				if (activeParticipants > 2) {
					// Keep all the speakers before determining which to send
					if (audioInput->mIsSpeaking) speakers.push_back(audioInput);

					// Select directly all the inputs that we need, there is two cases:
					// - An input just unmuted itself, we have to notify now
					// - An input muted itself, we have to notify now and every x second (for inputs that will connect
					//   after the event)
					if (audioInput->mNeedsToBeSent) mSelected.insert(audioInput);
				}
			}
		}

		if (activeParticipants > 2 && !speakers.empty()) {
			// Sort by descending volume order
			// Keep the first MAX_SPEAKER_AT_ONCE speakers
			if (speakers.size() > PacketRouter::MAX_SPEAKER_AT_ONCE) {
				std::sort(speakers.begin(), speakers.end(), [](auto a, auto b) { return a->mVolume > b->mVolume; });
				speakers.resize(PacketRouter::MAX_SPEAKER_AT_ONCE);
			}

			// Keep the pin of the active speaker which is the front of the vector
			mActiveSpeakerPin = speakers.front()->getPin();

			// Insert them into the selected set
			// We don't have to worry about duplication as it is taken care of by the set
			mSelected.insert(speakers.begin(), speakers.end());
		}
	} else {
		// If not in full packet mode, we send all audio.
		for (int i = 0; i < mRouter->getRouterInputsSize(); ++i) {
			auto audioInput = dynamic_cast<RouterAudioInput *>(mRouter->getRouterInput(i));

			if (audioInput != nullptr) mSelected.insert(audioInput);
		}
	}
}

const std::set<RouterAudioInput *> &RouterInputAudioSelector::getSelectedInputs() const {
	return mSelected;
}

int RouterInputAudioSelector::getActiveSpeakerPin() const {
	return mActiveSpeakerPin;
}

void RouterInputAudioSelector::clearSelectedInputs() {
	mSelected.clear();
}

#ifdef VIDEO_ENABLED
RouterInputVideoSelector::RouterInputVideoSelector(PacketRouter *router) : RouterInputSelector(router) {
}

void RouterInputVideoSelector::select() {
	for (int i = 0; i < mRouter->getRouterOutputsSize(); ++i) {
		auto videoOutput = dynamic_cast<RouterVideoOutput *>(mRouter->getRouterOutput(i));

		if (videoOutput != nullptr && videoOutput->mActiveSpeakerEnabled) {
			if (videoOutput->mNextSource != -1 && mRouter->getInputQueue(videoOutput->mNextSource) == nullptr) {
				PackerRouterLogContextualizer prlc(mRouter);
				ms_warning("Next source %i disappeared, choosing another one", videoOutput->mNextSource);
				videoOutput->mNextSource = -1;
			}

			if (videoOutput->mCurrentSource != -1 && mRouter->getInputQueue(videoOutput->mCurrentSource) == nullptr) {
				PackerRouterLogContextualizer prlc(mRouter);
				ms_warning("Current source %i disappeared", videoOutput->mCurrentSource);
				// Invalidate the current source until the next switch
				videoOutput->mCurrentSource = -1;
			}

			if (videoOutput->mNextSource == -1) {
				if (electNewSource(videoOutput) != -1) {
					PackerRouterLogContextualizer prlc(mRouter);
					ms_message("New source automatically selected for output pin [%d]: next source = [%i]", i,
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

						// We only notify if we are not in packet mode, as it is only for adding the csrc of the
						// nextSource.
						if (!mRouter->isFullPacketModeEnabled()) mRouter->notifyOutputSwitched(eventData);
					} else {
						// Else request a key frame
						if (!videoInput->mKeyFrameRequested) {
							PackerRouterLogContextualizer prlc(mRouter);
							ms_message("Need key-frame for pin %i", videoOutput->mNextSource);
							videoInput->mKeyFrameRequested = true;
						}
					}
				}
			}
		}
	}
}

int RouterInputVideoSelector::electNewSource(mediastreamer::RouterVideoOutput *output) {
	PackerRouterLogContextualizer prlc(mRouter);

	if (!output->mActiveSpeakerEnabled) {
		ms_error("electNewSource should only be called for active speaker case");
		return output->mNextSource;
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

	// Select which one(s) we will use.
	mSelector->select();

	// Transfer the selected inputs' data.
	for (const auto &output : mOutputs) {
		if (output != nullptr) output->transfer();
	}

	// Flush the rest.
	for (size_t i = 0, j = 0; i < ROUTER_MAX_INPUT_CHANNELS && j < getConnectedInputsCount(); ++i) {
		MSQueue *q = getInput((int)i);
		if (q) {
			ms_queue_flush(q);
			j++;
		}
	}

	unlock();
}

void PacketRouter::postprocess() {
}

void PacketRouter::setRoutingMode(PacketRouter::RoutingMode mode) {
	mRoutingMode = mode;

	switch (mRoutingMode) {
		case PacketRouter::RoutingMode::Audio:
			mSelector = make_unique<RouterInputAudioSelector>(this);
			break;
		case PacketRouter::RoutingMode::Video:
#ifdef VIDEO_ENABLED
			mSelector = make_unique<RouterInputVideoSelector>(this);
#else
		{
			PackerRouterLogContextualizer prlc(this);
			ms_error("Cannot set mode to video as it is disabled");
		}
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

void PacketRouter::enableEndToEndEncryption(bool enable) {
	mEndToEndEncryptionEnabled = enable;
}

bool PacketRouter::isEndToEndEncryptionEnabled() const {
	return mEndToEndEncryptionEnabled;
}

RouterInput *PacketRouter::getRouterInput(int index) const {
	PackerRouterLogContextualizer prlc(this);

	if (index < 0 || (size_t)index > ROUTER_MAX_INPUT_CHANNELS) {
		return nullptr;
	}

	try {
		const auto &input = mInputs.at(index);
		return input != nullptr ? input.get() : nullptr;
	} catch (const std::out_of_range &) {
		ms_error("Trying to get router input on un-existing pin");
	}

	return nullptr;
}

RouterOutput *PacketRouter::getRouterOutput(int index) const {
	PackerRouterLogContextualizer prlc(this);

	if (index < 0 || (size_t)index > mOutputs.size()) {
		return nullptr;
	}

	try {
		const auto &output = mOutputs.at(index);
		return output != nullptr ? output.get() : nullptr;
	} catch (const std::out_of_range &) {
		ms_error("Trying to get router output on un-existing pin");
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

int PacketRouter::getRouterActiveInputs() const {
	return static_cast<int>(std::count_if(mInputs.begin(), mInputs.end(),
	                                      [](const unique_ptr<RouterInput> &input) { return input != nullptr; }));
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
	PackerRouterLogContextualizer prlc(this);

	if (mRoutingMode == RoutingMode::Unknown) {
		ms_error("Trying to configure an output while mode has not yet been set");
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

		mInputs[pinData->input]->configure(pinData);
	}

	unlock();
}

void PacketRouter::unconfigureOutput(int pin) {
	PackerRouterLogContextualizer prlc(this);

	if (static_cast<size_t>(pin) > mOutputs.size()) return;

	lock();

	try {
		// Retrieve the self source pin
		const auto &output = mOutputs.at(pin);
		int selfSource = output != nullptr ? output->getSelfSource() : -1;

		// Remove the output
		mOutputs.at(pin) = nullptr;

		// Remove the source if not in use anymore
		if (selfSource != -1) removeUnusedInput(selfSource);
	} catch (const std::out_of_range &) {
		ms_error("Trying to unconfigure output on un-existing pin");
	}

	unlock();

	ms_message("Unconfigure output pin %i", pin);
}

void PacketRouter::setAsLocalMember(const MSPacketRouterPinControl *pinControl) {
	PackerRouterLogContextualizer prlc(this);

	ms_message("Pin #%i local member attribute: %i", pinControl->pin, pinControl->enabled);

	// If the input does not exist create it first, so we can configure it
	createInputIfNotExists(pinControl->pin);

	mInputs[pinControl->pin]->mLocal = pinControl->enabled;
}

int PacketRouter::getActiveSpeakerPin() const {
	const auto &selector = dynamic_cast<RouterInputAudioSelector *>(mSelector.get());
	if (selector) {
		return selector->getActiveSpeakerPin();
	}

	return -1;
}

#ifdef VIDEO_ENABLED
void PacketRouter::setFocus(int pin) {
	PackerRouterLogContextualizer prlc(this);

	if (mRoutingMode != RoutingMode::Video) {
		ms_error("Trying to set focus while not in video mode");
		return;
	}

	ms_message("Set focus %d", pin);

	for (size_t i = 0; i < mOutputs.size(); ++i) {
		auto output = dynamic_cast<RouterVideoOutput *>(mOutputs[i].get());
		if (output != nullptr && output->mActiveSpeakerEnabled) {
			// Never send back self contribution.
			if (pin != output->mSelfSource) {
				output->mNextSource = pin;
			}
			ms_message("This pin %zu self_source[%d], current_source %d and next_source %d", i, output->mSelfSource,
			           output->mCurrentSource, output->mNextSource);
		}
	}

	mFocusPin = pin;
}

int PacketRouter::getFocusPin() const {
	return mFocusPin;
}

void PacketRouter::notifyPli(int pin) {
	if (mRoutingMode != RoutingMode::Video) {
		PackerRouterLogContextualizer prlc(this);
		ms_error("Trying to notify PLI while not in video mode");
		return;
	}

	notify(MS_PACKET_ROUTER_SEND_PLI, &pin);
}

void PacketRouter::notifyFir(int pin) {
	if (mRoutingMode != RoutingMode::Video) {
		PackerRouterLogContextualizer prlc(this);
		ms_error("Trying to notify FIR while not in video mode");
		return;
	}

	notify(MS_PACKET_ROUTER_SEND_FIR, &pin);
}

void PacketRouter::notifyOutputSwitched(MSPacketRouterSwitchedEventData event) {
	notify(MS_PACKET_ROUTER_OUTPUT_SWITCHED, &event);
}

void PacketRouter::setInputFmt(const MSFmtDescriptor *format) {
	PackerRouterLogContextualizer prlc(this);

	if (mRoutingMode != RoutingMode::Video) {
		ms_error("Trying to set input format while not in video mode");
		return;
	}

	if (!mInputs.empty()) {
		ms_error("Input format has to be set before configuring any inputs");
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
			input = make_unique<RouterVideoInput>(this, index, mEncoding, mEndToEndEncryptionEnabled);
#endif
		}

		mInputs[index] = std::move(input);
	}
}

void PacketRouter::removeUnusedInput(int index) {
	PackerRouterLogContextualizer prlc(this);

	try {
		if (mRoutingMode == RoutingMode::Audio) {
			// In audio mode, each output is tied to one input. We can remove the source.
			mInputs.at(index) = nullptr;

			// Clear the selected inputs if any to avoid choosing the one being removed.
			// Selection will be redone in the next process.
			auto audioSelector = dynamic_cast<RouterInputAudioSelector *>(mSelector.get());
			if (audioSelector && !audioSelector->getSelectedInputs().empty()) {
				audioSelector->clearSelectedInputs();
			}
		} else if (mRoutingMode == RoutingMode::Video) {
#ifdef VIDEO_ENABLED
			// In video mode, before removing the input we have to check that no other output is using the current
			// source anymore.
			bool stillInUse = false;
			for (const auto &output : mOutputs) {
				const auto videoOutput = dynamic_cast<RouterVideoOutput *>(output.get());
				if (videoOutput != nullptr && videoOutput->mCurrentSource == index) {
					stillInUse = true;
					break;
				}
			}
			if (!stillInUse) mInputs.at(index) = nullptr;
#endif
		}
	} catch (const std::out_of_range &) {
		ms_error("Trying to remove input on un-existing pin");
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

int PacketRouterFilterWrapper::onSetFullPacketModeEnabled(MSFilter *f, void *arg) {
	try {
		static_cast<PacketRouter *>(f->data)->enableFullPacketMode(*static_cast<bool_t *>(arg));
		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}

int PacketRouterFilterWrapper::onGetFullPacketModeEnabled(MSFilter *f, void *arg) {
	try {
		*static_cast<bool_t *>(arg) = static_cast<PacketRouter *>(f->data)->isFullPacketModeEnabled();
		return 0;
	} catch (const PacketRouter::MethodCallFailed &) {
		return -1;
	}
}

int PacketRouterFilterWrapper::onSetEndToEndEncryptionEnabled(MSFilter *f, void *arg) {
	try {
		static_cast<PacketRouter *>(f->data)->enableEndToEndEncryption(*static_cast<bool_t *>(arg));
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

int PacketRouterFilterWrapper::onGetActiveSpeakerPin(MSFilter *f, void *arg) {
	try {
		auto router = static_cast<PacketRouter *>(f->data);
		PackerRouterLogContextualizer prlc(router);

		if (router->getRoutingMode() != PacketRouter::RoutingMode::Audio) {
			ms_error("Trying to call MS_PACKET_ROUTER_GET_ACTIVE_SPEAKER_PIN while not in audio mode");
			return -1;
		}

		int *pin = (int *)arg;
		*pin = router->getActiveSpeakerPin();

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
			PackerRouterLogContextualizer prlc(router);
			ms_message("Focus requested on pin %i", pin);
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
			PackerRouterLogContextualizer prlc(router);
			ms_error("Trying to call MS_PACKET_ROUTER_NOTIFY_PLI while not in video mode");
			return -1;
		}

		int pin = *static_cast<int *>(arg);
		if (pin < 0 || pin >= ROUTER_MAX_OUTPUT_CHANNELS) {
			PackerRouterLogContextualizer prlc(router);
			ms_error("Invalid argument to MS_PACKET_ROUTER_NOTIFY_PLI");
			return -1;
		}

		// Propagate the PLI to the current input source.
		auto output = dynamic_cast<RouterVideoOutput *>(router->getRouterOutput(pin));
		if (output == nullptr) {
			PackerRouterLogContextualizer prlc(router);
			ms_error("Cannot notify PLI, output on pin %d does not exist", pin);
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
			PackerRouterLogContextualizer prlc(router);
			ms_error("Trying to call MS_PACKET_ROUTER_NOTIFY_FIR while not in video mode");
			return -1;
		}

		int pin = *static_cast<int *>(arg);
		if (pin < 0 || pin >= ROUTER_MAX_OUTPUT_CHANNELS) {
			PackerRouterLogContextualizer prlc(router);
			ms_error("Invalid argument to MS_PACKET_ROUTER_NOTIFY_FIR");
			return -1;
		}

		// Propagate the FIR to the current input source.
		auto output = dynamic_cast<RouterVideoOutput *>(router->getRouterOutput(pin));
		if (output == nullptr) {
			PackerRouterLogContextualizer prlc(router);
			ms_error("Cannot notify FIR, output on pin %d does not exist", pin);
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
    {MS_PACKET_ROUTER_SET_FULL_PACKET_MODE_ENABLED, PacketRouterFilterWrapper::onSetFullPacketModeEnabled},
    {MS_PACKET_ROUTER_GET_FULL_PACKET_MODE_ENABLED, PacketRouterFilterWrapper::onGetFullPacketModeEnabled},
    {MS_PACKET_ROUTER_SET_END_TO_END_ENCRYPTION_ENABLED, PacketRouterFilterWrapper::onSetEndToEndEncryptionEnabled},
    {MS_PACKET_ROUTER_CONFIGURE_OUTPUT, PacketRouterFilterWrapper::onConfigureOutput},
    {MS_PACKET_ROUTER_UNCONFIGURE_OUTPUT, PacketRouterFilterWrapper::onUnconfigureOutput},
    {MS_PACKET_ROUTER_SET_AS_LOCAL_MEMBER, PacketRouterFilterWrapper::onSetAsLocalMember},
    {MS_PACKET_ROUTER_GET_ACTIVE_SPEAKER_PIN, PacketRouterFilterWrapper::onGetActiveSpeakerPin},
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
