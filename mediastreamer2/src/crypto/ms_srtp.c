/*
  mediastreamer2 library - modular sound and video processing and streaming
  Copyright (C) 2006-2014 Belledonne Communications, Grenoble

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

#if defined(ANDROID) || !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
// Android and Windows phone don't use make install
#include <srtp.h>
#include <srtp_priv.h>
#else
#include <srtp/srtp.h>
#include <srtp/srtp_priv.h>
#endif


#include "ortp/b64.h"

#define SRTP_PAD_BYTES (SRTP_MAX_TRAILER_LEN + 4)

/***********************************************/
/***** LOCAL FUNCTIONS                     *****/
/***********************************************/

static srtp_stream_ctx_t * find_other_ssrc(srtp_t srtp, uint32_t ssrc){
	srtp_stream_ctx_t *stream;
	for (stream=srtp->stream_list;stream!=NULL;stream=stream->next){
		if (stream->ssrc!=ssrc) return stream;
	}
	return stream;
}

/**** Sender functions ****/
static int _process_on_send(RtpSession* session,srtp_t srtp,mblk_t *m,bool_t is_rtp){
	int slen;
	err_status_t err;
	rtp_header_t *rtp_header=is_rtp?(rtp_header_t*)m->b_rptr:NULL;
	rtcp_common_header_t *rtcp_header=!is_rtp?(rtcp_common_header_t*)m->b_rptr:NULL;

	slen=msgdsize(m);

	if (rtp_header && (slen>RTP_FIXED_HEADER_SIZE && rtp_header->version==2)) {
		/* defragment incoming message and enlarge the buffer for srtp to write its data */
		msgpullup(m,slen+SRTP_PAD_BYTES);
		err=srtp_protect(srtp,m->b_rptr,&slen);
	} else if (rtcp_header && (slen>RTP_FIXED_HEADER_SIZE && rtcp_header->version==2)) {
		/* defragment incoming message and enlarge the buffer for srtp to write its data */
		msgpullup(m,slen+SRTP_PAD_BYTES);
		err=srtp_protect_rtcp(srtp,m->b_rptr,&slen);

	} else {
		/*ignoring non rtp/rtcp packets*/
 		return slen;
 	}

	/* check return code from srtp_protect */
	if (err==err_status_ok){
		return slen;
	}
	ortp_error("srtp_protect%s() failed (%d)", is_rtp?"":"_rtcp", err);
	return -1;
}

static int ms_srtp_process_on_send(RtpTransportModifier *t, mblk_t *m){
	return _process_on_send(t->session,(srtp_t)t->data, m,TRUE);
}
static int ms_srtcp_process_on_send(RtpTransportModifier *t, mblk_t *m){
	return _process_on_send(t->session,(srtp_t)t->data, m,FALSE);
}

