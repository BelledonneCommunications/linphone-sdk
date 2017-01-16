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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "ortp/port.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msrtp.h"
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

static int _ms_ticker_prio_from_env(const char *penv, MSTickerPrio *prio) {
	if (strcasecmp(penv, "NORMAL") == 0) {
		*prio = MS_TICKER_PRIO_NORMAL;
		return 0;
	}
	if (strcasecmp(penv, "HIGH") == 0) {
		*prio = MS_TICKER_PRIO_HIGH;
		return 0;
	}
	if (strcasecmp(penv, "REALTIME") == 0) {
		*prio = MS_TICKER_PRIO_REALTIME;
		return 0;
	}
	ms_error("Undefined priority %s", penv);
	return -1;
}

MSTickerPrio __ms_get_default_prio(bool_t is_video) {
	const char *penv = NULL;
	MSTickerPrio prio;

	if (is_video) {
#ifndef MS2_WINDOWS_UNIVERSAL
		penv = getenv("MS_VIDEO_PRIO");
#endif
		if(penv && _ms_ticker_prio_from_env(penv, &prio) == 0) return prio;

#ifdef __ios
		return MS_TICKER_PRIO_HIGH;
#else
		return MS_TICKER_PRIO_NORMAL;
#endif
	} else {
#ifndef MS2_WINDOWS_UNIVERSAL
		penv = getenv("MS_AUDIO_PRIO");
#endif
		if (penv && _ms_ticker_prio_from_env(penv, &prio) == 0) return prio;

		return MS_TICKER_PRIO_HIGH;

	}
}

void media_stream_init(MediaStream *stream, MSFactory *factory, const MSMediaStreamSessions *sessions) {
	stream->sessions = *sessions;
	
	stream->evd = ortp_ev_dispatcher_new(stream->sessions.rtp_session);
	stream->evq = ortp_ev_queue_new();
	stream->factory = factory; /*the factory is used later to instanciate everything in mediastreamer2.*/
	rtp_session_register_event_queue(stream->sessions.rtp_session, stream->evq);
	
	/*we give to the zrtp and dtls sessions a backpointer to all the stream sessions*/
	if (sessions->zrtp_context != NULL) {
		ms_zrtp_set_stream_sessions(sessions->zrtp_context, &stream->sessions);
	}
	if (sessions->dtls_context != NULL) {
		ms_dtls_srtp_set_stream_sessions(sessions->dtls_context, &stream->sessions);
	}
}

RtpSession * ms_create_duplex_rtp_session(const char* local_ip, int loc_rtp_port, int loc_rtcp_port, int mtu) {
	RtpSession *rtpr;

	rtpr = rtp_session_new(RTP_SESSION_SENDRECV);
	rtp_session_set_recv_buf_size(rtpr, MAX(mtu , MS_MINIMAL_MTU));
	rtp_session_set_scheduling_mode(rtpr, 0);
	rtp_session_set_blocking_mode(rtpr, 0);
	rtp_session_enable_adaptive_jitter_compensation(rtpr, TRUE);
	rtp_session_set_symmetric_rtp(rtpr, TRUE);
	rtp_session_set_local_addr(rtpr, local_ip, loc_rtp_port, loc_rtcp_port);
	rtp_session_signal_connect(rtpr, "timestamp_jump", (RtpCallback)rtp_session_resync, NULL);
	rtp_session_signal_connect(rtpr, "ssrc_changed", (RtpCallback)rtp_session_resync, NULL);
	rtp_session_set_ssrc_changed_threshold(rtpr, 0);
	rtp_session_set_rtcp_report_interval(rtpr, 2500);	/* At the beginning of the session send more reports. */
	rtp_session_set_multicast_loopback(rtpr,TRUE); /*very useful, specially for testing purposes*/
	rtp_session_set_send_ts_offset(rtpr, (uint32_t)bctbx_random());
	disable_checksums(rtp_session_get_rtp_socket(rtpr));
	return rtpr;
}

int media_stream_join_multicast_group(MediaStream *stream, const char *ip){
	return rtp_session_join_multicast_group(stream->sessions.rtp_session,ip);
}

