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

#include <bctoolbox/defs.h>

#include "baudot_context.h"
#include "baudot_decoding_context.h"
#include "baudot_filters.h"
#include "goertzel_state.h"
#include "mediastreamer2/baudot.h"
#include "mediastreamer2/msticker.h"

namespace mediastreamer {

static const float ENERGY_MIN_THRESHOLD = 0.01f;
static const int64_t NO_DETECTION_DURATION_MS = 300; // ms

constexpr static const float MIN_DETECTION_THRESHOLD = 0.88f;
constexpr static const float MAX_NO_DETECTION_THRESHOLD = 0.79f;

enum DetectionState {
	WaitingForCarrier,
	WaitingForLettersSpace,
	WaitingForLettersMarkUs,
	WaitingForLettersMarkEurope,
	RunningUs,
	RunningEurope,
};

class BaudotDetector {
public:
	BaudotDetector() {
		setFrameSize();
		mBufferizer = ms_bufferizer_new();
	}
	~BaudotDetector() {
		ms_bufferizer_destroy(mBufferizer);
	}

	void enableDecoding(bool enabled) {
		mDecodingEnabled = enabled;
	}

	void enableDetection(bool enabled) {
		mGlobalDetectionEnabled = enabled;
	}

	void notifyCharacterJustSent() {
		mTemporaryDetectionEnabled = false;
		bctbx_get_cur_time(&mTemporaryDetectionDisablingTime);
	}

	void setNbChannels(int channels) {
		mNbChannels = channels;
	}

	void setRate(int rate) {
		mRate = rate;
		setFrameSize();
		ms_message("Baudot detector: set rate %d Hz, framesize is %d", mRate, mFrameSize);
	}

	void process(MSFilter *f) {
		mblk_t *m;

		if (isTemporaryDetectionToBeEnabled()) {
			mTemporaryDetectionEnabled = true;
		}

		while ((m = ms_queue_get(f->inputs[0])) != nullptr) {
			if (mDecodingEnabled) {
				// Mute the output
				size_t s = msgdsize(m);
				mblk_t *silenceM = allocb(s, 0);
				ms_queue_put(f->outputs[0], silenceM);
				ms_bufferizer_put(mBufferizer, m);
			} else {
				ms_queue_put(f->outputs[0], m);
				ms_bufferizer_put(mBufferizer, dupmsg(m));
			}
		}

		uint8_t *multiChannelBuffer = reinterpret_cast<uint8_t *>(alloca(mFrameSize * mNbChannels));
		while (ms_bufferizer_read(mBufferizer, multiChannelBuffer, mFrameSize * mNbChannels) != 0) {
			if (mNbChannels == 1) {
				processSample(f, reinterpret_cast<int16_t *>(multiChannelBuffer));
			} else {
				int16_t *singleChannelBuffer = reinterpret_cast<int16_t *>(alloca(mFrameSize));
				int16_t *multiChannelSamplesBuffer = reinterpret_cast<int16_t *>(multiChannelBuffer);
				for (int i = 0; i < (mFrameSize / 2); i++) {
					int32_t value = 0;
					for (int j = 0; j < mNbChannels; j++) {
						value += (int32_t)multiChannelSamplesBuffer[(i * mNbChannels) + j];
					}
					singleChannelBuffer[i] = (int16_t)(value / mNbChannels);
				}
				processSample(f, multiChannelSamplesBuffer);
			}
		}

		notifyReceivedCharacters(f);
	}

private:
	// Calculate the number of bits corresponding to a tone duration, according to the given standard.
	static uint16_t calcNbBits(uint16_t nbMs, MSBaudotStandard standard) {
		switch (standard) {
			default:
			case MSBaudotStandardUs:
				return (uint16_t)(((float)nbMs / US_TONE_DURATION) + 0.5f);
			case MSBaudotStandardEurope:
				return (uint16_t)(((float)nbMs / EUROPE_TONE_DURATION) + 0.5f);
		}
	}

	static const char *modeToString(MSBaudotMode mode) {
		switch (mode) {
			case MSBaudotModeTty45:
				return "TTY45";
			case MSBaudotModeTty50:
				return "TTY50";
			case MSBaudotModeVoice:
				return "Voice";
			default:
				return "UNDEFINED";
		}
	}

