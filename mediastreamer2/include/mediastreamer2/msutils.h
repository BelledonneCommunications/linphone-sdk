/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2014  Belledonne Communications SARL

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

#ifndef msutils_h
#define msutils_h

#include "mediastreamer2/mscommon.h"


#ifdef __cplusplus
extern "C"{
#endif

typedef void (*MSAudioDiffProgressNotify)(void* user_data, int percentage);
/*utility function to check similarity between two audio wav files*/
MS2_PUBLIC int ms_audio_diff(const char *file1, const char *file2, double *ret, MSAudioDiffProgressNotify func, void *user_data);


/*Utility object to determine a maximum or minimum (but not both at the same time), of a signal during a sliding period of time.
**/
typedef struct _MSExtremum{
	float current_extremum;
	uint64_t extremum_time;
	float last_stable;
	int period;
}MSExtremum;

MS2_PUBLIC void ms_extremum_reset(MSExtremum *obj);

MS2_PUBLIC void ms_extremum_init(MSExtremum *obj, int period);

MS2_PUBLIC void ms_extremum_record_min(MSExtremum *obj, uint64_t curtime, float value);

MS2_PUBLIC void ms_extremum_record_max(MSExtremum *obj, uint64_t curtime, float value);

MS2_PUBLIC float ms_extremum_get_current(MSExtremum *obj);

#ifdef __cplusplus
}
#endif

#endif

