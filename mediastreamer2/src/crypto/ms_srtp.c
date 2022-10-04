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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif
#include "ortp/ortp.h"

#include "mediastreamer2/ms_srtp.h"
#include "mediastreamer2/mediastream.h"

#ifdef HAVE_SRTP

/*srtp defines all this stuff*/
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "srtp_prefix.h"

#include "ortp/b64.h"

#define NULL_SSRC 0

typedef struct _MSSrtpStreamContext {
	srtp_t srtp;
	/* store modifiers in the context just to be able to not append then again if the context is modified */
	RtpTransportModifier *modifier_rtp;
	RtpTransportModifier *modifier_rtcp;
	ms_mutex_t mutex;
	bool_t secured;
	bool_t mandatory_enabled;
	/* for stats purpose */
	MSSrtpKeySource source; /**< who provided the key (SDES, ZRTP, DTLS-SRTP) */
	MSCryptoSuite suite; /**< what crypto suite was set. Is set to MS_CRYPTO_SUITE_INVALID if setting fails */
	/* For double encryption */
	srtp_t inner_srtp;
} MSSrtpStreamContext;

struct _MSSrtpCtx {
	MSSrtpStreamContext send_rtp_context;
	MSSrtpStreamContext recv_rtp_context;
};

MSSrtpCtx* ms_srtp_context_new(void) {
	MSSrtpCtx* ctx = ms_new0(struct _MSSrtpCtx,1);

	ms_mutex_init(&ctx->send_rtp_context.mutex, NULL);
	ctx->send_rtp_context.source = MSSrtpKeySourceUnavailable;
	ctx->send_rtp_context.suite = MS_CRYPTO_SUITE_INVALID;

	ms_mutex_init(&ctx->recv_rtp_context.mutex, NULL);
	ctx->recv_rtp_context.source = MSSrtpKeySourceUnavailable;
	ctx->recv_rtp_context.suite = MS_CRYPTO_SUITE_INVALID;

	return ctx;
}
void ms_srtp_context_delete(MSSrtpCtx* session) {
	ms_mutex_destroy(&session->send_rtp_context.mutex);
	ms_mutex_destroy(&session->recv_rtp_context.mutex);
	if (session->send_rtp_context.srtp)
		srtp_dealloc(session->send_rtp_context.srtp);
	if (session->recv_rtp_context.srtp)
		srtp_dealloc(session->recv_rtp_context.srtp);
	if (session->send_rtp_context.inner_srtp)
		srtp_dealloc(session->send_rtp_context.inner_srtp);
	if (session->recv_rtp_context.inner_srtp)
		srtp_dealloc(session->recv_rtp_context.inner_srtp);

	ms_free(session);
}

