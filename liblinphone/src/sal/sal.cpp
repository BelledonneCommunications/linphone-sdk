/*
 * sal.cpp
 * Copyright (C) 2010-2018 Belledonne Communications SARL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "sal/sal.h"
#include "sal/call-op.h"
#include "sal/presence-op.h"
#include "sal/refer-op.h"
#include "sal/event-op.h"
#include "sal/message-op.h"
#include "bellesip_sal/sal_impl.h"
#include "tester_utils.h"
#include "private.h"

using namespace std;

LINPHONE_BEGIN_NAMESPACE

void Sal::processDialogTerminatedCb(void *sal, const belle_sip_dialog_terminated_event_t *event) {
	belle_sip_dialog_t* dialog =  belle_sip_dialog_terminated_event_get_dialog(event);
	SalOp* op = reinterpret_cast<SalOp *>(belle_sip_dialog_get_application_data(dialog));
	if (op && op->mCallbacks && op->mCallbacks->process_dialog_terminated) {
		op->mCallbacks->process_dialog_terminated(op,event);
	} else {
		ms_error("sal process_dialog_terminated no op found for this dialog [%p], ignoring",dialog);
	}
}

void Sal::processIoErrorCb(void *user_ctx, const belle_sip_io_error_event_t *event){
	belle_sip_client_transaction_t*client_transaction;
	SalOp* op;
	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(belle_sip_io_error_event_get_source(event),belle_sip_client_transaction_t)) {
		client_transaction=BELLE_SIP_CLIENT_TRANSACTION(belle_sip_io_error_event_get_source(event));
		op = (SalOp*)belle_sip_transaction_get_application_data(BELLE_SIP_TRANSACTION(client_transaction));
		/*also reset auth count on IO error*/
		op->mAuthRequests=0;
		if (op->mCallbacks && op->mCallbacks->process_io_error) {
			op->mCallbacks->process_io_error(op,event);
		}
	} else {
		/*ms_error("sal process_io_error not implemented yet for non transaction");*/
		/*nop, because already handle at transaction layer*/
	}
}

