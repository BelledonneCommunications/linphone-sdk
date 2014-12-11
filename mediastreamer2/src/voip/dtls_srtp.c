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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtls_srtp.h"

#ifdef WIN32
#include <malloc.h>
#endif

#ifdef HAVE_dtls

#include <polarssl/ssl.h>
#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"
#include "polarssl/ssl_cookie.h"




typedef struct _DtlsPolarsslContexts {
	x509_crt crt;
	ssl_context ssl;
	entropy_context entropy;
    ctr_drbg_context ctr_drbg;
	ssl_cookie_ctx cookie_ctx;
    pk_context pkey;
} DtlsPolarsslContext;

/**
 * incoming DTLS message are stored in a chain buffer to feed polarssl handshake when needed
 */
typedef struct _DtlsRawPacket {
	unsigned char *data;
	size_t length;
	void *next;
} DtlsRawPacket;

#define DTLS_STATUS_CONTEXT_NOT_READY 0
#define DTLS_STATUS_CONTEXT_READY 1
#define DTLS_STATUS_ON_GOING_HANDSHAKE 2
#define DTLS_STATUS_HANDSHAKE_OVER 3

#define READ_TIMEOUT_MS 1000

struct _MSDtlsSrtpContext{
	RtpSession *session;
	MediaStream *stream;
	RtpTransportModifier *rtp_modifier;
	DtlsPolarsslContext *dtls_context; /**< a structure containing all contexts needed by polarssl */
	uint8_t channel_status; /**< channel status : DTLS_STATUS_CONTEXT_NOT_READY, DTLS_STATUS_CONTEXT_READY, DTLS_STATUS_ON_GOING_HANDSHAKE, DTLS_STATUS_HANDSHAKE_OVER */
	DtlsRawPacket *incoming_buffer; /**< buffer of incoming DTLS packet to be read by polarssl callback */
	MSDtlsSrtpRole role; /**< can be unset(at init on caller side), client or server */
	uint64_t time_reference; /**< an epoch in ms, used to manage retransmission when we are client */
	char peer_fingerprint[256]; /**< used to store peer fingerprint passed through SDP */
};

// Helper functions
static ORTP_INLINE uint64_t get_timeval_in_millis() {
	struct timeval t;
	ortp_gettimeofday(&t,NULL);
	return (1000LL*t.tv_sec)+(t.tv_usec/1000LL);
}

/* DTLS API */

bool_t ms_dtls_available(){return TRUE;}

static int ms_dtls_srtp_rtp_process_on_send(struct _RtpTransportModifier *t, mblk_t *msg){
	return msgdsize(msg);
}
static int ms_dtls_srtp_rtcp_process_on_send(struct _RtpTransportModifier *t, mblk_t *msg)  {
	return msgdsize(msg);
}

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
static int ms_dtls_srtp_sendData (void *ctx, const unsigned char *data, size_t length ){
	MSDtlsSrtpContext *context = (MSDtlsSrtpContext *)ctx;
	RtpSession *session = context->session;
	RtpTransport *rtpt=NULL;
	mblk_t *msg;

	ms_message("DTLS Send packet len %d :\n", (int)length);

	/* get RTP transport from session */
	rtp_session_get_transports(session,&rtpt,NULL);

	/* generate message from raw data */
 	msg = rtp_session_create_packet_raw((uint8_t *)data, (int)length);

	return meta_rtp_transport_modifier_inject_packet(rtpt, context->rtp_modifier, msg , 0);
}

static int ms_dtls_srtp_DTLSread (void *ctx, unsigned char *buf, size_t len) {
	MSDtlsSrtpContext *context = (MSDtlsSrtpContext *)ctx;

	/* do we have something in the incoming buffer */
	if (context->incoming_buffer == NULL) {
		return POLARSSL_ERR_NET_WANT_READ;
	} else { /* read the first packet in the buffer and delete it */
		DtlsRawPacket *next_packet = context->incoming_buffer->next;
		size_t dataLength = context->incoming_buffer->length;
		memcpy(buf, context->incoming_buffer->data, dataLength);
		ms_free(context->incoming_buffer->data);
		ms_free(context->incoming_buffer);
		context->incoming_buffer = next_packet;

		return dataLength;
	}
}

static int ms_dtls_srtp_DTLSread_timeout (void *ctx, unsigned char *buf, size_t len, uint32_t timeout) {
	return ms_dtls_srtp_DTLSread(ctx, buf, len); /* ms_dtls_srtp_DTLSread is non blocking */
}

