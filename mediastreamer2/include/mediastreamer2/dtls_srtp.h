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

#ifndef ms_dtls_srtp_h
#define ms_dtls_srtp_h

#include <ortp/rtpsession.h>
#include "mediastreamer2/mscommon.h"

#ifdef __cplusplus
extern "C"{
#endif

/* defined in mediastream.h */
struct _MSMediaStreamSessions;

typedef enum {
	MSDtlsSrtpRoleInvalid,
	MSDtlsSrtpRoleIsServer,
	MSDtlsSrtpRoleIsClient,
	MSDtlsSrtpRoleUnset
} MSDtlsSrtpRole;

typedef struct MSDtlsSrtpParams {
	const char *pem_certificate; /**< Self certificate in pem format */
	const char *pem_pkey; /**< Private key associated to self certificate */
	MSDtlsSrtpRole role; /**< Unset(at caller init, role is then choosen by responder but we must still be able to receive packets) */
} MSDtlsSrtpParams;

typedef struct _MSDtlsSrtpContext MSDtlsSrtpContext;

MS2_PUBLIC bool_t ms_dtls_srtp_available(void);

MS2_PUBLIC MSDtlsSrtpContext* ms_dtls_srtp_context_new(struct _MSMediaStreamSessions *sessions, MSDtlsSrtpParams *params);
MS2_PUBLIC void ms_dtls_srtp_start(MSDtlsSrtpContext* context);
MS2_PUBLIC void ms_dtls_srtp_context_destroy(MSDtlsSrtpContext *ctx);

MS2_PUBLIC void ms_dtls_srtp_set_role(MSDtlsSrtpContext *context, MSDtlsSrtpRole role);
MS2_PUBLIC void ms_dtls_srtp_set_peer_fingerprint(MSDtlsSrtpContext *context, const char *peer_fingerprint);

#ifdef __cplusplus
}
#endif

#endif /* ms_dtls_srtp_h */
