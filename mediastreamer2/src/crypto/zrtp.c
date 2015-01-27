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

#ifdef WIN32
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
	char *zidFilename; /**< cache filename */
	char *peerURI; /**< use for cache management */
};

typedef enum {
	rtp_stream,
	rtcp_stream
} stream_type;



/***********************************************/
/***** LOCAL FUNCTIONS                     *****/
/***********************************************/

/********************/
/* Helper functions */
static ORTP_INLINE uint64_t get_timeval_in_millis() {
	struct timeval t;
	ortp_gettimeofday(&t,NULL);
	return (1000LL*t.tv_sec)+(t.tv_usec/1000LL);
}

/*****************************************/
/* ZRTP library Callbacks implementation */

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
static int32_t ms_zrtp_sendDataZRTP (void *clientData, uint8_t* data, int32_t length ){
	MSZrtpContext *userData = (MSZrtpContext *)clientData;
	RtpSession *session = userData->stream_sessions->rtp_session;
	RtpTransport *rtpt=NULL;
	mblk_t *msg;

	ms_message("ZRTP Send packet type %.8s on rtp session [%p]", data+16, session);

	/* get RTP transport from session */
	rtp_session_get_transports(session,&rtpt,NULL);

	/* generate message from raw data */
 	msg = rtp_session_create_packet_raw(data, length);

	meta_rtp_transport_modifier_inject_packet(rtpt, userData->rtp_modifier, msg , 0);

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
static int32_t ms_zrtp_srtpSecretsAvailable(void* clientData, bzrtpSrtpSecrets_t* secrets, uint8_t part) {
	MSZrtpContext *userData = (MSZrtpContext *)clientData;


	// Get authentication and cipher algorithms in srtp format
	if ((secrets->authTagAlgo != ZRTP_AUTHTAG_HS32) && ((secrets->authTagAlgo != ZRTP_AUTHTAG_HS80))) {
		ms_fatal("unsupported authentication algorithm by srtp");
	}

	if ((secrets->cipherAlgo != ZRTP_CIPHER_AES1) && (secrets->cipherAlgo != ZRTP_CIPHER_AES2) && (secrets->cipherAlgo != ZRTP_CIPHER_AES3)) {
		ms_fatal("unsupported cipher algorithm by srtp");
	}

	ms_message("ZRTP secrets are ready for %s; auth tag algo is %s", (part==ZRTP_SRTP_SECRETS_FOR_SENDER)?"sender":"receiver", (secrets->authTagAlgo==ZRTP_AUTHTAG_HS32)?"HS32":"HS80");


	if (part==ZRTP_SRTP_SECRETS_FOR_RECEIVER) {
		uint8_t *key = (uint8_t *)ms_malloc0((secrets->peerSrtpKeyLength+secrets->peerSrtpSaltLength+16)*sizeof(uint8_t));
		memcpy(key, secrets->peerSrtpKey, secrets->peerSrtpKeyLength);
		memcpy(key + secrets->peerSrtpKeyLength, secrets->peerSrtpSalt, secrets->peerSrtpSaltLength);

		if (secrets->authTagAlgo == ZRTP_AUTHTAG_HS32){
			media_stream_set_srtp_recv_key(userData->stream_sessions, MS_AES_128_SHA1_32, (const char *)key, (secrets->peerSrtpKeyLength+secrets->peerSrtpSaltLength), MSSRTP_ALL_STREAMS);
		}else if (secrets->authTagAlgo == ZRTP_AUTHTAG_HS80){
			media_stream_set_srtp_recv_key(userData->stream_sessions, MS_AES_128_SHA1_80, (const char *)key, (secrets->peerSrtpKeyLength+secrets->peerSrtpSaltLength), MSSRTP_ALL_STREAMS);
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
			media_stream_set_srtp_send_key(userData->stream_sessions, MS_AES_128_SHA1_32, (const char *)key, (secrets->selfSrtpKeyLength+secrets->selfSrtpSaltLength), MSSRTP_ALL_STREAMS);
		}else if (secrets->authTagAlgo == ZRTP_AUTHTAG_HS80){
			media_stream_set_srtp_send_key(userData->stream_sessions, MS_AES_128_SHA1_80, (const char *)key, (secrets->selfSrtpKeyLength+secrets->selfSrtpSaltLength), MSSRTP_ALL_STREAMS);
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
 * @param[in]	sas		The SAS string(4 characters, not null terminated, fixed length)
 * @param[in]	verified	if <code>verified</code> is true then SAS was verified by both parties during a previous call.
 */
static int ms_zrtp_startSrtpSession(void *clientData, char* sas, int32_t verified ){
	MSZrtpContext *userData = (MSZrtpContext *)clientData;

	// srtp processing is enabled in SecretsReady fuction when receiver secrets are ready
	// Indeed, the secrets on is called before both parts are given to secretsReady.

	OrtpEventData *eventData;
	OrtpEvent *ev;

	if (sas != NULL) {
		ev=ortp_event_new(ORTP_EVENT_ZRTP_SAS_READY);
		eventData=ortp_event_get_data(ev);
		memcpy(eventData->info.zrtp_sas.sas,sas,4);
		eventData->info.zrtp_sas.sas[4]=0;
		eventData->info.zrtp_sas.verified=(verified != 0) ? TRUE : FALSE;
		rtp_session_dispatch_event(userData->stream_sessions->rtp_session, ev);
		ms_message("ZRTP secrets on: SAS is %.4s previously verified %s", sas, verified == 0 ? "no" : "yes");
	}

	ev=ortp_event_new(ORTP_EVENT_ZRTP_ENCRYPTION_CHANGED);
	eventData=ortp_event_get_data(ev);
	eventData->info.zrtp_stream_encrypted=1;
	rtp_session_dispatch_event(userData->stream_sessions->rtp_session, ev);
	ms_message("Event dispatched to all: secrets are on");


	return 0;
}

/**
 * @brief Load the zrtp cache file into the given buffer
 * The output buffer is allocated by this function and is freed by lib bzrtp
 *
 * @param[in]	clientData	Pointer to our ZrtpContext structure used to retrieve ZID filename
 * @param[out]	output		Output buffer contains an XML null terminated string: the whole cache file. Is allocated by this function and freed by lib bzrtp
 * @param[out]	outputSize	Buffer length in bytes
 * @return	outputSize
 */
static int ms_zrtp_loadCache(void *clientData, uint8_t** output, uint32_t *outputSize) {
	/* get filename from ClientData */
	MSZrtpContext *userData = (MSZrtpContext *)clientData;
	char *filename = userData->zidFilename;
	FILE *CACHEFD = fopen(filename, "r+");
	if (CACHEFD == NULL) { /* file doesn't seem to exist, try to create it */
		CACHEFD = fopen(filename, "w");
		if (CACHEFD != NULL) { /* file created with success */
			*output = NULL;
			*outputSize = 0;
			fclose(CACHEFD);
			return 0;
		}
		return -1;
	}
	fseek(CACHEFD, 0L, SEEK_END);  /* Position to end of file */
  	*outputSize = ftell(CACHEFD);     /* Get file length */
  	rewind(CACHEFD);               /* Back to start of file */
	*output = (uint8_t *)malloc(*outputSize*sizeof(uint8_t)+1); /* string must be null terminated */
	fread(*output, 1, *outputSize, CACHEFD);
	*(*output+*outputSize) = '\0';
	*outputSize += 1;
	fclose(CACHEFD);
	return *outputSize;
}

/**
 * @brief Dump the content of a string in the cache file
 * @param[in]	clientData	Pointer to our ZrtpContext structure used to retrieve ZID filename
 * @param[in]	input		An XML string to be dumped into cache
 * @param[in]	inputSize	input string length in bytes
 * @return	number of bytes written to file
 */
static int ms_zrtp_writeCache(void *clientData, uint8_t* input, uint32_t inputSize) {
	/* get filename from ClientData */
	MSZrtpContext *userData = (MSZrtpContext *)clientData;
	char *filename = userData->zidFilename;

	FILE *CACHEFD = fopen(filename, "w+");
	int retval = fwrite(input, 1, inputSize, CACHEFD);
	fclose(CACHEFD);
	return retval;

}

/**
 * @brief This callback is called when context is ready to compute exported keys as in rfc6189 section 4.5.2
 * Computed keys are added to zid cache with sip URI of peer(found in client Data) to be used for IM ciphering
 *
 * @param[in]	clientData	Pointer to our ZrtpContext structure used to retrieve peer SIP URI
 * @param[in]	peerZid		Peer ZID to address correct node in zid cache
 * @param[in]	role		RESPONDER or INITIATOR, needed to compute the pair of keys for IM ciphering
 *
 * @return 	0 on success
 */
static int ms_zrtp_addExportedKeysInZidCache(void *clientData, uint8_t peerZid[12], uint8_t role) {
	MSZrtpContext *userData = (MSZrtpContext *)clientData;
	bzrtpContext_t *zrtpContext = userData->zrtpContext;

	if (userData->peerURI) {
		/* Write the peer sip URI in cache */
		bzrtp_addCustomDataInCache(zrtpContext, peerZid, (uint8_t *)"uri", 3, (uint8_t *)(userData->peerURI), strlen(userData->peerURI), 0, BZRTP_CUSTOMCACHE_PLAINDATA, BZRTP_CACHE_LOADFILE|BZRTP_CACHE_DONTWRITEFILE);
	}

	/* Derive the master keys and session Id 32 bytes each */
	bzrtp_addCustomDataInCache(zrtpContext, peerZid, (uint8_t *)"sndKey", 6, (uint8_t *)((role==RESPONDER)?"ResponderKey":"InitiatorKey"), 12, 32, BZRTP_CUSTOMCACHE_USEKDF, BZRTP_CACHE_DONTLOADFILE|BZRTP_CACHE_DONTWRITEFILE);
	bzrtp_addCustomDataInCache(zrtpContext, peerZid, (uint8_t *)"rcvKey", 6, (uint8_t *)((role==RESPONDER)?"InitiatorKey":"ResponderKey"), 12, 32, BZRTP_CUSTOMCACHE_USEKDF, BZRTP_CACHE_DONTLOADFILE|BZRTP_CACHE_DONTWRITEFILE);
	bzrtp_addCustomDataInCache(zrtpContext, peerZid, (uint8_t *)"sndSId", 6, (uint8_t *)((role==RESPONDER)?"ResponderSId":"InitiatorSId"), 12, 32, BZRTP_CUSTOMCACHE_USEKDF, BZRTP_CACHE_DONTLOADFILE|BZRTP_CACHE_DONTWRITEFILE);
	bzrtp_addCustomDataInCache(zrtpContext, peerZid, (uint8_t *)"rcvSId", 6, (uint8_t *)((role==RESPONDER)?"InitiatorSId":"ResponderSId"), 12, 32, BZRTP_CUSTOMCACHE_USEKDF, BZRTP_CACHE_DONTLOADFILE|BZRTP_CACHE_DONTWRITEFILE);

	/* Derive session index, 4 bytes */
	bzrtp_addCustomDataInCache(zrtpContext, peerZid, (uint8_t *)"sndIndex", 6, (uint8_t *)((role==RESPONDER)?"ResponderIndex":"InitiatorIndex"), 14, 4, BZRTP_CUSTOMCACHE_USEKDF, BZRTP_CACHE_DONTLOADFILE|BZRTP_CACHE_DONTWRITEFILE);
	bzrtp_addCustomDataInCache(zrtpContext, peerZid, (uint8_t *)"rcvIndex", 6, (uint8_t *)((role==RESPONDER)?"InitiatorIndex":"ResponderIndex"), 14, 4, BZRTP_CUSTOMCACHE_USEKDF, BZRTP_CACHE_DONTLOADFILE|BZRTP_CACHE_WRITEFILE);

	return 0;
}

/*************************************************/
/*** end of Callback functions implementations ***/


/*******************************************************/
/**** Transport Modifier Sender/Receiver functions  ****/

static int ms_zrtp_rtp_process_on_send(struct _RtpTransportModifier *t, mblk_t *msg){
	return msgdsize(msg);
}
static int ms_zrtp_rtcp_process_on_send(struct _RtpTransportModifier *t, mblk_t *msg)  {
	return msgdsize(msg);
}

static int ms_zrtp_rtp_process_on_receive(struct _RtpTransportModifier *t, mblk_t *msg){
	uint32_t *magicField;

	MSZrtpContext *userData = (MSZrtpContext*) t->data;
	bzrtpContext_t *zrtpContext = userData->zrtpContext;
	uint8_t* rtp;
	int rtpVersion;
	int msgLength = msgdsize(msg);

	// send a timer tick to the zrtp engine
	bzrtp_iterate(zrtpContext, userData->self_ssrc, get_timeval_in_millis());

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
	ms_message("ZRTP Receive packet type %.8s", rtp+16);

	// send ZRTP packet to engine
	bzrtp_processMessage(zrtpContext, userData->self_ssrc, rtp, msgLength);
	return 0;
}

/* Nothing to do on rtcp packets, just return packet length */
static int ms_zrtp_rtcp_process_on_receive(struct _RtpTransportModifier *t, mblk_t *msg)  {
	return msgdsize(msg);
}


/**************************************/
/**** session management functions ****/
static void ms_zrtp_transport_modifier_destroy(RtpTransportModifier *tp)  {
	ms_free(tp);
}

static int ms_zrtp_transport_modifier_new(MSZrtpContext* ctx, RtpTransportModifier **rtpt, RtpTransportModifier **rtcpt ) {
	if (rtpt){
		*rtpt=ms_new0(RtpTransportModifier,1);
		(*rtpt)->data=ctx; /* back link to get access to the other fields of the OrtoZrtpContext from the RtpTransportModifier structure */
		(*rtpt)->t_process_on_send=ms_zrtp_rtp_process_on_send;
		(*rtpt)->t_process_on_receive=ms_zrtp_rtp_process_on_receive;
		(*rtpt)->t_destroy=ms_zrtp_transport_modifier_destroy;
	}
	if (rtcpt){
		*rtcpt=ms_new0(RtpTransportModifier,1);
		(*rtcpt)->data=ctx; /* back link to get access to the other fields of the OrtoZrtpContext from the RtpTransportModifier structure */
		(*rtcpt)->t_process_on_send=ms_zrtp_rtcp_process_on_send;
		(*rtcpt)->t_process_on_receive=ms_zrtp_rtcp_process_on_receive;
		(*rtcpt)->t_destroy=ms_zrtp_transport_modifier_destroy;
	}
	return 0;
}

static MSZrtpContext* createUserData(bzrtpContext_t *context, MSZrtpParams *params) {
	MSZrtpContext *userData=ms_new0(MSZrtpContext,1);
	userData->zrtpContext=context;
	/* get the zidFilename (if any)*/
	if (params->zid_file != NULL) {
		userData->zidFilename = (char *)malloc(strlen(params->zid_file)+1);
		memcpy(userData->zidFilename, params->zid_file, strlen(params->zid_file));
		userData->zidFilename[strlen(params->zid_file)] = '\0';
	} else {
		userData->zidFilename = NULL;
	}

	return userData;
}

static MSZrtpContext* ms_zrtp_configure_context(MSZrtpContext *userData, RtpSession *s) {
	bzrtpContext_t *context=userData->zrtpContext;
	RtpTransport *rtpt=NULL,*rtcpt=NULL;
	RtpTransportModifier *rtp_modifier, *rtcp_modifier;

	rtp_session_get_transports(s,&rtpt,&rtcpt);

	ms_zrtp_transport_modifier_new(userData, &rtp_modifier,&rtcp_modifier);
	meta_rtp_transport_append_modifier(rtpt, rtp_modifier);
	meta_rtp_transport_append_modifier(rtcpt, rtcp_modifier);

	/* save transport modifier into context, needed to inject packets generated by ZRTP */
	userData->rtp_modifier = rtp_modifier;

	ms_message("Starting ZRTP engine on rtp session [%p]",s);
	bzrtp_startChannelEngine(context, s->snd.ssrc);
	return userData;
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

	ms_message("Creating ZRTP engine on rtp session [%p]",sessions->rtp_session);
	context = bzrtp_createBzrtpContext(sessions->rtp_session->snd.ssrc); /* create the zrtp context, provide the SSRC of first channel */
	/* set callback functions */
	bzrtp_setCallback(context, (int (*)())ms_zrtp_sendDataZRTP, ZRTP_CALLBACK_SENDDATA);
	bzrtp_setCallback(context, (int (*)())ms_zrtp_srtpSecretsAvailable, ZRTP_CALLBACK_SRTPSECRETSAVAILABLE);
	bzrtp_setCallback(context, (int (*)())ms_zrtp_startSrtpSession, ZRTP_CALLBACK_STARTSRTPSESSION);
	if (params->zid_file) {
		/*enabling cache*/
		bzrtp_setCallback(context, (int (*)())ms_zrtp_loadCache, ZRTP_CALLBACK_LOADCACHE);
		bzrtp_setCallback(context, (int (*)())ms_zrtp_writeCache, ZRTP_CALLBACK_WRITECACHE);
		/* enable exportedKeys computation only if we have an uri to associate them */
		if (params->uri && strlen(params->uri)>0) {
			bzrtp_setCallback(context, (int (*)())ms_zrtp_addExportedKeysInZidCache, ZRTP_CALLBACK_CONTEXTREADYFOREXPORTEDKEYS);
		}
	}
	/* create and link user data */
	userData=createUserData(context, params);
	userData->stream_sessions=sessions;
	userData->self_ssrc = sessions->rtp_session->snd.ssrc;

	/* get the sip URI of peer and store it into the context to set it in the cache. Done only for the first channel as it is useless for the other ones which doesn't update the cache */
	if (params->uri && strlen(params->uri)>0) {
		userData->peerURI = strdup(params->uri);
	} else {
		userData->peerURI = NULL;
	}

	bzrtp_setClientData(context, sessions->rtp_session->snd.ssrc, (void *)userData);

	bzrtp_initBzrtpContext(context); /* init is performed only when creating the first channel context */
	return ms_zrtp_configure_context(userData, sessions->rtp_session);
}

MSZrtpContext* ms_zrtp_multistream_new(MSMediaStreamSessions *sessions, MSZrtpContext* activeContext, MSZrtpParams *params) {
	int retval;
	MSZrtpContext *userData;
	if ((retval = bzrtp_addChannel(activeContext->zrtpContext, sessions->rtp_session->snd.ssrc)) != 0) {
		ms_warning("could't add stream: multistream not supported by peer %x", retval);
	}

	ms_message("Initializing multistream ZRTP context");
	userData=createUserData(activeContext->zrtpContext, params);
	userData->stream_sessions = sessions;
	userData->self_ssrc = sessions->rtp_session->snd.ssrc;
	bzrtp_setClientData(activeContext->zrtpContext, sessions->rtp_session->snd.ssrc, (void *)userData);

	return ms_zrtp_configure_context(userData, sessions->rtp_session);
}

void ms_zrtp_context_destroy(MSZrtpContext *ctx) {
	ms_message("Stopping ZRTP context");
	bzrtp_destroyBzrtpContext(ctx->zrtpContext, ctx->self_ssrc);

	if (ctx->zidFilename) free(ctx->zidFilename);
	if (ctx->peerURI) free(ctx->peerURI);
	free(ctx);
	ms_message("ORTP-ZRTP context destroyed");
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


#else

MSZrtpContext* ms_zrtp_context_new(MSMediaStreamSessions *sessions, MSZrtpParams *params){
	ms_message("ZRTP is disabled");
	return NULL;
}

MSZrtpContext* ms_zrtp_multistream_new(MSMediaStreamSessions *sessions, MSZrtpContext* activeContext, MSZrtpParams *params) {
	ms_message("ZRTP is disabled - not adding stream");
	return NULL;
}

bool_t ms_zrtp_available(){return FALSE;}
void ms_zrtp_sas_verified(MSZrtpContext* ctx){}
void ms_zrtp_sas_reset_verified(MSZrtpContext* ctx){}
void ms_zrtp_context_destroy(MSZrtpContext *ctx){}
void ms_zrtp_reset_transmition_timer(MSZrtpContext* ctx) {};
int ms_zrtp_transport_modifier_new(MSZrtpContext* ctx, RtpTransportModifier **rtpt, RtpTransportModifier **rtcpt ) {return 0;}
void ms_zrtp_transport_modifier_destroy(RtpTransportModifier *tp)  {}
void ms_zrtp_set_stream_sessions(MSZrtpContext *zrtp_context, MSMediaStreamSessions *stream_sessions) {}
#endif




