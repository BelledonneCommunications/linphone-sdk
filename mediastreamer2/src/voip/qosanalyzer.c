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

#define STATS_HISTORY 3

static const float unacceptable_loss_rate=10;
static const float significant_delay=0.2; /*seconds*/


typedef struct rtpstats{
	uint64_t high_seq_recv; /*highest sequence number received*/
	float lost_percentage; /*percentage of lost packet since last report*/
	float int_jitter; /*interrarrival jitter */
	float rt_prop; /*round trip propagation*/
	float send_bw; /*bandwidth consummation*/
}rtpstats_t;


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


typedef struct _MSSimpleQosAnalyser{
	MSQosAnalyser parent;
	RtpSession *session;
	int clockrate;
	rtpstats_t stats[STATS_HISTORY];
	int curindex;
	bool_t rt_prop_doubled;
	bool_t pad[3];

	double points[150][3];
	uint8_t network_state;
}MSSimpleQosAnalyser;



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

static bool_t rt_prop_increased(MSSimpleQosAnalyser *obj){
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

		obj->points[obj->curindex-1][0]=rtp_session_get_send_bandwidth(obj->session)/1000.0;
		obj->points[obj->curindex-1][1]=cur->lost_percentage;
		obj->points[obj->curindex-1][2]=cur->rt_prop;
		ms_message("MSQosAnalyser: lost_percentage=%f, int_jitter=%f ms, rt_prop=%f sec, send_bw=%f",cur->lost_percentage,cur->int_jitter,cur->rt_prop,obj->points[obj->curindex][0]);

		if (obj->curindex>2) printf("one more %d: %f %f\n", obj->curindex-1, obj->points[obj->curindex-1][0], obj->points[obj->curindex-1][1]);
	}
	return rb!=NULL;
}

static void compute_available_bw(MSSimpleQosAnalyser *obj){
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
	bool_t lossy_network = FALSE;
	double diff, m, b;
	uint8_t previous_state = obj->network_state;

	obj->network_state = MS_QOS_ANALYSER_NETWORK_FINE;

	if (n <= 1) {
		printf("Estimated BW is %f kbit/s\n", obj->points[0][0] * obj->points[0][1]);

		return;
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

	m = (x_y_sum - x_mean * y_mean * n) /
		(x_square_sum - x_mean * x_mean * n);

	b = (y_mean - m * x_mean);

	for (i = f; i <= last; i++) {
		double x = obj->points[i][0];
		double y = obj->points[i][1];

		mean_diff += fabs(m * x + b - y);
	}
	mean_diff /= n;

	/*to compute estimated BW, we need a minimum x-axis interval size*/
	if (diff > 0.05) {
		double avail_bw = (fabs(m) > 0.0001f) ? -b/m : mean_bw;
		printf("\tfor line is %f kbit/s:\ty=%f x + %f\n", avail_bw, m, b);

		lossy_network |= (m < .03f && b > 0.05);

		if (!lossy_network && (m>.03f||b>0.05)){
			obj->network_state = MS_QOS_ANALYSER_NETWORK_CONGESTED;
		}
	} else {
		lossy_network |= (y_mean > 0.05);
	}

	if (lossy_network) {
		/*since congestion may loss a high count of packets, stay in congested network while
		this is not a bit more stable*/
		if (previous_state == MS_QOS_ANALYSER_NETWORK_CONGESTED) {
			obj->network_state = MS_QOS_ANALYSER_NETWORK_CONGESTED;
		} else {
			obj->network_state = MS_QOS_ANALYSER_NETWORK_UNSTABLE;
		}
	/*another hint for a bad network: packets drop mean difference is high*/
	} else if (mean_diff > .1) {
		double rtt = obj->points[last][2];
		obj->network_state = (rtt > .5) ? MS_QOS_ANALYSER_NETWORK_CONGESTED : MS_QOS_ANALYSER_NETWORK_UNSTABLE;
	}

	if (obj->network_state == MS_QOS_ANALYSER_NETWORK_CONGESTED)
		printf("\t\tI think it is a %s network\n", "C-O-N-G-E-S-T-E-D");
	else if (obj->network_state == MS_QOS_ANALYSER_NETWORK_UNSTABLE)
		printf("\t\tI think it is a %s network\n", "UNSTABLE");
	else
		printf("\t\tI think it is a %s network\n", "stable");

}

static void simple_analyser_suggest_action(MSQosAnalyser *objbase, MSRateControlAction *action){
	MSSimpleQosAnalyser *obj=(MSSimpleQosAnalyser*)objbase;
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];

	compute_available_bw(obj);

	/*big losses and big jitter */
	if (cur->lost_percentage>=unacceptable_loss_rate){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=MIN(cur->lost_percentage,50);
		ms_message("MSQosAnalyser: loss rate unacceptable");
	}else if (rt_prop_increased(obj)){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=20;
		ms_message("MSQosAnalyser: rt_prop doubled.");
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

uint32_t ms_qos_analyser_get_network_state(const MSQosAnalyser *objbase){
	MSSimpleQosAnalyser *obj=(MSSimpleQosAnalyser*)objbase;
	return obj->network_state;
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
	return (MSQosAnalyser*)obj;
}

