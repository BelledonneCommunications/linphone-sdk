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

		obj->points[obj->curindex-1][0]=rtp_session_get_send_bandwidth(obj->session)/1000.0;
		obj->points[obj->curindex-1][1]=cur->lost_percentage/100.0;
		obj->points[obj->curindex-1][2]=cur->rt_prop;
		ms_message("MSQosAnalyser: lost_percentage=%f, int_jitter=%f ms, rt_prop=%f sec, send_bw=%f",cur->lost_percentage,cur->int_jitter,cur->rt_prop,obj->points[obj->curindex-1][0]);

		if (obj->curindex>2) printf("one more %d: %f %f\n", obj->curindex-1, obj->points[obj->curindex-1][0], obj->points[obj->curindex-1][1]);
	}
	return rb!=NULL;
}

static float compute_available_bw(MSStatefulQosAnalyser *obj){
	int i;
	double x_mean = 0.;
	double y_mean = 0.;
	double x_square_sum = 0.;
	double x_y_sum = 0.;
	int last = obj->curindex - 1;
	int f = last > 15 ? last - 13 : 2; //always skip the 2 first ones
	int n = (last - f + 1);
	double mean_bw = 0.;
	double mean_diff = 0.;
	int x_min_ind = f;
	int x_max_ind = f;
	int x_mean_inf_ind = f;
	int x_mean_sup_ind = f;
	bool_t lossy_network = FALSE;
	double diff, m, b;
	uint8_t previous_state = obj->network_state;
	uint8_t unstable_state = MSQosAnalyserNetworkUnstable;
	double rtt = obj->points[last][2];

	obj->network_state = MSQosAnalyserNetworkFine;

	if (n <= 1) {
		mean_bw = obj->points[0][0] * obj->points[0][1];
		printf("Estimated BW is %f kbit/s\n", mean_bw);

		return mean_bw;
	}

	if (obj->points[f][0] < obj->points[last][0]){
		x_mean_inf_ind=f;
		x_mean_sup_ind=last;
	}else{
		x_mean_inf_ind=last;
		x_mean_sup_ind=f;
	}
	for (i = f; i <= last; i++) {
		double x = obj->points[i][0];
		double y = obj->points[i][1];

		if (x < obj->points[x_min_ind][0]) x_min_ind = i;
		if (x > obj->points[x_max_ind][0]) x_max_ind = i;

		x_mean += x;
		y_mean += y;
		x_y_sum += x * y;
		x_square_sum  += x * x;

		mean_bw += x * (1 - y);
	}
	x_mean /= n;
	y_mean /= n;
	mean_bw /= n;
	printf("\tEstimated BW by avg is %f kbit/s\n", mean_bw);

	printf("sum=%f xmin=%d xmax=%d\n", x_mean, x_min_ind, x_max_ind);
	diff = (obj->points[x_max_ind][0] - obj->points[x_min_ind][0]) / x_mean;

	if (n > 2 && diff > 0.05) unstable_state = MSQosAnalyserNetworkLossy;

	m = (x_y_sum - x_mean * y_mean * n) /
		(x_square_sum - x_mean * x_mean * n);

	b = (y_mean - m * x_mean);

	for (i = f; i <= last; i++) {
		double x = obj->points[i][0];
		double y = obj->points[i][1];

		if (y > y_mean && x < obj->points[x_mean_sup_ind][0] && x > obj->points[x_mean_inf_ind][0]) x_mean_sup_ind = i;
		else if (y < y_mean && x > obj->points[x_mean_inf_ind][0] && x < obj->points[x_mean_sup_ind][0]) x_mean_inf_ind = i;

		mean_diff += fabs(m * x + b - y);
	}
	mean_diff /= n;
	lossy_network |= (y_mean > 0.1);

	printf("x_mean_inf=%d x_mean_sup=%d\n",x_mean_inf_ind,x_mean_sup_ind);
	/*to compute estimated BW, we need a minimum x-axis interval size*/
	if (diff > 0.05) {
		double avail_bw = (fabs(m) > 0.0001f) ? -b/m : mean_bw;
		printf("\tEstimated BW by interpolation is %f kbit/s:\ty=%f x + %f\n", avail_bw, m, b);

		if (b > 0.1){
			lossy_network = TRUE;
		/*}else if (m > 0.05){*/
		}else if ((obj->points[x_max_ind][1] - obj->points[x_min_ind][1]) / y_mean > 0.1){
			lossy_network = FALSE;

			/*if we are below the limitation BW, consider network is now fine*/
			if (obj->points[last][0] < .95*mean_bw){
				obj->network_state = MSQosAnalyserNetworkFine;
			}else{
				obj->network_state = MSQosAnalyserNetworkCongested;
			}
		}
	}


	if (obj->network_state == MSQosAnalyserNetworkFine){
		lossy_network |= (y_mean > .1 && obj->points[last][1] > y_mean / 2.);

		if (lossy_network) {
			/*since congestion may loss a high number of packets, stay in congested network while
			this is not a bit more stable*/
			if (previous_state == MSQosAnalyserNetworkCongested) {
				obj->network_state = MSQosAnalyserNetworkCongested;

				obj->network_state = unstable_state; // !!!
			} else {
				obj->network_state = unstable_state;
			}
		/*another hint for a bad network: packets drop mean difference is high*/
		} else if (mean_diff > .1) {
			obj->network_state = (rtt > .5) ? MSQosAnalyserNetworkCongested : unstable_state;
		}
	}

	if (obj->network_state == MSQosAnalyserNetworkLossy){
		if (obj->points[x_min_ind][0] / mean_bw > 1.){
			printf("Network was suggested lossy, but since we did not try its lower bound capability, "
				"we will consider this is congestion for yet\n");
			obj->network_state = MSQosAnalyserNetworkCongested;
		}
	}

	/*printf("mean_diff=%f y_mean=%f\n", mean_diff, y_mean);*/
	if (y_mean > 0.05){
		printf("BW should be %f\n", mean_bw);
	}
	if (diff > 0.05 && b < -0.05){
		if (obj->network_state != MSQosAnalyserNetworkCongested){
			printf("Even if network state is now %s, the congestion limit might be at %f\n",
				ms_qos_analyser_network_state_name(obj->network_state), mean_bw);
		}
	}
	printf("%f\n", y_mean);
	printf("\t\tI think it is a %s network\n", ms_qos_analyser_network_state_name(obj->network_state));

	if (obj->network_state == MSQosAnalyserNetworkLossy){
		//hack
		if (obj->points[last][0]>x_mean && obj->points[last][1]>1.5*y_mean){
			printf("lossy network and congestion probably too!\n");
			return (1 - (obj->points[x_max_ind][1] - y_mean)) * obj->points[x_max_ind][0];
		}
		printf("Average error %f %%\n", y_mean);
		return obj->points[last][0] * 2;
	}
	return mean_bw;
}

static void stateful_analyser_suggest_action(MSQosAnalyser *objbase, MSRateControlAction *action){
	MSStatefulQosAnalyser *obj=(MSStatefulQosAnalyser*)objbase;
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];

	float bw = /*0; if (FALSE)*/ compute_available_bw(obj);
	float curbw = obj->points[obj->curindex-1][0];
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


