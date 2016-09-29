/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtls_srtp.h"

#ifdef _WIN32
#include <malloc.h>
#endif

#include "bctoolbox/crypto.h"



typedef struct _DtlsBcToolBoxContexts {
	bctbx_x509_certificate_t *crt;
	bctbx_ssl_config_t *ssl_config;
	bctbx_ssl_context_t *ssl;
	bctbx_rng_context_t *rng;
	bctbx_signing_key_t *pkey;
	ms_mutex_t ssl_context_mutex;
} DtlsBcToolBoxContext;

/**
 * incoming DTLS message are stored in a chain buffer to feed polarssl handshake when needed
 */
typedef struct _DtlsRawPacket {
	unsigned char *data;
	size_t length;
	void *next;
} DtlsRawPacket;

/* DTLS only allow use of AES128 so we have 16 bytes key and 14 byte salt in any case */
#define DTLS_SRTP_KEY_LEN 16
#define DTLS_SRTP_SALT_LEN 14

#define DTLS_STATUS_CONTEXT_NOT_READY 0
#define DTLS_STATUS_CONTEXT_READY 1
#define DTLS_STATUS_HANDSHAKE_ONGOING 2
#define DTLS_STATUS_HANDSHAKE_OVER 3

#define READ_TIMEOUT_MS 1000

struct _MSDtlsSrtpContext{
	MSMediaStreamSessions *stream_sessions;
	MSDtlsSrtpRole role; /**< can be unset(at init on caller side), client or server */
	char peer_fingerprint[256]; /**< used to store peer fingerprint passed through SDP */

	RtpTransportModifier *rtp_modifier;
	RtpTransportModifier *rtcp_modifier;

	DtlsBcToolBoxContext *rtp_dtls_context; /**< a structure containing all contexts needed by polarssl for RTP channel */
	DtlsBcToolBoxContext *rtcp_dtls_context; /**< a structure containing all contexts needed by polarssl for RTCP channel */

	uint8_t rtp_channel_status; /**< channel status : DTLS_STATUS_CONTEXT_NOT_READY, DTLS_STATUS_CONTEXT_READY, DTLS_STATUS_HANDSHAKE_ONGOING, DTLS_STATUS_HANDSHAKE_OVER */
	uint8_t rtcp_channel_status; /**< channel status : DTLS_STATUS_CONTEXT_NOT_READY, DTLS_STATUS_CONTEXT_READY, DTLS_STATUS_HANDSHAKE_ONGOING, DTLS_STATUS_HANDSHAKE_OVER */

	DtlsRawPacket *rtp_incoming_buffer; /**< buffer of incoming DTLS packet to be read by polarssl callback */
	DtlsRawPacket *rtcp_incoming_buffer; /**< buffer of incoming DTLS packet to be read by polarssl callback */

	uint64_t rtp_time_reference; /**< an epoch in ms, used to manage retransmission when we are client */
	uint64_t rtcp_time_reference; /**< an epoch in ms, used to manage retransmission when we are client */
};

/***********************************************/
/***** LOCAL FUNCTIONS                     *****/
/***********************************************/
/*** DtlsBcToolBox context create/dstroy ***/

DtlsBcToolBoxContext *ms_dtls_srtp_bctbx_context_new(void) {
	// allocate the memory
	DtlsBcToolBoxContext *ctx = ms_new0(DtlsBcToolBoxContext,1);

	// create and initialise the requested fields
	ctx->rng = bctbx_rng_context_new();
	ctx->pkey = bctbx_signing_key_new();
	ctx->crt = bctbx_x509_certificate_new();
	ctx->ssl_config = bctbx_ssl_config_new();
	ctx->ssl = bctbx_ssl_context_new();
	ms_mutex_init(&(ctx->ssl_context_mutex), NULL);

	return ctx;
}

void ms_dtls_srtp_bctbx_context_free(DtlsBcToolBoxContext *ctx) {
	if (ctx != NULL) {
		bctbx_rng_context_free(ctx->rng);
		bctbx_signing_key_free(ctx->pkey);
		bctbx_x509_certificate_free(ctx->crt);
		bctbx_ssl_context_free(ctx->ssl);
		bctbx_ssl_config_free(ctx->ssl_config);
		ms_mutex_destroy(&(ctx->ssl_context_mutex));

		ms_free(ctx);
	}
}


/**************************/
/**** Helper functions ****/
static ORTP_INLINE uint64_t get_timeval_in_millis(void) {
	struct timeval t;
	ortp_gettimeofday(&t,NULL);
	return (1000LL*t.tv_sec)+(t.tv_usec/1000LL);
}

/**
 * @Brief Compute the certificate fingerprint(hash of DER formated certificate)
 * hash function to use shall be the same used by certificate signature(this is a way to ensure that the hash function is available at both ends as they already agreed on certificate)
 * However, peer may provide a fingerprint generated with another hash function(indicated at the fingerprint header).
 * In case certificate and fingerprint hash function differs, issue a warning and use the fingerprint one
 *
 * @param[in]	certificate		Certificate we shall compute the fingerprint
 * @param[in]	peer_fingerprint	Fingerprint received from peer, check its header to get the hash function used to generate it
 *
 * @return 0 if the fingerprint doesn't match, 1 is they do.
 */