void Sal::processRequestEventCb (void *ud, const belle_sip_request_event_t *event) {
	Sal *sal = (Sal *)ud;
	SalOp *op = NULL;
	belle_sip_request_t *req = belle_sip_request_event_get_request(event);
	belle_sip_dialog_t *dialog = belle_sip_request_event_get_dialog(event);
	belle_sip_header_address_t *origin_address;
	belle_sip_header_address_t *address=NULL;
	belle_sip_header_from_t *from_header;
	belle_sip_header_to_t *to;
	belle_sip_header_diversion_t *diversion;
	belle_sip_response_t *resp;
	belle_sip_header_t *evh;
	const char *method = belle_sip_request_get_method(req);
	belle_sip_header_contact_t *remote_contact = belle_sip_message_get_header_by_type(req, belle_sip_header_contact_t);
	belle_sip_header_t *subjectHeader = belle_sip_message_get_header(BELLE_SIP_MESSAGE(req), "Subject");

	from_header = belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(req),belle_sip_header_from_t);

	if (dialog) {
		op = (SalOp *)belle_sip_dialog_get_application_data(dialog);

		if (op == NULL  && strcmp("NOTIFY",method) == 0) {
			/*special case for Dialog created by notify mathing subscribe*/
			belle_sip_transaction_t * sub_trans = belle_sip_dialog_get_last_transaction(dialog);
			op = (SalOp*)belle_sip_transaction_get_application_data(sub_trans);
		}
		if (op == NULL || op->mState == SalOp::State::Terminated){
			ms_warning("Receiving request for null or terminated op [%p], ignored",op);
			return;
		}
	} else {
		/*handle the case where we are receiving a request with to tag but it is not belonging to any dialog*/
		belle_sip_header_to_t *to = belle_sip_message_get_header_by_type(req, belle_sip_header_to_t);
		if ((strcmp("INVITE",method)==0 || strcmp("NOTIFY",method)==0) && (belle_sip_header_to_get_tag(to) != NULL)) {
			ms_warning("Receiving %s with to-tag but no know dialog here. Rejecting.", method);
			resp=belle_sip_response_create_from_request(req,481);
			belle_sip_provider_send_response(sal->mProvider,resp);
			return;
		/* by default (eg. when a to-tag is present), out of dialog ACK are automatically
		handled in lower layers (belle-sip) but in case it misses, it will be forwarded to us */
		} else if (strcmp("ACK",method)==0 && (belle_sip_header_to_get_tag(to) == NULL)) {
			ms_warning("Receiving ACK without to-tag but no know dialog here. Ignoring");
			return;
		}

		if (strcmp("INVITE",method)==0) {
			op=new SalCallOp(sal);
			op->fillCallbacks();
		}else if ((strcmp("SUBSCRIBE",method)==0 || strcmp("NOTIFY",method)==0) && (evh=belle_sip_message_get_header(BELLE_SIP_MESSAGE(req),"Event"))!=NULL) {
			if (strncmp(belle_sip_header_get_unparsed_value(evh),"presence",strlen("presence"))==0){
				op=new SalPresenceOp(sal);
			} else {
				op=new SalSubscribeOp(sal);
			}
			op->fillCallbacks();
		}else if (strcmp("MESSAGE",method)==0) {
			op=new SalMessageOp(sal);
			op->fillCallbacks();
		}else if (strcmp("REFER",method)==0) {
			op=new SalReferOp(sal);
		}else if (strcmp("OPTIONS",method)==0) {
			resp=belle_sip_response_create_from_request(req,200);
			belle_sip_provider_send_response(sal->mProvider,resp);
			return;
		}else if (strcmp("INFO",method)==0) {
			resp=belle_sip_response_create_from_request(req,481);/*INFO out of call dialogs are not allowed*/
			belle_sip_provider_send_response(sal->mProvider,resp);
			return;
		}else if (strcmp("BYE",method)==0) {
			resp=belle_sip_response_create_from_request(req,481);/*out of dialog BYE */
			belle_sip_provider_send_response(sal->mProvider,resp);
			return;
		}else if (strcmp("CANCEL",method)==0) {
			resp=belle_sip_response_create_from_request(req,481);/*out of dialog CANCEL */
			belle_sip_provider_send_response(sal->mProvider,resp);
			return;
		}else if (sal->mEnableTestFeatures && strcmp("PUBLISH",method)==0) {
			resp=belle_sip_response_create_from_request(req,200);/*out of dialog PUBLISH */
			belle_sip_message_add_header((belle_sip_message_t*)resp,belle_sip_header_create("SIP-Etag","4441929FFFZQOA"));
			belle_sip_provider_send_response(sal->mProvider,resp);
			return;
		}else {
			ms_error("sal process_request_event not implemented yet for method [%s]",belle_sip_request_get_method(req));
			resp=belle_sip_response_create_from_request(req,405);
			belle_sip_message_add_header(BELLE_SIP_MESSAGE(resp)
										,BELLE_SIP_HEADER(belle_sip_header_allow_create("INVITE, CANCEL, ACK, BYE, SUBSCRIBE, NOTIFY, MESSAGE, OPTIONS, INFO")));
			belle_sip_provider_send_response(sal->mProvider,resp);
			return;
		}
		op->mDir=SalOp::Dir::Incoming;
	}

	if (!op->mFromAddress)  {
		if (belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(from_header)))
			address=belle_sip_header_address_create(belle_sip_header_address_get_displayname(BELLE_SIP_HEADER_ADDRESS(from_header))
					,belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(from_header)));
		else if ((belle_sip_header_address_get_absolute_uri(BELLE_SIP_HEADER_ADDRESS(from_header))))
			address=belle_sip_header_address_create2(belle_sip_header_address_get_displayname(BELLE_SIP_HEADER_ADDRESS(from_header))
					,belle_sip_header_address_get_absolute_uri(BELLE_SIP_HEADER_ADDRESS(from_header)));
		else
			ms_error("Cannot not find from uri from request [%p]",req);
		op->setFromAddress((SalAddress*)address);
		belle_sip_object_unref(address);
	}

	if( remote_contact ){
		op->setRemoteContact(belle_sip_header_get_unparsed_value(BELLE_SIP_HEADER(remote_contact)));
	}

	if (!op->mToAddress) {
		to=belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(req),belle_sip_header_to_t);
		if (belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(to)))
			address=belle_sip_header_address_create(belle_sip_header_address_get_displayname(BELLE_SIP_HEADER_ADDRESS(to))
					,belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(to)));
		else if ((belle_sip_header_address_get_absolute_uri(BELLE_SIP_HEADER_ADDRESS(to))))
			address=belle_sip_header_address_create2(belle_sip_header_address_get_displayname(BELLE_SIP_HEADER_ADDRESS(to))
					,belle_sip_header_address_get_absolute_uri(BELLE_SIP_HEADER_ADDRESS(to)));
		else
			ms_error("Cannot not find to uri from request [%p]",req);

		op->setToAddress((SalAddress*)address);
		belle_sip_object_unref(address);
	}

	if (subjectHeader) {
		const char *subject = belle_sip_header_get_unparsed_value(subjectHeader);
		op->setSubject(subject);
	}

	if(!op->mDiversionAddress){
		diversion=belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(req),belle_sip_header_diversion_t);
		if (diversion) {
			if (belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(diversion)))
				address=belle_sip_header_address_create(belle_sip_header_address_get_displayname(BELLE_SIP_HEADER_ADDRESS(diversion))
						,belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(diversion)));
			else if ((belle_sip_header_address_get_absolute_uri(BELLE_SIP_HEADER_ADDRESS(diversion))))
				address=belle_sip_header_address_create2(belle_sip_header_address_get_displayname(BELLE_SIP_HEADER_ADDRESS(diversion))
						,belle_sip_header_address_get_absolute_uri(BELLE_SIP_HEADER_ADDRESS(diversion)));
			else
				ms_warning("Cannot not find diversion header from request [%p]",req);
			if (address) {
				op->setDiversionAddress((SalAddress*)address);
				belle_sip_object_unref(address);
			}
		}
	}

	if (!op->mOrigin) {
		/*set origin uri*/
		origin_address=belle_sip_header_address_create(NULL,belle_sip_request_extract_origin(req));
		op->setNetworkOriginAddress((SalAddress*)origin_address);
		belle_sip_object_unref(origin_address);
	}
	if (!op->mRemoteUserAgent) {
		op->setRemoteUserAgent(BELLE_SIP_MESSAGE(req));
	}

	if (!op->mCallId) {
		op->mCallId=ms_strdup(belle_sip_header_call_id_get_call_id(BELLE_SIP_HEADER_CALL_ID(belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(req), belle_sip_header_call_id_t))));
	}
	/*It is worth noting that proxies can (and will) remove this header field*/
	op->setPrivacyFromMessage((belle_sip_message_t*)req);
	
	if (strcmp("ACK",method) != 0){
		/*The ACK custom header is processed specifically later on*/
		op->assignRecvHeaders((belle_sip_message_t*)req);
	}
	
	if (op->mCallbacks && op->mCallbacks->process_request_event) {
		op->mCallbacks->process_request_event(op,event);
	} else {
		ms_error("sal process_request_event not implemented yet");
	}

}

