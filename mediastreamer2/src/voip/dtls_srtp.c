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

#ifdef HAVE_DTLS

#include <polarssl/ssl.h>
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/ssl_cookie.h>
#include <polarssl/ssl_cache.h>
#include <polarssl/sha1.h>
#include <polarssl/sha256.h>
#include <polarssl/sha512.h>




typedef struct _DtlsPolarsslContexts {
	x509_crt crt;
	ssl_context ssl;
	entropy_context entropy;
	ctr_drbg_context ctr_drbg;
	ssl_cookie_ctx cookie_ctx;
	pk_context pkey;
	ssl_session saved_session;
	ssl_cache_context cache;
} DtlsPolarsslContext;

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
#define DTLS_STATUS_ON_GOING_HANDSHAKE 2
#define DTLS_STATUS_HANDSHAKE_OVER 3
#define DTLS_STATUS_RTCP_HANDSHAKE_ONGOING 4
#define DTLS_STATUS_RTCP_HANDSHAKE_OVER 5

#define READ_TIMEOUT_MS 1000

struct _MSDtlsSrtpContext{
	RtpSession *session;
	MediaStream *stream;
	RtpTransportModifier *rtp_modifier;
	RtpTransportModifier *rtcp_modifier;
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
	RtpTransport *rtpt=NULL, *rtcpt=NULL;
	mblk_t *msg;

	ms_message("DTLS Send packet len %d", (int)length);

	if ( context->channel_status >=  DTLS_STATUS_RTCP_HANDSHAKE_ONGOING ) {
		ms_message("DTLS RTCP Send packet len %d", (int)length);
		/* get RTCP transport from session */
		rtp_session_get_transports(session,NULL,&rtcpt);

		/* generate message from raw data */
		msg = rtp_session_create_packet_raw((uint8_t *)data, (int)length);

		return meta_rtp_transport_modifier_inject_packet(rtcpt, context->rtcp_modifier, msg , 0);
	} else { /* we are on the RTP handshake */
		ms_message("DTLS RTP Send packet len %d", (int)length);
		/* get RTP transport from session */
		rtp_session_get_transports(session,&rtpt,NULL);

		/* generate message from raw data */
		msg = rtp_session_create_packet_raw((uint8_t *)data, (int)length);

		return meta_rtp_transport_modifier_inject_packet(rtpt, context->rtp_modifier, msg , 0);
	}
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
			ssl_cache_init( &(context->dtls_context->cache) );
			ssl_set_session_cache( &(context->dtls_context->ssl), ssl_cache_get, &(context->dtls_context->cache), ssl_cache_set, &(context->dtls_context->cache) );
		}
		context->role = role;
	}
}

/* note : this code is duplicated in belle_sip/src/transport/tls_channel_polarssl.c */
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
			memcpy(hash_alg_string, "sha-1", 6);
		break;

		case POLARSSL_MD_SHA224:
			sha256(certificate->raw.p, certificate->raw.len, buffer, 1); /* last argument is a boolean, indicate to output sha-224 and not sha-256 */
			hash_length = 28;
			memcpy(hash_alg_string, "sha-224", 8);
		break;

		case POLARSSL_MD_SHA256:
			sha256(certificate->raw.p, certificate->raw.len, buffer, 0);
			hash_length = 32;
			memcpy(hash_alg_string, "sha-256", 8);
		break;

		case POLARSSL_MD_SHA384:
			sha512(certificate->raw.p, certificate->raw.len, buffer, 1); /* last argument is a boolean, indicate to output sha-384 and not sha-512 */
			hash_length = 48;
			memcpy(hash_alg_string, "sha-384", 8);
		break;

		case POLARSSL_MD_SHA512:
			sha512(certificate->raw.p, certificate->raw.len, buffer, 1); /* last argument is a boolean, indicate to output sha-384 and not sha-512 */
			hash_length = 64;
			memcpy(hash_alg_string, "sha-512", 8);
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


/**
 * Convert a polarssl defined value for SRTP protection profile to the mediastreamer enumeration of SRTP protection profile
 * @param[in]	dtls_srtp_protection_profile	A DTLS-SRTP protection profile defined by polarssl
 * @return the matching profile defined in mediatream.h
 */