static uint8_t ms_dtls_srtp_check_certificate_fingerprint(const bctbx_x509_certificate_t *certificate, const char *peer_fingerprint) {
	char fingerprint[256]; /* maximum length of the fingerprint for sha-512: 8+3*64+1 so we're good with 256 bytes buffer */
	bctbx_md_type_t hash_function = BCTBX_MD_UNDEFINED;
	bctbx_md_type_t certificate_signature_hash_function = BCTBX_MD_UNDEFINED;
	int32_t ret = 0;

	/* get Hash algorithm used from peer fingerprint */
	if (strncasecmp(peer_fingerprint, "sha-1 ", 6) ==0 ) {
		hash_function = BCTBX_MD_SHA1;
	} else if (strncasecmp(peer_fingerprint, "sha-224 ", 8) ==0 ){
		hash_function = BCTBX_MD_SHA224;
	} else if (strncasecmp(peer_fingerprint, "sha-256 ", 8) ==0 ){
		hash_function = BCTBX_MD_SHA256;
	} else if (strncasecmp(peer_fingerprint, "sha-384 ", 8) ==0 ){
		hash_function = BCTBX_MD_SHA384;
	} else if (strncasecmp(peer_fingerprint, "sha-512 ", 8) ==0 ){
		hash_function = BCTBX_MD_SHA512;
	} else { /* we have an unknown hash function: return null */
		ms_error("DTLS-SRTP received invalid peer fingerprint, hash function unknown");
		return 0;
	}

	/* retrieve the one used for the certificate signature */
	bctbx_x509_certificate_get_signature_hash_function(certificate, &certificate_signature_hash_function);

	/* check that hash function used match the one used for certificate signature */
	if (hash_function != certificate_signature_hash_function) {
		ms_warning("DTLS-SRTP peer fingerprint generated using a different hash function that the one used for certificate signature, peer is nasty but lucky we have the hash function required anyway");
	}

	/* compute the fingerprint using the requested hash function */
	ret = bctbx_x509_certificate_get_fingerprint(certificate, fingerprint, 255, hash_function);
	if (ret <= 0) {
		ms_error("DTLS Handshake successful but unable to compute peer certificate fingerprint : bctoolbox returns [-0x%x]", -ret);
	}

	/* compare fingerprints */
	if (strncasecmp((const char *)fingerprint, peer_fingerprint, strlen((const char *)fingerprint)) == 0) {
		return 1;
	} else {
		ms_error("DTLS Handshake successful but fingerprints differ received : %s computed %s", peer_fingerprint, fingerprint);
		return 0;
	}
}

/**
 * Convert a bctoolbox defined value for SRTP protection profile to the mediastreamer enumeration of SRTP protection profile
 * @param[in]	dtls_srtp_protection_profile	A DTLS-SRTP protection profile defined by polarssl
 * @return the matching profile defined in mediatream.h
 */
static MSCryptoSuite ms_dtls_srtp_bctbx_protection_profile_to_ms_crypto_suite(bctbx_dtls_srtp_profile_t dtls_srtp_protection_profile) {
	switch(dtls_srtp_protection_profile) {
		case BCTBX_SRTP_AES128_CM_HMAC_SHA1_80:
			return MS_AES_128_SHA1_80;
		case BCTBX_SRTP_AES128_CM_HMAC_SHA1_32:
			return MS_AES_128_SHA1_32;
		case BCTBX_SRTP_NULL_HMAC_SHA1_80:
			return MS_NO_CIPHER_SHA1_80;
		case BCTBX_SRTP_NULL_HMAC_SHA1_32: /* this profile is defined in DTLS-SRTP rfc but not implemented by libsrtp */
			return MS_CRYPTO_SUITE_INVALID;
		default:
			return MS_CRYPTO_SUITE_INVALID;
	}
}

static void schedule_rtp(struct _RtpTransportModifier *t) {
	MSDtlsSrtpContext *ctx = (MSDtlsSrtpContext *)t->data;
	/* it is not a dtls packet, but manage anyway the retransmission timer */
	if (ctx->role == MSDtlsSrtpRoleIsClient) { /* only if we are client */
		uint64_t current_time = get_timeval_in_millis();
		if (ctx->rtp_time_reference>0) { /* only when retransmission timer is armed */
			if (current_time - ctx->rtp_time_reference > READ_TIMEOUT_MS) {
				ms_message("DTLS repeating rtp ssl_handshake for context [%p]",ctx);
				ms_mutex_lock(&ctx->rtp_dtls_context->ssl_context_mutex);
				bctbx_ssl_handshake(ctx->rtp_dtls_context->ssl);
				ms_mutex_unlock(&ctx->rtp_dtls_context->ssl_context_mutex);
				ctx->rtp_time_reference = get_timeval_in_millis();
			}
		}
	}

}
static void schedule_rtcp(struct _RtpTransportModifier *t) {
	MSDtlsSrtpContext *ctx = (MSDtlsSrtpContext *)t->data;
	if (ctx->role == MSDtlsSrtpRoleIsClient) { /* only if we are client */
		uint64_t current_time = get_timeval_in_millis();
		if (ctx->rtcp_time_reference>0) { /* only when retransmission timer is armed */
			if (current_time - ctx->rtcp_time_reference > READ_TIMEOUT_MS) {
				ms_message("DTLS repeating rtcp ssl_handshake for context [%p]",ctx);
				ms_mutex_lock(&ctx->rtcp_dtls_context->ssl_context_mutex);
				bctbx_ssl_handshake(ctx->rtcp_dtls_context->ssl);
				ms_mutex_unlock(&ctx->rtcp_dtls_context->ssl_context_mutex);
				ctx->rtcp_time_reference = get_timeval_in_millis();
			}
		}

	}
}
/**
 * Check if the incoming message is a DTLS packet.
 * If it is, store it in the context incoming buffer and call the polarssl function wich will process it.
 * This function also manages the client retransmission timer
 *
 * @param[in] 		msg	the incoming message
 * @param[in/out]	ctx	the context containing the incoming buffer to store the DTLS packet
 * @param[out]		ret	the value returned by the polarssl function processing the packet(ssl_handshake)
 * @param[in]		is_rtp	TRUE if we are dealing with a RTP channel packet, FALSE for RTCP channel
 * @return TRUE if packet is a DTLS one, false otherwise
 */
