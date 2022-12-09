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


#ifndef msrtp_hh
#define msrtp_hh

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msvaddtx.h>
#include <ortp/ortp.h>

typedef int (*MSRtpSendRequestMixerToClientDataCb)(MSFilter *filter, rtp_audio_level_t **audio_levels, void *user_data);

struct _MSFilterRequestMixerToClientDataCb {
	MSRtpSendRequestMixerToClientDataCb cb;
	void *user_data;
};

typedef struct _MSFilterRequestMixerToClientDataCb MSFilterRequestMixerToClientDataCb;

typedef int (*MSRtpSendRequestClientToMixerDataCb)(MSFilter *filter, void *user_data);

struct _MSFilterRequestClientToMixerDataCb {
	MSRtpSendRequestClientToMixerDataCb cb;
	void *user_data;
};

typedef struct _MSFilterRequestClientToMixerDataCb MSFilterRequestClientToMixerDataCb;


#define MS_RTP_RECV_SET_SESSION			MS_FILTER_METHOD(MS_RTP_RECV_ID,0,RtpSession*)

#define MS_RTP_RECV_RESET_JITTER_BUFFER		MS_FILTER_METHOD_NO_ARG(MS_RTP_RECV_ID,1)

#define MS_RTP_RECV_SET_MIXER_TO_CLIENT_EXTENSION_ID	MS_FILTER_METHOD(MS_RTP_RECV_ID, 2, int)

#define MS_RTP_RECV_SET_CLIENT_TO_MIXER_EXTENSION_ID	MS_FILTER_METHOD(MS_RTP_RECV_ID, 3, int)

// The RTP_TRANSFER_MODE will bypass the extraction of the data and pass the packet in it's output as it is
#define MS_RTP_RECV_ENABLE_RTP_TRANSFER_MODE	MS_FILTER_METHOD(MS_RTP_RECV_ID, 4, bool_t)

#define MS_RTP_RECV_ENABLE_CSRC_EVENTS	MS_FILTER_METHOD(MS_RTP_RECV_ID, 5, bool_t)

#define MS_RTP_RECV_GENERIC_CN_RECEIVED		MS_FILTER_EVENT(MS_RTP_RECV_ID,0, MSCngData)

#define MS_RTP_RECV_MIXER_TO_CLIENT_AUDIO_LEVEL_RECEIVED		MS_FILTER_EVENT(MS_RTP_RECV_ID,1, rtp_audio_level_t[RTP_MAX_MIXER_TO_CLIENT_AUDIO_LEVEL])

#define MS_RTP_RECV_CLIENT_TO_MIXER_AUDIO_LEVEL_RECEIVED		MS_FILTER_EVENT(MS_RTP_RECV_ID,2, rtp_audio_level_t)

#define MS_RTP_RECV_CSRC_CHANGED		MS_FILTER_EVENT(MS_RTP_RECV_ID,3, uint32_t)


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

#define MS_RTP_SEND_SET_AUDIO_LEVEL_SEND_INTERVAL	MS_FILTER_METHOD(MS_RTP_SEND_ID, 10, int)

#define MS_RTP_SEND_SET_MIXER_TO_CLIENT_EXTENSION_ID	MS_FILTER_METHOD(MS_RTP_SEND_ID, 11, int)

#define MS_RTP_SEND_SET_MIXER_TO_CLIENT_DATA_REQUEST_CB	MS_FILTER_METHOD(MS_RTP_SEND_ID, 12, MSFilterRequestMixerToClientDataCb)

#define MS_RTP_SEND_SET_CLIENT_TO_MIXER_EXTENSION_ID	MS_FILTER_METHOD(MS_RTP_SEND_ID, 13, int)

#define MS_RTP_SEND_SET_CLIENT_TO_MIXER_DATA_REQUEST_CB	MS_FILTER_METHOD(MS_RTP_SEND_ID, 14, MSFilterRequestClientToMixerDataCb)

#define MS_RTP_SEND_SET_FRAME_MARKING_EXTENSION_ID	MS_FILTER_METHOD(MS_RTP_SEND_ID, 15, int)

// The RTP_TRANSFER_MODE will bypass the creation of the packet's header and send it as it is
// This is usefull to transfer packets that are already fully constructed
#define MS_RTP_SEND_ENABLE_RTP_TRANSFER_MODE	MS_FILTER_METHOD(MS_RTP_SEND_ID, 16, bool_t)

#define MS_RTP_SEND_SET_ACTIVE_SPEAKER_SSRC	MS_FILTER_METHOD(MS_RTP_SEND_ID, 17, uint32_t)

/**
 * Used to determine if DTMFs can be sent out-of-band
 */
#define MS_RTP_SEND_TELEPHONE_EVENT_SUPPORTED MS_FILTER_METHOD_NO_ARG(MS_RTP_SEND_ID, 18)

extern MSFilterDesc ms_rtp_send_desc;
extern MSFilterDesc ms_rtp_recv_desc;

#endif
