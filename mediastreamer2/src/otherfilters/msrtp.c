/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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


#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/msticker.h"

#include "ortp/telephonyevents.h"
#if defined(__cplusplus)
#define B64_NO_NAMESPACE
#endif
#include "ortp/b64.h"
#include "ortp/stun.h"

static const int default_dtmf_duration_ms=100; /*in milliseconds*/

struct SenderData {
	RtpSession *session;
	uint32_t tsoff;
	uint32_t last_ts;
	int64_t last_sent_time;
	int64_t last_stun_sent_time;
	uint32_t skip_until;
	int rate;
	int nchannels;
	int dtmf_duration;
	int dtmf_ts_step;
	uint32_t dtmf_ts_cur;
	char relay_session_id[64];
	int relay_session_id_size;
	uint64_t last_rsi_time;
	char dtmf;
	bool_t dtmf_start;
	bool_t skip;
	bool_t mute_mic;
	bool_t use_task;
};

typedef struct SenderData SenderData;

/* Send dummy STUN packet to open NAT ports ASAP. */
static void send_stun_packet(RtpSession *s)
{
	StunMessage msg;
	mblk_t *mp;
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;

	memset(&msg, 0, sizeof(StunMessage));
	stunBuildReqSimple(&msg, NULL, FALSE, FALSE, 1);
	len = stunEncodeMessage(&msg, buf, len, NULL);
	if (len > 0) {
		mp = allocb(len, BPRI_MED);
		memcpy(mp->b_wptr, buf, len);
		mp->b_wptr += len;
		rtp_session_sendm_with_ts(s, mp, 0);
	}
}

static void sender_init(MSFilter * f)
{
	SenderData *d = (SenderData *)ms_new0(SenderData, 1);
	const char *tmp=getenv("MS2_RTP_FIXED_DELAY");
	
	d->session = NULL;
	d->tsoff = 0;
	d->skip_until = 0;
	d->skip = FALSE;
	d->rate = 8000;
	d->nchannels = 1;
	d->dtmf = 0;
	d->dtmf_duration = 800;
	d->dtmf_ts_step=160;
	d->mute_mic=FALSE;
	d->relay_session_id_size=0;
	d->last_rsi_time=0;
	d->last_sent_time=-1;
	d->last_stun_sent_time = -1;
	d->last_ts=0;
	d->use_task= tmp ? (!!atoi(tmp)) : FALSE;
	if (d->use_task) ms_message("MSRtpSend will use tasks to send out packet at the beginning of ticks.");
	f->data = d;
}

static void sender_uninit(MSFilter * f)
{
	SenderData *d = (SenderData *) f->data;

	ms_free(d);
}

static int sender_send_dtmf(MSFilter * f, void *arg)
{
	const char *dtmf = (const char *) arg;
	SenderData *d = (SenderData *) f->data;

	ms_filter_lock(f);
	if (d->skip==TRUE)
	{
		ms_filter_unlock(f);
		ms_warning("MSRtpSend: already sending a dtmf.");
		return -1;
	}
	d->dtmf = dtmf[0];
	ms_filter_unlock(f);
	return 0;
}

static int sender_set_dtmf_duration(MSFilter * f, void *arg)
{
	SenderData *d = (SenderData *) f->data;
	d->dtmf_duration = *((int*)arg);
	return 0;
}

static int sender_set_session(MSFilter * f, void *arg)
{
	SenderData *d = (SenderData *) f->data;
	RtpSession *s = (RtpSession *) arg;
	PayloadType *pt =
		rtp_profile_get_payload(rtp_session_get_profile(s),
								rtp_session_get_send_payload_type(s));
	if (pt != NULL) {
		d->rate = pt->clock_rate;
		d->dtmf_duration=(default_dtmf_duration_ms*d->rate)/1000;
		d->dtmf_ts_step=(20*d->rate)/1000;
		send_stun_packet(s);
	} else {
		ms_warning("Sending undefined payload type ?");
	}
	d->session = s;
	return 0;
}

