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
#include "qosanalyzer.h"

#include <math.h>

#define LOSS_RATE_MIN_INTERVAL 120

/**
 * Analyses a received RTCP packet.
 * Returns TRUE is relevant information has been found in the rtcp message, FALSE otherwise.
**/
bool_t ms_qos_analyzer_process_rtcp(MSQosAnalyzer *obj,mblk_t *msg){
	if (obj->desc->process_rtcp){
		return obj->desc->process_rtcp(obj,msg);
	}
	ms_error("Unimplemented process_rtcp() call.");
	return FALSE;
}

void ms_qos_analyzer_suggest_action(MSQosAnalyzer *obj, MSRateControlAction *action){
	if (obj->desc->suggest_action){
		obj->desc->suggest_action(obj,action);
	}
}

void ms_qos_analyzer_update(MSQosAnalyzer *obj){
	if (obj->desc->update){
		obj->desc->update(obj);
	}
}

bool_t ms_qos_analyzer_has_improved(MSQosAnalyzer *obj){
	if (obj->desc->has_improved){
		return obj->desc->has_improved(obj);
	}
	ms_error("Unimplemented has_improved() call.");
	return TRUE;
}

void ms_qos_analyzer_set_on_action_suggested(MSQosAnalyzer *obj,
	void (*on_action_suggested)(void*, int, const char**), void* u){
	obj->on_action_suggested=on_action_suggested;
	obj->on_action_suggested_user_pointer=u;
}

void ms_qos_analyser_set_label(MSQosAnalyzer *obj, const char *label){
	if (obj->label){
		ms_free(obj->label);
		obj->label=NULL;
	}
	if (label) obj->label=ms_strdup(label);
}

MSQosAnalyzer *ms_qos_analyzer_ref(MSQosAnalyzer *obj){
	obj->refcnt++;
	return obj;
}

void ms_qos_analyzer_unref(MSQosAnalyzer *obj){
	obj->refcnt--;
	if (obj->refcnt<=0){
		if (obj->desc->uninit)
			obj->desc->uninit(obj);
		if (obj->label) ms_free(obj->label);
		ms_free(obj);
	}
}

const char *ms_rate_control_action_type_name(MSRateControlActionType t){
	switch(t){
		case MSRateControlActionDoNothing:
			return "DoNothing";
		case MSRateControlActionIncreaseQuality:
			return "IncreaseQuality";
		case MSRateControlActionDecreaseBitrate:
			return "DecreaseBitrate";
		case MSRateControlActionDecreasePacketRate:
			return "DecreasePacketRate";
	}
	return "bad action type";
}

/******************************************************************************/
/***************************** Simple QoS analyzer ****************************/
/******************************************************************************/
static bool_t rt_prop_doubled(rtpstats_t *cur,rtpstats_t *prev){
	//ms_message("AudioBitrateController: cur=%f, prev=%f",cur->rt_prop,prev->rt_prop);
	if (cur->rt_prop>=significant_delay && prev->rt_prop>0){
		if (cur->rt_prop>=(prev->rt_prop*2.0)){
			/*propagation doubled since last report */
			return TRUE;
		}
	}
	return FALSE;
}

static bool_t simple_rt_prop_increased(MSSimpleQosAnalyzer *obj){
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];
	rtpstats_t *prev=&obj->stats[(STATS_HISTORY+obj->curindex-1) % STATS_HISTORY];

	if (rt_prop_doubled(cur,prev)){
		obj->rt_prop_doubled=TRUE;
		return TRUE;
	}
	return FALSE;
}

