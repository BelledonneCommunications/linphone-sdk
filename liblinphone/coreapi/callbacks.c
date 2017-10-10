/*
linphone
Copyright (C) 2010  Simon MORLAT (simon.morlat@free.fr)

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


#include "c-wrapper/internal/c-sal.h"
#include "sal/call-op.h"
#include "sal/message-op.h"
#include "sal/refer-op.h"

#include "linphone/core.h"
#include "linphone/utils/utils.h"
#include "private.h"
#include "mediastreamer2/mediastream.h"
#include "linphone/lpconfig.h"
#include <bctoolbox/defs.h>

// stat
#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "c-wrapper/c-wrapper.h"
#include "call/call-p.h"
#include "chat/chat-room.h"
#include "conference/session/call-session.h"
#include "conference/session/call-session-p.h"
#include "conference/session/media-session.h"
#include "conference/session/media-session-p.h"

using namespace LinphonePrivate;

static void register_failure(SalOp *op);

static bool_t already_a_call_with_remote_address(const LinphoneCore *lc, const LinphoneAddress *remote) {
	bctbx_list_t *elem;
	ms_message("Searching for already_a_call_with_remote_address.");

	for(elem=lc->calls;elem!=NULL;elem=elem->next){
		const LinphoneCall *call=(LinphoneCall*)elem->data;
		const LinphoneAddress *cRemote=linphone_call_get_remote_address(call);
		if (linphone_address_weak_equal(cRemote,remote)) {
			ms_warning("already_a_call_with_remote_address found.");
			return TRUE;
		}
	}
	return FALSE;
}


static LinphoneCall * look_for_broken_call_to_replace(LinphonePrivate::SalOp *h, LinphoneCore *lc) {
	const bctbx_list_t *calls = linphone_core_get_calls(lc);
	const bctbx_list_t *it = calls;
	while (it != NULL) {
#if 0
		LinphoneCall *replaced_call = NULL;
		LinphoneCall *call = (LinphoneCall *)bctbx_list_get_data(it);
		SalOp *replaced_op = sal_call_get_replaces(h);
		if (replaced_op) replaced_call = (LinphoneCall*)sal_op_get_user_pointer(replaced_op);
		if ((call->broken && sal_call_compare_op(h, call->op))
			|| ((replaced_call == call) && (strcmp(sal_op_get_from(h), sal_op_get_from(replaced_op)) == 0) && (strcmp(sal_op_get_to(h), sal_op_get_to(replaced_op)) == 0))) {
			return call;
		}
#endif
		it = bctbx_list_next(it);
	}

	return NULL;
}

static void call_received(SalCallOp *h) {
	/* Look if this INVITE is for a call that has already been notified but broken because of network failure */
	LinphoneCore *lc = reinterpret_cast<LinphoneCore *>(h->get_sal()->get_user_pointer());
	LinphoneCall *replacedCall = look_for_broken_call_to_replace(h, lc);
	if (replacedCall) {
		linphone_call_replace_op(replacedCall, h);
		return;
	}

	LinphoneAddress *fromAddr = nullptr;
	const char *pAssertedId = sal_custom_header_find(h->get_recv_custom_header(), "P-Asserted-Identity");
	/* In some situation, better to trust the network rather than the UAC */
	if (lp_config_get_int(linphone_core_get_config(lc), "sip", "call_logs_use_asserted_id_instead_of_from", 0)) {
		if (pAssertedId) {
			LinphoneAddress *pAssertedIdAddr = linphone_address_new(pAssertedId);
			if (pAssertedIdAddr) {
				ms_message("Using P-Asserted-Identity [%s] instead of from [%s] for op [%p]", pAssertedId, h->get_from(), h);
				fromAddr = pAssertedIdAddr;
			} else
				ms_warning("Unsupported P-Asserted-Identity header for op [%p] ", h);
		} else
			ms_warning("No P-Asserted-Identity header found so cannot use it for op [%p] instead of from", h);
	}

	if (!fromAddr)
		fromAddr = linphone_address_new(h->get_from());
	LinphoneAddress *toAddr = linphone_address_new(h->get_to());

	/* First check if we can answer successfully to this invite */
	LinphonePresenceActivity *activity = nullptr;
	if ((linphone_presence_model_get_basic_status(lc->presence_model) == LinphonePresenceBasicStatusClosed)
		&& (activity = linphone_presence_model_get_activity(lc->presence_model))) {
		char *altContact = nullptr;
		switch (linphone_presence_activity_get_type(activity)) {
			case LinphonePresenceActivityPermanentAbsence:
				altContact = linphone_presence_model_get_contact(lc->presence_model);
				if (altContact) {
					SalErrorInfo sei;
					memset(&sei, 0, sizeof(sei));
					sal_error_info_set(&sei, SalReasonRedirect, "SIP", 0, nullptr, nullptr);
					SalAddress *altAddr = sal_address_new(altContact);
					h->decline_with_error_info(&sei, altAddr);
					ms_free(altContact);
					sal_address_unref(altAddr);
					LinphoneErrorInfo *ei = linphone_error_info_new();
					linphone_error_info_set(ei, nullptr, LinphoneReasonMovedPermanently, 302, "Moved permanently", nullptr);
					linphone_core_report_early_failed_call(lc, LinphoneCallIncoming, fromAddr, toAddr, ei);
					h->release();
					sal_error_info_reset(&sei);
					return;
				}
				break;
			default:
				/* Nothing special to be done */
				break;
		}
	}

	if (!linphone_core_can_we_add_call(lc)) { /* Busy */
		h->decline(SalReasonBusy, nullptr);
		LinphoneErrorInfo *ei = linphone_error_info_new();
		linphone_error_info_set(ei, nullptr, LinphoneReasonBusy, 486, "Busy - too many calls", nullptr);
		linphone_core_report_early_failed_call(lc, LinphoneCallIncoming, fromAddr, toAddr, ei);
		h->release();
		return;
	}

	/* Check if I'm the caller */
	LinphoneAddress *fromAddressToSearchIfMe = nullptr;
	if (h->get_privacy() == SalPrivacyNone)
		fromAddressToSearchIfMe = linphone_address_clone(fromAddr);
	else if (pAssertedId)
		fromAddressToSearchIfMe = linphone_address_new(pAssertedId);
	else
		ms_warning("Hidden from identity, don't know if it's me");
	if (fromAddressToSearchIfMe && already_a_call_with_remote_address(lc, fromAddressToSearchIfMe)) {
		char *addr = linphone_address_as_string(fromAddr);
		ms_warning("Receiving a call while one with same address [%s] is initiated, refusing this one with busy message", addr);
		h->decline(SalReasonBusy, nullptr);
		LinphoneErrorInfo *ei = linphone_error_info_new();
		linphone_error_info_set(ei, nullptr, LinphoneReasonBusy, 486, "Busy - duplicated call", nullptr);
		linphone_core_report_early_failed_call(lc, LinphoneCallIncoming, fromAddr, toAddr, ei);
		h->release();
		linphone_address_unref(fromAddressToSearchIfMe);
		ms_free(addr);
		return;
	}
	if (fromAddressToSearchIfMe)
		linphone_address_unref(fromAddressToSearchIfMe);

	LinphoneCall *call = linphone_call_new_incoming(lc, fromAddr, toAddr, h);
	linphone_address_unref(fromAddr);
	linphone_address_unref(toAddr);
	L_GET_PRIVATE_FROM_C_OBJECT(call)->startIncomingNotification();
}