/***********************************************/
/***** LOCAL FUNCTIONS                     *****/
/***********************************************/
static void check_and_create_srtp_context(MSMediaStreamSessions *sessions) {
	if (!sessions->srtp_context)
		sessions->srtp_context=ms_srtp_context_new();
}
/**** Sender functions ****/
static int _process_on_send(RtpSession* session,MSSrtpStreamContext *ctx, mblk_t *m, bool_t is_rtp) {
	int slen;
	err_status_t err;
	rtp_header_t *rtp_header=is_rtp?(rtp_header_t*)m->b_rptr:NULL;
	rtcp_common_header_t *rtcp_header=!is_rtp?(rtcp_common_header_t*)m->b_rptr:NULL;
	slen=(int)msgdsize(m);

	if (rtp_header && (slen>RTP_FIXED_HEADER_SIZE && rtp_header->version==2)) {
		ms_mutex_lock(&ctx->mutex);
		if (ctx->suite == MS_CRYPTO_SUITE_INVALID) { // No srtp is set up
			if (ctx->mandatory_enabled) {
				ms_mutex_unlock(&ctx->mutex);
				return 0; /* drop the packet */
			} else {
				ms_mutex_unlock(&ctx->mutex);
				return slen; /* pass it uncrypted */
			}
		}

		/* We do have an inner srtp context, double encryption is on. Note: double encryption applies to RTP stream only */
		if (ctx->inner_srtp) {
			/* RFC 8723:
			 * 1 - get the header without extension + payload and encrypt that with the inner encryption
			 * 2 - put back the orginal header with extensions
			 * 3 - append the OHB byte to 0x00 after the auth tag
			 * 4 - pass it to outer encryption */
			/* defragment incoming message and enlarge the buffer for srtp to write its data */
			msgpullup(m,slen+2*SRTP_MAX_TRAILER_LEN+5 /*for 32 bits alignment + 1 byte for OHB set to 0x00 */);

			if (rtp_get_extbit(m) != 0) { /* There is an extension header */
				uint8_t *payload = NULL;
				uint16_t cc = rtp_get_cc(m);
				int payload_size = rtp_get_payload(m,&payload);
				/* create a synthetic packet, it holds:
				 * RTP header + size of CSRC if any - No extensions
				 * payload
				 * reserve space for SRTP trailer */
				size_t synthetic_header_size = RTP_FIXED_HEADER_SIZE+4*cc;
				size_t synthetic_size = synthetic_header_size + payload_size + SRTP_MAX_TRAILER_LEN + 4;
				synthetic_size += 4 - synthetic_size%4; /* make sure the memory is 32 bits aligned for srtp */
				uint8_t *synthetic = (uint8_t *)ms_malloc0(synthetic_size);
				memcpy(synthetic, m->b_rptr, synthetic_header_size); /* copy header */
				((rtp_header_t*)(synthetic))->extbit = 0; /* force the ext bit to 0 */
				memcpy(synthetic+synthetic_header_size, payload, payload_size); /* append payload */

				/* encrypt the synthetic packet */
				int synthetic_len = synthetic_header_size+payload_size;
				err=srtp_protect(ctx->inner_srtp,synthetic,&synthetic_len);
				if (err!=err_status_ok){
					ms_warning("srtp_protect inner encryption failed (%d) for stream ctx [%p]", err,ctx);
					ms_free(synthetic);
					ms_mutex_unlock(&ctx->mutex);
					return -1;
				}

				/* put it back in the original one */
				memcpy(payload, synthetic + synthetic_header_size, synthetic_len-synthetic_header_size);
				ms_free(synthetic);
				slen += (synthetic_len - synthetic_header_size - payload_size); /* slen is header + payload -> set it with new payload size: (synthetic_len - synthetic_header_size) instead of the original payload_size  */


			} else { /* no extension header, we can directly proceed to inner encryption */
				/* defragment incoming message and enlarge the buffer for srtp to write its data */
				msgpullup(m,slen+2*SRTP_MAX_TRAILER_LEN+5 /*for 32 bits alignment + 1 byte for OHB set to 0x00 */);
				err=srtp_protect(ctx->inner_srtp,m->b_rptr,&slen);
				if (err!=err_status_ok){
					ms_warning("srtp_protect inner encryption failed (%d) for stream ctx [%p]", err,ctx);
					ms_mutex_unlock(&ctx->mutex);
					return -1;
				}
			}
			m->b_rptr[slen] = 0x00; /* the emtpy OHB */
			slen++;
		} else {
			/* defragment incoming message and enlarge the buffer for srtp to write its data */
			msgpullup(m,slen+SRTP_MAX_TRAILER_LEN+4 /*for 32 bits alignment*/);
		}

		err=srtp_protect(ctx->srtp,m->b_rptr,&slen);
		ms_mutex_unlock(&ctx->mutex);
	} else if (rtcp_header && (slen>RTP_FIXED_HEADER_SIZE && rtcp_header->version==2)) {
		ms_mutex_lock(&ctx->mutex);
		if (ctx->suite == MS_CRYPTO_SUITE_INVALID) { // No srtp is set up
			err=err_status_ok;
			if (ctx->mandatory_enabled) {
				slen = 0; /*droping packets*/
			}
		} else {
			/* defragment incoming message and enlarge the buffer for srtp to write its data */
			msgpullup(m,slen+SRTP_MAX_TRAILER_LEN+4 /*for 32 bits alignment*/ + 4 /*required by srtp_protect_rtcp*/);
			err=srtp_protect_rtcp(ctx->srtp,m->b_rptr,&slen);
		}
		ms_mutex_unlock(&ctx->mutex);
	} else {
		/*ignoring non rtp/rtcp packets*/
 		return slen;
 	}

	/* check return code from srtp_protect */
	if (err==err_status_ok){
		return slen;
	}
	ms_warning("srtp_protect%s() failed (%d) for stream ctx [%p]", is_rtp?"":"_rtcp", err,ctx);
	return -1;
}

static int ms_srtp_process_on_send(RtpTransportModifier *t, mblk_t *m){
	return _process_on_send(t->session,(MSSrtpStreamContext*)t->data, m, TRUE);
}

static int ms_srtcp_process_on_send(RtpTransportModifier *t, mblk_t *m){
	return _process_on_send(t->session,(MSSrtpStreamContext*)t->data, m, FALSE);
}

