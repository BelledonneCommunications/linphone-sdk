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

#include "mediastreamer2/mediastream.h"
#include "private.h"

#include <sys/types.h>

#ifndef _WIN32
	#include <sys/socket.h>
	#include <netdb.h>
#endif

static void text_stream_free(TextStream *stream) {
	media_stream_free(&stream->ms);
	ms_free(stream);
}

static void text_stream_hexdump(const uint8_t* data, const size_t s) {
	char buf[1000];
	const uint8_t* c = data;
	int pos = 0;
	int i = 0;
	
	buf[0] = '\0';
	while(c < data + s) {
		pos += snprintf(&buf[pos], 1000 - pos, " ['%c']0x%X", *c > 20 ? *c : 'X', *c);
		c++;
		i++;
	}
	
	buf[pos] = '\0';
	ms_debug("%s", buf);
}

static uint32_t text_stream_gettime() 
{
	struct timeval t;
	ortp_gettimeofday(&t, NULL);
	return (t.tv_sec * 1000 + t.tv_usec / 1000);
}

/**
 * How many bytes makes up one character.
 **/
static int utf8_test(const uint8_t c) {
	if (!(c & 0x80)) { return 1; }
	else if (!(c & 0x40)) { return 0; }
	else if (!(c & 0x20)) { return 2; }
	else if (!(c & 0x10)) { return 3; }
	else if (!(c & 0x08)) { return 4; }
	else { return -1; }
}

static bool_t is_utf8_buf_ok(const uint8_t* b, const size_t s) {
	int i, t;
	for (i = 0, t = 0; i < s; i++, t--) {
		if (t == 0) {
			t = utf8_test(b[i]);
			if (t <= 0) {
				return FALSE;
			}
		} else {
			if (utf8_test(b[i]) != 0) {
				return FALSE;
			}
		}
	}
	if (t) {
		return FALSE;
	}
	return TRUE; /* SUCCESS */
}

static int red_needed(int cur, int prev) {
	int t = cur-prev;
	if (t > 0) {
		return t - 1;
	} else if (t < -100) {
		return t + 0xFFFF;
	}
	return -1;
}

static inline bool_t is_data_to_send(const TextStream *stream) {
	int i;

	for (i = 0; i <= TS_REDGEN; i++) {
		if (stream->bufsize[i]) {
			return TRUE;
		}
	}
	return FALSE;
}

static inline int get_last_buf(const TextStream *stream) {
	return stream->pribuf == TS_REDGEN ? 0 : stream->pribuf + 1;
}

static inline int get_next_buf(const TextStream *stream, const int cur) {
	if (cur == stream->pribuf){
		return -1;
	}
	return cur == TS_REDGEN ? 0 : cur + 1;
}

static void use_next_buf(TextStream *stream) {
	stream->pribuf = get_last_buf(stream);
	stream->bufsize[stream->pribuf] = 0;
	stream->timestamp[stream->pribuf] = 0;
}

static inline uint32_t get_prev_time(const TextStream *stream) {
	return stream->timestamp[stream->pribuf ? stream->pribuf-1 : TS_REDGEN];
}

static inline uint32_t get_red_subheader(int pt, int offset, size_t length) {
	return (1 << 31) | ((0x7F & pt) << 24) | ((0x3FFF & offset) << 10) | (0x3FF & (int)length);
}

static mblk_t *realtime_text_stream_generate_red_packet(TextStream *stream) {
	int cur = get_last_buf(stream);
	uint8_t t140 = (stream->pt_t140 & 0x7F);
	int pri = stream->pribuf;
	mblk_t *packet;
	uint8_t payload[TS_OUTBUF_SIZE * TS_NUMBER_OF_OUTBUF + 4 * TS_REDGEN + 1];
	size_t payloadsize = 0;
	int mark = 1;

	/* this makes it possible to generate t140 with same function... */
	if (stream->pt_red > 0) {
		while (cur != pri) {
			uint32_t diff = stream->timestamp[pri] - stream->timestamp[cur];
			size_t size = stream->bufsize[cur];
			uint32_t sub = htonl(get_red_subheader(t140, diff, size));
			
			memcpy(&payload[payloadsize], &sub, 4);
			payloadsize += 4;
			
			cur = get_next_buf(stream,cur);
		}
		
		memcpy(&payload[payloadsize], &t140, 1);
		payloadsize += 1;

		cur = get_last_buf(stream);

		while (cur != pri) {
			if (stream->bufsize[cur]) {
				mark = 0;
				memcpy(&payload[payloadsize], &stream->buf[cur][0], stream->bufsize[cur]);
				payloadsize += stream->bufsize[cur];
			}

			cur = get_next_buf(stream, cur);
		}
	}

	if (stream->bufsize[pri]) {
		memcpy(&payload[payloadsize], &stream->buf[pri][0], stream->bufsize[pri]);
		payloadsize += stream->bufsize[pri];
	}

	packet = rtp_session_create_packet(stream->ms.sessions.rtp_session, RTP_FIXED_HEADER_SIZE, payload, payloadsize);
	rtp_set_markbit(packet, mark);

	return packet;
}

