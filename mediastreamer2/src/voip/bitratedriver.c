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

static const int max_ptime=100;
static const int min_ptime=10;

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
	RtpSession *session;
	MSFilter *encoder;
	int min_ptime;
	int nom_bitrate;
	int cur_ptime;
	int cur_bitrate;
	int encoder_caps;
};

typedef struct _MSAudioBitrateDriver MSAudioBitrateDriver;

static int apply_ptime(MSAudioBitrateDriver *obj,int target_ptime){
	char tmp[64];
	int result=-1;
	if (target_ptime < min_ptime || target_ptime>max_ptime) {
		ms_error("MSAudioBitrateDriver [%p]: cannot apply ptime value [%i] on [%p] because out of range [%i..%i]",obj,target_ptime,obj,min_ptime,max_ptime);
		return -1;
	}

	if (ms_filter_has_method(obj->encoder,MS_AUDIO_ENCODER_SET_PTIME)) {
		result = ms_filter_call_method(obj->encoder,MS_AUDIO_ENCODER_SET_PTIME,&target_ptime);
	} else {
		/*legacy*/
		snprintf(tmp,sizeof(tmp),"ptime=%i",target_ptime);
		result = ms_filter_call_method(obj->encoder,MS_FILTER_ADD_FMTP,tmp);
	}

	if (ms_filter_has_method(obj->encoder,MS_AUDIO_ENCODER_GET_PTIME)) {
		ms_filter_call_method(obj->encoder,MS_AUDIO_ENCODER_GET_PTIME,&obj->cur_ptime);
	} else {
		/*legacy*/
		if (result==0) {
			obj->cur_ptime=target_ptime;
		} /*else ptime remain unchanged*/
	}
	if (result == 0) {
		ms_message("MSAudioBitrateDriver [%p]: ptime is now [%i ms]",obj,obj->cur_ptime);
	} else {
		ms_message("MSAudioBitrateDriver [%p]: cannot move ptime from [%i ms] to [%i ms]",obj,obj->cur_ptime,target_ptime);
	}
	return result;
}

static int inc_ptime(MSAudioBitrateDriver *obj){
	return apply_ptime(obj,obj->cur_ptime+obj->min_ptime);
}

static int dec_ptime(MSAudioBitrateDriver *obj){
	return apply_ptime(obj,obj->cur_ptime-obj->min_ptime);
}