static int ms_srtp_process_dummy(RtpTransportModifier *t, mblk_t *m) {
	return (int)msgdsize(m);
}
static int _process_on_receive(RtpSession* session,MSSrtpStreamContext *ctx, mblk_t *m, bool_t is_rtp){
	int slen=(int)msgdsize(m);
	err_status_t srtp_err=err_status_ok;

	/* Check incoming message seems to be a valid RTP or RTCP */
	if (is_rtp){
		rtp_header_t *rtp=(rtp_header_t*)m->b_rptr;
		if (slen<RTP_FIXED_HEADER_SIZE || rtp->version!=2 ) {
			return slen;
		}
	}else{
		rtcp_common_header_t *rtcp=(rtcp_common_header_t*)m->b_rptr;
		if (slen<(int)(sizeof(rtcp_common_header_t)+4) || rtcp->version!=2 ) {
			return slen;
		}
	}

	if (ctx->suite == MS_CRYPTO_SUITE_INVALID) {
		if (ctx->mandatory_enabled) {
			return 0; /* drop message: we cannot decrypt but encryption is mandatory */
		} else {
			return slen; /* just pass it */
		}
	}

	if (is_rtp) {
		if ((srtp_err = srtp_unprotect(ctx->srtp,m->b_rptr,&slen)) != err_status_ok) {
			ms_warning("srtp_unprotect_rtp() failed (%d) on stream ctx [%p]", srtp_err,ctx);
			return -1;
		}
		/* Do we have double encryption */
		if (ctx->inner_srtp != NULL) {
			/* RFC8723: now that we applied outer crypto algo, we must
			 * 1 - get the OHB: if it is not 0 replace the headers
			 * 2 - if we have extensions remove them
			 * 3 - decrypt and put the extensions back */
			if (m->b_rptr[slen-1] != 0) { /* there is some OHB bits sets, restore it */
				// TODO: parse the OHB
			}
			slen--; /* drop the OHB Config byte */
			if (rtp_get_extbit(m) != 0) { /* There is an extension header */
				uint8_t *payload = NULL;
				uint16_t cc = rtp_get_cc(m);
				int extsize=rtp_get_extheader(m,NULL,NULL) + 4; // get_extheader returns the size of the ext header itself, add 4 bytes for the size itself as we need the actual size occupied by ext header
				int payload_size = rtp_get_payload(m,&payload); /* The payload size returned is incorrect as it includes the outer encryption auth tag and OHB */
				payload_size = slen -  (RTP_FIXED_HEADER_SIZE+4*cc+extsize); /* slen is the size of header(with ext) + payload */
				/* create a synthetic packet, it holds:
				 * RTP header + size of CSRC if any - No extensions
				 * payload (includes inner SRTP auth tag) */
				size_t synthetic_header_size = RTP_FIXED_HEADER_SIZE+4*cc;
				int synthetic_len = synthetic_header_size + payload_size;
				uint8_t *synthetic = (uint8_t *)ms_malloc0(synthetic_len);
				memcpy(synthetic, m->b_rptr, synthetic_header_size); /* copy header */
				((rtp_header_t*)(synthetic))->extbit = 0; /* force the ext bit to 0 */
				memcpy(synthetic+synthetic_header_size, payload, payload_size); /* append payload */

				/* decrypt the synthetic packet */
				srtp_err=srtp_unprotect(ctx->inner_srtp,synthetic,&synthetic_len);
				if (srtp_err!=err_status_ok){
					ms_warning("srtp_unprotect inner encryption failed (%d) for stream ctx [%p]", srtp_err,ctx);
					ms_free(synthetic);
					ms_mutex_unlock(&ctx->mutex);
					return -1;
				}

				/* put it back in the original packet */
				memcpy(payload, synthetic + synthetic_header_size, synthetic_len-synthetic_header_size);
				ms_free(synthetic);
				slen = RTP_FIXED_HEADER_SIZE+4*cc+extsize /* original header size */
					+ synthetic_len-synthetic_header_size; /* current payload size (after decrypt) */
			} else {
				/* no extension header, decrypt directly */
				srtp_err = srtp_unprotect(ctx->inner_srtp,m->b_rptr,&slen);
			}
		}
	} else { /* rtcp */
		srtp_err = srtp_unprotect_rtcp(ctx->srtp,m->b_rptr,&slen);
	}

	if (srtp_err==err_status_ok) {
		return slen;
	} else {
		ms_warning("srtp_unprotect%s() failed (%d) on stream ctx [%p]", is_rtp?"":"_rtcp", srtp_err,ctx);
		return -1;
	}
	return 0;
}

static int ms_srtp_process_on_receive(RtpTransportModifier *t, mblk_t *m){
	return _process_on_receive(t->session,(MSSrtpStreamContext*)t->data, m, TRUE);
}

static int ms_srtcp_process_on_receive(RtpTransportModifier *t, mblk_t *m){
	return _process_on_receive(t->session,(MSSrtpStreamContext*)t->data, m, FALSE);
}

/**** Session management functions ****/

/**
 * deallocate transport modifier ressources
 * @param[in/out] tp	The transport modifier to be deallocated
 */
static void ms_srtp_transport_modifier_destroy(RtpTransportModifier *tp){
	ms_free(tp);
}

static MSSrtpStreamContext* get_stream_context(MSMediaStreamSessions *sessions, bool_t is_send) {
	return is_send?&sessions->srtp_context->send_rtp_context:&sessions->srtp_context->recv_rtp_context;
}


