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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msrtt4103.h"
#include "mediastreamer2/msrtp.h"
#include "private.h"

#include <sys/types.h>

#ifndef _WIN32
	#include <sys/socket.h>
	#include <netdb.h>
#endif

static void text_stream_free(TextStream *stream) {
	media_stream_free(&stream->ms);
	if (stream->rttsource != NULL) ms_filter_destroy(stream->rttsource);
	if (stream->rttsink != NULL) ms_filter_destroy(stream->rttsink);
	ms_free(stream);
}

static void text_stream_process_rtcp(MediaStream *media_stream, mblk_t *m) {
}

TextStream *text_stream_new_with_sessions(MSFactory *factory, const MSMediaStreamSessions *sessions) {
	TextStream *stream = (TextStream *)ms_new0(TextStream, 1);
	stream->pt_red = 0;
	stream->pt_t140 = 0;

	stream->ms.type = MSText;
	media_stream_init(&stream->ms, factory, sessions);

	ms_factory_enable_statistics(factory, TRUE);
	ms_factory_reset_statistics(factory);

	rtp_session_resync(stream->ms.sessions.rtp_session);
	/*some filters are created right now to allow configuration by the application before start() */
	stream->ms.rtpsend = ms_factory_create_filter(factory, MS_RTP_SEND_ID);
	stream->ms.ice_check_list = NULL;
	stream->ms.qi = ms_quality_indicator_new(stream->ms.sessions.rtp_session);
	ms_quality_indicator_set_label(stream->ms.qi, "text");
	stream->ms.process_rtcp = text_stream_process_rtcp;

	return stream;
}

TextStream *text_stream_new(MSFactory *factory, int loc_rtp_port, int loc_rtcp_port, bool_t ipv6) {
	return text_stream_new2(factory, ipv6 ? "::" : "0.0.0.0", loc_rtp_port, loc_rtcp_port);
}

TextStream *text_stream_new2(MSFactory *factory, const char* ip, int loc_rtp_port, int loc_rtcp_port) {
	TextStream *stream;
	MSMediaStreamSessions sessions = {0};
	sessions.rtp_session = ms_create_duplex_rtp_session(ip, loc_rtp_port, loc_rtcp_port, ms_factory_get_mtu(factory));
	stream = text_stream_new_with_sessions(factory, &sessions);
	stream->ms.owns_sessions = TRUE;
	return stream;
}

TextStream* text_stream_start(TextStream *stream, RtpProfile *profile, const char *rem_rtp_addr, int rem_rtp_port, const char *rem_rtcp_addr, int rem_rtcp_port, int payload_type /* ignored */) {
	RtpSession *rtps = stream->ms.sessions.rtp_session;
	MSConnectionHelper h;
	
	rtp_session_set_profile(rtps, profile);
	if (rem_rtp_port > 0) rtp_session_set_remote_addr_full(rtps, rem_rtp_addr, rem_rtp_port, rem_rtcp_addr, rem_rtcp_port);
	if (rem_rtcp_port > 0) {
		rtp_session_enable_rtcp(rtps, TRUE);
	} else {
		rtp_session_enable_rtcp(rtps, FALSE);
	}

	stream->pt_t140 = rtp_profile_get_payload_number_from_mime_and_flag(profile, "t140", PAYLOAD_TYPE_FLAG_CAN_SEND);
	stream->pt_red = rtp_profile_get_payload_number_from_mime_and_flag(profile, "red", PAYLOAD_TYPE_FLAG_CAN_SEND);
	if (payload_type == stream->pt_t140) {
		ms_debug("Text payload type is T140");
	} else if (payload_type == stream->pt_red) {
		ms_debug("Text payload type is RED");
	} else {
		/* we dont know this kind of textstream... */
		ms_warning("Unknown type of textstream");
	}
	rtp_session_set_payload_type(rtps, payload_type);
	
	if (rem_rtp_port > 0) ms_filter_call_method(stream->ms.rtpsend, MS_RTP_SEND_SET_SESSION, rtps);
	stream->ms.rtprecv = ms_factory_create_filter(stream->ms.factory, MS_RTP_RECV_ID);
	ms_filter_call_method(stream->ms.rtprecv, MS_RTP_RECV_SET_SESSION, rtps);
	stream->ms.sessions.rtp_session = rtps;
	
	if (stream->ms.sessions.ticker == NULL) media_stream_start_ticker(&stream->ms);

	stream->rttsource = ms_factory_create_filter(stream->ms.factory, MS_RTT_4103_SOURCE_ID);
	stream->rttsink = ms_factory_create_filter(stream->ms.factory, MS_RTT_4103_SINK_ID);
	
	ms_filter_call_method(stream->rttsource, MS_RTT_4103_SOURCE_SET_T140_PAYLOAD_TYPE_NUMBER, &stream->pt_t140);
	ms_filter_call_method(stream->rttsink, MS_RTT_4103_SINK_SET_T140_PAYLOAD_TYPE_NUMBER, &stream->pt_t140);
	if (payload_type == stream->pt_red) {
		ms_filter_call_method(stream->rttsource, MS_RTT_4103_SOURCE_SET_RED_PAYLOAD_TYPE_NUMBER, &stream->pt_red);
		ms_filter_call_method(stream->rttsink, MS_RTT_4103_SINK_SET_RED_PAYLOAD_TYPE_NUMBER, &stream->pt_red);
	}
	
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, stream->rttsource, -1, 0);
	ms_connection_helper_link(&h, stream->ms.rtpsend, 0, -1);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, stream->ms.rtprecv, -1, 0);
	ms_connection_helper_link(&h, stream->rttsink, 0, -1);
	
	ms_ticker_attach_multiple(stream->ms.sessions.ticker, stream->rttsource, stream->ms.rtprecv, NULL);
	
	stream->ms.start_time = stream->ms.last_packet_time = ms_time(NULL);
	stream->ms.is_beginning = TRUE;
	stream->ms.state = MSStreamStarted;
	return stream;
}