/**** Receiver functions ****/
/*
* The ssrc_any_inbound feature of the libsrtp is not working good.
* It cannot be changed dynamically nor removed.
* As a result we prefer not to use it, but instead the recv stream is configured with a dummy SSRC value.
* When the first packet arrives, or when the SSRC changes, then we change the ssrc value inside the srtp stream context,
* so that the stream that was configured with the dummy SSRC value becomes now fully valid.
*/
/* note added by jehan, no longer needed as policy ssrc_any_inbound can support rekeying if session->stream_template is reset
static void update_recv_stream(RtpSession *session, srtp_t srtp, uint32_t new_ssrc){
	uint32_t send_ssrc=rtp_session_get_send_ssrc(session);
	srtp_stream_ctx_t *recvstream=find_other_ssrc(srtp,htonl(send_ssrc));
	if (recvstream){
		recvstream->ssrc=new_ssrc;
	}
}
*/
static int _process_on_receive(RtpSession* session,srtp_t srtp,mblk_t *m,bool_t is_rtp, int err){
	int slen;
	err_status_t srtp_err;

	/* keep NON-RTP data unencrypted */
	if (is_rtp){
		rtp_header_t *rtp=(rtp_header_t*)m->b_rptr;
		if (err<RTP_FIXED_HEADER_SIZE || rtp->version!=2 )
			return err;
	}else{
		rtcp_common_header_t *rtcp=(rtcp_common_header_t*)m->b_rptr;
		if (err<(sizeof(rtcp_common_header_t)+4) || rtcp->version!=2 )
			return err;
	}

	slen=err;
	srtp_err = is_rtp?srtp_unprotect(srtp,m->b_rptr,&slen):srtp_unprotect_rtcp(srtp,m->b_rptr,&slen);
/*	if (srtp_err==err_status_no_ctx) {
		update_recv_stream(session,srtp,new_ssrc);
		slen=err;
		srtp_err = is_rtp?srtp_unprotect(srtp,m->b_rptr,&slen):srtp_unprotect_rtcp(srtp,m->b_rptr,&slen);
	}*/
	if (srtp_err==err_status_ok) {
		return slen;
	} else {
		ms_error("srtp_unprotect%s() failed (%d)", is_rtp?"":"_rtcp", srtp_err);
		return -1;
	}
}
static int ms_srtp_process_on_receive(RtpTransportModifier *t, mblk_t *m){
	return _process_on_receive(t->session,(srtp_t)t->data, m,TRUE,msgdsize(m));
}
static int ms_srtcp_process_on_receive(RtpTransportModifier *t, mblk_t *m){
	return _process_on_receive(t->session,(srtp_t)t->data, m,FALSE,msgdsize(m));
}




/**** Session management functions ****/

/**
 * deallocate transport modifier ressources
 * @param[in/out] tp	The transport modifier to be deallocated
 */
static void ms_srtp_transport_modifier_destroy(RtpTransportModifier *tp){
	ms_free(tp);
}

/**
 * create a new transport modifier for srtp
 * @param[in] srtp	the srtp session to be used by the modifier
 */
static int ms_srtp_transport_modifier_new(srtp_t srtp, RtpTransportModifier **rtpt, RtpTransportModifier **rtcpt ){
	if (rtpt) {
		(*rtpt)=ms_new0(RtpTransportModifier,1);
		(*rtpt)->data=srtp;
		(*rtpt)->t_process_on_send=ms_srtp_process_on_send;
		(*rtpt)->t_process_on_receive=ms_srtp_process_on_receive;
		(*rtpt)->t_destroy=ms_srtp_transport_modifier_destroy;
	}
	if (rtcpt) {
		(*rtcpt)=ms_new0(RtpTransportModifier,1);
		(*rtcpt)->data=srtp;
		(*rtcpt)->t_process_on_send=ms_srtcp_process_on_send;
		(*rtcpt)->t_process_on_receive=ms_srtcp_process_on_receive;
		(*rtcpt)->t_destroy=ms_srtp_transport_modifier_destroy;
	}
	return 0;
}