void media_stream_start_ticker(MediaStream *stream) {
	MSTickerParams params = {0};
	char name[32] = {0};

	if (stream->sessions.ticker) return;
	snprintf(name, sizeof(name) - 1, "%s MSTicker", media_stream_type_str(stream));
	name[0] = toupper(name[0]);
	params.name = name;
	params.prio = __ms_get_default_prio((stream->type == MSVideo) ? TRUE : FALSE);
	stream->sessions.ticker = ms_ticker_new_with_params(&params);
}

const char * media_stream_type_str(MediaStream *stream) {
	return ms_format_type_to_string(stream->type);
}

void ms_media_stream_sessions_uninit(MSMediaStreamSessions *sessions){
	if (sessions->srtp_context) {
		ms_srtp_context_delete(sessions->srtp_context);
		sessions->srtp_context=NULL;
	}
	if (sessions->rtp_session) {
		rtp_session_destroy(sessions->rtp_session);
		sessions->rtp_session=NULL;
	}
	if (sessions->zrtp_context != NULL) {
		ms_zrtp_context_destroy(sessions->zrtp_context);
		sessions->zrtp_context = NULL;
	}
	if (sessions->dtls_context != NULL) {
		ms_dtls_srtp_context_destroy(sessions->dtls_context);
		sessions->dtls_context = NULL;
	}
	if (sessions->ticker){
		ms_ticker_destroy(sessions->ticker);
		sessions->ticker=NULL;
	}
}

void media_stream_free(MediaStream *stream) {
	if (stream->sessions.zrtp_context != NULL) {
		ms_zrtp_set_stream_sessions(stream->sessions.zrtp_context, NULL);
	}
	if (stream->sessions.dtls_context != NULL) {
		ms_dtls_srtp_set_stream_sessions(stream->sessions.dtls_context, NULL);
	}
	
	if (stream->sessions.rtp_session != NULL) rtp_session_unregister_event_queue(stream->sessions.rtp_session, stream->evq);
	if (stream->evq != NULL) ortp_ev_queue_destroy(stream->evq);
	if (stream->evd != NULL) ortp_ev_dispatcher_destroy(stream->evd);
	if (stream->owns_sessions) ms_media_stream_sessions_uninit(&stream->sessions);
	if (stream->rc != NULL) ms_bitrate_controller_destroy(stream->rc);
	if (stream->rtpsend != NULL) ms_filter_destroy(stream->rtpsend);
	if (stream->rtprecv != NULL) ms_filter_destroy(stream->rtprecv);
	if (stream->encoder != NULL) ms_filter_destroy(stream->encoder);
	if (stream->decoder != NULL) ms_filter_destroy(stream->decoder);
	if (stream->voidsink != NULL) ms_filter_destroy(stream->voidsink);
	if (stream->qi) ms_quality_indicator_destroy(stream->qi);
}

bool_t media_stream_started(MediaStream *stream) {
	return stream->start_time != 0;
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
	stream->dscp = dscp;
	if ((stream->sessions.rtp_session != NULL) && (stream->sessions.rtp_session->rtp.gs.rem_addr.ss_family != AF_UNSPEC))
		return rtp_session_set_dscp(stream->sessions.rtp_session, dscp);
	return 0;
}

void media_stream_enable_adaptive_bitrate_control(MediaStream *stream, bool_t enabled) {
	stream->rc_enable = enabled;
}

void media_stream_set_adaptive_bitrate_algorithm(MediaStream *stream, MSQosAnalyzerAlgorithm algorithm) {
	stream->rc_algorithm = algorithm;
}

void media_stream_enable_adaptive_jittcomp(MediaStream *stream, bool_t enabled) {
	rtp_session_enable_adaptive_jitter_compensation(stream->sessions.rtp_session, enabled);
}

void media_stream_enable_dtls(MediaStream *stream, MSDtlsSrtpParams *params){
	if (stream->sessions.dtls_context==NULL) {
		ms_message("Start DTLS media stream context in stream session [%p]", &(stream->sessions));
		stream->sessions.dtls_context=ms_dtls_srtp_context_new(&(stream->sessions), params);
	}
}

void media_stream_set_ice_check_list(MediaStream *stream, IceCheckList *cl) {
	bool_t stun_enabled = TRUE;
	stream->ice_check_list = cl;
	if (stream->ice_check_list != NULL) {
		ice_check_list_set_rtp_session(stream->ice_check_list, stream->sessions.rtp_session);
		stun_enabled = FALSE;
	}
	if (stream->rtpsend != NULL) {
		ms_filter_call_method(stream->rtpsend, MS_RTP_SEND_ENABLE_STUN, &stun_enabled);
	}
}

