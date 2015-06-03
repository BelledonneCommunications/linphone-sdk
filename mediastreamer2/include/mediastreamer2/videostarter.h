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


#ifndef VIDEOSTARTER_H
#define VIDEOSTARTER_H

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msqueue.h>

/**
 * The goal of this small object is to tell when to send I frames at startup:
 * at 2 and 4 seconds.
 */

#ifdef __cplusplus
extern "C"{
#endif

	typedef struct MSVideoStarter {
		uint64_t next_time;
		int i_frame_count;
		bool_t active;
	} MSVideoStarter;

	MS2_PUBLIC void ms_video_starter_init(MSVideoStarter *vs);
	MS2_PUBLIC void ms_video_starter_first_frame(MSVideoStarter *vs, uint64_t curtime);
	MS2_PUBLIC bool_t ms_video_starter_need_i_frame(MSVideoStarter *vs, uint64_t curtime);
	MS2_PUBLIC void ms_video_starter_deactivate(MSVideoStarter *vs);

#ifdef __cplusplus
}
#endif

#endif /* VIDEOSTARTER_H */