void ms_dtls_srtp_set_peer_fingerprint(MSDtlsSrtpContext *context, const char *peer_fingerprint) {
	if (context) {
		memcpy(context->peer_fingerprint, peer_fingerprint, strlen(peer_fingerprint));
	}
}

void ms_dtls_srtp_set_role(MSDtlsSrtpContext *context, MSDtlsSrtpRole role) {
	if (context) {
		/* if role is isServer and was Unset, we must complete the server setup */
		if ((context->role == MSDtlsSrtpRoleUnset) && (role == MSDtlsSrtpRoleIsServer)) {
			ssl_set_endpoint(&(context->dtls_context->ssl), SSL_IS_SERVER);
			ssl_cookie_setup( &(context->dtls_context->cookie_ctx), ctr_drbg_random, &(context->dtls_context->ctr_drbg) );
		   	ssl_set_dtls_cookies( &(context->dtls_context->ssl), ssl_cookie_write, ssl_cookie_check, &(context->dtls_context->cookie_ctx) );
		    ssl_session_reset( &(context->dtls_context->ssl) );
			ssl_set_client_transport_id(&(context->dtls_context->ssl), (const unsigned char *)(&(context->session->snd.ssrc)), 4);
		}
		context->role = role;
	}
}

unsigned char *ms_dtls_srtp_generate_certificate_fingerprint(const x509_crt *certificate) {
	unsigned char *fingerprint = NULL;
	unsigned char buffer[64]; /* buffer is max length of returned hash, which is 64 in case we use sha-512 */
	size_t hash_length = 0;
	char hash_alg_string[8]; /* buffer to store the string description of the algo, longest is SHA-512(7 chars + null termination) */
	/* fingerprint is a hash of the DER formated certificate (found in crt->raw.p) using the same hash function used by certificate signature */
	switch (certificate->sig_md) {
		case POLARSSL_MD_SHA1:
			sha1(certificate->raw.p, certificate->raw.len, buffer);
			hash_length = 20;
			memcpy(hash_alg_string, "SHA-1", 6);
		break;

		default:
		break;
	}

	if (hash_length>0) {
		int i;
		int fingerprint_index = strlen(hash_alg_string);
		char prefix=' ';
		/* fingerprint will be : hash_alg_string+' '+HEX : separated values: length is strlen(hash_alg_string)+3*hash_lenght + 1 for null termination */
		fingerprint = (unsigned char *)malloc(fingerprint_index+3*hash_length+1);
		sprintf((char *)fingerprint, "%s", hash_alg_string);
		for (i=0; i<hash_length; i++, fingerprint_index+=3) {
			sprintf((char *)(fingerprint+fingerprint_index),"%c%02X", prefix,buffer[i]);
			prefix=':';
		}
		*(fingerprint+fingerprint_index) = '\0';
	}

	return fingerprint;
}


