/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#ifndef dtmfgen_h
#define dtmfgen_h

#include <mediastreamer2/msfilter.h>

#define MS_DTMF_GEN_PUT		MS_FILTER_METHOD(MS_DTMF_GEN_ID,0,const char)
/** Plays dtmf tone given in argument with default duration*/

#define MS_DTMF_GEN_PLAY		MS_FILTER_METHOD(MS_DTMF_GEN_ID,0,const char) /*alias to put*/

/**Start playing a given dtmf, then it has to be stopped using MS_DTMF_GEN_STOP */
#define MS_DTMF_GEN_START		MS_FILTER_METHOD(MS_DTMF_GEN_ID,1,const char)

/**Stop currently played dtmf*/
#define MS_DTMF_GEN_STOP		MS_FILTER_METHOD_NO_ARG(MS_DTMF_GEN_ID,2)

/**
 * Structure describing a custom tone.
**/
struct _MSDtmfGenCustomTone{
	char tone_name[8];	/**<Tone name for convenience*/
	int duration;		/**<Duration of the tone in milliseconds*/
	int frequencies[2];	/**<Frequencies of the tone to be played */
	float amplitude;	/**<Amplitude of the tone, 1.0 being the 0dbm normalized level*/
	int interval;		/**<Interval 'between tones' in milliseconds*/
	int repeat_count;	/**<Number of times the tone is repeated.*/
};

typedef struct _MSDtmfGenCustomTone MSDtmfGenCustomTone;

/**Play a custom tone according to the supplied tone description*/
#define MS_DTMF_GEN_PLAY_CUSTOM	MS_FILTER_METHOD(MS_DTMF_GEN_ID,3,MSDtmfGenCustomTone)

/**Sets default amplitude for dtmfs, expressed in the 0..1 range*/
#define MS_DTMF_GEN_SET_DEFAULT_AMPLITUDE MS_FILTER_METHOD(MS_DTMF_GEN_ID,4,float)


/**
 * Structure carried by MS_DTMF_GEN_EVENT
**/
struct _MSDtmfGenEvent{
	uint64_t tone_start_time;
	char tone_name[8];
};

typedef struct _MSDtmfGenEvent MSDtmfGenEvent;

/**
 * Event sent by the filter each time a tone or dtmf is generated.
**/
#define MS_DTMF_GEN_EVENT		MS_FILTER_EVENT(MS_DTMF_GEN_ID,0,MSDtmfGenEvent)

extern MSFilterDesc ms_dtmf_gen_desc;

#endif