bool_t media_stream_dtls_supported(void){
	return ms_dtls_srtp_available();
}

/*deprecated*/
bool_t media_stream_enable_srtp(MediaStream *stream, MSCryptoSuite suite, const char *snd_key, const char *rcv_key) {
	return ms_media_stream_sessions_set_srtp_recv_key_b64(&stream->sessions,suite,rcv_key)==0 && ms_media_stream_sessions_set_srtp_send_key_b64(&stream->sessions,suite,snd_key)==0;
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
	hints.ai_flags = AI_NUMERICHOST;
	err = getaddrinfo(remote,"8000", &hints, &res0);
	if (err != 0) {
		ms_warning("ms_is_ipv6(%s): %s", remote, gai_strerror(err));
		return FALSE;
	}
	ret = (res0->ai_addr->sa_family == AF_INET6);
	freeaddrinfo(res0);
	return ret;
}

bool_t ms_is_multicast_addr(const struct sockaddr *addr) {
	return ortp_is_multicast_addr(addr);
}

bool_t ms_is_multicast(const char *address) {
	bool_t ret = FALSE;
	struct addrinfo hints, *res0;
	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICHOST;
	err = getaddrinfo(address,"8000", &hints, &res0);
	if (err != 0) {
		ms_warning("ms_is_multicast(%s): %s", address, gai_strerror(err));
		return FALSE;
	}
	ret = ms_is_multicast_addr(res0->ai_addr);

	freeaddrinfo(res0);
	return ret;
}

static void media_stream_process_rtcp(MediaStream *stream, mblk_t *m, time_t curtime){
	stream->last_packet_time=curtime;
	ms_message("%s stream [%p]: receiving RTCP %s%s",media_stream_type_str(stream),stream,(rtcp_is_SR(m)?"SR":""),(rtcp_is_RR(m)?"RR":""));
	do{
		if (stream->rc_enable && stream->rc) ms_bitrate_controller_process_rtcp(stream->rc,m);
		if (stream->qi) ms_quality_indicator_update_from_feedback(stream->qi,m);
		stream->process_rtcp(stream,m);
	}while(rtcp_next_packet(m));
}

void media_stream_iterate(MediaStream *stream){
	time_t curtime=ms_time(NULL);

	if (stream->ice_check_list) ice_check_list_process(stream->ice_check_list,stream->sessions.rtp_session);
	/*we choose to update the quality indicator as much as possible, since local statistics can be computed realtime. */
	if (stream->state==MSStreamStarted){
		if (stream->is_beginning && (curtime-stream->start_time>15)){
			rtp_session_set_rtcp_report_interval(stream->sessions.rtp_session,5000);
			stream->is_beginning=FALSE;
		}
		if (stream->qi && curtime>stream->last_iterate_time) ms_quality_indicator_update_local(stream->qi);
	}
	stream->last_iterate_time=curtime;

	if (stream->rc) ms_bitrate_controller_update(stream->rc);

	if (stream->evd) {
		ortp_ev_dispatcher_iterate(stream->evd);
	}

	if (stream->evq){
		OrtpEvent *ev=NULL;

		while ((ev=ortp_ev_queue_get(stream->evq))!=NULL){
			OrtpEventType evt=ortp_event_get_type(ev);
			if (evt==ORTP_EVENT_RTCP_PACKET_RECEIVED){
				mblk_t *m=ortp_event_get_data(ev)->packet;
				media_stream_process_rtcp(stream,m,curtime);
			}else if (evt==ORTP_EVENT_RTCP_PACKET_EMITTED){
				ms_message("%s_stream_iterate[%p], local statistics available:"
							"\n\tLocal current jitter buffer size: %5.1fms",
					media_stream_type_str(stream), stream, rtp_session_get_jitter_stats(stream->sessions.rtp_session)->jitter_buffer_size_ms);
			} else if (evt==ORTP_EVENT_STUN_PACKET_RECEIVED && stream->ice_check_list){
				ice_handle_stun_packet(stream->ice_check_list,stream->sessions.rtp_session,ortp_event_get_data(ev));
			} else if ((evt == ORTP_EVENT_ZRTP_ENCRYPTION_CHANGED) || (evt == ORTP_EVENT_DTLS_ENCRYPTION_CHANGED)) {
				ms_message("%s_stream_iterate[%p]: is %s ",media_stream_type_str(stream) , stream, media_stream_secured(stream) ? "encrypted" : "not encrypted");
			}
			ortp_event_destroy(ev);
		}
	}
}