static int sender_mute_mic(MSFilter * f, void *arg)
{
	SenderData *d = (SenderData *) f->data;
	ms_filter_lock(f);
	d->mute_mic=TRUE;
	ms_filter_unlock(f);
	return 0;
}

static int sender_unmute_mic(MSFilter * f, void *arg)
{
	SenderData *d = (SenderData *) f->data;
	ms_filter_lock(f);
	d->mute_mic=FALSE;
	ms_filter_unlock(f);
	return 0;
}

static int sender_set_relay_session_id(MSFilter *f, void*arg){
	SenderData *d = (SenderData *) f->data;
	const char *tmp=(const char *)arg;
	d->relay_session_id_size=b64_decode(tmp, strlen(tmp), (void*)d->relay_session_id, (unsigned int)sizeof(d->relay_session_id));
	return 0;
}

static int sender_get_sr(MSFilter *f, void *arg){
	SenderData *d = (SenderData *) f->data;
	PayloadType *pt;
	if (d->session==NULL) {
		ms_warning("Could not obtain sample rate, session is not set.");
		return -1;
	}
	pt=rtp_profile_get_payload(rtp_session_get_profile(d->session),
									rtp_session_get_recv_payload_type(d->session));
	if (pt != NULL) {
		if (strcasecmp(pt->mime_type,"G722")==0)
			*(int*)arg=16000;
		else
			*(int*)arg=pt->clock_rate;
	}else{
		ms_warning("MSRtpSend: Could not obtain sample rate, payload type is unknown.");
		return -1;
	}
	return 0;
}

static int sender_get_ch(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	*(int *)arg = d->nchannels;
	return 0;
}

static int sender_set_ch(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *)f->data;
	d->nchannels = *(int *)arg;
	return 0;
}

/* the goal of that function is to return a absolute timestamp closest to real time, with respect of given packet_ts, which is a relative to an undefined origin*/
static uint32_t get_cur_timestamp(MSFilter * f, mblk_t *im)
{
	SenderData *d = (SenderData *) f->data;
	uint32_t curts = (uint32_t)( (f->ticker->time*(uint64_t)d->rate)/(uint64_t)1000) ;
	int diffts;
	uint32_t netts;
	int difftime_ts;

	if (im && d->dtmf==0){ /*do not perform timestamp adjustment while a dtmf is being sent, otherwise durations are erroneous */
		uint32_t packet_ts=mblk_get_timestamp_info(im);
		if (d->last_sent_time==-1){
			d->tsoff = curts - packet_ts;
		}else{
			diffts=packet_ts-d->last_ts;
			difftime_ts=((f->ticker->time-d->last_sent_time)*d->rate)/1000;
			/* detect timestamp jump in the stream and adjust so that they become continuous on the network*/
			if (abs(diffts-difftime_ts)>(d->rate/5)){
				uint32_t tsoff=curts - packet_ts;
				ms_message("Adjusting output timestamp by %i",(tsoff-d->tsoff));
				d->tsoff = tsoff;
			}
		}
		netts = packet_ts + d->tsoff;
		d->last_sent_time=f->ticker->time;
		d->last_ts=packet_ts;
	}else netts=curts;
	return netts;
}