	static const char *detectionStateToString(DetectionState state) {
		switch (state) {
			case WaitingForCarrier:
				return "WaitingForCarrier";
			case WaitingForLettersSpace:
				return "WaitingForLettersSpace";
			case WaitingForLettersMarkUs:
				return "WaitingForLettersMarkUs";
			case WaitingForLettersMarkEurope:
				return "WaitingForLettersMarkEurope";
			case RunningUs:
				return "RunningUs";
			case RunningEurope:
				return "RunningEurope";
			default:
				return "UNDEFINED";
		}
	}

	static bool isMarkDetected(float spaceEnergy, float markEnergy) {
		return (markEnergy > MIN_DETECTION_THRESHOLD) && (spaceEnergy < MAX_NO_DETECTION_THRESHOLD);
	}

	static bool isSpaceDetected(float spaceEnergy, float markEnergy) {
		return (spaceEnergy > MIN_DETECTION_THRESHOLD) && (markEnergy < MAX_NO_DETECTION_THRESHOLD);
	}

	void addBits(MSFilter *f, bool value, uint16_t nb) {
		if (nb == 0) return;

		for (uint16_t i = 0; i < nb; i++) {
			if (value) {
				mCurrentCode = mCurrentCode | (1 << mCurrentCodeBit);
			}
			mCurrentCodeBit++;
			if (mCurrentCodeBit == 8) {
				if (!mDecodingEnabled && (mCurrentCode == BAUDOT_LETTERS_CODE)) {
					switch (mDetectionState) {
						case RunningUs:
							notifyState(f, MSBaudotDetectorStateTty45);
							break;
						case RunningEurope:
							notifyState(f, MSBaudotDetectorStateTty50);
							break;
						default:
							break;
					}
				}
				mContext.feed(mCurrentCode);
				mCurrentCodeBit = 0;
				mCurrentCode = 0;
			}
		}
	}

	void changeDetectionState(DetectionState newState) {
		if (newState != mDetectionState) {
			ms_message("Baudot detection state: %s -> %s", detectionStateToString(mDetectionState),
			           detectionStateToString(newState));
			mDetectionState = newState;
			if (newState == RunningUs) {
				mContext.setStandard(MSBaudotStandardUs);
			} else if (newState == RunningEurope) {
				mContext.setStandard(MSBaudotStandardEurope);
			}
		}
	}

	float computeEnergy(int16_t *samples, int nbSamples) {
		float energy = 0;

		for (int i = 0; i < nbSamples; ++i) {
			float current_sample_energy = 0;

			for (int j = 0; j < mNbChannels; j++) {
				float s = (float)samples[(i * mNbChannels) + j];
				current_sample_energy += s * s;
			}

			energy += current_sample_energy / mNbChannels;
		}

		return energy;
	}

	bool detectionEnabled() const {
		return mGlobalDetectionEnabled && mTemporaryDetectionEnabled;
	}

	void handleMarkBits(MSFilter *f) {
		if (mConsecutiveMarkMs == 0) return;

		switch (mDetectionState) {
			case WaitingForCarrier:
				if ((mConsecutiveMarkMs >= (CARRIER_DURATION - 1)) && (mConsecutiveMarkMs <= (CARRIER_DURATION + 1))) {
					changeDetectionState(WaitingForLettersSpace);
				}
				break;
			case WaitingForLettersMarkUs:
				if (calcNbBits(mConsecutiveMarkMs, MSBaudotStandardUs) == 7) {
					changeDetectionState(RunningUs);
					notifyState(f, MSBaudotDetectorStateTty45);
					mContext.feed(BAUDOT_LETTERS_CODE);
				} else {
					changeDetectionState(WaitingForCarrier);
				}
				break;
			case WaitingForLettersMarkEurope:
				if (calcNbBits(mConsecutiveMarkMs, MSBaudotStandardEurope) == 7) {
					changeDetectionState(RunningEurope);
					notifyState(f, MSBaudotDetectorStateTty50);
					mContext.feed(BAUDOT_LETTERS_CODE);
				} else {
					changeDetectionState(WaitingForCarrier);
				}
				break;
			case RunningUs:
				addBits(f, true, calcNbBits(mConsecutiveMarkMs, MSBaudotStandardUs));
				break;
			case RunningEurope:
				addBits(f, true, calcNbBits(mConsecutiveMarkMs, MSBaudotStandardEurope));
				break;
			case WaitingForLettersSpace:
			default:
				// Do nothing...
				break;
		}

		mConsecutiveMarkMs = 0;
	}

