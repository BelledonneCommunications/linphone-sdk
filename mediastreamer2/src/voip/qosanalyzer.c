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

#define RED 		"[1m[31m"
#define YELLOW 		"[1m[33m"
#define GREEN 		"[1m[32m"
#define RESET 		"[0m"
#define VA_ARGS(...) , ##__VA_ARGS__
#define P(c, ...) 	printf(GREEN c RESET VA_ARGS(__VA_ARGS__))

/**
 * Analyses a received RTCP packet.
 * Returns TRUE is relevant information has been found in the rtcp message, FALSE otherwise.
**/
bool_t ms_qos_analyser_process_rtcp(MSQosAnalyser *obj,mblk_t *msg){
	if (obj->desc->process_rtcp){
		return obj->desc->process_rtcp(obj,msg);
	}
	ms_error("Unimplemented process_rtcp() call.");
	return FALSE;
}

void ms_qos_analyser_suggest_action(MSQosAnalyser *obj, MSRateControlAction *action){
	if (obj->desc->suggest_action){
		obj->desc->suggest_action(obj,action);
	}
}

void ms_qos_analyser_update(MSQosAnalyser *obj){
	if (obj->desc->update){
		obj->desc->update(obj);
	}
}

bool_t ms_qos_analyser_has_improved(MSQosAnalyser *obj){
	if (obj->desc->has_improved){
		return obj->desc->has_improved(obj);
	}
	ms_error("Unimplemented has_improved() call.");
	return TRUE;
}

MSQosAnalyser *ms_qos_analyser_ref(MSQosAnalyser *obj){
	obj->refcnt++;
	return obj;
}

void ms_qos_analyser_unref(MSQosAnalyser *obj){
	obj->refcnt--;
	if (obj->refcnt<=0){
		if (obj->desc->uninit)
			obj->desc->uninit(obj);
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
/***************************** Simple QoS analyser ****************************/
/******************************************************************************/
static bool_t rt_prop_doubled(rtpstats_t *cur,rtpstats_t *prev){
	/*ms_message("AudioBitrateController: cur=%f, prev=%f",cur->rt_prop,prev->rt_prop);*/
	if (cur->rt_prop>=significant_delay && prev->rt_prop>0){
		if (cur->rt_prop>=(prev->rt_prop*2.0)){
			/*propagation doubled since last report */
			return TRUE;
		}
	}
	return FALSE;
}

static bool_t simple_rt_prop_increased(MSSimpleQosAnalyser *obj){
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];
	rtpstats_t *prev=&obj->stats[(STATS_HISTORY+obj->curindex-1) % STATS_HISTORY];

	if (rt_prop_doubled(cur,prev)){
		obj->rt_prop_doubled=TRUE;
		return TRUE;
	}
	return FALSE;
}

static bool_t simple_analyser_process_rtcp(MSQosAnalyser *objbase, mblk_t *rtcp){
	MSSimpleQosAnalyser *obj=(MSSimpleQosAnalyser*)objbase;
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

		cur->high_seq_recv=report_block_get_high_ext_seq(rb);
		cur->lost_percentage=100.0*(float)report_block_get_fraction_lost(rb)/256.0;
		cur->int_jitter=1000.0*(float)report_block_get_interarrival_jitter(rb)/(float)obj->clockrate;
		cur->rt_prop=rtp_session_get_round_trip_propagation(obj->session);

		ms_message("MSQosAnalyser: lost_percentage=%f, int_jitter=%f ms, rt_prop=%fsec",
			cur->lost_percentage,cur->int_jitter,cur->rt_prop);
	}
	return rb!=NULL;
}

static void simple_analyser_suggest_action(MSQosAnalyser *objbase, MSRateControlAction *action){
	MSSimpleQosAnalyser *obj=(MSSimpleQosAnalyser*)objbase;
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];

	/*big losses and big jitter */
	if (cur->lost_percentage>=unacceptable_loss_rate && cur->int_jitter>=big_jitter){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=MIN(cur->lost_percentage,50);
		ms_message("MSQosAnalyser: loss rate unacceptable and big jitter");
	}else if (simple_rt_prop_increased(obj)){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=20;
		ms_message("MSQosAnalyser: rt_prop doubled.");
	}else if (cur->lost_percentage>=unacceptable_loss_rate){
		/*big loss rate but no jitter, and no big rtp_prop: pure lossy network*/
		action->type=MSRateControlActionDecreasePacketRate;
		ms_message("MSQosAnalyser: loss rate unacceptable.");
	}else{
		action->type=MSRateControlActionDoNothing;
		ms_message("MSQosAnalyser: everything is fine.");
	}
}

