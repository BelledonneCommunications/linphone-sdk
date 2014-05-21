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

#define RED "[1m[31m"
#define YELLOW "[1m[33m"
#define GREEN "[1m[32m"
#define RESET "[0m"
#define P(c, ...) printf(GREEN c RESET,  __VA_ARGS__)
#define P2(c) printf(GREEN c RESET)

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
	return;
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

static bool_t stateful_rt_prop_increased(MSStatefulQosAnalyser *obj){
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];
	rtpstats_t *prev=&obj->stats[(STATS_HISTORY+obj->curindex-1) % STATS_HISTORY];

	if (rt_prop_doubled(cur,prev)){
		obj->rt_prop_doubled=TRUE;
		return TRUE;
	}
	return FALSE;
}

static int sort_points(const rtcpstatspoint_t *p1, const rtcpstatspoint_t *p2){
	return p1->bandwidth > p2->bandwidth;
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
			if (obj->curindex % 10 == 6){
				P(YELLOW "SKIPPED first MIN burst %d: %f %f\n", obj->curindex-1, rtp_session_get_send_bandwidth(obj->session)/1000.0, cur->lost_percentage/100.0);
			}else{
				obj->latest=ms_new0(rtcpstatspoint_t, 1);
				obj->latest->bandwidth=rtp_session_get_send_bandwidth(obj->session)/1000.0;
				P(RED "%f vs %f\n", (obj->session->rtp.stats.sent - obj->last_sent_count)*0.01/(time(0) - obj->last_timestamp),
					rtp_session_get_send_bandwidth(obj->session)/1000.0);
				/*obj->latest->bandwidth=(obj->session->rtp.stats.sent - obj->last_sent_count)*.01/(time(0) - obj->last_timestamp);*/
				obj->latest->loss_percent=cur->lost_percentage/100.0;
				obj->latest->rtt=cur->rt_prop;
				obj->last_timestamp = time(0);
				obj->last_sent_count = obj->session->rtp.stats.sent;
				obj->rtcpstatspoint = ms_list_insert_sorted(obj->rtcpstatspoint, obj->latest, (MSCompareFunc)sort_points);

				P(YELLOW "one more %d: %f %f\n", obj->curindex-1, obj->latest->bandwidth, obj->latest->loss_percent);
			}
		}
	}
	return rb!=NULL;
}

// static float compute_network_state(MSStatefulQosAnalyser *obj){
// 	int i;
// 	double x_mean = 0.;
// 	double y_mean = 0.;
// 	double x_square_sum = 0.;
// 	double x_y_sum = 0.;
// 	int last = obj->curindex - 1;
// 	int f = 2;//last > 15 ? last - 13 : 2; //always skip the 2 first ones
// 	int n = (last - f + 1);
// 	double mean_bw = 0.;
// 	double mean_diff = 0.;
// 	int x_min_ind = f;
// 	int x_max_ind = f;
// 	bool_t lossy_network = FALSE;
// 	double diff, m, b;
// 	uint8_t previous_state = obj->network_state;
// 	uint8_t unstable_state = MSQosAnalyserNetworkUnstable;
// 	double rtt = ((rtcpstatspoint_t*)obj->latest->data)->rtt;


// 	obj->network_state = MSQosAnalyserNetworkFine;

// 	for (i = f; i <= last; i++){
// 		double x = obj->rtcpstatspoint[i].bandwidth;
// 		double y = obj->rtcpstatspoint[i].loss_percent;

// 		if (x < obj->rtcpstatspoint[x_min_ind].bandwidth) x_min_ind = i;
// 		if (x > obj->rtcpstatspoint[x_max_ind].bandwidth) x_max_ind = i;

// 		x_mean += x;
// 		y_mean += y;
// 		x_y_sum += x * y;
// 		x_square_sum  += x * x;

// 		mean_bw += x * (1 - y);
// 	}
// 	x_mean /= n;
// 	y_mean /= n;
// 	mean_bw /= n;
// 	P("\tEstimated BW by avg is %f kbit/s\n", mean_bw);

// 	diff = (obj->rtcpstatspoint[x_max_ind].bandwidth - obj->rtcpstatspoint[x_min_ind].bandwidth) / x_mean;

// 	if (n > 2 && diff > 0.05) unstable_state = MSQosAnalyserNetworkLossy;

// 	m = (x_y_sum - x_mean * y_mean * n) /
// 		(x_square_sum - x_mean * x_mean * n);

// 	b = (y_mean - m * x_mean);

// 	for (i = f; i <= last; i++){
// 		double x = obj->rtcpstatspoint[i].bandwidth;
// 		double y = obj->rtcpstatspoint[i].loss_percent;