static int ms_dtls_srtp_rtp_process_on_receive(struct _RtpTransportModifier *t, mblk_t *msg){
	MSDtlsSrtpContext *ctx = (MSDtlsSrtpContext *)t->data;

	int ret;
	size_t msgLength = msgdsize(msg);

	/* check if we have an on-going handshake */
	if (ctx->channel_status == DTLS_STATUS_CONTEXT_NOT_READY) {
		return msgLength;
	}

	// check incoming message length
	if (msgLength<RTP_FIXED_HEADER_SIZE) {
		return msgLength;
	}

	/* check if it is a DTLS packet (first byte B as 19 < B < 64) rfc5764 section 5.1.2 */
	if ((*(msg->b_rptr)>19) && (*(msg->b_rptr)<64)) {
		DtlsRawPacket *incoming_dtls_packet = (DtlsRawPacket *)ms_malloc0(sizeof(DtlsRawPacket));
		incoming_dtls_packet->next=NULL;
		incoming_dtls_packet->data=(unsigned char *)ms_malloc(msgLength);
		incoming_dtls_packet->length=msgLength;
		memcpy(incoming_dtls_packet->data, msg->b_rptr, msgLength);
		//memcpy(incoming_dtls_packet->data, helloHex, msgLength);
		/* store the packet in the incoming buffer */
		if (ctx->incoming_buffer==NULL) { /* buffer is empty */
			ctx->incoming_buffer = incoming_dtls_packet;
		} else { /* queue it at the end of current buffer */
			DtlsRawPacket *last_packet = ctx->incoming_buffer;
			while (last_packet->next != NULL) last_packet = last_packet->next;
			last_packet->next = incoming_dtls_packet;
		}
		if (ctx->role == MSDtlsSrtpRoleUnset) { /* role is unset but we receive a packet: we are caller and shall initialise as server and then process the incoming packet */
			ms_dtls_srtp_set_role(ctx, MSDtlsSrtpRoleIsServer); /* this call will update role and complete server setup */
		}

		/* check if we have to process it */
		ret = ssl_handshake(&(ctx->dtls_context->ssl));
		if (ret==POLARSSL_ERR_SSL_HELLO_VERIFY_REQUIRED) {
			ssl_session_reset(&(ctx->dtls_context->ssl));
			ssl_set_client_transport_id(&(ctx->dtls_context->ssl), (const unsigned char *)(&(ctx->session->snd.ssrc)), 4); 
		}

		if (ctx->role == MSDtlsSrtpRoleIsClient) { /* if we are client, manage the retransmission timer */
			ctx->time_reference = get_timeval_in_millis();
		}

		if ((ret==0) && (ctx->channel_status == DTLS_STATUS_CONTEXT_READY)) { /* handshake is over, give the keys to srtp : 128 bits client write - 128 bits server write - 112 bits client salt - 112 server salt */
			unsigned char *computed_peer_fingerprint = NULL;
			

			computed_peer_fingerprint = ms_dtls_srtp_generate_certificate_fingerprint(ssl_get_peer_cert(&(ctx->dtls_context->ssl)));
			if (strcmp((const char *)computed_peer_fingerprint, ctx->peer_fingerprint) == 0) {
				uint8_t *key = (uint8_t *)ms_malloc0(256);
				OrtpEventData *eventData;
				OrtpEvent *ev;
				ms_message("DTLS Handshake successful and fingerprints match");
						
				ctx->channel_status = DTLS_STATUS_HANDSHAKE_OVER;
				ctx->time_reference = 0; /* unarm the timer */
			
				if (ctx->role == MSDtlsSrtpRoleIsServer) {
					/* reception(client write) key and salt +16bits padding */
					memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys, 128);
					memcpy(key + 128, ctx->dtls_context->ssl.dtls_srtp_keys+240, 112);
					media_stream_set_srtp_recv_key(ctx->stream, MS_AES_128_SHA1_80, (const char *)key, FALSE);
					/* emission(server write) key and salt +16bits padding */
					memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys+128, 128);
					memcpy(key + 128, ctx->dtls_context->ssl.dtls_srtp_keys+368, 112);
					media_stream_set_srtp_send_key(ctx->stream, MS_AES_128_SHA1_80, (const char *)key, FALSE);
				} else if (ctx->role == MSDtlsSrtpRoleIsClient){ /* this enpoint act as DTLS client */
					/* emission(client write) key and salt +16bits padding */
					memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys, 128);
					memcpy(key + 128, ctx->dtls_context->ssl.dtls_srtp_keys+240, 112);
					media_stream_set_srtp_send_key(ctx->stream, MS_AES_128_SHA1_80, (const char *)key, FALSE);
					/* reception(server write) key and salt +16bits padding */
					memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys+128, 128);
					memcpy(key + 128, ctx->dtls_context->ssl.dtls_srtp_keys+368, 112);
					media_stream_set_srtp_recv_key(ctx->stream, MS_AES_128_SHA1_80, (const char *)key, FALSE);
				}

				/* send event */
				ev=ortp_event_new(ORTP_EVENT_DTLS_ENCRYPTION_CHANGED);
				eventData=ortp_event_get_data(ev);
				eventData->info.dtls_stream_encrypted=1;
				rtp_session_dispatch_event(ctx->session, ev);
				ms_message("Event dispatched to all: secrets are on");

				ms_free(key);
			} else {
				ms_message("DTLS Handshake successful but fingerprints differ received : %s computed %s", ctx->peer_fingerprint, computed_peer_fingerprint);
			}
		}
		return 0;
	} else {
		if (ctx->role == MSDtlsSrtpRoleIsClient) { /* if we are client, manage the retransmission timer */
			if (ctx->time_reference>0) { /* only when retransmission timer is armed */
				if (get_timeval_in_millis() - ctx->time_reference > READ_TIMEOUT_MS) {
					ssl_handshake(&(ctx->dtls_context->ssl));
					ctx->time_reference = get_timeval_in_millis();
				}
			}
		}

	}
	return msgLength;
}

