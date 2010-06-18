/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#ifndef msaudiomixer_h
#define msaudiomixer_h

#include "mediastreamer2/msfilter.h"

typedef struct MSAudioMixerCtl{
	int pin;
	float gain;
} MSAudioMixerCtl;

#define MS_AUDIO_MIXER_SET_INPUT_GAIN	MS_FILTER_METHOD(MS_AUDIO_MIXER_ID,0,MSAudioMixerCtl)

#endif
