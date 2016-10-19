/*
 * rfc4103_source.c - Real time text RFC 4103 sender.
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

typedef struct _RealTimeTextSourceData {
	uint8_t buf[TS_NUMBER_OF_OUTBUF][TS_OUTBUF_SIZE];
	size_t bufsize[TS_NUMBER_OF_OUTBUF];
	int pribuf;
	uint32_t timestamp[TS_NUMBER_OF_OUTBUF];
	int pt_t140;
	int pt_red;
} RealTimeTextSourceData;

static uint32_t get_prev_time(const RealTimeTextSourceData *stream) {
	return stream->timestamp[stream->pribuf ? stream->pribuf-1 : TS_REDGEN];
}

static int get_last_buf(const RealTimeTextSourceData *stream) {
	return stream->pribuf == TS_REDGEN ? 0 : stream->pribuf + 1;
}

static void use_next_buf(RealTimeTextSourceData *stream) {
	stream->pribuf = get_last_buf(stream);
	stream->bufsize[stream->pribuf] = 0;
	stream->timestamp[stream->pribuf] = 0;
}

static int get_next_buf(const RealTimeTextSourceData *stream, const int cur) {
	if (cur == stream->pribuf){
		return -1;
	}
	return cur == TS_REDGEN ? 0 : cur + 1;
}

static uint32_t get_red_subheader(int pt, int offset, size_t length) {
	return (1 << 31) | ((0x7F & pt) << 24) | ((0x3FFF & offset) << 10) | (0x3FF & (int)length);
}

static mblk_t *realtime_text_stream_generate_red_packet(RealTimeTextSourceData *stream) {
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
			
			cur = get_next_buf(stream, cur);
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
	
	packet = allocb(payloadsize, 0);
	memcpy(packet->b_wptr, &payload, payloadsize);
	packet->b_wptr += payloadsize;
	mblk_set_marker_info(packet, mark);

	return packet;
}

static void ms_rtt_4103_source_putchar32(RealTimeTextSourceData *stream, uint32_t ic) {
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

static bool_t is_data_to_send(const RealTimeTextSourceData *stream) {
	if (stream->pt_red > 0) {
		int i;

		for (i = 0; i <= TS_REDGEN; i++) {
			if (stream->bufsize[i] > 0) {
				return TRUE;
			}
		}
		return FALSE;
	} else {
		return stream->bufsize[stream->pribuf] > 0;
	}
}

static mblk_t* send_data(RealTimeTextSourceData *stream, uint32_t timestamp) {
	int i = stream->pribuf;
	uint32_t prevtime = get_prev_time(stream);
	mblk_t *m = NULL;

	if (is_data_to_send(stream)) {
		if (timestamp < prevtime || (timestamp - prevtime) > TS_SEND_INTERVAL) {
			stream->timestamp[i] = timestamp;
			m = realtime_text_stream_generate_red_packet(stream);
			mblk_set_timestamp_info(m, timestamp);
			use_next_buf(stream);
		}
	} else {
		if (timestamp < prevtime || (timestamp - prevtime) > TS_KEEP_ALIVE_INTERVAL) {
			ms_rtt_4103_source_putchar32(stream, 0xFEFF); /* BOM */
			ms_debug("Sending BOM");
			return send_data(stream, timestamp);
		}
	}
	
	return m;
}

static void ms_rtt_4103_source_init(MSFilter *f) {
	RealTimeTextSourceData *s = ms_new0(RealTimeTextSourceData, 1);
	s->pt_red = 0;
	s->pt_t140 = 0;
	f->data = s;
}

static void ms_rtt_4103_source_preprocess(MSFilter *f) {
	
}

static void ms_rtt_4103_source_process(MSFilter *f) {
	RealTimeTextSourceData *s = (RealTimeTextSourceData *)f->data;
	uint32_t timestamp = (uint32_t)f->ticker->time;
	mblk_t *m;
	
	ms_filter_lock(f);
	m = send_data(s, timestamp);
	if (m) {
		ms_queue_put(f->outputs[0], m);
	}
	ms_filter_unlock(f);
}

static void ms_rtt_4103_source_postprocess(MSFilter *f) {
	
}

static void ms_rtt_4103_source_uninit(MSFilter *f) {
	ms_free(f->data);
}

static int ms_rtt_4103_source_set_t140_payload(MSFilter *f, void *t140) {
	RealTimeTextSourceData *s = (RealTimeTextSourceData *)f->data;
	
	ms_filter_lock(f);
	s->pt_t140 = *(int *)t140;
	ms_debug("T140 payload number is %i", s->pt_t140);
	ms_filter_unlock(f);
	
	return 0;
}

static int ms_rtt_4103_source_set_red_payload(MSFilter *f, void *red) {
	RealTimeTextSourceData *s = (RealTimeTextSourceData *)f->data;
	
	ms_filter_lock(f);
	s->pt_red = *(int *)red;
	ms_debug("RED payload number is %i", s->pt_red);
	ms_filter_unlock(f);
	
	return 0;
}

static int ms_rtt_4103_source_put_char32(MSFilter *f, void *character) {
	RealTimeTextSourceData *s = (RealTimeTextSourceData *)f->data;
	uint32_t char_to_send = *(uint32_t*) character;
	
	ms_filter_lock(f);
	ms_rtt_4103_source_putchar32(s, char_to_send);
	ms_debug("Sending char 32: %lu", (long unsigned) char_to_send);
	ms_filter_unlock(f);
	
	return 0;
}

static MSFilterMethod ms_rtt_4103_source_methods[] = {
	{ MS_RTT_4103_SOURCE_SET_T140_PAYLOAD_TYPE_NUMBER,	ms_rtt_4103_source_set_t140_payload	},
	{ MS_RTT_4103_SOURCE_SET_RED_PAYLOAD_TYPE_NUMBER,	ms_rtt_4103_source_set_red_payload	},
	{ MS_RTT_4103_SOURCE_PUT_CHAR32, 					ms_rtt_4103_source_put_char32		},
	{ 0,												NULL								}
};

MSFilterDesc ms_rtt_4103_source_desc = {
	MS_RTT_4103_SOURCE_ID,
	"MSRTT4103Source",
	"A filter to send real time text",
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	ms_rtt_4103_source_init,
	ms_rtt_4103_source_preprocess,
	ms_rtt_4103_source_process,
	ms_rtt_4103_source_postprocess,
	ms_rtt_4103_source_uninit,
	ms_rtt_4103_source_methods,
	0
};

MS_FILTER_DESC_EXPORT(ms_rtt_4103_source_desc)