void Sal::processResponseEventCb(void *user_ctx, const belle_sip_response_event_t *event) {
	belle_sip_client_transaction_t* client_transaction = belle_sip_response_event_get_client_transaction(event);
	belle_sip_response_t* response = belle_sip_response_event_get_response(event);
	int response_code = belle_sip_response_get_status_code(response);

	if (!client_transaction) {
		ms_warning("Discarding stateless response [%i]",response_code);
		return;
	} else {
		SalOp* op = (SalOp*)belle_sip_transaction_get_application_data(BELLE_SIP_TRANSACTION(client_transaction));
		belle_sip_request_t* request=belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(client_transaction));
		belle_sip_header_contact_t* remote_contact = belle_sip_message_get_header_by_type(response, belle_sip_header_contact_t);

		if (op->mState == SalOp::State::Terminated) {
			belle_sip_message("Op [%p] is terminated, nothing to do with this [%i]", op, response_code);
			return;
		}
		/*do it all the time, since we can receive provisional responses from a different instance than the final one*/
		op->setRemoteUserAgent(BELLE_SIP_MESSAGE(response));

		if(remote_contact) {
			op->setRemoteContact(belle_sip_header_get_unparsed_value(BELLE_SIP_HEADER(remote_contact)));
		}

		if (!op->mCallId) {
			op->mCallId=ms_strdup(belle_sip_header_call_id_get_call_id(BELLE_SIP_HEADER_CALL_ID(belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(response), belle_sip_header_call_id_t))));
		}

		op->assignRecvHeaders((belle_sip_message_t*)response);

		if (op->mCallbacks && op->mCallbacks->process_response_event) {
			/*handle authorization*/
			switch (response_code) {
				case 200:
					break;
				case 401:
				case 407:
					if (op->mState == SalOp::State::Terminating && strcmp("BYE",belle_sip_request_get_method(request))!=0) {
						/*only bye are completed*/
						belle_sip_message("Op is in state terminating, nothing else to do ");
					} else {
						if (op->mPendingAuthTransaction){
							belle_sip_object_unref(op->mPendingAuthTransaction);
							op->mPendingAuthTransaction=NULL;
						}
						if (++op->mAuthRequests > 2) {
							ms_warning("Auth info cannot be found for op [%s/%s] after 2 attempts, giving up",op->getFrom()
																												,op->getTo());
							op->mRoot->mCallbacks.auth_failure(op,op->mAuthInfo);
							op->mRoot->removePendingAuth(op);
						} else {
							op->mPendingAuthTransaction=(belle_sip_client_transaction_t*)belle_sip_object_ref(client_transaction);
							op->processAuthentication();
							return;
						}
					}
					break;
				case 403:
					if (op->mAuthInfo) op->mRoot->mCallbacks.auth_failure(op,op->mAuthInfo);
					break;
				case 302:
				case 301:
					if (op->processRedirect() == 0)
						return;
					break;
			}
			if (response_code >= 180 && response_code !=401 && response_code !=407 && response_code !=403) {
				/*not an auth request*/
				op->mAuthRequests=0;
			}
			op->mCallbacks->process_response_event(op,event);
		} else {
			ms_error("Unhandled event response [%p]",event);
		}
	}
}

void Sal::processTimeoutCb(void *user_ctx, const belle_sip_timeout_event_t *event) {
	belle_sip_client_transaction_t* client_transaction = belle_sip_timeout_event_get_client_transaction(event);
	SalOp* op = (SalOp*)belle_sip_transaction_get_application_data(BELLE_SIP_TRANSACTION(client_transaction));
	if (op && op->mCallbacks && op->mCallbacks->process_timeout) {
		op->mCallbacks->process_timeout(op,event);
	} else {
		ms_error("Unhandled event timeout [%p]",event);
	}
}

void Sal::processTransactionTerminatedCb(void *user_ctx, const belle_sip_transaction_terminated_event_t *event) {
	belle_sip_client_transaction_t* client_transaction = belle_sip_transaction_terminated_event_get_client_transaction(event);
	belle_sip_server_transaction_t* server_transaction = belle_sip_transaction_terminated_event_get_server_transaction(event);
	belle_sip_transaction_t* trans;
	SalOp* op;

	if(client_transaction)
		trans=BELLE_SIP_TRANSACTION(client_transaction);
	 else
		trans=BELLE_SIP_TRANSACTION(server_transaction);

	op = (SalOp*)belle_sip_transaction_get_application_data(trans);
	if (op && op->mCallbacks && op->mCallbacks->process_transaction_terminated) {
		op->mCallbacks->process_transaction_terminated(op,event);
	} else {
		ms_message("Unhandled transaction terminated [%p]",trans);
	}
	if (op) {
		op->unref(); /*because every transaction ref op*/
		belle_sip_transaction_set_application_data(trans,NULL); /*no longuer reference something we do not ref to avoid futur access of a released op*/
	}
}

