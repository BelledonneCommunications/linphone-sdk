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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "mediastreamer2/bitratecontrol.h"

static const int max_ptime=100;

int ms_bitrate_driver_execute_action(MSBitrateDriver *obj, const MSRateControlAction *action){
	if (obj->desc->execute_action)
		return obj->desc->execute_action(obj,action);
	else ms_error("Driver does not implement execute_action");
	return -1;
}

MSBitrateDriver * ms_bitrate_driver_ref(MSBitrateDriver *obj){
	obj->refcnt++;
	return obj;
}

void ms_bitrate_driver_unref(MSBitrateDriver *obj){
	obj->refcnt--;
	if (obj->refcnt<=0){
		if (obj->desc->uninit)
			obj->desc->uninit(obj);
		ms_free(obj);
	}
}

struct _MSAudioBitrateDriver{
	MSBitrateDriver parent;
	MSFilter *encoder;
	int min_ptime;
	int nom_bitrate;
	int cur_ptime;
	int cur_bitrate;
};

typedef struct _MSAudioBitrateDriver MSAudioBitrateDriver;

static void apply_ptime(MSAudioBitrateDriver *obj){
	char tmp[64];
	snprintf(tmp,sizeof(tmp),"ptime=%i",obj->cur_ptime);
	if (ms_filter_call_method(obj->encoder,MS_FILTER_ADD_FMTP,tmp)!=0){
		ms_message("AudioBitrateController: failed ptime command.");
	}else ms_message("AudioBitrateController: ptime changed to %i",obj->cur_ptime);
}

static int inc_ptime(MSAudioBitrateDriver *obj){
	if (obj->cur_ptime>=max_ptime){
		ms_message("AudioBitrateController: maximum ptime reached");
		return -1;
	}
	obj->cur_ptime+=obj->min_ptime;
	apply_ptime(obj);
	return 0;
}

static int audio_bitrate_driver_execute_action(MSBitrateDriver *objbase, const MSRateControlAction *action){
	MSAudioBitrateDriver *obj=(MSAudioBitrateDriver*)objbase;
	ms_message("MSAudioBitrateDriver: executing action of type %s, value=%i",ms_rate_control_action_type_name(action->type),action->value);
	if (action->type==MSRateControlActionDecreaseBitrate){
		/*reducing bitrate of the codec actually doesn't work very well (not enough). Increasing ptime is much more efficient*/
		if (inc_ptime(obj)==-1){
			if (obj->nom_bitrate>0){
				int cur_br=0;
				int new_br;

				if (obj->nom_bitrate==0){
					if (ms_filter_call_method(obj->encoder,MS_FILTER_GET_BITRATE,&obj->nom_bitrate)!=0){
						ms_message("MSAudioBitrateDriver: Encoder has nominal bitrate %i",obj->nom_bitrate);
					}	
					obj->cur_bitrate=obj->nom_bitrate;
				}
				/*if max ptime is reached, then try to reduce the codec bitrate if possible */
				
				if (ms_filter_call_method(obj->encoder,MS_FILTER_GET_BITRATE,&cur_br)!=0){
					ms_message("AudioBitrateController: GET_BITRATE failed");
					return 0;
				}
				new_br=cur_br-((cur_br*action->value)/100);
		
				ms_message("MSAudioBitrateDriver: Attempting to reduce audio bitrate to %i",new_br);
				if (ms_filter_call_method(obj->encoder,MS_FILTER_SET_BITRATE,&new_br)!=0){
					ms_message("MSAudioBitrateDriver: SET_BITRATE failed, incrementing ptime");
					inc_ptime(obj);
					return 0;
				}
				new_br=0;
				ms_filter_call_method(obj->encoder,MS_FILTER_GET_BITRATE,&new_br);
				ms_message("MSAudioBitrateDriver: bitrate actually set to %i",new_br);
				obj->cur_bitrate=new_br;
			}
		}
	}else if (action->type==MSRateControlActionDecreasePacketRate){
		inc_ptime(obj);
	}else if (action->type==MSRateControlActionIncreaseQuality){
		if (obj->cur_bitrate<obj->nom_bitrate){
			ms_message("MSAudioBitrateDriver: increasing bitrate of codec");
			if (ms_filter_call_method(obj->encoder,MS_FILTER_SET_BITRATE,&obj->nom_bitrate)!=0){
				ms_message("MSAudioBitrateDriver: could not restore nominal codec bitrate (%i)",obj->nom_bitrate);
			}else obj->cur_bitrate=obj->nom_bitrate;		
		}else if (obj->cur_ptime>obj->min_ptime){
			obj->cur_ptime-=obj->min_ptime;
			apply_ptime(obj);
		}else return -1;
	}
	return 0;
}

static void audio_bitrate_driver_uninit(MSBitrateDriver *objbase){
	//MSAudioBitrateDriver *obj=(MSBitrateDriver*)objbase;
	
}

static MSBitrateDriverDesc audio_bitrate_driver={
	audio_bitrate_driver_execute_action,
	audio_bitrate_driver_uninit
};


MSBitrateDriver *ms_audio_bitrate_driver_new(MSFilter *encoder){
	MSAudioBitrateDriver *obj=ms_new0(MSAudioBitrateDriver,1);
	obj->parent.desc=&audio_bitrate_driver;
	obj->encoder=encoder;
	obj->cur_ptime=obj->min_ptime=20;
	obj->cur_bitrate=obj->nom_bitrate=0;
	return (MSBitrateDriver*)obj;
}
