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
#include "qosanalyzer.h"

#include <math.h>

#define LOSS_RATE_MIN_INTERVAL 60
#define LOSS_RATE_MIN_TIME 3000

/**
 * Analyses a received RTCP packet.
 * Returns TRUE is relevant information has been found in the rtcp message, FALSE otherwise.
**/
bool_t ms_qos_analyzer_process_rtcp(MSQosAnalyzer *obj,mblk_t *msg){
	if (obj->desc->process_rtcp){
		return obj->desc->process_rtcp(obj,msg);
	}
	ms_error("MSQosAnalyzer: Unimplemented process_rtcp() call.");
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
	ms_error("MSQosAnalyzer: Unimplemented has_improved() call.");
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

const char* ms_qos_analyzer_algorithm_to_string(MSQosAnalyzerAlgorithm alg) {
	switch (alg){
		case MSQosAnalyzerAlgorithmSimple: return "Simple";
		case MSQosAnalyzerAlgorithmStateful: return "Stateful";
		default: return NULL;
	}
}
MSQosAnalyzerAlgorithm ms_qos_analyzer_algorithm_from_string(const char* alg) {
	if (alg == NULL || strcasecmp(alg, "Simple")==0)
		return MSQosAnalyzerAlgorithmSimple;
	else if (strcasecmp(alg, "Stateful")==0)
		return MSQosAnalyzerAlgorithmStateful;

	ms_error("MSQosAnalyzer: Invalid QoS analyzer: %s", alg);
	return MSQosAnalyzerAlgorithmSimple;
}

const char* ms_qos_analyzer_get_name(MSQosAnalyzer *obj){
	return ms_qos_analyzer_algorithm_to_string(obj->type);
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
		if (obj->lre) ortp_loss_rate_estimator_destroy(obj->lre);

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
	bool_t got_stats=FALSE;

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
		if (ortp_loss_rate_estimator_process_report_block(objbase->lre,obj->session,rb)){
			cur->lost_percentage=ortp_loss_rate_estimator_get_value(objbase->lre);
			cur->int_jitter=1000.0f*(float)report_block_get_interarrival_jitter(rb)/(float)obj->clockrate;
			cur->rt_prop=rtp_session_get_round_trip_propagation(obj->session);

			ms_message("MSSimpleQosAnalyzer: lost_percentage=%f, int_jitter=%f ms, rt_prop=%f sec",
				cur->lost_percentage,cur->int_jitter,cur->rt_prop);
			got_stats=TRUE;
		}
	}
	return got_stats;
}

static void simple_analyzer_suggest_action(MSQosAnalyzer *objbase, MSRateControlAction *action){
	MSSimpleQosAnalyzer *obj=(MSSimpleQosAnalyzer*)objbase;
	rtpstats_t *cur=&obj->stats[obj->curindex % STATS_HISTORY];

	/*big losses and big jitter */
	if (cur->lost_percentage>=unacceptable_loss_rate && cur->int_jitter>=big_jitter){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=(int)MIN(cur->lost_percentage,50);
		ms_message("MSSimpleQosAnalyzer: loss rate unacceptable and big jitter");
	}else if (simple_rt_prop_increased(obj)){
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=20;
		ms_message("MSSimpleQosAnalyzer: rt_prop doubled.");
	}else if (cur->lost_percentage>=unacceptable_loss_rate){
		/*big loss rate but no jitter, and no big rtp_prop: pure lossy network*/
		action->type=MSRateControlActionDecreaseBitrate;
		action->value=(int)MIN(cur->lost_percentage,50);
		ms_message("MSSimpleQosAnalyzer: loss rate unacceptable.");
	}else{
		action->type=MSRateControlActionDoNothing;
		ms_message("MSSimpleQosAnalyzer: everything is fine.");
	}

	if (objbase->on_action_suggested!=NULL){
		int i;
		char *data[4];
		int datac = sizeof(data) / sizeof(data[0]);
		data[0]=ms_strdup("%loss rt_prop_increased int_jitter_ms rt_prop_ms");
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
			ms_message("MSSimpleQosAnalyzer: lost percentage has improved");
			return TRUE;
		}else goto end;
	}
	if (obj->rt_prop_doubled && cur->rt_prop<prev->rt_prop){
		ms_message("MSSimpleQosAnalyzer: rt prop decreased");
		obj->rt_prop_doubled=FALSE;
		return TRUE;
	}

	end:
	ms_message("MSSimpleQosAnalyzer: no improvements.");
	return FALSE;
}

