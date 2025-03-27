/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include <bctoolbox/defs.h>

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msrtp.h"
#include "ortp/port.h"
#include "private.h"
#include <ctype.h>
#include <limits.h>

#if __APPLE__
#include "TargetConditionals.h"
#endif

#ifndef MS_MINIMAL_MTU
/*this is used for determining the minimum size of recv buffers for RTP packets
 Keep 1500 for maximum interoparibility*/
#define MS_MINIMAL_MTU 1500
#endif

static const char *media_stream_id = "media-stream-id";

#if defined(_WIN32_WCE)
time_t ms_time(time_t *t) {
	DWORD timemillis = GetTickCount();
	if (timemillis > 0) {
		if (t != NULL) *t = timemillis / 1000;
	}
	return timemillis / 1000;
}
#endif

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif // _MSC_VER
static void disable_checksums(ortp_socket_t sock) {
#if defined(DISABLE_CHECKSUMS) && defined(SO_NO_CHECK)
	int option = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_NO_CHECK, &option, sizeof(option)) == -1) {
		ms_warning("Could not disable udp checksum: %s", strerror(errno));
	}
#endif
}
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif // _MSC_VER

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
		if (penv && _ms_ticker_prio_from_env(penv, &prio) == 0) return prio;

		return MS_TICKER_PRIO_HIGH;
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
	media_stream_add_tmmbr_handler(stream, media_stream_tmmbr_received, stream);
	media_stream_add_goog_remb_handler(stream, media_stream_goog_remb_received, stream);
	stream->stun_allowed = TRUE;
	stream->transfer_mode = FALSE;
}

void media_stream_set_log_tag(MediaStream *stream, const char *log_tag) {
	if (stream->log_tag) bctbx_free(stream->log_tag);
	stream->log_tag = bctbx_strdup(log_tag);
}

void media_stream_add_tmmbr_handler(MediaStream *stream,
                                    void (*on_tmmbr_received)(const OrtpEventData *evd, void *),
                                    void *user_data) {
	ortp_ev_dispatcher_connect(stream->evd, ORTP_EVENT_RTCP_PACKET_RECEIVED, RTCP_RTPFB,
	                           (OrtpEvDispatcherCb)on_tmmbr_received, user_data);
}

void media_stream_remove_tmmbr_handler(MediaStream *stream,
                                       void (*on_tmmbr_received)(const OrtpEventData *evd, void *),
                                       BCTBX_UNUSED(void *user_data)) {
	ortp_ev_dispatcher_disconnect(stream->evd, ORTP_EVENT_RTCP_PACKET_RECEIVED, RTCP_RTPFB,
	                              (OrtpEvDispatcherCb)on_tmmbr_received);
}

void media_stream_add_goog_remb_handler(MediaStream *stream,
                                        void (*on_goog_remb_received)(const OrtpEventData *evd, void *),
                                        void *user_data) {
	ortp_ev_dispatcher_connect(stream->evd, ORTP_EVENT_RTCP_PACKET_RECEIVED, RTCP_PSFB,
	                           (OrtpEvDispatcherCb)on_goog_remb_received, user_data);
}

void media_stream_remove_goog_remb_handler(MediaStream *stream,
                                           void (*on_goog_remb_received)(const OrtpEventData *evd, void *),
                                           BCTBX_UNUSED(void *user_data)) {
	ortp_ev_dispatcher_disconnect(stream->evd, ORTP_EVENT_RTCP_PACKET_RECEIVED, RTCP_PSFB,
	                              (OrtpEvDispatcherCb)on_goog_remb_received);
}

static void on_ssrc_changed(RtpSession *session,
                            BCTBX_UNUSED(void *unused1),
                            BCTBX_UNUSED(void *unused2),
                            BCTBX_UNUSED(void *unused3)) {
	ms_message("SSRC change detected !");
	rtp_session_resync(session);
}
static void rtp_session_resync_cb(RtpSession *session,
                                  BCTBX_UNUSED(void *unused1),
                                  BCTBX_UNUSED(void *unused2),
                                  BCTBX_UNUSED(void *unused3)) {
	rtp_session_resync(session);
}
RtpSession *ms_create_duplex_rtp_session(const char *local_ip, int loc_rtp_port, int loc_rtcp_port, int mtu) {
	RtpSession *rtpr;
	const int socket_buf_size = 2000000;
	int rtcp_interval;

	rtpr = rtp_session_new(RTP_SESSION_SENDRECV);
	rtp_session_set_recv_buf_size(rtpr, MAX(mtu, MS_MINIMAL_MTU));
	rtp_session_set_scheduling_mode(rtpr, 0);
	rtp_session_set_blocking_mode(rtpr, 0);
	rtp_session_enable_adaptive_jitter_compensation(rtpr, TRUE);
	rtp_session_set_symmetric_rtp(rtpr, TRUE);
	if (local_ip) {
		rtp_session_set_local_addr(rtpr, local_ip, loc_rtp_port, loc_rtcp_port);
	} else {
		local_ip = "::0";
		if (rtp_session_set_local_addr(rtpr, local_ip, loc_rtp_port, loc_rtcp_port) < 0) {
			local_ip = "0.0.0.0";
			rtp_session_set_local_addr(rtpr, local_ip, loc_rtp_port, loc_rtcp_port);
		}
	}

	rtp_session_signal_connect(rtpr, "timestamp_jump", (RtpCallback)rtp_session_resync_cb, NULL);
	rtp_session_signal_connect(rtpr, "ssrc_changed", (RtpCallback)on_ssrc_changed, NULL);

	rtp_session_set_ssrc_changed_threshold(rtpr, 0);
	/* At the beginning of the session send more reports.
	   The randomness part is for internal tests, to avoid simultaenous sending of RTCP
	   report, which is unefficient for round trip delay computation.
	*/
	rtcp_interval = 2000 + (bctbx_random() % 1000);
	rtp_session_set_rtcp_report_interval(rtpr, rtcp_interval);
	rtp_session_set_multicast_loopback(rtpr, TRUE); /*very useful, specially for testing purposes*/
	rtp_session_set_send_ts_offset(rtpr, (uint32_t)bctbx_random());
	rtp_session_enable_avpf_feature(rtpr, ORTP_AVPF_FEATURE_TMMBR, TRUE);
	disable_checksums(rtp_session_get_rtp_socket(rtpr));

	/* Enlarge kernel socket buffers, which is necessary for video streams because large amounts of data can arrive
	 * between two ticks of mediastreamer2 processing. Previously, this was done only for VideoStreams, but since the
	 * sockets in the audio RtpSession may be used for video too (in RTP bundle mode), then it has to be done also for
	 * audio sockets.
	 */
	rtp_session_set_rtp_socket_recv_buffer_size(rtpr, socket_buf_size);
	rtp_session_set_rtp_socket_send_buffer_size(rtpr, socket_buf_size);
	return rtpr;
}