bool_t media_stream_alive(MediaStream *ms, int timeout){
	const rtp_stats_t *stats;

	if (ms->state!=MSStreamStarted){
		return TRUE;
	}
	stats=rtp_session_get_stats(ms->sessions.rtp_session);
	if (stats->recv!=0){
		if (stats->recv!=ms->last_packet_count){
			ms->last_packet_count=stats->recv;
			ms->last_packet_time=ms_time(NULL);
		}
	}
	if (ms_time(NULL)-ms->last_packet_time>timeout){
		/* more than timeout seconds of inactivity*/
		return FALSE;
	}
	return TRUE;
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
	return rtp_session_get_rtp_send_bandwidth(stream->sessions.rtp_session);
}

float media_stream_get_down_bw(const MediaStream *stream) {
	return rtp_session_get_rtp_recv_bandwidth(stream->sessions.rtp_session);
}

float media_stream_get_rtcp_up_bw(const MediaStream *stream) {
	return rtp_session_get_rtcp_send_bandwidth(stream->sessions.rtp_session);
}

float media_stream_get_rtcp_down_bw(const MediaStream *stream) {
	return rtp_session_get_rtcp_recv_bandwidth(stream->sessions.rtp_session);
}

void media_stream_reclaim_sessions(MediaStream *stream, MSMediaStreamSessions *sessions){
	memcpy(sessions,&stream->sessions, sizeof(MSMediaStreamSessions));
	stream->owns_sessions=FALSE;
}

bool_t media_stream_secured (const MediaStream *stream) {
	if (stream->state != MSStreamStarted)
		return FALSE;

	switch (stream->type) {
		case MSAudio:
		case MSText:
			/*fixme need also audio stream direction to be more precise*/
			return ms_media_stream_sessions_secured(&stream->sessions, MediaStreamSendRecv);
		case MSVideo:{
			VideoStream *vs = (VideoStream*)stream;
			return ms_media_stream_sessions_secured(&stream->sessions, vs->dir);
		}
		case MSUnknownMedia:
		break;
	}
	return FALSE;
}

bool_t media_stream_avpf_enabled(const MediaStream *stream) {
	return rtp_session_avpf_enabled(stream->sessions.rtp_session);
}

uint16_t media_stream_get_avpf_rr_interval(const MediaStream *stream) {
	return rtp_session_get_avpf_rr_interval(stream->sessions.rtp_session);
}

MSStreamState media_stream_get_state(const MediaStream *stream) {
	return stream->state;
}

RtpSession * media_stream_get_rtp_session(const MediaStream *stream) {
	return stream->sessions.rtp_session;
}

#define keywordcmp(key,b) strncmp(key,b,sizeof(key))

/* see  http://www.iana.org/assignments/sdp-security-descriptions/sdp-security-descriptions.xhtml#sdp-security-descriptions-3 */



MSCryptoSuite ms_crypto_suite_build_from_name_params(const MSCryptoSuiteNameParams *descrption){
	const char *name=descrption->name, *parameters=descrption->params;
	if (keywordcmp ( "AES_CM_128_HMAC_SHA1_80",name ) == 0 ){
		if (parameters && strstr(parameters,"UNENCRYPTED_SRTP")) return MS_NO_CIPHER_SHA1_80;
		else if (parameters && strstr(parameters,"UNAUTHENTICATED_SRTP")) return MS_AES_128_NO_AUTH;
		else return MS_AES_128_SHA1_80;
	}else if ( keywordcmp ( "AES_CM_128_HMAC_SHA1_32",name ) == 0 ){
		if (parameters && strstr(parameters,"UNENCRYPTED_SRTP")) goto error;
		if (parameters && strstr(parameters,"UNAUTHENTICATED_SRTP")) return MS_AES_128_NO_AUTH;
		else return MS_AES_128_SHA1_32;
	}else if ( keywordcmp ("AES_256_CM_HMAC_SHA1_32", name) == 0 ){
		if (parameters && strstr(parameters,"UNENCRYPTED_SRTP")) goto error;
		if (parameters && strstr(parameters,"UNAUTHENTICATED_SRTP")) goto error;
		return MS_AES_256_SHA1_32;
	}else if ( keywordcmp ("AES_256_CM_HMAC_SHA1_80", name) == 0 ){
		if (parameters && strstr(parameters,"UNENCRYPTED_SRTP")) goto error;
		if (parameters && strstr(parameters,"UNAUTHENTICATED_SRTP")) goto error;
		return MS_AES_256_SHA1_80;
	}else if ( keywordcmp ("AES_CM_256_HMAC_SHA1_80", name) == 0 ){
        if (parameters && strstr(parameters,"UNENCRYPTED_SRTP")) goto error;
        if (parameters && strstr(parameters,"UNAUTHENTICATED_SRTP")) goto error;
        return MS_AES_CM_256_SHA1_80;
    }
error:
    ms_error("Unsupported crypto suite '%s' with parameters '%s'",name, parameters ? parameters : "");
    return MS_CRYPTO_SUITE_INVALID;
}