void Sal::processAuthRequestedCb(void *sal, belle_sip_auth_event_t *event) {
	SalAuthInfo* auth_info = sal_auth_info_create(event);
	((Sal*)sal)->mCallbacks.auth_requested(reinterpret_cast<Sal *>(sal),auth_info);
	belle_sip_auth_event_set_passwd(event,(const char*)auth_info->password);
	belle_sip_auth_event_set_ha1(event,(const char*)auth_info->ha1);
	belle_sip_auth_event_set_userid(event,(const char*)auth_info->userid);
	belle_sip_auth_event_set_signing_key(event,(belle_sip_signing_key_t *)auth_info->key);
	belle_sip_auth_event_set_client_certificates_chain(event,(belle_sip_certificates_chain_t* )auth_info->certificates);
	sal_auth_info_delete(auth_info);
}

Sal::Sal(MSFactory *factory){
	belle_sip_listener_callbacks_t listener_callbacks = {0};

	/*belle_sip_object_enable_marshal_check(TRUE);*/
	mFactory = factory;
	/*first create the stack, which initializes the belle-sip object's pool for this thread*/
	mStack = belle_sip_stack_new(NULL);

	mUserAgent=belle_sip_header_user_agent_new();
#if defined(PACKAGE_NAME) && defined(LIBLINPHONE_VERSION)
	belle_sip_header_user_agent_add_product(user_agent, PACKAGE_NAME "/" LIBLINPHONE_VERSION);
#else
	belle_sip_header_user_agent_add_product(mUserAgent, "Unknown");
#endif
	   appendStackStringToUserAgent();
	belle_sip_object_ref(mUserAgent);

	mProvider = belle_sip_stack_create_provider(mStack,NULL);
	enableNatHelper(TRUE);

	listener_callbacks.process_dialog_terminated=processDialogTerminatedCb;
	listener_callbacks.process_io_error=processIoErrorCb;
	listener_callbacks.process_request_event=processRequestEventCb;
	listener_callbacks.process_response_event=processResponseEventCb;
	listener_callbacks.process_timeout=processTimeoutCb;
	listener_callbacks.process_transaction_terminated=processTransactionTerminatedCb;
	listener_callbacks.process_auth_requested=processAuthRequestedCb;
	mListener=belle_sip_listener_create_from_callbacks(&listener_callbacks, this);
	belle_sip_provider_add_sip_listener(mProvider, mListener);
}

Sal::~Sal() {
	belle_sip_object_unref(mUserAgent);
	belle_sip_object_unref(mProvider);
	belle_sip_object_unref(mStack);
	belle_sip_object_unref(mListener);
	if (mSupported) belle_sip_object_unref(mSupported);
	bctbx_list_free_with_data(mSupportedTags,ms_free);
	bctbx_list_free_with_data(mSupportedContentTypes, ms_free);
	if (mUuid) ms_free(mUuid);
	if (mRootCa) ms_free(mRootCa);
	if (mRootCaData) ms_free(mRootCaData);
	if (mLinphoneSpecs) ms_free(mLinphoneSpecs);
}

void Sal::setCallbacks(const Callbacks *cbs) {
	memcpy(&mCallbacks,cbs,sizeof(*cbs));
	if (mCallbacks.call_received==NULL)
		mCallbacks.call_received=(OnCallReceivedCb)unimplementedStub;
	if (mCallbacks.call_ringing==NULL)
		mCallbacks.call_ringing=(OnCallRingingCb)unimplementedStub;
	if (mCallbacks.call_accepted==NULL)
		mCallbacks.call_accepted=(OnCallAcceptedCb)unimplementedStub;
	if (mCallbacks.call_failure==NULL)
		mCallbacks.call_failure=(OnCallFailureCb)unimplementedStub;
	if (mCallbacks.call_terminated==NULL)
		mCallbacks.call_terminated=(OnCallTerminatedCb)unimplementedStub;
	if (mCallbacks.call_released==NULL)
		mCallbacks.call_released=(OnCallReleasedCb)unimplementedStub;
	if (mCallbacks.call_updating==NULL)
		mCallbacks.call_updating=(OnCallUpdatingCb)unimplementedStub;
	if (mCallbacks.auth_failure==NULL)
		mCallbacks.auth_failure=(OnAuthFailureCb)unimplementedStub;
	if (mCallbacks.register_success==NULL)
		mCallbacks.register_success=(OnRegisterSuccessCb)unimplementedStub;
	if (mCallbacks.register_failure==NULL)
		mCallbacks.register_failure=(OnRegisterFailureCb)unimplementedStub;
	if (mCallbacks.dtmf_received==NULL)
		mCallbacks.dtmf_received=(OnDtmfReceivedCb)unimplementedStub;
	if (mCallbacks.notify==NULL)
		mCallbacks.notify=(OnNotifyCb)unimplementedStub;
	if (mCallbacks.subscribe_received==NULL)
		mCallbacks.subscribe_received=(OnSubscribeReceivedCb)unimplementedStub;
	if (mCallbacks.incoming_subscribe_closed==NULL)
		mCallbacks.incoming_subscribe_closed=(OnIncomingSubscribeClosedCb)unimplementedStub;
	if (mCallbacks.parse_presence_requested==NULL)
		mCallbacks.parse_presence_requested=(OnParsePresenceRequestedCb)unimplementedStub;
	if (mCallbacks.convert_presence_to_xml_requested==NULL)
		mCallbacks.convert_presence_to_xml_requested=(OnConvertPresenceToXMLRequestedCb)unimplementedStub;
	if (mCallbacks.notify_presence==NULL)
		mCallbacks.notify_presence=(OnNotifyPresenceCb)unimplementedStub;
	if (mCallbacks.subscribe_presence_received==NULL)
		mCallbacks.subscribe_presence_received=(OnSubscribePresenceReceivedCb)unimplementedStub;
	if (mCallbacks.message_received==NULL)
		mCallbacks.message_received=(OnMessageReceivedCb)unimplementedStub;
	if (mCallbacks.ping_reply==NULL)
		mCallbacks.ping_reply=(OnPingReplyCb)unimplementedStub;
	if (mCallbacks.auth_requested==NULL)
		mCallbacks.auth_requested=(OnAuthRequestedCb)unimplementedStub;
	if (mCallbacks.info_received==NULL)
		mCallbacks.info_received=(OnInfoReceivedCb)unimplementedStub;
	if (mCallbacks.on_publish_response==NULL)
		mCallbacks.on_publish_response=(OnPublishResponseCb)unimplementedStub;
	if (mCallbacks.on_expire==NULL)
		mCallbacks.on_expire=(OnExpireCb)unimplementedStub;
}