static int audio_bitrate_driver_execute_action(MSBitrateDriver *objbase, const MSRateControlAction *action){
	MSAudioBitrateDriver *obj=(MSAudioBitrateDriver*)objbase;
	ms_message("MSAudioBitrateDriver: executing action of type %s, value=%i",ms_rate_control_action_type_name(action->type),action->value);

	if (obj->nom_bitrate==0){
		ms_filter_call_method(obj->encoder,MS_FILTER_GET_BITRATE,&obj->nom_bitrate);
		if (obj->nom_bitrate==0){
			ms_warning("MSAudioBitrateDriver: Not doing bitrate control on audio encoder, it does not seem to support that. Controlling ptime only.");
			obj->nom_bitrate=-1;
		}else
			obj->cur_bitrate=obj->nom_bitrate;
	}
	if (obj->cur_ptime==0 || ms_filter_has_method(obj->encoder,MS_AUDIO_ENCODER_GET_PTIME)){ /*always sync current ptime if possible*/
		ms_filter_call_method(obj->encoder,MS_AUDIO_ENCODER_GET_PTIME,&obj->cur_ptime);
		if (obj->cur_ptime==0){
			ms_warning("MSAudioBitrateDriver: encoder %s does not implement MS_AUDIO_ENCODER_GET_PTIME. Consider to implement this method for better accuracy of rate control.",obj->encoder->desc->name);
			obj->cur_ptime=obj->min_ptime;
		}
	}

	if (action->type==MSRateControlActionDecreaseBitrate){
		
		/*reducing bitrate of the codec isn't sufficient. Increasing ptime is much more efficient*/
		if ((obj->encoder_caps & MS_AUDIO_ENCODER_CAP_AUTO_PTIME) || inc_ptime(obj)){
			if (obj->nom_bitrate>0){
				int cur_br=0;
				int new_br;

				/*if max ptime is reached, then try to reduce the codec bitrate if possible */

				if (ms_filter_call_method(obj->encoder,MS_FILTER_GET_BITRATE,&cur_br)!=0){
					ms_message("MSAudioBitrateDriver: GET_BITRATE failed");
					return 0;
				}
				obj->cur_bitrate=cur_br;
				new_br=cur_br-((cur_br*action->value)/100);

				ms_message("MSAudioBitrateDriver: Attempting to reduce audio bitrate from %i to %i",cur_br,new_br);
				if (ms_filter_call_method(obj->encoder,MS_FILTER_SET_BITRATE,&new_br)!=0){
					ms_message("MSAudioBitrateDriver: SET_BITRATE failed, incrementing ptime");
					return inc_ptime(obj);
				} else {
					rtp_session_set_target_upload_bandwidth(obj->session, new_br);
				}
				new_br=0;
				ms_filter_call_method(obj->encoder,MS_FILTER_GET_BITRATE,&new_br);
				ms_message("MSAudioBitrateDriver: bitrate actually set to %i",new_br);
				obj->cur_bitrate=new_br;
			}
		}
	}else if (action->type==MSRateControlActionDecreasePacketRate){
		return inc_ptime(obj);
	}else if (action->type==MSRateControlActionIncreaseQuality){
		int ret=0;
		if (!(obj->encoder_caps & MS_AUDIO_ENCODER_CAP_AUTO_PTIME)){
			if (obj->cur_ptime>obj->min_ptime){
				ret=dec_ptime(obj);
			}
		}
		if (obj->nom_bitrate>0){
			int cur_bitrate=0;
			if (ms_filter_call_method(obj->encoder,MS_FILTER_GET_BITRATE,&cur_bitrate)==0){
				if (cur_bitrate > 0  && cur_bitrate<obj->nom_bitrate){
					obj->cur_bitrate=(obj->cur_bitrate*140)/100;
					if (obj->cur_bitrate>= obj->nom_bitrate) {
						obj->cur_bitrate=obj->nom_bitrate;
						ret=-1;/*we reached the nominal value*/
					}
					ms_message("MSAudioBitrateDriver: increasing bitrate of codec to %i",obj->cur_bitrate);
					if (ms_filter_call_method(obj->encoder,MS_FILTER_SET_BITRATE,&obj->cur_bitrate)!=0){
						ms_message("MSAudioBitrateDriver: could not set codec bitrate to %i",obj->cur_bitrate);
					}else {
						rtp_session_set_target_upload_bandwidth(obj->session, obj->cur_bitrate);
					}
				}
			}else ms_warning("MSAudioBitrateDriver: MS_FILTER_GET_BITRATE failed.");
		}
		return ret;
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


MSBitrateDriver *ms_audio_bitrate_driver_new(RtpSession *session, MSFilter *encoder){
	MSAudioBitrateDriver *obj=ms_new0(MSAudioBitrateDriver,1);
	obj->parent.desc=&audio_bitrate_driver;
	obj->session = session;
	obj->encoder=encoder;
	obj->min_ptime=20;
	obj->cur_ptime=0;
	obj->cur_bitrate=obj->nom_bitrate=0;
	if (ms_filter_has_method(obj->encoder, MS_AUDIO_ENCODER_GET_CAPABILITIES))
			ms_filter_call_method(obj->encoder, MS_AUDIO_ENCODER_GET_CAPABILITIES, &obj->encoder_caps);
	return (MSBitrateDriver*)obj;
}

static const int min_video_bitrate=64000;

typedef struct _MSAVBitrateDriver{
	MSBitrateDriver parent;
	MSBitrateDriver *audio_driver;
	RtpSession *vsession;
	MSFilter *venc;
	int nom_bitrate;
	int cur_bitrate;
}MSAVBitrateDriver;

static int dec_video_bitrate(MSAVBitrateDriver *obj, const MSRateControlAction *action){
	int new_br;

	ms_filter_call_method(obj->venc,MS_FILTER_GET_BITRATE,&obj->cur_bitrate);
	new_br=(int)(((float)obj->cur_bitrate)*(100.0f-(float)action->value)/100.0f);
	if (new_br<min_video_bitrate){
		ms_message("MSAVBitrateDriver: reaching low bound.");
		new_br=min_video_bitrate;
	}
	if (new_br!=obj->cur_bitrate){
		ms_message("MSAVBitrateDriver: targeting %i bps for video encoder.",new_br);
		ms_filter_call_method(obj->venc,MS_FILTER_SET_BITRATE,&new_br);
		rtp_session_set_target_upload_bandwidth(obj->vsession, new_br);
		obj->cur_bitrate=new_br;
	}
	return new_br==min_video_bitrate ? -1 : 0;
}

static int inc_video_bitrate(MSAVBitrateDriver *obj, const MSRateControlAction *action){
	int newbr;
	int ret=0;

	if (obj->cur_bitrate==0) return -1; /*current  bitrate was not known*/
	newbr=(int)((float)obj->cur_bitrate*(1.0f+((float)action->value/100.0f)));
	if (newbr>obj->nom_bitrate){
		newbr=obj->nom_bitrate;
		ret=-1;
	}
	if (newbr!=obj->cur_bitrate){
		obj->cur_bitrate=newbr;
		ms_message("MSAVBitrateDriver: increasing bitrate to %i bps for video encoder.",obj->cur_bitrate);
		ms_filter_call_method(obj->venc,MS_FILTER_SET_BITRATE,&obj->cur_bitrate);
		rtp_session_set_target_upload_bandwidth(obj->vsession, obj->cur_bitrate);
	}
	return ret;
}

static int av_driver_execute_action(MSBitrateDriver *objbase, const MSRateControlAction *action){
	MSAVBitrateDriver *obj=(MSAVBitrateDriver*)objbase;
	int ret=0;
	if (obj->nom_bitrate==0){
		ms_filter_call_method(obj->venc,MS_FILTER_GET_BITRATE,&obj->nom_bitrate);
		if (obj->nom_bitrate==0){
			ms_warning("MSAVBitrateDriver: Not doing adaptive rate control on video encoder, it does not seem to support that.");
			return -1;
		}
	}

	switch(action->type){
		case MSRateControlActionDecreaseBitrate:
			ret=dec_video_bitrate(obj,action);
		break;
		case MSRateControlActionDecreasePacketRate:
			if (obj->audio_driver){
				ret=ms_bitrate_driver_execute_action(obj->audio_driver,action);
			}
		break;
		case MSRateControlActionIncreaseQuality:
			ret=inc_video_bitrate(obj,action);
		break;
		case MSRateControlActionDoNothing:
		break;
	}
	return ret;
}

static void av_bitrate_driver_uninit(MSBitrateDriver *objbase){
	MSAVBitrateDriver *obj=(MSAVBitrateDriver*)objbase;
	if (obj->audio_driver)
		ms_bitrate_driver_unref(obj->audio_driver);

}

static MSBitrateDriverDesc av_bitrate_driver={
	av_driver_execute_action,
	av_bitrate_driver_uninit
};

MSBitrateDriver *ms_av_bitrate_driver_new(RtpSession *asession, MSFilter *aenc, RtpSession *vsession, MSFilter *venc){
	MSAVBitrateDriver *obj=ms_new0(MSAVBitrateDriver,1);
	obj->parent.desc=&av_bitrate_driver;
	obj->audio_driver=((asession != NULL) && (aenc!=NULL)) ? ms_bitrate_driver_ref(ms_audio_bitrate_driver_new(asession, aenc)) : NULL;
	obj->vsession = vsession;
	obj->venc=venc;

	return (MSBitrateDriver*)obj;
}



typedef struct _MSBandwidthBitrateDriver{
	MSBitrateDriver parent;
	MSBitrateDriver *audio_driver;
	RtpSession *vsession;
	MSFilter *venc;
	int nom_bitrate;
	int cur_bitrate;
}MSBandwidthBitrateDriver;

static int bandwidth_change_ptime(MSAudioBitrateDriver *obj, int percent){
	return apply_ptime(obj,(int)(obj->min_ptime + (max_ptime - obj->min_ptime) * percent / 100.f));
}

static int bandwidth_change_video_bitrate(MSBandwidthBitrateDriver *obj, const MSRateControlAction *action){
	int newbr;
	int oldbr;
	int ret=0;
	bool_t decrease = (action->type == MSRateControlActionDecreaseBitrate);
	int bound= (decrease) ? min_video_bitrate : obj->nom_bitrate;

	ms_filter_call_method(obj->venc,MS_FILTER_GET_BITRATE,&obj->cur_bitrate);
	if (obj->cur_bitrate==0){
		ms_message("MSBandwidthBitrateDriver: current bitrate was not known.");
		return -1; /*current  bitrate was not known*/
	}
	newbr = (int)((float)obj->cur_bitrate*(100.0f+(decrease?-1:1)*(float)action->value)/100.0f);
	if ((decrease&&newbr<bound) || (!decrease&&newbr>bound)){
		if (obj->cur_bitrate==bound){
			ms_message("MSBandwidthBitrateDriver: bitrate already reached %s limit %d b/s.", (decrease)?"min":"max",bound);
			return -1;
		}
		newbr=bound;
	}
	oldbr=obj->cur_bitrate;
	obj->cur_bitrate=newbr;
	rtp_session_set_target_upload_bandwidth(obj->vsession, obj->cur_bitrate);
	ms_filter_call_method(obj->venc,MS_FILTER_SET_BITRATE,&obj->cur_bitrate);
	ms_filter_call_method(obj->venc,MS_FILTER_GET_BITRATE,&newbr);

	ms_message("MSBandwidthBitrateDriver: changing bitrate from %i to %i bps for video encoder.",oldbr,newbr);
	return ret;
}

static int bandwidth_driver_execute_action(MSBitrateDriver *objbase, const MSRateControlAction *action){
	MSBandwidthBitrateDriver *obj=(MSBandwidthBitrateDriver*)objbase;
	int ret=0;
	if (obj->nom_bitrate==0&&obj->venc){
		ms_filter_call_method(obj->venc,MS_FILTER_GET_BITRATE,&obj->nom_bitrate);
		if (obj->nom_bitrate==0){
			ms_warning("MSBandwidthBitrateDriver: Not doing adaptive rate control on video encoder, it does not seem to support that.");
			return -1;
		}
	}

	if (!obj->venc){
		ret=1;
	}
	switch(action->type){
		case MSRateControlActionDecreaseBitrate:
			if (obj->venc){
				ret=bandwidth_change_video_bitrate(obj,action);
			}
			if (ret!=0 && obj->audio_driver){
				ret=ms_bitrate_driver_execute_action(obj->audio_driver,action);
			}
		break;
		case MSRateControlActionDecreasePacketRate:
			if (obj->audio_driver){
				ret=bandwidth_change_ptime((MSAudioBitrateDriver*) obj->audio_driver, 100);
			}else{
				ret=1;
			}
		break;
		case MSRateControlActionIncreaseQuality:
			if (obj->venc){
				ret=bandwidth_change_video_bitrate(obj,action);
			}
			if (ret!=0 && obj->audio_driver){
				ret=ms_bitrate_driver_execute_action(obj->audio_driver,action);
			}
		break;
		case MSRateControlActionDoNothing:
		break;
	}
	ms_message("MSBandwidthBitrateDriver: Action %s %s", ms_rate_control_action_type_name(action->type), ret == 0 ? "succeeded" : "failed");
	return ret;
}

static void bandwidth_bitrate_driver_uninit(MSBitrateDriver *objbase){
}

static MSBitrateDriverDesc bandwidth_bitrate_driver={
	bandwidth_driver_execute_action,
	bandwidth_bitrate_driver_uninit
};

MSBitrateDriver *ms_bandwidth_bitrate_driver_new(RtpSession *asession, MSFilter *aenc, RtpSession *vsession, MSFilter *venc){
	MSBandwidthBitrateDriver *obj=ms_new0(MSBandwidthBitrateDriver,1);
	obj->parent.desc=&bandwidth_bitrate_driver;
	obj->vsession = vsession;
	obj->audio_driver=(aenc!=NULL) ? ms_bitrate_driver_ref(ms_audio_bitrate_driver_new(asession,aenc)) : NULL;
	obj->venc=venc;

	return (MSBitrateDriver*)obj;
}