static bool_t simple_analyzer_process_rtcp(MSQosAnalyzer *objbase, mblk_t *rtcp){
	MSSimpleQosAnalyzer *obj=(MSSimpleQosAnalyzer*)objbase;
	rtpstats_t *cur;
	const report_block_t *rb=NULL;
	if (rtcp_is_SR(rtcp)){
		rb=rtcp_SR_get_report_block(rtcp,0);
	}else if (rtcp_is_RR(rtcp)){
		rb=rtcp_RR_get_report_block(rtcp,0);
	}
	if (rb && report_block_get_ssrc(rb)==rtp_session_get_send_ssrc(obj->session)){

		obj->curindex++;
		cur=&obj->stats[obj->curindex % STATS_HISTORY];

		if (obj->clockrate==0){
			PayloadType *pt=rtp_profile_get_payload(rtp_session_get_send_profile(obj->session),rtp_session_get_send_payload_type(obj->session));
			if (pt!=NULL) obj->clockrate=pt->clock_rate;
			else return FALSE;
		}
		if (ortp_loss_rate_estimator_process_report_block(obj->lre,rb)){
			cur->lost_percentage=ortp_loss_rate_estimator_get_value(obj->lre);
			cur->int_jitter=1000.0*(float)report_block_get_interarrival_jitter(rb)/(float)obj->clockrate;
			cur->rt_prop=rtp_session_get_round_trip_propagation(obj->session);
			/*
			ms_message("MSQosAnalyzer[%s]: lost_percentage=%f, int_jitter=%f ms, rt_prop=%f sec",
				objbase->label ? objbase->label : "", cur->lost_percentage,cur->int_jitter,cur->rt_prop);
			*/
		}
	}
	return rb!=NULL;
}

static void simple_analyzer_suggest_action(MSQosAnalyzer *objbase, MSRateControlAction *action){
	MSSimpleQosAnalyzer *obj=(MSSimpleQosAnalyzer*)objbase;
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];

	/*big losses and big jitter */
	if (cur->lost_percentage>=unacceptable_loss_rate && cur->int_jitter>=big_jitter){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=MIN(cur->lost_percentage,50);
		ms_message("MSQosAnalyzer: loss rate unacceptable and big jitter");
	}else if (simple_rt_prop_increased(obj)){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=20;
		ms_message("MSQosAnalyzer: rt_prop doubled.");
	}else if (cur->lost_percentage>=unacceptable_loss_rate){
		/*big loss rate but no jitter, and no big rtp_prop: pure lossy network*/
		action->type=MSRateControlActionDecreasePacketRate;
		ms_message("MSQosAnalyzer: loss rate unacceptable.");
	}else{
		action->type=MSRateControlActionDoNothing;
		ms_message("MSQosAnalyzer: everything is fine.");
	}

	if (objbase->on_action_suggested!=NULL){
		int i;
		const int datac=4;
		char *data[datac];
		data[0]=ms_strdup("lost_percentage rt_prop_increased int_jitter_ms rt_prop_ms");
		data[1]=ms_strdup_printf("%d %d %d %d"
			, (int)cur->lost_percentage
			, (simple_rt_prop_increased(obj)==TRUE)
			, (int)cur->int_jitter
			, (int)(1000*cur->rt_prop));
		data[2]=ms_strdup("action_type action_value");
		data[3]=ms_strdup_printf("%s %d"
			, ms_rate_control_action_type_name(action->type)
			, action->value);

		objbase->on_action_suggested(objbase->on_action_suggested_user_pointer, datac, (const char**)data);

		for (i=0;i<datac;++i){
			ms_free(data[i]);
		}
	}
}

static bool_t simple_analyzer_has_improved(MSQosAnalyzer *objbase){
	MSSimpleQosAnalyzer *obj=(MSSimpleQosAnalyzer*)objbase;
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];
	rtpstats_t *prev=&obj->stats[(STATS_HISTORY+obj->curindex-1) % STATS_HISTORY];

	if (prev->lost_percentage>=unacceptable_loss_rate){
		if (cur->lost_percentage<prev->lost_percentage){
			ms_message("MSQosAnalyzer: lost percentage has improved");
			return TRUE;
		}else goto end;
	}
	if (obj->rt_prop_doubled && cur->rt_prop<prev->rt_prop){
		ms_message("MSQosAnalyzer: rt prop decreased");
		obj->rt_prop_doubled=FALSE;
		return TRUE;
	}

	end:
	ms_message("MSQosAnalyzer: no improvements.");
	return FALSE;
}

static void simple_analyzer_uninit(MSQosAnalyzer *objbase){
	MSSimpleQosAnalyzer *obj=(MSSimpleQosAnalyzer*)objbase;
	ortp_loss_rate_estimator_destroy(obj->lre);
}

static MSQosAnalyzerDesc simple_analyzer_desc={
	simple_analyzer_process_rtcp,
	simple_analyzer_suggest_action,
	simple_analyzer_has_improved,
	NULL,
	simple_analyzer_uninit
};