static void call_rejected(SalCallOp *h){
	LinphoneCore *lc=(LinphoneCore *)h->get_sal()->get_user_pointer();
	LinphoneErrorInfo *ei = linphone_error_info_new();
	linphone_error_info_from_sal_op(ei, h);
	linphone_core_report_early_failed_call(lc, LinphoneCallIncoming, linphone_address_new(h->get_from()), linphone_address_new(h->get_to()), ei);
}

#if 0
static void start_remote_ring(LinphoneCore *lc, LinphoneCall *call) {
	if (lc->sound_conf.play_sndcard!=NULL){
		MSSndCard *ringcard=lc->sound_conf.lsd_card ? lc->sound_conf.lsd_card : lc->sound_conf.play_sndcard;
		if (call->localdesc->streams[0].max_rate>0) ms_snd_card_set_preferred_sample_rate(ringcard, call->localdesc->streams[0].max_rate);
		/*we release sound before playing ringback tone*/
		if (call->audiostream)
			audio_stream_unprepare_sound(call->audiostream);
		if( lc->sound_conf.remote_ring ){
			lc->ringstream=ring_start(lc->factory, lc->sound_conf.remote_ring,2000,ringcard);
		}
	}
}
#endif

static void call_ringing(SalOp *h) {
	LinphonePrivate::CallSession *session = reinterpret_cast<LinphonePrivate::CallSession *>(h->get_user_pointer());
	if (!session) return;
	L_GET_PRIVATE(session)->remoteRinging();
}