int ms_crypto_suite_to_name_params(MSCryptoSuite cs, MSCryptoSuiteNameParams *params ){
     params->name=NULL;
     params->params=NULL;
     switch(cs){
        case MS_CRYPTO_SUITE_INVALID:
     		break;
        case MS_AES_128_SHA1_80:
     		params->name= "AES_CM_128_HMAC_SHA1_80";
     		break;
     	case MS_AES_128_SHA1_32:
     		params->name="AES_CM_128_HMAC_SHA1_32";
     		break;
     	case MS_AES_128_NO_AUTH:
     		params->name="AES_CM_128_HMAC_SHA1_80";
     		params->params="UNAUTHENTICATED_SRTP";
     		break;
     	case MS_NO_CIPHER_SHA1_80:
     		params->name="AES_CM_128_HMAC_SHA1_80";
     		params->params="UNENCRYPTED_SRTP UNENCRYPTED_SRTCP";
     		break;
     	case MS_AES_256_SHA1_80:
     		params->name="AES_256_CM_HMAC_SHA1_80";
     		break;
     	case MS_AES_CM_256_SHA1_80:
     	    params->name="AES_CM_256_HMAC_SHA1_80";
     	    break;
     	case MS_AES_256_SHA1_32:
     		params->name= "AES_256_CM_HMAC_SHA1_32";
     		break;
     }
     if (params->name==NULL) return -1;
     return 0;
}

OrtpEvDispatcher* media_stream_get_event_dispatcher(const MediaStream *stream) {
	return stream->evd;
}

const char *ms_resource_type_to_string(MSResourceType type){
	switch(type){
		case MSResourceDefault:
			return "MSResourceDefault";
		case MSResourceInvalid:
			return "MSResourceInvalid";
		case MSResourceCamera:
			return "MSResourceCamera";
		case MSResourceFile:
			return "MSResourceFile";
		case MSResourceRtp:
			return "MSResourceRtp";
		case MSResourceSoundcard:
			return "MSResourceSoundcard";
	}
	return "INVALID";
}

bool_t ms_media_resource_is_consistent(const MSMediaResource *r){
	switch(r->type){
		case MSResourceCamera:
		case MSResourceRtp:
		case MSResourceSoundcard:
			if (r->resource_arg == NULL){
				ms_error("No resource argument specified for resource type %s", ms_resource_type_to_string(r->type));
				return FALSE;
			}
			return TRUE;
		break;
		case MSResourceFile:
			/*setting up file player/recorder without specifying the file to play immediately is allowed*/
		case MSResourceDefault:
			return TRUE;
		case MSResourceInvalid:
			ms_error("Invalid resource type specified");
			return FALSE;
	}
	ms_error("Unsupported media resource type [%i]", (int)r->type);  
	return FALSE;
}

bool_t ms_media_stream_io_is_consistent(const MSMediaStreamIO *io){
	return ms_media_resource_is_consistent(&io->input) && ms_media_resource_is_consistent(&io->output);
}

/*stubs*/
#ifndef VIDEO_ENABLED
void video_stream_open_player(VideoStream *stream, MSFilter *sink){
}

void video_stream_close_player(VideoStream *stream){
}

const char *video_stream_get_default_video_renderer(void){
	return NULL;
}

MSWebCamDesc *ms_mire_webcam_desc_get(void){
	return NULL;
}

#endif

