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


#include "ortp/port.h"
#include "ortp/ortp_srtp.h"
#include "ortp/b64.h"

#include "mediastreamer2/mediastream.h"
#include "private.h"

#ifdef ORTP_HAVE_SRTP
#if defined(ANDROID) || defined(WINAPI_FAMILY_PHONE_APP)
// Android and Windows phone don't use make install
#include <srtp_priv.h>
#else
#include <srtp/srtp_priv.h>
#endif

#endif /*ORTP_HAVE_SRTP*/

#include <ctype.h>


#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif


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

		if (stream->type == MSVideo){
			/* Q: why only video ? A: because an audio format can be used at different rates: ex: speex/16000 speex/8000*/
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
	params.prio = __ms_get_default_prio((stream->type == MSVideo) ? TRUE : FALSE);
	stream->sessions.ticker = ms_ticker_new_with_params(&params);
}

const char * media_stream_type_str(MediaStream *stream) {
	return ms_format_type_to_string(stream->type);
}

void ms_media_stream_sessions_uninit(MSMediaStreamSessions *sessions){
	if (sessions->srtp_session) {
		RtpTransport *rtptr=NULL,*rtcptr=NULL;
		ortp_srtp_dealloc(sessions->srtp_session);
		if (sessions->rtp_session){
			rtp_session_get_transports(sessions->rtp_session,&rtptr,&rtcptr);
			rtp_session_set_transports(sessions->rtp_session,NULL,NULL);
			if (rtptr) srtp_transport_destroy(rtptr);
			if (rtcptr) srtp_transport_destroy(rtcptr);
		}
		sessions->srtp_session=NULL;
	}
	if (sessions->rtp_session) {
		rtp_session_destroy(sessions->rtp_session);
		sessions->rtp_session=NULL;
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

#ifdef ORTP_HAVE_SRTP

static int check_srtp_session_created(MediaStream *stream){
	if (stream->sessions.srtp_session==NULL){
		err_status_t err;
		srtp_t session;
		RtpTransport *rtp=NULL,*rtcp=NULL;

		err = ortp_srtp_create(&session, NULL);
		if (err != 0) {
			ms_error("Failed to create srtp session (%d)", err);
			return -1;
		}
		stream->sessions.srtp_session=session;
		srtp_transport_new(session,&rtp,&rtcp);
		rtp_session_set_transports(stream->sessions.rtp_session,rtp,rtcp);
		stream->sessions.is_secured=TRUE;
	}
	return 0;
}

static int add_srtp_stream(srtp_t srtp, MSCryptoSuite suite, uint32_t ssrc, const char* b64_key, bool_t inbound)
{
	srtp_policy_t policy;
	uint8_t* key;
	int key_size;
	err_status_t err;
	unsigned b64_key_length = strlen(b64_key);
	ssrc_t ssrc_conf;

	memset(&policy,0,sizeof(policy));

	switch(suite){
		case MS_AES_128_SHA1_32:
			crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
			// srtp doc says: not adapted to rtcp...
			crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtcp);
			break;
		case MS_AES_128_NO_AUTH:
			crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
			// srtp doc says: not adapted to rtcp...
			crypto_policy_set_aes_cm_128_null_auth(&policy.rtcp);
			break;
		case MS_NO_CIPHER_SHA1_80:
			crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
			crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
			break;
		case MS_AES_128_SHA1_80: /*default mode*/
			crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
			crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
			break;
		case MS_AES_256_SHA1_80:
			crypto_policy_set_aes_cm_256_hmac_sha1_80(&policy.rtp);
			crypto_policy_set_aes_cm_256_hmac_sha1_80(&policy.rtcp);
			break;
		case MS_AES_256_SHA1_32:
			crypto_policy_set_aes_cm_256_hmac_sha1_32(&policy.rtp);
			crypto_policy_set_aes_cm_256_hmac_sha1_32(&policy.rtcp);
			break;
		case MS_CRYPTO_SUITE_INVALID:
			return -1;
			break;
	}
	key_size = b64_decode(b64_key, b64_key_length, 0, 0);
	if (key_size != policy.rtp.cipher_key_len) {
		ortp_error("Key size (%d) doesn't match the selected srtp profile (required %d)",
			key_size,
			policy.rtp.cipher_key_len);
			return -1;
	}
	key = (uint8_t*) ortp_malloc0(key_size+2); /*srtp uses padding*/
	if (b64_decode(b64_key, b64_key_length, key, key_size) != key_size) {
		ortp_error("Error decoding key");
		ortp_free(key);
		return -1;
	}
	if (!inbound)
		policy.allow_repeat_tx=1; /*necessary for telephone-events*/

	/*ssrc_conf.type=inbound ? ssrc_any_inbound : ssrc_specific;*/
	ssrc_conf.type=ssrc_specific;
	ssrc_conf.value=ssrc;

	policy.ssrc = ssrc_conf;
	policy.key = key;
	policy.next = NULL;

	err = srtp_add_stream(srtp, &policy);
	if (err != err_status_ok) {
		ortp_error("Failed to add stream to srtp session (%d)", err);
		ortp_free(key);
		return -1;
	}

	ortp_free(key);
	return 0;
}

static uint32_t find_other_ssrc(srtp_t srtp, uint32_t ssrc){
	srtp_stream_ctx_t *stream;
	for (stream=srtp->stream_list;stream!=NULL;stream=stream->next){
		if (stream->ssrc!=ssrc) return stream->ssrc;
	}
	return 0;
}

#endif

#ifdef ORTP_HAVE_SRTP
#define _ORTP_HAVE_SRTP 1
#else
#define _ORTP_HAVE_SRTP 0
#endif

bool_t media_stream_srtp_supported(void){
	return _ORTP_HAVE_SRTP & ortp_srtp_supported();
}

int media_stream_set_srtp_recv_key(MediaStream *stream, MSCryptoSuite suite, const char* key){

	if (!media_stream_srtp_supported()) {
		ms_error("ortp srtp support disabled in oRTP or mediastreamer2");
		return -1;
	}
#ifdef ORTP_HAVE_SRTP
	{
		uint32_t ssrc,send_ssrc;
		bool_t updated=FALSE;

		if (check_srtp_session_created(stream)==-1)
			return -1;

		/*check if a previous key was configured, in which case remove it*/
		send_ssrc=rtp_session_get_send_ssrc(stream->sessions.rtp_session);
		ssrc=find_other_ssrc(stream->sessions.srtp_session,htonl(send_ssrc));

		/*careful: remove_stream takes the SSRC in network byte order...*/
		if (ortp_srtp_remove_stream(stream->sessions.srtp_session,ssrc)==0)
			updated=TRUE;
		ssrc=rtp_session_get_recv_ssrc(stream->sessions.rtp_session);
		ms_message("media_stream_set_srtp_recv_key(): %s key %s",updated ? "changing to" : "starting with", key);
		return add_srtp_stream(stream->sessions.srtp_session,suite,ssrc,key,TRUE);
	}
#else
	return -1;
#endif
}

int media_stream_set_srtp_send_key(MediaStream *stream, MSCryptoSuite suite, const char* key){

	if (!media_stream_srtp_supported()) {
		ms_error("ortp srtp support disabled in oRTP or mediastreamer2");
		return -1;
	}

#ifdef ORTP_HAVE_SRTP
	{
		uint32_t ssrc;
		bool_t updated=FALSE;

		if (check_srtp_session_created(stream)==-1)
			return -1;

		/*check if a previous key was configured, in which case remove it*/
		ssrc=rtp_session_get_send_ssrc(stream->sessions.rtp_session);
		if (ssrc!=0){
			/*careful: remove_stream takes the SSRC in network byte order...*/
			if (ortp_srtp_remove_stream(stream->sessions.srtp_session,htonl(ssrc))==0)
				updated=TRUE;
		}
		ms_message("media_stream_set_srtp_send_key(): %s key %s",updated ? "changing to" : "starting with", key);
		return add_srtp_stream(stream->sessions.srtp_session,suite,ssrc,key,FALSE);
	}
#else
	return -1;
#endif
}

/*deprecated*/
bool_t media_stream_enable_srtp(MediaStream *stream, MSCryptoSuite suite, const char *snd_key, const char *rcv_key) {
	return media_stream_set_srtp_recv_key(stream,suite,rcv_key)==0 && media_stream_set_srtp_send_key(stream,suite,snd_key)==0;
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

static void media_stream_process_rtcp(MediaStream *stream, mblk_t *m, time_t curtime){
	stream->last_packet_time=curtime;
	ms_message("%s stream [%p]: receiving RTCP %s%s",media_stream_type_str(stream),stream,(rtcp_is_SR(m)?"SR":""),(rtcp_is_RR(m)?"RR":""));
	do{
		if (stream->use_rc&&stream->rc) ms_bitrate_controller_process_rtcp(stream->rc,m);
		if (stream->qi) ms_quality_indicator_update_from_feedback(stream->qi,m);
		stream->process_rtcp(stream,m);
	}while(rtcp_next_packet(m));

}

void media_stream_iterate(MediaStream *stream){
	time_t curtime=ms_time(NULL);

	if (stream->ice_check_list) ice_check_list_process(stream->ice_check_list,stream->sessions.rtp_session);
	/*we choose to update the quality indicator as much as possible, since local statistics can be computed realtime. */
	if (stream->state==MSStreamStarted){
		if (stream->qi && curtime>stream->last_iterate_time) ms_quality_indicator_update_local(stream->qi);
	}
	stream->last_iterate_time=curtime;

	if (stream->rc) ms_bitrate_controller_update(stream->rc);

	if (stream->evq){
		OrtpEvent *ev=NULL;

		while ((ev=ortp_ev_queue_get(stream->evq))!=NULL){
			OrtpEventType evt=ortp_event_get_type(ev);
			if (evt==ORTP_EVENT_RTCP_PACKET_RECEIVED){
				mblk_t *m=ortp_event_get_data(ev)->packet;
				media_stream_process_rtcp(stream,m,curtime);
			}else if (evt==ORTP_EVENT_RTCP_PACKET_EMITTED){
				ms_message("%s_stream_iterate[%p]: local statistics available\n\tLocal's current jitter buffer size:%f ms",
					media_stream_type_str(stream), stream, rtp_session_get_jitter_stats(stream->sessions.rtp_session)->jitter_buffer_size_ms);
			}else if ((evt==ORTP_EVENT_STUN_PACKET_RECEIVED)&&(stream->ice_check_list)){
				ice_handle_stun_packet(stream->ice_check_list,stream->sessions.rtp_session,ortp_event_get_data(ev));
			} else if (evt == ORTP_EVENT_ZRTP_ENCRYPTION_CHANGED) {
				OrtpEventData *evd=ortp_event_get_data(ev);
				stream->sessions.is_secured=evd->info.zrtp_stream_encrypted;
				ms_message("%s_stream_iterate[%p]: is %s ",media_stream_type_str(stream) , stream, stream->sessions.is_secured ? "encrypted" : "not encrypted");
			}
			ortp_event_destroy(ev);
		}
	}
}

bool_t media_stream_alive(MediaStream *ms, int timeout){
	const rtp_stats_t *stats=rtp_session_get_stats(ms->sessions.rtp_session);
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
	return rtp_session_get_send_bandwidth(stream->sessions.rtp_session);
}

float media_stream_get_down_bw(const MediaStream *stream) {
	return rtp_session_get_recv_bandwidth(stream->sessions.rtp_session);
}

void media_stream_reclaim_sessions(MediaStream *stream, MSMediaStreamSessions *sessions){
	memcpy(sessions,&stream->sessions, sizeof(MSMediaStreamSessions));
	stream->owns_sessions=FALSE;
}

bool_t media_stream_secured (const MediaStream *stream) {
	return stream->sessions.is_secured;
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
	}else if ( keywordcmp ( "AES_CM_256_HMAC_SHA1_32",name ) == 0 ){
		if (parameters && strstr(parameters,"UNENCRYPTED_SRTP")) goto error;
		if (parameters && strstr(parameters,"UNAUTHENTICATED_SRTP")) goto error;
		return MS_AES_256_SHA1_32;
	}else if ( keywordcmp ( "AES_CM_256_HMAC_SHA1_80",name ) == 0 ){
		if (parameters && strstr(parameters,"UNENCRYPTED_SRTP")) goto error;
		if (parameters && strstr(parameters,"UNAUTHENTICATED_SRTP")) goto error;
		return MS_AES_256_SHA1_80;
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
			params->name="AES_CM_256_HMAC_SHA1_80";
			break;
		case MS_AES_256_SHA1_32:
			params->name="AES_CM_256_HMAC_SHA1_32";
			break;
	}
	if (params->name==NULL) return -1;
	return 0;
}