#if 0
static void start_pending_refer(LinphoneCall *call){
	linphone_core_start_refered_call(call->core, call,NULL);
}
#endif

/*
 * could be reach :
 *  - when the call is accepted
 *  - when a request is accepted (pause, resume)
 */
static void call_accepted(SalOp *op) {
	LinphonePrivate::CallSession *session = reinterpret_cast<LinphonePrivate::CallSession *>(op->get_user_pointer());
	if (!session) {
		ms_warning("call_accepted: CallSession no longer exists");
		return;
	}
	L_GET_PRIVATE(session)->accepted();
}

/* this callback is called when an incoming re-INVITE/ SIP UPDATE modifies the session*/
static void call_updating(SalOp *op, bool_t is_update) {
	LinphonePrivate::CallSession *session = reinterpret_cast<LinphonePrivate::CallSession *>(op->get_user_pointer());
	if (!session) {
		ms_warning("call_updating: CallSession no longer exists");
		return;
	}
	L_GET_PRIVATE(session)->updating(!!is_update);
}


static void call_ack_received(SalOp *op, SalCustomHeader *ack) {
	LinphonePrivate::CallSession *session = reinterpret_cast<LinphonePrivate::CallSession *>(op->get_user_pointer());
	if (!session) {
		ms_warning("call_ack_received(): no CallSession for which an ack is expected");
		return;
	}
	L_GET_PRIVATE(session)->ackReceived(reinterpret_cast<LinphoneHeaders *>(ack));
}


static void call_ack_being_sent(SalOp *op, SalCustomHeader *ack) {
	LinphonePrivate::CallSession *session = reinterpret_cast<LinphonePrivate::CallSession *>(op->get_user_pointer());
	if (!session) {
		ms_warning("call_ack_being_sent(): no CallSession for which an ack is supposed to be sent");
		return;
	}
	L_GET_PRIVATE(session)->ackBeingSent(reinterpret_cast<LinphoneHeaders *>(ack));
}

static void call_terminated(SalOp *op, const char *from) {
	LinphonePrivate::CallSession *session = reinterpret_cast<LinphonePrivate::CallSession *>(op->get_user_pointer());
	if (!session)
		return;
	L_GET_PRIVATE(session)->terminated();
}

#if 0
static int resume_call_after_failed_transfer(LinphoneCall *call){
	if (call->was_automatically_paused && call->state==LinphoneCallPausing)
		return BELLE_SIP_CONTINUE; /*was still in pausing state*/

	if (call->was_automatically_paused && call->state==LinphoneCallPaused){
		if (sal_op_is_idle(call->op)){
			linphone_call_resume(call);
		}else {
			ms_message("resume_call_after_failed_transfer(), salop was busy");
			return BELLE_SIP_CONTINUE;
		}
	}
	linphone_call_unref(call);
	return BELLE_SIP_STOP;
}
#endif

static void call_failure(SalOp *op) {
	LinphonePrivate::CallSession *session = reinterpret_cast<LinphonePrivate::CallSession *>(op->get_user_pointer());
	if (!session) {
		ms_warning("Failure reported on already terminated CallSession");
		return;
	}
	L_GET_PRIVATE(session)->failure();
}

