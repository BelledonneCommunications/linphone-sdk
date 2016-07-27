/*
mediastreamer2 library - modular sound and video processing and streaming

 * Copyright (C) 2011  Belledonne Communications, Grenoble, France

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

#include "mediastreamer2/qualityindicator.h"

#include <math.h>

#define RATING_SCALE 5.0f
#define WORSE_JITTER 0.2f
#define WORSE_RT_PROP 5.0f
#define SEQ_INTERVAL 60
#define TIME_INTERVAL 3000


struct _MSQualityIndicator{
	RtpSession *session;
	char *label;
	OrtpLossRateEstimator *lr_estimator;
	int clockrate;
	double sum_ratings;
	double sum_lq_ratings;
	float rating;
	float lq_rating;	/* Listening-quality rating */
	float local_rating;
	float remote_rating;
	float local_lq_rating;	/* Local listening-quality rating */
	float remote_lq_rating;	/* Remote listening-quality rating */
	uint64_t last_packet_count;
	uint32_t last_ext_seq;
	uint32_t last_late;
	int count;
	float cur_late_rate;
	float cur_loss_rate;
};

MSQualityIndicator *ms_quality_indicator_new(RtpSession *session){
	MSQualityIndicator *qi=ms_new0(MSQualityIndicator,1);
	qi->session=session;
	qi->lr_estimator=ortp_loss_rate_estimator_new(SEQ_INTERVAL, TIME_INTERVAL, qi->session);
	qi->rating=5.0;
	qi->lq_rating=5.0;
	qi->local_rating=1.0;
	qi->remote_rating=1.0;
	qi->local_lq_rating=1.0;
	qi->remote_lq_rating=1.0;
	return qi;
}

void ms_quality_indicator_set_label(MSQualityIndicator *obj, const char *label){
	if (obj->label){
		ms_free(obj->label);
		obj->label=NULL;
	}
	if (label) obj->label=ms_strdup(label);
}

float ms_quality_indicator_get_rating(const MSQualityIndicator *qi){
	return qi->rating;
}

float ms_quality_indicator_get_lq_rating(const MSQualityIndicator *qi) {
	return qi->lq_rating;
}

static float inter_jitter_rating(float inter_jitter){
	float tmp=inter_jitter/WORSE_JITTER;
	if (tmp>1) tmp=1;
	return 1.0f-(0.3f*tmp);
}

static float rt_prop_rating(float rt_prop){
	float tmp=rt_prop/WORSE_RT_PROP;
	if (tmp>1) tmp=1;
	return 1.0f-(0.7f*tmp);
}

static float loss_rating(float loss){
	/* the exp function allows to have a rating that decrease rapidly at the begining.
	 Indeed even with 10% loss, the quality is significantly affected. It is not good at all.
	 With this formula:
	 5% losses gives a rating of 4/5
	 20% losses gives a rating of 2.2/5
	 80% losses gives a rating of 0.2
	*/
	return expf(-loss*4.0f);
}

static float compute_rating(float loss_rate, float inter_jitter, float late_rate, float rt_prop){
	return loss_rating(loss_rate)*inter_jitter_rating(inter_jitter)*loss_rating(late_rate)*rt_prop_rating(rt_prop);
}

/* Compute listening-quality rating */
static float compute_lq_rating(float loss_rate, float inter_jitter, float late_rate) {
	return loss_rating(loss_rate) * inter_jitter_rating(inter_jitter) * loss_rating(late_rate);
}

static void update_global_rating(MSQualityIndicator *qi){
	qi->rating=RATING_SCALE*qi->remote_rating*qi->local_rating;
	qi->sum_ratings+=qi->rating;
	qi->lq_rating=RATING_SCALE*qi->remote_lq_rating*qi->local_lq_rating;
	qi->sum_lq_ratings+=qi->lq_rating;
	qi->count++;
}

