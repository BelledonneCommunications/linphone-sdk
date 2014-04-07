/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

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


#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mediastream.h"
#include "private.h"

#include <ctype.h>

#ifndef MS_MINIMAL_MTU
/*this is used for determining the minimum size of recv buffers for RTP packets
 Keep 1500 for maximum interoparibility*/
#define MS_MINIMAL_MTU 1500 
#endif



#if defined(_WIN32_WCE)
time_t
ms_time(time_t *t) {
	DWORD timemillis = GetTickCount();
	if (timemillis > 0) {
		if (t != NULL) *t = timemillis / 1000;
	}
	return timemillis / 1000;
}
#endif


static void disable_checksums(ortp_socket_t sock) {
#if defined(DISABLE_CHECKSUMS) && defined(SO_NO_CHECK)
	int option = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_NO_CHECK, &option, sizeof(option)) == -1) {
		ms_warning("Could not disable udp checksum: %s", strerror(errno));
	}
#endif
}

/**
 * This function must be called from the MSTicker thread:
 * it replaces one filter by another one.
 * This is a dirty hack that works anyway.
 * It would be interesting to have something that does the job
 * more easily within the MSTicker API.
 */
static void media_stream_change_decoder(MediaStream *stream, int payload) {
	RtpSession *session = stream->sessions.rtp_session;
	RtpProfile *prof = rtp_session_get_profile(session);
	PayloadType *pt = rtp_profile_get_payload(prof, payload);
	
	if (stream->decoder == NULL){
		ms_message("media_stream_change_decoder(): ignored, no decoder.");
		return;
	}
	
	if (pt != NULL){
		MSFilter *dec;

		if (stream->type == VideoStreamType){
			/* Q: why only video ? this optimization seems relevant for audio too.*/
			if ((stream->decoder != NULL) && (stream->decoder->desc->enc_fmt != NULL)
			&& (strcasecmp(pt->mime_type, stream->decoder->desc->enc_fmt) == 0)) {
				/* Same formats behind different numbers, nothing to do. */
				return;
			}
		}

		dec = ms_filter_create_decoder(pt->mime_type);
		if (dec != NULL) {
			MSFilter *nextFilter = stream->decoder->outputs[0]->next.filter;
			ms_filter_unlink(stream->rtprecv, 0, stream->decoder, 0);
			ms_filter_unlink(stream->decoder, 0, nextFilter, 0);
			ms_filter_postprocess(stream->decoder);
			ms_filter_destroy(stream->decoder);
			stream->decoder = dec;
			if (pt->recv_fmtp != NULL)
				ms_filter_call_method(stream->decoder, MS_FILTER_ADD_FMTP, (void *)pt->recv_fmtp);
			ms_filter_link(stream->rtprecv, 0, stream->decoder, 0);
			ms_filter_link(stream->decoder, 0, nextFilter, 0);
			ms_filter_preprocess(stream->decoder,stream->sessions.ticker);
		} else {
			ms_warning("No decoder found for %s", pt->mime_type);
		}
	} else {
		ms_warning("No payload defined with number %i", payload);
	}
}

MSTickerPrio __ms_get_default_prio(bool_t is_video) {
	const char *penv;

	if (is_video) {
#ifdef __ios
		return MS_TICKER_PRIO_HIGH;
#else
		return MS_TICKER_PRIO_NORMAL;
#endif
	}

	penv = getenv("MS_AUDIO_PRIO");
	if (penv) {
		if (strcasecmp(penv, "NORMAL") == 0) return MS_TICKER_PRIO_NORMAL;
		if (strcasecmp(penv, "HIGH") == 0) return MS_TICKER_PRIO_HIGH;
		if (strcasecmp(penv, "REALTIME") == 0) return MS_TICKER_PRIO_REALTIME;
		ms_error("Undefined priority %s", penv);
	}
#ifdef __linux
	return MS_TICKER_PRIO_REALTIME;
#else
	return MS_TICKER_PRIO_HIGH;
#endif
}

RtpSession * create_duplex_rtpsession(int loc_rtp_port, int loc_rtcp_port, bool_t ipv6) {
	RtpSession *rtpr;

	rtpr = rtp_session_new(RTP_SESSION_SENDRECV);
	rtp_session_set_recv_buf_size(rtpr, MAX(ms_get_mtu() , MS_MINIMAL_MTU));
	rtp_session_set_scheduling_mode(rtpr, 0);
	rtp_session_set_blocking_mode(rtpr, 0);
	rtp_session_enable_adaptive_jitter_compensation(rtpr, TRUE);
	rtp_session_set_symmetric_rtp(rtpr, TRUE);
	rtp_session_set_local_addr(rtpr, ipv6 ? "::" : "0.0.0.0", loc_rtp_port, loc_rtcp_port);
	rtp_session_signal_connect(rtpr, "timestamp_jump", (RtpCallback)rtp_session_resync, (long)NULL);
	rtp_session_signal_connect(rtpr, "ssrc_changed", (RtpCallback)rtp_session_resync, (long)NULL);
	rtp_session_set_ssrc_changed_threshold(rtpr, 0);
	rtp_session_set_rtcp_report_interval(rtpr, 2500);	/* At the beginning of the session send more reports. */
	disable_checksums(rtp_session_get_rtp_socket(rtpr));
	return rtpr;
}