static void call_released(SalOp *op) {
	LinphonePrivate::CallSession *session = reinterpret_cast<LinphonePrivate::CallSession *>(op->get_user_pointer());
	if (!session) {
		/* We can get here when the core manages call at Sal level without creating a Call object. Typicially,
		 * when declining an incoming call with busy because maximum number of calls is reached. */
		return;
	}
	L_GET_PRIVATE(session)->setState(LinphoneCallReleased, "Call released");
}

static void call_cancel_done(SalOp *op) {
#if 0
	LinphoneCall *call = (LinphoneCall *)sal_op_get_user_pointer(op);
	if (call->reinvite_on_cancel_response_requested == TRUE) {
		call->reinvite_on_cancel_response_requested = FALSE;
		linphone_call_reinvite_to_recover_from_connection_loss(call);
	}
#endif
}

static void auth_failure(SalOp *op, SalAuthInfo* info) {
	LinphoneCore *lc = reinterpret_cast<LinphoneCore *>(op->get_sal()->get_user_pointer());
	LinphoneAuthInfo *ai = NULL;

	if (info != NULL) {
		ai = (LinphoneAuthInfo*)_linphone_core_find_auth_info(lc, info->realm, info->username, info->domain, TRUE);
		if (ai){
			LinphoneAuthMethod method = info->mode == SalAuthModeHttpDigest ? LinphoneAuthHttpDigest : LinphoneAuthTls;
			LinphoneAuthInfo *auth_info = linphone_core_create_auth_info(lc, info->username, NULL, NULL, NULL, info->realm, info->domain);
			ms_message("%s/%s/%s/%s authentication fails.", info->realm, info->username, info->domain, info->mode == SalAuthModeHttpDigest ? "HttpDigest" : "Tls");
			/*ask again for password if auth info was already supplied but apparently not working*/
			linphone_core_notify_authentication_requested(lc, auth_info, method);
			linphone_auth_info_unref(auth_info);
			// Deprecated
			linphone_core_notify_auth_info_requested(lc, info->realm, info->username, info->domain);
		}
	}
}

static void register_success(SalOp *op, bool_t registered){
	LinphoneProxyConfig *cfg=(LinphoneProxyConfig *)op->get_user_pointer();
	if (!cfg){
		ms_message("Registration success for deleted proxy config, ignored");
		return;
	}
	linphone_proxy_config_set_state(cfg, registered ? LinphoneRegistrationOk : LinphoneRegistrationCleared ,
									registered ? "Registration successful" : "Unregistration done");
}

static void register_failure(SalOp *op){
	LinphoneProxyConfig *cfg=(LinphoneProxyConfig*)op->get_user_pointer();
	const SalErrorInfo *ei=op->get_error_info();
	const char *details=ei->full_string;

	if (cfg==NULL){
		ms_warning("Registration failed for unknown proxy config.");
		return ;
	}
	if (details==NULL)
		details="no response timeout";

	if ((ei->reason == SalReasonServiceUnavailable || ei->reason == SalReasonIOError)
			&& linphone_proxy_config_get_state(cfg) == LinphoneRegistrationOk) {
		linphone_proxy_config_set_state(cfg,LinphoneRegistrationProgress,"Service unavailable, retrying");
	} else {
		linphone_proxy_config_set_state(cfg,LinphoneRegistrationFailed,details);
	}
	if (cfg->presence_publish_event){
		/*prevent publish to be sent now until registration gets successful*/
		linphone_event_terminate(cfg->presence_publish_event);
		cfg->presence_publish_event=NULL;
		cfg->send_publish=cfg->publish;
	}
}

static void vfu_request(SalOp *op) {
	LinphonePrivate::CallSession *session = reinterpret_cast<LinphonePrivate::CallSession *>(op->get_user_pointer());
	if (!session)
		return;
	LinphonePrivate::MediaSession *mediaSession = dynamic_cast<LinphonePrivate::MediaSession *>(session);
	if (!mediaSession) {
		ms_warning("VFU request but no MediaSession!");
		return;
	}
	L_GET_PRIVATE(mediaSession)->sendVfu();
}

static void dtmf_received(SalOp *op, char dtmf){
#if 0
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);
	if (!call) return;
	linphone_call_notify_dtmf_received(call, dtmf);
