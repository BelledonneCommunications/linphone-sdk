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

#include "baudot_encoding_context.h"
#include "baudot_filters.h"
#include "mediastreamer2/baudot.h"
#include "mediastreamer2/msticker.h"

#include <cmath>

namespace mediastreamer {

const uint16_t SPACE_TONE_FREQ = 1800; // Hz
const uint16_t MARK_TONE_FREQ = 1400;  // Hz

const uint8_t US_TONE_DURATION = 22;     // ms
const uint8_t EUROPE_TONE_DURATION = 20; // ms

const uint8_t CARRIER_DURATION = 150; // ms

static const double PI = std::acos(-1);

class BaudotGenerator {
public:
	BaudotGenerator() {
		setAmplitude();
	}
	~BaudotGenerator() = default;

	int getNbChannels() const {
		return mNbChannels;
	}

	int getRate() const {
		return mRate;
	}

	void sendChar(const char c) {
		mContext.encodeChar(c);
	}

	void sendString(const std::string &text) {
		mContext.encodeString(text);
	}

	void setDefaultAmplitude(int amplitude) {
		mDefaultAmplitude = (float)amplitude;
		setAmplitude();
	}

	void setNbChannels(int channels) {
		mNbChannels = channels;
	}

	void setPauseTimeout(uint8_t timeout) {
		mContext.setPauseTimeout(timeout);
	}

	void setRate(int rate) {
		mRate = rate;
	}

	void setMode(MSBaudotMode mode) {
		switch (mode) {
			case MSBaudotModeVoice:
				mIsActive = false;
				break;
			case MSBaudotModeTty45:
				mIsActive = true;
				mContext.setStandard(MSBaudotStandardUs);
				break;
			case MSBaudotModeTty50:
				mIsActive = true;
				mContext.setStandard(MSBaudotStandardEurope);
				break;
		}
	}

	void process(MSFilter *f) {
		mblk_t *m;

		ms_filter_lock(f);

		if (mIsActive) {
			// If the Baudot generator is active, we discard the incoming stream samples and generate our own.
			ms_queue_flush(f->inputs[0]);

			int nbSamples = (f->ticker->interval * mRate) / 1000;
			m = allocb(nbSamples * mNbChannels * 2, 0);

			if (toneToPlay()) {
				if (writeTone((int16_t *)m->b_wptr, nbSamples)) {
					// Notify that a tone has been written
					ms_filter_notify_no_arg(f, MS_BAUDOT_GENERATOR_CHARACTER_SENT_EVENT);
				}
			} else {
				memset(m->b_wptr, 0, nbSamples * mNbChannels * 2);
			}

			m->b_wptr += nbSamples * mNbChannels * 2;
			ms_queue_put(f->outputs[0], m);
		} else {
			// If the Baudot generator is not active, simply forward the incoming stream samples.
			while ((m = ms_queue_get(f->inputs[0])) != nullptr) {
				ms_queue_put(f->outputs[0], m);
			}
		}

		ms_filter_unlock(f);
	}

private:
	void loadFreqAndDuration() {
		if (!mCurrentCode.has_value() || (mCurrentCodeBit >= 8)) {
			mCurrentCode = mContext.nextCode();
			mCurrentCodeBit = 0;
		}

		if (mCurrentCode.has_value()) {
			if (mCurrentCode.value() == BAUDOT_CARRIER_CODE) {
				// Handle carrier sending
				mCurrentFreq = (float)MARK_TONE_FREQ / mRate;
				mToneDuration = (mRate * CARRIER_DURATION) / 1000;
				mCurrentCode = mContext.nextCode();
				mCurrentCodeBit = 0;
			} else {
				uint8_t bit = (mCurrentCode.value() >> mCurrentCodeBit) & 0x01;
				uint8_t nbBits = 1;
				mCurrentCodeBit++;
				while (mCurrentCode.has_value() && (((mCurrentCode.value() >> mCurrentCodeBit) & 0x01) == bit)) {
					mCurrentCodeBit++;
					nbBits++;
					if (mCurrentCodeBit >= 8) {
						mCurrentCode = mContext.nextCode();
						mCurrentCodeBit = 0;
					}
				}
				mCurrentFreq = (float)(bit ? MARK_TONE_FREQ : SPACE_TONE_FREQ) / mRate;
				setToneDuration(nbBits);
			}
		} else {
			mCurrentFreq = std::nullopt;
		}
	}

	void setAmplitude() {
		mAmplitude = mDefaultAmplitude * 0.7f * 32767.0f;
	}

	void setToneDuration(uint8_t nbBits) {
		switch (mContext.getStandard()) {
			default:
			case MSBaudotStandardUs:
				mToneDuration = (nbBits * mRate * US_TONE_DURATION) / 1000;
				break;
			case MSBaudotStandardEurope:
				mToneDuration = (nbBits * mRate * EUROPE_TONE_DURATION) / 1000;
				break;
		}
	}

	bool toneToPlay() const {
		return mCurrentFreq.has_value() || mCurrentCode.has_value() || mContext.hasEncodedCharacters();
	}