static int ms_media_stream_session_fill_srtp_context(MSMediaStreamSessions *sessions, bool_t is_send, bool_t is_inner) {
	err_status_t err=0;
	RtpTransport *transport_rtp=NULL,*transport_rtcp=NULL;
	MSSrtpStreamContext* stream_ctx = get_stream_context(sessions,is_send);

	rtp_session_get_transports(sessions->rtp_session,&transport_rtp,&transport_rtcp);

	ms_mutex_lock(&stream_ctx->mutex);

	if (is_inner) { /* inner srtp context just need to setup itself, not the modifier*/
		if (stream_ctx->inner_srtp) {
			/*we cannot reuse srtp context, so freeing first*/
			srtp_dealloc(stream_ctx->inner_srtp);
			stream_ctx->inner_srtp=NULL;
		}

		err = srtp_create(&stream_ctx->inner_srtp, NULL);
		if (err != 0) {
			ms_error("Failed to create inner srtp session (%d) for stream sessions [%p]", err,sessions);
			goto end;
		}
	} else { /* this is outer srtp context, setup srtp and modifier */
		if (stream_ctx->srtp && stream_ctx->secured) {
			/*we cannot reuse srtp context, so freeing first*/
			srtp_dealloc(stream_ctx->srtp);
			stream_ctx->srtp=NULL;
		}

		if (!stream_ctx->srtp) {
			err = srtp_create(&stream_ctx->srtp, NULL);
			if (err != 0) {
				ms_error("Failed to create srtp session (%d) for stream sessions [%p]", err,sessions);
				goto end;
			}
		}

		if (!stream_ctx->modifier_rtp) {
			stream_ctx->modifier_rtp=ms_new0(RtpTransportModifier,1);
			stream_ctx->modifier_rtp->data=stream_ctx;
			stream_ctx->modifier_rtp->t_process_on_send=is_send?ms_srtp_process_on_send:ms_srtp_process_dummy;
			stream_ctx->modifier_rtp->t_process_on_receive=is_send?ms_srtp_process_dummy:ms_srtp_process_on_receive;
			stream_ctx->modifier_rtp->t_destroy=ms_srtp_transport_modifier_destroy;
			meta_rtp_transport_append_modifier(transport_rtp, stream_ctx->modifier_rtp);
		}

		if (!stream_ctx->modifier_rtcp) {
			stream_ctx->modifier_rtcp=ms_new0(RtpTransportModifier,1);
			stream_ctx->modifier_rtcp->data=stream_ctx;
			stream_ctx->modifier_rtcp->t_process_on_send=is_send?ms_srtcp_process_on_send:ms_srtp_process_dummy;
			stream_ctx->modifier_rtcp->t_process_on_receive=is_send?ms_srtp_process_dummy:ms_srtcp_process_on_receive;
			stream_ctx->modifier_rtcp->t_destroy=ms_srtp_transport_modifier_destroy;
			meta_rtp_transport_append_modifier(transport_rtcp, stream_ctx->modifier_rtcp);
		}
	}
end:
	ms_mutex_unlock(&stream_ctx->mutex);
	return err;

}

static int ms_media_stream_sessions_fill_srtp_context_all_stream(struct _MSMediaStreamSessions *sessions) {
	int  err = -1;
	/*check if exist before filling*/

	if (!(get_stream_context(sessions, TRUE)->srtp) && (err = ms_media_stream_session_fill_srtp_context(sessions, TRUE, FALSE)))
		 return err;

	if (!get_stream_context(sessions, FALSE)->srtp)
		err = ms_media_stream_session_fill_srtp_context(sessions,FALSE, FALSE);

	return err;
}