#endif
}

static void call_refer_received(SalOp *op, const SalAddress *referto){
#if 0
	LinphoneCore *lc=(LinphoneCore *)sal_get_user_pointer(sal);
	LinphoneCall *call=(LinphoneCall*)sal_op_get_user_pointer(op);
	LinphoneAddress *refer_to_addr = linphone_address_new(referto);
	char method[20] = "";

	if(refer_to_addr) {
		const char *tmp = linphone_address_get_method_param(refer_to_addr);
		if(tmp) strncpy(method, tmp, sizeof(method));
		linphone_address_unref(refer_to_addr);
	}
	if (call && (strlen(method) == 0 || strcmp(method, "INVITE") == 0)) {
		if (call->refer_to!=NULL){
			ms_free(call->refer_to);
		}
		call->refer_to=ms_strdup(referto);
		call->refer_pending=TRUE;
		linphone_call_set_state(call,LinphoneCallRefered,"Refered");
		if (call->refer_pending) linphone_core_start_refered_call(lc,call,NULL);
	}else {
		linphone_core_notify_refer_received(lc,referto);
	}
#endif
}

static void message_received(SalOp *op, const SalMessage *msg){
	LinphoneCore *lc=(LinphoneCore *)op->get_sal()->get_user_pointer();
	LinphoneCall *call=(LinphoneCall*)op->get_user_pointer();
	LinphoneReason reason = lc->chat_deny_code;
	if (reason == LinphoneReasonNone) {
		linphone_core_message_received(lc, op, msg);
	}
	auto messageOp = dynamic_cast<SalMessageOpInterface *>(op);
	messageOp->reply(linphone_reason_to_sal(reason));
	if (!call) op->release();
}

static void parse_presence_requested(SalOp *op, const char *content_type, const char *content_subtype, const char *body, SalPresenceModel **result) {
	linphone_notify_parse_presence(content_type, content_subtype, body, result);
}

static void convert_presence_to_xml_requested(SalOp *op, SalPresenceModel *presence, const char *contact, char **content) {
	/*for backward compatibility because still used by notify. No loguer used for publish*/

	if(linphone_presence_model_get_presentity((LinphonePresenceModel*)presence) == NULL) {
		LinphoneAddress * presentity = linphone_address_new(contact);
		linphone_presence_model_set_presentity((LinphonePresenceModel*)presence, presentity);
		linphone_address_unref(presentity);
	}
	*content = linphone_presence_model_to_xml((LinphonePresenceModel*)presence);
}

static void notify_presence(SalOp *op, SalSubscribeStatus ss, SalPresenceModel *model, const char *msg){
	LinphoneCore *lc=(LinphoneCore *)op->get_sal()->get_user_pointer();
	linphone_notify_recv(lc,op,ss,model);
}

static void subscribe_presence_received(SalPresenceOp *op, const char *from){
	LinphoneCore *lc=(LinphoneCore *)op->get_sal()->get_user_pointer();
	linphone_subscription_new(lc,op,from);
}

static void subscribe_presence_closed(SalPresenceOp *op, const char *from){
	LinphoneCore *lc=(LinphoneCore *)op->get_sal()->get_user_pointer();
	linphone_subscription_closed(lc,op);
}

static void ping_reply(SalOp *op) {
	LinphonePrivate::CallSession *session = reinterpret_cast<LinphonePrivate::CallSession *>(op->get_user_pointer());
	if (!session) {
		ms_warning("Ping reply without CallSession attached...");
		return;
	}
	L_GET_PRIVATE(session)->pingReply();
}

static bool_t fill_auth_info_with_client_certificate(LinphoneCore *lc, SalAuthInfo* sai) {
	const char *chain_file = linphone_core_get_tls_cert_path(lc);
	const char *key_file = linphone_core_get_tls_key_path(lc);

	if (key_file && chain_file) {
#ifndef _WIN32
		// optinal check for files
		struct stat st;
		if (stat(key_file, &st)) {
			ms_warning("No client certificate key found in %s", key_file);
			return FALSE;
		}
		if (stat(chain_file, &st)) {
			ms_warning("No client certificate chain found in %s", chain_file);
			return FALSE;
		}
#endif
		sal_certificates_chain_parse_file(sai, chain_file, SAL_CERTIFICATE_RAW_FORMAT_PEM);
		sal_signing_key_parse_file(sai, key_file, "");
	} else if (lc->tls_cert && lc->tls_key) {
		sal_certificates_chain_parse(sai, lc->tls_cert, SAL_CERTIFICATE_RAW_FORMAT_PEM);
		sal_signing_key_parse(sai, lc->tls_key, "");
	}
	return sai->certificates && sai->key;
}