static void send_data(TextStream *stream) {
	uint32_t time = text_stream_gettime();
	int i = stream->pribuf;
	uint32_t prevtime = get_prev_time(stream);

	if (is_data_to_send(stream)) {
		if (time < prevtime || (time - prevtime) > TS_SEND_INTERVAL) {
			stream->timestamp[i] = time;

			rtp_session_sendm_with_ts(stream->ms.sessions.rtp_session, realtime_text_stream_generate_red_packet(stream), time);
			use_next_buf(stream);
		}
	} else {
		if (time < prevtime || (time - prevtime) > TS_KEEP_ALIVE_INTERVAL) {
			text_stream_putchar32(stream, 0xFEFF); /* BOM */
			send_data(stream);
		}
	}
}

static void text_stream_process_rtcp(MediaStream *media_stream, mblk_t *m) {
}

TextStream *text_stream_new_with_sessions(const MSMediaStreamSessions *sessions) {
	TextStream *stream = (TextStream *)ms_new0(TextStream, 1);

	stream->ms.type = MSText;
	stream->ms.sessions = *sessions;
	media_stream_init(&stream->ms);

	ms_filter_enable_statistics(TRUE);
	ms_filter_reset_statistics();

	if (sessions->zrtp_context != NULL) {
		ms_zrtp_set_stream_sessions(sessions->zrtp_context, &(stream->ms.sessions));
	}
	if (sessions->dtls_context != NULL) {
		ms_dtls_srtp_set_stream_sessions(sessions->dtls_context, &(stream->ms.sessions));
	}
	rtp_session_resync(stream->ms.sessions.rtp_session);
	/*some filters are created right now to allow configuration by the application before start() */
	stream->ms.rtpsend = ms_filter_new(MS_RTP_SEND_ID);
	stream->ms.ice_check_list = NULL;
	stream->ms.qi = ms_quality_indicator_new(stream->ms.sessions.rtp_session);
	ms_quality_indicator_set_label(stream->ms.qi, "text");
	stream->ms.process_rtcp = text_stream_process_rtcp;

	return stream;
}

TextStream *text_stream_new(int loc_rtp_port, int loc_rtcp_port, bool_t ipv6) {
	return text_stream_new2(ipv6 ? "::" : "0.0.0.0", loc_rtp_port, loc_rtcp_port);
}

TextStream *text_stream_new2(const char* ip, int loc_rtp_port, int loc_rtcp_port) {
	TextStream *stream;
	MSMediaStreamSessions sessions = {0};
	sessions.rtp_session = ms_create_duplex_rtp_session(ip, loc_rtp_port, loc_rtcp_port);
	stream = text_stream_new_with_sessions(&sessions);
	stream->ms.owns_sessions = TRUE;
	return stream;
}

TextStream* text_stream_start(TextStream *stream, RtpProfile *profile, const char *rem_rtp_addr, int rem_rtp_port, const char *rem_rtcp_addr, int rem_rtcp_port, int payload_type /* ignored */) {
	rtp_session_set_profile(stream->ms.sessions.rtp_session, profile);

	if (rem_rtp_port > 0) {
		int iRes = rtp_session_set_remote_addr_full(stream->ms.sessions.rtp_session, rem_rtp_addr, rem_rtp_port, rem_rtcp_addr, rem_rtcp_port);

		if (iRes != 0)
			ms_error("something wrong in realtime_text_stream_start(...)");
	}

	stream->pt_t140 = rtp_profile_get_payload_number_from_mime(profile, "t140");
	stream->pt_red = rtp_profile_get_payload_number_from_mime(profile, "red");
	
	if (stream->pt_red > 0 && stream->pt_t140 > 0) {
		rtp_session_set_payload_type(stream->ms.sessions.rtp_session, stream->pt_red);
	} else if (stream->pt_t140 > 0) {
		rtp_session_set_payload_type(stream->ms.sessions.rtp_session, stream->pt_t140);
	} else {
		/* we dont know this kind of textstream... */
		ms_warning("unkown type of textstream");
	}
	
	stream->ms.state = MSStreamStarted;
	return stream;
}

