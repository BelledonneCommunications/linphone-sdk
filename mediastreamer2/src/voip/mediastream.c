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
	RtpSession *session = stream->session;
	RtpProfile *prof = rtp_session_get_profile(session);
	PayloadType *pt = rtp_profile_get_payload(prof, payload);
	if (pt != NULL){
		MSFilter *dec;

		if ((stream->type == VideoStreamType)
			&& (stream->decoder != NULL) && (stream->decoder->desc->enc_fmt != NULL)
			&& (strcasecmp(pt->mime_type, stream->decoder->desc->enc_fmt) == 0)) {
			/* Same formats behind different numbers, nothing to do. */
			return;
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
			ms_filter_preprocess(stream->decoder,stream->ticker);
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
	rtp_session_set_recv_buf_size(rtpr, ms_get_mtu());
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

void start_ticker(MediaStream *stream) {
	MSTickerParams params = {0};
	char name[16];

	snprintf(name, sizeof(name) - 1, "%s MSTicker", media_stream_type_str(stream));
	name[0] = toupper(name[0]);
	params.name = name;
	params.prio = __ms_get_default_prio((stream->type == VideoStreamType) ? TRUE : FALSE);
	stream->ticker = ms_ticker_new_with_params(&params);
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

void media_stream_free(MediaStream *stream) {
	if (stream->zrtp_context != NULL) {
		ortp_zrtp_context_destroy(stream->zrtp_context);
		stream->zrtp_context = NULL;
	}
	if (stream->session != NULL) {
		rtp_session_unregister_event_queue(stream->session, stream->evq);
		rtp_session_destroy(stream->session);
	}
	if (stream->evq) ortp_ev_queue_destroy(stream->evq);
	if (stream->rc != NULL) ms_bitrate_controller_destroy(stream->rc);
	if (stream->rtpsend != NULL) ms_filter_destroy(stream->rtpsend);
	if (stream->rtprecv != NULL) ms_filter_destroy(stream->rtprecv);
	if (stream->encoder != NULL) ms_filter_destroy(stream->encoder);
	if (stream->decoder != NULL) ms_filter_destroy(stream->decoder);
	if (stream->voidsink != NULL) ms_filter_destroy(stream->voidsink);
	if (stream->ticker != NULL) ms_ticker_destroy(stream->ticker);
	if (stream->qi) ms_quality_indicator_destroy(stream->qi);
}

void media_stream_set_rtcp_information(MediaStream *stream, const char *cname, const char *tool) {
	if (stream->session != NULL) {
		rtp_session_set_source_description(stream->session, cname, NULL, NULL, NULL, NULL, tool, NULL);
	}
}

void media_stream_get_local_rtp_stats(MediaStream *stream, rtp_stats_t *lstats) {
	if (stream->session) {
		const rtp_stats_t *stats = rtp_session_get_stats(stream->session);
		memcpy(lstats, stats, sizeof(*stats));
	} else memset(lstats, 0, sizeof(rtp_stats_t));
}

int media_stream_set_dscp(MediaStream *stream, int dscp) {
	ms_message("Setting DSCP to %i for %s stream.", dscp, media_stream_type_str(stream));
	return rtp_session_set_dscp(stream->session, dscp);
}

void media_stream_enable_adaptive_bitrate_control(MediaStream *stream, bool_t enabled) {
	stream->use_rc = enabled;
}

void media_stream_enable_adaptive_jittcomp(MediaStream *stream, bool_t enabled) {
	rtp_session_enable_adaptive_jitter_compensation(stream->session, enabled);
}

bool_t media_stream_enable_srtp(MediaStream *stream, enum ortp_srtp_crypto_suite_t suite, const char *snd_key, const char *rcv_key) {
	/* Assign new srtp transport to stream->session with 2 Master Keys. */
	RtpTransport *rtp_tpt, *rtcp_tpt;

	if (!ortp_srtp_supported()) {
		ms_error("ortp srtp support not enabled");
		return FALSE;
	}

	ms_message("%s: %s stream snd_key='%s' rcv_key='%s'", __FUNCTION__, media_stream_type_str(stream), snd_key, rcv_key);
	stream->srtp_session = ortp_srtp_create_configure_session(suite, rtp_session_get_send_ssrc(stream->session), snd_key, rcv_key);
	if (!stream->srtp_session) return FALSE;

	// TODO: check who will free rtp_tpt ?
	srtp_transport_new(stream->srtp_session, &rtp_tpt, &rtcp_tpt);
	rtp_session_set_transports(stream->session, rtp_tpt, rtcp_tpt);
	return TRUE;
}

const MSQualityIndicator *media_stream_get_quality_indicator(MediaStream *stream){
	return stream->qi;
}

bool_t ms_is_ipv6(const char *remote) {
	bool_t ret = FALSE;
#ifdef INET6
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
#endif
	return ret;
}

void mediastream_payload_type_changed(RtpSession *session, unsigned long data) {
	MediaStream *stream = (MediaStream *)data;
	int pt = rtp_session_get_recv_payload_type(stream->session);
	media_stream_change_decoder(stream, pt);
}

void media_stream_iterate(MediaStream *stream){
	time_t curtime=ms_time(NULL);
	
	if (stream->is_beginning && (curtime-stream->start_time>15)){
		rtp_session_set_rtcp_report_interval(stream->session,5000);
		stream->is_beginning=FALSE;
	}
	if (stream->ice_check_list) ice_check_list_process(stream->ice_check_list,stream->session);
	/*we choose to update the quality indicator as much as possible, since local statistics can be computed realtime. */
	if (stream->qi && curtime>stream->last_iterate_time) ms_quality_indicator_update_local(stream->qi);
	stream->last_iterate_time=curtime;
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