static int send_dtmf(MSFilter * f, uint32_t timestamp_start)
{
	SenderData *d = (SenderData *) f->data;
	mblk_t *m1;
	int tev_type;

	/* create the first telephony event packet */
	switch (d->dtmf){
		case '1':
			tev_type=TEV_DTMF_1;
		break;
		case '2':
			tev_type=TEV_DTMF_2;
		break;
		case '3':
			tev_type=TEV_DTMF_3;
		break;
		case '4':
			tev_type=TEV_DTMF_4;
		break;
		case '5':
			tev_type=TEV_DTMF_5;
		break;
		case '6':
			tev_type=TEV_DTMF_6;
		break;
		case '7':
			tev_type=TEV_DTMF_7;
		break;
		case '8':
			tev_type=TEV_DTMF_8;
		break;
		case '9':
			tev_type=TEV_DTMF_9;
		break;
		case '*':
			tev_type=TEV_DTMF_STAR;
		break;
		case '0':
			tev_type=TEV_DTMF_0;
		break;
		case '#':
			tev_type=TEV_DTMF_POUND;
		break;

		case 'A':
		case 'a':
		  tev_type=TEV_DTMF_A;
		  break;


		case 'B':
		case 'b':
		  tev_type=TEV_DTMF_B;
		  break;

		case 'C':
		case 'c':
		  tev_type=TEV_DTMF_C;
		  break;

		case 'D':
		case 'd':
		  tev_type=TEV_DTMF_D;
		  break;

		case '!':
		  tev_type=TEV_FLASH;
		  break;


		default:
		ms_warning("Bad dtmf: %c.",d->dtmf);
		return -1;
	}


	
	m1=rtp_session_create_telephone_event_packet(d->session,timestamp_start==d->dtmf_ts_cur);
	
	if (m1==NULL) return -1;

	d->dtmf_ts_cur+=d->dtmf_ts_step;
	if (RTP_TIMESTAMP_IS_NEWER_THAN(d->dtmf_ts_cur, d->skip_until)) {
		//retransmit end of rtp dtmf event
		mblk_t *tmp;
		rtp_session_add_telephone_event(d->session,m1,tev_type,1,10,(d->dtmf_ts_cur-timestamp_start));
		tmp=copymsg(m1);
		rtp_session_sendm_with_ts(d->session,tmp,timestamp_start);
		d->session->rtp.snd_seq--;
		tmp=copymsg(m1);
		rtp_session_sendm_with_ts(d->session,tmp,timestamp_start);
		d->session->rtp.snd_seq--;
		rtp_session_sendm_with_ts(d->session,m1,timestamp_start);
		d->skip = FALSE;
		d->dtmf = 0;
		ms_message("Finished sending RFC2833 dtmf %c",d->dtmf);
	}else {
		rtp_session_add_telephone_event(d->session,m1,tev_type,0,10,(d->dtmf_ts_cur-timestamp_start));
		rtp_session_sendm_with_ts(d->session,m1,timestamp_start);
	}
	return 0;
}

static void _sender_process(MSFilter * f)
{
	SenderData *d = (SenderData *) f->data;
	RtpSession *s = d->session;

	mblk_t *im;
	uint32_t timestamp;


	if (d->relay_session_id_size>0 && 
		( (f->ticker->time-d->last_rsi_time)>5000 || d->last_rsi_time==0) ) {
		ms_message("relay session id sent in RTCP APP");
		rtp_session_send_rtcp_APP(s,0,"RSID",(const uint8_t *)d->relay_session_id,d->relay_session_id_size);
		d->last_rsi_time=f->ticker->time;
	}

	ms_filter_lock(f);
	im = ms_queue_get(f->inputs[0]);
	do {
		mblk_t *header;

		timestamp = get_cur_timestamp(f, im);
		
		if (d->dtmf != 0 && !d->skip) {
			ms_debug("prepare to send RFC2833 dtmf.");
			d->skip_until = timestamp + d->dtmf_duration;
			d->dtmf_ts_cur=timestamp;
			d->skip = TRUE;
		}
		if (d->skip) {
			uint32_t origin_ts=d->skip_until-d->dtmf_duration;
			if (RTP_TIMESTAMP_IS_NEWER_THAN(timestamp,d->dtmf_ts_cur)){
				ms_debug("Sending RFC2833 packet, start_timestamp=%u, dtmf_ts_cur=%u",origin_ts,d->dtmf_ts_cur);
				send_dtmf(f, origin_ts);
			}
		}
		if (im){
			if (d->skip == FALSE && d->mute_mic==FALSE){
				header = rtp_session_create_packet(s, 12, NULL, 0);
				rtp_set_markbit(header, mblk_get_marker_info(im));
				header->b_cont = im;
				rtp_session_sendm_with_ts(s, header, timestamp);
			}else{
				freemsg(im);
			}
		}
	}while ((im = ms_queue_get(f->inputs[0])) != NULL);

	if (d->last_sent_time == -1) {
		if ((d->last_stun_sent_time == -1) || ((f->ticker->time - d->last_stun_sent_time) >= 500)) {
			d->last_stun_sent_time = f->ticker->time;
		}
		if (d->last_stun_sent_time == f->ticker->time) {
			send_stun_packet(s);
		}
	}

	ms_filter_unlock(f);
}