MSQosAnalyzer * ms_simple_qos_analyzer_new(RtpSession *session){
	MSSimpleQosAnalyzer *obj=ms_new0(MSSimpleQosAnalyzer,1);
	obj->session=session;
	obj->parent.desc=&simple_analyzer_desc;
	obj->parent.type=Simple;
	obj->lre=ortp_loss_rate_estimator_new(LOSS_RATE_MIN_INTERVAL, rtp_session_get_seq_number(session));
	return (MSQosAnalyzer*)obj;
}




/******************************************************************************/
/***************************** Stateful QoS analyzer ****************************/
/******************************************************************************/
#if 1
#define RED 		"[1m[31m"
#define YELLOW 		"[1m[33m"
#define GREEN 		"[1m[32m"
#define RESET 		"[0m"
#define P( X ) 	printf X
#else
#define RED
#define YELLOW
#define GREEN
#define RESET
#define P( X ) ms_message X
#endif

const char *ms_qos_analyzer_network_state_name(MSQosAnalyzerNetworkState state){
	switch(state){
		case MSQosAnalyzerNetworkFine:
			return "fine";
		case MSQosAnalyzerNetworkUnstable:
			return "unstable";
		case MSQosAnalyzerNetworkCongested:
			return "congested";
		case MSQosAnalyzerNetworkLossy:
			return "lossy";
	}
	return "bad state type";
}

static int earlier_than(rtcpstatspoint_t *p, const time_t * now){
	if (p->timestamp < *now){
		ms_free(p);
		return FALSE;
	}
	return TRUE;
}
static int sort_points(const rtcpstatspoint_t *p1, const rtcpstatspoint_t *p2){
	return p1->bandwidth > p2->bandwidth;
}

static int stateful_qos_analyzer_get_total_emitted(const MSStatefulQosAnalyzer *obj, const report_block_t *rb){
	double dup = obj->burst_ratio;
	int burst_within_start = MAX(obj->previous_ext_high_seq_num_rec, obj->start_seq_number);
	int burst_within_end = MIN(report_block_get_high_ext_seq(rb), obj->last_seq_number);
	int uniq_emitted=report_block_get_high_ext_seq(rb) - obj->previous_ext_high_seq_num_rec;

	return uniq_emitted + MAX(0,burst_within_end - burst_within_start) * dup;
}

static double stateful_qos_analyzer_upload_bandwidth(MSStatefulQosAnalyzer *obj){
	double up_bw=rtp_session_get_send_bandwidth(obj->session)/1000.0;

	if (obj->upload_bandwidth_count){
		obj->upload_bandwidth_latest=obj->upload_bandwidth_sum/obj->upload_bandwidth_count;
	}

	obj->upload_bandwidth_count=0;
	obj->upload_bandwidth_sum=0;

	P(( GREEN "latest_up_bw=%f vs sum_up_bw=%f\n", up_bw, obj->upload_bandwidth_latest ));
	return obj->upload_bandwidth_latest;
}

