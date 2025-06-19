/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

#include <map>

#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msvolume.h>

using VolumeMap = std::map<uint32_t, int>;

extern "C" AudioStreamVolumes *audio_stream_volumes_new() {
	return (AudioStreamVolumes *)(new VolumeMap());
}

extern "C" void audio_stream_volumes_delete(AudioStreamVolumes *volumes) {
	delete ((VolumeMap *)volumes);
}

extern "C" void audio_stream_volumes_insert(AudioStreamVolumes *volumes, uint32_t ssrc, int volume) {
	(*((VolumeMap *)volumes))[ssrc] = volume;
}

extern "C" void audio_stream_volumes_erase(AudioStreamVolumes *volumes, uint32_t ssrc) {
	((VolumeMap *)volumes)->erase(ssrc);
}

extern "C" void audio_stream_volumes_clear(AudioStreamVolumes *volumes) {
	((VolumeMap *)volumes)->clear();
}

extern "C" int audio_stream_volumes_size(AudioStreamVolumes *volumes) {
	return (int)((VolumeMap *)volumes)->size();
}

extern "C" int audio_stream_volumes_find(AudioStreamVolumes *volumes, uint32_t ssrc) {
	auto map = (VolumeMap *)volumes;
	auto search = map->find(ssrc);
	if (search != map->end()) {
		return search->second;
	}

	return AUDIOSTREAMVOLUMES_NOT_FOUND;
}

extern "C" int audio_stream_volumes_append(AudioStreamVolumes *volumes, AudioStreamVolumes *append) {
	auto volumesMap = (VolumeMap *)volumes;
	auto appendMap = (VolumeMap *)append;

	for (auto &values : *appendMap) {
		(*volumesMap)[values.first] = values.second;
	}

	return (int)appendMap->size();
}

extern "C" void audio_stream_volumes_reset_values(AudioStreamVolumes *volumes) {
	for (auto &values : *((VolumeMap *)volumes)) {
		if (values.second != MS_VOLUME_DB_MUTED) values.second = MS_VOLUME_DB_LOWEST;
	}
}

extern "C" uint32_t audio_stream_volumes_get_best(AudioStreamVolumes *volumes) {
	int max_db_over_member = MS_VOLUME_DB_LOWEST;
	auto map = (VolumeMap *)volumes;
	uint32_t best = 0;

	for (auto &values : *(map)) {
		if (values.second > MS_VOLUME_DB_MIN_THRESHOLD && values.second > max_db_over_member) {
			max_db_over_member = values.second;
			best = values.first;
		}
	}

	return best;
}

extern "C" bool_t audio_stream_volumes_is_speaking(AudioStreamVolumes *volumes) {
	auto map = (VolumeMap *)volumes;
	for (auto &values : *map) {
		if (values.second > MS_VOLUME_DB_MIN_THRESHOLD) {
			return true;
		}
	}
	return false;
}

extern "C" void audio_stream_volumes_populate_audio_levels(AudioStreamVolumes *volumes,
                                                           rtp_audio_level_t *audio_levels) {
	int i = 0;
	for (auto &values : *((VolumeMap *)volumes)) {
		audio_levels[i].csrc = values.first;
		audio_levels[i++].dbov = ms_volume_dbm0_to_dbov((float)values.second);
	}
}
