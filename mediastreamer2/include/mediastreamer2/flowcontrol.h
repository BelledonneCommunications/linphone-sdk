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

#ifndef flowcontrol_h
#define flowcontrol_h

typedef enum _MSAudioFlowControlStrategy{
	MSAudioFlowControlBasic, /**< Immediately drop requested number of samples */
	MSAudioFlowControlSoft /**< Elimate silent frames first, use zero-crossing sample deletion */
}MSAudioFlowControlStrategy;

typedef struct _MSAudioFlowControlConfig{
	MSAudioFlowControlStrategy strategy;
	float silent_threshold; /**< threshold under which a frame is considered as silent (linear), used by "soft" strategy */ 
}MSAudioFlowControlConfig;

typedef struct _MSAudioFlowController {
	MSAudioFlowControlConfig config;
	uint32_t target_samples;
	uint32_t total_samples;
	uint32_t current_pos;
	uint32_t current_dropped;
} MSAudioFlowController;


#ifdef __cplusplus
extern "C"{
#endif

MS2_PUBLIC void ms_audio_flow_controller_init(MSAudioFlowController *ctl);

MS2_PUBLIC void ms_audio_flow_controller_set_config(MSAudioFlowController *ctl, const MSAudioFlowControlConfig *config);

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
 * Event than can be emitted by any filter each time some samples need to be dropped.
 * @FIXME It is badly named. It should belong to MSFilter base class or to a specific audio interface.
**/
#define MS_AUDIO_FLOW_CONTROL_DROP_EVENT MS_FILTER_EVENT(MS_AUDIO_FLOW_CONTROL_ID, 0, MSAudioFlowControlDropEvent)

/**
 * Set configuration of the flow controller. It can be changed at run-time.
 */
#define MS_AUDIO_FLOW_CONTROL_SET_CONFIG MS_FILTER_METHOD(MS_AUDIO_FLOW_CONTROL_ID, 0, MSAudioFlowControlConfig)

/**
 * Request to drop samples.
 */
#define MS_AUDIO_FLOW_CONTROL_DROP MS_FILTER_METHOD(MS_AUDIO_FLOW_CONTROL_ID, 1, MSAudioFlowControlDropEvent)



#ifdef __cplusplus
}
#endif

#endif
