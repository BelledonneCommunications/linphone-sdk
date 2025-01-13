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

#ifndef _MS_BAUDOT_GEN_H_
#define _MS_BAUDOT_GEN_H_

#include <mediastreamer2/msfilter.h>

enum _MSBaudotStandard {
	MSBaudotStandardUs,
	MSBaudotStandardEurope,
};

typedef enum _MSBaudotStandard MSBaudotStandard;

/** Send the given text as Baudot tones. */
#define MS_BAUDOT_GENERATOR_SEND_STRING MS_FILTER_METHOD(MS_BAUDOT_GENERATOR_ID, 0, const char *)

/** Send the given character as Baudot tones. */
#define MS_BAUDOT_GENERATOR_SEND_CHARACTER MS_FILTER_METHOD(MS_BAUDOT_GENERATOR_ID, 1, const char *)

/** Set the mode of the Baudot tone generator filter. */
#define MS_BAUDOT_GENERATOR_SET_MODE MS_FILTER_METHOD(MS_BAUDOT_GENERATOR_ID, 2, MSBaudotMode)

/** Define the Baudot significant pause timeout after which a LETTERS tone is retransmitted before resuming transmission
 * (in seconds). Default is 5s. */
#define MS_BAUDOT_GENERATOR_SET_PAUSE_TIMEOUT MS_FILTER_METHOD(MS_BAUDOT_GENERATOR_ID, 3, uint8_t)

/** Sets default amplitude for Baudot tones, expressed in the 0..1 range. */
#define MS_BAUDOT_GENERATOR_SET_DEFAULT_AMPLITUDE MS_FILTER_METHOD(MS_BAUDOT_GENERATOR_ID, 4, float)

/** Event generated when a Baudot character is sent. */
#define MS_BAUDOT_GENERATOR_CHARACTER_SENT_EVENT MS_FILTER_EVENT_NO_ARG(MS_BAUDOT_GENERATOR_ID, 0)

enum _MSBaudotDetectorState {
	MSBaudotDetectorStateTty45,
	MSBaudotDetectorStateTty50,
};

typedef enum _MSBaudotDetectorState MSBaudotDetectorState;

enum _MSBaudotMode {
	MSBaudotModeTty45,
	MSBaudotModeTty50,
	MSBaudotModeVoice,
};

typedef enum _MSBaudotMode MSBaudotMode;

/** Enable/disable Baudot tones decoding. */
#define MS_BAUDOT_DETECTOR_ENABLE_DECODING MS_FILTER_METHOD(MS_BAUDOT_DETECTOR_ID, 0, bool_t)

/** Enable/disable Baudot tones detection. */
#define MS_BAUDOT_DETECTOR_ENABLE_DETECTION MS_FILTER_METHOD(MS_BAUDOT_DETECTOR_ID, 1, bool_t)

/** Notify that a Baudot character has just been sent to be able to deactivate detection for 300 ms. */
#define MS_BAUDOT_DETECTOR_NOTIFY_CHARACTER_JUST_SENT MS_FILTER_METHOD_NO_ARG(MS_BAUDOT_DETECTOR_ID, 2)

/** Event generated when a Baudot tones are detected (needs the detection to be enabled). */
#define MS_BAUDOT_DETECTOR_STATE_EVENT MS_FILTER_EVENT(MS_BAUDOT_DETECTOR_ID, 0, MSBaudotDetectorState)

/** Event generated when a Baudot character is detected. */
#define MS_BAUDOT_DETECTOR_CHARACTER_EVENT MS_FILTER_EVENT(MS_BAUDOT_DETECTOR_ID, 1, char)

#endif /* _MS_BAUDOT_GEN_H_ */