int media_stream_join_multicast_group(MediaStream *stream, const char *ip) {
	return rtp_session_join_multicast_group(stream->sessions.rtp_session, ip);
}

void media_stream_start_ticker(MediaStream *stream) {
	MSTickerParams params = {0};
	char name[32] = {0};

	if (stream->sessions.ticker) return;
	if (stream->log_tag) bctbx_push_log_tag(media_stream_id, stream->log_tag);
	snprintf(name, sizeof(name) - 1, "%s MSTicker", media_stream_type_str(stream));
	name[0] = toupper(name[0]);
	params.name = name;
	params.prio = __ms_get_default_prio((stream->type == MSVideo) ? TRUE : FALSE);
	stream->sessions.ticker = ms_ticker_new_with_params(&params);
	if (stream->log_tag) bctbx_pop_log_tag(media_stream_id);
}

const char *media_stream_type_str(MediaStream *stream) {
	return ms_format_type_to_string(stream->type);
}

static void ms_media_stream_bundle_and_sessions_free(void *b) {
	RtpSession *session = (RtpSession *)b;
	if (session->bundle != NULL) {
		rtp_bundle_remove_session(session->bundle, session);
	}
	rtp_session_destroy(session);
}

void ms_media_stream_sessions_uninit(MSMediaStreamSessions *sessions) {
	if (sessions->srtp_context) {
		ms_srtp_context_delete(sessions->srtp_context);
		sessions->srtp_context = NULL;
	}

	if (sessions->rtp_session) {
		rtp_session_destroy(sessions->rtp_session);
		sessions->rtp_session = NULL;
	}
	if (sessions->fec_session) {
		rtp_session_destroy(sessions->fec_session);
		sessions->fec_session = NULL;
	}
	if (sessions->zrtp_context != NULL) {
		ms_zrtp_context_destroy(sessions->zrtp_context);
		sessions->zrtp_context = NULL;
	}
	if (sessions->dtls_context != NULL) {
		ms_dtls_srtp_context_destroy(sessions->dtls_context);
		sessions->dtls_context = NULL;
	}
	if (sessions->ticker) {
		ms_ticker_destroy(sessions->ticker);
		sessions->ticker = NULL;
	}

	if (sessions->auxiliary_sessions != NULL) {
		bctbx_list_free_with_data(sessions->auxiliary_sessions, ms_media_stream_bundle_and_sessions_free);
		sessions->auxiliary_sessions = NULL;
	}
}

void media_stream_free(MediaStream *stream) {
	media_stream_remove_tmmbr_handler(stream, media_stream_tmmbr_received, stream);
	media_stream_remove_goog_remb_handler(stream, media_stream_goog_remb_received, stream);

	if (stream->sessions.zrtp_context != NULL) {
		ms_zrtp_set_stream_sessions(stream->sessions.zrtp_context, NULL);
	}
	if (stream->sessions.dtls_context != NULL) {
		ms_dtls_srtp_set_stream_sessions(stream->sessions.dtls_context, NULL);
	}

	if (stream->sessions.auxiliary_sessions != NULL) {
		for (bctbx_list_t *it = stream->sessions.auxiliary_sessions; it != NULL; it = it->next) {
			RtpSession *aux = (RtpSession *)it->data;
			if (aux != NULL) {
				rtp_session_unregister_event_queues(aux);
			}
		}
	}
	if (stream->sessions.rtp_session != NULL)
		rtp_session_unregister_event_queue(stream->sessions.rtp_session, stream->evq);

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
	if (stream->fec_parameters != NULL) fec_params_destroy(stream->fec_parameters);
	if (stream->log_tag) bctbx_free(stream->log_tag);
	if (stream->last_goog_remb_received) freemsg(stream->last_goog_remb_received);
}