void Sal::setTlsProperties(){
	belle_sip_listening_point_t *lp=belle_sip_provider_get_listening_point(mProvider,"TLS");
	if (lp){
		belle_sip_tls_listening_point_t *tlp=BELLE_SIP_TLS_LISTENING_POINT(lp);
		belle_tls_crypto_config_t *crypto_config = belle_tls_crypto_config_new();
		int verify_exceptions = BELLE_TLS_VERIFY_NONE;
		if (!mTlsVerify) verify_exceptions = BELLE_TLS_VERIFY_ANY_REASON;
		else if (!mTlsVerifyCn) verify_exceptions = BELLE_TLS_VERIFY_CN_MISMATCH;
		belle_tls_crypto_config_set_verify_exceptions(crypto_config, verify_exceptions);
		if (mRootCa != NULL) belle_tls_crypto_config_set_root_ca(crypto_config, mRootCa);
		if (mRootCaData != NULL) belle_tls_crypto_config_set_root_ca_data(crypto_config, mRootCaData);
		if (mSslConfig != NULL) belle_tls_crypto_config_set_ssl_config(crypto_config, mSslConfig);
		belle_sip_tls_listening_point_set_crypto_config(tlp, crypto_config);
		belle_sip_object_unref(crypto_config);
	}
}

int Sal::addListenPort(SalAddress* addr, bool_t is_tunneled) {
	int result;
	belle_sip_listening_point_t* lp;
	if (is_tunneled){
#ifdef TUNNEL_ENABLED
		if (sal_address_get_transport(addr)!=SalTransportUDP){
			ms_error("Tunneled mode is only available for UDP kind of transports.");
			return -1;
		}
		lp = belle_sip_tunnel_listening_point_new(mStack, mTunnelClient);
		if (!lp){
			ms_error("Could not create tunnel listening point.");
			return -1;
		}
#else
		ms_error("No tunnel support in library.");
		return -1;
#endif
	}else{
		lp = belle_sip_stack_create_listening_point(mStack,
									sal_address_get_domain(addr),
									sal_address_get_port(addr),
									sal_transport_to_string(sal_address_get_transport(addr)));
	}
	if (lp) {
		belle_sip_listening_point_set_keep_alive(lp,(int)mKeepAlive);
		result = belle_sip_provider_add_listening_point(mProvider,lp);
		if (sal_address_get_transport(addr)==SalTransportTLS) {
			         setTlsProperties();
		}
	} else {
		return -1;
	}
	return result;
}

int Sal::setListenPort(const char *addr, int port, SalTransport tr, bool_t is_tunneled) {
	SalAddress* sal_addr = sal_address_new(NULL);
	int result;
	sal_address_set_domain(sal_addr,addr);
	sal_address_set_port(sal_addr,port);
	sal_address_set_transport(sal_addr,tr);
	result = addListenPort(sal_addr, is_tunneled);
	sal_address_destroy(sal_addr);
	return result;
}

int Sal::getListeningPort(SalTransport tr){
	const char *tpn=sal_transport_to_string(tr);
	belle_sip_listening_point_t *lp=belle_sip_provider_get_listening_point(mProvider, tpn);
	if (lp){
		return belle_sip_listening_point_get_port(lp);
	}
	return 0;
}

int Sal::unlistenPorts(){
	const belle_sip_list_t * lps = belle_sip_provider_get_listening_points(mProvider);
	belle_sip_list_t * tmp_list = belle_sip_list_copy(lps);
	belle_sip_list_for_each2 (tmp_list,(void (*)(void*,void*))removeListeningPoint,mProvider);
	belle_sip_list_free(tmp_list);
	ms_message("sal_unlisten_ports done");
	return 0;
}

int Sal::isTransportAvailable(SalTransport t) {
	switch(t){
		case SalTransportUDP:
		case SalTransportTCP:
			return TRUE;
		case SalTransportTLS:
			return belle_sip_stack_tls_available(mStack);
		case SalTransportDTLS:
			return FALSE;
	}
	return FALSE;
}

void Sal::makeSupportedHeader(){
	bctbx_list_t *it;
	char *alltags=NULL;
	size_t buflen=64;
	size_t written=0;

	if (mSupported){
		belle_sip_object_unref(mSupported);
		mSupported=NULL;
	}
	for(it=mSupportedTags;it!=NULL;it=it->next){
		const char *tag=(const char*)it->data;
		size_t taglen=strlen(tag);
		if (alltags==NULL || (written+taglen+1>=buflen)) alltags=reinterpret_cast<char *>(ms_realloc(alltags,(buflen=buflen*2)));
		written+=(size_t)snprintf(alltags+written,buflen-written,it->next ? "%s, " : "%s",tag);
	}
	if (alltags){
		mSupported=belle_sip_header_create("Supported",alltags);
		if (mSupported){
			belle_sip_object_ref(mSupported);
		}
		ms_free(alltags);
	}
}

