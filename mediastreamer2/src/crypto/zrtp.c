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

#include "mediastreamer2/zrtp.h"
#include "mediastreamer2/mediastream.h"

#ifdef _WIN32
#include <malloc.h>
#endif

#ifdef HAVE_ZRTP
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#include <bzrtp/bzrtp.h>

struct _MSZrtpContext{
	MSMediaStreamSessions *stream_sessions; /**< a retro link to the stream session as we need it to configure srtp sessions */
	uint32_t self_ssrc; /**< store the sender ssrc as it is needed by zrtp to manage channels(and we may destroy stream_sessions before destroying zrtp's one) */
	RtpTransportModifier *rtp_modifier; /**< transport modifier needed to be able to inject the ZRTP packet for sending */
	bzrtpContext_t *zrtpContext; /**< the opaque zrtp context from libbzrtp */
	/* cache related data */
	uint32_t limeKeyTimeSpan; /**< amount in seconds of the lime key life span */
	void *cacheDB; /**< pointer to an already open sqlite db holding the zid cache */
};

/***********************************************/
/***** LOCAL FUNCTIONS                     *****/
/***********************************************/

/********************/
/* Helper functions */

/* trace functions: bzrtp algo code to string */
static const char *bzrtp_hash_toString(uint8_t hashAlgo) {
	switch(hashAlgo) {
		case(ZRTP_UNSET_ALGO): return "unset";
		case(ZRTP_HASH_S256): return "SHA-256";
		case(ZRTP_HASH_S384): return "SHA-384";
		case(ZRTP_HASH_N256): return "SHA3-256";
		case(ZRTP_HASH_N384): return "SHA3-384";
		default: return "Unknown Algo";
	}
}

static const char *bzrtp_keyAgreement_toString(uint8_t keyAgreementAlgo) {
	switch(keyAgreementAlgo) {
		case(ZRTP_UNSET_ALGO): return "unset";
		case(ZRTP_KEYAGREEMENT_DH2k): return "DHM-2048";
		case(ZRTP_KEYAGREEMENT_EC25): return "ECDH-256";
		case(ZRTP_KEYAGREEMENT_DH3k): return "DHM-3072";
		case(ZRTP_KEYAGREEMENT_EC38): return "ECDH-384";
		case(ZRTP_KEYAGREEMENT_EC52): return "ECDH-521";
		case(ZRTP_KEYAGREEMENT_X255): return "ECDH-255";
		case(ZRTP_KEYAGREEMENT_X448): return "ECDH-448";
		case(ZRTP_KEYAGREEMENT_Prsh): return "PreShared";
		case(ZRTP_KEYAGREEMENT_Mult): return "MultiStream";
		default: return "Unknown Algo";
	}
}

static const char *bzrtp_cipher_toString(uint8_t cipherAlgo) {
	switch(cipherAlgo) {
		case(ZRTP_UNSET_ALGO): return "unset";
		case(ZRTP_CIPHER_AES1): return "AES-128";
		case(ZRTP_CIPHER_AES2): return "AES-192";
		case(ZRTP_CIPHER_AES3): return "AES-256";
		case(ZRTP_CIPHER_2FS1): return "TwoFish-128";
		case(ZRTP_CIPHER_2FS2): return "TwoFish-192";
		case(ZRTP_CIPHER_2FS3): return "TwoFish-256";
		default: return "Unknown Algo";
	}
}

static const char *bzrtp_authtag_toString(uint8_t authtagAlgo) {
	switch(authtagAlgo) {
		case(ZRTP_UNSET_ALGO): return "unset";
		case(ZRTP_AUTHTAG_HS32): return "HMAC-SHA1-32";
		case(ZRTP_AUTHTAG_HS80): return "HMAC-SHA1-80";
		case(ZRTP_AUTHTAG_SK32): return "Skein-32";
		case(ZRTP_AUTHTAG_SK64): return "Skein-64";
		default: return "Unknown Algo";
	}
}

static const char *bzrtp_sas_toString(uint8_t sasAlgo) {
	switch(sasAlgo) {
		case(ZRTP_UNSET_ALGO): return "unset";
		case(ZRTP_SAS_B32): return "Base32";
		case(ZRTP_SAS_B256): return "PGP-WordList";
		default: return "Unknown Algo";
	}
}
/*****************************************/
/* ZRTP library Callbacks implementation */

/**
 * @brief collect messages from the bzrtp lib, log them and pass some through ORTP_EVENTS to liblinphone
 *
 * @param[in]	clientData	Pointer to our ZrtpContext structure
 * @param[in]	messageLevel	One of  BZRTP_MESSAGE_ERROR, BZRTP_MESSAGE_WARNING, BZRTP_MESSAGE_LOG, BZRTP_MESSAGE_DEBUG
 * @param[in]	messageId	Message code mapped to an int as defined in bzrtp.h
 * @param[in]	messageString	Can be NULL or a NULL terminated string
 *
 * @return	0 on success
 */
static int ms_zrtp_statusMessage(void *clientData, const uint8_t messageLevel, const uint8_t messageId, const char *messageString) {
	MSZrtpContext *userData = (MSZrtpContext *)clientData;
	OrtpEventData *eventData;
	OrtpEvent *ev;

	switch (messageId) {
		case BZRTP_MESSAGE_CACHEMISMATCH:
			ev=ortp_event_new(ORTP_EVENT_ZRTP_CACHE_MISMATCH);
			eventData=ortp_event_get_data(ev);
			eventData->info.zrtp_info.cache_mismatch=1;
			rtp_session_dispatch_event(userData->stream_sessions->rtp_session, ev);
			ms_warning("Zrtp Event dispatched : cache mismatch"); /* log it as a warning */
			break;
		case BZRTP_MESSAGE_PEERVERSIONOBSOLETE:
			ev=ortp_event_new(ORTP_EVENT_ZRTP_PEER_VERSION_OBSOLETE);
			eventData=ortp_event_get_data(ev);
			rtp_session_dispatch_event(userData->stream_sessions->rtp_session, ev);
			ms_message("Zrtp Event dispatched : Peer ZRTP engine version is obsolete and may not allow LIME to work correctly, peer ZRTP engine identifies itself as %.16s", messageString==NULL?"NULL":messageString);
			break;
		case BZRTP_MESSAGE_PEERNOTBZRTP:
			/* Do not forward this message upward, just log it, shall we use this to prevent LIME keys creation as it is unlikely peer implement LIME? */
			ms_warning("Peer ZRTP engine version is not BZRTP and would not allow LIME to work correctly, peer ZRTP engine identifies itself as %.16s", messageString==NULL?"NULL":messageString);
			break;
		default:
			/* unexepected message, do nothing, just log it as a warning */
			ms_warning("Zrtp Message Unknown : Level %d Id %d message %s", messageLevel, messageId, messageString==NULL?"NULL":messageString);
			break;
	}

	return 0;
}

