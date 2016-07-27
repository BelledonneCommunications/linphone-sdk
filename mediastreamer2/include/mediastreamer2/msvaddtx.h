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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef msvaddtx_h
#define msvaddtx_h

#include "mediastreamer2/msfilter.h"

typedef struct _MSCngData{
	int datasize;
	uint8_t data[32];
}MSCngData;

/** Event generated when silence is detected. Payload contains the data encoding the background noise*/
#define MS_VAD_DTX_NO_VOICE	MS_FILTER_EVENT(MS_VAD_DTX_ID, 0, MSCngData)

#define MS_VAD_DTX_VOICE	MS_FILTER_EVENT_NO_ARG(MS_VAD_DTX_ID, 1)


#endif