static bool_t ms_dtls_srtp_process_dtls_packet(mblk_t *msg, MSDtlsSrtpContext *ctx, int *ret, bool_t is_rtp) {
	size_t msgLength = msgdsize(msg);
	uint64_t *time_reference = (is_rtp == TRUE)?&(ctx->rtp_time_reference):&(ctx->rtcp_time_reference);
	bctbx_ssl_context_t *ssl = (is_rtp == TRUE)?ctx->rtp_dtls_context->ssl:ctx->rtcp_dtls_context->ssl;
	ms_mutex_t *mutex = (is_rtp == TRUE)?&ctx->rtp_dtls_context->ssl_context_mutex:&ctx->rtcp_dtls_context->ssl_context_mutex;
	uint8_t channel_status = (is_rtp == TRUE)?(ctx->rtp_channel_status):(ctx->rtcp_channel_status);

	// check if incoming message length is compatible with potential DTLS message
	if (msgLength<RTP_FIXED_HEADER_SIZE) {
		return FALSE;
	}

	/* check if it is a DTLS packet (first byte B as 19 < B < 64) rfc5764 section 5.1.2 */
	if ((*(msg->b_rptr)>19) && (*(msg->b_rptr)<64)) {

		DtlsRawPacket *incoming_dtls_packet;
		incoming_dtls_packet = (DtlsRawPacket *)ms_malloc0(sizeof(DtlsRawPacket));
		incoming_dtls_packet->next=NULL;
		incoming_dtls_packet->data=(unsigned char *)ms_malloc(msgLength);
		incoming_dtls_packet->length=msgLength;
		memcpy(incoming_dtls_packet->data, msg->b_rptr, msgLength);

		/*required by webrtc in server case when ice is not completed yet*/
		/* no more required because change is performed by ice.c once a check list is ready rtp_session_update_remote_sock_addr(rtp_session, msg,is_rtp,FALSE);*/

		ms_message("DTLS Receive %s packet len %d sessions: %p rtp session %p", is_rtp==TRUE?"RTP":"RTCP", (int)msgLength, ctx->stream_sessions, ctx->stream_sessions->rtp_session);

		/* store the packet in the incoming buffer */
		if (is_rtp == TRUE) {
			if (ctx->rtp_incoming_buffer==NULL) { /* buffer is empty */
				ctx->rtp_incoming_buffer = incoming_dtls_packet;
			} else { /* queue it at the end of current buffer */
				DtlsRawPacket *last_packet = ctx->rtp_incoming_buffer;
				while (last_packet->next != NULL) last_packet = last_packet->next;
				last_packet->next = incoming_dtls_packet;
			}
		} else {
			if (ctx->rtcp_incoming_buffer==NULL) { /* buffer is empty */
				ctx->rtcp_incoming_buffer = incoming_dtls_packet;
			} else { /* queue it at the end of current buffer */
				DtlsRawPacket *last_packet = ctx->rtcp_incoming_buffer;
				while (last_packet->next != NULL) last_packet = last_packet->next;
				last_packet->next = incoming_dtls_packet;
			}
		}
		
		/* while DTLS handshake is on going route DTLS packets to bctoolbox engine through ssl_handshake() */
		if (channel_status != DTLS_STATUS_HANDSHAKE_OVER) {
			/* role is unset but we receive a packet: we are caller and shall initialise as server and then process the incoming packet */
			if (ctx->role == MSDtlsSrtpRoleUnset) {
				ms_dtls_srtp_set_role(ctx, MSDtlsSrtpRoleIsServer); /* this call will update role and complete server setup */
				ms_dtls_srtp_start(ctx); /* complete the ssl setup and change channel_status to DTLS_STATUS_HANDSHAKE_ONGOING on both RTP and RTCP channel*/
			}
			ms_mutex_lock(mutex);
			/* process the packet and store result */
			*ret = bctbx_ssl_handshake(ssl);

			/* if we are client, manage the retransmission timer */
			if (ctx->role == MSDtlsSrtpRoleIsClient) {
				*time_reference = get_timeval_in_millis();
			}
			ms_mutex_unlock(mutex);
		} else { /* when DTLS handshake is over, route DTLS packets to bctoolbox engine through ssl_read() */
			/* we need a buffer to store the message read even if we don't use it */
			unsigned char *buf = ms_malloc(msgLength+1);
			ms_mutex_lock(mutex);
			*ret = bctbx_ssl_read(ssl, buf, msgLength);
			ms_mutex_unlock(mutex);
		}

		/* report the error in logs only when different than requested read(waiting for data) */
		if (*ret<0 && *ret != BCTBX_ERROR_NET_WANT_READ) {
			char err_str[512];
			err_str[0]='\0';
			bctbx_strerror(*ret, err_str, 512);
			ms_warning("DTLS handhake returns -0x%x : %s [on sessions: %p rtp session %p]", -*ret, err_str, ctx->stream_sessions, ctx->stream_sessions->rtp_session);
		}

		return TRUE;
	}

	return FALSE;

}

static void ms_dtls_srtp_check_channels_status(MSDtlsSrtpContext *ctx) {

	if (((ctx->rtp_channel_status == DTLS_STATUS_HANDSHAKE_OVER) && (rtp_session_rtcp_mux_enabled(ctx->stream_sessions->rtp_session)))
		|| ((ctx->rtp_channel_status == DTLS_STATUS_HANDSHAKE_OVER) && (ctx->rtcp_channel_status == DTLS_STATUS_HANDSHAKE_OVER))) {
		OrtpEventData *eventData;
		OrtpEvent *ev;
		/* send event */
		ev=ortp_event_new(ORTP_EVENT_DTLS_ENCRYPTION_CHANGED);
		eventData=ortp_event_get_data(ev);
		eventData->info.dtls_stream_encrypted=1;
		rtp_session_dispatch_event(ctx->stream_sessions->rtp_session, ev);
		ms_message("DTLS Event dispatched to all: secrets are on for this stream");
	} 
}

/********************************************/
/**** polarssl DTLS packet I/O functions ****/

/**
* Send a DTLS packet via RTP.
*
* DTLS calls this method to send a DTLS packet via the RTP session.
*
* @param ctx
*    Pointer to the MSDtlsSrtpContext structure.
* @param data
*    Points to DTLS message to send.
* @param length
*    The length in bytes of the data
* @return
*    length of data sent
*/
static int ms_dtls_srtp_rtp_sendData (void *ctx, const unsigned char *data, size_t length ){
	MSDtlsSrtpContext *context = (MSDtlsSrtpContext *)ctx;
	RtpSession *session = context->stream_sessions->rtp_session;
	RtpTransport *rtpt=NULL;
	mblk_t *msg;
	int ret;

	ms_message("DTLS Send RTP packet len %d sessions: %p rtp session %p", (int)length, context->stream_sessions, context->stream_sessions->rtp_session);

	/* get RTP transport from session */
	rtp_session_get_transports(session,&rtpt,NULL);

	/* generate message from raw data */
	msg = rtp_session_create_packet_raw((uint8_t *)data, length);

	ret = meta_rtp_transport_modifier_inject_packet_to_send(rtpt, context->rtp_modifier, msg , 0);

	freemsg(msg);
	return ret;
}