static void sender_process(MSFilter * f){
	SenderData *d = (SenderData *) f->data;
	RtpSession *s = d->session;
	if (s == NULL){
		ms_queue_flush(f->inputs[0]);
		return;
	}
	if (d->use_task)
		ms_filter_postpone_task(f,_sender_process);
	else _sender_process(f);
}


static MSFilterMethod sender_methods[] = {
	{MS_RTP_SEND_MUTE_MIC, sender_mute_mic},
	{MS_RTP_SEND_UNMUTE_MIC, sender_unmute_mic},
	{MS_RTP_SEND_SET_SESSION, sender_set_session},
	{MS_RTP_SEND_SEND_DTMF, sender_send_dtmf},
	{MS_RTP_SEND_SET_RELAY_SESSION_ID, sender_set_relay_session_id},
	{MS_FILTER_GET_SAMPLE_RATE, sender_get_sr },
	{MS_FILTER_GET_NCHANNELS, sender_get_ch },
	{MS_FILTER_SET_NCHANNELS, sender_set_ch },
	{MS_RTP_SEND_SET_DTMF_DURATION, sender_set_dtmf_duration },
	{0, NULL}
};

#ifdef _MSC_VER

MSFilterDesc ms_rtp_send_desc = {
	MS_RTP_SEND_ID,
	"MSRtpSend",
	N_("RTP output filter"),
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	sender_init,
	NULL,
	sender_process,
	NULL,
	sender_uninit,
	sender_methods,
	MS_FILTER_IS_PUMP
};

#else

MSFilterDesc ms_rtp_send_desc = {
	.id = MS_RTP_SEND_ID,
	.name = "MSRtpSend",
	.text = N_("RTP output filter"),
	.category = MS_FILTER_OTHER,
	.ninputs = 1,
	.noutputs = 0,
	.init = sender_init,
	.process = sender_process,
	.uninit = sender_uninit,
	.methods = sender_methods,
	.flags=MS_FILTER_IS_PUMP
};

#endif

struct ReceiverData {
	RtpSession *session;
	int rate;
	int nchannels;
	bool_t starting;
	bool_t reset_jb;
};

typedef struct ReceiverData ReceiverData;

static void receiver_init(MSFilter * f)
{
	ReceiverData *d = (ReceiverData *)ms_new0(ReceiverData, 1);
	d->session = NULL;
	d->rate = 8000;
	d->nchannels = 1;
	f->data = d;
}

static void receiver_postprocess(MSFilter * f){
	/*ReceiverData *d = (ReceiverData *) f->data;*/
}

static void receiver_uninit(MSFilter * f){
	ReceiverData *d = (ReceiverData *) f->data;
	ms_free(d);
}

static int receiver_set_session(MSFilter * f, void *arg)
{
	ReceiverData *d = (ReceiverData *) f->data;
	RtpSession *s = (RtpSession *) arg;
	PayloadType *pt = rtp_profile_get_payload(rtp_session_get_profile(s),
											  rtp_session_get_recv_payload_type
											  (s));
	if (pt != NULL) {
		d->rate = pt->clock_rate;
	} else {
		ms_warning("Receiving undefined payload type %i ?",
		    rtp_session_get_recv_payload_type(s));
	}
	d->session = s;

	return 0;
}