void ms_quality_indicator_update_from_feedback(MSQualityIndicator *qi, mblk_t *rtcp){
	const report_block_t *rb=NULL;
	if (rtcp_is_SR(rtcp)){
		rb=rtcp_SR_get_report_block(rtcp,0);
	}else if (rtcp_is_RR(rtcp)){
		rb=rtcp_RR_get_report_block(rtcp,0);
	}else{
		return;
	}
	if (qi->clockrate==0){
		PayloadType *pt=rtp_profile_get_payload(rtp_session_get_send_profile(qi->session),rtp_session_get_send_payload_type(qi->session));
		if (pt!=NULL) qi->clockrate=pt->clock_rate;
		else return;
	}
	if (rb){
		float loss_rate;
		float inter_jitter=(float)report_block_get_interarrival_jitter(rb)/(float)qi->clockrate;
		float rt_prop=rtp_session_get_round_trip_propagation(qi->session);
		bool_t new_value;

		new_value=ortp_loss_rate_estimator_process_report_block(qi->lr_estimator,qi->session,rb);
		loss_rate=ortp_loss_rate_estimator_get_value(qi->lr_estimator);
		qi->remote_rating=compute_rating(loss_rate/100.0f,inter_jitter,0,rt_prop);
		qi->remote_lq_rating=compute_lq_rating(loss_rate/100.0f,inter_jitter,0);
		update_global_rating(qi);
		if (new_value){
			ms_message("MSQualityIndicator[%p][%s], remote statistics available:"
						"\n\t%-20s: %3.1f%%"
						"\n\t%-20s: %3.1fms"
						"\n\t%-20s: %3.1fms"
						,qi,qi->label ? qi->label : "no label"
						,"Loss rate", loss_rate
						,"Inter-arrival jitter",1000*inter_jitter
						,"RT propagation",1000*rt_prop);
		}
	}
}

void ms_quality_indicator_update_local(MSQualityIndicator *qi){
	const rtp_stats_t *stats=rtp_session_get_stats(qi->session);
	int lost,late,recvcnt;
	float loss_rate=0,late_rate=0;
	uint32_t ext_seq=rtp_session_get_rcv_ext_seq_number(qi->session);

	recvcnt=(int)(stats->packet_recv-qi->last_packet_count);
	if (recvcnt==0){
		// ms_message("ms_quality_indicator_update_local(): no packet received since last call");
		return;/* no information usable*/
	}else if (recvcnt<0){
		qi->last_packet_count=stats->packet_recv;
		qi->last_ext_seq=ext_seq;
		recvcnt=0; /*should not happen obviously*/
		return;
	}else if (qi->last_packet_count==0){
		qi->last_ext_seq=ext_seq;
	}

	lost=(ext_seq-qi->last_ext_seq) - (recvcnt);
	qi->last_ext_seq=ext_seq;
	qi->last_packet_count=stats->packet_recv;

	late=(int)(stats->outoftime-qi->last_late);
	qi->last_late=(uint32_t)stats->outoftime;


	if (lost<0) lost=0; /* will be the case at least the first time, because we don't know the initial sequence number*/
	if (late<0) late=0;

	loss_rate=(float)lost/(float)recvcnt;
	qi->cur_loss_rate=loss_rate*100.0f;

	late_rate=(float)late/(float)recvcnt;
	qi->cur_late_rate=late_rate*100.0f;

	qi->local_rating=compute_rating(loss_rate,0,late_rate,rtp_session_get_round_trip_propagation(qi->session));
	qi->local_lq_rating=compute_lq_rating(loss_rate,0,late_rate);
	update_global_rating(qi);
}

float ms_quality_indicator_get_average_rating(const MSQualityIndicator *qi){
	if (qi->count==0) return -1; /*no rating available*/
	return (float)(qi->sum_ratings/(double)qi->count);
}

float ms_quality_indicator_get_average_lq_rating(const MSQualityIndicator *qi) {
	if (qi->count == 0) return -1; /* No rating available */
	return (float)(qi->sum_lq_ratings / (double)qi->count);
}

float ms_quality_indicator_get_local_loss_rate(const MSQualityIndicator *qi){
	return qi->cur_loss_rate;
}

float ms_quality_indicator_get_local_late_rate(const MSQualityIndicator *qi){
	return qi->cur_late_rate;
}

void ms_quality_indicator_destroy(MSQualityIndicator *qi){
	ortp_loss_rate_estimator_destroy(qi->lr_estimator);
	if (qi->label) ms_free(qi->label);
	ms_free(qi);
}