void Sal::setSupportedTags(const char* tags){
	mSupportedTags=bctbx_list_free_with_data(mSupportedTags,ms_free);
	if (tags){
		char *iter;
		char *buffer=ms_strdup(tags);
		char *tag;
		char *context=NULL;
		iter=buffer;
		while((tag=strtok_r(iter,", ",&context))!=NULL){
			iter=NULL;
			mSupportedTags=bctbx_list_append(mSupportedTags,ms_strdup(tag));
		}
		ms_free(buffer);
	}
	   makeSupportedHeader();
}

void Sal::addSupportedTag(const char* tag){
	bctbx_list_t *elem=bctbx_list_find_custom(mSupportedTags,(bctbx_compare_func)strcasecmp,tag);
	if (!elem){
		mSupportedTags=bctbx_list_append(mSupportedTags,ms_strdup(tag));
		      makeSupportedHeader();
	}
}

void Sal::removeSupportedTag(const char* tag){
	bctbx_list_t *elem=bctbx_list_find_custom(mSupportedTags,(bctbx_compare_func)strcasecmp,tag);
	if (elem){
		ms_free(elem->data);
		mSupportedTags=bctbx_list_erase_link(mSupportedTags,elem);
		      makeSupportedHeader();
	}
}

int Sal::resetTransports() {
	ms_message("Reseting transports");
	belle_sip_provider_clean_channels(mProvider);
	return 0;
}

ortp_socket_t Sal::getSocket() const {
	ms_warning("sal_get_socket is deprecated");
	return -1;
}

void Sal::setUserAgent(const char *user_agent) {
	belle_sip_header_user_agent_set_products(mUserAgent,NULL);
	belle_sip_header_user_agent_add_product(mUserAgent,user_agent);
}

const char* Sal::getUserAgent() const {
	static char user_agent[255];
	belle_sip_header_user_agent_get_products_as_string(mUserAgent, user_agent, 254);
	return user_agent;
}

void Sal::appendStackStringToUserAgent() {
	char stack_string[64];
	snprintf(stack_string, sizeof(stack_string) - 1, "(belle-sip/%s)", belle_sip_version_to_string());
	belle_sip_header_user_agent_add_product(mUserAgent, stack_string);
}

void Sal::setKeepAlivePeriod(unsigned int value) {
	const belle_sip_list_t* iterator;
	belle_sip_listening_point_t* lp;
	mKeepAlive=value;
	for (iterator=belle_sip_provider_get_listening_points(mProvider);iterator!=NULL;iterator=iterator->next) {
		lp=(belle_sip_listening_point_t*)iterator->data;
		if (mUseTcpTlsKeepAlive || strcasecmp(belle_sip_listening_point_get_transport(lp),"udp")==0) {
			belle_sip_listening_point_set_keep_alive(lp,(int)mKeepAlive);
		}
	}
}

int Sal::setTunnel(void *tunnelclient) {
#ifdef TUNNEL_ENABLED
	mTunnelClient=tunnelclient;
	return 0;
#else
	return -1;
#endif
}

bool_t Sal::isContentTypeSupported(const char *content_type) const {
	bctbx_list_t *item;
	for (item = mSupportedContentTypes; item != NULL; item = bctbx_list_next(item)) {
		const char *item_content_type = (const char *)bctbx_list_get_data(item);
		if (strcmp(item_content_type, content_type) == 0) return TRUE;
	}
	return FALSE;
}

void Sal::addContentTypeSupport(const char *content_type) {
	if ((content_type != NULL) && (isContentTypeSupported(content_type) == FALSE)) {
		mSupportedContentTypes = bctbx_list_append(mSupportedContentTypes, ms_strdup(content_type));
	}
}

void Sal::removeContentTypeSupport(const char *content_type) {
	if (content_type != NULL) {
		if (bctbx_list_find(mSupportedContentTypes, content_type)) {
			mSupportedContentTypes = bctbx_list_remove(mSupportedContentTypes, (char *)content_type);
		}
	}
}

void Sal::useRport(bool_t use_rports) {
	belle_sip_provider_enable_rport(mProvider,use_rports);
	ms_message("Sal use rport [%s]", use_rports ? "enabled" : "disabled");
}

void Sal::setContactLinphoneSpecs(const char *specs) {
	if (mLinphoneSpecs) {
		ms_free(mLinphoneSpecs);
		mLinphoneSpecs = NULL;
	}
	if (specs) {
		mLinphoneSpecs = ms_strdup(specs);
	}
}

void Sal::setRootCa(const char* rootCa) {
	if (mRootCa) {
		ms_free(mRootCa);
		mRootCa = NULL;
	}
	if (rootCa)
		mRootCa = ms_strdup(rootCa);
	setTlsProperties();
}

void Sal::setRootCaData(const char* data) {
	if (mRootCaData) {
		ms_free(mRootCaData);
		mRootCaData = NULL;
	}
	if (data)
		mRootCaData = ms_strdup(data);
	setTlsProperties();
}

void Sal::verifyServerCertificates(bool_t verify) {
	mTlsVerify=verify;
	setTlsProperties();
}

void Sal::verifyServerCn(bool_t verify) {
	mTlsVerifyCn = verify;
	setTlsProperties();
}