static bool_t simple_analyser_has_improved(MSQosAnalyser *objbase){
	MSSimpleQosAnalyser *obj=(MSSimpleQosAnalyser*)objbase;
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];
	rtpstats_t *prev=&obj->stats[(STATS_HISTORY+obj->curindex-1) % STATS_HISTORY];

	if (prev->lost_percentage>=unacceptable_loss_rate){
		if (cur->lost_percentage<prev->lost_percentage){
			ms_message("MSQosAnalyser: lost percentage has improved");
			return TRUE;
		}else goto end;
	}
	if (obj->rt_prop_doubled && cur->rt_prop<prev->rt_prop){
		ms_message("MSQosAnalyser: rt prop decreased");
		obj->rt_prop_doubled=FALSE;
		return TRUE;
	}

	end:
	ms_message("MSQosAnalyser: no improvements.");
	return FALSE;
}

static MSQosAnalyserDesc simple_analyser_desc={
	simple_analyser_process_rtcp,
	simple_analyser_suggest_action,
	simple_analyser_has_improved
};

MSQosAnalyser * ms_simple_qos_analyser_new(RtpSession *session){
	MSSimpleQosAnalyser *obj=ms_new0(MSSimpleQosAnalyser,1);
	obj->session=session;
	obj->parent.desc=&simple_analyser_desc;
	obj->parent.type=Simple;
	return (MSQosAnalyser*)obj;
}




