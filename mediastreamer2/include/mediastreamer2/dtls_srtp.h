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

/* an opaque structure containing all context data needed by DTLS-SRTP */
typedef struct _MSDtlsSrtpContext MSDtlsSrtpContext;

/**
 * check if DTLS-SRTP is available
 * @return TRUE if it is available, FALSE if not
 */
MS2_PUBLIC bool_t ms_dtls_srtp_available(void);

/**
 * Create an initialise a DTLS-SRTP context
 * @param[in]	sessions	A link to the stream sessions structures, used to get rtp session to add transport modifier and needed to set SRTP sessions when keys are ready
 * @param[in]	params		Self certificate and private key to be used for this session. Role (client/server) may be given but can be set later
 * @return	a pointer to the opaque context structure needed by DTLS-SRTP
 */
MS2_PUBLIC MSDtlsSrtpContext* ms_dtls_srtp_context_new(struct _MSMediaStreamSessions *sessions, MSDtlsSrtpParams *params);

/**
 * Start the DTLS-SRTP channel: send DTLS ClientHello if we are client
 * @param[in/out]	context		the DTLS-SRTP context
 */
MS2_PUBLIC void ms_dtls_srtp_start(MSDtlsSrtpContext* context);

/**
 * Free ressources used by DTLS-SRTP context
 * @param[in/out]	context		the DTLS-SRTP context
 */
MS2_PUBLIC void ms_dtls_srtp_context_destroy(MSDtlsSrtpContext *ctx);

/**
 * Set DTLS role: server or client, called when SDP exchange reach the point where we can determine self role
 * @param[in/out]	context		the DTLS-SRTP context
 * @param[in]		role		Client/Server/Invalid/Unset according to SDP INVITE processing
 */
MS2_PUBLIC void ms_dtls_srtp_set_role(MSDtlsSrtpContext *context, MSDtlsSrtpRole role);

/**
 * Give to the DTLS-SRTP context the peer certificate fingerprint extracted from trusted SDP INVITE,
 * it will be compared(case insensitive) with locally computed one after DTLS handshake is completed successfully and peer certicate retrieved
 * @param[in/out]	context			the DTLS-SRTP context
 * @param[in]		peer_fingerprint	a null terminated string containing the peer certificate as found in the SDP INVITE(including the heading hash algorithm name)
 */
MS2_PUBLIC void ms_dtls_srtp_set_peer_fingerprint(MSDtlsSrtpContext *context, const char *peer_fingerprint);

#ifdef __cplusplus
}
#endif

#endif /* ms_dtls_srtp_h */