/**
* @brief Send a ZRTP packet via RTP transport modifiers.
*
* ZRTP calls this method to send a ZRTP packet via the RTP session.
*
* @param[in]	clientData	Pointer to our ZrtpContext structure used to retrieve RTP session
* @param[in]	data		Points to ZRTP message to send.
* @param[in]	length		The length in bytes of the data
* @return	0 on success
*/
static int32_t ms_zrtp_sendDataZRTP (void *clientData, const uint8_t* data, uint16_t length ){
	MSZrtpContext *userData = (MSZrtpContext *)clientData;
	RtpSession *session = userData->stream_sessions->rtp_session;
	RtpTransport *rtpt=NULL;
	mblk_t *msg;

	ms_message("ZRTP Send packet type %.8s on rtp session [%p]", data+16, session);

	/* get RTP transport from session */
	rtp_session_get_transports(session,&rtpt,NULL);

	/* generate message from raw data */
 	msg = rtp_session_create_packet_raw(data, length);

	meta_rtp_transport_modifier_inject_packet_to_send(rtpt, userData->rtp_modifier, msg , 0);

	freemsg(msg);

	return 0;
}

/**
 * @briefThis function is called by ZRTP engine as soon as SRTP secrets are ready to be used
 * Depending on which role we assume in the ZRTP protocol (Initiator or Responder, randomly selected)
 * both secrets may not be available at the same time, the part argument is either
 * ZRTP_SRTP_SECRETS_FOR_SENDER or ZRTP_SRTP_SECRETS_FOR_RECEIVER.
 * Secrets are used to set up SRTP sessions
 *
 * @param[in]	clientData	Pointer to our ZrtpContext structure used to retrieve stream sessions structure needed to setup SRTP sessions
 * @param[in]	secrets		The SRTP keys and algorithm setup
 * @param[in]	part		for receiver or for sender in order to determine which SRTP stream the secret apply to
 * @return 	0 on success
 */
static int32_t ms_zrtp_srtpSecretsAvailable(void* clientData, const bzrtpSrtpSecrets_t *secrets, uint8_t part) {
	MSZrtpContext *userData = (MSZrtpContext *)clientData;


	// Get authentication and cipher algorithms in srtp format
	if ((secrets->authTagAlgo != ZRTP_AUTHTAG_HS32) && ((secrets->authTagAlgo != ZRTP_AUTHTAG_HS80))) {
		ms_fatal("unsupported authentication algorithm by srtp");
	}

	if ((secrets->cipherAlgo != ZRTP_CIPHER_AES1) && (secrets->cipherAlgo != ZRTP_CIPHER_AES3)) {
		ms_fatal("unsupported cipher algorithm by srtp");
	}

	ms_message("ZRTP secrets are ready for %s; auth tag algo is %s and cipher algo is %s", (part==ZRTP_SRTP_SECRETS_FOR_SENDER)?"sender":"receiver", bzrtp_authtag_toString(secrets->authTagAlgo), bzrtp_cipher_toString(secrets->cipherAlgo));


	if (part==ZRTP_SRTP_SECRETS_FOR_RECEIVER) {
		uint8_t *key = (uint8_t *)ms_malloc0((secrets->peerSrtpKeyLength+secrets->peerSrtpSaltLength+16)*sizeof(uint8_t));
		memcpy(key, secrets->peerSrtpKey, secrets->peerSrtpKeyLength);
		memcpy(key + secrets->peerSrtpKeyLength, secrets->peerSrtpSalt, secrets->peerSrtpSaltLength);

		if (secrets->authTagAlgo == ZRTP_AUTHTAG_HS32){
			if (secrets->cipherAlgo == ZRTP_CIPHER_AES3){
				ms_media_stream_sessions_set_srtp_recv_key(userData->stream_sessions, MS_AES_256_SHA1_32, (const char *)key, (secrets->peerSrtpKeyLength+secrets->peerSrtpSaltLength), MSSRTP_ALL_STREAMS);
			}else{
				ms_media_stream_sessions_set_srtp_recv_key(userData->stream_sessions, MS_AES_128_SHA1_32, (const char *)key, (secrets->peerSrtpKeyLength+secrets->peerSrtpSaltLength), MSSRTP_ALL_STREAMS);
			}
		}else if (secrets->authTagAlgo == ZRTP_AUTHTAG_HS80){
			if (secrets->cipherAlgo == ZRTP_CIPHER_AES3){
				ms_media_stream_sessions_set_srtp_recv_key(userData->stream_sessions, MS_AES_256_SHA1_80, (const char *)key, (secrets->peerSrtpKeyLength+secrets->peerSrtpSaltLength), MSSRTP_ALL_STREAMS);
			}else{
				ms_media_stream_sessions_set_srtp_recv_key(userData->stream_sessions, MS_AES_128_SHA1_80, (const char *)key, (secrets->peerSrtpKeyLength+secrets->peerSrtpSaltLength), MSSRTP_ALL_STREAMS);
			}
		}else{
			ms_fatal("unsupported auth tag");
		}
		ms_free(key);
	}

	if (part==ZRTP_SRTP_SECRETS_FOR_SENDER) {
		uint8_t *key = (uint8_t *)ms_malloc0((secrets->selfSrtpKeyLength+secrets->selfSrtpSaltLength+16)*sizeof(uint8_t));
		memcpy(key, secrets->selfSrtpKey, secrets->selfSrtpKeyLength);
		memcpy(key + secrets->selfSrtpKeyLength, secrets->selfSrtpSalt, secrets->selfSrtpSaltLength);

		if (secrets->authTagAlgo == ZRTP_AUTHTAG_HS32){
			if (secrets->cipherAlgo == ZRTP_CIPHER_AES3){
				ms_media_stream_sessions_set_srtp_send_key(userData->stream_sessions, MS_AES_256_SHA1_32, (const char *)key, (secrets->selfSrtpKeyLength+secrets->selfSrtpSaltLength), MSSRTP_ALL_STREAMS);
			}else{
				ms_media_stream_sessions_set_srtp_send_key(userData->stream_sessions, MS_AES_128_SHA1_32, (const char *)key, (secrets->selfSrtpKeyLength+secrets->selfSrtpSaltLength), MSSRTP_ALL_STREAMS);
			}
		}else if (secrets->authTagAlgo == ZRTP_AUTHTAG_HS80){
			if (secrets->cipherAlgo == ZRTP_CIPHER_AES3){
				ms_media_stream_sessions_set_srtp_send_key(userData->stream_sessions, MS_AES_256_SHA1_80, (const char *)key, (secrets->selfSrtpKeyLength+secrets->selfSrtpSaltLength), MSSRTP_ALL_STREAMS);
			}else{
				ms_media_stream_sessions_set_srtp_send_key(userData->stream_sessions, MS_AES_128_SHA1_80, (const char *)key, (secrets->selfSrtpKeyLength+secrets->selfSrtpSaltLength), MSSRTP_ALL_STREAMS);
			}
		}else{
			ms_fatal("unsupported auth tag");
		}
		ms_free(key);
	}

	return 0;
}