/**
* Send a DTLS packet via RTCP.
*
* DTLS calls this method to send a DTLS packet via the RTCP session.
*
* @param ctx
*    Pointer to the MSDtlsSrtpContext structure.
* @param data
*    Points to DTLS message to send.
* @param length
*    The length in bytes of the data
* @return
*    length of data sent
*/
static int ms_dtls_srtp_rtcp_sendData (void *ctx, const unsigned char *data, size_t length ){
	MSDtlsSrtpContext *context = (MSDtlsSrtpContext *)ctx;
	RtpSession *session = context->stream_sessions->rtp_session;
	RtpTransport *rtcpt=NULL;
	mblk_t *msg;
	int ret;

	ms_message("DTLS Send RTCP packet len %d sessions: %p rtp session %p", (int)length, context->stream_sessions, context->stream_sessions->rtp_session);

	/* get RTCP transport from session */
	rtp_session_get_transports(session,NULL,&rtcpt);

	/* generate message from raw data */
	msg = rtp_session_create_packet_raw((uint8_t *)data, length);

	ret = meta_rtp_transport_modifier_inject_packet_to_send(rtcpt, context->rtcp_modifier, msg , 0);
	freemsg(msg);

	return ret;
}


static int ms_dtls_srtp_rtp_DTLSread (void *ctx, unsigned char *buf, size_t len) {
	MSDtlsSrtpContext *context = (MSDtlsSrtpContext *)ctx;

	/* do we have something in the incoming buffer */
	if (context->rtp_incoming_buffer == NULL) {
		return BCTBX_ERROR_NET_WANT_READ;
	} else { /* read the first packet in the buffer and delete it */
		DtlsRawPacket *next_packet = context->rtp_incoming_buffer->next;
		size_t dataLength = context->rtp_incoming_buffer->length;
		memcpy(buf, context->rtp_incoming_buffer->data, dataLength);
		ms_free(context->rtp_incoming_buffer->data);
		ms_free(context->rtp_incoming_buffer);
		context->rtp_incoming_buffer = next_packet;

		return (int)dataLength;
	}
}

static int ms_dtls_srtp_rtcp_DTLSread (void *ctx, unsigned char *buf, size_t len) {
	MSDtlsSrtpContext *context = (MSDtlsSrtpContext *)ctx;

	/* do we have something in the incoming buffer */
	if (context->rtcp_incoming_buffer == NULL) {
		return BCTBX_ERROR_NET_WANT_READ;
	} else { /* read the first packet in the buffer and delete it */
		DtlsRawPacket *next_packet = context->rtcp_incoming_buffer->next;
		size_t dataLength = context->rtcp_incoming_buffer->length;
		memcpy(buf, context->rtcp_incoming_buffer->data, dataLength);
		ms_free(context->rtcp_incoming_buffer->data);
		ms_free(context->rtcp_incoming_buffer);
		context->rtcp_incoming_buffer = next_packet;

		return (int)dataLength;
	}
}


//static int ms_dtls_srtp_rtp_DTLSread_timeout (void *ctx, unsigned char *buf, size_t len, uint32_t timeout) {
//	return ms_dtls_srtp_rtp_DTLSread(ctx, buf, len); /* ms_dtls_srtp_DTLSread is non blocking */
//}

//static int ms_dtls_srtp_rtcp_DTLSread_timeout (void *ctx, unsigned char *buf, size_t len, uint32_t timeout) {
//	return ms_dtls_srtp_rtcp_DTLSread(ctx, buf, len); /* ms_dtls_srtp_DTLSread is non blocking */
//}


/*******************************************************/
/**** Transport Modifier Sender/Receiver functions  ****/