static bool_t fill_auth_info(LinphoneCore *lc, SalAuthInfo* sai) {
	LinphoneAuthInfo *ai = NULL;
	if (sai->mode == SalAuthModeTls) {
		ai = (LinphoneAuthInfo*)_linphone_core_find_tls_auth_info(lc);
	} else {
		ai = (LinphoneAuthInfo*)_linphone_core_find_auth_info(lc,sai->realm,sai->username,sai->domain, FALSE);
	}
	if (ai) {
		if (sai->mode == SalAuthModeHttpDigest) {
			sai->userid = ms_strdup(ai->userid ? ai->userid : ai->username);
			sai->password = ai->passwd?ms_strdup(ai->passwd) : NULL;
			sai->ha1 = ai->ha1 ? ms_strdup(ai->ha1) : NULL;
		} else if (sai->mode == SalAuthModeTls) {
			if (ai->tls_cert && ai->tls_key) {
				sal_certificates_chain_parse(sai, ai->tls_cert, SAL_CERTIFICATE_RAW_FORMAT_PEM);
				sal_signing_key_parse(sai, ai->tls_key, "");
			} else if (ai->tls_cert_path && ai->tls_key_path) {
				sal_certificates_chain_parse_file(sai, ai->tls_cert_path, SAL_CERTIFICATE_RAW_FORMAT_PEM);
				sal_signing_key_parse_file(sai, ai->tls_key_path, "");
			} else {
				fill_auth_info_with_client_certificate(lc, sai);
			}
		}

		if (sai->realm && !ai->realm){
			/*if realm was not known, then set it so that ha1 may eventually be calculated and clear text password dropped*/
			linphone_auth_info_set_realm(ai, sai->realm);
			linphone_core_write_auth_info(lc, ai);
		}
		return TRUE;
	} else {
		if (sai->mode == SalAuthModeTls) {
			return fill_auth_info_with_client_certificate(lc, sai);
		}
		return FALSE;
	}
}
static bool_t auth_requested(Sal* sal, SalAuthInfo* sai) {
	LinphoneCore *lc = (LinphoneCore *)sal->get_user_pointer();
	if (fill_auth_info(lc,sai)) {
		return TRUE;
	} else {
		LinphoneAuthMethod method = sai->mode == SalAuthModeHttpDigest ? LinphoneAuthHttpDigest : LinphoneAuthTls;
		LinphoneAuthInfo *ai = linphone_core_create_auth_info(lc, sai->username, NULL, NULL, NULL, sai->realm, sai->domain);
		linphone_core_notify_authentication_requested(lc, ai, method);
		linphone_auth_info_unref(ai);
		// Deprecated
		linphone_core_notify_auth_info_requested(lc, sai->realm, sai->username, sai->domain);
		if (fill_auth_info(lc, sai)) {
			return TRUE;
		}
		return FALSE;
	}
}

static void notify_refer(SalOp *op, SalReferStatus status){
	LinphoneCall *call=(LinphoneCall*) op->get_user_pointer();
	LinphoneCallState cstate;
	if (call==NULL) {
		ms_warning("Receiving notify_refer for unknown call.");
		return ;
	}
	switch(status){
		case SalReferTrying:
			cstate=LinphoneCallOutgoingProgress;
		break;
		case SalReferSuccess:
			cstate=LinphoneCallConnected;
		break;
		case SalReferFailed:
			cstate=LinphoneCallError;
		break;
		default:
			cstate=LinphoneCallError;
	}
	linphone_call_set_transfer_state(call, cstate);
	if (cstate==LinphoneCallConnected){
		/*automatically terminate the call as the transfer is complete.*/
		linphone_call_terminate(call);
	}
}

