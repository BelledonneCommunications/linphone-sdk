/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011 Belledonne Communications SARL
Author: Simon MORLAT (simon.morlat@linphone.org)

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

/*
 * Convenient API to create and manage audio conferences.
 */

#ifndef conference_h
#define conference_h

#include "mediastreamer2/mediastream.h"

typedef struct _MSAudioConferenceParams{
	int samplerate;
}MSAudioConferenceParams;

struct _MSAudioConference{
	MSTicker *ticker;
	MSFilter *mixer;
	MSAudioConferenceParams params;
	int nmembers;
};

typedef struct _MSAudioConference MSAudioConference;

struct _MSAudioEndpoint{
	AudioStream *st;
	MSFilter *in_resampler,*out_resampler;
	MSCPoint out_cut_point;
	MSCPoint in_cut_point;
	MSCPoint in_cut_point_prev;
	MSCPoint mixer_in;
	MSCPoint mixer_out;
	MSAudioConference *conference;
	int pin;
	int samplerate;
};

typedef struct _MSAudioEndpoint MSAudioEndpoint;



#ifdef __cplusplus
extern "C" {
#endif

MSAudioConference * ms_audio_conference_new(const MSAudioConferenceParams *params);
const MSAudioConferenceParams *ms_audio_conference_get_params(MSAudioConference *obj);
void ms_audio_conference_add_member(MSAudioConference *obj, MSAudioEndpoint *ep);
void ms_audio_conference_remove_member(MSAudioConference *obj, MSAudioEndpoint *ep);
void ms_audio_conference_mute_member(MSAudioConference *obj, MSAudioEndpoint *ep, bool_t muted);
int ms_audio_conference_size(MSAudioConference *obj);
void ms_audio_conference_destroy(MSAudioConference *obj);
	
MSAudioEndpoint * ms_audio_endpoint_get_from_stream(AudioStream *st, bool_t is_remote);
void ms_audio_endpoint_release_from_stream(MSAudioEndpoint *obj);



#ifdef __cplusplus
}
#endif

#endif