static int ms_set_srtp_crypto_policy(MSCryptoSuite suite, crypto_policy_t *policy, bool_t is_rtp) {
	switch(suite){
		case MS_AES_128_SHA1_32:
			// srtp doc says: not adapted to rtcp...
			crypto_policy_set_aes_cm_128_hmac_sha1_32(policy);
			break;
		case MS_AES_128_SHA1_80_NO_AUTH:
		case MS_AES_128_SHA1_32_NO_AUTH:
			// srtp doc says: not adapted to rtcp...
			crypto_policy_set_aes_cm_128_null_auth(policy);
			break;
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
			// SRTP is not encrypted
			// SRTCP is encrypted
			(is_rtp) ? crypto_policy_set_null_cipher_hmac_sha1_80(policy) : crypto_policy_set_aes_cm_128_hmac_sha1_80(policy);
			break;
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
			// SRTP is encrypted
			// SRTCP is not encrypted
			(is_rtp) ? crypto_policy_set_aes_cm_128_hmac_sha1_80(policy) : crypto_policy_set_null_cipher_hmac_sha1_80(policy);
			break;
		case MS_AES_128_SHA1_80_NO_CIPHER:
			// SRTP is not encrypted
			// SRTCP is not encrypted
			crypto_policy_set_null_cipher_hmac_sha1_80(policy);
			break;
		case MS_AES_128_SHA1_80: /*default mode*/
			crypto_policy_set_aes_cm_128_hmac_sha1_80(policy);
			break;
		case MS_AES_256_SHA1_80: // For backward compatibility
		case MS_AES_CM_256_SHA1_80:
			crypto_policy_set_aes_cm_256_hmac_sha1_80(policy);
			break;
		case MS_AES_256_SHA1_32:
			crypto_policy_set_aes_cm_256_hmac_sha1_32(policy);
			break;
		case MS_AEAD_AES_128_GCM:
			srtp_crypto_policy_set_aes_gcm_128_16_auth(policy);
			break;
		case MS_AEAD_AES_256_GCM:
			srtp_crypto_policy_set_aes_gcm_256_16_auth(policy);
			break;
		case MS_CRYPTO_SUITE_INVALID:
		default:
			return -1;
			break;
	}
	return 0;
}

static bool_t ms_srtp_is_crypto_policy_secure(MSCryptoSuite suite) {
	switch(suite){
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_80_NO_AUTH:
		case MS_AES_128_SHA1_32_NO_AUTH:
		case MS_AES_128_SHA1_80:
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
		case MS_AES_256_SHA1_32:
		case MS_AEAD_AES_128_GCM:
		case MS_AEAD_AES_256_GCM:
			return TRUE;
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
		case MS_AES_128_SHA1_80_NO_CIPHER:
		case MS_CRYPTO_SUITE_INVALID:
		default:
			return FALSE;
	}
}

const char * ms_crypto_suite_to_string(MSCryptoSuite suite) {
	switch(suite) {
		case MS_CRYPTO_SUITE_INVALID:
			return "<invalid-or-unsupported-suite>";
			break;
		case MS_AES_128_SHA1_80:
			return "AES_CM_128_HMAC_SHA1_80";
			break;
		case MS_AES_128_SHA1_32:
			return "AES_CM_128_HMAC_SHA1_32";
			break;
		case MS_AES_128_SHA1_80_NO_AUTH:
			return "AES_CM_128_HMAC_SHA1_80 UNAUTHENTICATED_SRTP";
			break;
		case MS_AES_128_SHA1_32_NO_AUTH:
			return "AES_CM_128_HMAC_SHA1_32 UNAUTHENTICATED_SRTP";
			break;
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
			return "AES_CM_128_HMAC_SHA1_80 UNENCRYPTED_SRTP";
			break;
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
			return "AES_CM_128_HMAC_SHA1_80 UNENCRYPTED_SRTCP";
			break;
		case MS_AES_128_SHA1_80_NO_CIPHER:
			return "AES_CM_128_HMAC_SHA1_80 UNENCRYPTED_SRTP UNENCRYPTED_SRTCP";
			break;
		case MS_AES_256_SHA1_80:
			return "AES_256_CM_HMAC_SHA1_80";
			break;
		case MS_AES_CM_256_SHA1_80:
			return "AES_CM_256_HMAC_SHA1_80";
			break;
		case MS_AES_256_SHA1_32:
			return "AES_256_CM_HMAC_SHA1_32";
			break;
		case MS_AEAD_AES_128_GCM:
			return "AEAD_AES_128_GCM";
			break;
		case MS_AEAD_AES_256_GCM:
			return "AEAD_AES_256_GCM";
			break;
	}
	return "<invalid-or-unsupported-suite>";
}

static int ms_srtp_add_or_update_stream(srtp_t session, const srtp_policy_t *policy)
{

	err_status_t status = srtp_update_stream(session, policy);
	if (status != 0) {
		status = srtp_add_stream(session, policy);
	}

	return status;
}