	void handleSpaceBits(MSFilter *f) {
		if (mConsecutiveSpaceMs == 0) return;

		switch (mDetectionState) {
			case WaitingForLettersSpace:
				if (isDuration(mConsecutiveSpaceMs, MSBaudotStandardUs)) {
					changeDetectionState(WaitingForLettersMarkUs);
				} else if (isDuration(mConsecutiveSpaceMs, MSBaudotStandardEurope)) {
					changeDetectionState(WaitingForLettersMarkEurope);
				}
				break;
			case RunningUs:
				addBits(f, false, calcNbBits(mConsecutiveSpaceMs, MSBaudotStandardUs));
				break;
			case RunningEurope:
				addBits(f, false, calcNbBits(mConsecutiveSpaceMs, MSBaudotStandardEurope));
				break;
			case WaitingForCarrier:
			case WaitingForLettersMarkUs:
			case WaitingForLettersMarkEurope:
			default:
				// Do nothing...
				break;
		}

		mConsecutiveSpaceMs = 0;
	}

	void initGoertzelStates() {
		mGoertzelStates[0].init(SPACE_TONE_FREQ, mRate);
		mGoertzelStates[1].init(MARK_TONE_FREQ, mRate);
	}

	bool isDuration(uint16_t nbMs, MSBaudotStandard standard) {
		switch (standard) {
			default:
			case MSBaudotStandardUs:
				return (nbMs == US_TONE_DURATION) || (nbMs == (US_TONE_DURATION + 1));
			case MSBaudotStandardEurope:
				return (nbMs == EUROPE_TONE_DURATION) || (nbMs == EUROPE_TONE_DURATION + 1);
		}
	}

	bool isTemporaryDetectionToBeEnabled() const {
		if (mTemporaryDetectionEnabled) return false;

		bctoolboxTimeSpec testedTime;

		// Get the current time and remove the no-detection duration to it.
		bctbx_get_cur_time(&testedTime);
		bctbx_timespec_add_millisecs(&testedTime, 0 - NO_DETECTION_DURATION_MS);

		return (bctbx_timespec_compare(&testedTime, &mTemporaryDetectionDisablingTime) >= 0);
	}

	void notifyReceivedCharacters(MSFilter *f) {
		if (mDecodingEnabled) {
			std::optional<char> decodedCharacter = std::nullopt;
			while ((decodedCharacter = mContext.nextDecodedCharacter()).has_value()) {
				char characterToNotify = decodedCharacter.value();
				ms_filter_notify(f, MS_BAUDOT_DETECTOR_CHARACTER_EVENT, &characterToNotify);
			}
		}
	}

	void notifyState(MSFilter *f, MSBaudotDetectorState state) {
		if (mGlobalDetectionEnabled) {
			if (!mDecodingEnabled) {
				mContext.clear();
			}
			ms_filter_notify(f, MS_BAUDOT_DETECTOR_STATE_EVENT, &state);
		}
	}

	void processSample(MSFilter *f, int16_t *buffer) {
		float energy = computeEnergy(buffer, mFrameSize / 2);
		if (energy > (ENERGY_MIN_THRESHOLD * 32767.0 * 32767.0 * 0.7)) {
			float spaceStateEnergy = mGoertzelStates[0].run(buffer, mFrameSize / 2, energy);
			float markStateEnergy = mGoertzelStates[1].run(buffer, mFrameSize / 2, energy);
			if (isSpaceDetected(spaceStateEnergy, markStateEnergy)) {
				handleMarkBits(f);
				mConsecutiveSpaceMs++;
				mConsecutiveUndeterminedMs = 0;
				return;
			} else if (isMarkDetected(spaceStateEnergy, markStateEnergy)) {
				handleSpaceBits(f);
				mConsecutiveMarkMs++;
				mConsecutiveUndeterminedMs = 0;
				return;
			} else {
				if (mConsecutiveUndeterminedMs == 0) {
					mConsecutiveSpaceMs++;
					mConsecutiveMarkMs++;
					mConsecutiveUndeterminedMs++;
					return;
				} else {
					mConsecutiveUndeterminedMs++;
				}
			}
		} else {
			mConsecutiveUndeterminedMs = 0;
		}

		if (mConsecutiveMarkMs > mConsecutiveSpaceMs) {
			handleMarkBits(f);
		} else if (mConsecutiveSpaceMs > mConsecutiveMarkMs) {
			handleSpaceBits(f);
		}
		mCurrentCode = 0;
	}