static MSCryptoSuite ms_polarssl_dtls_srtp_protection_profile_to_ms_crypto_suite(enum DTLS_SRTP_protection_profiles dtls_srtp_protection_profile) {
	switch(dtls_srtp_protection_profile) {
		case SRTP_AES128_CM_HMAC_SHA1_80:
			return MS_AES_128_SHA1_80;
		case SRTP_AES128_CM_HMAC_SHA1_32:
			return MS_AES_128_SHA1_32;
		case SRTP_NULL_HMAC_SHA1_80:
			return MS_NO_CIPHER_SHA1_80;
		case SRTP_NULL_HMAC_SHA1_32: /* this profile is defined in DTLS-SRTP rfc but not implemented by libsrtp */
			return MS_CRYPTO_SUITE_INVALID;
		default:
			return MS_CRYPTO_SUITE_INVALID;
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
 * @return TRUE if packet is a DTLS one, false otherwise
 */
static bool_t ms_dtls_srtp_process_dtls_packet(mblk_t *msg, MSDtlsSrtpContext *ctx, int *ret) {
	size_t msgLength = msgdsize(msg);
	// check if incoming message length is compatible with potential DTLS message
	if (msgLength<RTP_FIXED_HEADER_SIZE) {
		return FALSE;
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
		
		/* role is unset but we receive a packet: we are caller and shall initialise as server and then process the incoming packet */
		if (ctx->role == MSDtlsSrtpRoleUnset) {
			ms_dtls_srtp_set_role(ctx, MSDtlsSrtpRoleIsServer); /* this call will update role and complete server setup */
		}

		/* process the packet and store result */
		*ret = ssl_handshake(&(ctx->dtls_context->ssl));

		/* when we are server, we may issue a hello verify, so reset session, keep cookies(transport id) and expect an other Hello from client */
		if (*ret==POLARSSL_ERR_SSL_HELLO_VERIFY_REQUIRED) {
			ssl_session_reset(&(ctx->dtls_context->ssl));
			ssl_set_client_transport_id(&(ctx->dtls_context->ssl), (const unsigned char *)(&(ctx->session->snd.ssrc)), 4); 
		}

		/* if we are client, manage the retransmission timer */
		if (ctx->role == MSDtlsSrtpRoleIsClient) {
			ctx->time_reference = get_timeval_in_millis();
		}

		return TRUE;
	} else { /* it is not a dtls packet, but manage anyway the retransmission timer */
		if (ctx->role == MSDtlsSrtpRoleIsClient) { /* only if we are client */
			if (ctx->time_reference>0) { /* only when retransmission timer is armed */
				if (get_timeval_in_millis() - ctx->time_reference > READ_TIMEOUT_MS) {
					ssl_handshake(&(ctx->dtls_context->ssl));
					ctx->time_reference = get_timeval_in_millis();
				}
			}
		}

	}

	return FALSE;

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

	/* check if it is a DTLS packet and process it */
	if (ms_dtls_srtp_process_dtls_packet(msg, ctx, &ret) == TRUE){
		
		if ((ret==0) && (ctx->channel_status == DTLS_STATUS_CONTEXT_READY)) { /* handshake is over, give the keys to srtp : 128 bits client write - 128 bits server write - 112 bits client salt - 112 server salt */
			MSCryptoSuite agreed_srtp_protection_profile = MS_CRYPTO_SUITE_INVALID;

			ctx->channel_status = DTLS_STATUS_HANDSHAKE_OVER;

			/* check the srtp profile get selected during handshake */
			agreed_srtp_protection_profile = ms_polarssl_dtls_srtp_protection_profile_to_ms_crypto_suite(ssl_get_dtls_srtp_protection_profile(&(ctx->dtls_context->ssl)));
			if ( agreed_srtp_protection_profile == MS_CRYPTO_SUITE_INVALID) {
				ms_message("DTLS Handshake successful but unable to agree on srtp_profile to use");
			} else {
				unsigned char *computed_peer_fingerprint = NULL;

				computed_peer_fingerprint = ms_dtls_srtp_generate_certificate_fingerprint(ssl_get_peer_cert(&(ctx->dtls_context->ssl)));
				if (strncasecmp((const char *)computed_peer_fingerprint, ctx->peer_fingerprint, strlen((const char *)computed_peer_fingerprint)) == 0) {
					int i;
					uint8_t *key = (uint8_t *)ms_malloc0(256);
					ms_message("DTLS Handshake successful and fingerprints match, srtp protection profile %d", ctx->dtls_context->ssl.chosen_dtls_srtp_profile);

					ctx->time_reference = 0; /* unarm the timer */
					printf("On recupere les bits generes: %ld\n",ctx->dtls_context->ssl.dtls_srtp_keys_len);
					for (i=0; i<ctx->dtls_context->ssl.dtls_srtp_keys_len; i++) {
						printf("%02x",ctx->dtls_context->ssl.dtls_srtp_keys[i]);
					}
					printf("\n");

					if (ctx->role == MSDtlsSrtpRoleIsServer) {
						printf("On est serveur\n");
						/* reception(client write) key and salt +16bits padding */
						memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, ctx->dtls_context->ssl.dtls_srtp_keys+2*DTLS_SRTP_KEY_LEN, DTLS_SRTP_SALT_LEN);
						media_stream_set_srtp_recv_key(ctx->stream, agreed_srtp_protection_profile, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTP_STREAM);
						/* emission(server write) key and salt +16bits padding */
						memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys+DTLS_SRTP_KEY_LEN, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, ctx->dtls_context->ssl.dtls_srtp_keys+2*DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, DTLS_SRTP_SALT_LEN);
						media_stream_set_srtp_send_key(ctx->stream, agreed_srtp_protection_profile, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTP_STREAM);
					} else if (ctx->role == MSDtlsSrtpRoleIsClient){ /* this enpoint act as DTLS client */
						printf("On est client\n");
						/* emission(client write) key and salt +16bits padding */
						memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, ctx->dtls_context->ssl.dtls_srtp_keys+2*DTLS_SRTP_KEY_LEN, DTLS_SRTP_SALT_LEN);
						media_stream_set_srtp_send_key(ctx->stream, agreed_srtp_protection_profile, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTP_STREAM);
						/* reception(server write) key and salt +16bits padding */
						memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys+DTLS_SRTP_KEY_LEN, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, ctx->dtls_context->ssl.dtls_srtp_keys+2*DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, DTLS_SRTP_SALT_LEN);
						media_stream_set_srtp_recv_key(ctx->stream, agreed_srtp_protection_profile, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTP_STREAM);

						/* if we are client save the session */
						if( ( ret = ssl_get_session( &(ctx->dtls_context->ssl), &(ctx->dtls_context->saved_session) ) ) != 0 )
						{
							printf( " failed\n  ! ssl_get_session returned -0x%x\n\n", -ret );
						} else {
							printf ("DTLS session saved\n");
						}
					}

					ms_free(key);


				        ret = ssl_close_notify( &(ctx->dtls_context->ssl) );
					printf("DTLS : close ssl returns %d", ret);

					ssl_session_reset(&(ctx->dtls_context->ssl));
					ctx->channel_status = DTLS_STATUS_RTCP_HANDSHAKE_ONGOING;

					if (ctx->role == MSDtlsSrtpRoleIsClient){ /* We are client, try to resume session and start RTCP handshake */
						if( ( ret = ssl_set_session( &(ctx->dtls_context->ssl), &(ctx->dtls_context->saved_session) ) ) != 0 ) {
							printf( " failed\n  ! ssl_set_session returned %d\n\n", ret );
						}

						/* start the DTLS handshake on rtcp channel */
						ret = ssl_handshake(&(ctx->dtls_context->ssl));
						printf("Le premier handshake RTCP retourne %d\n", ret);
					} else { /* we are server, just reload the transport_id and wait for clientHello */
						ssl_set_client_transport_id(&(ctx->dtls_context->ssl), (const unsigned char *)(&(ctx->session->snd.ssrc)), 4);
					}

				} else {
					ms_error("DTLS Handshake successful but fingerprints differ received : %s computed %s", ctx->peer_fingerprint, computed_peer_fingerprint);
				}
			}
		}
		return 0;
	}
	return msgLength;
}