static int ms_dtls_srtp_rtp_process_on_receive(struct _RtpTransportModifier *t, mblk_t *msg){
	MSDtlsSrtpContext *ctx = (MSDtlsSrtpContext *)t->data;

	int ret;
	size_t msgLength = msgdsize(msg);

	/* check if we have an on-going handshake */
	if (ctx->rtp_channel_status == DTLS_STATUS_CONTEXT_NOT_READY) {
		return (int)msgLength;
	}

	// check incoming message length
	if (msgLength<RTP_FIXED_HEADER_SIZE) {
		return (int)msgLength;
	}

	/* check if it is a DTLS packet and process it */
	if (ms_dtls_srtp_process_dtls_packet(msg, ctx, &ret, TRUE) == TRUE){
		
		if ((ret==0) && (ctx->rtp_channel_status == DTLS_STATUS_HANDSHAKE_ONGOING)) { /* handshake is over, give the keys to srtp : 128 bits client write - 128 bits server write - 112 bits client salt - 112 server salt */
			MSCryptoSuite agreed_srtp_protection_profile = MS_CRYPTO_SUITE_INVALID;

			ctx->rtp_channel_status = DTLS_STATUS_HANDSHAKE_OVER;

			/* check the srtp profile get selected during handshake */
			agreed_srtp_protection_profile = ms_dtls_srtp_bctbx_protection_profile_to_ms_crypto_suite(bctbx_ssl_get_dtls_srtp_protection_profile(ctx->rtp_dtls_context->ssl));
			if ( agreed_srtp_protection_profile == MS_CRYPTO_SUITE_INVALID) {
				ms_message("DTLS Handshake successful but unable to agree on srtp_profile to use");
			} else {
				if (ms_dtls_srtp_check_certificate_fingerprint(bctbx_ssl_get_peer_certificate(ctx->rtp_dtls_context->ssl), (const char *)(ctx->peer_fingerprint)) == 1) {
					char dtls_srtp_key_material[128];
					size_t dtls_srt_key_material_length = 128;
					uint8_t *key = (uint8_t *)ms_malloc0(256);
					ms_message("DTLS Handshake on RTP channel successful and fingerprints match, srtp protection profile %d", agreed_srtp_protection_profile);

					ctx->rtp_time_reference = 0; /* unarm the timer */
					ret = bctbx_ssl_get_dtls_srtp_key_material(ctx->rtp_dtls_context->ssl, dtls_srtp_key_material, &dtls_srt_key_material_length);
					if (ret < 0) {
						ms_error("DTLS RTP Handshake : Unable to retrieve DTLS SRTP key material [-0x%x]", -ret);
						return 0;
					}

					if (ctx->role == MSDtlsSrtpRoleIsServer) {
						/* reception(client write) key and salt +16bits padding */
						memcpy(key, dtls_srtp_key_material, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, dtls_srtp_key_material+2*DTLS_SRTP_KEY_LEN, DTLS_SRTP_SALT_LEN);
						ms_media_stream_sessions_set_srtp_recv_key(ctx->stream_sessions, agreed_srtp_protection_profile, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTP_STREAM);
						/* emission(server write) key and salt +16bits padding */
						memcpy(key, dtls_srtp_key_material+DTLS_SRTP_KEY_LEN, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, dtls_srtp_key_material+2*DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, DTLS_SRTP_SALT_LEN);
						ms_media_stream_sessions_set_srtp_send_key(ctx->stream_sessions, agreed_srtp_protection_profile, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTP_STREAM);
					} else if (ctx->role == MSDtlsSrtpRoleIsClient){ /* this enpoint act as DTLS client */
						/* emission(client write) key and salt +16bits padding */
						memcpy(key, dtls_srtp_key_material, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, dtls_srtp_key_material+2*DTLS_SRTP_KEY_LEN, DTLS_SRTP_SALT_LEN);
						ms_media_stream_sessions_set_srtp_send_key(ctx->stream_sessions, agreed_srtp_protection_profile, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTP_STREAM);
						/* reception(server write) key and salt +16bits padding */
						memcpy(key, dtls_srtp_key_material+DTLS_SRTP_KEY_LEN, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, dtls_srtp_key_material+2*DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, DTLS_SRTP_SALT_LEN);
						ms_media_stream_sessions_set_srtp_recv_key(ctx->stream_sessions, agreed_srtp_protection_profile, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTP_STREAM);
					}

					ms_free(key);

					ms_dtls_srtp_check_channels_status(ctx);
				}
			}

			if (ctx->role != MSDtlsSrtpRoleIsServer) { /* close the connection only if we are client, if we are server, the client may ask again for last packets */
				/*FireFox version 43 requires DTLS channel to be kept openned, probably a bug in FireFox ret = ssl_close_notify( &(ctx->rtp_dtls_context->ssl) );*/
				
			}

		}
		return 0;
	}
	return (int)msgLength;
}

static int ms_dtls_srtp_rtcp_process_on_receive(struct _RtpTransportModifier *t, mblk_t *msg)  {
	MSDtlsSrtpContext *ctx = (MSDtlsSrtpContext *)t->data;

	int ret;
	size_t msgLength = msgdsize(msg);

	// check incoming message length
	if (msgLength<RTP_FIXED_HEADER_SIZE) {
		return (int)msgLength;
	}

	/* check if we have an on-going handshake */
	if (ctx->rtp_channel_status == DTLS_STATUS_CONTEXT_NOT_READY) {
		return (int)msgLength;
	}

	/* check if it is a DTLS packet and process it */
	if (ms_dtls_srtp_process_dtls_packet(msg, ctx, &ret, FALSE) == TRUE){

		if ((ret==0) && (ctx->rtcp_channel_status == DTLS_STATUS_HANDSHAKE_ONGOING)) { /* rtcp handshake is over, give the keys to srtp : 128 bits client write - 128 bits server write - 112 bits client salt - 112 server salt */
			MSCryptoSuite agreed_srtp_protection_profile = MS_CRYPTO_SUITE_INVALID;

			ctx->rtcp_channel_status = DTLS_STATUS_HANDSHAKE_OVER;

			/* check the srtp profile get selected during handshake */
			agreed_srtp_protection_profile = ms_dtls_srtp_bctbx_protection_profile_to_ms_crypto_suite(bctbx_ssl_get_dtls_srtp_protection_profile(ctx->rtcp_dtls_context->ssl));
			if ( agreed_srtp_protection_profile == MS_CRYPTO_SUITE_INVALID) {
				ms_error("DTLS RTCP Handshake successful but unable to agree on srtp_profile to use");
			} else {
				if (ms_dtls_srtp_check_certificate_fingerprint(bctbx_ssl_get_peer_certificate(ctx->rtcp_dtls_context->ssl), (const char *)(ctx->peer_fingerprint)) == 1) {
					char dtls_srtp_key_material[128];
					size_t dtls_srt_key_material_length = 128;
					uint8_t *key = (uint8_t *)ms_malloc0(256);

					ms_message("DTLS RTCP Handshake successful and fingerprints match, srtp protection profile %d", agreed_srtp_protection_profile);

					ctx->rtcp_time_reference = 0; /* unarm the timer */

					ret = bctbx_ssl_get_dtls_srtp_key_material(ctx->rtcp_dtls_context->ssl, dtls_srtp_key_material, &dtls_srt_key_material_length);
					if (ret < 0) {
						ms_error("DTLS RTCP Handshake : Unable to retrieve DTLS SRTP key material [-0x%x]", -ret);
						return 0;
					}

					if (ctx->role == MSDtlsSrtpRoleIsServer) {
						/* reception(client write) key and salt +16bits padding */
						memcpy(key, dtls_srtp_key_material, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, dtls_srtp_key_material+2*DTLS_SRTP_KEY_LEN, DTLS_SRTP_SALT_LEN);
						ms_media_stream_sessions_set_srtp_recv_key(ctx->stream_sessions, MS_AES_128_SHA1_80, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTCP_STREAM);
						/* emission(server write) key and salt +16bits padding */
						memcpy(key, dtls_srtp_key_material+DTLS_SRTP_KEY_LEN, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, dtls_srtp_key_material+2*DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, DTLS_SRTP_SALT_LEN);
						ms_media_stream_sessions_set_srtp_send_key(ctx->stream_sessions, MS_AES_128_SHA1_80, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTCP_STREAM);
					} else if (ctx->role == MSDtlsSrtpRoleIsClient){ /* this enpoint act as DTLS client */
						/* emission(client write) key and salt +16bits padding */
						memcpy(key, dtls_srtp_key_material, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, dtls_srtp_key_material+2*DTLS_SRTP_KEY_LEN, DTLS_SRTP_SALT_LEN);
						ms_media_stream_sessions_set_srtp_send_key(ctx->stream_sessions, MS_AES_128_SHA1_80, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTCP_STREAM);
						/* reception(server write) key and salt +16bits padding */
						memcpy(key, dtls_srtp_key_material+DTLS_SRTP_KEY_LEN, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, dtls_srtp_key_material+2*DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, DTLS_SRTP_SALT_LEN);
						ms_media_stream_sessions_set_srtp_recv_key(ctx->stream_sessions, MS_AES_128_SHA1_80, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTCP_STREAM);
					}

					ms_free(key);

					ms_dtls_srtp_check_channels_status(ctx);
				}
			}

			if (ctx->role != MSDtlsSrtpRoleIsServer) { /* close the connection only if we are client, if we are server, the client may ask again for last packets */
			/*FireFox version 43 requires DTLS channel to be kept openned, probably a bug in FireFox  ret = ssl_close_notify( &(ctx->rtcp_dtls_context->ssl) );*/
			}
		}

		return 0;
	} 
	return (int)msgdsize(msg);
}

