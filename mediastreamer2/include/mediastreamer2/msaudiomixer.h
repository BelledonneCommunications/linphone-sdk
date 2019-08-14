/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
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
#ifndef msaudiomixer_h
#define msaudiomixer_h

#include <mediastreamer2/msfilter.h>

typedef struct MSAudioMixerCtl{
	int pin;
	union param_t { 
		float gain; /**<gain correction */
		int active; /**< to mute or unmute the input channel */
		int enabled; /**< to mute/unmute the output channel*/
	} param;
} MSAudioMixerCtl;

#define MS_AUDIO_MIXER_SET_INPUT_GAIN			MS_FILTER_METHOD(MS_AUDIO_MIXER_ID,0,MSAudioMixerCtl)
/** Tells whether channel contributes to the mix.*/
#define MS_AUDIO_MIXER_SET_ACTIVE			MS_FILTER_METHOD(MS_AUDIO_MIXER_ID,1,MSAudioMixerCtl)
#define MS_AUDIO_MIXER_ENABLE_CONFERENCE_MODE		MS_FILTER_METHOD(MS_AUDIO_MIXER_ID,2,int)

/**The master channel is the one that is used to synchronize the others. No flow control is done on the master channel*/
#define MS_AUDIO_MIXER_SET_MASTER_CHANNEL		MS_FILTER_METHOD(MS_AUDIO_MIXER_ID,3,int)

#define MS_AUDIO_MIXER_ENABLE_OUTPUT			MS_FILTER_METHOD(MS_AUDIO_MIXER_ID,4,MSAudioMixerCtl)
#endif