static LinphoneChatMessageState chatStatusSal2Linphone(SalMessageDeliveryStatus status){
	switch(status){
		case SalMessageDeliveryInProgress:
			return LinphoneChatMessageStateInProgress;
		case SalMessageDeliveryDone:
			return LinphoneChatMessageStateDelivered;
		case SalMessageDeliveryFailed:
			return LinphoneChatMessageStateNotDelivered;
	}
	return LinphoneChatMessageStateIdle;
}

static void message_delivery_update(SalOp *op, SalMessageDeliveryStatus status){
	LinphoneChatMessage *chat_msg=(LinphoneChatMessage* )op->get_user_pointer();

	if (chat_msg == NULL) {
		// Do not handle delivery status for isComposing messages.
		return;
	}
	// check that the message does not belong to an already destroyed chat room - if so, do not invoke callbacks
	if (linphone_chat_message_get_chat_room(chat_msg) != NULL) {
		linphone_chat_message_update_state(chat_msg, chatStatusSal2Linphone(status));
	}
	if (status != SalMessageDeliveryInProgress) { /*only release op if not in progress*/
		linphone_chat_message_unref(chat_msg);
	}
}

static void info_received(SalOp *op, SalBodyHandler *body_handler){
	LinphoneCore *lc=(LinphoneCore *)op->get_sal()->get_user_pointer();
	linphone_core_notify_info_message(lc,op,body_handler);
}

static void subscribe_response(SalOp *op, SalSubscribeStatus status, int will_retry){
	LinphoneEvent *lev=(LinphoneEvent*)op->get_user_pointer();

	if (lev==NULL) return;

	if (status==SalSubscribeActive){
		linphone_event_set_state(lev,LinphoneSubscriptionActive);
	}else if (status==SalSubscribePending){
		linphone_event_set_state(lev,LinphoneSubscriptionPending);
	}else{
		if (will_retry){
			linphone_event_set_state(lev,LinphoneSubscriptionOutgoingProgress);
		}
		else linphone_event_set_state(lev,LinphoneSubscriptionError);
	}
}

static void notify(SalSubscribeOp *op, SalSubscribeStatus st, const char *eventname, SalBodyHandler *body_handler){
	LinphoneEvent *lev=(LinphoneEvent*)op->get_user_pointer();
	LinphoneCore *lc=(LinphoneCore *)op->get_sal()->get_user_pointer();
	bool_t out_of_dialog = (lev==NULL);
	if (out_of_dialog) {
		/*out of dialog notify */
		lev = linphone_event_new_with_out_of_dialog_op(lc,op,LinphoneSubscriptionOutgoing,eventname);
	}
	{
		LinphoneContent *ct=linphone_content_from_sal_body_handler(body_handler);
		if (ct) {
			linphone_core_notify_notify_received(lc,lev,eventname,ct);
			linphone_content_unref(ct);
		}
	}
	if (out_of_dialog){
		/*out of dialog NOTIFY do not create an implicit subscription*/
		linphone_event_set_state(lev, LinphoneSubscriptionTerminated);
	}else if (st!=SalSubscribeNone){
		linphone_event_set_state(lev,linphone_subscription_state_from_sal(st));
	}
}

static void subscribe_received(SalSubscribeOp *op, const char *eventname, const SalBodyHandler *body_handler){
	LinphoneEvent *lev=(LinphoneEvent*)op->get_user_pointer();
	LinphoneCore *lc=(LinphoneCore *)op->get_sal()->get_user_pointer();

	if (lev==NULL) {
		lev=linphone_event_new_with_op(lc,op,LinphoneSubscriptionIncoming,eventname);
		linphone_event_set_state(lev,LinphoneSubscriptionIncomingReceived);
	}else{
		/*subscribe refresh, unhandled*/
	}

}

static void incoming_subscribe_closed(SalOp *op){
	LinphoneEvent *lev=(LinphoneEvent*)op->get_user_pointer();

	linphone_event_set_state(lev,LinphoneSubscriptionTerminated);
}

