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


#ifndef msrtp_hh
#define msrtp_hh

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msvaddtx.h>
#include <ortp/ortp.h>

#define MS_RTP_RECV_SET_SESSION			MS_FILTER_METHOD(MS_RTP_RECV_ID,0,RtpSession*)

#define MS_RTP_RECV_RESET_JITTER_BUFFER		MS_FILTER_METHOD_NO_ARG(MS_RTP_RECV_ID,1)

#define MS_RTP_RECV_GENERIC_CN_RECEIVED		MS_FILTER_EVENT(MS_RTP_RECV_ID,0, MSCngData)


#define MS_RTP_SEND_SET_SESSION			MS_FILTER_METHOD(MS_RTP_SEND_ID,0,RtpSession*)

#define MS_RTP_SEND_SEND_DTMF			MS_FILTER_METHOD(MS_RTP_SEND_ID,1,const char)

#define MS_RTP_SEND_SET_DTMF_DURATION		MS_FILTER_METHOD(MS_RTP_SEND_ID,2,int)

#define MS_RTP_SEND_MUTE			MS_FILTER_METHOD_NO_ARG(MS_RTP_SEND_ID,3)

#define MS_RTP_SEND_UNMUTE			MS_FILTER_METHOD_NO_ARG(MS_RTP_SEND_ID,4)

#define MS_RTP_SEND_SET_RELAY_SESSION_ID	MS_FILTER_METHOD(MS_RTP_SEND_ID,5,const char *)

#define MS_RTP_SEND_SEND_GENERIC_CN		MS_FILTER_METHOD(MS_RTP_SEND_ID,6, const MSCngData)

#define MS_RTP_SEND_ENABLE_STUN			MS_FILTER_METHOD(MS_RTP_SEND_ID, 7, bool_t)

#define MS_RTP_SEND_ENABLE_TS_ADJUSTMENT	MS_FILTER_METHOD(MS_RTP_SEND_ID, 8, bool_t)

#define MS_RTP_SEND_ENABLE_STUN_FORCED			MS_FILTER_METHOD(MS_RTP_SEND_ID, 9, bool_t)


extern MSFilterDesc ms_rtp_send_desc;
extern MSFilterDesc ms_rtp_recv_desc;

#endif