void media_stream_start_ticker(MediaStream *stream) {
	MSTickerParams params = {0};
	char name[16];

	if (stream->sessions.ticker) return;
	snprintf(name, sizeof(name) - 1, "%s MSTicker", media_stream_type_str(stream));
	name[0] = toupper(name[0]);
	params.name = name;
	params.prio = __ms_get_default_prio((stream->type == VideoStreamType) ? TRUE : FALSE);
	stream->sessions.ticker = ms_ticker_new_with_params(&params);
}

const char * media_stream_type_str(MediaStream *stream) {
	switch (stream->type) {
		default:
		case AudioStreamType:
			return "audio";
		case VideoStreamType:
			return "video";
	}
}

void ms_media_stream_sessions_uninit(MSMediaStreamSessions *sessions){
	if (sessions->rtp_session) {
		rtp_session_destroy(sessions->rtp_session);
		sessions->rtp_session=NULL;
	}
	if (sessions->srtp_session) {
		ortp_srtp_dealloc(sessions->srtp_session);
		sessions->srtp_session=NULL;
	}
	if (sessions->zrtp_context != NULL) {
		ortp_zrtp_context_destroy(sessions->zrtp_context);
		sessions->zrtp_context = NULL;
	}
	if (sessions->ticker){
		ms_ticker_destroy(sessions->ticker);
		sessions->ticker=NULL;
	}
}

void media_stream_free(MediaStream *stream) {
	if (stream->sessions.rtp_session != NULL){
		rtp_session_unregister_event_queue(stream->sessions.rtp_session, stream->evq);
	}
	if (stream->owns_sessions){
		ms_media_stream_sessions_uninit(&stream->sessions);
	}
	if (stream->evq) ortp_ev_queue_destroy(stream->evq);
	if (stream->rc != NULL) ms_bitrate_controller_destroy(stream->rc);
	if (stream->rtpsend != NULL) ms_filter_destroy(stream->rtpsend);
	if (stream->rtprecv != NULL) ms_filter_destroy(stream->rtprecv);
	if (stream->encoder != NULL) ms_filter_destroy(stream->encoder);
	if (stream->decoder != NULL) ms_filter_destroy(stream->decoder);
	if (stream->voidsink != NULL) ms_filter_destroy(stream->voidsink);
	if (stream->qi) ms_quality_indicator_destroy(stream->qi);
	
}

void media_stream_set_rtcp_information(MediaStream *stream, const char *cname, const char *tool) {
	if (stream->sessions.rtp_session != NULL) {
		rtp_session_set_source_description(stream->sessions.rtp_session, cname, NULL, NULL, NULL, NULL, tool, NULL);
	}
}

void media_stream_get_local_rtp_stats(MediaStream *stream, rtp_stats_t *lstats) {
	if (stream->sessions.rtp_session) {
		const rtp_stats_t *stats = rtp_session_get_stats(stream->sessions.rtp_session);
		memcpy(lstats, stats, sizeof(*stats));
	} else memset(lstats, 0, sizeof(rtp_stats_t));
}

int media_stream_set_dscp(MediaStream *stream, int dscp) {
	ms_message("Setting DSCP to %i for %s stream.", dscp, media_stream_type_str(stream));
	return rtp_session_set_dscp(stream->sessions.rtp_session, dscp);
}

void media_stream_enable_adaptive_bitrate_control(MediaStream *stream, bool_t enabled) {
	stream->use_rc = enabled;
}

void media_stream_enable_adaptive_jittcomp(MediaStream *stream, bool_t enabled) {
	rtp_session_enable_adaptive_jitter_compensation(stream->sessions.rtp_session, enabled);
}

bool_t media_stream_enable_srtp(MediaStream *stream, enum ortp_srtp_crypto_suite_t suite, const char *snd_key, const char *rcv_key) {
	/* Assign new srtp transport to stream->session with 2 Master Keys. */
	RtpTransport *rtp_tpt, *rtcp_tpt;

	if (!ortp_srtp_supported()) {
		ms_error("ortp srtp support not enabled");
		return FALSE;
	}
	if (stream->sessions.srtp_session!=NULL){
		ms_warning("media_stream_enable_srtp(): session already configured");
		return FALSE;
	}

	ms_message("%s: %s stream snd_key='%s' rcv_key='%s'", __FUNCTION__, media_stream_type_str(stream), snd_key, rcv_key);
	stream->sessions.srtp_session = ortp_srtp_create_configure_session(suite, rtp_session_get_send_ssrc(stream->sessions.rtp_session), snd_key, rcv_key);
	if (!stream->sessions.srtp_session) return FALSE;

	// TODO: check who will free rtp_tpt ?
	srtp_transport_new(stream->sessions.srtp_session, &rtp_tpt, &rtcp_tpt);
	rtp_session_set_transports(stream->sessions.rtp_session, rtp_tpt, rtcp_tpt);
	return TRUE;
}