/**
 * @brief Switch on the security.
 *
 * ZRTP calls this method after it has computed the SAS and checked
 * if it was verified in the past.
 *
 * This method must enable SRTP processing if it was not enabled
 * during setSecretsReady().
 *
 * This call will trigger an event which shall be catched by linphone_call_handle_stream_events
 *
 * @param[in]	clientData	Pointer to our ZrtpContext structure used to retrieve RTP session
 * @param[in]	secrets 	A structure containing the SAS string(null terminated, variable length according to SAS rendering algo choosen) and informations about the crypto algorithms used by ZRTP during negotiation
 * @param[in]	verified	if <code>verified</code> is true then SAS was verified by both parties during a previous call.
 */
static int ms_zrtp_startSrtpSession(void *clientData,  const bzrtpSrtpSecrets_t *secrets, int32_t verified ){
	MSZrtpContext *userData = (MSZrtpContext *)clientData;

	// srtp processing is enabled in SecretsReady fuction when receiver secrets are ready
	// Indeed, the secrets on is called before both parts are given to secretsReady.

	OrtpEventData *eventData;
	OrtpEvent *ev;

	if (secrets->sas != NULL) {
		ev=ortp_event_new(ORTP_EVENT_ZRTP_SAS_READY);
		eventData=ortp_event_get_data(ev);
		// support both b32 and b256 format SAS strings
		snprintf(eventData->info.zrtp_info.sas, sizeof(eventData->info.zrtp_info.sas), "%s", secrets->sas);
		eventData->info.zrtp_info.verified=(verified != 0) ? TRUE : FALSE;
		eventData->info.zrtp_info.cache_mismatch=( secrets->cacheMismatch != 0) ? TRUE : FALSE;
		rtp_session_dispatch_event(userData->stream_sessions->rtp_session, ev);
		ms_message("ZRTP secrets on: SAS is %.32s previously verified %s on session [%p]", secrets->sas, verified == 0 ? "no" : "yes", userData->stream_sessions);
		ms_message("ZRTP algo used during negotiation: Cipher: %s - KeyAgreement: %s - Hash: %s - AuthTag: %s - Sas Rendering: %s", bzrtp_cipher_toString(secrets->cipherAlgo), bzrtp_keyAgreement_toString(secrets->keyAgreementAlgo), bzrtp_hash_toString(secrets->hashAlgo), bzrtp_authtag_toString(secrets->authTagAlgo), bzrtp_sas_toString(secrets->sasAlgo));
	}

	ev=ortp_event_new(ORTP_EVENT_ZRTP_ENCRYPTION_CHANGED);
	eventData=ortp_event_get_data(ev);
	eventData->info.zrtp_stream_encrypted=1;
	rtp_session_dispatch_event(userData->stream_sessions->rtp_session, ev);
	ms_message("Event dispatched to all: secrets are on");


	return 0;
}

/**
 * @brief This callback is called when context is ready to compute exported keys as in rfc6189 section 4.5.2
 * Computed keys are added to zid cache with sip URI of peer(found in client Data) to be used for LIME(IM ciphering)
 *
 * @param[in]	zidCacheData	Pointer to our ZidCacheContext structure used to retrieve ZID filename
 * @param[in]	clientData	Pointer to our ZrtpContext structure used to retrieve peer SIP URI
 * @param[in]	peerZid		Peer ZID to address correct node in zid cache
 * @param[in]	role		RESPONDER or INITIATOR, needed to compute the pair of keys for IM ciphering
 *
 * @return 	0 on success
 */