/******************************************************************************/
/***************************** Stateful QoS analyser ****************************/
/******************************************************************************/
const char *ms_qos_analyser_network_state_name(MSQosAnalyserNetworkState state){
	switch(state){
		case MSQosAnalyserNetworkFine:
			return "fine";
		case MSQosAnalyserNetworkUnstable:
			return "unstable";
		case MSQosAnalyserNetworkCongested:
			return "congested";
		case MSQosAnalyserNetworkLossy:
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

static int stateful_qos_analyser_get_total_emitted(const MSStatefulQosAnalyser *obj, const report_block_t *rb){
	double dup = obj->burst_ratio;
	int burst_within_start = MAX(obj->previous_ext_high_seq_num_rec, obj->start_seq_number);
	int burst_within_end = MIN(report_block_get_high_ext_seq(rb), obj->last_seq_number);
	int uniq_emitted=report_block_get_high_ext_seq(rb) - obj->previous_ext_high_seq_num_rec;

	return uniq_emitted + MAX(0,burst_within_end - burst_within_start) * dup;
}

static double stateful_qos_analyser_upload_bandwidth(MSStatefulQosAnalyser *obj){
	double up_bw=rtp_session_get_send_bandwidth(obj->session)/1000.0;

	if (obj->upload_bandwidth_count){
		obj->upload_bandwidth_latest=obj->upload_bandwidth_sum/obj->upload_bandwidth_count;
	}

	obj->upload_bandwidth_count=0;
	obj->upload_bandwidth_sum=0;

	P(GREEN "latest_up_bw=%f vs sum_up_bw=%f\n", up_bw, obj->upload_bandwidth_latest);
	return up_bw;
}

static bool_t stateful_analyser_process_rtcp(MSQosAnalyser *objbase, mblk_t *rtcp){
	MSStatefulQosAnalyser *obj=(MSStatefulQosAnalyser*)objbase;
	rtpstats_t *cur;
	const report_block_t *rb=NULL;
	if (rtcp_is_SR(rtcp)){
		rb=rtcp_SR_get_report_block(rtcp,0);
	}else if (rtcp_is_RR(rtcp)){
		rb=rtcp_RR_get_report_block(rtcp,0);
	}
	if (rb && report_block_get_ssrc(rb)==rtp_session_get_send_ssrc(obj->session)){
		double up_bw = stateful_qos_analyser_upload_bandwidth(obj);
		int total_emitted=stateful_qos_analyser_get_total_emitted(obj, rb);
		obj->curindex++;
		cur=&obj->stats[obj->curindex % STATS_HISTORY];

		if (obj->clockrate==0){
			PayloadType *pt=rtp_profile_get_payload(rtp_session_get_send_profile(obj->session),rtp_session_get_send_payload_type(obj->session));
			if (pt!=NULL) obj->clockrate=pt->clock_rate;
			else return FALSE;
		}

		cur->high_seq_recv=report_block_get_high_ext_seq(rb);
		cur->lost_percentage=100.0*(float)report_block_get_fraction_lost(rb)/256.0;
		cur->int_jitter=1000.0*(float)report_block_get_interarrival_jitter(rb)/(float)obj->clockrate;
		cur->rt_prop=rtp_session_get_round_trip_propagation(obj->session);
		ms_message("MSQosAnalyser: lost_percentage=%f, int_jitter=%f ms, rt_prop=%f sec",cur->lost_percentage,cur->int_jitter,cur->rt_prop);
		if (obj->curindex>2){
			double loss_rate = cur->lost_percentage/100.0;
			int cum_loss=report_block_get_cum_packet_loss(rb);
			int cum_loss_curr=cum_loss - obj->cum_loss_prev;
			int uniq_emitted=report_block_get_high_ext_seq(rb) - obj->previous_ext_high_seq_num_rec;

			if (obj->previous_ext_high_seq_num_rec > 0){
				printf("RECEIVE cumloss=%d uniq_emitted=%d total_emitted=%d\n", cum_loss_curr, uniq_emitted, total_emitted);
				loss_rate=(1. - (uniq_emitted - cum_loss_curr) * 1.f / total_emitted);
				printf("RECEIVE estimated loss rate=%f vs 'real'=%f\n", loss_rate, report_block_get_fraction_lost(rb)/2.56);
			}

			if (obj->curindex % 10 == 6){
				P(YELLOW "SKIPPED first MIN burst %d: %f %f\n", obj->curindex-1, up_bw, loss_rate);
			}else{
				obj->latest=ms_new0(rtcpstatspoint_t, 1);
				obj->latest->timestamp=ms_time(0);
				obj->latest->bandwidth=up_bw;
				obj->latest->loss_percent=loss_rate;
				obj->latest->rtt=cur->rt_prop;

				obj->rtcpstatspoint=ms_list_insert_sorted(obj->rtcpstatspoint, obj->latest, (MSCompareFunc)sort_points);

				P(YELLOW "one more %d: %f %f\n", obj->curindex-1, obj->latest->bandwidth, obj->latest->loss_percent);
			}

			if (ms_list_size(obj->rtcpstatspoint) > ESTIM_HISTORY){
				P(RED "Reached list maximum capacity (count=%d)", ms_list_size(obj->rtcpstatspoint));
				/*clean everything which occurred 60 sec or more ago*/
				time_t clear_time = ms_time(0) - 60;
				obj->rtcpstatspoint = ms_list_remove_custom(obj->rtcpstatspoint, (MSCompareFunc)earlier_than, &clear_time);
				P(RED "--> Cleaned list (count=%d)\n", ms_list_size(obj->rtcpstatspoint));
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

static void smooth_values(MSStatefulQosAnalyser *obj){
	//smooth values
	MSList *it = obj->rtcpstatspoint;
	double prev_loss;
	rtcpstatspoint_t *curr = (rtcpstatspoint_t *)it->data;
	prev_loss = curr->loss_percent;
	it = it->next;
	curr = (rtcpstatspoint_t *)it->data;
	/*float w = obj->rtcpstatspoint[i].bandwidth/obj->rtcpstatspoint[i+1].bandwidth;
	obj->rtcpstatspoint[i].loss_percent = (prev + obj->rtcpstatspoint[i+1].loss_percent*w)/(1+w);*/
	((rtcpstatspoint_t *)it->prev->data)->loss_percent = lerp(prev_loss, curr->loss_percent, .25);
	/*obj->rtcpstatspoint[i].loss_percent = MIN(prev, obj->rtcpstatspoint[i+1].loss_percent);*/
	/*obj->rtcpstatspoint[i].loss_percent = obj->rtcpstatspoint[i+1].loss_percent;*/
	/*float w1 = obj->rtcpstatspoint[i].bandwidth;
	float w2 = obj->rtcpstatspoint[i+1].bandwidth;
	obj->rtcpstatspoint[i].loss_percent = (w2*prev + w1*obj->rtcpstatspoint[i+1].loss_percent)/(w1+w2);*/
	while (it->next != NULL){
		curr = (rtcpstatspoint_t *)it->data;
		rtcpstatspoint_t *prev = ((rtcpstatspoint_t *)it->prev->data);
		rtcpstatspoint_t *next = ((rtcpstatspoint_t *)it->next->data);

		float v = (curr->bandwidth - prev->bandwidth) / (next->bandwidth - prev->bandwidth);
		float new_loss = lerp(prev_loss, next->loss_percent, v);
		prev_loss = curr->loss_percent;
		curr->loss_percent = (curr->loss_percent + new_loss) / 2.;
		it = it->next;
	}
	curr = (rtcpstatspoint_t *)it->data;
	/*w = obj->rtcpstatspoint[i-1].bandwidth/obj->rtcpstatspoint[i].bandwidth;
	obj->rtcpstatspoint[i].loss_percent = (obj->rtcpstatspoint[i].loss_percent + prev*w)/(1+w);*/
	curr->loss_percent = lerp(prev_loss, curr->loss_percent, .75);
	/*obj->rtcpstatspoint[i].loss_percent = MAX(prev, obj->rtcpstatspoint[i].loss_percent);*/
	/*obj->rtcpstatspoint[i].loss_percent = prev;*/
	/*w1 = obj->rtcpstatspoint[i-1].bandwidth;
	w2 = obj->rtcpstatspoint[i].bandwidth;
	obj->rtcpstatspoint[i].loss_percent = (w1*prev + w2*obj->rtcpstatspoint[i].loss_percent)/(w1+w2);*/
}

static float compute_available_bw(MSStatefulQosAnalyser *obj){
	MSList *it;
	double constant_network_loss = 0.;
	double mean_bw = 0.;
	MSList *current = obj->rtcpstatspoint;
	MSList *last = current;
	int size = ms_list_size(obj->rtcpstatspoint);
	if (current == NULL){
		P(RED "Not points available for computation.\n");
		return -1;
	}else if (size == 1){
		rtcpstatspoint_t *p = (rtcpstatspoint_t *)current->data;
		mean_bw = p->bandwidth * (1 - p->loss_percent);
		P(RED "One single point available for computation. Estimated BW is %f kbit/s\n", mean_bw);

		return mean_bw;
	}

	while (last->next) last = last->next;

	/*suppose that first point is a reliable estimation of the constant network loss rate*/
	if (size > 3){
		smooth_values(obj);
		constant_network_loss = ((rtcpstatspoint_t *)obj->rtcpstatspoint->next->data)->loss_percent;
	}else{
		constant_network_loss = ((rtcpstatspoint_t *)obj->rtcpstatspoint->data)->loss_percent;
	}


	P("\tconstant_network_loss=%f\n", constant_network_loss);
	for (it = obj->rtcpstatspoint; it != NULL; it=it->next){
		rtcpstatspoint_t * point = (rtcpstatspoint_t *)it->data;
		P(YELLOW "\t\tsorted values %d: %f %f\n",
			ms_list_position(obj->rtcpstatspoint, it), point->bandwidth, point->loss_percent);
	}

	while (current == obj->rtcpstatspoint || (current!=NULL && ((rtcpstatspoint_t*)current->data)->loss_percent<0.03+constant_network_loss)){
		P("\t%d is stable\n", ms_list_position(obj->rtcpstatspoint, current));

		for (it=last;it!=current;it=it->prev){
			if (((rtcpstatspoint_t *)it->data)->loss_percent <= 0.03 + ((rtcpstatspoint_t*)current->data)->loss_percent){
				P("\t%d is less than %d\n", ms_list_position(obj->rtcpstatspoint, it), ms_list_position(obj->rtcpstatspoint, current));
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

	P(RED "[0->%d] Last stable is %d(%f;%f)"
		, ms_list_position(obj->rtcpstatspoint, last)
		, ms_list_position(obj->rtcpstatspoint, (current ? current->prev : last))
		, ((rtcpstatspoint_t*) (current ? current->prev->data : last->data))->bandwidth
		, ((rtcpstatspoint_t*) (current ? current->prev->data : last->data))->loss_percent);
	if (current!=NULL){
		P(RED ", first unstable is %d(%f;%f)"
			, ms_list_position(obj->rtcpstatspoint, current)
			, ((rtcpstatspoint_t*) current->data)->bandwidth
			, ((rtcpstatspoint_t*) current->data)->loss_percent);
	}
	P(RED " --> estimated_available_bw=%f\n", mean_bw);

	obj->network_loss_rate = constant_network_loss;
	obj->congestion_bandwidth = mean_bw;
	obj->network_state =
		(current==NULL && constant_network_loss < .1) ?	MSQosAnalyserNetworkFine
		: (constant_network_loss > .1) ?				MSQosAnalyserNetworkLossy
		:												MSQosAnalyserNetworkCongested;

	return mean_bw;
}

static void stateful_analyser_suggest_action(MSQosAnalyser *objbase, MSRateControlAction *action){
	MSStatefulQosAnalyser *obj=(MSStatefulQosAnalyser*)objbase;

	float curbw = obj->latest ? obj->latest->bandwidth : 0.f;
	float bw = compute_available_bw(obj);

	/*rtp_session_set_duplication_ratio(obj->session, 0);*/
	/*try a burst every 50 seconds (10 RTCP packets)*/
	if (obj->curindex % 10 == 0){
		P(YELLOW "try burst!\n");
		/*bw *= 3;*/
		/*rtp_session_set_duplication_ratio(obj->session, 2);*/
		obj->burst_state = MSStatefulQosAnalyserBurstEnable;
	}
	/*test a min burst to avoid overestimation of available bandwidth*/
	else if (obj->curindex % 10 == 5 || obj->curindex % 10 == 6){
		P(YELLOW "try minimal burst!\n");
		bw *= .33;
	}
	/*not bandwidth estimation computed*/
	if (bw <= 0){
		action->type=MSRateControlActionDoNothing;
		action->value=0;
	}else if (bw > curbw){
		action->type=MSRateControlActionIncreaseQuality;
		action->value=MAX(0, 100.* (bw - curbw) / curbw);
	}else{
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=MAX(10,(100. - bw * 100. / curbw));
	}

	P(YELLOW "%s of value %d\n", ms_rate_control_action_type_name(action->type), action->value);
}

static bool_t stateful_analyser_has_improved(MSQosAnalyser *objbase){
	MSStatefulQosAnalyser *obj=(MSStatefulQosAnalyser*)objbase;
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];
	rtpstats_t *prev=&obj->stats[(STATS_HISTORY+obj->curindex-1) % STATS_HISTORY];

	if (prev->lost_percentage>=unacceptable_loss_rate){
		if (cur->lost_percentage<prev->lost_percentage){
			ms_message("MSQosAnalyser: lost percentage has improved");
			return TRUE;
		}else goto end;
	}
	if (obj->rt_prop_doubled && cur->rt_prop<prev->rt_prop){
		ms_message("MSQosAnalyser: rt prop decreased");
		obj->rt_prop_doubled=FALSE;
		return TRUE;
	}

	end:
	ms_message("MSQosAnalyser: no improvements.");
	return FALSE;
}

static void stateful_analyser_update(MSQosAnalyser *objbase){
	MSStatefulQosAnalyser *obj=(MSStatefulQosAnalyser*)objbase;
	static time_t last_measure;

	if (last_measure != ms_time(0)){
		obj->upload_bandwidth_count++;
		obj->upload_bandwidth_sum+=rtp_session_get_send_bandwidth(obj->session)/1000.0;
	}
	last_measure = ms_time(0);

	switch (obj->burst_state){
	case MSStatefulQosAnalyserBurstEnable:{
		obj->burst_state=MSStatefulQosAnalyserBurstInProgress;
		ortp_gettimeofday(&obj->start_time, NULL);
		rtp_session_set_duplication_ratio(obj->session, 2);
		obj->start_seq_number=obj->last_seq_number=obj->session->rtp.snd_seq;
	} case MSStatefulQosAnalyserBurstInProgress: {
		struct timeval now;
		double elapsed;

		ortp_gettimeofday(&now,NULL);
		elapsed=((now.tv_sec-obj->start_time.tv_sec)*1000.0) +  ((now.tv_usec-obj->start_time.tv_usec)/1000.0);

		obj->last_seq_number=obj->session->rtp.snd_seq;

		/*burst should last 1sec*/
		if (elapsed > obj->burst_duration_ms){
			obj->burst_state=MSStatefulQosAnalyserBurstDisable;
			rtp_session_set_duplication_ratio(obj->session, 0);
		}
	} case MSStatefulQosAnalyserBurstDisable: {
	}
	}
}

static void stateful_analyser_uninit(MSQosAnalyser *objbase){
	MSStatefulQosAnalyser *obj=(MSStatefulQosAnalyser*)objbase;
	ms_list_for_each(obj->rtcpstatspoint, ms_free);
}

static MSQosAnalyserDesc stateful_analyser_desc={
	stateful_analyser_process_rtcp,
	stateful_analyser_suggest_action,
	stateful_analyser_has_improved,
	stateful_analyser_update,
	stateful_analyser_uninit,
};

MSQosAnalyser * ms_stateful_qos_analyser_new(RtpSession *session){
	MSStatefulQosAnalyser *obj=ms_new0(MSStatefulQosAnalyser,1);
	obj->session=session;
	obj->parent.desc=&stateful_analyser_desc;
	obj->parent.type=Stateful;
	obj->burst_duration_ms=1000;
	obj->burst_ratio=2;
	return (MSQosAnalyser*)obj;
}