void text_stream_stop(TextStream *stream) {
	stream->ms.state = MSStreamStopped;
	text_stream_free(stream);
}

static void insert_lost_char(uint8_t *p){
	p[0] = 0xEF;
	p[1] = 0xBF;
	p[2] = 0xBD;
}

static int read_t140_data(TextStream *stream, uint8_t *data, int readsize){
	if (readsize < 0) {
		ms_warning("corrupt packet (readsize<0)");
		return -1;
	} else if (readsize > TS_INBUF_SIZE - stream->inbufsize) {
		readsize = (int)(TS_INBUF_SIZE - stream->inbufsize);
		ms_warning("reading less characters than in buffer");
		
		/*POTENTIAL BUG... if last char is a multi char but just parts of it get 
		in, next function fails and whole buf is ignored. */
	}
	if (readsize > 0) {
		ms_debug("Reading %i bytes\n", readsize);
		if (!is_utf8_buf_ok(data, readsize)) {
			text_stream_hexdump(data, readsize);
			ms_warning("not a valid utf8 payload");
			stream->inbufsize = 0;
			return -1;
		}
		memcpy(&stream->inbuf[stream->inbufsize], data, readsize);
		stream->inbufsize += readsize;
	}
	return 0; /* 0 on success */
}

static void process_t140_packet(TextStream *stream, mblk_t *packet) {
	int seqno = rtp_get_seqnumber(packet);
	uint8_t *payload;
	int payloadsize = rtp_get_payload(packet, &payload);

	ms_debug("t140 seqno:%i",seqno);
	if (stream->flags & TS_FLAG_NOTFIRST) {
		int t = red_needed(seqno, stream->prevseqno);
		if (t < 0) {
			ms_warning("packet arrived out of order");
			return;
		} else if (t > 0) {
			stream->inbufsize = 3;
			insert_lost_char(stream->inbuf);
		}
	}
	if (read_t140_data(stream, payload, payloadsize)) {
		return; /* return without updatting seqno */
	}
	stream->prevseqno = seqno;
}

static void process_red_packet(TextStream *stream, mblk_t *packet) {
	int seqno = rtp_get_seqnumber(packet);
	uint8_t *payload;
	int redgen = 0;
	int pos = 0;
	int redneeded;
	int readstart;
	int payloadsize = rtp_get_payload(packet, &payload);
	/* check how many is red, also check if its the right payload for the red */
	
	ms_debug("red seqno:%i", seqno);
	while ((pos < payloadsize) && (payload[pos] & (1 << 7))) {
		redgen++;
		if (((int)payload[pos] & 0x7F) != stream->pt_t140) {
			ms_warning("invalid red packet");
			return;
		}
		pos += 4;
	}
	
	ms_debug("red redgen:%i",redgen);
	if ((int)payload[pos] != stream->pt_t140) {
		ms_warning("invalid red packet");
		return;
	}
	if (stream->flags & TS_FLAG_NOTFIRST) {
		redneeded = red_needed(seqno, stream->prevseqno);
	} else {
		redneeded = 0;
	}
	if (redneeded < 0) {
		ms_warning("packet arrived out of order");
		return;
	}
	
	ms_debug("red redneeded:%i", redneeded);
	if (redneeded > redgen) {
		/* we need more red than we got */
		stream->inbufsize = 3;
		insert_lost_char(stream->inbuf);
		redneeded = redgen;
	}
	/* loop over unneeded red */
	readstart = redgen * 4 + 1;
	for (pos = 0; pos < (redgen - redneeded) * 4; pos += 4) {
		readstart += (((uint32_t)payload[pos + 2] << 8) | (uint32_t)payload[pos + 3]) & 0x3FF;
	}
	if (read_t140_data(stream, &payload[readstart], payloadsize - readstart)) {
		ms_debug("error reading");
		return; /* return without updating seqno */
	}
	stream->prevseqno = seqno;
}

static bool_t read_text_packet(TextStream *stream) {
	uint32_t time = text_stream_gettime();
	mblk_t *packet = rtp_session_recvm_with_ts(stream->ms.sessions.rtp_session, time);
	int pt;

	stream->inbufpos = stream->inbuf;
	stream->inbufsize = 0;
	
	if (packet == NULL) {
		return FALSE;
	}
	pt = rtp_get_payload_type(packet);
	if (pt == stream->pt_t140) {
		process_t140_packet(stream, packet);
	} else if (stream->pt_red && pt == stream->pt_red) {
		process_red_packet(stream,packet);
	} else {
		ms_warning("unkown pt for text packet");
		ms_debug("p:%i t:%i r:%i", pt, stream->pt_t140, stream->pt_red);
	}
	if (!(stream->flags & TS_FLAG_NOTFIRST)) {
		stream->flags |= TS_FLAG_NOTFIRST;
	}
	freemsg(packet);
	return TRUE;
}