static bool_t stateful_analyzer_process_rtcp(MSQosAnalyzer *objbase, mblk_t *rtcp){
	MSStatefulQosAnalyzer *obj=(MSStatefulQosAnalyzer*)objbase;
	rtpstats_t *cur;
	const report_block_t *rb=NULL;
	if (rtcp_is_SR(rtcp)){
		rb=rtcp_SR_get_report_block(rtcp,0);
	}else if (rtcp_is_RR(rtcp)){
		rb=rtcp_RR_get_report_block(rtcp,0);
	}
	if (rb && report_block_get_ssrc(rb)==rtp_session_get_send_ssrc(obj->session)){
		double up_bw = stateful_qos_analyzer_upload_bandwidth(obj);
		int total_emitted=stateful_qos_analyzer_get_total_emitted(obj, rb);
		obj->curindex++;
		cur=&obj->stats[obj->curindex % STATS_HISTORY];

		if (obj->clockrate==0){
			PayloadType *pt=rtp_profile_get_payload(rtp_session_get_send_profile(obj->session),rtp_session_get_send_payload_type(obj->session));
			if (pt!=NULL) obj->clockrate=pt->clock_rate;
			else return FALSE;
		}

		cur->lost_percentage=100.0*(float)report_block_get_fraction_lost(rb)/256.0;
		cur->int_jitter=1000.0*(float)report_block_get_interarrival_jitter(rb)/(float)obj->clockrate;
		cur->rt_prop=rtp_session_get_round_trip_propagation(obj->session);
		ms_message("MSQosAnalyzer: lost_percentage=%f, int_jitter=%f ms, rt_prop=%f sec",cur->lost_percentage,cur->int_jitter,cur->rt_prop);
		if (obj->curindex>2){
			double loss_rate = cur->lost_percentage/100.0;
			int cum_loss=report_block_get_cum_packet_loss(rb);
			int cum_loss_curr=cum_loss - obj->cum_loss_prev;
			int uniq_emitted=report_block_get_high_ext_seq(rb) - obj->previous_ext_high_seq_num_rec;

			if (obj->previous_ext_high_seq_num_rec > 0){
				loss_rate=(1. - (uniq_emitted - cum_loss_curr) * 1.f / total_emitted);
				P(( "RECEIVE estimated loss rate=%f vs 'real'=%f\n", loss_rate, report_block_get_fraction_lost(rb)/256. ));
			}

			obj->latest=ms_new0(rtcpstatspoint_t, 1);
			obj->latest->timestamp=ms_time(0);
			obj->latest->bandwidth=up_bw;
			obj->latest->loss_percent=MAX(0,loss_rate);
			obj->latest->rtt=cur->rt_prop;

			obj->rtcpstatspoint=ms_list_insert_sorted(obj->rtcpstatspoint, obj->latest, (MSCompareFunc)sort_points);

			if (obj->latest->loss_percent < 1e-5){
				MSList *it=obj->rtcpstatspoint;
				MSList *latest_pos=ms_list_find(obj->rtcpstatspoint,obj->latest);
				while (it!=latest_pos->next){
					((rtcpstatspoint_t *)it->data)->loss_percent=0.f;
					it = it->next;
				}
			}
			P(( YELLOW "one more %d: %f %f\n", obj->curindex-2, obj->latest->bandwidth, obj->latest->loss_percent ));

			if (ms_list_size(obj->rtcpstatspoint) > ESTIM_HISTORY){
				P(( RED "Reached list maximum capacity (count=%d)", ms_list_size(obj->rtcpstatspoint) ));
				/*clean everything which occurred 60 sec or more ago*/
				time_t clear_time = ms_time(0) - 60;
				obj->rtcpstatspoint = ms_list_remove_custom(obj->rtcpstatspoint, (MSCompareFunc)earlier_than, &clear_time);
				P(( RED "--> Cleaned list (count=%d)\n", ms_list_size(obj->rtcpstatspoint) ));
			}
		}
		obj->cum_loss_prev=report_block_get_cum_packet_loss(rb);
		obj->previous_ext_high_seq_num_rec=report_block_get_high_ext_seq(rb);
	}
	return rb!=NULL;
}

static float lerp(float inf, float sup, float v){
	return inf + (sup - inf) * v;
}

static MSList *find_first_with_loss(MSList *list){
	for(;list!=NULL;list=list->next){
		if (((rtcpstatspoint_t *)list->data)->loss_percent > 1e-5){
			return list;
		}
	}
	return NULL;
}

static void smooth_values(MSStatefulQosAnalyzer *obj){
	MSList *first_loss = find_first_with_loss(obj->rtcpstatspoint);
	MSList *it = obj->rtcpstatspoint;
	rtcpstatspoint_t *curr = (rtcpstatspoint_t *)it->data;
	double prev_loss = 0.;

	if (first_loss == obj->rtcpstatspoint){
		prev_loss = curr->loss_percent;
		curr->loss_percent = lerp(curr->loss_percent, ((rtcpstatspoint_t *)it->next->data)->loss_percent, .25);
		it = it->next;
	}else{
		it = first_loss;
	}

	/*nothing to smooth*/
	if (it == NULL){
		return;
	}

	curr = (rtcpstatspoint_t *)it->data;

	while (it->next != NULL){
		rtcpstatspoint_t *prev = ((rtcpstatspoint_t *)it->prev->data);
		rtcpstatspoint_t *next = ((rtcpstatspoint_t *)it->next->data);

		float v = (curr->bandwidth - prev->bandwidth) / (next->bandwidth - prev->bandwidth);
		float new_loss = lerp(prev_loss, next->loss_percent, v);
		prev_loss = curr->loss_percent;
		curr->loss_percent = (curr->loss_percent + new_loss) / 2.;
		it = it->next;
		curr = (rtcpstatspoint_t *)it->data;
	}
	curr->loss_percent = lerp(prev_loss, curr->loss_percent, .75);
}

