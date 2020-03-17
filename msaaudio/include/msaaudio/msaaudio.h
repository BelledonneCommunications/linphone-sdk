/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
 *
 * aaudio.h - Header file of Android Media plugin for Linphone, based on AAudio APIs.
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

#ifndef ms_aaudio_h
#define ms_aaudio_h

#include <mediastreamer2/mssndcard.h>

static int DeviceFavoriteSampleRate = 44100;

struct AAudioContext {
	AAudioContext() {
		samplerate = DeviceFavoriteSampleRate;
		nchannels = 1;
		builtin_aec = false;
	}

	int samplerate;
	int nchannels;
	bool builtin_aec;
};

MSFilter *android_snd_card_create_reader(MSSndCard *card);
MSFilter *android_snd_card_create_writer(MSSndCard *card);

void register_aaudio_recorder(MSFactory* factory);
void register_aaudio_player(MSFactory* factory);

#ifdef __ANDROID__
#include <jni.h>
#ifdef __cplusplus
extern "C"{
#endif

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerAudioBroadcastReceiver_updateDeviceChangedFlag (JNIEnv * env, jobject obj, jlong ptr, jboolean deviceChanged);

#ifdef __cplusplus
}
#endif


#endif

#endif // ms_aaudio_h
