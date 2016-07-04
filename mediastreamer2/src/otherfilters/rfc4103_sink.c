/*
 * rfc4103_sink.c - Real time text RFC 4103 sender.
 *
 * Copyright (C) 2015  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msrtt4103.h"

typedef struct _RealTimeTextSinkData {
	int flags;
	int prevseqno;
	uint8_t inbuf[TS_INBUF_SIZE];
	size_t inbufsize;
	uint8_t* inbufpos;
	int pt_t140;
	int pt_red;
} RealTimeTextSinkData;

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
	for (i = 0, t = 0; i < (int)s; i++, t--) {
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

static void insert_lost_char(uint8_t *p){
	p[0] = 0xEF;
	p[1] = 0xBF;
	p[2] = 0xBD;
}

static int read_t140_data(RealTimeTextSinkData *stream, uint8_t *data, int readsize) {
	int buf_size = (int)TS_INBUF_SIZE - (int)stream->inbufsize;
	if (readsize < 0) {
		ms_warning("corrupt packet (readsize<0)");
		return -1;
	} else if (readsize > buf_size) {
		readsize = buf_size;
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

static void process_t140_packet(RealTimeTextSinkData *stream, mblk_t *packet) {
	int seqno = mblk_get_cseq(packet);
	uint8_t *payload = packet->b_rptr;
	size_t payloadsize = msgdsize(packet);

	ms_debug("t140 seqno:%i", seqno);
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
	if (read_t140_data(stream, payload, (int)payloadsize)) {
		return; /* return without updatting seqno */
	}
	stream->prevseqno = seqno;
}

static void process_red_packet(RealTimeTextSinkData *stream, mblk_t *packet) {
	int seqno = mblk_get_cseq(packet);
	uint8_t *payload = packet->b_rptr;
	size_t payloadsize = msgdsize(packet);
	int redgen = 0;
	int pos = 0;
	int redneeded;
	int readstart;
	/* check how many is red, also check if its the right payload for the red */
	
	ms_debug("red seqno:%i", seqno);
	while ((pos < (int)payloadsize) && (payload[pos] & (1 << 7))) {
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
	if (read_t140_data(stream, &payload[readstart], (int)payloadsize - readstart)) {
		ms_debug("error reading");
		return; /* return without updating seqno */
	}
	stream->prevseqno = seqno;
}

static bool_t read_text_packet(RealTimeTextSinkData *stream, mblk_t *packet) {
	stream->inbufpos = stream->inbuf;
	stream->inbufsize = 0;
	
	if (packet == NULL) {
		return FALSE;
	}
	
	if (stream->pt_red > 0) {
		process_red_packet(stream, packet);
	} else {
		process_t140_packet(stream, packet);
	}
	
	if (!(stream->flags & TS_FLAG_NOTFIRST)) {
		stream->flags |= TS_FLAG_NOTFIRST;
	}
	
	freemsg(packet);
	return TRUE;
}

static bool_t text_stream_ischar(RealTimeTextSinkData *stream) {
	if (stream->inbufsize) {
		return TRUE;
	}
	return FALSE;
}

static char text_stream_getchar(RealTimeTextSinkData *stream) {
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
	return '\0';
}

static uint32_t text_stream_getchar32(RealTimeTextSinkData *stream) {
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

static void ms_rtt_4103_sink_init(MSFilter *f) {
	RealTimeTextSinkData *s = ms_new0(RealTimeTextSinkData, 1);
	s->pt_red = 0;
	s->pt_t140 = 0;
	f->data = s;
}

static void ms_rtt_4103_sink_preprocess(MSFilter *f) {
	
}

static void ms_rtt_4103_sink_process(MSFilter *f) {
	RealTimeTextSinkData *s = (RealTimeTextSinkData *)f->data;
	mblk_t *im;
	
	ms_filter_lock(f);
	while((im = ms_queue_get(f->inputs[0])) != NULL) {
		read_text_packet(s, im);
		
		while (text_stream_ischar(s)) {
			uint32_t character = text_stream_getchar32(s);
			
			if (character != 0) {
				RealtimeTextReceivedCharacter *data = ms_new0(RealtimeTextReceivedCharacter, 1);
				data->character = character;
				ms_debug("Received char 32: %lu", (long unsigned) character);
				ms_filter_notify(f, MS_RTT_4103_RECEIVED_CHAR, data);
			} else {
				s->inbufsize = 0; // This will stop the text_stream_ischar
			}
		}
	}
	ms_filter_unlock(f);
}

static void ms_rtt_4103_sink_postprocess(MSFilter *f) {
	
}

static void ms_rtt_4103_sink_uninit(MSFilter *f) {
	ms_free(f->data);
}

static int ms_rtt_4103_sink_set_t140_payload(MSFilter *f, void *t140) {
	RealTimeTextSinkData *s = (RealTimeTextSinkData *)f->data;
	ms_filter_lock(f);
	s->pt_t140 = *(int *)t140;
	ms_debug("T140 payload number is %i", s->pt_t140);
	ms_filter_unlock(f);
	return 0;
}

static int ms_rtt_4103_sink_set_red_payload(MSFilter *f, void *red) {
	RealTimeTextSinkData *s = (RealTimeTextSinkData *)f->data;
	ms_filter_lock(f);
	s->pt_red = *(int *)red;
	ms_debug("RED payload number is %i", s->pt_red);
	ms_filter_unlock(f);
	return 0;
}

static MSFilterMethod ms_rtt_4103_sink_methods[] = {
	{ MS_RTT_4103_SINK_SET_T140_PAYLOAD_TYPE_NUMBER,	ms_rtt_4103_sink_set_t140_payload	},
	{ MS_RTT_4103_SINK_SET_RED_PAYLOAD_TYPE_NUMBER,		ms_rtt_4103_sink_set_red_payload	},
	{ 0,												NULL								}
};

MSFilterDesc ms_rtt_4103_sink_desc = {
	MS_RTT_4103_SINK_ID,
	"MSRTT4103Sink",
	"A filter to receive real time text",
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	ms_rtt_4103_sink_init,
	ms_rtt_4103_sink_preprocess,
	ms_rtt_4103_sink_process,
	ms_rtt_4103_sink_postprocess,
	ms_rtt_4103_sink_uninit,
	ms_rtt_4103_sink_methods,
	MS_FILTER_IS_PUMP
};

MS_FILTER_DESC_EXPORT(ms_rtt_4103_sink_desc)