static int ms_dtls_srtp_rtp_process_on_send(struct _RtpTransportModifier *t, mblk_t *msg){
	return (int)msgdsize(msg);
}
static int ms_dtls_srtp_rtcp_process_on_send(struct _RtpTransportModifier *t, mblk_t *msg)  {
	return (int)msgdsize(msg);
}

/**************************************/
/**** session management functions ****/
static void ms_dtls_srtp_transport_modifier_destroy(RtpTransportModifier *tp)  {
	ms_free(tp);
}

static int ms_dtls_srtp_transport_modifier_new(MSDtlsSrtpContext* ctx, RtpTransportModifier **rtpt, RtpTransportModifier **rtcpt ) {
	if (rtpt){
		*rtpt=ms_new0(RtpTransportModifier,1);
		(*rtpt)->data=ctx; /* back link to get access to the other fields of the OrtoZrtpContext from the RtpTransportModifier structure */
		(*rtpt)->t_process_on_send=ms_dtls_srtp_rtp_process_on_send;
		(*rtpt)->t_process_on_receive=ms_dtls_srtp_rtp_process_on_receive;
		(*rtpt)->t_process_on_schedule=schedule_rtp;
		(*rtpt)->t_destroy=ms_dtls_srtp_transport_modifier_destroy;
	}
	if (rtcpt){
		*rtcpt=ms_new0(RtpTransportModifier,1);
		(*rtcpt)->data=ctx; /* back link to get access to the other fields of the OrtoZrtpContext from the RtpTransportModifier structure */
		(*rtcpt)->t_process_on_send=ms_dtls_srtp_rtcp_process_on_send;
		(*rtcpt)->t_process_on_receive=ms_dtls_srtp_rtcp_process_on_receive;
		(*rtcpt)->t_process_on_schedule=schedule_rtcp;
		(*rtcpt)->t_destroy=ms_dtls_srtp_transport_modifier_destroy;
	}
	return 0;
}



static void ms_dtls_srtp_set_transport(MSDtlsSrtpContext *userData, RtpSession *s)  {
	RtpTransport *rtpt=NULL,*rtcpt=NULL;
	RtpTransportModifier *rtp_modifier, *rtcp_modifier;

	rtp_session_get_transports(s,&rtpt,&rtcpt);

	ms_dtls_srtp_transport_modifier_new(userData, &rtp_modifier,&rtcp_modifier);

	meta_rtp_transport_append_modifier(rtpt, rtp_modifier);
	meta_rtp_transport_append_modifier(rtcpt, rtcp_modifier);

	/* save transport modifier into context, needed to inject packets generated by DTLS */
	userData->rtp_modifier = rtp_modifier;
	userData->rtcp_modifier = rtcp_modifier;
}

static int ms_dtls_srtp_initialise_bctbx_dtls_context(DtlsBcToolBoxContext *dtlsContext, MSDtlsSrtpParams *params, RtpSession *s){
	int ret;
	bctbx_dtls_srtp_profile_t dtls_srtp_protection_profiles[2] = {BCTBX_SRTP_AES128_CM_HMAC_SHA1_80, BCTBX_SRTP_AES128_CM_HMAC_SHA1_32};
	
	/* initialise certificate */
	ret = bctbx_x509_certificate_parse( dtlsContext->crt, (const char *) params->pem_certificate, strlen( params->pem_certificate )+1 );
	if( ret < 0 ) {
		return ret;
	}
	
	ret =  bctbx_signing_key_parse( dtlsContext->pkey, (const char *) params->pem_pkey, strlen( params->pem_pkey )+1, NULL, 0 );
	if( ret != 0 ) {
		return ret;
	}

	/* configure ssl */
	if (params->role == MSDtlsSrtpRoleIsClient) {
		bctbx_ssl_config_defaults(dtlsContext->ssl_config, BCTBX_SSL_IS_CLIENT, BCTBX_SSL_TRANSPORT_DATAGRAM);
	} else { /* configure it by default as server, nothing is actually performed until we start the channel but this helps to get correct defaults settings */
		bctbx_ssl_config_defaults(dtlsContext->ssl_config, BCTBX_SSL_IS_SERVER, BCTBX_SSL_TRANSPORT_DATAGRAM);
	}

	bctbx_ssl_config_set_dtls_srtp_protection_profiles(  dtlsContext->ssl_config, dtls_srtp_protection_profiles, 2 ); /* TODO: get param from caller to select available profiles */

	bctbx_ssl_config_set_rng(dtlsContext->ssl_config, (int (*)(void *, unsigned char *, size_t))bctbx_rng_get, dtlsContext->rng);

	/* set certificates */
	/* this will force server to send his certificate to client as we need it to compute the fingerprint even if we won't verify it */
	bctbx_ssl_config_set_authmode(dtlsContext->ssl_config, BCTBX_SSL_VERIFY_OPTIONAL);
	bctbx_ssl_config_set_own_cert( dtlsContext->ssl_config, dtlsContext->crt, dtlsContext->pkey );
	/* This is useless as peer would certainly be a self signed certificate and we won't verify it but avoid runtime warnings */
	bctbx_ssl_config_set_ca_chain(dtlsContext->ssl_config, dtlsContext->crt);

	/* we are not ready yet to actually start the ssl context, this will be done by calling bctbx_ssl_setup when stream starts */
	return 0;

}