static float compute_available_bw(MSStatefulQosAnalyzer *obj){
	MSList *it;
	double constant_network_loss = 0.;
	double mean_bw = 0.;
	MSList *current = obj->rtcpstatspoint;
	MSList *last = current;
	int size = ms_list_size(obj->rtcpstatspoint);
	if (current == NULL){
		P(( RED "Not points available for computation.\n" ));
		return -1;
	}

	while (last->next){
		last = last->next;
	}

	if (size > 3){
		smooth_values(obj);
	}
	/*suppose that first point is a reliable estimation of the constant network loss rate*/
	constant_network_loss = ((rtcpstatspoint_t *)obj->rtcpstatspoint->data)->loss_percent;


	P(( "\tconstant_network_loss=%f\n", constant_network_loss ));
	for (it = obj->rtcpstatspoint; it != NULL; it=it->next){
		rtcpstatspoint_t * point = (rtcpstatspoint_t *)it->data;
		P(( YELLOW "\t\tsorted values %d: %f %f\n",
			ms_list_position(obj->rtcpstatspoint, it), point->bandwidth, point->loss_percent));
	}

	if (size == 1){
		P(( RED "One single point" ));
		rtcpstatspoint_t *p = (rtcpstatspoint_t *)current->data;
		mean_bw = p->bandwidth * ((p->loss_percent>1e-5) ? (1-p->loss_percent):2);
	}else{
		while (current!=NULL && ((rtcpstatspoint_t*)current->data)->loss_percent<0.03+constant_network_loss){
			P(( "\t%d is stable\n", ms_list_position(obj->rtcpstatspoint, current) ));

			for (it=last;it!=current;it=it->prev){
				if (((rtcpstatspoint_t *)it->data)->loss_percent <= 0.03 + ((rtcpstatspoint_t*)current->data)->loss_percent){
					P(( "\t%d is less than %d\n", ms_list_position(obj->rtcpstatspoint, it), ms_list_position(obj->rtcpstatspoint, current) ));
					current = it;
					break;
				}
			}

			current = current->next;
		}

		if (current == NULL){
			/*constant loss rate - bad network conditions but no congestion*/
			mean_bw = 2 * ((rtcpstatspoint_t*)last->data)->bandwidth;
		}else if (current->prev == obj->rtcpstatspoint){
			/*only first packet is stable - might still be above real bandwidth*/
			rtcpstatspoint_t *p = (rtcpstatspoint_t *)current->prev->data;
			mean_bw = p->bandwidth * (1 - p->loss_percent);
		}else{
			/*there is some congestion*/
			mean_bw = .5*(((rtcpstatspoint_t*)current->prev->data)->bandwidth+((rtcpstatspoint_t*)current->data)->bandwidth);
		}

		P(( RED "[0->%d] Last stable is %d(%f;%f ))"
			, ms_list_position(obj->rtcpstatspoint, last)
			, ms_list_position(obj->rtcpstatspoint, (current ? current->prev : last))
			, ((rtcpstatspoint_t*) (current ? current->prev->data : last->data))->bandwidth
			, ((rtcpstatspoint_t*) (current ? current->prev->data : last->data))->loss_percent));
		if (current!=NULL){
			P(( RED ", first unstable is %d(%f;%f ))"
				, ms_list_position(obj->rtcpstatspoint, current)
				, ((rtcpstatspoint_t*) current->data)->bandwidth
				, ((rtcpstatspoint_t*) current->data)->loss_percent));
		}
	}
	P(( RED " --> estimated_available_bw=%f\n", mean_bw ));

	obj->network_loss_rate = constant_network_loss;
	obj->congestion_bandwidth = mean_bw;
	obj->network_state =
		(current==NULL && constant_network_loss < .1) ?	MSQosAnalyzerNetworkFine
		: (constant_network_loss > .1) ?				MSQosAnalyzerNetworkLossy
		:												MSQosAnalyzerNetworkCongested;

	return mean_bw;
}