static int receiver_get_sr(MSFilter *f, void *arg){
	ReceiverData *d = (ReceiverData *) f->data;
	PayloadType *pt;
	if (d->session==NULL) {
		ms_warning("Could not obtain sample rate, session is not set.");
		return -1;
	}
	pt=rtp_profile_get_payload(rtp_session_get_profile(d->session),
									rtp_session_get_recv_payload_type(d->session));
	if (pt != NULL) {
		if (strcasecmp(pt->mime_type,"G722")==0)
			*(int*)arg=16000;
		else
			*(int*)arg=pt->clock_rate;
	}else{
		ms_warning("Could not obtain sample rate, payload type is unknown.");
		return -1;
	}
	return 0;
}

static int receiver_get_ch(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	*(int *)arg = d->nchannels;
	return 0;
}

static int receiver_set_ch(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	d->nchannels = *(int *)arg;
	return 0;
}

static int receiver_reset_jitter_buffer(MSFilter *f, void *arg) {
	ReceiverData *d = (ReceiverData *)f->data;
	d->reset_jb = TRUE;
	return 0;
}


static void receiver_preprocess(MSFilter * f){
	ReceiverData *d = (ReceiverData *) f->data;
	d->starting=TRUE;
}

static void receiver_process(MSFilter * f)
{
	ReceiverData *d = (ReceiverData *) f->data;
	mblk_t *m;
	uint32_t timestamp;

	if (d->session == NULL)
		return;
	
	if (d->reset_jb){
		ms_message("Reseting jitter buffer");
		rtp_session_resync(d->session);
		d->reset_jb=FALSE;
	}

	if (d->starting){
		PayloadType *pt=rtp_profile_get_payload(
			rtp_session_get_profile(d->session),
			rtp_session_get_recv_payload_type(d->session));
		if (pt && pt->type!=PAYLOAD_VIDEO)
			rtp_session_flush_sockets(d->session);
		d->starting=FALSE;
	}

	timestamp = (uint32_t) (f->ticker->time * (d->rate/1000));
	while ((m = rtp_session_recvm_with_ts(d->session, timestamp)) != NULL) {
		mblk_set_timestamp_info(m, rtp_get_timestamp(m));
		mblk_set_marker_info(m, rtp_get_markbit(m));
		mblk_set_cseq(m, rtp_get_seqnumber(m));
		rtp_get_payload(m,&m->b_rptr);
		ms_queue_put(f->outputs[0], m);
	}
}

static MSFilterMethod receiver_methods[] = {
	{	MS_RTP_RECV_SET_SESSION	, receiver_set_session	},
	{	MS_RTP_RECV_RESET_JITTER_BUFFER, receiver_reset_jitter_buffer },
	{	MS_FILTER_GET_SAMPLE_RATE	, receiver_get_sr		},
	{	MS_FILTER_GET_NCHANNELS	,	receiver_get_ch	},
	{	MS_FILTER_SET_NCHANNELS	,	receiver_set_ch	},
	{	0, NULL}
};

#ifdef _MSC_VER

MSFilterDesc ms_rtp_recv_desc = {
	MS_RTP_RECV_ID,
	"MSRtpRecv",
	N_("RTP input filter"),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	receiver_init,
	receiver_preprocess,
	receiver_process,
	receiver_postprocess,
	receiver_uninit,
	receiver_methods
};

#else

MSFilterDesc ms_rtp_recv_desc = {
	.id = MS_RTP_RECV_ID,
	.name = "MSRtpRecv",
	.text = N_("RTP input filter"),
	.category = MS_FILTER_OTHER,
	.ninputs = 0,
	.noutputs = 1,
	.init = receiver_init,
	.preprocess = receiver_preprocess,
	.process = receiver_process,
	.postprocess=receiver_postprocess,
	.uninit = receiver_uninit,
	.methods = receiver_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_rtp_send_desc)
MS_FILTER_DESC_EXPORT(ms_rtp_recv_desc)