/***********************************************/
/***** EXPORTED FUNCTIONS                  *****/
/***********************************************/
/**** Private to mediastreamer2 functions ****/
/* header declared in voip/private.h */
void ms_dtls_srtp_set_stream_sessions(MSDtlsSrtpContext *dtls_context, MSMediaStreamSessions *stream_sessions) {
	if (dtls_context!=NULL) {
		dtls_context->stream_sessions = stream_sessions;
	}
}

/**** Public functions ****/
/* header declared in include/mediastreamer2/dtls_srtp.h */
bool_t ms_dtls_srtp_available(){
	return ms_srtp_supported()  && bctbx_dtls_srtp_supported();
}

void ms_dtls_srtp_set_peer_fingerprint(MSDtlsSrtpContext *context, const char *peer_fingerprint) {
	if (context) {
		size_t peer_fingerprint_length = strlen(peer_fingerprint)+1; // include the null termination
		if (peer_fingerprint_length>sizeof(context->peer_fingerprint)) {
			memcpy(context->peer_fingerprint, peer_fingerprint, sizeof(context->peer_fingerprint));
			ms_error("DTLS-SRTP received from SDP INVITE a peer fingerprint %d bytes length wich is longer than maximum storage %d bytes", (int)peer_fingerprint_length, (int)sizeof(context->peer_fingerprint));
		} else {
			memcpy(context->peer_fingerprint, peer_fingerprint, peer_fingerprint_length);
		}
	}
}

void ms_dtls_srtp_set_role(MSDtlsSrtpContext *context, MSDtlsSrtpRole role) {
	if (context) {
		ms_mutex_lock(&context->rtp_dtls_context->ssl_context_mutex);
		ms_mutex_lock(&context->rtcp_dtls_context->ssl_context_mutex);

		/* if role has changed and handshake already setup and going, reset the session */
		if (context->role != role && context->rtp_channel_status == DTLS_STATUS_HANDSHAKE_ONGOING ) {
			bctbx_ssl_session_reset( context->rtp_dtls_context->ssl );
		}
		if (context->role != role && context->rtcp_channel_status == DTLS_STATUS_HANDSHAKE_ONGOING ) {
			bctbx_ssl_session_reset( context->rtcp_dtls_context->ssl );
		}

		/* if role is isServer and was Unset, we must complete the server setup */
		if (((context->role == MSDtlsSrtpRoleIsClient) || (context->role == MSDtlsSrtpRoleUnset)) && (role == MSDtlsSrtpRoleIsServer)) {
			bctbx_ssl_config_set_endpoint(context->rtp_dtls_context->ssl_config, BCTBX_SSL_IS_SERVER);

			bctbx_ssl_config_set_endpoint(context->rtcp_dtls_context->ssl_config, BCTBX_SSL_IS_SERVER);
		}
		ms_message("DTLS set role from [%s] to [%s] for context [%p]"
				,context->role==MSDtlsSrtpRoleIsServer?"server":(context->role==MSDtlsSrtpRoleIsClient?"client":"unset role")
				,role==MSDtlsSrtpRoleIsServer?"server":(role==MSDtlsSrtpRoleIsClient?"client":"unset role")
				,context);
		context->role = role;
		ms_mutex_unlock(&context->rtp_dtls_context->ssl_context_mutex);
		ms_mutex_unlock(&context->rtcp_dtls_context->ssl_context_mutex);

	}
}

MSDtlsSrtpContext* ms_dtls_srtp_context_new(MSMediaStreamSessions *sessions, MSDtlsSrtpParams *params) {
	MSDtlsSrtpContext *userData;
	RtpSession *s = sessions->rtp_session;
	int ret;

	/* Create and init the polar ssl DTLS contexts */
	DtlsBcToolBoxContext *rtp_dtls_context = ms_dtls_srtp_bctbx_context_new();
	DtlsBcToolBoxContext *rtcp_dtls_context = ms_dtls_srtp_bctbx_context_new();

	ms_message("Creating DTLS-SRTP engine on session [%p] as %s", s, params->role==MSDtlsSrtpRoleIsServer?"server":(params->role==MSDtlsSrtpRoleIsClient?"client":"unset role"));

	/* create and link user data */
	userData=ms_new0(MSDtlsSrtpContext,1);
	userData->rtp_dtls_context=rtp_dtls_context;
	userData->rtcp_dtls_context=rtcp_dtls_context;
	userData->role = params->role;
	userData->rtp_time_reference = 0;
	userData->rtcp_time_reference = 0;

	userData->stream_sessions=sessions;
	userData->rtp_channel_status = 0;
	userData->rtcp_channel_status = 0;
	userData->rtp_incoming_buffer = NULL;
	userData->rtcp_incoming_buffer = NULL;
	userData->rtp_channel_status = DTLS_STATUS_CONTEXT_NOT_READY;
	userData->rtcp_channel_status = DTLS_STATUS_CONTEXT_NOT_READY;
	ms_dtls_srtp_set_transport(userData, s);

	ret = ms_dtls_srtp_initialise_bctbx_dtls_context(rtp_dtls_context, params, s);
	if (ret!=0) {
		ms_error("DTLS init error : rtp bctoolbox context init returned -0x%0x on stream session [%p]", -ret, sessions);
		return NULL;
	}
	ret = ms_dtls_srtp_initialise_bctbx_dtls_context(rtcp_dtls_context, params, s);
	if (ret!=0) {
		ms_error("DTLS init error : rtcp bctoolbox context init returned -0x%0x on stream session [%p]", -ret, sessions);
		return NULL;
	}

	/* set ssl transport functions */
	bctbx_ssl_set_io_callbacks( rtp_dtls_context->ssl, userData, ms_dtls_srtp_rtp_sendData, ms_dtls_srtp_rtp_DTLSread);
	bctbx_ssl_set_io_callbacks( rtcp_dtls_context->ssl, userData, ms_dtls_srtp_rtcp_sendData, ms_dtls_srtp_rtcp_DTLSread);

	userData->rtp_channel_status = DTLS_STATUS_CONTEXT_READY;
	userData->rtcp_channel_status = DTLS_STATUS_CONTEXT_READY;

	return userData;
}

