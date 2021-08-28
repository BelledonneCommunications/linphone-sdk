/*
 * Copyright (c) 2010-2021 Belledonne Communications SARL.
 *
 * msoboe.h - Header file of Android Media plugin for Linphone, based on Oboe APIs.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ms_oboe_h
#define ms_oboe_h

#include <oboe/Oboe.h>
#include <mediastreamer2/android_utils.h>
#include <mediastreamer2/mssndcard.h>

extern int DeviceFavoriteSampleRate;
extern int DeviceFavoriteFramesPerBurst;

struct OboeContext {
	OboeContext() {
		sampleRate = DeviceFavoriteSampleRate;
		nchannels = 1;
		builtin_aec = false;
		device_changed = false;
	}

	int sampleRate;
	int nchannels;
	bool builtin_aec;
	bool device_changed;
};

const char *oboe_state_to_string(oboe::StreamState state);
const char *oboe_api_to_string(oboe::AudioApi api);
const char *oboe_direction_to_string(oboe::Direction direction);
const char *oboe_sharing_mode_to_string(oboe::SharingMode mode);
const char *oboe_performance_mode_to_string(oboe::PerformanceMode mode);
const char *oboe_format_to_string(oboe::AudioFormat format);

MSFilter *android_snd_card_create_reader(MSSndCard *card);
MSFilter *android_snd_card_create_writer(MSSndCard *card);

void register_oboe_recorder(MSFactory* factory);
void register_oboe_player(MSFactory* factory);

#endif // ms_oboe_h
