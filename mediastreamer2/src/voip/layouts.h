/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL, Grenoble France.

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

#ifndef mslayouts_hh
#define mslayouts_hh

#include "mediastreamer2/msvideo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MS_LAYOUT_MIN_SIZE 40

void ms_layout_center_rectangle(MSVideoSize wsize, MSVideoSize vsize, MSRect *rect);
	
void ms_layout_compute(MSVideoSize wsize, MSVideoSize vsize, MSVideoSize orig_psize,
                       int localrect_pos, float scalefactor, MSRect *mainrect, MSRect *localrect);

#ifdef __cplusplus
}
#endif

#endif