static int ms_zrtp_addExportedKeysInZidCache(void *clientData, int zuid, uint8_t role) {
	MSZrtpContext *userData = (MSZrtpContext *)clientData;
	bzrtpContext_t *zrtpContext = userData->zrtpContext;
	bctoolboxTimeSpec currentTime;
	/* columns to be written in cache */
	const char *colNames[] = {"sndKey", "rcvKey", "sndSId", "rcvSId", "sndIndex", "rcvIndex", "valid"};
	uint8_t *colValues[7];
	size_t colLength[] = {32, 32, 32, 32, 4, 4, 8}; /* data length: keys and session ID : 32 bytes, Indexes: 4 bytes(uint32_t), validity : 8 bytes(UTC time as int64_t) */
	int i,ret;

	/* allocate colValues */
	for (i=0; i<7; i++) {
		colValues[i] = (uint8_t *)ms_malloc(colLength[i]*sizeof(uint8_t));
	}

	/* First compute the exported keys */
	bzrtp_exportKey(zrtpContext, ((role==BZRTP_ROLE_RESPONDER)?"ResponderKey":"InitiatorKey"), 12, colValues[0], &colLength[0]); /* sndKey */
	bzrtp_exportKey(zrtpContext, ((role==BZRTP_ROLE_RESPONDER)?"InitiatorKey":"ResponderKey"), 12, colValues[1], &colLength[1]); /* rcvKey */
	bzrtp_exportKey(zrtpContext, ((role==BZRTP_ROLE_RESPONDER)?"ResponderSId":"InitiatorSId"), 12, colValues[2], &colLength[2]); /* snd Session Id*/
	bzrtp_exportKey(zrtpContext, ((role==BZRTP_ROLE_RESPONDER)?"InitiatorSId":"ResponderSId"), 12, colValues[3], &colLength[3]); /* rcv Session Id*/
	bzrtp_exportKey(zrtpContext, ((role==BZRTP_ROLE_RESPONDER)?"ResponderIndex":"InitiatorIndex"), 14, colValues[4], &colLength[4]); /* snd Index */
	bzrtp_exportKey(zrtpContext, ((role==BZRTP_ROLE_RESPONDER)?"InitiatorIndex":"ResponderIndex"), 14, colValues[5], &colLength[5]); /* rcv Index */

	/* insert validity */
	if (userData->limeKeyTimeSpan == 0) { /* time span is 0: key is forever valid, just put 0 in cache */
		memset(colValues[6], 0, 8);
	} else { /* get current time and add time span */
		bctbx_get_utc_cur_time(&currentTime);
		bctbx_timespec_add(&currentTime, (int64_t)(userData->limeKeyTimeSpan));
		/* store the int64_t in big endian in the cache(cache is not typed, all data seen as blob) */
		colValues[6][0] = (currentTime.tv_sec>>56)&0xFF;
		colValues[6][1] = (currentTime.tv_sec>>48)&0xFF;
		colValues[6][2] = (currentTime.tv_sec>>40)&0xFF;
		colValues[6][3] = (currentTime.tv_sec>>32)&0xFF;
		colValues[6][4] = (currentTime.tv_sec>>24)&0xFF;
		colValues[6][5] = (currentTime.tv_sec>>16)&0xFF;
		colValues[6][6] = (currentTime.tv_sec>>8)&0xFF;
		colValues[6][7] = (currentTime.tv_sec)&0xFF;
	}

	/* colValues 4 and 5 hold index(uint32_t as 4 bytes in big endian), make sure the first bit on the MSB is set to 0 to avoid loop in the index */
	colValues[4][0] &= 0x7F;
	colValues[5][0] &= 0x7F;

	/* then insert all in cache */
	ret = bzrtp_cache_write(userData->cacheDB, zuid, "lime", colNames, colValues, colLength, 7);

	for (i=0; i<7; i++) {
		ms_free(colValues[i]);
	}

	return ret;
}

/*************************************************/
/*** end of Callback functions implementations ***/


/*******************************************************/
/**** Transport Modifier Sender/Receiver functions  ****/

static int ms_zrtp_rtp_process_on_send(RtpTransportModifier *t, mblk_t *msg){
	return (int)msgdsize(msg);
}
static int ms_zrtp_rtcp_process_on_send(RtpTransportModifier *t, mblk_t *msg)  {
	return (int)msgdsize(msg);
}

static void ms_zrtp_rtp_on_schedule(RtpTransportModifier *t){
	MSZrtpContext *userData = (MSZrtpContext*) t->data;
	bzrtpContext_t *zrtpContext = userData->zrtpContext;
	// send a timer tick to the zrtp engine
	bzrtp_iterate(zrtpContext, userData->self_ssrc, bctbx_get_cur_time_ms());
}

static int ms_zrtp_rtp_process_on_receive(RtpTransportModifier *t, mblk_t *msg){
	uint32_t *magicField;

	MSZrtpContext *userData = (MSZrtpContext*) t->data;
	bzrtpContext_t *zrtpContext = userData->zrtpContext;
	uint8_t* rtp;
	int rtpVersion;
	int msgLength = (int)msgdsize(msg);

	

	// check incoming message length
	if (msgLength<RTP_FIXED_HEADER_SIZE) {
		return msgLength;
	}

	rtp=msg->b_rptr;
	rtpVersion = ((rtp_header_t*)rtp)->version;
	magicField=(uint32_t *)(rtp + 4);

	// Check if there is a ZRTP packet to receive
	if (rtpVersion!=0 || ntohl(*magicField) != ZRTP_MAGIC_COOKIE) {
		return msgLength;
	}

	// display received message
	ms_message("ZRTP Receive packet type %.8s on rtp session [%p]", rtp+16, t->session);

	// check if the ZRTP channel is started, if not(ZRTP not set on our side but incoming zrtp packets), start it
	if (bzrtp_getChannelStatus(zrtpContext, userData->self_ssrc) == BZRTP_CHANNEL_INITIALISED) {
		bzrtp_startChannelEngine(zrtpContext, userData->self_ssrc);
	}

	// send ZRTP packet to engine
	bzrtp_processMessage(zrtpContext, userData->self_ssrc, rtp, msgLength);
	return 0;
}

/* Nothing to do on rtcp packets, just return packet length */
static int ms_zrtp_rtcp_process_on_receive(RtpTransportModifier *t, mblk_t *msg)  {
	return (int)msgdsize(msg);
}


/**************************************/
/**** session management functions ****/
static void ms_zrtp_transport_modifier_destroy(RtpTransportModifier *tp)  {
	ms_free(tp);
}

static int ms_zrtp_transport_modifier_new(MSZrtpContext* ctx, RtpTransportModifier **rtpt, RtpTransportModifier **rtcpt ) {
	if (rtpt){
		*rtpt=ms_new0(RtpTransportModifier,1);
		(*rtpt)->data=ctx; /* back link to get access to the other fields of the OrtpZrtpContext from the RtpTransportModifier structure */
		(*rtpt)->t_process_on_send=ms_zrtp_rtp_process_on_send;
		(*rtpt)->t_process_on_receive=ms_zrtp_rtp_process_on_receive;
		(*rtpt)->t_destroy=ms_zrtp_transport_modifier_destroy;
		(*rtpt)->t_process_on_schedule = ms_zrtp_rtp_on_schedule;
	}
	if (rtcpt){
		*rtcpt=ms_new0(RtpTransportModifier,1);
		(*rtcpt)->data=ctx; /* back link to get access to the other fields of the OrtpZrtpContext from the RtpTransportModifier structure */
		(*rtcpt)->t_process_on_send=ms_zrtp_rtcp_process_on_send;
		(*rtcpt)->t_process_on_receive=ms_zrtp_rtcp_process_on_receive;
		(*rtcpt)->t_destroy=ms_zrtp_transport_modifier_destroy;
	}
	return 0;
}