/* Nothing to do on rtcp packets, just return packet length */
static int ms_dtls_srtp_rtcp_process_on_receive(struct _RtpTransportModifier *t, mblk_t *msg)  {
	return msgdsize(msg);
}

static MSDtlsSrtpContext* createUserData(DtlsPolarsslContext *context, MSDtlsSrtpParams *params) {
	MSDtlsSrtpContext *userData=ms_new0(MSDtlsSrtpContext,1);
	userData->dtls_context=context;
	userData->role = params->role;
	userData->time_reference = 0;

	/* get certificate and fingerprint from params */

	return userData;
}

int ms_dtls_srtp_transport_modifier_new(MSDtlsSrtpContext* ctx, RtpTransportModifier **rtpt, RtpTransportModifier **rtcpt ) {
	if (rtpt){
		*rtpt=ms_new0(RtpTransportModifier,1);
		(*rtpt)->data=ctx; /* back link to get access to the other fields of the OrtoZrtpContext from the RtpTransportModifier structure */
		(*rtpt)->t_process_on_send=ms_dtls_srtp_rtp_process_on_send;
		(*rtpt)->t_process_on_receive=ms_dtls_srtp_rtp_process_on_receive;
		(*rtpt)->t_destroy=ms_dtls_srtp_transport_modifier_destroy;
	}
	if (rtcpt){
		*rtcpt=ms_new0(RtpTransportModifier,1);
		(*rtcpt)->data=ctx; /* back link to get access to the other fields of the OrtoZrtpContext from the RtpTransportModifier structure */
		(*rtcpt)->t_process_on_send=ms_dtls_srtp_rtcp_process_on_send;
		(*rtcpt)->t_process_on_receive=ms_dtls_srtp_rtcp_process_on_receive;
		(*rtcpt)->t_destroy=ms_dtls_srtp_transport_modifier_destroy;
	}
	return 0;
}


MSDtlsSrtpRole ms_dtls_srtp_get_role(MSDtlsSrtpContext *context) {
	if (context) {
		return context->role;
	} else {
		return MSDtlsSrtpRoleInvalid;
	}
}

static void ms_dtls_srtp_set_transport(MSDtlsSrtpContext *userData, RtpSession *s)  {
	RtpTransport *rtpt=NULL,*rtcpt=NULL;
	RtpTransportModifier *rtp_modifier, *rtcp_modifier;

	rtp_session_get_transports(s,&rtpt,&rtcpt);

	ms_dtls_srtp_transport_modifier_new(userData, &rtp_modifier,&rtcp_modifier);
	/*if transports are set, we assume they are meta transporters, otherwise create them*/
	if (rtpt==NULL&&rtcpt==NULL){
		meta_rtp_transport_new(&rtpt, TRUE, NULL, 0);
		meta_rtp_transport_new(&rtcpt, FALSE, NULL, 0);
	}

	meta_rtp_transport_append_modifier(rtpt, rtp_modifier);
	meta_rtp_transport_append_modifier(rtcpt, rtcp_modifier);

	/* save transport modifier into context, needed to inject packets generated by DTLS */
	userData->rtp_modifier = rtp_modifier;

	rtp_session_set_transports(s, rtpt, rtcpt);
}