static MSQosAnalyzerDesc simple_analyzer_desc={
	simple_analyzer_process_rtcp,
	simple_analyzer_suggest_action,
	simple_analyzer_has_improved,
	NULL,
	NULL
};

MSQosAnalyzer * ms_simple_qos_analyzer_new(RtpSession *session){
	MSSimpleQosAnalyzer *obj=ms_new0(MSSimpleQosAnalyzer,1);
	obj->session=session;
	obj->parent.desc=&simple_analyzer_desc;
	obj->parent.type=MSQosAnalyzerAlgorithmSimple;
	obj->parent.lre=ortp_loss_rate_estimator_new(LOSS_RATE_MIN_INTERVAL, LOSS_RATE_MIN_TIME, session);
	return (MSQosAnalyzer*)obj;
}




/******************************************************************************/
/***************************** Stateful QoS analyzer **************************/
/******************************************************************************/
static int earlier_than(rtcpstatspoint_t *p, const time_t * now){
	if (p->timestamp < *now){
		ms_free(p);
		return FALSE;
	}
	return TRUE;
}
static int sort_by_bandwidth(const rtcpstatspoint_t *p1, const rtcpstatspoint_t *p2){
	return p1->bandwidth > p2->bandwidth;
}

static float stateful_qos_analyzer_upload_bandwidth(MSStatefulQosAnalyzer *obj, uint32_t seq_num){
	int latest_bw;
	float bw_per_seqnum=0.f;
	float bw_per_avg=0.f;

	/*First method to compute bandwidth*/
	if (obj->upload_bandwidth_count){
		bw_per_avg=(float)(obj->upload_bandwidth_sum/obj->upload_bandwidth_count);
	}
	obj->upload_bandwidth_count=0;
	obj->upload_bandwidth_sum=0;

	for (latest_bw=0;latest_bw<BW_HISTORY;++latest_bw){
		ms_debug("MSStatefulQosAnalyzer[%p]:\t%u\t-->\t%f", obj,
			obj->upload_bandwidth[latest_bw].seq_number,
			obj->upload_bandwidth[latest_bw].up_bandwidth);
	}

	if (obj->upload_bandwidth[(obj->upload_bandwidth_cur+1)%BW_HISTORY].seq_number>seq_num){
		ms_warning("MSStatefulQosAnalyzer[%p]: saved to much points - seq_number lower "
			"than oldest measure! Increase BW_HISTORY or reduce ptime!", obj);
	}else{
		int count = 0;
		latest_bw=obj->upload_bandwidth_cur;
		/*Get the average of all measures with seq number lower than the one from the report*/
		for (latest_bw=0; latest_bw<BW_HISTORY; ++latest_bw){
			if (obj->upload_bandwidth[latest_bw].seq_number>0
				&& obj->upload_bandwidth[latest_bw].seq_number<seq_num){
				count++;
				bw_per_seqnum+=obj->upload_bandwidth[latest_bw].up_bandwidth;
			}
		}
		// invalid, no measures available
		if (count==0){
			ms_error("MSStatefulQosAnalyzer[%p]: no measures available to compute bandwidth for ext_seq=%u", obj, seq_num);
			bw_per_seqnum = rtp_session_get_send_bandwidth(obj->session)/1000.0f;
		}else{
			bw_per_seqnum /= count;//((BW_HISTORY + obj->upload_bandwidth_cur - latest_bw) % BW_HISTORY);
			ms_debug("MSStatefulQosAnalyzer[%p]: found average bandwidth for seq_num=%u", obj, seq_num);
		}
	}

	ms_message("MSStatefulQosAnalyzer[%p]: bw_curent=%f vs bw_per_avg=%f vs bw_per_seqnum=%f"
				, obj
				, rtp_session_get_send_bandwidth(obj->session)/1000.0
				, bw_per_avg
				, bw_per_seqnum);

	obj->upload_bandwidth_latest = bw_per_seqnum;
	return (float)obj->upload_bandwidth_latest;
}

