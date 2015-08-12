/*
 * android_echo.h -Android echo cancellation utilities.
 *
 * Copyright (C) 2009-2012  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef ms_devices_h
#define ms_devices_h

#include <mediastreamer2/mscommon.h>

#define DEVICE_HAS_BUILTIN_AEC 			(1)
#define DEVICE_HAS_BUILTIN_AEC_CRAPPY	(1<<1) /*set when the device is claiming to have AEC but we should not trust it */
#define DEVICE_USE_ANDROID_MIC			(1<<2) /*set when the device needs to capture using MIC instead of Voice communication (I.E kindle fire) */
#define DEVICE_HAS_BUILTIN_OPENSLES_AEC		(1<<3) /*set when the device automatically enables it's AEC when using OpenSLES */
#define DEVICE_HAS_CRAPPY_ANDROID_FASTTRACK		(1<<4) /*set when the AUDIO_OUTPUT_FLAG_FAST flag of android AudioTrack doesn't work*/
#define DEVICE_HAS_CRAPPY_ANDROID_FASTRECORD	(1<<5) /*set when the AUDIO_INPUT_FLAG_FAST flag of android AudioRecord doesn't work*/
#define DEVICE_HAS_UNSTANDARD_LIBMEDIA			(1<<6) /*set when the libmedia backend shall not be used because of proprietary modifications made into it by the manufacturer*/

struct SoundDeviceAudioHacks {
	const char *equalizer;
	float mic_gain;
};

typedef struct SoundDeviceAudioHacks SoundDeviceAudioHacks;

struct SoundDeviceDescription{
	const char *manufacturer;
	const char *model;
	const char *platform;
	unsigned int flags;
	int delay;
	int recommended_rate;
	SoundDeviceAudioHacks *hacks;
};

typedef struct SoundDeviceDescription SoundDeviceDescription;


#ifdef __cplusplus
extern "C"{
#endif

/*obtain information regarding sound system of current device*/
SoundDeviceDescription * sound_device_description_get(void);

extern SoundDeviceDescription genericSoundDeviceDescriptor;

#ifdef __cplusplus
}
#endif

#endif