MSDtlsSrtpContext* ms_dtls_srtp_context_new(MediaStream *stream, RtpSession *s, MSDtlsSrtpParams *params){
	MSDtlsSrtpContext *userData;
	int ret;
	enum DTLS_SRTP_protection_profiles dtls_srtp_protection_profiles[2] = {SRTP_AES128_CM_HMAC_SHA1_80, SRTP_AES128_CM_HMAC_SHA1_32};
	
	/* Create and init the DTLS context */
	DtlsPolarsslContext *dtlsContext = ortp_new0(DtlsPolarsslContext,1);
	
	ms_message("Creating DTLS-SRTP engine on session [%p] as %s", s, params->role==MSDtlsSrtpRoleIsServer?"server":(params->role==MSDtlsSrtpRoleIsClient?"client":"unset role"));

	/* create and link user data */
	userData=createUserData(dtlsContext, params);
	userData->session=s;
	userData->stream=stream;
	userData->channel_status = 0;
	userData->incoming_buffer = NULL;
	ms_dtls_srtp_set_transport(userData, s);

	memset( &(dtlsContext->ssl), 0, sizeof( ssl_context ) );
	ssl_cookie_init( &(dtlsContext->cookie_ctx) );
    x509_crt_init( &(dtlsContext->crt) );
    entropy_init( &(dtlsContext->entropy) );
    ctr_drbg_init( &(dtlsContext->ctr_drbg), entropy_func, &(dtlsContext->entropy),
                               NULL, 0 );
	
	/* initialise certificate */
    ret = x509_crt_parse( &(dtlsContext->crt), (const unsigned char *) params->pem_certificate, strlen( params->pem_certificate ) );
	if( ret < 0 )
    {
        printf( " failed\n  !  x509_crt_parse returned -0x%x\n\n", -ret );
    }
	
	ret =  pk_parse_key( &(dtlsContext->pkey), (const unsigned char *) params->pem_pkey, strlen( params->pem_pkey ), NULL, 0 );
    if( ret != 0 )
    {
        printf( " failed\n  !  pk_parse_key returned -0x%x\n\n", -ret );
    }

	/* ssl setup */

	ssl_init(&(dtlsContext->ssl));
	if( ret < 0 )
    {
        printf( " failed\n  !  ssl_init returned -0x%x\n\n", -ret );
    }

	if (params->role == MSDtlsSrtpRoleIsClient) {
		ssl_set_endpoint(&(dtlsContext->ssl), SSL_IS_CLIENT);
	} else if (params->role == MSDtlsSrtpRoleIsServer) {
		ssl_set_endpoint(&(dtlsContext->ssl), SSL_IS_SERVER);
	}
	ssl_set_transport(&(dtlsContext->ssl), SSL_TRANSPORT_DATAGRAM);
	ssl_set_dtls_srtp_protection_profiles(  &(dtlsContext->ssl), dtls_srtp_protection_profiles, 2 ); /* TODO: get param from caller to select available profiles */

	/* set CA chain */
    ssl_set_authmode( &(dtlsContext->ssl), SSL_VERIFY_OPTIONAL );
	ssl_set_rng(  &(dtlsContext->ssl), ctr_drbg_random, &(dtlsContext->ctr_drbg) );
    ssl_set_ca_chain( &(dtlsContext->ssl), &(dtlsContext->crt), NULL, NULL );	
	ssl_set_own_cert( &(dtlsContext->ssl), &(dtlsContext->crt), &(dtlsContext->pkey) );
	if (params->role == MSDtlsSrtpRoleIsServer) {
		ssl_cookie_setup( &(dtlsContext->cookie_ctx), ctr_drbg_random, &(dtlsContext->ctr_drbg) );
    	ssl_set_dtls_cookies( &(dtlsContext->ssl), ssl_cookie_write, ssl_cookie_check, &(dtlsContext->cookie_ctx) );
	    ssl_session_reset( &(dtlsContext->ssl) );
		ssl_set_client_transport_id(&(dtlsContext->ssl), (const unsigned char *)(&(s->snd.ssrc)), 4);
	}

	/* set ssl transport functions */
	ssl_set_bio_timeout( &(dtlsContext->ssl), userData,
                         ms_dtls_srtp_sendData, ms_dtls_srtp_DTLSread, ms_dtls_srtp_DTLSread_timeout,
                         READ_TIMEOUT_MS );

	userData->channel_status = DTLS_STATUS_CONTEXT_READY;

	return userData;//ms_dtls_srtp_configure_context(userData,s);
}

void ms_dtls_srtp_start(MSDtlsSrtpContext* context) {
	if (context == NULL ) {
		ms_warning("DTLS start but no context\n");
		return;
	}
	/* if we are client, start the handshake(send a clientHello) */
	if (context->role == MSDtlsSrtpRoleIsClient) {
		ssl_set_endpoint(&(context->dtls_context->ssl), SSL_IS_CLIENT);
		ssl_handshake(&(context->dtls_context->ssl));
		context->time_reference = get_timeval_in_millis(); /* arm the timer for retransmission */
	}

}

void ms_dtls_srtp_context_destroy(MSDtlsSrtpContext *ctx) {
	free(ctx);
	ms_message("DTLS-SRTP context destroyed");
}

void ms_dtls_srtp_transport_modifier_destroy(RtpTransportModifier *tp)  {
	ms_free(tp);
}

#else /* HAVE_dtls */

bool_t ms_dtls_available(){return FALSE;}

#endif /* HAVE_dtls */