static void stateful_analyzer_suggest_action(MSQosAnalyzer *objbase, MSRateControlAction *action){
	MSStatefulQosAnalyzer *obj=(MSStatefulQosAnalyzer*)objbase;

	float curbw = obj->latest ? obj->latest->bandwidth : 0.f;
	float bw = compute_available_bw(obj);

	/*try a burst every 50 seconds (10 RTCP packets)*/
	if (obj->curindex % 10 == 0){
		P(( YELLOW "try burst!\n" ));
		obj->burst_state = MSStatefulQosAnalyzerBurstEnable;
	}
	/*test a min burst to avoid overestimation of available bandwidth*/
	else if (obj->curindex % 10 == 2 || obj->curindex % 10 == 3){
		P(( YELLOW "try minimal burst!\n" ));
		bw *= .33;
	}

	/*no bandwidth estimation computed*/
	if (bw <= 0 || curbw <= 0){
		action->type=MSRateControlActionDoNothing;
		action->value=0;
	}else if (bw > curbw){
		action->type=MSRateControlActionIncreaseQuality;
		action->value=MAX(0, 100. * (bw / curbw - 1));
	}else{
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=MAX(10, -100. * (bw / curbw - 1));
	}

	P(( YELLOW "%s of value %d\n", ms_rate_control_action_type_name(action->type), action->value ));
}

static bool_t stateful_analyzer_has_improved(MSQosAnalyzer *objbase){
	/*never tell the controller that situation has improved to avoid 'Stable' state
	which is not necessary for this analyzer*/
	return FALSE;
}

static void stateful_analyzer_update(MSQosAnalyzer *objbase){
	MSStatefulQosAnalyzer *obj=(MSStatefulQosAnalyzer*)objbase;
	static time_t last_measure;

	if (last_measure != ms_time(0)){
		obj->upload_bandwidth_count++;
		obj->upload_bandwidth_sum+=rtp_session_get_send_bandwidth(obj->session)/1000.0;
	}
	last_measure = ms_time(0);

	if (obj->burst_duration_ms>0){
		switch (obj->burst_state){
		case MSStatefulQosAnalyzerBurstEnable:{
			obj->burst_state=MSStatefulQosAnalyzerBurstInProgress;
			ortp_gettimeofday(&obj->start_time, NULL);
			rtp_session_set_duplication_ratio(obj->session, obj->burst_ratio);
			obj->start_seq_number=obj->last_seq_number=obj->session->rtp.snd_seq;
		} case MSStatefulQosAnalyzerBurstInProgress: {
			struct timeval now;
			double elapsed;

			ortp_gettimeofday(&now,NULL);
			elapsed=((now.tv_sec-obj->start_time.tv_sec)*1000.0) +  ((now.tv_usec-obj->start_time.tv_usec)/1000.0);

			obj->last_seq_number=obj->session->rtp.snd_seq;

			if (elapsed > obj->burst_duration_ms){
				obj->burst_state=MSStatefulQosAnalyzerBurstDisable;
				rtp_session_set_duplication_ratio(obj->session, 0);
			}
		} case MSStatefulQosAnalyzerBurstDisable: {
		}
		}
	}
}

static void stateful_analyzer_uninit(MSQosAnalyzer *objbase){
	MSStatefulQosAnalyzer *obj=(MSStatefulQosAnalyzer*)objbase;
	ms_list_for_each(obj->rtcpstatspoint, ms_free);
}

static MSQosAnalyzerDesc stateful_analyzer_desc={
	stateful_analyzer_process_rtcp,
	stateful_analyzer_suggest_action,
	stateful_analyzer_has_improved,
	stateful_analyzer_update,
	stateful_analyzer_uninit,
};

MSQosAnalyzer * ms_stateful_qos_analyzer_new(RtpSession *session){
	MSStatefulQosAnalyzer *obj=ms_new0(MSStatefulQosAnalyzer,1);
	obj->session=session;
	obj->parent.desc=&stateful_analyzer_desc;
	obj->parent.type=Stateful;
	/*double the upload bandwidth based on a 5 sec RTCP reports interval*/
	obj->burst_duration_ms=1000;
	obj->burst_ratio=9;
	return (MSQosAnalyzer*)obj;
}