static int ms_add_srtp_stream(MSSrtpStreamContext *stream_ctx, MSCryptoSuite suite, const char *key, size_t key_length, bool_t is_send, bool_t is_inner, uint32_t ssrc) {
	srtp_policy_t policy;
	err_status_t err;
	ssrc_t ssrc_conf;
	srtp_t srtp = is_inner?stream_ctx->inner_srtp:stream_ctx->srtp;

	memset(&policy,0,sizeof(policy));

	/* Init both RTP and RTCP policies, even if this srtp_t is used for one of them.
	 * Indeed the key derivation algorithm that computes the SRTCP auth key depends on parameters 
	 * of the SRTP stream.*/
	if (ms_set_srtp_crypto_policy(suite, &policy.rtp, TRUE) != 0) {
		return -1;
	}
	
	// and RTCP stream
	if (ms_set_srtp_crypto_policy(suite, &policy.rtcp, FALSE) != 0) {
		return -1;
	}

	// Check key size,
	switch (suite) {
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
			if ((int)key_length != policy.rtcp.cipher_key_len) {
				ms_error("Key size (%i) doesn't match the selected srtcp profile (required %d) - srtp profile unencrypted", (int)key_length,
						policy.rtcp.cipher_key_len);
				return -1;
			}
			break;
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
			if ((int)key_length != policy.rtp.cipher_key_len) {
				ms_error("Key size (%i) doesn't match the selected srtp profile (required %d) - srtcp profile unencrypted", (int)key_length,
						policy.rtp.cipher_key_len);
				return -1;
			}
			break;
		default: // both rtp and rtcp policies should match the given one
			if (((int)key_length != policy.rtp.cipher_key_len) || ((int)key_length != policy.rtcp.cipher_key_len)) {
				ms_error("Key size (%i) doesn't match the selected srtp profile (required %d) or srtcp profile (required %d)", (int)key_length,
						policy.rtp.cipher_key_len, policy.rtcp.cipher_key_len);
				return -1;
			}
			break;
	}

	if (is_send)
		policy.allow_repeat_tx=1; /*necessary for telephone-events*/

	/* When RTP bundle mode is used, the srtp_t is used to encrypt or decrypt several RTP streams (SSRC) at the same time.
	 * This is why we use the "template" mode of libsrtp, using ssrc_any_inbound and ssrc_any_outbound
	 * For inner encryption we must specify a SSRC when in reception as it is how we will be able to sort streams - they will not use the same inner key
	 * in sending mode, we have one key anyway */
	ssrc_conf.type=is_send ? ssrc_any_outbound : (is_inner?ssrc_specific:ssrc_any_inbound);
	ssrc_conf.value=ssrc;

	policy.ssrc = ssrc_conf;
	policy.key = (uint8_t *)key;
	policy.next = NULL;

	err = ms_srtp_add_or_update_stream(srtp, &policy);
	if (err != err_status_ok) {
		ms_error("Failed to add stream to srtp session (%d)", err);
		return -1;
	}

	return 0;
}

/***********************************************/
/***** EXPORTED FUNCTIONS                  *****/
/***********************************************/
/**** Private to mediastreamer2 functions ****/
/* header declared in voip/private.h */
static int srtp_init_done=0;

int ms_srtp_init(void)
{

	err_status_t st=0;
	ms_message("srtp init");
	if (!srtp_init_done) {
		st=srtp_init();
		if (st==0) {
			srtp_init_done++;
		}else{
			ms_fatal("Couldn't initialize SRTP library: %d.", st);
		}
	}else srtp_init_done++;
	return (int)st;
}

void ms_srtp_shutdown(void){
	srtp_init_done--;
	if (srtp_init_done==0){
		srtp_shutdown();
	}
}

static int ms_media_stream_sessions_set_srtp_key(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, bool_t is_send, bool_t is_inner, MSSrtpKeySource source, uint32_t ssrc){
	int error = -1;
	int ret = 0;
	check_and_create_srtp_context(sessions);

	if (key) {
		ms_message("media_stream_set_srtp_%s%s_key(): key %02x..%02x stream sessions is [%p]", (is_inner?"inner_":""), (is_send?"send":"recv"), (uint8_t)key[0], (uint8_t)key[key_length-1], sessions);
	} else {
		ms_message("media_stream_set_srtp_%s%s_key(): key none stream sessions is [%p]", (is_inner?"inner_":""), (is_send?"send":"recv"), sessions);
	}

	MSSrtpStreamContext* stream_ctx = get_stream_context(sessions, is_send);

	/* When the key is NULL or suite set to INVALID, juste deactivate SRTP */
	if (key == NULL || suite == MS_CRYPTO_SUITE_INVALID) {
		if (is_inner) {
		} else {
			if (stream_ctx->srtp) {
				srtp_dealloc(stream_ctx->srtp);
				stream_ctx->srtp=NULL;
			}
			stream_ctx->secured = FALSE;
			stream_ctx->source = MSSrtpKeySourceUnavailable;
			stream_ctx->suite = MS_CRYPTO_SUITE_INVALID;
		}
	} else if ((error = ms_media_stream_session_fill_srtp_context(sessions,is_send, is_inner))) {
		stream_ctx->secured=FALSE;
		stream_ctx->source = MSSrtpKeySourceUnavailable;
		stream_ctx->suite=MS_CRYPTO_SUITE_INVALID;
		ret = error;
	} else	if ((error = ms_add_srtp_stream(stream_ctx, suite, key, key_length, is_send, is_inner, ssrc))) {
		stream_ctx->secured=FALSE;
		stream_ctx->source = MSSrtpKeySourceUnavailable;
		stream_ctx->suite=MS_CRYPTO_SUITE_INVALID;
		ret = error;
	} else {
		if (!is_inner) { // not stats on inner encryption
			stream_ctx->secured=ms_srtp_is_crypto_policy_secure(suite);
			stream_ctx->source = source;
			stream_ctx->suite = suite;
		}
	}

	if (!is_inner) {
		/* Srtp encryption has changed, notify to get it in call stats */
		OrtpEvent *ev = ortp_event_new(ORTP_EVENT_SRTP_ENCRYPTION_CHANGED);
		OrtpEventData *eventData=ortp_event_get_data(ev);
		eventData->info.srtp_info.is_send = is_send;
		eventData->info.srtp_info.source = source;
		eventData->info.srtp_info.suite = suite;
		rtp_session_dispatch_event(sessions->rtp_session, ev);
	}

	return ret;
}


