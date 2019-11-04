/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef msrtt4103_h
#define msrtt4103_h

#include <mediastreamer2/msfilter.h>

#define TS_FLAG_NOTFIRST 0x01
#define TS_FLAG_NOCALLBACK 0x02

#define TS_OUTBUF_SIZE 1024
#define TS_REDGEN 2
#define TS_NUMBER_OF_OUTBUF TS_REDGEN + 1
#define TS_INBUF_SIZE TS_OUTBUF_SIZE * TS_NUMBER_OF_OUTBUF
#define TS_SEND_INTERVAL 299

#define MS_RTT_4103_SOURCE_SET_T140_PAYLOAD_TYPE_NUMBER MS_FILTER_METHOD(MS_RTT_4103_SOURCE_ID, 0, int)
#define MS_RTT_4103_SINK_SET_T140_PAYLOAD_TYPE_NUMBER MS_FILTER_METHOD(MS_RTT_4103_SINK_ID, 0, int)
#define MS_RTT_4103_SOURCE_SET_RED_PAYLOAD_TYPE_NUMBER MS_FILTER_METHOD(MS_RTT_4103_SOURCE_ID, 1, int)
#define MS_RTT_4103_SINK_SET_RED_PAYLOAD_TYPE_NUMBER MS_FILTER_METHOD(MS_RTT_4103_SINK_ID, 1, int)
#define MS_RTT_4103_SOURCE_PUT_CHAR32 MS_FILTER_METHOD(MS_RTT_4103_SOURCE_ID, 2, uint32_t)
#define MS_RTT_4103_SOURCE_SET_KEEP_ALIVE_INTERVAL MS_FILTER_METHOD(MS_RTT_4103_SOURCE_ID, 3, unsigned int)

typedef struct _RealtimeTextReceivedCharacter {
	uint32_t character;
} RealtimeTextReceivedCharacter;

#define MS_RTT_4103_RECEIVED_CHAR MS_FILTER_EVENT(MS_RTT_4103_SINK_ID, 0, RealtimeTextReceivedCharacter)

#endif