static MSZrtpContext* ms_zrtp_configure_context(MSZrtpContext *userData, RtpSession *s) {
	RtpTransport *rtpt=NULL,*rtcpt=NULL;
	RtpTransportModifier *rtp_modifier, *rtcp_modifier;

	rtp_session_get_transports(s,&rtpt,&rtcpt);

	ms_zrtp_transport_modifier_new(userData, &rtp_modifier,&rtcp_modifier);
	meta_rtp_transport_append_modifier(rtpt, rtp_modifier);
	meta_rtp_transport_append_modifier(rtcpt, rtcp_modifier);

	/* save transport modifier into context, needed to inject packets generated by ZRTP */
	userData->rtp_modifier = rtp_modifier;

	return userData;
}

static void set_hash_suites(bzrtpContext_t *ctx, const MSZrtpHash *hashes, const MsZrtpCryptoTypesCount count) {
	int i;
	uint8_t bzrtpCount = 0;
	uint8_t bzrtpHashes[7];

	for (i=0; i < count; i++) {
		switch (hashes[i]) {
			case MS_ZRTP_HASH_INVALID: break;
			case MS_ZRTP_HASH_S256: bzrtpHashes[bzrtpCount++] = ZRTP_HASH_S256; break;
			case MS_ZRTP_HASH_S384: bzrtpHashes[bzrtpCount++] = ZRTP_HASH_S384; break;
			case MS_ZRTP_HASH_N256: bzrtpHashes[bzrtpCount++] = ZRTP_HASH_N256; break;
			case MS_ZRTP_HASH_N384: bzrtpHashes[bzrtpCount++] = ZRTP_HASH_N384; break;
		}
	}

	bzrtp_setSupportedCryptoTypes(ctx, ZRTP_HASH_TYPE, bzrtpHashes, bzrtpCount);
}

static void set_cipher_suites(bzrtpContext_t *ctx, const MSZrtpCipher *ciphers, const MsZrtpCryptoTypesCount count) {
	int i;
	uint8_t bzrtpCount = 0;
	uint8_t bzrtpCiphers[7];

	for (i=0; i < count; i++) {
		switch (ciphers[i]) {
			case MS_ZRTP_CIPHER_INVALID: break;
			case MS_ZRTP_CIPHER_AES1:    bzrtpCiphers[bzrtpCount++] = ZRTP_CIPHER_AES1; break;
			case MS_ZRTP_CIPHER_AES2:    bzrtpCiphers[bzrtpCount++] = ZRTP_CIPHER_AES2; break;
			case MS_ZRTP_CIPHER_AES3:    bzrtpCiphers[bzrtpCount++] = ZRTP_CIPHER_AES3; break;
			case MS_ZRTP_CIPHER_2FS1:    bzrtpCiphers[bzrtpCount++] = ZRTP_CIPHER_2FS1; break;
			case MS_ZRTP_CIPHER_2FS2:    bzrtpCiphers[bzrtpCount++] = ZRTP_CIPHER_2FS2; break;
			case MS_ZRTP_CIPHER_2FS3:    bzrtpCiphers[bzrtpCount++] = ZRTP_CIPHER_2FS3; break;
		}
	}

	bzrtp_setSupportedCryptoTypes(ctx, ZRTP_CIPHERBLOCK_TYPE, bzrtpCiphers, bzrtpCount);
}

static void set_auth_tag_suites(bzrtpContext_t *ctx, const MSZrtpAuthTag *authTags, const MsZrtpCryptoTypesCount count) {
	int i;
	uint8_t bzrtpCount = 0;
	uint8_t bzrtpAuthTags[7];

	for (i=0; i < count; i++) {
		switch (authTags[i]) {
			case MS_ZRTP_AUTHTAG_INVALID: break;
			case MS_ZRTP_AUTHTAG_HS32:    bzrtpAuthTags[bzrtpCount++] = ZRTP_AUTHTAG_HS32; break;
			case MS_ZRTP_AUTHTAG_HS80:    bzrtpAuthTags[bzrtpCount++] = ZRTP_AUTHTAG_HS80; break;
			case MS_ZRTP_AUTHTAG_SK32:    bzrtpAuthTags[bzrtpCount++] = ZRTP_AUTHTAG_SK32; break;
			case MS_ZRTP_AUTHTAG_SK64:    bzrtpAuthTags[bzrtpCount++] = ZRTP_AUTHTAG_SK64; break;
		}
	}

	bzrtp_setSupportedCryptoTypes(ctx, ZRTP_AUTHTAG_TYPE, bzrtpAuthTags, bzrtpCount);
}

static void set_key_agreement_suites(bzrtpContext_t *ctx, const MSZrtpKeyAgreement *keyAgreements, const MsZrtpCryptoTypesCount count) {
	int i;
	uint8_t bzrtpCount = 0;
	uint8_t bzrtpKeyAgreements[7];

	for (i=0; i < count; i++) {
		switch (keyAgreements[i]) {
			case MS_ZRTP_KEY_AGREEMENT_INVALID: break;
			case MS_ZRTP_KEY_AGREEMENT_DH2K:    bzrtpKeyAgreements[bzrtpCount++] = ZRTP_KEYAGREEMENT_DH2k; break;
			case MS_ZRTP_KEY_AGREEMENT_DH3K:    bzrtpKeyAgreements[bzrtpCount++] = ZRTP_KEYAGREEMENT_DH3k; break;
			case MS_ZRTP_KEY_AGREEMENT_EC25:    bzrtpKeyAgreements[bzrtpCount++] = ZRTP_KEYAGREEMENT_EC25; break;
			case MS_ZRTP_KEY_AGREEMENT_EC38:    bzrtpKeyAgreements[bzrtpCount++] = ZRTP_KEYAGREEMENT_EC38; break;
			case MS_ZRTP_KEY_AGREEMENT_EC52:    bzrtpKeyAgreements[bzrtpCount++] = ZRTP_KEYAGREEMENT_EC52; break;
			case MS_ZRTP_KEY_AGREEMENT_X255:    bzrtpKeyAgreements[bzrtpCount++] = ZRTP_KEYAGREEMENT_X255; break;
			case MS_ZRTP_KEY_AGREEMENT_X448:    bzrtpKeyAgreements[bzrtpCount++] = ZRTP_KEYAGREEMENT_X448; break;
		}
	}

	bzrtp_setSupportedCryptoTypes(ctx, ZRTP_KEYAGREEMENT_TYPE, bzrtpKeyAgreements, bzrtpCount);
}

