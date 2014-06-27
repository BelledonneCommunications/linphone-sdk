/*
 * flowcontrol.h - routines to silently discard samples in excess (used by AEC implementation)
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

#ifndef flowcontrol_h
#define flowcontrol_h



typedef struct _MSAudioFlowController {
	int target_samples;
	int total_samples;
	int current_pos;
	int current_dropped;
} MSAudioFlowController;


#ifdef __cplusplus
extern "C"{
#endif

MS2_PUBLIC void ms_audio_flow_controller_init(MSAudioFlowController *ctl);

MS2_PUBLIC void ms_audio_flow_controller_set_target(MSAudioFlowController *ctl, int samples_to_drop, int total_samples);

MS2_PUBLIC mblk_t *ms_audio_flow_controller_process(MSAudioFlowController *ctl, mblk_t *m);


#ifdef __cplusplus
}
#endif

#endif