void Sal::setSslConfig(void *ssl_config) {
	mSslConfig = ssl_config;
	setTlsProperties();
}

void Sal::setUuid(const char *uuid){
	if (mUuid){
		ms_free(mUuid);
		mUuid=NULL;
	}
	if (uuid)
		mUuid=ms_strdup(uuid);
}

int Sal::createUuid(char *uuid, size_t len) {
	if (generateUuid(uuid, len) == 0) {
		      setUuid(uuid);
		return 0;
	}
	return -1;
}

int Sal::generateUuid(char *uuid, size_t len) {
	sal_uuid_t uuid_struct;
	int i;
	int written;

	if (len==0) return -1;
	/*create an UUID as described in RFC4122, 4.4 */
	belle_sip_random_bytes((unsigned char*)&uuid_struct, sizeof(sal_uuid_t));
	uuid_struct.clock_seq_hi_and_reserved&=(unsigned char)~(1<<6);
	uuid_struct.clock_seq_hi_and_reserved|=(unsigned char)1<<7;
	uuid_struct.time_hi_and_version&=(unsigned char)~(0xf<<12);
	uuid_struct.time_hi_and_version|=(unsigned char)4<<12;

	written=snprintf(uuid,len,"%8.8x-%4.4x-%4.4x-%2.2x%2.2x-", uuid_struct.time_low, uuid_struct.time_mid,
			uuid_struct.time_hi_and_version, uuid_struct.clock_seq_hi_and_reserved,
			uuid_struct.clock_seq_low);
	if ((written < 0) || ((size_t)written > (len +13))) {
		ms_error("sal_create_uuid(): buffer is too short !");
		return -1;
	}
	for (i = 0; i < 6; i++)
		written+=snprintf(uuid+written,len-(unsigned long)written,"%2.2x", uuid_struct.node[i]);
	uuid[len-1]='\0';
	return 0;
}

void Sal::addPendingAuth(SalOp *op){
	if (bctbx_list_find(mPendingAuths,op)==NULL){
		mPendingAuths=bctbx_list_append(mPendingAuths,op);
		op->mHasAuthPending=TRUE;
	}
}

void Sal::removePendingAuth(SalOp *op){
	if (op->mHasAuthPending){
		op->mHasAuthPending=FALSE;
		if (bctbx_list_find(mPendingAuths,op)){
			mPendingAuths=bctbx_list_remove(mPendingAuths,op);
		}
	}
}

void Sal::setDefaultSdpHandling(SalOpSDPHandling sdp_handling_method)  {
	if (sdp_handling_method != SalOpSDPNormal ) ms_message("Enabling special SDP handling for all new SalOp in Sal[%p]!", this);
	mDefaultSdpHandling = sdp_handling_method;
}

void Sal::enableNatHelper(bool_t enable) {
	mNatHelperEnabled=enable;
	belle_sip_provider_enable_nat_helper(mProvider,enable);
	ms_message("Sal nat helper [%s]",enable?"enabled":"disabled");
}

void Sal::getDefaultLocalIp(int address_family, char *ip, size_t iplen) {
	strncpy(ip,address_family==AF_INET6 ? "::1" : "127.0.0.1",iplen);
	ms_error("sal_get_default_local_ip() is deprecated.");
}

void Sal::setDnsServers(const bctbx_list_t *servers){
	belle_sip_list_t *l = NULL;

	/*we have to convert the bctbx_list_t into a belle_sip_list_t first*/
	for (; servers != NULL; servers = servers->next){
		l = belle_sip_list_append(l, servers->data);
	}
	belle_sip_stack_set_dns_servers(mStack, l);
	belle_sip_list_free(l);
}

belle_sip_source_t *Sal::createTimer(belle_sip_source_func_t func, void *data, unsigned int timeout_value_ms, const char* timer_name) {
	belle_sip_main_loop_t *ml = belle_sip_stack_get_main_loop(mStack);
	return belle_sip_main_loop_create_timeout(ml, func, data, timeout_value_ms, timer_name);
}

void Sal::cancelTimer(belle_sip_source_t *timer) {
	belle_sip_main_loop_t *ml = belle_sip_stack_get_main_loop(mStack);
	belle_sip_main_loop_remove_source(ml, timer);
}

belle_sip_response_t* Sal::createResponseFromRequest (belle_sip_request_t* req, int code ) {
	belle_sip_response_t *resp=belle_sip_response_create_from_request(req,code);
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(resp),BELLE_SIP_HEADER(mUserAgent));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(resp), mSupported);
	return resp;
}




/***********************************/
/* Global functions implementation */
/***********************************/

int to_sip_code(SalReason r) {
	int ret=500;
	switch(r){
		case SalReasonNone:
			ret=200;
			break;
		case SalReasonIOError:
			ret=503;
			break;
		case SalReasonUnknown:
			ret=400;
			break;
		case SalReasonBusy:
			ret=486;
			break;
		case SalReasonDeclined:
			ret=603;
			break;
		case SalReasonDoNotDisturb:
			ret=600;
			break;
		case SalReasonForbidden:
			ret=403;
			break;
		case SalReasonUnsupportedContent:
			ret=415;
			break;
		case SalReasonNotFound:
			ret=404;
			break;
		case SalReasonRedirect:
			ret=302;
			break;
		case SalReasonTemporarilyUnavailable:
			ret=480;
			break;
		case SalReasonServiceUnavailable:
			ret=503;
			break;
		case SalReasonRequestPending:
			ret=491;
			break;
		case SalReasonUnauthorized:
			ret=401;
			break;
		case SalReasonNotAcceptable:
			ret=488; /*or maybe 606 Not Acceptable ?*/
			break;
		case SalReasonNoMatch:
			ret=481;
			break;
		case SalReasonRequestTimeout:
			ret=408;
			break;
		case SalReasonMovedPermanently:
			ret=301;
			break;
		case SalReasonGone:
			ret=410;
			break;
		case SalReasonAddressIncomplete:
			ret=484;
			break;
		case SalReasonNotImplemented:
			ret=501;
			break;
		case SalReasonServerTimeout:
			ret=504;
			break;
		case SalReasonBadGateway:
			ret=502;
			break;
		case SalReasonInternalError:
			ret=500;
			break;
	}
	return ret;
}