static void set_sas_suites(bzrtpContext_t *ctx, const MSZrtpSasType *sasTypes, const MsZrtpCryptoTypesCount count) {
	int i;
	uint8_t bzrtpCount = 0;
	uint8_t bzrtpSasTypes[7];

	for (i=0; i < count; i++) {
		switch (sasTypes[i]) {
			case MS_ZRTP_SAS_INVALID: break;
			case MS_ZRTP_SAS_B32:     bzrtpSasTypes[bzrtpCount++] = ZRTP_SAS_B32; break;
			case MS_ZRTP_SAS_B256:    bzrtpSasTypes[bzrtpCount++] = ZRTP_SAS_B256; break;
		}
	}

	bzrtp_setSupportedCryptoTypes(ctx, ZRTP_SAS_TYPE, bzrtpSasTypes, bzrtpCount);
}

/***********************************************/
/***** EXPORTED FUNCTIONS                  *****/
/***********************************************/
/**** Private to mediastreamer2 functions ****/
/* header declared in voip/private.h */
void ms_zrtp_set_stream_sessions(MSZrtpContext *zrtp_context, MSMediaStreamSessions *stream_sessions) {
	if (zrtp_context!=NULL) {
		zrtp_context->stream_sessions = stream_sessions;
	}
}

/**** Public functions ****/
/* header declared in include/mediastreamer2/zrtp.h */
bool_t ms_zrtp_available(){return TRUE;}

MSZrtpContext* ms_zrtp_context_new(MSMediaStreamSessions *sessions, MSZrtpParams *params) {
	MSZrtpContext *userData;
	bzrtpContext_t *context;
	bzrtpCallbacks_t cbs={0};

	ms_message("Creating ZRTP engine on rtp session [%p] ssrc 0x%x",sessions->rtp_session, sessions->rtp_session->snd.ssrc);
	context = bzrtp_createBzrtpContext();

	if (params->zidCacheDB != NULL && params->selfUri != NULL && params->peerUri) { /* to enable cache we need a self and peer uri and a pointer to the sqlite cache DB */
		/*enabling cache*/
		bzrtp_setZIDCache(context, params->zidCacheDB, params->selfUri, params->peerUri);
		cbs.bzrtp_contextReadyForExportedKeys=ms_zrtp_addExportedKeysInZidCache;
	}

	/* set other callback functions */
	cbs.bzrtp_sendData=ms_zrtp_sendDataZRTP;
	cbs.bzrtp_srtpSecretsAvailable=ms_zrtp_srtpSecretsAvailable;
	cbs.bzrtp_startSrtpSession=ms_zrtp_startSrtpSession;
	cbs.bzrtp_statusMessage=ms_zrtp_statusMessage;
	cbs.bzrtp_messageLevel=BZRTP_MESSAGE_LOG; /* get log, warnings and error messages from bzrtp lib */

	bzrtp_setCallbacks(context, &cbs);

	/* set crypto params */
	set_hash_suites(context, params->hashes, params->hashesCount);
	set_cipher_suites(context, params->ciphers, params->ciphersCount);
	set_auth_tag_suites(context, params->authTags, params->authTagsCount);
	set_key_agreement_suites(context, params->keyAgreements, params->keyAgreementsCount);
	set_sas_suites(context, params->sasTypes, params->sasTypesCount);

	/* complete the initialisation of zrtp context, this will also create the main channel context with the given SSRC */
	bzrtp_initBzrtpContext(context, sessions->rtp_session->snd.ssrc); /* init is performed only when creating the main channel context */

	/* create and link main channel user data */
	userData=ms_new0(MSZrtpContext,1);
	userData->zrtpContext=context;
	userData->stream_sessions=sessions;
	userData->self_ssrc = sessions->rtp_session->snd.ssrc;

	userData->limeKeyTimeSpan = params->limeKeyTimeSpan;
	userData->cacheDB = params->zidCacheDB; /* add a link to the ZidCache to be able to free it when we will destroy this context */

	bzrtp_setClientData(context, sessions->rtp_session->snd.ssrc, (void *)userData);

	return ms_zrtp_configure_context(userData, sessions->rtp_session);
}

MSZrtpContext* ms_zrtp_multistream_new(MSMediaStreamSessions *sessions, MSZrtpContext* activeContext) {
	int retval;
	MSZrtpContext *userData;
	if ((retval = bzrtp_addChannel(activeContext->zrtpContext, sessions->rtp_session->snd.ssrc)) != 0) {
		ms_warning("ZRTP could't add stream, returns %x", retval);
	}

	ms_message("Initializing multistream ZRTP context on rtp session [%p] ssrc 0x%x",sessions->rtp_session, sessions->rtp_session->snd.ssrc);
	userData=ms_new0(MSZrtpContext,1);
	userData->zrtpContext=activeContext->zrtpContext;
	userData->stream_sessions = sessions;
	userData->self_ssrc = sessions->rtp_session->snd.ssrc;
	/* no cache related information here as it is not needed for multistream channel */
	bzrtp_setClientData(activeContext->zrtpContext, sessions->rtp_session->snd.ssrc, (void *)userData);

	return ms_zrtp_configure_context(userData, sessions->rtp_session);
}

int ms_zrtp_channel_start(MSZrtpContext *ctx) {
	int retval;
	ms_message("Starting ZRTP engine on rtp session [%p] ssrc 0x%x",ctx->stream_sessions->rtp_session, ctx->self_ssrc);
	if ((retval = bzrtp_startChannelEngine(ctx->zrtpContext, ctx->self_ssrc)) != 0) {
		/* remap some error code */
		if (retval == BZRTP_ERROR_CHANNELALREADYSTARTED) {
			ms_message("ZRTP channel already started");
			return MSZRTP_ERROR_CHANNEL_ALREADY_STARTED;
		} else {
			ms_message("Unable to start ZRTP channel, error code %x", retval);
		}
	}
	return retval;
}


