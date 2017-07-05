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
	uint32_t target_samples;
	uint32_t total_samples;
	uint32_t current_pos;
	uint32_t current_dropped;
} MSAudioFlowController;


#ifdef __cplusplus
extern "C"{
#endif

MS2_PUBLIC void ms_audio_flow_controller_init(MSAudioFlowController *ctl);

MS2_PUBLIC void ms_audio_flow_controller_set_target(MSAudioFlowController *ctl, uint32_t samples_to_drop, uint32_t total_samples);

MS2_PUBLIC mblk_t *ms_audio_flow_controller_process(MSAudioFlowController *ctl, mblk_t *m);


/**
 * Structure carried by MS_AUDIO_FLOW_CONTROL_DROP_EVENT
**/
typedef struct _MSAudioFlowControlDropEvent{
	uint32_t flow_control_interval_ms;
	uint32_t drop_ms;
} MSAudioFlowControlDropEvent;

/**
 * Event sent by the filter each time some samples need to be dropped.
**/
#define MS_AUDIO_FLOW_CONTROL_DROP_EVENT MS_FILTER_EVENT(MS_AUDIO_FLOW_CONTROL_ID, 0, MSAudioFlowControlDropEvent)


#ifdef __cplusplus
}
#endif

#endif
