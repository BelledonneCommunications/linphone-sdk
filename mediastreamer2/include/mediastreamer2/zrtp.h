/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2014 Belledonne Communications

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef ms_zrtp_h
#define ms_zrtp_h

#include <ortp/rtpsession.h>
#include "mediastreamer2/mscommon.h"

#ifdef __cplusplus
extern "C"{
#endif

/* defined in mediastream.h */
struct _MediaStream; 

typedef struct MSZrtpParams {
	const char *zid_file; // File where to store secrets and other information
	const char *uri; /* the sip URI of correspondant */
} MSZrtpParams;

typedef struct _MSZrtpContext MSZrtpContext ;

MS2_PUBLIC bool_t ms_zrtp_available(void);

/**
  * @deprecated Use ms_zrtp_transport_modifier_new() instead. Using #srtp_transport_new will prevent usage of multiple
  * encryptions and/or custom packets transmission.
*/
MS2_PUBLIC MSZrtpContext* ms_zrtp_context_new(struct _MediaStream *stream, RtpSession *s, MSZrtpParams *params);
MS2_PUBLIC void ms_zrtp_context_destroy(MSZrtpContext *ctx);
/**
 * can be used to give more time for establishing zrtp session
 * */
MS2_PUBLIC void ms_zrtp_reset_transmition_timer(MSZrtpContext* ctx, RtpSession *s);

MS2_PUBLIC MSZrtpContext* ms_zrtp_multistream_new(struct _MediaStream *stream, MSZrtpContext* activeContext, RtpSession *s, MSZrtpParams *params);

MS2_PUBLIC void ms_zrtp_sas_verified(MSZrtpContext* ctx);
MS2_PUBLIC void ms_zrtp_sas_reset_verified(MSZrtpContext* ctx);



MS2_PUBLIC int ms_zrtp_transport_modifier_new(MSZrtpContext* ctx, RtpTransportModifier **rtpt, RtpTransportModifier **rtcpt );
MS2_PUBLIC void ms_zrtp_transport_modifier_destroy(RtpTransportModifier *tp);


#ifdef __cplusplus
}
#endif

#endif /* ms_zrtp_h */