void ms_zrtp_context_destroy(MSZrtpContext *ctx) {
	ms_message("Stopping ZRTP context on session [%p]", ctx->stream_sessions ? ctx->stream_sessions->rtp_session : NULL);
	bzrtp_destroyBzrtpContext(ctx->zrtpContext, ctx->self_ssrc);
	ms_free(ctx);
	ms_message("ZRTP context destroyed");
}

void ms_zrtp_reset_transmition_timer(MSZrtpContext* ctx) {
	bzrtp_resetRetransmissionTimer(ctx->zrtpContext, ctx->self_ssrc);
}

void ms_zrtp_sas_verified(MSZrtpContext* ctx){
	bzrtp_SASVerified(ctx->zrtpContext);
}

void ms_zrtp_sas_reset_verified(MSZrtpContext* ctx){
	bzrtp_resetSASVerified(ctx->zrtpContext);
}

int ms_zrtp_getHelloHash(MSZrtpContext* ctx, uint8_t *output, size_t outputLength) {
	return bzrtp_getSelfHelloHash(ctx->zrtpContext, ctx->self_ssrc, output, outputLength);
}

int ms_zrtp_setPeerHelloHash(MSZrtpContext *ctx, uint8_t *peerHelloHashHexString, size_t peerHelloHashHexStringLength) {
	return bzrtp_setPeerHelloHash(ctx->zrtpContext, ctx->self_ssrc, peerHelloHashHexString, peerHelloHashHexStringLength);
}

/**
 * @brief Check the given sqlite3 DB and create requested tables if needed
 * 	Also manage DB schema upgrade
 * @param[in/out]	db	Pointer to the sqlite3 db open connection
 * 				Use a void * to keep this API when building cacheless
 *
 * @return 0 on succes, MSZRTP_CACHE_SETUP if cache was empty, MSZRTP_CACHE_UPDATE if db structure was updated error code otherwise
 */
int ms_zrtp_initCache(void *db) {
	int ret = bzrtp_initCache(db);
	switch (ret) {
		case BZRTP_CACHE_SETUP:
			return MSZRTP_CACHE_SETUP;
		case BZRTP_CACHE_UPDATE:
			return MSZRTP_CACHE_UPDATE;
		case 0:
			return 0;
		default:
			ms_warning("bzrtp_initCache function returned a non zero code %x, something went probably wrong", ret);
			return MSZRTP_CACHE_ERROR;
	}
}

/**
 * @brief Perform migration from xml version to sqlite3 version of cache
 *	Warning: new version of cache associate a ZID to each local URI, the old one did not
 *		the migration function will associate any data in the cache to the sip URI given in parameter which shall be the default URI
 * @param[in]		cacheXml	a pointer to an xmlDocPtr structure containing the old cache to be migrated
 * @param[in/out]	cacheSqlite	a pointer to an sqlite3 structure containing a cache initialised using ms_zrtp_cache_init function
 * @param[in]		selfURI		default sip URI for this end point, NULL terminated char
 *
 * @return	0 on success, MSZRTP_ERROR_CACHEDISABLED when bzrtp was not compiled with cache enabled, MSZRTP_ERROR_CACHEMIGRATIONFAILED on error during migration
 */
int ms_zrtp_cache_migration(void *cacheXmlPtr, void *cacheSqlite, const char *selfURI) {
	int ret = bzrtp_cache_migration(cacheXmlPtr, cacheSqlite, selfURI);
	switch (ret) {
		case BZRTP_ERROR_CACHEDISABLED:
			return MSZRTP_ERROR_CACHEDISABLED;
		case BZRTP_ERROR_CACHEMIGRATIONFAILED:
			return MSZRTP_ERROR_CACHEMIGRATIONFAILED;
		case 0:
			return 0;
		default:
			ms_warning("bzrtp_cache_migration function returned a non zero code %x, something went probably wrong", ret);
			return MSZRTP_CACHE_ERROR;
	}
}

#else

MSZrtpContext* ms_zrtp_context_new(MSMediaStreamSessions *sessions, MSZrtpParams *params){
	ms_message("ZRTP is disabled");
	return NULL;
}

MSZrtpContext* ms_zrtp_multistream_new(MSMediaStreamSessions *sessions, MSZrtpContext* activeContext) {
	ms_message("ZRTP is disabled - not adding stream");
	return NULL;
}

int ms_zrtp_channel_start(MSZrtpContext *ctx) { return 0;}
bool_t ms_zrtp_available(){return FALSE;}
void ms_zrtp_sas_verified(MSZrtpContext* ctx){}
void ms_zrtp_sas_reset_verified(MSZrtpContext* ctx){}
void ms_zrtp_context_destroy(MSZrtpContext *ctx){}
void ms_zrtp_reset_transmition_timer(MSZrtpContext* ctx) {};
int ms_zrtp_transport_modifier_new(MSZrtpContext* ctx, RtpTransportModifier **rtpt, RtpTransportModifier **rtcpt ) {return 0;}
void ms_zrtp_transport_modifier_destroy(RtpTransportModifier *tp)  {}
void ms_zrtp_set_stream_sessions(MSZrtpContext *zrtp_context, MSMediaStreamSessions *stream_sessions) {}
int ms_zrtp_getHelloHash(MSZrtpContext* ctx, uint8_t *output, size_t outputLength) {return 0;}
int ms_zrtp_setPeerHelloHash(MSZrtpContext *ctx, uint8_t *peerHelloHashHexString, size_t peerHelloHashHexStringLength) {return 0;}
int ms_zrtp_initCache(void *db){return 0;}
int ms_zrtp_cache_migration(void *cacheXmlPtr, void *cacheSqlite, const char *selfURI) {return 0;}
#endif