static bool_t stateful_analyzer_process_rtcp(MSQosAnalyzer *objbase, mblk_t *rtcp){
	MSStatefulQosAnalyzer *obj=(MSStatefulQosAnalyzer*)objbase;
	const report_block_t *rb=NULL;
	if (rtcp_is_SR(rtcp)){
		rb=rtcp_SR_get_report_block(rtcp,0);
	}else if (rtcp_is_RR(rtcp)){
		rb=rtcp_RR_get_report_block(rtcp,0);
	}


	if (rb && report_block_get_ssrc(rb)==rtp_session_get_send_ssrc(obj->session)){
		if (ortp_loss_rate_estimator_process_report_block(objbase->lre,obj->session,rb)){
			int i;
			float loss_rate = ortp_loss_rate_estimator_get_value(objbase->lre);
			float up_bw = stateful_qos_analyzer_upload_bandwidth(obj,report_block_get_high_ext_seq(rb));
			obj->curindex++;

			/*flush bandwidth estimation measures for seq number lower than remote report block received*/
			for (i=0;i<BW_HISTORY;i++){
				if (obj->upload_bandwidth[i].seq_number<report_block_get_high_ext_seq(rb)){
					obj->upload_bandwidth[i].seq_number=0;
					obj->upload_bandwidth[i].up_bandwidth=0.f;
				}
			}

			/* Always skip the first report, since values might be erroneous due
			to initialization of multiples objects (encoder/decoder/stats computing..)
			Instead assume loss rate is a good estimation of network capacity */
			if (obj->curindex==1)  {
				obj->network_loss_rate=loss_rate;
				return TRUE;
			}

			obj->latest=ms_new0(rtcpstatspoint_t, 1);
			obj->latest->timestamp=ms_time(0);
			obj->latest->bandwidth=up_bw;
			obj->latest->loss_percent=loss_rate;
			obj->latest->rtt=rtp_session_get_round_trip_propagation(obj->session);

			obj->rtcpstatspoint=bctbx_list_insert_sorted(obj->rtcpstatspoint,
				obj->latest, (bctbx_compare_func)sort_by_bandwidth);

			/*if the measure was 0% loss, reset to 0% every measures below it*/
			if (obj->latest->loss_percent < 1e-5){
				bctbx_list_t *it=obj->rtcpstatspoint;
				bctbx_list_t *latest_pos=bctbx_list_find(obj->rtcpstatspoint,obj->latest);
				while (it!=latest_pos->next){
					((rtcpstatspoint_t *)it->data)->loss_percent=0.f;
					it = it->next;
				}
			}
			ms_message("MSStatefulQosAnalyzer[%p]: one more %d: %f %f",
				obj, obj->curindex-1, obj->latest->bandwidth, obj->latest->loss_percent);

			if (bctbx_list_size(obj->rtcpstatspoint) > ESTIM_HISTORY){
				size_t prev_size = bctbx_list_size(obj->rtcpstatspoint);

				/*clean everything which occurred 60 sec or more ago*/
				time_t clear_time = ms_time(0) - 60;
				obj->rtcpstatspoint = bctbx_list_remove_custom(obj->rtcpstatspoint,
					(bctbx_compare_func)earlier_than, &clear_time);
				ms_message("MSStatefulQosAnalyzer[%p]: reached list maximum capacity "
					"(count=%u) --> Cleaned list (count=%u)",
					obj, (unsigned int)prev_size, (unsigned int)bctbx_list_size(obj->rtcpstatspoint));
			}
			return TRUE;
		}
	}
	return FALSE;
}