static void on_publish_response(SalOp* op){
	LinphoneEvent *lev=(LinphoneEvent*)op->get_user_pointer();
	const SalErrorInfo *ei=op->get_error_info();

	if (lev==NULL) return;
	if (ei->reason==SalReasonNone){
		if (!lev->terminating)
			linphone_event_set_publish_state(lev,LinphonePublishOk);
		else
			linphone_event_set_publish_state(lev,LinphonePublishCleared);
	}else{
		if (lev->publish_state==LinphonePublishOk){
			linphone_event_set_publish_state(lev,LinphonePublishProgress);
		}else{
			linphone_event_set_publish_state(lev,LinphonePublishError);
		}
	}
}


static void on_expire(SalOp *op){
	LinphoneEvent *lev=(LinphoneEvent*)op->get_user_pointer();

	if (lev==NULL) return;

	if (linphone_event_get_publish_state(lev)==LinphonePublishOk){
		linphone_event_set_publish_state(lev,LinphonePublishExpiring);
	}else if (linphone_event_get_subscription_state(lev)==LinphoneSubscriptionActive){
		linphone_event_set_state(lev,LinphoneSubscriptionExpiring);
	}
}

static void on_notify_response(SalOp *op){
	LinphoneEvent *lev=(LinphoneEvent*)op->get_user_pointer();

	if (lev==NULL) return;
	/*this is actually handling out of dialogs notify - for the moment*/
	if (!lev->is_out_of_dialog_op) return;
	switch (linphone_event_get_subscription_state(lev)){
		case LinphoneSubscriptionIncomingReceived:
			if (op->get_error_info()->reason == SalReasonNone){
				linphone_event_set_state(lev, LinphoneSubscriptionTerminated);
			}else{
				linphone_event_set_state(lev, LinphoneSubscriptionError);
			}
		break;
		default:
			ms_warning("Unhandled on_notify_response() case %s", linphone_subscription_state_to_string(linphone_event_get_subscription_state(lev)));
	}
}

static void refer_received(SalOp *op, const SalAddress *refer_to){
	if (sal_address_has_param(refer_to, "text")) {
		LinphonePrivate::Address addr(sal_address_as_string(refer_to));
		if (addr.isValid()) {
			LinphoneCore *lc = reinterpret_cast<LinphoneCore *>(op->get_sal()->get_user_pointer());
			if (addr.hasParam("admin")) {
				LinphoneChatRoom *cr = _linphone_core_find_group_chat_room(lc, op->get_to());
				if (cr) {
					std::shared_ptr<Participant> participant = L_GET_CPP_PTR_FROM_C_OBJECT(cr)->findParticipant(addr);
					if (participant) {
						bool value = Utils::stob(addr.getParamValue("admin"));
						L_GET_CPP_PTR_FROM_C_OBJECT(cr)->setParticipantAdminStatus(participant, value);
					}
					static_cast<SalReferOp *>(op)->reply(SalReasonNone);
					return;
				}
			} else {
				LinphoneChatRoom *cr = _linphone_core_join_client_group_chat_room(lc, addr);
				if (cr) {
					static_cast<SalReferOp *>(op)->reply(SalReasonNone);
					return;
				}
			}
		}
	}
	static_cast<SalReferOp *>(op)->reply(SalReasonDeclined);
}

Sal::Callbacks linphone_sal_callbacks={
	call_received,
	call_rejected,
	call_ringing,
	call_accepted,
	call_ack_received,
	call_ack_being_sent,
	call_updating,
	call_terminated,
	call_failure,
	call_released,
	call_cancel_done,
	call_refer_received,
	auth_failure,
	register_success,
	register_failure,
	vfu_request,
	dtmf_received,
	message_received,
	message_delivery_update,
	notify_refer,
	subscribe_received,
	incoming_subscribe_closed,
	subscribe_response,
	notify,
	subscribe_presence_received,
	subscribe_presence_closed,
	parse_presence_requested,
	convert_presence_to_xml_requested,
	notify_presence,
	ping_reply,
	auth_requested,
	info_received,
	on_publish_response,
	on_expire,
	on_notify_response,
	refer_received,
};