/**** Public Functions ****/
/* header declared in include/mediastreamer2/ms_srtp.h */
bool_t ms_srtp_supported(void){
	return TRUE;
}


bool_t ms_media_stream_sessions_secured(const MSMediaStreamSessions *sessions,MediaStreamDir dir) {
	if(!sessions->srtp_context)
		return FALSE;

	switch (dir) {
	case MediaStreamSendRecv: return (sessions->srtp_context->send_rtp_context.secured && sessions->srtp_context->recv_rtp_context.secured);
	case MediaStreamSendOnly: return sessions->srtp_context->send_rtp_context.secured; 
	case MediaStreamRecvOnly: return sessions->srtp_context->recv_rtp_context.secured;
	}

	return FALSE;
}

MSSrtpKeySource ms_media_stream_sessions_get_srtp_key_source(const MSMediaStreamSessions *sessions, MediaStreamDir dir) {
	switch (dir) {
		case MediaStreamSendRecv:
			// Check sender and receiver keys have the same source
			if (sessions->srtp_context->send_rtp_context.source == sessions->srtp_context->recv_rtp_context.source) {
				return sessions->srtp_context->send_rtp_context.source;
			} else {
				return MSSrtpKeySourceUnavailable;
			}
			break;
		case MediaStreamSendOnly:
			return sessions->srtp_context->send_rtp_context.source;
		case MediaStreamRecvOnly:
			return sessions->srtp_context->recv_rtp_context.source;
	}
	return MSSrtpKeySourceUnavailable;
}

MSCryptoSuite ms_media_stream_sessions_get_srtp_crypto_suite(const MSMediaStreamSessions *sessions, MediaStreamDir dir) {
	switch (dir) {
		case MediaStreamSendRecv:
			// Check sender and receiver keys have the suite
			if (sessions->srtp_context->send_rtp_context.suite == sessions->srtp_context->recv_rtp_context.suite) {
				return sessions->srtp_context->send_rtp_context.suite;
			} else {
				return MS_CRYPTO_SUITE_INVALID;
			}
			break;
		case MediaStreamSendOnly:
			return sessions->srtp_context->send_rtp_context.suite;
		case MediaStreamRecvOnly:
			return sessions->srtp_context->recv_rtp_context.suite;
	}
	return MS_CRYPTO_SUITE_INVALID;
}

static int ms_media_stream_sessions_set_srtp_key_b64_base(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key, MSSrtpKeySource source, bool_t is_send, bool_t is_inner, uint32_t ssrc){
	int retval;

	size_t key_length = 0;
	char *key = NULL;

	if (b64_key != NULL) {
		/* decode b64 key */
		size_t b64_key_length = strlen(b64_key);
		size_t max_key_length = b64_decode(b64_key, b64_key_length, 0, 0);
		key = (char *) ms_malloc0(max_key_length+1);
		if ((key_length = b64_decode(b64_key, b64_key_length, key, max_key_length)) == 0) {
			ms_error("Error decoding b64 srtp recv key");
			ms_free(key);
			return -1;
		}
	}

	/* pass decoded key to set_recv_key function */
	retval = ms_media_stream_sessions_set_srtp_key(sessions, suite, key, key_length, is_send, is_inner, source, ssrc);

	ms_free(key);

	return retval;
}

int ms_media_stream_sessions_set_srtp_recv_key_b64(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key, MSSrtpKeySource source){
	return ms_media_stream_sessions_set_srtp_key_b64_base(sessions, suite, b64_key, source, FALSE, FALSE, NULL_SSRC);
}
int ms_media_stream_sessions_set_srtp_inner_recv_key_b64(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key, MSSrtpKeySource source, uint32_t ssrc){
	return ms_media_stream_sessions_set_srtp_key_b64_base(sessions, suite, b64_key, source, FALSE, TRUE, ssrc);
}

int ms_media_stream_sessions_set_srtp_recv_key(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, MSSrtpKeySource source) {
	return ms_media_stream_sessions_set_srtp_key(sessions, suite, key, key_length, FALSE, FALSE, source, NULL_SSRC);
}