	bool writeTone(int16_t *sample, int nbSamples) {
		bool hasWroteTone = false;

		for (int i = 0; i < nbSamples; i++) {
			if (mCurrentDuration >= mToneDuration) {
				mCurrentDuration = 0;
				loadFreqAndDuration();
			}

			if (mCurrentFreq.has_value()) {
				int16_t toneSample =
				    (int16_t)(mAmplitude * std::sin(2.0 * PI * mCurrentDuration * mCurrentFreq.value()));
				for (int j = 0; j < mNbChannels; j++) {
					sample[(i * mNbChannels) + j] = toneSample;
				}
				mCurrentDuration++;
				hasWroteTone = true;
			} else {
				mToneDuration =
				    0; // IMPORTANT to reset the tone duration here, so that the next character can be loaded.
				for (int j = 0; j < mNbChannels; j++) {
					sample[(i * mNbChannels) + j] = 0;
				}
			}
		}

		return hasWroteTone;
	}

	BaudotEncodingContext mContext;
	std::optional<float> mCurrentFreq = std::nullopt;
	std::optional<uint8_t> mCurrentCode = std::nullopt;
	float mDefaultAmplitude = 0.3f;
	float mAmplitude;
	int mNbChannels = 1;
	int mRate = 8000;
	int mCurrentDuration = 0;
	int mToneDuration = 0;
	uint8_t mCurrentCodeBit = 0;
	bool mIsActive = false;
};

}; // namespace mediastreamer

using namespace mediastreamer;

static void baudot_generator_init(MSFilter *f) {
	f->data = new BaudotGenerator();
}

static void baudot_generator_process(MSFilter *f) {
	auto gen = reinterpret_cast<BaudotGenerator *>(f->data);
	gen->process(f);
}

static void baudot_generator_uninit(MSFilter *f) {
	delete reinterpret_cast<BaudotGenerator *>(f->data);
}

static int baudot_generator_set_rate(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotGenerator *>(f->data);
	gen->setRate(*reinterpret_cast<int *>(arg));
	return 0;
}

static int baudot_generator_get_rate(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotGenerator *>(f->data);
	*reinterpret_cast<int *>(arg) = gen->getRate();
	return 0;
}

static int baudot_generator_set_nchannels(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotGenerator *>(f->data);
	gen->setNbChannels(*reinterpret_cast<int *>(arg));
	return 0;
}

static int baudot_generator_get_nchannels(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotGenerator *>(f->data);
	*reinterpret_cast<int *>(arg) = gen->getNbChannels();
	return 0;
}

static int baudot_generator_send_char(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotGenerator *>(f->data);
	ms_filter_lock(f);
	gen->sendChar(*reinterpret_cast<const char *>(arg));
	ms_filter_unlock(f);
	return 0;
}

static int baudot_generator_send_string(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotGenerator *>(f->data);
	ms_filter_lock(f);
	gen->sendString(std::string(reinterpret_cast<const char *>(arg)));
	ms_filter_unlock(f);
	return 0;
}

static int baudot_generator_set_mode(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotGenerator *>(f->data);
	gen->setMode(*reinterpret_cast<MSBaudotMode *>(arg));
	return 0;
}

static int baudot_generator_set_pause_timeout(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotGenerator *>(f->data);
	gen->setPauseTimeout(*reinterpret_cast<uint8_t *>(arg));
	return 0;
}

static int baudot_generator_set_default_amplitude(MSFilter *f, void *arg) {
	auto gen = reinterpret_cast<BaudotGenerator *>(f->data);
	gen->setDefaultAmplitude(*reinterpret_cast<int *>(arg));
	return 0;
}

MSFilterMethod baudot_generator_methods[] = {
    {MS_FILTER_SET_SAMPLE_RATE, baudot_generator_set_rate},
    {MS_FILTER_GET_SAMPLE_RATE, baudot_generator_get_rate},
    {MS_FILTER_SET_NCHANNELS, baudot_generator_set_nchannels},
    {MS_FILTER_GET_NCHANNELS, baudot_generator_get_nchannels},
    {MS_BAUDOT_GENERATOR_SEND_CHARACTER, baudot_generator_send_char},
    {MS_BAUDOT_GENERATOR_SEND_STRING, baudot_generator_send_string},
    {MS_BAUDOT_GENERATOR_SET_MODE, baudot_generator_set_mode},
    {MS_BAUDOT_GENERATOR_SET_PAUSE_TIMEOUT, baudot_generator_set_pause_timeout},
    {MS_BAUDOT_GENERATOR_SET_DEFAULT_AMPLITUDE, baudot_generator_set_default_amplitude},
    {0, nullptr}};

extern "C" {

#ifdef _MSC_VER

MSFilterDesc ms_baudot_generator_desc = {MS_BAUDOT_GENERATOR_ID,
                                         "MSBaudotGenerator",
                                         N_("Baudot tone generator"),
                                         MS_FILTER_OTHER,
                                         nullptr,
                                         1,
                                         1,
                                         baudot_generator_init,
                                         nullptr,
                                         baudot_generator_process,
                                         nullptr,
                                         baudot_generator_uninit,
                                         baudot_generator_methods,
                                         MS_FILTER_IS_PUMP};

#else

MSFilterDesc ms_baudot_generator_desc = {.id = MS_BAUDOT_GENERATOR_ID,
                                         .name = "MSBaudotGenerator",
                                         .text = N_("Baudot tone generator"),
                                         .category = MS_FILTER_OTHER,
                                         .ninputs = 1,
                                         .noutputs = 1,
                                         .init = baudot_generator_init,
                                         .process = baudot_generator_process,
                                         .uninit = baudot_generator_uninit,
                                         .methods = baudot_generator_methods,
                                         .flags = MS_FILTER_IS_PUMP};

#endif

} // extern "C"

MS_FILTER_DESC_EXPORT(ms_baudot_generator_desc)
