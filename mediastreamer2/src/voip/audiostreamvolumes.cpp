/*
 * Copyright (c) 2010-2021 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
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

#include <map>

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msvolume.h>

using VolumeMap = std::map<uint32_t, int>;

extern "C" AudioStreamVolumes *audio_stream_volumes_new() {
	return (AudioStreamVolumes *)(new VolumeMap());
}

extern "C" void audio_stream_volumes_delete(AudioStreamVolumes *volumes) {
	delete ((VolumeMap*) volumes);
}

extern "C" void audio_stream_volumes_insert(AudioStreamVolumes *volumes, uint32_t ssrc, int volume) {
	(*((VolumeMap*) volumes))[ssrc] = volume;
}

extern "C" void audio_stream_volumes_erase(AudioStreamVolumes *volumes, uint32_t ssrc) {
	((VolumeMap*) volumes)->erase(ssrc);
}

extern "C" void audio_stream_volumes_clear(AudioStreamVolumes *volumes) {
	((VolumeMap*) volumes)->clear();
}

extern "C" int audio_stream_volumes_size(AudioStreamVolumes *volumes) {
	return (int) ((VolumeMap*) volumes)->size();
}

extern "C" int audio_stream_volumes_find(AudioStreamVolumes *volumes, uint32_t ssrc) {
	auto map = (VolumeMap *) volumes;
	auto search = map->find(ssrc);
	if (search != map->end()) {
		return search->second;
	}

	return AUDIOSTREAMVOLUMES_NOT_FOUND;
}

extern "C" void audio_stream_volumes_reset_values(AudioStreamVolumes *volumes) {
	for (auto &values : *((VolumeMap *) volumes)) {
		values.second = MS_VOLUME_DB_LOWEST;
	}
}