int ms_media_stream_sessions_set_srtp_inner_recv_key(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, MSSrtpKeySource source, uint32_t ssrc) {
	return ms_media_stream_sessions_set_srtp_key(sessions, suite, key, key_length, FALSE, TRUE, source, ssrc);
}



int ms_media_stream_sessions_set_srtp_send_key_b64(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key, MSSrtpKeySource source) {
	return ms_media_stream_sessions_set_srtp_key_b64_base(sessions, suite, b64_key, source, TRUE, FALSE, NULL_SSRC);
}

int ms_media_stream_sessions_set_srtp_inner_send_key_b64(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key, MSSrtpKeySource source) {
	return ms_media_stream_sessions_set_srtp_key_b64_base(sessions, suite, b64_key, source, TRUE, TRUE, NULL_SSRC);
}

int ms_media_stream_sessions_set_srtp_send_key(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, MSSrtpKeySource source){
	return ms_media_stream_sessions_set_srtp_key(sessions, suite, key, key_length, TRUE, FALSE, source, NULL_SSRC);
}

int ms_media_stream_sessions_set_srtp_inner_send_key(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, MSSrtpKeySource source){
	return ms_media_stream_sessions_set_srtp_key(sessions, suite, key, key_length, TRUE, TRUE, source, NULL_SSRC);
}

int ms_media_stream_sessions_set_encryption_mandatory(MSMediaStreamSessions *sessions, bool_t yesno) {
	/*for now, managing all streams in one time*/
	int err;
	check_and_create_srtp_context(sessions);
	if (yesno) {
		if ((err = ms_media_stream_sessions_fill_srtp_context_all_stream(sessions))) {
			return err;
		}
	}
	sessions->srtp_context->send_rtp_context.mandatory_enabled=yesno;
	sessions->srtp_context->recv_rtp_context.mandatory_enabled=yesno;
	return 0;
}

bool_t ms_media_stream_sessions_get_encryption_mandatory(const MSMediaStreamSessions *sessions) {

	if(!sessions->srtp_context)
		return FALSE;

	return 	   sessions->srtp_context->send_rtp_context.mandatory_enabled
			&& sessions->srtp_context->recv_rtp_context.mandatory_enabled;
}

#else /* HAVE_SRTP */

typedef void* srtp_t;
typedef int err_status_t;

bool_t ms_srtp_supported(void){
	return FALSE;
}

int ms_srtp_init(void) {
	return -1;
}

void ms_srtp_shutdown(void){
}

int ms_media_stream_sessions_set_srtp_recv_key_b64(struct _MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key, MSSrtpKeySource source){
	ms_error("Unable to set srtp recv key b64: srtp support disabled in mediastreamer2");
	return -1;
}

int ms_media_stream_sessions_set_srtp_recv_key(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, MSSrtpKeySource source){
	ms_error("Unable to set srtp recv key: srtp support disabled in mediastreamer2");
	return -1;
}

int ms_media_stream_sessions_set_srtp_send_key_b64(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key, MSSrtpKeySource source){
	ms_error("Unable to set srtp send key b64: srtp support disabled in mediastreamer2");
	return -1;
}

int ms_media_stream_sessions_set_srtp_send_key(MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, MSSrtpKeySource source){
	ms_error("Unable to set srtp send key: srtp support disabled in mediastreamer2");
	return -1;
}

int ms_media_stream_session_set_encryption_mandatory(MSMediaStreamSessions *sessions, bool_t yesno) {
	ms_error("Unable to set srtp encryption mandatory mode: srtp support disabled in mediastreamer2");
	return -1;
}

bool_t ms_media_stream_sessions_get_encryption_mandatory(const MSMediaStreamSessions *sessions) {
	ms_error("Unable to get srtp encryption mandatory mode: srtp support disabled in mediastreamer2");
	return -1;
}

bool_t ms_media_stream_sessions_secured(const MSMediaStreamSessions *sessions,MediaStreamDir dir) {
	return FALSE;
}
 
MSSrtpKeySource ms_media_stream_sessions_get_srtp_key_source(const MSMediaStreamSessions *sessions, MediaStreamDir dir) {
	return MSSrtpKeySourceUnavailable;
}

MSCryptoSuite ms_media_stream_sessions_get_srtp_crypto_suite(const MSMediaStreamSessions *sessions, MediaStreamDir dir) {
	return MS_CRYPTO_SUITE_INVALID;
}

void ms_srtp_context_delete(MSSrtpCtx* session) {
	ms_error("Unable to delete srtp context [%p]: srtp support disabled in mediastreamer2",session);
}

int ms_media_stream_sessions_set_encryption_mandatory(MSMediaStreamSessions *sessions, bool_t yesno) {
	ms_error("Unable to set encryption_mandatory [%p]: srtp support disabled in mediastreamer2",sessions);
	return -1;
}
#endif
