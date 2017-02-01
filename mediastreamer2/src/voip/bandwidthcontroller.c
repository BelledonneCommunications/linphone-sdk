/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2017 Belledonne Communications SARL

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
#include "mediastreamer2/mediastream.h"


MSBandwidthController *ms_bandwidth_controller_new(void){
	MSBandwidthController *obj = ms_new0(MSBandwidthController, 1);
	return obj;
}

static void ms_bandwidth_controller_estimate_bandwidths(MSBandwidthController *obj){
	bctbx_list_t *elem;
	float bandwidth = 0;
	
	for (elem = obj->streams; elem != NULL; elem = elem->next){
		MediaStream *ms = (MediaStream*) elem->data;
		float bw;
		if (media_stream_get_state(ms) != MSStreamStarted) continue;
		bw = rtp_session_get_recv_bandwidth_smooth(ms->sessions.rtp_session);
		bandwidth += bw;
		if (ms == obj->controlled_stream) obj->stats.controlled_stream_bandwidth = bw;
	}
	obj->stats.estimated_download_bandwidth = bandwidth;
	ms_message("MSBandwidthController: total received bandwidth is %f kbit/s, controlled stream bandwidth is %f kbit/s", bandwidth*1e-3, obj->stats.controlled_stream_bandwidth*1.e-3);
}

static float compute_target_bandwith_for_controlled_stream(MSBandwidthController *obj, float reduction_factor){
	float bw_used_by_other_streams;
	float new_total_bandwidth_requested;
	float controlled_stream_bandwidth_requested;

	bw_used_by_other_streams = obj->stats.estimated_download_bandwidth - obj->stats.controlled_stream_bandwidth;
	new_total_bandwidth_requested = obj->stats.estimated_download_bandwidth * reduction_factor;
	controlled_stream_bandwidth_requested = new_total_bandwidth_requested - bw_used_by_other_streams;
	if (controlled_stream_bandwidth_requested <= 0){
		ms_error("MSBandwidthController: controlled stream bandwidth requested is %f (bug)", controlled_stream_bandwidth_requested);
	}
	return controlled_stream_bandwidth_requested;
}

static void resync_jitter_buffers(MSBandwidthController *obj){
	bctbx_list_t *elem;
	
	for (elem = obj->streams; elem != NULL; elem = elem->next){
		MediaStream *ms = (MediaStream*) elem->data;
		rtp_session_resync(ms->sessions.rtp_session);
	}
}

static void on_congestion_state_changed(const OrtpEventData *evd, void *user_pointer){
	MediaStream *ms = (MediaStream*)user_pointer;
	MSBandwidthController *obj = ms->bandwidth_controller;
	float controlled_stream_bandwidth_requested;
	RtpSession *session;
	
	if (ms != obj->controlled_stream){
		ms_message("MSBandwidthController: congestion event (%i) received on stream [%p][%s], not the controlled one.", (int)evd->info.congestion_detected,
			   ms, ms_format_type_to_string(ms->type));
		return;
	}
	session = obj->controlled_stream->sessions.rtp_session;
	if (evd->info.congestion_detected){
		/*We are detecting a congestion. First estimate the total bandwidth received at the time of the congestion*/
		ms_bandwidth_controller_estimate_bandwidths(obj);
		/*we need to clear the congestion by firstly requesting a bandwidth usage much lower than the theoritically possible,
		 so that the congested router can finaly expedite all the late packets it retains.*/
		controlled_stream_bandwidth_requested = compute_target_bandwith_for_controlled_stream(obj, 0.7f);
		
		if (controlled_stream_bandwidth_requested > 0){
			ms_message("MSBandwidthController: congestion detected - sending tmmbr for stream [%p][%s] for target [%f] kbit/s",
				   obj->controlled_stream, ms_format_type_to_string(obj->controlled_stream->type), controlled_stream_bandwidth_requested*1e-3);
		}
	}else{
		/*now that the congestion has ended, we can submit a new TMMBR to request a bandwidth closer to the maximum available*/
		controlled_stream_bandwidth_requested = compute_target_bandwith_for_controlled_stream(obj, 0.9f);
		
		if (controlled_stream_bandwidth_requested > 0){
			ms_message("MSBandwidthController: congestion resolved - sending tmmbr for stream [%p][%s] for target [%f] kbit/s",
				   obj->controlled_stream, ms_format_type_to_string(obj->controlled_stream->type), controlled_stream_bandwidth_requested*1e-3);
		}
		/*we shall reset the jitter buffers, so that they recover faster their diverged states*/
		resync_jitter_buffers(obj);
	}
	rtp_session_send_rtcp_fb_tmmbr(session, (uint64_t)controlled_stream_bandwidth_requested);
}

/*THis function just selects a video stream if any, or an audio stream otherwise.
 * It could be refined to select the most consuming stream...*/
static void elect_controlled_stream(MSBandwidthController *obj){
	bctbx_list_t *elem;
	bool_t done = FALSE;
	
	obj->controlled_stream = NULL;
	for (elem = obj->streams; elem != NULL && !done; elem = elem->next){
		MediaStream *ms = (MediaStream*) elem->data;
		switch(ms->type){
			case MSAudio:
				obj->controlled_stream = ms;
			break;
			case MSVideo:
				obj->controlled_stream = ms;
				done = TRUE;
			break;
			case MSText:
			break;
			case MSUnknownMedia:
			break;
		}
	}
}


void ms_bandwidth_controller_add_stream(MSBandwidthController *obj, struct _MediaStream *stream){
	ortp_ev_dispatcher_connect(media_stream_get_event_dispatcher(stream), ORTP_EVENT_CONGESTION_STATE_CHANGED, 0, 
		on_congestion_state_changed, stream);
	rtp_session_enable_congestion_detection(stream->sessions.rtp_session, TRUE);
	stream->bandwidth_controller = obj;
	obj->streams = bctbx_list_append(obj->streams, stream);
	elect_controlled_stream(obj);
}

void ms_bandwidth_controller_remove_stream(MSBandwidthController *obj, struct _MediaStream *stream){
	if (bctbx_list_find(obj->streams, stream) == NULL) return;
	ortp_ev_dispatcher_disconnect(media_stream_get_event_dispatcher(stream), ORTP_EVENT_CONGESTION_STATE_CHANGED, 0, 
		on_congestion_state_changed);
	rtp_session_enable_congestion_detection(stream->sessions.rtp_session, FALSE);
	stream->bandwidth_controller = NULL;
	obj->streams = bctbx_list_remove(obj->streams, stream);
	elect_controlled_stream(obj);
}

const MSBandwidthControllerStats * ms_bandwidth_controller_get_stats(MSBandwidthController *obj){
	return &obj->stats;
}

void ms_bandwidth_controller_destroy(MSBandwidthController *obj){
	bctbx_list_free(obj->streams);
	obj->streams = NULL;
	ms_free(obj);
}
