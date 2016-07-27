/*
mediastreamer2 library - modular sound and video processing and streaming

 * Copyright (C) 2011  Belledonne Communications, Grenoble, France

	 Author: Simon Morlat <simon.morlat@linphone.org>

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

#include "mediastreamer2/bitratecontrol.h"

static const int probing_up_interval=10;

enum state_t{
	Init,
	Probing,
	Stable,
	ProbingUp
};

const char *state_name(enum state_t st){
	switch(st){
		case Init: return "Init";
		case Probing: return "Probing";
		case Stable: return "Stable";
		case ProbingUp: return "ProbingUp";
	}
	return "bad state";
}

struct _MSBitrateController{
	MSQosAnalyzer *analyzer;
	MSBitrateDriver *driver;
	enum state_t state;
	int stable_count;
	int probing_up_count;

};

MSBitrateController *ms_bitrate_controller_new(MSQosAnalyzer *qosanalyzer, MSBitrateDriver *driver){
	MSBitrateController *obj=ms_new0(MSBitrateController,1);
	obj->analyzer=ms_qos_analyzer_ref(qosanalyzer);
	obj->driver=ms_bitrate_driver_ref(driver);
	return obj;
}

static int execute_action(MSBitrateController *obj, const MSRateControlAction *action){
	return ms_bitrate_driver_execute_action(obj->driver,action);
}

static void state_machine(MSBitrateController *obj){
	MSRateControlAction action = {0};
	switch(obj->state){
		case Stable:
			obj->stable_count++;
		case Init:
			ms_qos_analyzer_suggest_action(obj->analyzer,&action);
			if (action.type!=MSRateControlActionDoNothing){
				execute_action(obj,&action);
				obj->state=Probing;
			}else if (obj->stable_count>=probing_up_interval){
				action.type=MSRateControlActionIncreaseQuality;
				action.value=10;
				execute_action(obj,&action);
				obj->state=ProbingUp;
				obj->probing_up_count=0;
			}
		break;
		case Probing:
			obj->stable_count=0;
			if (ms_qos_analyzer_has_improved(obj->analyzer)){
				obj->state=Stable;
			}else{
				ms_qos_analyzer_suggest_action(obj->analyzer,&action);
				if (action.type!=MSRateControlActionDoNothing){
					execute_action(obj,&action);
				}
			}
		break;
		case ProbingUp:
			obj->stable_count=0;
			obj->probing_up_count++;
			ms_qos_analyzer_suggest_action(obj->analyzer,&action);
			if (action.type!=MSRateControlActionDoNothing){
				execute_action(obj,&action);
				obj->state=Probing;
			}else{
				/*continue with slow ramp up*/
				if (obj->probing_up_count==2){
					action.type=MSRateControlActionIncreaseQuality;
					action.value=10;
					if (execute_action(obj,&action)==-1){
						/* we reached the maximum*/
						obj->state=Init;
					}
					obj->probing_up_count=0;
				}
			}
		break;
		default:
		break;
	}
	ms_message("MSBitrateController: current state is %s",state_name(obj->state));
}



void ms_bitrate_controller_process_rtcp(MSBitrateController *obj, mblk_t *rtcp){
	if (ms_qos_analyzer_process_rtcp(obj->analyzer,rtcp)){
		state_machine(obj);
	}
}

void ms_bitrate_controller_update(MSBitrateController *obj){
	ms_qos_analyzer_update(obj->analyzer);
}

MSQosAnalyzer * ms_bitrate_controller_get_qos_analyzer(MSBitrateController *obj){
	return obj->analyzer;
}

void ms_bitrate_controller_destroy(MSBitrateController *obj){
	ms_qos_analyzer_unref(obj->analyzer);
	ms_bitrate_driver_unref(obj->driver);
	ms_free(obj);
}

MSBitrateController *ms_audio_bitrate_controller_new(RtpSession *session, MSFilter *encoder, unsigned int flags){
	return ms_bitrate_controller_new(
	                                 ms_simple_qos_analyzer_new(session),
	                                 ms_audio_bitrate_driver_new(session, encoder));
}

MSBitrateController *ms_av_bitrate_controller_new(RtpSession *asession, MSFilter *aenc, RtpSession *vsession, MSFilter *venc){
	return ms_bitrate_controller_new(
	                                 ms_simple_qos_analyzer_new(vsession),
	                                 ms_av_bitrate_driver_new(asession, aenc, vsession, venc));
}

MSBitrateController *ms_bandwidth_bitrate_controller_new(RtpSession *asession, MSFilter *aenc, RtpSession *vsession, MSFilter *venc){
	return ms_bitrate_controller_new(
	                                 ms_stateful_qos_analyzer_new(vsession?vsession:asession),
	                                 ms_bandwidth_bitrate_driver_new(asession, aenc, vsession, venc));
}