LINPHONE_END_NAMESPACE

/*******************************/
/* C++ to C wrapping functions */
/*******************************/

using namespace LinphonePrivate;

extern "C" {

Sal *linphone_core_get_sal(const LinphoneCore *lc) {
	return lc->sal;
}

SalOp *linphone_proxy_config_get_sal_op(const LinphoneProxyConfig *cfg) {
	return cfg->op;
}

SalOp *linphone_call_get_op_as_sal_op(const LinphoneCall *call) {
	return linphone_call_get_op(call);
}

Sal *sal_init(MSFactory *factory) {
	return new Sal(factory);
}

void sal_uninit(Sal* sal) {
	delete sal;
}

int sal_create_uuid(Sal *ctx, char *uuid, size_t len) {
	return ctx->createUuid(uuid, len);
}

void sal_set_uuid(Sal *ctx, const char *uuid) {
	ctx->setUuid(uuid);
}

void sal_default_set_sdp_handling(Sal* h, SalOpSDPHandling handling_method) {
	h->setDefaultSdpHandling(handling_method);
}

void sal_set_send_error(Sal *sal,int value) {
	sal->setSendError(value);
}

void sal_set_recv_error(Sal *sal,int value) {
	sal->setRecvError(value);
}

int sal_enable_pending_trans_checking(Sal *sal, bool_t value) {
	return sal->enablePendingTransactionChecking(value);
}

void sal_enable_unconditional_answer(Sal *sal,int value) {
	sal->enableUnconditionalAnswer(value);
}

void sal_set_dns_timeout(Sal* sal,int timeout) {
	sal->setDnsTimeout(timeout);
}

void sal_set_dns_user_hosts_file(Sal *sal, const char *hosts_file) {
	sal->setDnsUserHostsFile(hosts_file);
}

void *sal_get_stack_impl(Sal *sal) {
	return sal->getStackImpl();
}

void sal_set_refresher_retry_after(Sal *sal,int value) {
	sal->setRefresherRetryAfter(value);
}

int sal_get_refresher_retry_after(const Sal *sal) {
	return sal->getRefresherRetryAfter();
}

void sal_set_transport_timeout(Sal* sal,int timeout) {
	sal->setTransportTimeout(timeout);
}

void sal_enable_test_features(Sal*ctx, bool_t enabled) {
	ctx->enableTestFeatures(enabled);
}

int sal_transport_available(Sal *ctx, SalTransport t) {
	return ctx->isTransportAvailable(t);
}

const SalErrorInfo *sal_op_get_error_info(const SalOp *op) {
	return op->getErrorInfo();
}

bool_t sal_call_dialog_request_pending(const SalOp *op) {
	auto callOp = dynamic_cast<const SalCallOp *>(op);
	return callOp->dialogRequestPending();
}

void sal_call_set_sdp_handling(SalOp *h, SalOpSDPHandling handling) {
	auto callOp = dynamic_cast<SalCallOp *>(h);
	callOp->setSdpHandling(handling);
}

SalMediaDescription * sal_call_get_final_media_description(SalOp *h) {
	auto callOp = dynamic_cast<SalCallOp *>(h);
	return callOp->getFinalMediaDescription();
}

belle_sip_resolver_context_t *sal_resolve_a(Sal *sal, const char *name, int port, int family, belle_sip_resolver_callback_t cb, void *data) {
	return sal->resolveA(name, port, family, cb, data);
}

Sal *sal_op_get_sal(SalOp *op) {
	return op->getSal();
}

SalOp *sal_create_refer_op(Sal *sal) {
	return new SalReferOp(sal);
}

void sal_release_op(SalOp *op) {
	op->release();
}

void sal_op_set_from(SalOp *sal_refer_op, const char* from) {
	auto referOp = dynamic_cast<SalReferOp *>(sal_refer_op);
	referOp->setFrom(from);
}

void sal_op_set_to(SalOp *sal_refer_op, const char* to) {
	auto referOp = dynamic_cast<SalReferOp *>(sal_refer_op);
	referOp->setTo(to);
}

void sal_op_send_refer(SalOp *sal_refer_op, SalAddress* refer_to) {
	auto referOp = dynamic_cast<SalReferOp *>(sal_refer_op);
	referOp->sendRefer(refer_to);
}

void sal_set_user_pointer(Sal *sal, void *user_pointer) {
	sal->setUserPointer(user_pointer);
}

void *sal_get_user_pointer(Sal *sal) {
	return sal->getUserPointer();
}

void sal_set_call_refer_callback(Sal *sal, void (*OnReferCb)(SalOp *op, const SalAddress *referto)) {
	struct Sal::Callbacks cbs = {NULL};
	cbs.refer_received = OnReferCb;
	sal->setCallbacks(&cbs);
}

}
