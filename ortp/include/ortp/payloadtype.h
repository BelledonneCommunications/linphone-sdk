/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of oRTP
 * (see https://gitlab.linphone.org/BC/public/ortp).
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

/**
 * \file payloadtype.h
 * \brief Definition of payload types
 *
 **/

#ifndef PAYLOADTYPE_H
#define PAYLOADTYPE_H
#include <ortp/port.h>

#ifdef __cplusplus
extern "C" {
#endif

/* flags for PayloadType::flags */

#define PAYLOAD_TYPE_ALLOCATED (1)
/*payload type represents a VBR codec*/
#define PAYLOAD_TYPE_IS_VBR (1 << 1)
#define PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED (1 << 2)
/* private flags for future use by ortp */
#define PAYLOAD_TYPE_PRIV1 (1 << 3)
/* user flags, can be used by the application on top of oRTP */
#define PAYLOAD_TYPE_USER_FLAG_0 (1 << 4)
#define PAYLOAD_TYPE_USER_FLAG_1 (1 << 5)
#define PAYLOAD_TYPE_USER_FLAG_2 (1 << 6)
#define PAYLOAD_TYPE_USER_FLAG_3 (1 << 7)
#define PAYLOAD_TYPE_USER_FLAG_4 (1 << 8)
#define PAYLOAD_TYPE_USER_FLAG_5 (1 << 9)
#define PAYLOAD_TYPE_USER_FLAG_6 (1 << 10)
#define PAYLOAD_TYPE_USER_FLAG_7 (1 << 11)
/* ask for more if you need*/

#define PAYLOAD_TYPE_FLAG_CAN_RECV PAYLOAD_TYPE_USER_FLAG_1
#define PAYLOAD_TYPE_FLAG_CAN_SEND PAYLOAD_TYPE_USER_FLAG_2

#define PAYLOAD_AUDIO_CONTINUOUS 0
#define PAYLOAD_AUDIO_PACKETIZED 1
#define PAYLOAD_VIDEO 2
#define PAYLOAD_TEXT 3
#define PAYLOAD_OTHER 4 /* ?? */

#define PAYLOAD_TYPE_AVPF_NONE 0
#define PAYLOAD_TYPE_AVPF_FIR (1 << 0)
#define PAYLOAD_TYPE_AVPF_PLI (1 << 1)
#define PAYLOAD_TYPE_AVPF_SLI (1 << 2)
#define PAYLOAD_TYPE_AVPF_RPSI (1 << 3)

struct _PayloadTypeAvpfParams {
	unsigned char features;    /**< A bitmask of PAYLOAD_TYPE_AVPF_* macros. */
	bool_t rpsi_compatibility; /*< Linphone uses positive feeback for RPSI. However first versions handling
	    AVPF wrongly declared RPSI as negative feedback, so this is kept for compatibility
	    with these versions but will probably be removed at some point in time. */
	uint16_t trr_interval;     /**< The interval in milliseconds between regular RTCP packets. */
};

struct _OrtpPayloadType {
	int type;             /**< one of PAYLOAD_* macros*/
	int clock_rate;       /**< rtp clock rate*/
	char bits_per_sample; /* in case of continuous audio data */
	char *zero_pattern;
	int pattern_length;
	/* other useful information for the application*/
	int normal_bitrate;                 /*in bit/s */
	char *mime_type;                    /**<actually the submime, ex: pcm, pcma, gsm*/
	int channels;                       /**< number of channels of audio */
	char *recv_fmtp;                    /* various format parameters for the incoming stream */
	char *send_fmtp;                    /* various format parameters for the outgoing stream */
	struct _PayloadTypeAvpfParams avpf; /* AVPF parameters */
	int flags;
	void *user_data;
};

#ifndef PayloadType_defined
#define PayloadType_defined
typedef struct _OrtpPayloadType OrtpPayloadType;
typedef OrtpPayloadType PayloadType;
typedef struct _PayloadTypeAvpfParams PayloadTypeAvpfParams;
#endif

#define payload_type_set_flag(pt, flag) (pt)->flags |= ((int)flag)
#define payload_type_unset_flag(pt, flag) (pt)->flags &= (~(int)flag)
#define payload_type_get_flags(pt) (pt)->flags

ORTP_PUBLIC PayloadType *payload_type_new(void);
ORTP_PUBLIC PayloadType *payload_type_clone(const PayloadType *payload);
ORTP_PUBLIC char *payload_type_get_rtpmap(PayloadType *pt);
ORTP_PUBLIC void payload_type_destroy(PayloadType *pt);
ORTP_PUBLIC void payload_type_set_recv_fmtp(PayloadType *pt, const char *fmtp);
ORTP_PUBLIC void payload_type_set_send_fmtp(PayloadType *pt, const char *fmtp);
ORTP_PUBLIC void payload_type_append_recv_fmtp(PayloadType *pt, const char *fmtp);
ORTP_PUBLIC void payload_type_append_send_fmtp(PayloadType *pt, const char *fmtp);
#define payload_type_get_avpf_params(pt) ((pt)->avpf)
ORTP_PUBLIC void payload_type_set_avpf_params(PayloadType *pt, PayloadTypeAvpfParams params);
ORTP_PUBLIC bool_t payload_type_is_vbr(const PayloadType *pt);

#define payload_type_get_bitrate(pt) ((pt)->normal_bitrate)
#define payload_type_get_rate(pt) ((pt)->clock_rate)
#define payload_type_get_mime(pt) ((pt)->mime_type)

ORTP_PUBLIC bool_t fmtp_get_value(const char *fmtp, const char *param_name, char *result, size_t result_len);

#define payload_type_set_user_data(pt, p) (pt)->user_data = (p)
#define payload_type_get_user_data(pt) ((pt)->user_data)

/* some payload types */
/* audio */
ORTP_VAR_PUBLIC PayloadType payload_type_pcmu8000;
ORTP_VAR_PUBLIC PayloadType payload_type_pcma8000;
ORTP_VAR_PUBLIC PayloadType payload_type_pcm8000;
ORTP_VAR_PUBLIC PayloadType payload_type_l16_mono;
ORTP_VAR_PUBLIC PayloadType payload_type_l16_stereo;
ORTP_VAR_PUBLIC PayloadType payload_type_lpc1016;
ORTP_VAR_PUBLIC PayloadType payload_type_g729;
ORTP_VAR_PUBLIC PayloadType payload_type_g7231;
ORTP_VAR_PUBLIC PayloadType payload_type_g7221;
ORTP_VAR_PUBLIC PayloadType payload_type_cn;
ORTP_VAR_PUBLIC PayloadType payload_type_g726_40;
ORTP_VAR_PUBLIC PayloadType payload_type_g726_32;
ORTP_VAR_PUBLIC PayloadType payload_type_g726_24;
ORTP_VAR_PUBLIC PayloadType payload_type_g726_16;
ORTP_VAR_PUBLIC PayloadType payload_type_aal2_g726_40;
ORTP_VAR_PUBLIC PayloadType payload_type_aal2_g726_32;
ORTP_VAR_PUBLIC PayloadType payload_type_aal2_g726_24;
ORTP_VAR_PUBLIC PayloadType payload_type_aal2_g726_16;
ORTP_VAR_PUBLIC PayloadType payload_type_gsm;
ORTP_VAR_PUBLIC PayloadType payload_type_lpc;
ORTP_VAR_PUBLIC PayloadType payload_type_lpc1015;
ORTP_VAR_PUBLIC PayloadType payload_type_speex_nb;
ORTP_VAR_PUBLIC PayloadType payload_type_speex_wb;
ORTP_VAR_PUBLIC PayloadType payload_type_speex_uwb;
ORTP_VAR_PUBLIC PayloadType payload_type_ilbc;
ORTP_VAR_PUBLIC PayloadType payload_type_amr;
ORTP_VAR_PUBLIC PayloadType payload_type_amrwb;
ORTP_VAR_PUBLIC PayloadType payload_type_truespeech;
ORTP_VAR_PUBLIC PayloadType payload_type_evrc0;
ORTP_VAR_PUBLIC PayloadType payload_type_evrcb0;
ORTP_VAR_PUBLIC PayloadType payload_type_aaceld_16k;
ORTP_VAR_PUBLIC PayloadType payload_type_aaceld_22k;
ORTP_VAR_PUBLIC PayloadType payload_type_aaceld_32k;
ORTP_VAR_PUBLIC PayloadType payload_type_aaceld_44k;
ORTP_VAR_PUBLIC PayloadType payload_type_aaceld_48k;
ORTP_VAR_PUBLIC PayloadType payload_type_opus;
ORTP_VAR_PUBLIC PayloadType payload_type_isac;
ORTP_VAR_PUBLIC PayloadType payload_type_gsm_efr;
ORTP_VAR_PUBLIC PayloadType payload_type_codec2;
ORTP_VAR_PUBLIC PayloadType payload_type_bv16;

/* video */
ORTP_VAR_PUBLIC PayloadType payload_type_mpv;
ORTP_VAR_PUBLIC PayloadType payload_type_h261;
ORTP_VAR_PUBLIC PayloadType payload_type_h263;
ORTP_VAR_PUBLIC PayloadType payload_type_h263_1998;
ORTP_VAR_PUBLIC PayloadType payload_type_h263_2000;
ORTP_VAR_PUBLIC PayloadType payload_type_mp4v;
ORTP_VAR_PUBLIC PayloadType payload_type_theora;
ORTP_VAR_PUBLIC PayloadType payload_type_h264;
ORTP_VAR_PUBLIC PayloadType payload_type_h265;
ORTP_VAR_PUBLIC PayloadType payload_type_x_snow;
ORTP_VAR_PUBLIC PayloadType payload_type_jpeg;
ORTP_VAR_PUBLIC PayloadType payload_type_vp8;
ORTP_VAR_PUBLIC PayloadType payload_type_av1;

ORTP_VAR_PUBLIC PayloadType payload_type_g722;
ORTP_VAR_PUBLIC PayloadType payload_type_flexfec;
/* text */
ORTP_VAR_PUBLIC PayloadType payload_type_t140;
ORTP_VAR_PUBLIC PayloadType payload_type_t140_red;

/* non standard file transfer over UDP */
ORTP_VAR_PUBLIC PayloadType payload_type_x_udpftp;

/* telephone-event */
ORTP_VAR_PUBLIC PayloadType payload_type_telephone_event;

#ifdef __cplusplus
}
#endif

#endif