static int ms_dtls_srtp_rtcp_process_on_receive(struct _RtpTransportModifier *t, mblk_t *msg)  {
	MSDtlsSrtpContext *ctx = (MSDtlsSrtpContext *)t->data;

	int ret;
	size_t msgLength = msgdsize(msg);

	// check incoming message length
	if (msgLength<RTP_FIXED_HEADER_SIZE) {
		return msgLength;
	}

	/* check if it is a DTLS packet and process it */
	if (ms_dtls_srtp_process_dtls_packet(msg, ctx, &ret) == TRUE){

		if ((ret==0) && (ctx->channel_status == DTLS_STATUS_RTCP_HANDSHAKE_ONGOING)) { /* rtcp handshake is over, give the keys to srtp : 128 bits client write - 128 bits server write - 112 bits client salt - 112 server salt */
			OrtpEventData *eventData;
			OrtpEvent *ev;
			uint8_t *key = (uint8_t *)ms_malloc0(256);
			int i;

			MSCryptoSuite agreed_srtp_protection_profile = MS_CRYPTO_SUITE_INVALID;

			ctx->channel_status = DTLS_STATUS_RTCP_HANDSHAKE_OVER;

			/* check the srtp profile get selected during handshake */
			agreed_srtp_protection_profile = ms_polarssl_dtls_srtp_protection_profile_to_ms_crypto_suite(ssl_get_dtls_srtp_protection_profile(&(ctx->dtls_context->ssl)));
			if ( agreed_srtp_protection_profile == MS_CRYPTO_SUITE_INVALID) {
				ms_message("DTLS RTCP Handshake successful but unable to agree on srtp_profile to use");
			} else {
				unsigned char *computed_peer_fingerprint = NULL;

				computed_peer_fingerprint = ms_dtls_srtp_generate_certificate_fingerprint(ssl_get_peer_cert(&(ctx->dtls_context->ssl)));
				if (strncasecmp((const char *)computed_peer_fingerprint, ctx->peer_fingerprint, strlen((const char *)computed_peer_fingerprint)) == 0) {

					ms_message("DTLS RTCP Handshake successful and fingerprints match, srtp protection profile %d", ctx->dtls_context->ssl.chosen_dtls_srtp_profile);

					printf("On recupere les bits generes: %ld\n",ctx->dtls_context->ssl.dtls_srtp_keys_len);
					for (i=0; i<ctx->dtls_context->ssl.dtls_srtp_keys_len; i++) {
						printf("%02x",ctx->dtls_context->ssl.dtls_srtp_keys[i]);
					}
					printf("\n");

					ctx->time_reference = 0; /* unarm the timer */
					if (ctx->role == MSDtlsSrtpRoleIsServer) {
						printf("On est serveur\n");
						/* reception(client write) key and salt +16bits padding */
						memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, ctx->dtls_context->ssl.dtls_srtp_keys+2*DTLS_SRTP_KEY_LEN, DTLS_SRTP_SALT_LEN);
						media_stream_set_srtp_recv_key(ctx->stream, MS_AES_128_SHA1_80, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTCP_STREAM);
						/* emission(server write) key and salt +16bits padding */
						memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys+DTLS_SRTP_KEY_LEN, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, ctx->dtls_context->ssl.dtls_srtp_keys+2*DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, DTLS_SRTP_SALT_LEN);
						media_stream_set_srtp_send_key(ctx->stream, MS_AES_128_SHA1_80, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTCP_STREAM);
					} else if (ctx->role == MSDtlsSrtpRoleIsClient){ /* this enpoint act as DTLS client */
						printf("On est client\n");
						/* emission(client write) key and salt +16bits padding */
						memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, ctx->dtls_context->ssl.dtls_srtp_keys+2*DTLS_SRTP_KEY_LEN, DTLS_SRTP_SALT_LEN);
						media_stream_set_srtp_send_key(ctx->stream, MS_AES_128_SHA1_80, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTCP_STREAM);
						/* reception(server write) key and salt +16bits padding */
						memcpy(key, ctx->dtls_context->ssl.dtls_srtp_keys+DTLS_SRTP_KEY_LEN, DTLS_SRTP_KEY_LEN);
						memcpy(key + DTLS_SRTP_KEY_LEN, ctx->dtls_context->ssl.dtls_srtp_keys+2*DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, DTLS_SRTP_SALT_LEN);
						media_stream_set_srtp_recv_key(ctx->stream, MS_AES_128_SHA1_80, (const char *)key, DTLS_SRTP_KEY_LEN+DTLS_SRTP_SALT_LEN, MSSRTP_RTCP_STREAM);
					}

					ms_free(key);

					/* send event */
					ev=ortp_event_new(ORTP_EVENT_DTLS_ENCRYPTION_CHANGED);
					eventData=ortp_event_get_data(ev);
					eventData->info.dtls_stream_encrypted=1;
					rtp_session_dispatch_event(ctx->session, ev);
					ms_message("Event dispatched to all: secrets are on");

					ret = ssl_close_notify( &(ctx->dtls_context->ssl) );
					printf("DTLS : close ssl returns %d", ret);
				}
			}
		}

		return 0;
	} 
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

	meta_rtp_transport_append_modifier(rtpt, rtp_modifier);
	meta_rtp_transport_append_modifier(rtcpt, rtcp_modifier);

	/* save transport modifier into context, needed to inject packets generated by DTLS */
	userData->rtp_modifier = rtp_modifier;
	userData->rtcp_modifier = rtcp_modifier;
}

