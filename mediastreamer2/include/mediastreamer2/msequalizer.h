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

#ifndef msequalizer_h
#define msequalizer_h

#include <mediastreamer2/msfilter.h>

typedef struct _MSEqualizerGain{
	float frequency; ///< In hz
	float gain; ///< between 0-1.2
	float width; ///< frequency band width around mid frequency for which the gain is applied, in Hz. Use 0 for the lowest frequency resolution.
}MSEqualizerGain;

#ifdef __cplusplus
extern "C" {
#endif

MS2_PUBLIC MSList *ms_parse_equalizer_string(const char *str);

#ifdef __cplusplus
}
#endif

#define MS_EQUALIZER_SET_GAIN		MS_FILTER_METHOD(MS_EQUALIZER_ID,0,MSEqualizerGain)
#define MS_EQUALIZER_GET_GAIN		MS_FILTER_METHOD(MS_EQUALIZER_ID,1,MSEqualizerGain)
#define MS_EQUALIZER_SET_ACTIVE		MS_FILTER_METHOD(MS_EQUALIZER_ID,2,int)
/**dump the spectral response into a table of float. The table must be sized according to the value returned by
 * MS_EQUALIZER_GET_NUM_FREQUENCIES 
**/
#define MS_EQUALIZER_DUMP_STATE		MS_FILTER_METHOD(MS_EQUALIZER_ID,3,float)

/**returns the number of frequencies*/
#define MS_EQUALIZER_GET_NUM_FREQUENCIES	MS_FILTER_METHOD(MS_EQUALIZER_ID,4,int)


#endif