static double lerp(double inf, double sup, double v){
	return inf + (sup - inf) * v;
}

static bctbx_list_t *find_first_with_loss(bctbx_list_t *list){
	for(;list!=NULL;list=list->next){
		if (((rtcpstatspoint_t *)list->data)->loss_percent > 1e-5){
			return list;
		}
	}
	return NULL;
}

static void smooth_values(MSStatefulQosAnalyzer *obj){
	bctbx_list_t *first_loss = find_first_with_loss(obj->rtcpstatspoint);
	bctbx_list_t *it = obj->rtcpstatspoint;
	rtcpstatspoint_t *curr = (rtcpstatspoint_t *)it->data;
	double prev_loss = 0.;

	if (first_loss == obj->rtcpstatspoint){
		prev_loss = curr->loss_percent;
		curr->loss_percent = lerp(curr->loss_percent,
			((rtcpstatspoint_t *)it->next->data)->loss_percent, .25);
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

		double v = ((curr->bandwidth - prev->bandwidth) / (next->bandwidth - prev->bandwidth));
		double new_loss = lerp(prev_loss, next->loss_percent, v);
		prev_loss = curr->loss_percent;
		curr->loss_percent = (curr->loss_percent + new_loss) / 2.;
		it = it->next;
		curr = (rtcpstatspoint_t *)it->data;
	}
	curr->loss_percent = lerp(prev_loss, curr->loss_percent, .75);
}