MSDtlsSrtpContext* ms_dtls_srtp_context_new(MediaStream *stream, MSDtlsSrtpParams *params){
	MSDtlsSrtpContext *userData;
	int ret;
	enum DTLS_SRTP_protection_profiles dtls_srtp_protection_profiles[2] = {SRTP_AES128_CM_HMAC_SHA1_80, SRTP_AES128_CM_HMAC_SHA1_32};
	RtpSession *s = stream->sessions.rtp_session;
	
	/* Create and init the DTLS context */
	DtlsPolarsslContext *dtlsContext = ms_new0(DtlsPolarsslContext,1);
	
	ms_message("Creating DTLS-SRTP engine on session [%p] as %s", s, params->role==MSDtlsSrtpRoleIsServer?"server":(params->role==MSDtlsSrtpRoleIsClient?"client":"unset role"));

	/* create and link user data */
	userData=createUserData(dtlsContext, params);
	userData->session=s;
	userData->stream=stream;
	userData->channel_status = 0;
	userData->incoming_buffer = NULL;
	ms_dtls_srtp_set_transport(userData, s);

	memset( &(dtlsContext->ssl), 0, sizeof( ssl_context ) );
	memset( &(dtlsContext->saved_session), 0, sizeof( ssl_session ) );
	ssl_cookie_init( &(dtlsContext->cookie_ctx) );
	x509_crt_init( &(dtlsContext->crt) );
	entropy_init( &(dtlsContext->entropy) );
	ctr_drbg_init( &(dtlsContext->ctr_drbg), entropy_func, &(dtlsContext->entropy), NULL, 0 );
	
	/* initialise certificate */
	ret = x509_crt_parse( &(dtlsContext->crt), (const unsigned char *) params->pem_certificate, strlen( params->pem_certificate ) );
	if( ret < 0 ) {
		printf( " failed\n  !  x509_crt_parse returned -0x%x\n\n", -ret );
	}
	
	ret =  pk_parse_key( &(dtlsContext->pkey), (const unsigned char *) params->pem_pkey, strlen( params->pem_pkey ), NULL, 0 );
	if( ret != 0 ) {
		printf( " failed\n  !  pk_parse_key returned -0x%x\n\n", -ret );
	}

	/* ssl setup */
	ssl_init(&(dtlsContext->ssl));
	if( ret < 0 ) {
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
	ssl_set_authmode( &(dtlsContext->ssl), SSL_VERIFY_OPTIONAL ); /* this will force server to send his certificate to client as we need it to compute the fingerprint */
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
	ssl_set_bio_timeout( &(dtlsContext->ssl), userData, ms_dtls_srtp_sendData, ms_dtls_srtp_DTLSread, ms_dtls_srtp_DTLSread_timeout, READ_TIMEOUT_MS );

	userData->channel_status = DTLS_STATUS_CONTEXT_READY;

	return userData;
}

void ms_dtls_srtp_start(MSDtlsSrtpContext* context) {
	if (context == NULL ) {
		ms_warning("DTLS start but no context\n");
		return;
	}
	ms_message("DTLS start stream\n");
	/* if we are client, start the handshake(send a clientHello) */
	if (context->role == MSDtlsSrtpRoleIsClient) {
		ssl_set_endpoint(&(context->dtls_context->ssl), SSL_IS_CLIENT);
		ssl_handshake(&(context->dtls_context->ssl));
		context->time_reference = get_timeval_in_millis(); /* arm the timer for retransmission */
		printf("DTLS timer is %llu\n", (long long unsigned int)context->time_reference);
	}

}

void ms_dtls_srtp_context_destroy(MSDtlsSrtpContext *ctx) {
	free(ctx);
	ms_message("DTLS-SRTP context destroyed");
}

void ms_dtls_srtp_transport_modifier_destroy(RtpTransportModifier *tp)  {
	ms_free(tp);
}

#else /* HAVE_DTLS */

bool_t ms_dtls_available(){return FALSE;}

#endif /* HAVE_DTLS */