MSFactory *media_stream_get_factory(MediaStream *stream) {
	return (stream) ? stream->factory : NULL;
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

void media_stream_get_local_fec_stats(MediaStream *stream, fec_stats *lstats) {
	if (stream->sessions.rtp_session->fec_stream == NULL) {
		memset(lstats, 0, sizeof(fec_stats));
		return;
	}
	const fec_stats *stats = fec_stream_get_stats(stream->sessions.rtp_session->fec_stream);
	memcpy(lstats, stats, sizeof(*stats));
}

int media_stream_set_dscp(MediaStream *stream, int dscp) {
	ms_message("Setting DSCP to %i for %s stream.", dscp, media_stream_type_str(stream));
	return rtp_session_set_dscp(stream->sessions.rtp_session, dscp);
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

void media_stream_set_stun_allowed(MediaStream *stream, bool_t value) {
	stream->stun_allowed = value;
}

/* This function decides whether it is necessary to send dummy stun packets for firewall opening. */
static void media_stream_configure_stun_packet_sending(MediaStream *stream) {
	bool_t stun_enabled = stream->stun_allowed;
	if (stream->ice_check_list) stun_enabled = FALSE;
	if (stream->sessions.rtp_session->bundle && !stream->sessions.rtp_session->is_primary) {
		stun_enabled = FALSE;
	}
	if (stream->rtpsend != NULL) {
		ms_filter_call_method(stream->rtpsend, MS_RTP_SEND_ENABLE_STUN, &stun_enabled);
		if (stream->sessions.dtls_context) {
			/* In case STUN packets are necessary, and DTLS is used and encryption mandatory is set to TRUE,
			 * no RTP packets will be emitted at all, until handshake takes places.
			 * We must configure the rtpsend filter to regularly send dummy stun packets
			 * to ensure that firewall gets open to the remote endpoint.
			 * Note that we are unable to check here if encryption mandatory is on (due to order of operation that might
			 * be undetermined), but it is acceptable to send dummy stun packets even if encryption mandatory is off.
			 */
			ms_filter_call_method(stream->rtpsend, MS_RTP_SEND_ENABLE_STUN_FORCED, &stun_enabled);
		}
	}
}

void media_stream_enable_dtls(MediaStream *stream, const MSDtlsSrtpParams *params) {
	if (stream->sessions.dtls_context == NULL) {
		MSDtlsSrtpParams params_copy = *params;
		ms_message("Create DTLS media stream context in stream session [%p]", &(stream->sessions));
		if (params_copy.mtu == 0) params_copy.mtu = ms_factory_get_mtu(stream->factory);

		stream->sessions.dtls_context = ms_dtls_srtp_context_new(&(stream->sessions), &params_copy);
		media_stream_configure_stun_packet_sending(stream);
	}
}

void media_stream_set_ice_check_list(MediaStream *stream, IceCheckList *cl) {
	stream->ice_check_list = cl;
	if (stream->ice_check_list != NULL) {
		ice_check_list_set_rtp_session(stream->ice_check_list, stream->sessions.rtp_session);
	}
	media_stream_configure_stun_packet_sending(stream);
}

bool_t media_stream_dtls_supported(void) {
	return ms_dtls_srtp_available();
}

/*deprecated*/
/* This function is called only when using SDES to exchange SRTP keys */
bool_t media_stream_enable_srtp(MediaStream *stream, MSCryptoSuite suite, const char *snd_key, const char *rcv_key) {
	return ms_media_stream_sessions_set_srtp_recv_key_b64(&stream->sessions, suite, rcv_key, MSSrtpKeySourceSDES) ==
	           0 &&
	       ms_media_stream_sessions_set_srtp_send_key_b64(&stream->sessions, suite, snd_key, MSSrtpKeySourceSDES) == 0;
}

const MSQualityIndicator *media_stream_get_quality_indicator(MediaStream *stream) {
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
	err = getaddrinfo(remote, "8000", &hints, &res0);
	if (err != 0) {
		ms_warning("ms_is_ipv6(%s): %s", remote, gai_strerror(err));
		return FALSE;
	}
	ret = (res0->ai_addr->sa_family == AF_INET6);
	freeaddrinfo(res0);
	return ret;
}

bool_t ms_is_multicast_addr(const struct sockaddr *addr) {
	return bctbx_is_multicast_addr(addr);
}

bool_t ms_is_multicast(const char *address) {
	bool_t ret = FALSE;
	struct addrinfo hints, *res0;
	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICHOST;
	err = getaddrinfo(address, "8000", &hints, &res0);
	if (err != 0) {
		ms_warning("ms_is_multicast(%s): %s", address, gai_strerror(err));
		return FALSE;
	}
	ret = ms_is_multicast_addr(res0->ai_addr);

	freeaddrinfo(res0);
	return ret;
}

static void media_stream_process_rtcp(MediaStream *stream, mblk_t *m) {
	RtcpParserContext rtcp_parser;
	const mblk_t *rtcp_packet;

	ms_message("%s stream [%p]: receiving RTCP %s%s", media_stream_type_str(stream), stream,
	           (rtcp_is_SR(m) ? "SR" : ""), (rtcp_is_RR(m) ? "RR" : ""));
	rtcp_packet = rtcp_parser_context_init(&rtcp_parser, m);
	do {
		if (stream->rc_enable && stream->rc) ms_bitrate_controller_process_rtcp(stream->rc, rtcp_packet);
		if (stream->qi) ms_quality_indicator_update_from_feedback(stream->qi, rtcp_packet);
#ifdef VIDEO_ENABLED
		if (stream->video_quality_controller)
			ms_video_quality_controller_update_from_feedback(stream->video_quality_controller, rtcp_packet);
		stream->process_rtcp(stream, rtcp_packet);
#endif
	} while ((rtcp_packet = rtcp_parser_context_next_packet(&rtcp_parser)) != NULL);
	rtcp_parser_context_uninit(&rtcp_parser);
}

void media_stream_set_direction(MediaStream *stream, MediaStreamDir dir) {
	RtpSessionMode mode = RTP_SESSION_SENDRECV;
	stream->direction = dir;
	if (stream->sessions.rtp_session) {
		switch (dir) {
			case MediaStreamSendOnly:
				mode = RTP_SESSION_SENDONLY;
				break;
			case MediaStreamRecvOnly:
				mode = RTP_SESSION_RECVONLY;
				break;
			case MediaStreamSendRecv:
				mode = RTP_SESSION_SENDRECV;
				break;
		}
		rtp_session_set_mode(stream->sessions.rtp_session, mode);
	}
	if (dir == MediaStreamSendOnly && stream->bandwidth_controller) {
		ms_bandwidth_controller_elect_controlled_streams(stream->bandwidth_controller);
	}
}

MediaStreamDir media_stream_get_direction(const MediaStream *stream) {
	return stream->direction;
}

static void media_stream_process_event_queue(MediaStream *stream, OrtpEvQueue *q) {
	OrtpEvent *ev = NULL;

	while ((ev = ortp_ev_queue_get(q)) != NULL) {
		OrtpEventType evt = ortp_event_get_type(ev);
		if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
			mblk_t *m = ortp_event_get_data(ev)->packet;
			media_stream_process_rtcp(stream, m);
		} else if (evt == ORTP_EVENT_RTCP_PACKET_EMITTED) {
			ms_message("%s_stream_iterate[%p], local statistics available:"
			           "\n\tLocal current jitter buffer size: %5.1fms",
			           media_stream_type_str(stream), stream,
			           rtp_session_get_jitter_stats(stream->sessions.rtp_session)->jitter_buffer_size_ms);
		} else if (evt == ORTP_EVENT_STUN_PACKET_RECEIVED && stream->ice_check_list) {
			ice_handle_stun_packet(stream->ice_check_list, stream->sessions.rtp_session, ortp_event_get_data(ev));
		} else if ((evt == ORTP_EVENT_ZRTP_ENCRYPTION_CHANGED) || (evt == ORTP_EVENT_DTLS_ENCRYPTION_CHANGED)) {
			ms_message("%s_stream_iterate[%p]: is %s ", media_stream_type_str(stream), stream,
			           media_stream_secured(stream) ? "encrypted" : "not encrypted");
		}
		ortp_event_destroy(ev);
	}
}

void media_stream_iterate(MediaStream *stream) {
	time_t curtime = ms_time(NULL);

	if (stream->log_tag) bctbx_push_log_tag(media_stream_id, stream->log_tag);

	if (stream->ice_check_list) ice_check_list_process(stream->ice_check_list, stream->sessions.rtp_session);
	/*we choose to update the quality indicator as much as possible, since local statistics can be computed realtime. */
	if (stream->state == MSStreamStarted) {
		if (stream->is_beginning && (curtime - stream->start_time > 15)) {
			rtp_session_set_rtcp_report_interval(stream->sessions.rtp_session, 5000);
			stream->is_beginning = FALSE;
		}
		if (stream->qi && curtime > stream->last_iterate_time) {
			if (stream->direction != MediaStreamSendOnly) { // Local quality indicator in send only will be wrong
				ms_quality_indicator_update_local(stream->qi);
			}
		}
	}
	stream->last_iterate_time = curtime;

	if (stream->rc) ms_bitrate_controller_update(stream->rc);

	if (stream->evd) {
		ortp_ev_dispatcher_iterate(stream->evd);
	}

	if (stream->evq) {
		media_stream_process_event_queue(stream, stream->evq);
	}

	if (stream->log_tag) bctbx_pop_log_tag(media_stream_id);
}

bool_t media_stream_alive(MediaStream *ms, int timeout) {
	const rtp_stats_t *rtpstats;
	if (ms->state != MSStreamStarted) {
		return TRUE;
	}

	/* get stats on main session and auxiliary ones: ms->last packet count stores the sum of the recv RTP and RTCP
	 * packets*/
	rtpstats = rtp_session_get_stats(ms->sessions.rtp_session);
	uint64_t recv_total_stats = rtpstats->recv + rtpstats->recv_rtcp_packets;
	bctbx_list_t *it = ms->sessions.auxiliary_sessions;
	while (it) {
		rtpstats = rtp_session_get_stats((RtpSession *)bctbx_list_get_data(it));
		recv_total_stats += rtpstats->recv + rtpstats->recv_rtcp_packets;
		it = it->next;
	}
	if (recv_total_stats != 0) {
		if (recv_total_stats != ms->last_packet_count) {
			ms->last_packet_count = recv_total_stats;
			ms->last_packet_time = ms_time(NULL);
		}
	}
	if (ms_time(NULL) - ms->last_packet_time > timeout) {
		/* more than timeout seconds of inactivity*/
		return FALSE;
	}
	return TRUE;
}

float media_stream_get_quality_rating(MediaStream *stream) {
	if (stream->qi) {
		return ms_quality_indicator_get_rating(stream->qi);
	}
	return -1;
}

float media_stream_get_average_quality_rating(MediaStream *stream) {
	if (stream->qi) {
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

int media_stream_set_target_network_bitrate(MediaStream *stream, int target_bitrate) {
	stream->target_bitrate = target_bitrate;
	return 0;
}

int media_stream_set_max_network_bitrate(MediaStream *stream, int max_bitrate) {
	stream->max_target_bitrate = max_bitrate;
	return 0;
}

int media_stream_get_target_network_bitrate(const MediaStream *stream) {
	return stream->target_bitrate;
}

typedef float (*RtpSessionBandwidthFunc)(RtpSession *);
static float media_stream_sum_bandwidth(const MediaStream *stream, RtpSessionBandwidthFunc func) {
	float ret = func(stream->sessions.rtp_session);
	bctbx_list_t *elem;
	for (elem = stream->sessions.auxiliary_sessions; elem != NULL; elem = elem->next) {
		RtpSession *aux_session = (RtpSession *)elem->data;
		ret += func(aux_session);
	}
	return ret;
}

float media_stream_get_up_bw(const MediaStream *stream) {
	return media_stream_sum_bandwidth(stream, rtp_session_get_rtp_send_bandwidth);
}

float media_stream_get_down_bw(const MediaStream *stream) {
	return media_stream_sum_bandwidth(stream, rtp_session_get_rtp_recv_bandwidth);
}

float media_stream_get_fec_up_bw(const MediaStream *stream) {
	if (stream->sessions.fec_session == NULL) {
		return 0.f;
	}
	return rtp_session_get_rtp_send_bandwidth(stream->sessions.fec_session);
}

float media_stream_get_fec_down_bw(const MediaStream *stream) {
	if (stream->sessions.fec_session == NULL) {
		return 0.f;
	}
	return rtp_session_get_rtp_recv_bandwidth(stream->sessions.fec_session);
}

float media_stream_get_rtcp_up_bw(const MediaStream *stream) {
	return media_stream_sum_bandwidth(stream, rtp_session_get_rtcp_send_bandwidth);
}

float media_stream_get_rtcp_down_bw(const MediaStream *stream) {
	return media_stream_sum_bandwidth(stream, rtp_session_get_rtcp_recv_bandwidth);
}

void media_stream_reclaim_sessions(MediaStream *stream, MSMediaStreamSessions *sessions) {
	memcpy(sessions, &stream->sessions, sizeof(MSMediaStreamSessions));
	stream->owns_sessions = FALSE;
}

bool_t media_stream_secured(const MediaStream *stream) {
	if (stream->state != MSStreamStarted) return FALSE;

	switch (stream->type) {
		case MSAudio:
		case MSText:
		case MSVideo:
			return ms_media_stream_sessions_secured(&stream->sessions, stream->direction);
		case MSUnknownMedia:
			break;
	}
	return FALSE;
}

MSSrtpKeySource media_stream_get_srtp_key_source(const MediaStream *stream, MediaStreamDir dir, bool_t is_inner) {
	if (stream->state != MSStreamStarted) return MSSrtpKeySourceUnavailable;

	switch (stream->type) {
		case MSAudio:
		case MSText:
		case MSVideo:
			return ms_media_stream_sessions_get_srtp_key_source(&stream->sessions, dir, is_inner);
		case MSUnknownMedia:
		default:
			break;
	}
	return MSSrtpKeySourceUnavailable;
}

MSCryptoSuite media_stream_get_srtp_crypto_suite(const MediaStream *stream, MediaStreamDir dir, bool_t is_inner) {
	if (stream->state != MSStreamStarted) return MS_CRYPTO_SUITE_INVALID;

	switch (stream->type) {
		case MSAudio:
		case MSText:
		case MSVideo:
			return ms_media_stream_sessions_get_srtp_crypto_suite(&stream->sessions, dir, is_inner);
		case MSUnknownMedia:
		default:
			break;
	}
	return MS_CRYPTO_SUITE_INVALID;
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

RtpSession *media_stream_get_rtp_session(const MediaStream *stream) {
	return stream->sessions.rtp_session;
}

#define keywordcmp(key, b) strncmp(key, b, sizeof(key))

/* see
 * http://www.iana.org/assignments/sdp-security-descriptions/sdp-security-descriptions.xhtml#sdp-security-descriptions-3
 */

MSCryptoSuite ms_crypto_suite_build_from_name_params(const MSCryptoSuiteNameParams *descrption) {
	const char *name = descrption->name, *parameters = descrption->params;
	if (keywordcmp("AES_CM_128_HMAC_SHA1_80", name) == 0) {
		if (parameters && strstr(parameters, "UNENCRYPTED_SRTP") && strstr(parameters, "UNENCRYPTED_SRTCP"))
			return MS_AES_128_SHA1_80_NO_CIPHER;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTP")) return MS_AES_128_SHA1_80_SRTP_NO_CIPHER;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTCP")) return MS_AES_128_SHA1_80_SRTCP_NO_CIPHER;
		else if (parameters && strstr(parameters, "UNAUTHENTICATED_SRTP")) return MS_AES_128_SHA1_80_NO_AUTH;
		else return MS_AES_128_SHA1_80;
	} else if (keywordcmp("AES_CM_128_HMAC_SHA1_32", name) == 0) {
		if (parameters && strstr(parameters, "UNENCRYPTED_SRTP") && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNAUTHENTICATED_SRTP")) return MS_AES_128_SHA1_32_NO_AUTH;
		else return MS_AES_128_SHA1_32;
	} else if (keywordcmp("AES_256_CM_HMAC_SHA1_32", name) == 0) {
		if (parameters && strstr(parameters, "UNENCRYPTED_SRTP") && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNAUTHENTICATED_SRTP")) goto error;
		else return MS_AES_256_SHA1_32;
	} else if (keywordcmp("AES_256_CM_HMAC_SHA1_80", name) == 0) {
		if (parameters && strstr(parameters, "UNENCRYPTED_SRTP") && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNAUTHENTICATED_SRTP")) goto error;
		else return MS_AES_256_SHA1_80;
	} else if (keywordcmp("AES_CM_256_HMAC_SHA1_80", name) == 0) {
		if (parameters && strstr(parameters, "UNENCRYPTED_SRTP") && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNAUTHENTICATED_SRTP")) goto error;
		else return MS_AES_CM_256_SHA1_80;
	} else if (keywordcmp("AEAD_AES_128_GCM", name) == 0) {
		if (parameters && strstr(parameters, "UNENCRYPTED_SRTP") && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNAUTHENTICATED_SRTP")) goto error;
		else return MS_AEAD_AES_128_GCM;
	} else if (keywordcmp("AEAD_AES_256_GCM", name) == 0) {
		if (parameters && strstr(parameters, "UNENCRYPTED_SRTP") && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTP")) goto error;
		else if (parameters && strstr(parameters, "UNENCRYPTED_SRTCP")) goto error;
		else if (parameters && strstr(parameters, "UNAUTHENTICATED_SRTP")) goto error;
		else return MS_AEAD_AES_256_GCM;
	}
error:
	ms_error("Unsupported crypto suite '%s' with parameters '%s'", name, parameters ? parameters : "");
	return MS_CRYPTO_SUITE_INVALID;
}

bool_t ms_crypto_suite_is_unencrypted(MSCryptoSuite cs) {
	return (cs == MS_AES_128_SHA1_80_SRTP_NO_CIPHER) || (cs == MS_AES_128_SHA1_80_SRTCP_NO_CIPHER) ||
	       (cs == MS_AES_128_SHA1_80_NO_CIPHER);
}

bool_t ms_crypto_suite_is_unauthenticated(MSCryptoSuite cs) {
	return (cs == MS_AES_128_SHA1_80_NO_AUTH) || (cs == MS_AES_128_SHA1_32_NO_AUTH);
}

int ms_crypto_suite_to_name_params(MSCryptoSuite cs, MSCryptoSuiteNameParams *params) {
	params->name = NULL;
	params->params = NULL;
	switch (cs) {
		case MS_CRYPTO_SUITE_INVALID:
			break;
		case MS_AES_128_SHA1_80:
			params->name = "AES_CM_128_HMAC_SHA1_80";
			break;
		case MS_AES_128_SHA1_32:
			params->name = "AES_CM_128_HMAC_SHA1_32";
			break;
		case MS_AES_128_SHA1_80_NO_AUTH:
			params->name = "AES_CM_128_HMAC_SHA1_80";
			params->params = "UNAUTHENTICATED_SRTP";
			break;
		case MS_AES_128_SHA1_32_NO_AUTH:
			params->name = "AES_CM_128_HMAC_SHA1_32";
			params->params = "UNAUTHENTICATED_SRTP";
			break;
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
			params->name = "AES_CM_128_HMAC_SHA1_80";
			params->params = "UNENCRYPTED_SRTP";
			break;
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
			params->name = "AES_CM_128_HMAC_SHA1_80";
			params->params = "UNENCRYPTED_SRTCP";
			break;
		case MS_AES_128_SHA1_80_NO_CIPHER:
			params->name = "AES_CM_128_HMAC_SHA1_80";
			params->params = "UNENCRYPTED_SRTP UNENCRYPTED_SRTCP";
			break;
		case MS_AES_256_SHA1_80:
			params->name = "AES_256_CM_HMAC_SHA1_80";
			break;
		case MS_AES_CM_256_SHA1_80:
			params->name = "AES_CM_256_HMAC_SHA1_80";
			break;
		case MS_AES_256_SHA1_32:
			params->name = "AES_256_CM_HMAC_SHA1_32";
			break;
		case MS_AEAD_AES_128_GCM:
			params->name = "AEAD_AES_128_GCM";
			break;
		case MS_AEAD_AES_256_GCM:
			params->name = "AEAD_AES_256_GCM";
			break;
	}
	if (params->name == NULL) return -1;
	return 0;
}

OrtpEvDispatcher *media_stream_get_event_dispatcher(const MediaStream *stream) {
	return stream->evd;
}

const char *ms_resource_type_to_string(MSResourceType type) {
	switch (type) {
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
		case MSResourceVoid:
			return "MSResourceVoid";
		case MSResourceItc:
			return "MSResourceItc";
		case MSResourceScreenSharing:
			return "MSResourceScreenSharing";
	}
	return "INVALID";
}

bool_t ms_media_resource_is_consistent(const MSMediaResource *r) {
	switch (r->type) {
		case MSResourceRtp:
		case MSResourceSoundcard:
			if (r->resource_arg == NULL) {
				ms_error("No resource argument specified for resource type %s", ms_resource_type_to_string(r->type));
				return FALSE;
			}
			return TRUE;
			break;
		case MSResourceCamera:
			if (r->camera == NULL) {
				ms_error("No camera specified for resource type %s", ms_resource_type_to_string(r->type));
				return FALSE;
			}
			return TRUE;
			break;
		case MSResourceScreenSharing:
			return TRUE;
			break;
		case MSResourceFile:
			/*setting up file player/recorder without specifying the file to play immediately is allowed*/
		case MSResourceDefault:
		case MSResourceItc:
			return TRUE;
		case MSResourceInvalid:
			ms_error("Invalid resource type specified");
			return FALSE;
		case MSResourceVoid:
			return TRUE;
	}
	ms_error("Unsupported media resource type [%i]", (int)r->type);
	return FALSE;
}

bool_t ms_media_stream_io_is_consistent(const MSMediaStreamIO *io) {
	return ms_media_resource_is_consistent(&io->input) && ms_media_resource_is_consistent(&io->output);
}

/*stubs*/
#ifndef VIDEO_ENABLED
void video_stream_open_player(BCTBX_UNUSED(VideoStream *stream), BCTBX_UNUSED(MSFilter *sink)) {
}

void video_stream_close_player(BCTBX_UNUSED(VideoStream *stream)) {
}

void video_stream_enable_recording(BCTBX_UNUSED(VideoStream *stream), BCTBX_UNUSED(bool_t enabled)) {
}

MSWebCamDesc *ms_mire_webcam_desc_get(void) {
	return NULL;
}

#endif

static int update_bitrate_limit_from_tmmbr(MediaStream *obj, int br_limit) {
	int previous_br_limit = rtp_session_get_target_upload_bandwidth(obj->sessions.rtp_session);
	if (!obj->encoder) {
		ms_warning("TMMBR not applicable because no encoder for this stream.");
		return -1;
	}

	if (obj->max_target_bitrate > 0 && br_limit > obj->max_target_bitrate) {
		ms_message("TMMBR is greater than maximum target bitrate set (%i > %i), capping to %i bits/s", br_limit,
		           obj->max_target_bitrate, obj->max_target_bitrate);
		br_limit = obj->max_target_bitrate;
	}

	if (previous_br_limit == br_limit) {
		ms_message("Previous bitrate limit was already %i, skipping...", br_limit);
		return -1;
	}

	if (obj->type != MSVideo) {
		if (ms_filter_call_method(obj->encoder, MS_FILTER_SET_BITRATE, &br_limit) != 0) {
			ms_warning("Failed to apply bitrate constraint to %s", obj->encoder->desc->name);
		}
	}

	media_stream_set_target_network_bitrate(obj, br_limit);
	rtp_session_set_target_upload_bandwidth(obj->sessions.rtp_session, br_limit);
	return br_limit;
}

void media_stream_process_tmmbr(MediaStream *ms, uint64_t tmmbr_mxtbr) {
	int br_int;

	ms_message("MediaStream[%p]: received a TMMBR for bitrate %llu kbits/s", ms,
	           (unsigned long long)(tmmbr_mxtbr / 1000));

	/* When audio estimator is on, the actual output will be increased so reduce the incoming TMMBR */
	if (ms->type == MSAudio && media_stream_get_rtp_session(ms)->audio_bandwidth_estimator_enabled &&
	    media_stream_get_rtp_session(ms)->rtp.audio_bw_estimator) {
		tmmbr_mxtbr -=
		    tmmbr_mxtbr / rtp_session_get_audio_bandwidth_estimator_duplicate_rate(media_stream_get_rtp_session(ms));
	}
	if (tmmbr_mxtbr < (uint64_t)INT_MAX) {
		br_int = (int)tmmbr_mxtbr;
	} else {
		br_int = INT_MAX;
	}

	br_int = update_bitrate_limit_from_tmmbr(ms, br_int);
	if (br_int == -1) return;

#ifdef VIDEO_ENABLED
	if (ms->type != MSVideo) return;

	ms_video_quality_controller_update_from_tmmbr(ms->video_quality_controller, br_int);

#endif
}

void media_stream_tmmbr_received(const OrtpEventData *evd, void *user_pointer) {

	if (rtcp_RTPFB_get_type(evd->packet) != RTCP_RTPFB_TMMBR) return;
	MediaStream *ms = (MediaStream *)user_pointer;
	uint64_t tmmbr_mxtbr = rtcp_RTPFB_tmmbr_get_max_bitrate(evd->packet);
	media_stream_process_tmmbr(ms, tmmbr_mxtbr);
}

static bool_t mediastream_goog_remb_equals(mblk_t *received, mblk_t *previous) {
	const rtcp_fb_header_t *received_header = (rtcp_fb_header_t *)(received->b_rptr + sizeof(rtcp_common_header_t));
	const rtcp_fb_header_t *previous_header = (rtcp_fb_header_t *)(previous->b_rptr + sizeof(rtcp_common_header_t));

	if (received_header->packet_sender_ssrc != previous_header->packet_sender_ssrc) return FALSE;

	const rtcp_fb_goog_remb_fci_t *received_fci = rtcp_PSFB_goog_remb_get_fci(received);
	const rtcp_fb_goog_remb_fci_t *previous_fci = rtcp_PSFB_goog_remb_get_fci(previous);

	if (received_fci->value != previous_fci->value) return FALSE;

	const uint32_t *received_ssrcs = (uint32_t *)(received->b_rptr + sizeof(rtcp_common_header_t) +
	                                              sizeof(rtcp_fb_header_t) + sizeof(rtcp_fb_goog_remb_fci_t));
	const uint32_t *previous_ssrcs = (uint32_t *)(previous->b_rptr + sizeof(rtcp_common_header_t) +
	                                              sizeof(rtcp_fb_header_t) + sizeof(rtcp_fb_goog_remb_fci_t));

	// Since both goog-remb have the same value, then they have the same number of SSRCs
	for (int i = 0; i < rtcp_fb_goog_remb_fci_get_num_ssrc(received_fci); ++i) {
		if (received_ssrcs[i] != previous_ssrcs[i]) return FALSE;
	}

	return TRUE;
}

void media_stream_goog_remb_received(const OrtpEventData *evd, void *user_pointer) {
	if (rtcp_PSFB_get_type(evd->packet) != RTCP_PSFB_AFB) return;

	// Make sure we actually have a goog-remb
	const rtcp_fb_goog_remb_fci_t *fci = rtcp_PSFB_goog_remb_get_fci(evd->packet);
	if (fci == NULL || ntohl(fci->identifier) != 0x52454d42) return;

	// And that this goog-remb is actually about us
	if (rtcp_fb_goog_remb_fci_get_num_ssrc(fci) > 1) {
		ms_warning("Received a goog-remb with more that 1 ssrc feedback, ignoring...");
		return;
	}

	MediaStream *ms = (MediaStream *)user_pointer;
	const uint32_t *ssrcs = (uint32_t *)(evd->packet->b_rptr + sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t) +
	                                     sizeof(rtcp_fb_goog_remb_fci_t));
	if (ntohl(ssrcs[0]) != media_stream_get_send_ssrc(ms)) {
		ms_warning("Received a goog-remb for ssrc (%u) that is not for us, ignoring...", ntohl(ssrcs[0]));
		return;
	}

	// If we received the same goog-remb ignore it
	if (ms->last_goog_remb_received != NULL) {
		if (mediastream_goog_remb_equals(evd->packet, ms->last_goog_remb_received)) return;

		freemsg(ms->last_goog_remb_received);
	}

	// Store it for next comparison
	ms->last_goog_remb_received = copymsg(evd->packet);

	// Process as a TMMBR
	uint64_t tmmbr_mxtbr = rtcp_PSFB_goog_remb_get_max_bitrate(evd->packet);
	media_stream_process_tmmbr(ms, tmmbr_mxtbr);
}

void media_stream_print_summary(MediaStream *ms) {
	ms_message("MediaStream[%p] (%s) with RtpSession[%p] summary:", ms, ms_format_type_to_string(ms->type),
	           ms->sessions.rtp_session);
	ms_message("send-ssrc = [dec:%u hex:%x]", rtp_session_get_send_ssrc(ms->sessions.rtp_session),
	           rtp_session_get_send_ssrc(ms->sessions.rtp_session));
	ms_message("recv-ssrc = [dec:%u hex:%x]", rtp_session_get_recv_ssrc(ms->sessions.rtp_session),
	           rtp_session_get_recv_ssrc(ms->sessions.rtp_session));
	if (ms->ice_check_list != NULL) {
		ice_check_list_print_route(ms->ice_check_list, "ICE route:");
		ms->ice_check_list = NULL;
	}
	if (!ms->sessions.rtp_session->fec_stream) {
		rtp_stats_display(rtp_session_get_stats(ms->sessions.rtp_session),
		                  "                     RTP STATISTICS                          ");
	} else {
		rtp_stats_display_all(rtp_session_get_stats(ms->sessions.rtp_session),
		                      rtp_session_get_stats(ms->sessions.fec_session),
		                      "                                RTP STATISTICS                                   ");
		fec_stream_print_stats(ms->sessions.rtp_session->fec_stream);
	}
}

/* Create a new RTP session copying the setting of the given one */
RtpSession *media_stream_rtp_session_new_from_session(RtpSession *session, int mode) {
	RtpSession *s = rtp_session_new(mode);
	/* TODO: also copy IP and port? This is used only in bundle so it shall not be needed */
	/* profile */
	rtp_session_set_send_profile(s, rtp_session_get_send_profile(session));
	rtp_session_set_recv_profile(s, rtp_session_get_recv_profile(session));

	/* payload */
	rtp_session_set_send_payload_type(s, rtp_session_get_send_payload_type(session));
	rtp_session_set_recv_payload_type(s, rtp_session_get_recv_payload_type(session));

	/* jitter settings */
	if (rtp_session_jitter_buffer_enabled(session)) {
		JBParameters jitter_params;
		rtp_session_enable_jitter_buffer(s, TRUE);
		rtp_session_get_jitter_buffer_params(session, &jitter_params);
		rtp_session_set_jitter_buffer_params(s, &jitter_params);
	} else {
		rtp_session_enable_jitter_buffer(s, FALSE);
	}
	/* RTCP */
	rtp_session_enable_rtcp(s, rtp_session_rtcp_enabled(session));
	rtp_session_enable_rtcp_mux(s, rtp_session_rtcp_mux_enabled(session));

	/* We want the auxiliary session to dispatch its events to the same recipients as the main session.
	 * For that we simply register the main's RtpSession current event queues into the new auxiliary session.
	 */
	for (bctbx_list_t *it = session->eventqs; it != NULL; it = it->next) {
		OrtpEvQueue *q = (OrtpEvQueue *)it->data;
		if (q != NULL) rtp_session_register_event_queue(s, q);
	}

	return s;
}

void media_stream_on_outgoing_ssrc_in_bundle(RtpSession *session, void *mp, void *s, void *userData) {
	mblk_t *m = (mblk_t *)mp;
	uint32_t ssrc = rtp_get_ssrc(m);
	RtpBundle *bundle = session->bundle;
	RtpSession **newSession = (RtpSession **)s;
	MediaStream *ms = (MediaStream *)userData;

	/* fetch the MID from the packet and check it is in sync with the current RtpSession one
	 * Do not create a new session (-> packet drop) if :
	 * - no MID in packet
	 * - current session and packet MID do not match
	 */
	int midId = rtp_bundle_get_mid_extension_id(bundle);
	uint8_t *mid = NULL;
	char *sMid = NULL;
	size_t midSize = rtp_get_extension_header(m, midId != -1 ? midId : RTP_EXTENSION_MID, &mid);
	if (midSize == (size_t)-1) {
		/* there is no MID in the incoming packet */
		ms_warning("New outgoing SSRC %u on session %p but no MID found in the incoming packet", ssrc, session);
		return;
	} else {
		sMid = bctbx_malloc0(midSize + 1);
		memcpy(sMid, mid, midSize);
		/* Check the mid in packet matches the stream's session one */
		char *streamMid = rtp_bundle_get_session_mid(session->bundle, ms->sessions.rtp_session);
		if ((strlen(streamMid) != midSize) || (memcmp(mid, streamMid, midSize) != 0)) {
			ms_warning("New outgoing SSRC %u on session %p but packet Mid %s differs from session mid %s", ssrc,
			           session, sMid, streamMid);
			bctbx_free(streamMid);
			bctbx_free(sMid);
			return;
		}
		if (streamMid != NULL) bctbx_free(streamMid);
	}

	if (*newSession != NULL) {
		ms_error("New outgoing SSRC %u on session %p but the session has already been created", ssrc, session);
		return;
	}

	/* create a new session copying param from the main one */
	*newSession = media_stream_rtp_session_new_from_session(ms->sessions.rtp_session, RTP_SESSION_SENDONLY);
	ms_message("New outgoing SSRC %u on session %p detected, create a new session %p", ssrc, session, *newSession);
	rtp_session_enable_transfer_mode(*newSession, TRUE); // relay rtp session is in transfer mode
	bctbx_free(sMid);

	/* keep track of newly created session */
	ms->sessions.auxiliary_sessions = bctbx_list_append(ms->sessions.auxiliary_sessions, *newSession);

	/* this new session is associated to the outgoing SSRC */
}

uint32_t media_stream_get_send_ssrc(const MediaStream *stream) {
	return rtp_session_get_send_ssrc(stream->sessions.rtp_session);
}

uint32_t media_stream_get_recv_ssrc(const MediaStream *stream) {
	return rtp_session_get_recv_ssrc(stream->sessions.rtp_session);
}

FecParams *media_stream_extract_fec_params(const PayloadType *fec_payload_type) {

	size_t max_size = 10;
	char buffer[10];
	int rw = 100000;

	FecParams *params = NULL;
	if (fmtp_get_value(fec_payload_type->recv_fmtp, "repair-window", buffer, max_size)) {
		rw = atoi(buffer);
		ms_message("[flexfec] repair window set to %d according to fmtp", rw);
	} else {
		ms_error("[flexfec] Impossible to read value of repair window. A default value of %d is given.", rw);
	}
	params = fec_params_new(rw);
	return params;
}

void media_stream_create_or_update_fec_session(MediaStream *ms) {
	if (ms->sessions.rtp_session->bundle == NULL) return;

	RtpProfile *profile = rtp_session_get_send_profile(ms->sessions.rtp_session);
	const PayloadType *fec_payload_type = rtp_profile_get_payload_from_mime(profile, "flexfec");
	if (fec_payload_type == NULL) return;

	char *mid = rtp_bundle_get_session_mid(ms->sessions.rtp_session->bundle, ms->sessions.rtp_session);
	if (mid == NULL) return;

	if (ms->sessions.fec_session == NULL) {
		RtpSession *fec_session = rtp_session_new(RTP_SESSION_SENDRECV);
		rtp_session_set_scheduling_mode(fec_session, 0);
		rtp_session_set_blocking_mode(fec_session, 0);
		rtp_session_enable_rtcp(fec_session, TRUE);
		rtp_session_set_rtcp_report_interval(fec_session, 2500);
		rtp_session_enable_avpf_feature(fec_session, ORTP_AVPF_FEATURE_TMMBR, TRUE);
		fec_session->fec_stream = NULL;
		ms->sessions.fec_session = fec_session;
	} else {
		rtp_session_reset_stats(ms->sessions.fec_session);
	}

	rtp_session_set_profile(ms->sessions.fec_session, profile);
	int payload_type_number = rtp_profile_get_payload_number_from_mime(profile, "flexfec");
	rtp_session_set_payload_type(ms->sessions.fec_session, payload_type_number);
	rtp_bundle_add_session(ms->sessions.rtp_session->bundle, mid, ms->sessions.fec_session);
	ms->fec_parameters = media_stream_extract_fec_params(fec_payload_type);
	ms->fec_stream = fec_stream_new(ms->sessions.rtp_session, ms->sessions.fec_session, ms->fec_parameters);
	ms_message("create or update FEC session [%p] with new FEC stream [%p], related to rtp_session [%p] in bundle [%p]",
	           ms->sessions.fec_session, ms->fec_stream, ms->sessions.rtp_session, ms->sessions.rtp_session->bundle);
	bctbx_free(mid);
}

void media_stream_destroy_fec_stream(MediaStream *ms) {
	fec_stream_unsubscribe(ms->fec_stream, ms->fec_parameters);
	fec_stream_destroy(ms->fec_stream);
	ms->fec_stream = NULL;
	ms->sessions.rtp_session->fec_stream = NULL;
}

void media_stream_enable_conference_local_mix(MediaStream *stream, bool_t enabled) {
	stream->local_mix_conference = enabled;
}

void media_stream_enable_transfer_mode(MediaStream *stream, bool_t enable) {
	stream->transfer_mode = enable;
}

bool_t media_stream_fec_enabled(MediaStream *stream) {
	return (stream->sessions.rtp_session->fec_stream != NULL);
}