#define STRING_COMPARE_RETURN(string, value)\
	if (strcmp(string,#value) == 0) return value

#define CASE_RETURN_STRING(value)\
	case value: return #value;

#define SWITCH_CRYPTO_ALGO(value, cases)\
	switch(value) {\
		cases\
	}\
	return "<NULL>";

MS2_PUBLIC MSZrtpHash ms_zrtp_hash_from_string(const char* str) {
	STRING_COMPARE_RETURN(str, MS_ZRTP_HASH_S256);
	STRING_COMPARE_RETURN(str, MS_ZRTP_HASH_S384);
	STRING_COMPARE_RETURN(str, MS_ZRTP_HASH_N256);
	STRING_COMPARE_RETURN(str, MS_ZRTP_HASH_N384);
	return MS_ZRTP_HASH_INVALID;
}

MS2_PUBLIC const char* ms_zrtp_hash_to_string(const MSZrtpHash hash) {
	SWITCH_CRYPTO_ALGO(hash,\
		CASE_RETURN_STRING(MS_ZRTP_HASH_INVALID);\
		CASE_RETURN_STRING(MS_ZRTP_HASH_S256);\
		CASE_RETURN_STRING(MS_ZRTP_HASH_S384);\
		CASE_RETURN_STRING(MS_ZRTP_HASH_N256);\
		CASE_RETURN_STRING(MS_ZRTP_HASH_N384);\
	);
}

MS2_PUBLIC MSZrtpCipher ms_zrtp_cipher_from_string(const char* str) {
	STRING_COMPARE_RETURN(str, MS_ZRTP_CIPHER_AES1);
	STRING_COMPARE_RETURN(str, MS_ZRTP_CIPHER_AES2);
	STRING_COMPARE_RETURN(str, MS_ZRTP_CIPHER_AES3);
	STRING_COMPARE_RETURN(str, MS_ZRTP_CIPHER_2FS1);
	STRING_COMPARE_RETURN(str, MS_ZRTP_CIPHER_2FS2);
	STRING_COMPARE_RETURN(str, MS_ZRTP_CIPHER_2FS3);
	return MS_ZRTP_CIPHER_INVALID;
}

MS2_PUBLIC const char* ms_zrtp_cipher_to_string(const MSZrtpCipher cipher) {
	SWITCH_CRYPTO_ALGO(cipher,\
		CASE_RETURN_STRING(MS_ZRTP_CIPHER_INVALID);\
		CASE_RETURN_STRING(MS_ZRTP_CIPHER_AES1);\
		CASE_RETURN_STRING(MS_ZRTP_CIPHER_AES2);\
		CASE_RETURN_STRING(MS_ZRTP_CIPHER_AES3);\
		CASE_RETURN_STRING(MS_ZRTP_CIPHER_2FS1);\
		CASE_RETURN_STRING(MS_ZRTP_CIPHER_2FS2);\
		CASE_RETURN_STRING(MS_ZRTP_CIPHER_2FS3);\
	);
}


MS2_PUBLIC MSZrtpAuthTag ms_zrtp_auth_tag_from_string(const char* str) {
	STRING_COMPARE_RETURN(str, MS_ZRTP_AUTHTAG_HS32);
	STRING_COMPARE_RETURN(str, MS_ZRTP_AUTHTAG_HS80);
	STRING_COMPARE_RETURN(str, MS_ZRTP_AUTHTAG_SK32);
	STRING_COMPARE_RETURN(str, MS_ZRTP_AUTHTAG_SK64);
	return MS_ZRTP_AUTHTAG_INVALID;
}

MS2_PUBLIC const char* ms_zrtp_auth_tag_to_string(const MSZrtpAuthTag authTag) {
	SWITCH_CRYPTO_ALGO(authTag,\
		CASE_RETURN_STRING(MS_ZRTP_AUTHTAG_INVALID);\
		CASE_RETURN_STRING(MS_ZRTP_AUTHTAG_HS32);\
		CASE_RETURN_STRING(MS_ZRTP_AUTHTAG_HS80);\
		CASE_RETURN_STRING(MS_ZRTP_AUTHTAG_SK32);\
		CASE_RETURN_STRING(MS_ZRTP_AUTHTAG_SK64);\
	);
}

MSZrtpKeyAgreement ms_zrtp_key_agreement_from_string(const char* str) {
	STRING_COMPARE_RETURN(str, MS_ZRTP_KEY_AGREEMENT_DH2K);
	STRING_COMPARE_RETURN(str, MS_ZRTP_KEY_AGREEMENT_DH3K);
	STRING_COMPARE_RETURN(str, MS_ZRTP_KEY_AGREEMENT_EC25);
	STRING_COMPARE_RETURN(str, MS_ZRTP_KEY_AGREEMENT_EC38);
	STRING_COMPARE_RETURN(str, MS_ZRTP_KEY_AGREEMENT_EC52);
	STRING_COMPARE_RETURN(str, MS_ZRTP_KEY_AGREEMENT_X255);
	STRING_COMPARE_RETURN(str, MS_ZRTP_KEY_AGREEMENT_X448);
	return MS_ZRTP_KEY_AGREEMENT_INVALID;
}

const char* ms_zrtp_key_agreement_to_string(const MSZrtpKeyAgreement keyAgreement) {
	SWITCH_CRYPTO_ALGO(keyAgreement,\
		CASE_RETURN_STRING(MS_ZRTP_KEY_AGREEMENT_INVALID);\
		CASE_RETURN_STRING(MS_ZRTP_KEY_AGREEMENT_DH2K);\
		CASE_RETURN_STRING(MS_ZRTP_KEY_AGREEMENT_DH3K);\
		CASE_RETURN_STRING(MS_ZRTP_KEY_AGREEMENT_EC25);\
		CASE_RETURN_STRING(MS_ZRTP_KEY_AGREEMENT_EC38);\
		CASE_RETURN_STRING(MS_ZRTP_KEY_AGREEMENT_EC52);\
		CASE_RETURN_STRING(MS_ZRTP_KEY_AGREEMENT_X255);\
		CASE_RETURN_STRING(MS_ZRTP_KEY_AGREEMENT_X448);\
	);
}

MS2_PUBLIC MSZrtpSasType ms_zrtp_sas_type_from_string(const char* str) {
	STRING_COMPARE_RETURN(str, MS_ZRTP_SAS_B32);
	STRING_COMPARE_RETURN(str, MS_ZRTP_SAS_B256);
	return MS_ZRTP_SAS_INVALID;
}

MS2_PUBLIC const char* ms_zrtp_sas_type_to_string(const MSZrtpSasType sasType) {
	SWITCH_CRYPTO_ALGO(sasType,\
		CASE_RETURN_STRING(MS_ZRTP_SAS_INVALID);\
		CASE_RETURN_STRING(MS_ZRTP_SAS_B32);\
		CASE_RETURN_STRING(MS_ZRTP_SAS_B256);\
	);
}