	// Sets the frame size for a 1 ms sample.
	void setFrameSize() {
		static const int frameMsDuration = 1;
		mFrameSize = 2 * (frameMsDuration * mRate) / 1000;
		initGoertzelStates();
	}

	BaudotDecodingContext mContext;
	GoertzelState mGoertzelStates[2];
	DetectionState mDetectionState = WaitingForCarrier;
	MSBufferizer *mBufferizer = nullptr;
	bctoolboxTimeSpec mTemporaryDetectionDisablingTime;
	int mNbChannels = 1;
	int mRate = 8000;
	int mFrameSize;
	uint16_t mConsecutiveSpaceMs = 0;
	uint16_t mConsecutiveMarkMs = 0;
	uint16_t mConsecutiveUndeterminedMs = 0;
	uint8_t mCurrentCode = 0;
	uint8_t mCurrentCodeBit = 0;
	bool mDecodingEnabled = false;
	bool mGlobalDetectionEnabled = true;
	bool mTemporaryDetectionEnabled = true;
};
}; // namespace mediastreamer

using namespace mediastreamer;

static void baudot_detector_init(MSFilter *f) {
	f->data = new BaudotDetector();
}

static void baudot_detector_process(MSFilter *f) {
	auto gen = reinterpret_cast<BaudotDetector *>(f->data);
	gen->process(f);
}

static void baudot_detector_uninit(MSFilter *f) {
	delete reinterpret_cast<BaudotDetector *>(f->data);
}

static int baudot_detector_set_rate(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotDetector *>(f->data);
	gen->setRate(*reinterpret_cast<int *>(arg));
	return 0;
}

static int baudot_detector_set_nchannels(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotDetector *>(f->data);
	gen->setNbChannels(*reinterpret_cast<int *>(arg));
	return 0;
}

static int baudot_detector_enable_decoding(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotDetector *>(f->data);
	gen->enableDecoding(!!*reinterpret_cast<bool_t *>(arg));
	return 0;
}

static int baudot_detector_enable_detection(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotDetector *>(f->data);
	gen->enableDetection(*reinterpret_cast<bool_t *>(arg) ? true : false);
	return 0;
}

static int baudot_detector_notify_character_just_sent(MSFilter *f, BCTBX_UNUSED(void *arg)) {
	auto gen = reinterpret_cast<BaudotDetector *>(f->data);
	gen->notifyCharacterJustSent();
	return 0;
}

MSFilterMethod baudot_detector_methods[] = {
    {MS_FILTER_SET_SAMPLE_RATE, baudot_detector_set_rate},
    {MS_FILTER_SET_NCHANNELS, baudot_detector_set_nchannels},
    {MS_BAUDOT_DETECTOR_ENABLE_DECODING, baudot_detector_enable_decoding},
    {MS_BAUDOT_DETECTOR_ENABLE_DETECTION, baudot_detector_enable_detection},
    {MS_BAUDOT_DETECTOR_NOTIFY_CHARACTER_JUST_SENT, baudot_detector_notify_character_just_sent},
    {0, nullptr}};

extern "C" {

#ifdef _MSC_VER

MSFilterDesc ms_baudot_detector_desc = {MS_BAUDOT_DETECTOR_ID,
                                        "MSBaudotDetector",
                                        N_("Baudot tone detector"),
                                        MS_FILTER_OTHER,
                                        nullptr,
                                        1,
                                        1,
                                        baudot_detector_init,
                                        nullptr,
                                        baudot_detector_process,
                                        nullptr,
                                        baudot_detector_uninit,
                                        baudot_detector_methods,
                                        MS_FILTER_IS_PUMP};

#else

MSFilterDesc ms_baudot_detector_desc = {.id = MS_BAUDOT_DETECTOR_ID,
                                        .name = "MSBaudotDetector",
                                        .text = N_("Baudot tone detector"),
                                        .category = MS_FILTER_OTHER,
                                        .ninputs = 1,
                                        .noutputs = 1,
                                        .init = baudot_detector_init,
                                        .process = baudot_detector_process,
                                        .uninit = baudot_detector_uninit,
                                        .methods = baudot_detector_methods,
                                        .flags = MS_FILTER_IS_PUMP};

#endif

} // extern "C"

MS_FILTER_DESC_EXPORT(ms_baudot_detector_desc)