static double compute_available_bw(MSStatefulQosAnalyzer *obj){
	bctbx_list_t *it;
	double constant_network_loss = 0.;
	double mean_bw = 0.;
	bctbx_list_t *current = obj->rtcpstatspoint;
	bctbx_list_t *last = current;
	size_t size = bctbx_list_size(obj->rtcpstatspoint);
	if (current == NULL){
		ms_message("MSStatefulQosAnalyzer[%p]: no points available for estimation", obj);
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

	ms_message("MSStatefulQosAnalyzer[%p]:\tconstant_network_loss=%f", obj, constant_network_loss);
#ifdef DEBUG
	for (it = obj->rtcpstatspoint; it != NULL; it=it->next){
		rtcpstatspoint_t * point = (rtcpstatspoint_t *)it->data;
		(void)point;
		ms_message("MSStatefulQosAnalyzer[%p]:\t\tsorted values %d: %f %f",
			obj, bctbx_list_position(obj->rtcpstatspoint, it), point->bandwidth, point->loss_percent);
	}
#endif

	if (size == 1){
		rtcpstatspoint_t *p = (rtcpstatspoint_t *)current->data;
		ms_message("MSStatefulQosAnalyzer[%p]: one single point", obj);
		mean_bw = p->bandwidth * ((p->loss_percent>1e-5) ? (100-p->loss_percent)/100.f:2);
	}else{
		while (current!=NULL && ((rtcpstatspoint_t*)current->data)->loss_percent<3+constant_network_loss){
			ms_message("MSStatefulQosAnalyzer[%p]:\t%d is stable", obj, bctbx_list_position(obj->rtcpstatspoint, current));

			/*find the last stable measure point, starting from highest bandwidth*/
			for (it=last;it!=current;it=it->prev){
				if (((rtcpstatspoint_t *)it->data)->loss_percent <= 3 + ((rtcpstatspoint_t*)current->data)->loss_percent){
					ms_message("MSStatefulQosAnalyzer[%p]:\t%d is less than %d",
						obj, bctbx_list_position(obj->rtcpstatspoint, it), bctbx_list_position(obj->rtcpstatspoint, current));
					current = it;
					break;
				}
			}
			/*current is the first unstable point, so taking the next one*/
			current = current->next;
		}

		/*all points are below the constant loss rate threshold:
		there might be bad network conditions but no congestion*/
		if (current == NULL){
			mean_bw = 2 * ((rtcpstatspoint_t*)last->data)->bandwidth;
		/*only first packet is stable*/
		}else if (current->prev == obj->rtcpstatspoint){
			rtcpstatspoint_t *p = (rtcpstatspoint_t *)current->prev->data;
			mean_bw = p->bandwidth * (100 - p->loss_percent) / 100.;
		/*otherwise, there is a congestion detected starting at "current"*/
		}else{
			rtcpstatspoint_t *laststable = (rtcpstatspoint_t*)current->prev->data;
			rtcpstatspoint_t *firstunstable = (rtcpstatspoint_t*)current->data;
			mean_bw = .5*(laststable->bandwidth+firstunstable->bandwidth);
		}

		ms_message("MSStatefulQosAnalyzer[%p]: [0->%d] last stable is %d(%f;%f)"
			, obj
			, bctbx_list_position(obj->rtcpstatspoint, last)
			, bctbx_list_position(obj->rtcpstatspoint, (current ? current->prev : last))
			, ((rtcpstatspoint_t*) (current ? current->prev->data : last->data))->bandwidth
			, ((rtcpstatspoint_t*) (current ? current->prev->data : last->data))->loss_percent);
		if (current!=NULL){
			ms_message("MSStatefulQosAnalyzer[%p]: , first unstable is %d(%f;%f)"
				, obj
				, bctbx_list_position(obj->rtcpstatspoint, current)
				, ((rtcpstatspoint_t*) current->data)->bandwidth
				, ((rtcpstatspoint_t*) current->data)->loss_percent);
		}
	}
	ms_message("MSStatefulQosAnalyzer[%p]:  --> estimated_available_bw=%f", obj, mean_bw);

	obj->network_loss_rate = constant_network_loss;
	obj->congestion_bandwidth = mean_bw;

	return mean_bw;
}

static void stateful_analyzer_suggest_action(MSQosAnalyzer *objbase, MSRateControlAction *action){
	MSStatefulQosAnalyzer *obj=(MSStatefulQosAnalyzer*)objbase;

	double curbw = 0;
	double bw = 0;
	rtcpstatspoint_t* greatest_pt = NULL;
	/*if this is the first measure, there is not enough reliable data to use; we
	assume loss rate is due to non congestionned network. This is mainly useful
	in the case loss rate is high (>30%), to reduce quality even before the second
	RTCP report which can be really used.
	*/
	if (obj->curindex==1){
		if (obj->network_loss_rate!=0.f){
			action->type=MSRateControlActionDecreaseBitrate;
			action->value=(int)obj->network_loss_rate;
		}
	}else {
		curbw = obj->latest ? obj->latest->bandwidth : 0.;
		bw = compute_available_bw(obj);
		greatest_pt = bctbx_list_size(obj->rtcpstatspoint) ?
			(rtcpstatspoint_t*)bctbx_list_nth_data(obj->rtcpstatspoint, (int)bctbx_list_size(obj->rtcpstatspoint)-1)
			: NULL;

		/*try a burst every 50 seconds (10 RTCP packets)*/
		if (obj->curindex % 10 == 6){
			ms_message("MSStatefulQosAnalyzer[%p]: try burst!", obj);
			obj->burst_state = MSStatefulQosAnalyzerBurstEnable;
		}
		/*test a min burst to avoid overestimation of available bandwidth but only
		if there is some loss*/
		else if (greatest_pt!=NULL && greatest_pt->loss_percent>1
				&& (obj->curindex % 10 == 2 || obj->curindex % 10 == 3)){
			ms_message("MSStatefulQosAnalyzer[%p]: try minimal burst!", obj);
			bw *= .33;
		}

		/*no bandwidth estimation computed*/
		if (bw <= 0 || curbw <= 0){
			action->type=MSRateControlActionDoNothing;
			action->value=0;
		}else if (bw > curbw){
			action->type=MSRateControlActionIncreaseQuality;
			action->value=MAX(0, (int)(100 * (bw / curbw - 1)));
		}else{
			action->type=MSRateControlActionDecreaseBitrate;
			action->value=MAX(10, (int)(-100 * (bw / curbw - 1)));
		}
	}

	ms_message("MSStatefulQosAnalyzer[%p]: %s of value %d",
		obj, ms_rate_control_action_type_name(action->type), action->value);


	if (objbase->on_action_suggested!=NULL){
		int i;
		char *data[4];
		int datac = sizeof(data) / sizeof(data[0]);
		data[0]=ms_strdup("%loss rtt_ms cur_bw");
		data[1]=ms_strdup_printf("%d %d %d"
			, obj->latest?(int)obj->latest->loss_percent:0
			, obj->latest?(int)obj->latest->rtt:0
			, obj->latest?(int)obj->latest->bandwidth:0
			);
		data[2]=ms_strdup("action_type action_value est_bw");
		data[3]=ms_strdup_printf("%s %d %d"
			, ms_rate_control_action_type_name(action->type)
			, action->value
			, (int)bw
			);

		objbase->on_action_suggested(objbase->on_action_suggested_user_pointer, datac, (const char**)data);

		for (i=0;i<datac;++i){
			ms_free(data[i]);
		}
	}
}

static bool_t stateful_analyzer_has_improved(MSQosAnalyzer *objbase){
	/*never tell the controller that situation has improved to avoid 'Stable' state
	which is not necessary for this analyzer*/
	return FALSE;
}

static void stateful_analyzer_update(MSQosAnalyzer *objbase){
	MSStatefulQosAnalyzer *obj=(MSStatefulQosAnalyzer*)objbase;
	static time_t last_measure;

	/* Every seconds, save the bandwidth used. This is needed to know how much
	bandwidth was used when receiving a receiver report. Since the report contains
	the "last sequence number", it allows us to precisely know which interval to
	consider */
	if (last_measure != ms_time(0)){
		obj->upload_bandwidth_count++;
		obj->upload_bandwidth_sum+=rtp_session_get_send_bandwidth(obj->session)/1000.0;

		/* Save bandwidth used at this time */
		obj->upload_bandwidth[obj->upload_bandwidth_cur].seq_number = rtp_session_get_seq_number(obj->session);
		obj->upload_bandwidth[obj->upload_bandwidth_cur].up_bandwidth = rtp_session_get_send_bandwidth(obj->session)/1000.0f;
		obj->upload_bandwidth_cur = (obj->upload_bandwidth_cur+1)%BW_HISTORY;
	}
	last_measure = ms_time(0);

	if (obj->burst_duration_ms>0){
		switch (obj->burst_state){
		case MSStatefulQosAnalyzerBurstEnable:{
			obj->burst_state=MSStatefulQosAnalyzerBurstInProgress;
			ortp_gettimeofday(&obj->start_time, NULL);
			rtp_session_set_duplication_ratio(obj->session, (float)obj->burst_ratio);
		} case MSStatefulQosAnalyzerBurstInProgress: {
			struct timeval now;
			float elapsed;

			ortp_gettimeofday(&now,NULL);
			elapsed=((now.tv_sec-obj->start_time.tv_sec)*1000.0f) +  ((now.tv_usec-obj->start_time.tv_usec)/1000.0f);

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
	bctbx_list_for_each(obj->rtcpstatspoint, ms_free);
	bctbx_list_free(obj->rtcpstatspoint);
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
	obj->parent.type=MSQosAnalyzerAlgorithmStateful;
	obj->parent.lre=ortp_loss_rate_estimator_new(LOSS_RATE_MIN_INTERVAL, LOSS_RATE_MIN_TIME, session);

	/*burst period will float the upload bandwidth assuming 5 sec RTCP reports interval*/
	obj->burst_duration_ms=1000;
	obj->burst_ratio=9;
	return (MSQosAnalyzer*)obj;
}