static bool_t text_stream_ischar(TextStream *stream) {
	if (stream->inbufsize) {
		return TRUE;
	}
	return read_text_packet(stream);
}

void text_stream_iterate(TextStream *stream) {
	send_data(stream);

	while (text_stream_ischar(stream)) {
		OrtpEvent *ev;
		OrtpEventData *eventData;
		uint32_t character = text_stream_getchar32(stream);
		
		if (character != 0) {
			ev = ortp_event_new(ORTP_EVENT_RTT_CHARACTER_RECEIVED);
			eventData = ortp_event_get_data(ev);
			eventData->info.received_rtt_character = character;
			rtp_session_dispatch_event(stream->ms.sessions.rtp_session, ev);
		}
	}
}

void text_stream_putchar(TextStream *stream, const char c) {
	int i = stream->pribuf;

	if (stream->bufsize[i] < TS_OUTBUF_SIZE) {
		stream->buf[i][stream->bufsize[i]++] = c;
	} else {
		ms_warning("Text stream out buf is full, can not add more characters to buffer!");
	}
}

void text_stream_putchar32(TextStream *stream, uint32_t ic) {
	int i = stream->pribuf;
	uint8_t* c = &stream->buf[i][stream->bufsize[i]];
	if (ic < 0x80) {
		if (stream->bufsize[i] < TS_OUTBUF_SIZE) {
			c[0] = ic;
			stream->bufsize[i]++;
		}
	} else if (ic < 0x800) {
		if (stream->bufsize[i] + 1 < TS_OUTBUF_SIZE) {
			c[1] = 0x80 + ((ic & 0x3F));
			c[0] = 0xC0 + ((ic >> 6) & 0x1F);
			stream->bufsize[i] += 2;
		}
	} else if (ic < 0x100000) {
		if (stream->bufsize[i] + 2 < TS_OUTBUF_SIZE) {
			c[2] = 0x80 + (ic & 0x3F);
			c[1] = 0x80 + ((ic >> 6) & 0x3F);
			c[0] = 0xE0 + ((ic >> 12) & 0xF);
			stream->bufsize[i] += 3;
		}
	} else if (ic < 0x110000) {
		if (stream->bufsize[i] + 3 < TS_OUTBUF_SIZE) {
			c[3] = 0x80 + (ic & 0x3F);
			c[2] = 0x80 + ((ic >> 6) & 0x3F);
			c[1] = 0x80 + ((ic >> 12) & 0x3F);
			c[0] = 0xF0 + ((ic >> 18) & 0x7);
			stream->bufsize[i] += 4;
		}
	}
}

char text_stream_getchar(TextStream *stream) {
	uint8_t *p = stream->inbufpos;
	uint8_t *end = &stream->inbuf[stream->inbufsize];
	while (end > p) {
		if (p[0] != '\0') {
			if (end - p >= 3) {
				if (p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) { /* BOM */
					p += 3;
					continue;
				}
			}
			stream->inbufpos = p + 1;
			return *p;
		}
		p++;
	}
	if (read_text_packet(stream)) {
		return text_stream_getchar(stream);
	}
	return '\0';
}

uint32_t text_stream_getchar32(TextStream *stream) {
	uint32_t c = text_stream_getchar(stream);
	int t = utf8_test(c);
	switch (t) {
		case 1:
			return c;
		case 2:
			c = (c & 0x1F) << 6;
			c += ((uint32_t) text_stream_getchar(stream) & 0x3F);
			return c;
		case 3:
			c = (c & 0x0F) << 12;
			c += (((uint32_t) text_stream_getchar(stream) & 0x3F) << 6);
			c += ((uint32_t) text_stream_getchar(stream) & 0x3F);
			return c;
		case 4:
			c = (c & 0x7) << 19;
			c += (((uint32_t) text_stream_getchar(stream) & 0x3F) << 12);
			c += (((uint32_t) text_stream_getchar(stream) & 0x3F) << 6);
			c += ((uint32_t) text_stream_getchar(stream) & 0x3F);
			return c;
		default:
			return 0;
	}
}