static int ms_check_srtp_session_created(struct _MSMediaStreamSessions *sessions,  MSSrtpStreamType stream_type){
	if (stream_type == MSSRTP_ALL_STREAMS) { /* This is the usual case when both RTP and RTCP share the same key */
		if (sessions->srtp_session==NULL){
			err_status_t err;
			srtp_t srtp_session;
			RtpTransport *rtp=NULL,*rtcp=NULL;
			RtpTransportModifier *rtp_modifier, *rtcp_modifier;

			err = srtp_create(&srtp_session, NULL);
			if (err != 0) {
				ms_error("Failed to create srtp session (%d)", err);
				return -1;
			}

			sessions->srtp_session=srtp_session;
			ms_srtp_transport_modifier_new(srtp_session,&rtp_modifier,&rtcp_modifier);
			rtp_session_get_transports(sessions->rtp_session,&rtp,&rtcp);
			meta_rtp_transport_append_modifier(rtp, rtp_modifier);
			meta_rtp_transport_append_modifier(rtcp, rtcp_modifier);
			sessions->is_secured=TRUE;
		}
	} else if (stream_type == MSSRTP_RTP_STREAM) { /* Allocate only the RTP stream */
		if (sessions->srtp_session==NULL){
			err_status_t err;
			srtp_t srtp_session;
			RtpTransport *rtp=NULL;
			RtpTransportModifier *rtp_modifier;

			err = srtp_create(&srtp_session, NULL);
			if (err != 0) {
				ms_error("Failed to create srtp session (%d)", err);
				return -1;
			}

			sessions->srtp_session=srtp_session;
			ms_srtp_transport_modifier_new(srtp_session,&rtp_modifier, NULL);
			rtp_session_get_transports(sessions->rtp_session,&rtp,NULL);
			meta_rtp_transport_append_modifier(rtp, rtp_modifier);
			if (sessions->srtp_rtcp_session!=NULL) {
				sessions->is_secured=TRUE;
			} else {
				sessions->is_secured=FALSE;
			}
		}
	} else if (stream_type == MSSRTP_RTCP_STREAM) { /* Allocate only the RTCP stream */
		if (sessions->srtp_rtcp_session==NULL){
			err_status_t err;
			srtp_t srtp_session;
			RtpTransport *rtcp=NULL;
			RtpTransportModifier *rtcp_modifier;

			err = srtp_create(&srtp_session, NULL);
			if (err != 0) {
				ms_error("Failed to create srtp session (%d)", err);
				return -1;
			}

			sessions->srtp_rtcp_session=srtp_session;
			ms_srtp_transport_modifier_new(srtp_session,NULL,&rtcp_modifier);
			rtp_session_get_transports(sessions->rtp_session,NULL,&rtcp);
			meta_rtp_transport_append_modifier(rtcp, rtcp_modifier);
			if (sessions->srtp_session!=NULL) {
				sessions->is_secured=TRUE;
			} else {
				sessions->is_secured=FALSE;
			}
		}
	}
	return 0;
}

static int ms_set_srtp_crypto_policy(MSCryptoSuite suite, crypto_policy_t *policy) {
	switch(suite){
		case MS_AES_128_SHA1_32:
			// srtp doc says: not adapted to rtcp...
			crypto_policy_set_aes_cm_128_hmac_sha1_32(policy);
			break;
		case MS_AES_128_NO_AUTH:
			// srtp doc says: not adapted to rtcp...
			crypto_policy_set_aes_cm_128_null_auth(policy);
			break;
		case MS_NO_CIPHER_SHA1_80:
			crypto_policy_set_null_cipher_hmac_sha1_80(policy);
			break;
		case MS_AES_128_SHA1_80: /*default mode*/
			crypto_policy_set_aes_cm_128_hmac_sha1_80(policy);
			break;
		case MS_AES_256_SHA1_80:
			crypto_policy_set_aes_cm_256_hmac_sha1_80(policy);
			break;
		case MS_AES_256_SHA1_32:
			crypto_policy_set_aes_cm_256_hmac_sha1_32(policy);
			break;
		case MS_CRYPTO_SUITE_INVALID:
			return -1;
			break;
	}
	return 0;
}