void text_stream_stop(TextStream *stream) {
	if (stream->ms.sessions.ticker) {
		if (stream->ms.state == MSStreamPreparing) {
			text_stream_unprepare_text(stream);
		} else if (stream->ms.state == MSStreamStarted) {
			MSConnectionHelper h;
			stream->ms.state = MSStreamStopped;
			ms_ticker_detach(stream->ms.sessions.ticker, stream->rttsource);
			ms_ticker_detach(stream->ms.sessions.ticker, stream->ms.rtprecv);
			
			if (stream->ms.ice_check_list != NULL) {
				ice_check_list_print_route(stream->ms.ice_check_list, "Text session's route");
				stream->ms.ice_check_list = NULL;
			}
			
			rtp_stats_display(rtp_session_get_stats(stream->ms.sessions.rtp_session),
					"             TEXT SESSION'S RTP STATISTICS                ");
			
			ms_connection_helper_start(&h);
			ms_connection_helper_unlink(&h, stream->rttsource, -1, 0);
			ms_connection_helper_unlink(&h, stream->ms.rtpsend, 0, -1);
			ms_connection_helper_start(&h);
			ms_connection_helper_unlink(&h, stream->ms.rtprecv, -1, 0);
			ms_connection_helper_unlink(&h, stream->rttsink, 0, -1);
		}
	}
	ms_factory_log_statistics(stream->ms.factory);
	text_stream_free(stream);
	
}

void text_stream_iterate(TextStream *stream) {
	media_stream_iterate(&stream->ms);
}

void text_stream_putchar32(TextStream *stream, uint32_t ic) {
	if (stream->rttsource) {
		ms_filter_call_method(stream->rttsource, MS_RTT_4103_SOURCE_PUT_CHAR32, &ic);
	}
}

void text_stream_prepare_text(TextStream *stream){
	text_stream_unprepare_text(stream);
	stream->ms.rtprecv = ms_factory_create_filter(stream->ms.factory, MS_RTP_RECV_ID);
	rtp_session_set_payload_type(stream->ms.sessions.rtp_session, 0);
	rtp_session_enable_rtcp(stream->ms.sessions.rtp_session, FALSE);
	ms_filter_call_method(stream->ms.rtprecv, MS_RTP_RECV_SET_SESSION, stream->ms.sessions.rtp_session);
	stream->ms.voidsink = ms_factory_create_filter(stream->ms.factory, MS_VOID_SINK_ID);
	ms_filter_link(stream->ms.rtprecv, 0, stream->ms.voidsink, 0);
	media_stream_start_ticker(&stream->ms);
	ms_ticker_attach(stream->ms.sessions.ticker, stream->ms.rtprecv);
	stream->ms.state = MSStreamPreparing;
}

static void stop_preload_graph(TextStream *stream){
	ms_ticker_detach(stream->ms.sessions.ticker, stream->ms.rtprecv);
	ms_filter_unlink(stream->ms.rtprecv, 0, stream->ms.voidsink, 0);
	ms_filter_destroy(stream->ms.voidsink);
	ms_filter_destroy(stream->ms.rtprecv);
	stream->ms.voidsink = stream->ms.rtprecv = NULL;
}

void text_stream_unprepare_text(TextStream *stream){
	if (stream->ms.state == MSStreamPreparing) {
		stop_preload_graph(stream);
		stream->ms.state = MSStreamInitialized;
	}
}