void ms_dtls_srtp_start(MSDtlsSrtpContext* context) {
	if (context == NULL ) {
		ms_warning("DTLS start but no context\n");
		return;
	}
	ms_message("DTLS start stream on stream sessions [%p], RCTP mux is %s", context->stream_sessions, rtp_session_rtcp_mux_enabled(context->stream_sessions->rtp_session)?"enabled":"disabled");

	/* if we are client, start the handshake(send a clientHello) */
	if (context->role == MSDtlsSrtpRoleIsClient) {
		ms_mutex_lock(&context->rtp_dtls_context->ssl_context_mutex);
		bctbx_ssl_config_set_endpoint(context->rtp_dtls_context->ssl_config, BCTBX_SSL_IS_CLIENT);
		/* complete ssl setup*/
		bctbx_ssl_context_setup(context->rtp_dtls_context->ssl, context->rtp_dtls_context->ssl_config);
		/* and start the handshake */
		bctbx_ssl_handshake(context->rtp_dtls_context->ssl);
		context->rtp_time_reference = get_timeval_in_millis(); /* arm the timer for retransmission */
		context->rtp_channel_status = DTLS_STATUS_HANDSHAKE_ONGOING;
		ms_mutex_unlock(&context->rtp_dtls_context->ssl_context_mutex);
		/* We shall start handshake on RTCP channel too only if RTCP mux is not enabled */
		if (!rtp_session_rtcp_mux_enabled(context->stream_sessions->rtp_session)) {
			ms_mutex_lock(&context->rtcp_dtls_context->ssl_context_mutex);
			bctbx_ssl_config_set_endpoint(context->rtcp_dtls_context->ssl_config, BCTBX_SSL_IS_CLIENT);
			/* complete ssl setup*/
			bctbx_ssl_context_setup(context->rtcp_dtls_context->ssl, context->rtcp_dtls_context->ssl_config);
			/* and start the handshake */
			bctbx_ssl_handshake(context->rtcp_dtls_context->ssl);
			context->rtcp_time_reference = get_timeval_in_millis(); /* arm the timer for retransmission */
			context->rtcp_channel_status = DTLS_STATUS_HANDSHAKE_ONGOING;
			ms_mutex_unlock(&context->rtcp_dtls_context->ssl_context_mutex);
		}
	}

	/* if we are server and we didn't started yet the DTLS engine, do it now */
	if (context->role == MSDtlsSrtpRoleIsServer) {
		if (context->rtp_channel_status == DTLS_STATUS_CONTEXT_READY) {
			ms_mutex_lock(&context->rtp_dtls_context->ssl_context_mutex);
			bctbx_ssl_config_set_endpoint(context->rtp_dtls_context->ssl_config, BCTBX_SSL_IS_SERVER);
			/* complete ssl setup*/
			bctbx_ssl_context_setup(context->rtp_dtls_context->ssl, context->rtp_dtls_context->ssl_config);
			context->rtp_channel_status = DTLS_STATUS_HANDSHAKE_ONGOING;
			ms_mutex_unlock(&context->rtp_dtls_context->ssl_context_mutex);

			/* We shall start server on RTCP channel too only if RTCP mux is not enabled */
			if (!rtp_session_rtcp_mux_enabled(context->stream_sessions->rtp_session) && context->rtcp_channel_status == DTLS_STATUS_CONTEXT_READY) {
				ms_mutex_lock(&context->rtcp_dtls_context->ssl_context_mutex);
				bctbx_ssl_config_set_endpoint(context->rtcp_dtls_context->ssl_config, BCTBX_SSL_IS_SERVER);
				/* complete ssl setup*/
				bctbx_ssl_context_setup(context->rtcp_dtls_context->ssl, context->rtcp_dtls_context->ssl_config);
				context->rtcp_channel_status = DTLS_STATUS_HANDSHAKE_ONGOING;
				ms_mutex_unlock(&context->rtcp_dtls_context->ssl_context_mutex);
			}
		}
	}

}

void ms_dtls_srtp_context_destroy(MSDtlsSrtpContext *ctx) {
	/* clean bctoolbox contexts */
	ms_dtls_srtp_bctbx_context_free(ctx->rtp_dtls_context);
	ms_dtls_srtp_bctbx_context_free(ctx->rtcp_dtls_context);

	/* clean incoming buffers */
	while (ctx->rtp_incoming_buffer!=NULL) {
		DtlsRawPacket *next_packet = ctx->rtp_incoming_buffer->next;
		ms_free(ctx->rtp_incoming_buffer->data);
		ms_free(ctx->rtp_incoming_buffer);
		ctx->rtp_incoming_buffer = next_packet;
	}
	while (ctx->rtcp_incoming_buffer!=NULL) {
		DtlsRawPacket *next_packet = ctx->rtcp_incoming_buffer->next;
		ms_free(ctx->rtcp_incoming_buffer->data);
		ms_free(ctx->rtcp_incoming_buffer);
		ctx->rtcp_incoming_buffer = next_packet;
	}

	ms_free(ctx);
	ms_message("DTLS-SRTP context destroyed");
}