// 		mean_diff += fabs(m * x + b - y);
// 	}
// 	mean_diff /= n;
// 	lossy_network |= (y_mean > 0.1);

// 	/*to compute estimated BW, we need a minimum x-axis interval size*/
// 	if (diff > 0.05){
// 		double avail_bw = (fabs(m) > 0.0001f) ? -b/m : mean_bw;
// 		P("\tEstimated BW by interpolation is %f kbit/s:\ty=%f x + %f\n", avail_bw, m, b);

// 		if (b > 0.1){
// 			lossy_network = TRUE;
// 		/*}else if (m > 0.05){*/
// 		}else if ((obj->rtcpstatspoint[x_max_ind].loss_percent - obj->rtcpstatspoint[x_min_ind].loss_percent) / y_mean > 0.1){
// 			lossy_network = FALSE;

// 			if we are below the limitation BW, consider network is now fine
// 			if (obj->rtcpstatspoint[last].bandwidth < .95*mean_bw){
// 				obj->network_state = MSQosAnalyserNetworkFine;
// 			}else{
// 				obj->network_state = MSQosAnalyserNetworkCongested;
// 			}
// 		}
// 	}


// 	if (obj->network_state == MSQosAnalyserNetworkFine){
// 		lossy_network |= (y_mean > .1 && obj->rtcpstatspoint[last].loss_percent > y_mean / 2.);

// 		if (lossy_network){
// 			/*since congestion may loss a high number of packets, stay in congested network while
// 			this is not a bit more stable*/
// 			if (previous_state == MSQosAnalyserNetworkCongested){
// 				obj->network_state = MSQosAnalyserNetworkCongested;

// 				obj->network_state = unstable_state; // !!!
// 			} else{
// 				obj->network_state = unstable_state;
// 			}
// 		/*another hint for a bad network: packets drop mean difference is high*/
// 		} else if (mean_diff > .1){
// 			obj->network_state = (rtt > .5) ? MSQosAnalyserNetworkCongested : unstable_state;
// 		}
// 	}

// 	if (obj->network_state == MSQosAnalyserNetworkLossy){
// 		if (obj->rtcpstatspoint[x_min_ind].bandwidth / mean_bw > 1.){
// 			P2(RED "Network was suggested lossy, but since we did not try its lower bound capability, "
// 				"we will consider this is congestion for yet\n");
// 			obj->network_state = MSQosAnalyserNetworkCongested;
// 		}
// 	}

// 	if (diff > 0.05 && b < -0.05){
// 		if (obj->network_state != MSQosAnalyserNetworkCongested){
// 			P("Even if network state is now %s, the congestion limit might be at %f\n",
// 				ms_qos_analyser_network_state_name(obj->network_state), mean_bw);
// 		}
// 	}
// 	P("I think it is a %s network\n", ms_qos_analyser_network_state_name(obj->network_state));

// 	if (obj->network_state == MSQosAnalyserNetworkLossy){
// 		//hack
// 		if (obj->rtcpstatspoint[last].bandwidth>x_mean && obj->rtcpstatspoint[last].loss_percent>1.5*y_mean){
// 			P2(RED "lossy network and congestion probably too!\n");
// 			mean_bw = (1 - (obj->rtcpstatspoint[x_max_ind].loss_percent - y_mean)) * obj->rtcpstatspoint[x_max_ind].bandwidth;
// 		}
// 		mean_bw = obj->rtcpstatspoint[last].bandwidth * 2;
// 	}
// 	return mean_bw;
// }