static int ms_add_srtp_stream(srtp_t srtp, MSCryptoSuite suite, uint32_t ssrc, const char* key, size_t key_length, bool_t inbound, MSSrtpStreamType stream_type)
{
	srtp_policy_t policy;
	err_status_t err;
	ssrc_t ssrc_conf;

	memset(&policy,0,sizeof(policy));

	switch (stream_type) {
		case MSSRTP_ALL_STREAMS:
			if (ms_set_srtp_crypto_policy(suite, &policy.rtp) != 0) {
				return -1;
			}
			if (ms_set_srtp_crypto_policy(suite, &policy.rtcp) != 0) {
				return -1;
			}
			break;

		case MSSRTP_RTP_STREAM:
			if (ms_set_srtp_crypto_policy(suite, &policy.rtp) != 0) {
				return -1;
			}
			break;

		case MSSRTP_RTCP_STREAM:
			if (ms_set_srtp_crypto_policy(suite, &policy.rtcp) != 0) {
				return -1;
			}
			break;

		default:
			ms_error("Invalid stream_type(%d) given when trying to add a srtp stream", stream_type);
			return -1;

	}
	/* check if key length match given policy */
	if (stream_type==MSSRTP_ALL_STREAMS || stream_type==MSSRTP_RTP_STREAM) {
		if (key_length != policy.rtp.cipher_key_len) {
			ms_error("Key size (%i) doesn't match the selected srtp profile (required %d)", (int)key_length, policy.rtp.cipher_key_len);	
			return -1;
		}
	} else {
		if (key_length != policy.rtcp.cipher_key_len) {
			ms_error("Key size (%i) doesn't match the selected srtp profile (required %d)", (int)key_length, policy.rtcp.cipher_key_len);	
			return -1;
		}
	}

	if (!inbound)
		policy.allow_repeat_tx=1; /*necessary for telephone-events*/

	ssrc_conf.type=inbound ? ssrc_any_inbound : ssrc_specific;
	/*ssrc_conf.type=ssrc_specific;*/
	ssrc_conf.value=ssrc;

	policy.ssrc = ssrc_conf;
	policy.key = (uint8_t *)key;
	policy.next = NULL;
	if (inbound && srtp->stream_template) {
		/* deallocate stream template, if there is one to be still be able to have late ssrc*/
		/*	auth_dealloc(srtp->stream_template->rtcp_auth);
		    cipher_dealloc(srtp->stream_template->rtcp_cipher);
		    crypto_free(srtp->stream_template->limit);
		    auth_dealloc(srtp->stream_template->rtp_auth);
		    rdbx_dealloc(&srtp->stream_template->rtp_rdbx);*/
		    crypto_free(srtp->stream_template);
		    srtp->stream_template=NULL;
	}
	err = srtp_add_stream(srtp, &policy);
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
			err_reporting_init("mediastreamer2");
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

/**** Public Functions ****/
/* header declared in include/mediastreamer2/ms_srtp.h */
bool_t ms_srtp_supported(void){
	return TRUE;
}


int media_stream_set_srtp_recv_key_b64(struct _MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key){
	int retval;

	/* decode b64 key */
	size_t b64_key_length = strlen(b64_key);
	size_t key_length = b64_decode(b64_key, b64_key_length, 0, 0);
	char *key = (char *) ms_malloc0(key_length+2); /*srtp uses padding*/
	if (b64_decode(b64_key, b64_key_length, key, key_length) != key_length) {
		ms_error("Error decoding b64 srtp recv key");
		ms_free(key);
		return -1;
	}

	/* pass decoded key to set_recv_key function */
	retval = media_stream_set_srtp_recv_key(sessions, suite, key, key_length, MSSRTP_ALL_STREAMS);

	ms_free(key);

	return retval;
}

int media_stream_set_srtp_recv_key(struct _MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, MSSrtpStreamType stream_type){

	uint32_t ssrc,send_ssrc;
	srtp_stream_ctx_t *srtp_stream = NULL;
	bool_t updated=FALSE;
	srtp_t srtp_session;

	if (ms_check_srtp_session_created(sessions, stream_type)==-1) {
		return -1;
	}

	switch(stream_type){
		case MSSRTP_ALL_STREAMS:
		case MSSRTP_RTP_STREAM:
			srtp_session = sessions->srtp_session;
			break;
		case MSSRTP_RTCP_STREAM:
			srtp_session = sessions->srtp_rtcp_session;
			break;
		default:
			ms_error("Invalid stream_type %d in set_srtp_recv_key on sessions [%p]", stream_type, sessions);
			return -1;
	}

	/*check if a previous key was configured, in which case remove it*/
	send_ssrc=rtp_session_get_send_ssrc(sessions->rtp_session);
	srtp_stream = find_other_ssrc(srtp_session,htonl(send_ssrc));
	if (srtp_stream != NULL) {
		ssrc = srtp_stream->ssrc;
	} else {
		ssrc = 0;
	}

	/*careful: remove_stream takes the SSRC in network byte order...*/
	if (srtp_remove_stream(srtp_session, ssrc)==0) {
		updated=TRUE;
	}
	ssrc=rtp_session_get_recv_ssrc(sessions->rtp_session);
	ms_message("media_stream_set_srtp_recv_key(): %s key %02x..%02x\nsrtp session is %p ssrc %08x",updated ? "changing to" : "starting with", (uint8_t)key[0], (uint8_t)key[key_length-1], srtp_session, ssrc);
	
	return ms_add_srtp_stream(srtp_session,suite, ssrc, key, key_length, TRUE, stream_type);
}

int media_stream_set_srtp_send_key_b64(struct _MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key){
	int retval;

	/* decode b64 key */
	size_t b64_key_length = strlen(b64_key);
	size_t key_length = b64_decode(b64_key, b64_key_length, 0, 0);
	char *key = (char *) ms_malloc0(key_length+2); /*srtp uses padding*/
	if (b64_decode(b64_key, b64_key_length, key, key_length) != key_length) {
		ms_error("Error decoding b64 srtp send key");
		ms_free(key);
		return -1;
	}

	/* pass decoded key to set_send_key function */
	retval = media_stream_set_srtp_send_key(sessions, suite, key, key_length, MSSRTP_ALL_STREAMS);

	ms_free(key);

	return retval;
}

int media_stream_set_srtp_send_key(struct _MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, MSSrtpStreamType stream_type){

	uint32_t ssrc;
	bool_t updated=FALSE;
	srtp_t srtp_session;

	if (ms_check_srtp_session_created(sessions, stream_type)==-1) {
		return -1;
	}

	switch(stream_type){
		case MSSRTP_ALL_STREAMS:
		case MSSRTP_RTP_STREAM:
			srtp_session = sessions->srtp_session;
			break;
		case MSSRTP_RTCP_STREAM:
			srtp_session = sessions->srtp_rtcp_session;
			break;
		default:
			ms_error("Invalid stream_type %d in set_srtp_send_key on sessions [%p]", stream_type, sessions);
			return -1;
	}

	/*check if a previous key was configured, in which case remove it*/
	ssrc=rtp_session_get_send_ssrc(sessions->rtp_session);
	if (ssrc!=0){
		/*careful: remove_stream takes the SSRC in network byte order...*/
		if (srtp_remove_stream(srtp_session,htonl(ssrc))==0)
			updated=TRUE;
	}
	ms_message("media_stream_set_srtp_send_key(): %s key %02x..%02x\nsrtp session is %p ssrc %08x",updated ? "changing to" : "starting with", (uint8_t)key[0], (uint8_t)key[key_length-1], srtp_session, ssrc);

	return ms_add_srtp_stream(srtp_session, suite, ssrc, key, key_length, FALSE, stream_type);
}



int ms_srtp_dealloc(MSSrtpCtx session)
{
	return (int)srtp_dealloc((srtp_t)session);
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

int media_stream_set_srtp_recv_key_b64(struct _MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key){
	ms_error("Unable to set srtp recv key b64: srtp support disabled in mediastreamer2");
	return -1;
}

int media_stream_set_srtp_recv_key(struct _MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, MSSrtpStreamType stream_type){
	ms_error("Unable to set srtp recv key: srtp support disabled in mediastreamer2");
	return -1;
}

int media_stream_set_srtp_send_key_b64(struct _MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* b64_key){
	ms_error("Unable to set srtp send key b64: srtp support disabled in mediastreamer2");
	return -1;
}

int media_stream_set_srtp_send_key(struct _MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char* key, size_t key_length, MSSrtpStreamType stream_type){
	ms_error("Unable to set srtp send key: srtp support disabled in mediastreamer2");
	return -1;
}

int ms_srtp_dealloc(MSSrtpCtx session)
{
	return -1;
}
#endif