const MSQualityIndicator *media_stream_get_quality_indicator(MediaStream *stream){
	return stream->qi;
}

bool_t ms_is_ipv6(const char *remote) {
	bool_t ret = FALSE;
	struct addrinfo hints, *res0;
	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	err = getaddrinfo(remote,"8000", &hints, &res0);
	if (err != 0) {
		ms_warning("get_local_addr_for: %s", gai_strerror(err));
		return FALSE;
	}
	ret = (res0->ai_addr->sa_family == AF_INET6);
	freeaddrinfo(res0);
	return ret;
}

void mediastream_payload_type_changed(RtpSession *session, unsigned long data) {
	MediaStream *stream = (MediaStream *)data;
	int pt = rtp_session_get_recv_payload_type(stream->sessions.rtp_session);
	media_stream_change_decoder(stream, pt);
}

void media_stream_iterate(MediaStream *stream){
	time_t curtime=ms_time(NULL);
	
	if (stream->is_beginning && (curtime-stream->start_time>15)){
		rtp_session_set_rtcp_report_interval(stream->sessions.rtp_session,5000);
		stream->is_beginning=FALSE;
	}
	if ((curtime-stream->last_bw_sampling_time)>=1) {
		/*update bandwidth stat every second more or less*/
		stream->up_bw=rtp_session_compute_send_bandwidth(stream->sessions.rtp_session);
		stream->down_bw=rtp_session_compute_recv_bandwidth(stream->sessions.rtp_session);
		stream->last_bw_sampling_time=curtime;
	}
	if (stream->ice_check_list) ice_check_list_process(stream->ice_check_list,stream->sessions.rtp_session);
	/*we choose to update the quality indicator as much as possible, since local statistics can be computed realtime. */
	if (stream->qi && curtime>stream->last_iterate_time) ms_quality_indicator_update_local(stream->qi);
	stream->last_iterate_time=curtime;
	if (stream->evq){
		OrtpEvent *ev=ortp_ev_queue_get(stream->evq);
		if (ev!=NULL){
			OrtpEventType evt=ortp_event_get_type(ev);
			if (evt==ORTP_EVENT_RTCP_PACKET_RECEIVED){
				mblk_t *m=ortp_event_get_data(ev)->packet;
				ms_message("stream [%p]: receiving RTCP %s%s",stream,(rtcp_is_SR(m)?"SR":""),(rtcp_is_RR(m)?"RR":""));
				stream->process_rtcp(stream,m);
			}else if (evt==ORTP_EVENT_RTCP_PACKET_EMITTED){
				ms_message("%s_stream_iterate[%p]: local statistics available\n\tLocal's current jitter buffer size:%f ms",
					media_stream_type_str(stream), stream, rtp_session_get_jitter_stats(stream->sessions.rtp_session)->jitter_buffer_size_ms);
			}else if ((evt==ORTP_EVENT_STUN_PACKET_RECEIVED)&&(stream->ice_check_list)){
				ice_handle_stun_packet(stream->ice_check_list,stream->sessions.rtp_session,ortp_event_get_data(ev));
			}
			ortp_event_destroy(ev);
		}
	}
}

float media_stream_get_quality_rating(MediaStream *stream){
	if (stream->qi){
		return ms_quality_indicator_get_rating(stream->qi);
	}
	return -1;
}

float media_stream_get_average_quality_rating(MediaStream *stream){
	if (stream->qi){
		return ms_quality_indicator_get_average_rating(stream->qi);
	}
	return -1;
}

float media_stream_get_lq_quality_rating(MediaStream *stream) {
	if (stream->qi) {
		return ms_quality_indicator_get_lq_rating(stream->qi);
	}
	return -1;
}

float media_stream_get_average_lq_quality_rating(MediaStream *stream) {
	if (stream->qi) {
		return ms_quality_indicator_get_average_lq_rating(stream->qi);
	}
	return -1;
}

int media_stream_set_target_network_bitrate(MediaStream *stream,int target_bitrate) {
	stream->target_bitrate=target_bitrate;
	return 0;
}

int media_stream_get_target_network_bitrate(const MediaStream *stream) {
	return stream->target_bitrate;
}

float media_stream_get_up_bw(const MediaStream *stream) {
	return stream->up_bw;
}

float media_stream_get_down_bw(const MediaStream *stream) {
	return stream->down_bw;
}

void media_stream_reclaim_sessions(MediaStream *stream, MSMediaStreamSessions *sessions){
	memcpy(sessions,&stream->sessions, sizeof(MSMediaStreamSessions));
	stream->owns_sessions=FALSE;
}