/*static void sort_array(rtcpstatspoint_t *pts,int start, int end){
	int i,j;

	for (i = start; i < end; ++i){
		for (j = i+1; j <= end; j++){
			if (pts[i].bandwidth > pts[j].bandwidth){
				rtcpstatspoint_t tmp;
				memcpy(&tmp, &pts[i], sizeof(rtcpstatspoint_t));
				memcpy(&pts[i], &pts[j], sizeof(rtcpstatspoint_t));
				memcpy(&pts[j], &tmp, sizeof(rtcpstatspoint_t));
			}
		}
	}
}*/

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
	double y_mean = 0.;
	double mean_bw = 0.;
	MSList *current = obj->rtcpstatspoint;
	MSList *last = current;
	int size = ms_list_size(obj->rtcpstatspoint);
	if (current == NULL){
		P2(RED "Not points available for computation.\n");
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
		y_mean = ((rtcpstatspoint_t *)obj->rtcpstatspoint->next->data)->loss_percent;
	}else{
		y_mean = ((rtcpstatspoint_t *)obj->rtcpstatspoint->data)->loss_percent;
	}


	P("\tavg_lost_rate=%f\n", y_mean);
	for (it = obj->rtcpstatspoint; it != NULL; it=it->next){
		rtcpstatspoint_t * point = (rtcpstatspoint_t *)it->data;
		P(YELLOW "\t\t\tsorted values %d: %f %f\n",
			ms_list_position(obj->rtcpstatspoint, it), point->bandwidth, point->loss_percent);
	}

	while (current == obj->rtcpstatspoint || (current!=NULL && ((rtcpstatspoint_t*)current->data)->loss_percent<0.03+y_mean)){
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
	}else{
		/*there is some congestion*/
		mean_bw = .5*(((rtcpstatspoint_t*)current->prev->data)->bandwidth+((rtcpstatspoint_t*)current->data)->bandwidth);
	}
	rtcpstatspoint_t * prev = (rtcpstatspoint_t*) (current ? current->prev->data : last->data);
	P(RED "[0->%d] Last stable is %d(%f;%f)"
		, ms_list_position(obj->rtcpstatspoint, last)
		, ms_list_position(obj->rtcpstatspoint, (current ? current->prev : last))
		, prev->bandwidth
		, prev->loss_percent);

	if (current!=NULL)
		P(RED ", first unstable is %d(%f;%f)"
			, ms_list_position(obj->rtcpstatspoint, current)
			, ((rtcpstatspoint_t*) current->data)->bandwidth
			, ((rtcpstatspoint_t*) current->data)->loss_percent);
	P(RED " --> estimated_available_bw=%f\n", mean_bw);

	obj->network_loss_rate = y_mean;
	obj->congestion_bandwidth = mean_bw;

	return mean_bw;
}

static void stateful_analyser_suggest_action(MSQosAnalyser *objbase, MSRateControlAction *action){
	MSStatefulQosAnalyser *obj=(MSStatefulQosAnalyser*)objbase;
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];

	float curbw = obj->latest ? obj->latest->bandwidth : 0.f;
	float bw = /*0; if (FALSE)*/ compute_available_bw(obj);


	/*try a burst every 50 seconds (10 RTCP packets)*/
	if (obj->curindex % 10 < 2){
		P2(YELLOW "try burst!\n");
		bw *= 3;
	}
	/*test only - test a min burst to avoid overestimation of available bandwidth*/
	else if (obj->curindex % 10 == 5 || obj->curindex % 10 == 6){
		P2(YELLOW "try minimal burst!\n");
		bw *= .33;
	}

	/*not bandwidth estimation computed*/
	if (bw <= 0){
		action->type=MSRateControlActionDoNothing;
	}else if (bw > curbw){
		action->type=MSRateControlActionIncreaseQuality;
		action->value=MAX(0, 100.* (bw - curbw) / curbw);
	}else{
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=MAX(10,(100. - bw * 100. / curbw));
	}

	P(YELLOW "%s of value %d\n", ms_rate_control_action_type_name(action->type), action->value);
	return;

	if (obj->network_state == MSQosAnalyserNetworkCongested){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=MAX(10,3*(100. - bw * 100. / curbw));
		ms_message("MSQosAnalyser: congested network - decrease %%%d", action->value);
	} else if (obj->network_state == MSQosAnalyserNetworkLossy){
	/*if (curbw > bw){
			action->type=MSRateControlActionDecreaseBitrate;
			action->value=MAX(0,3*(100. - bw * 100. / curbw));
			ms_message("MSQosAnalyser: lossy network - decrease quality %%%d", action->value);
		}else{*/
			action->type=MSRateControlActionIncreaseQuality;
			action->value=MAX(0,curbw * 100. / bw);
			/*action->value=50;*/
			ms_message("MSQosAnalyser: lossy network - increase quality %%%d", action->value);
		/*}*/
	}

	/*big losses and big jitter */
	if (cur->lost_percentage>=unacceptable_loss_rate){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=cur->lost_percentage ;//MIN(cur->lost_percentage,50);
		ms_message("MSQosAnalyser: loss rate unacceptable and big jitter");
	}else if (stateful_rt_prop_increased(obj)){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=20;
		ms_message("MSQosAnalyser: rt_prop doubled.");
	}else{
		action->type=MSRateControlActionDoNothing;
		ms_message("MSQosAnalyser: everything is fine.");
	}
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

static MSQosAnalyserDesc stateful_analyser_desc={
	stateful_analyser_process_rtcp,
	stateful_analyser_suggest_action,
	stateful_analyser_has_improved
};

MSQosAnalyser * ms_stateful_qos_analyser_new(RtpSession *session){
	MSStatefulQosAnalyser *obj=ms_new0(MSStatefulQosAnalyser,1);
	obj->session=session;
	obj->parent.desc=&stateful_analyser_desc;
	obj->parent.type=Stateful;
	return (MSQosAnalyser*)obj;
}


