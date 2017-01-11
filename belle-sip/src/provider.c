/*
	belle-sip - SIP (RFC3261) library.
	Copyright (C) 2010  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belle_sip_internal.h"
#include "listeningpoint_internal.h"
#include "md5.h"
#include "belle-sip/message.h"

typedef struct authorization_context {
	belle_sip_header_call_id_t* callid;
	const char* scheme;
	const char* realm;
	const char* nonce;
	const char* qop;
	const char* opaque;
	const char* user_id;
	const char* algorithm;
	int nonce_count;
	int is_proxy;
}authorization_context_t;

GET_SET_STRING(authorization_context,realm)
GET_SET_STRING(authorization_context,nonce)
GET_SET_STRING(authorization_context,qop)
GET_SET_STRING(authorization_context,scheme)
GET_SET_STRING(authorization_context,opaque)
GET_SET_STRING(authorization_context,user_id)
GET_SET_STRING(authorization_context,algorithm)
GET_SET_INT(authorization_context,nonce_count,int)
static authorization_context_t* belle_sip_authorization_create(belle_sip_header_call_id_t* call_id) {
	authorization_context_t* result = malloc(sizeof(authorization_context_t));
	memset(result,0,sizeof(authorization_context_t));
	result->callid=call_id;
	belle_sip_object_ref(result->callid);
	return result;
}
static void belle_sip_authorization_destroy(authorization_context_t* object) {
	DESTROY_STRING(object,scheme);
	DESTROY_STRING(object,realm);
	DESTROY_STRING(object,nonce);
	DESTROY_STRING(object,qop);
	DESTROY_STRING(object,opaque);
	DESTROY_STRING(object,user_id);
	DESTROY_STRING(object,algorithm);
	belle_sip_object_unref(object->callid);
	belle_sip_free(object);
}

static void finalize_transaction(belle_sip_transaction_t *tr){
	belle_sip_transaction_state_t state=belle_sip_transaction_get_state(tr);
	if (state!=BELLE_SIP_TRANSACTION_TERMINATED){
		belle_sip_message("Transaction [%p] still in state [%s], will force termination.",tr,belle_sip_transaction_state_to_string(state));
		belle_sip_transaction_terminate(tr);
	}
}

static void finalize_transactions(const belle_sip_list_t *l){
	belle_sip_list_t *copy=belle_sip_list_copy(l);
	belle_sip_list_free_with_data(copy,(void (*)(void*))finalize_transaction);
}

static void belle_sip_provider_uninit(belle_sip_provider_t *p){
	finalize_transactions(p->client_transactions);
	p->client_transactions=NULL;
	finalize_transactions(p->server_transactions);
	p->server_transactions=NULL;
	p->listeners=belle_sip_list_free(p->listeners);
	p->internal_listeners=belle_sip_list_free(p->internal_listeners);
	p->auth_contexts=belle_sip_list_free_with_data(p->auth_contexts,(void(*)(void*))belle_sip_authorization_destroy);
	p->dialogs=belle_sip_list_free_with_data(p->dialogs,belle_sip_object_unref);
	p->lps=belle_sip_list_free_with_data(p->lps,belle_sip_object_unref);
}

static void channel_state_changed(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_channel_state_t state){
	belle_sip_io_error_event_t ev;
	belle_sip_provider_t* prov=BELLE_SIP_PROVIDER(obj);
	if (state == BELLE_SIP_CHANNEL_ERROR || state == BELLE_SIP_CHANNEL_DISCONNECTED) {
		ev.transport=belle_sip_channel_get_transport_name(chan);
		ev.port=chan->peer_port;
		ev.host=chan->peer_name;
		ev.source=BELLE_SIP_OBJECT(prov);
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(prov->listeners,process_io_error,&ev);
		/*IO error is also relevant for internal listener like refreshers*/
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(prov->internal_listeners,process_io_error,&ev);
		if (!chan->force_close) belle_sip_provider_release_channel(prov,chan);
	}
}

static int notify_client_transaction_match(const void *transaction, const void *notify){
	belle_sip_client_transaction_t *tr=(belle_sip_client_transaction_t*)transaction;
	belle_sip_request_t *notify_req=(belle_sip_request_t*)notify;
	return !belle_sip_client_transaction_is_notify_matching_pending_subscribe(tr,notify_req);
}

belle_sip_client_transaction_t * belle_sip_provider_find_matching_pending_subscribe_client_transaction_from_notify_req(belle_sip_provider_t *prov, belle_sip_request_t *req) {
	belle_sip_list_t* elem;
	if (strcmp("NOTIFY",belle_sip_request_get_method(req)) != 0) {
		belle_sip_error("belle_sip_provider_find_matching_pending_subscribe_client_transaction_from_notify_req requires a NOTIFY request, not a [%s], on prov [%p]"
						,belle_sip_request_get_method(req)
						,prov);
	}
	elem=belle_sip_list_find_custom(prov->client_transactions,notify_client_transaction_match,req);
	return elem?BELLE_SIP_CLIENT_TRANSACTION(elem->data):NULL;
}

static void belle_sip_provider_dispatch_request(belle_sip_provider_t* prov, belle_sip_request_t *req){
	belle_sip_server_transaction_t *t;
	belle_sip_request_event_t ev;
	t=belle_sip_provider_find_matching_server_transaction(prov,req);
	if (t){
		belle_sip_object_ref(t);
		belle_sip_server_transaction_on_request(t,req);
		belle_sip_object_unref(t);
	}else{
		const char *method=belle_sip_request_get_method(req);
		ev.dialog=NULL;
		/* Should we limit to ACK ?  */
		/*Search for a dialog if exist */

		if (strcmp("CANCEL",method) == 0) {
			/* Call leg does not exist */
			belle_sip_server_transaction_t *tr = belle_sip_provider_create_server_transaction(prov, req);
			belle_sip_server_transaction_send_response(tr, belle_sip_response_create_from_request(req, 481));
			return;
		}

		ev.dialog=belle_sip_provider_find_dialog_from_message(prov,(belle_sip_message_t*)req,1/*request=uas*/);
		if (ev.dialog){
			if (strcmp("ACK",method)==0){
				if (belle_sip_dialog_handle_ack(ev.dialog,req)==-1){
					/*absorbed ACK retransmission, ignore */
					return;
				}
			}else if ((strcmp("INVITE",method)==0)&&(ev.dialog->needs_ack)){
				belle_sip_dialog_stop_200Ok_retrans(ev.dialog);
			}else if (!belle_sip_dialog_is_authorized_transaction(ev.dialog,method)){
				belle_sip_server_transaction_t *tr=belle_sip_provider_create_server_transaction(prov,req);
				belle_sip_server_transaction_send_response(tr,
					belle_sip_response_create_from_request(req,491));
				return;
			}
		} else if (strcmp("NOTIFY",method) == 0) {
			/*search for matching subscribe*/
			belle_sip_client_transaction_t *sub = belle_sip_provider_find_matching_pending_subscribe_client_transaction_from_notify_req(prov,req);
			if (sub) {
				belle_sip_message("Found matching subscribe for NOTIFY [%p], creating dialog",req);
				ev.dialog=belle_sip_provider_create_dialog_internal(prov,BELLE_SIP_TRANSACTION(sub),FALSE);
			}
		}
	
		if (prov->unconditional_answer_enabled && strcmp("ACK",method)!=0) { /*always answer predefined value (I.E 480 by default)*/
			belle_sip_server_transaction_t *tr=belle_sip_provider_create_server_transaction(prov,req);
			belle_sip_server_transaction_send_response(tr,belle_sip_response_create_from_request(req,prov->unconditional_answer));
			return;
		} else {
			ev.source=(belle_sip_object_t*)prov;
			ev.server_transaction=NULL;
			ev.request=req;
			BELLE_SIP_PROVIDER_INVOKE_LISTENERS(prov->listeners,process_request_event,&ev);
		}
	}
}

static belle_sip_list_t*  belle_sip_provider_get_auth_context_by_realm_or_call_id(belle_sip_provider_t *p,belle_sip_header_call_id_t* call_id,belle_sip_uri_t *from_uri,const char* realm);

static int belle_sip_auth_context_find_by_nonce(const void* elem, const void* nonce_value){
	authorization_context_t * a = (authorization_context_t*)elem;

	return strcmp(a->nonce, (const char*)nonce_value);
}

static void belle_sip_provider_dispatch_response(belle_sip_provider_t* p, belle_sip_response_t *msg){
	belle_sip_client_transaction_t *t;
	t=belle_sip_provider_find_matching_client_transaction(p,msg);

	/*good opportunity to cleanup auth context if answer = 401|407|403*/

	switch (belle_sip_response_get_status_code(msg)) {
	case 401:
	case 403:
	case 407: {
		if (t!=NULL){
			const char* nonce = NULL;
			belle_sip_message_t* req = BELLE_SIP_MESSAGE(belle_sip_transaction_get_request((belle_sip_transaction_t*)t));
			belle_sip_header_authorization_t* authorization=BELLE_SIP_HEADER_AUTHORIZATION(belle_sip_message_get_header_by_type(req, belle_sip_header_proxy_authorization_t));
			if (authorization==NULL) authorization=belle_sip_message_get_header_by_type(req, belle_sip_header_authorization_t);
			if (authorization!=NULL){
				nonce = belle_sip_header_authorization_get_nonce(authorization);
				if (nonce != NULL){
					belle_sip_list_t * auth_context_with_nonce = NULL;
					while ((auth_context_with_nonce = belle_sip_list_find_custom(p->auth_contexts, belle_sip_auth_context_find_by_nonce, nonce)) != NULL){
						belle_sip_authorization_destroy(auth_context_with_nonce->data);
						p->auth_contexts = belle_sip_list_delete_link(p->auth_contexts, auth_context_with_nonce);
					}
				}
			}
		}
	}
	}
	if (t){ /*In some re-connection case, specially over udp, transaction may be found, but without associated channel*/
		if (t->base.channel == NULL) {
			belle_sip_channel_t *chan;
			belle_sip_message("Transaction [%p] does not have any channel associated, searching for a new one",t);
			chan=belle_sip_provider_get_channel(p,t->next_hop); /*might be faster to get channel directly from upper level*/
			if (chan){
				belle_sip_object_ref(chan);
				belle_sip_channel_add_listener(chan,BELLE_SIP_CHANNEL_LISTENER(t));
				t->base.channel=chan;
			}
		}
	}
		
	/*
	 * If a transaction is found and have a channel, pass it to the transaction and let it decide what to do.
	 * Else notifies directly.
	 */
	if (t && t->base.channel){
		/*since the add_response may indirectly terminate the transaction, we need to guarantee the transaction is not freed
		 * until full completion*/
		belle_sip_object_ref(t);
		belle_sip_client_transaction_add_response(t,msg);
		belle_sip_object_unref(t);
	}else{
		belle_sip_response_event_t event;
		event.source=(belle_sip_object_t*)p;
		event.client_transaction=NULL;
		event.dialog=NULL;
		event.response=msg;
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(p->listeners,process_response_event,&event);
	}
}

void belle_sip_provider_dispatch_message(belle_sip_provider_t *prov, belle_sip_message_t *msg){

	if (TRUE
#ifndef BELLE_SIP_DONT_CHECK_HEADERS_IN_MESSAGE
			&& belle_sip_message_check_headers(msg)
#endif
	){
		if (belle_sip_message_is_request(msg)){
			belle_sip_provider_dispatch_request(prov,(belle_sip_request_t*)msg);
		}else{
			belle_sip_provider_dispatch_response(prov,(belle_sip_response_t*)msg);
		}
	}else{
		/* incorrect message received, answer bad request if it was a request.*/
		if (belle_sip_message_is_request(msg)){
			belle_sip_response_t *resp=belle_sip_response_create_from_request(BELLE_SIP_REQUEST(msg),400);
			if (resp){
				belle_sip_provider_send_response(prov,resp);
			}
		}/*otherwise what can we do ?*/
	}
	belle_sip_object_unref(msg);
}

/*
 * takes example on 16.11 of RFC3261
 */
static void compute_hash_from_invariants(belle_sip_message_t *msg, char *branchid, size_t branchid_size, const char *initial){
	md5_state_t ctx;
	char tmp[256]={0};
	uint8_t digest[16];

	belle_sip_header_call_id_t* callid_hdr = belle_sip_message_get_header_by_type(msg,belle_sip_header_call_id_t);
	belle_sip_header_cseq_t*      cseq_hdr = belle_sip_message_get_header_by_type(msg,belle_sip_header_cseq_t);
	belle_sip_header_from_t*      from_hdr = belle_sip_message_get_header_by_type(msg,belle_sip_header_from_t);
	belle_sip_header_to_t*          to_hdr = belle_sip_message_get_header_by_type(msg,belle_sip_header_to_t);

	unsigned int    cseq = cseq_hdr   ? belle_sip_header_cseq_get_seq_number(cseq_hdr)   : 0;
	const char   *callid = callid_hdr ? belle_sip_header_call_id_get_call_id(callid_hdr) : "";
	const char *from_tag = from_hdr   ? belle_sip_header_from_get_tag(from_hdr)          : "";
	const char   *to_tag = to_hdr     ? belle_sip_header_to_get_tag(to_hdr)              : "";

	belle_sip_uri_t *requri=NULL;
	belle_sip_header_via_t *via=NULL;
	belle_sip_header_via_t *prev_via=NULL;
	const belle_sip_list_t *vias=belle_sip_message_get_headers(msg,"via");
	int is_request=belle_sip_message_is_request(msg);

	if (vias){
		via=(belle_sip_header_via_t*)vias->data;
		if (vias->next){
			prev_via=(belle_sip_header_via_t*)vias->next->data;
		}
	}

	if (is_request){
		requri=belle_sip_request_get_uri(BELLE_SIP_REQUEST(msg));
	}

	belle_sip_md5_init(&ctx);
	if (initial)
		belle_sip_md5_append(&ctx,(uint8_t*)initial,(int)strlen(initial));
	if (requri){
		size_t offset=0;
		belle_sip_object_marshal((belle_sip_object_t*)requri,tmp,sizeof(tmp)-1,&offset);
		belle_sip_md5_append(&ctx,(uint8_t*)tmp,(int)strlen(tmp));
	}
	if (from_tag)
		belle_sip_md5_append(&ctx,(uint8_t*)from_tag,(int)strlen(from_tag));
	if (to_tag)
		belle_sip_md5_append(&ctx,(uint8_t*)to_tag,(int)strlen(to_tag));
	belle_sip_md5_append(&ctx,(uint8_t*)callid,(int)strlen(callid));
	belle_sip_md5_append(&ctx,(uint8_t*)&cseq,sizeof(cseq));
	if (is_request){
		if (prev_via){
			size_t offset=0;
			belle_sip_object_marshal((belle_sip_object_t*)prev_via,tmp,sizeof(tmp)-1,&offset);
			belle_sip_md5_append(&ctx,(uint8_t*)tmp,(int)offset);
		}
	}else{
		if (via){
			size_t offset=0;
			belle_sip_object_marshal((belle_sip_object_t*)via,tmp,sizeof(tmp)-1,&offset);
			belle_sip_md5_append(&ctx,(uint8_t*)tmp,(int)offset);
		}
	}
	belle_sip_md5_finish(&ctx,digest);
	belle_sip_octets_to_text(digest,sizeof(digest),branchid,branchid_size);
}

/*
 * RFC2543 10.1.2:
 * "Responses are mapped to requests by the matching To, From, Call-ID,
 * CSeq headers and the branch parameter of the first Via header."
 *
 * to-tag must not be used because an ACK will contain one while original INVITE will not.
 * Cseq's method is changed for CANCEL so we must not use it as well.
**/

static char *compute_rfc2543_branch(belle_sip_request_t *req, char *branchid, size_t branchid_size){
	md5_state_t ctx;
	unsigned int cseq=belle_sip_header_cseq_get_seq_number(belle_sip_message_get_header_by_type(req,belle_sip_header_cseq_t));
	uint8_t digest[16];
	const char* callid=belle_sip_header_call_id_get_call_id(belle_sip_message_get_header_by_type(req,belle_sip_header_call_id_t));
	belle_sip_header_via_t *via=belle_sip_message_get_header_by_type(req,belle_sip_header_via_t);
	const char *v_branch=belle_sip_header_via_get_branch(via);
	belle_sip_header_from_t *from=belle_sip_message_get_header_by_type(req,belle_sip_header_from_t);
	char *from_str=belle_sip_object_to_string(from);
	belle_sip_header_to_t *to=belle_sip_message_get_header_by_type(req,belle_sip_header_to_t);
	char *to_str=belle_sip_object_to_string(belle_sip_header_address_get_uri((belle_sip_header_address_t*)to));

	belle_sip_md5_init(&ctx);

	belle_sip_md5_append(&ctx,(uint8_t*)from_str,(int)strlen(from_str));
	belle_sip_md5_append(&ctx,(uint8_t*)to_str,(int)strlen(to_str));
	belle_sip_md5_append(&ctx,(uint8_t*)callid,(int)strlen(callid));
	belle_sip_md5_append(&ctx,(uint8_t*)&cseq,sizeof(cseq));
	belle_sip_free(from_str);
	belle_sip_free(to_str);

	if (v_branch)
		belle_sip_md5_append(&ctx,(uint8_t*)v_branch,(int)strlen(v_branch));

	belle_sip_md5_finish(&ctx,digest);
	belle_sip_octets_to_text(digest,sizeof(digest),branchid,branchid_size);
	return branchid;
}

static void fix_outgoing_via(belle_sip_provider_t *p, belle_sip_channel_t *chan, belle_sip_message_t *msg){
	belle_sip_header_via_t *via=BELLE_SIP_HEADER_VIA(belle_sip_message_get_header(msg,"via"));
	if (p->rport_enabled) belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(via),"rport",NULL);

	belle_sip_header_via_set_host(via,chan->local_ip);
	belle_sip_header_via_set_port(via,chan->local_port);
	belle_sip_header_via_set_protocol(via,"SIP/2.0");
	belle_sip_header_via_set_transport(via,belle_sip_channel_get_transport_name(chan));

	if (belle_sip_header_via_get_branch(via)==NULL){
		/*branch id should not be set random here (stateless forwarding): but rather a hash of message invariants*/
		char branchid[24];
		char token[BELLE_SIP_BRANCH_ID_LENGTH];
		compute_hash_from_invariants(msg,token,sizeof(token),NULL);
		snprintf(branchid,sizeof(branchid)-1,BELLE_SIP_BRANCH_MAGIC_COOKIE ".%s",token);
		belle_sip_header_via_set_branch(via,branchid);
		belle_sip_message("Computing branch id %s for message sent statelessly", branchid);
	}
}

static void channel_on_message_headers(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_message_t *msg){
	/*not used*/
}

static void channel_on_message(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_message_t *msg){
	belle_sip_object_ref(msg);
	belle_sip_provider_dispatch_message(BELLE_SIP_PROVIDER(obj),msg);
}

static int channel_on_auth_requested(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, const char* distinguished_name){
	if (BELLE_SIP_IS_INSTANCE_OF(chan,belle_sip_tls_channel_t)) {
		belle_sip_provider_t *prov=BELLE_SIP_PROVIDER(obj);
		belle_sip_auth_event_t* auth_event = belle_sip_auth_event_create((belle_sip_object_t*)prov,NULL,NULL);
		belle_sip_tls_channel_t *tls_chan=BELLE_SIP_TLS_CHANNEL(chan);
		auth_event->mode=BELLE_SIP_AUTH_MODE_TLS;
		belle_sip_auth_event_set_distinguished_name(auth_event,distinguished_name);
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(prov->listeners,process_auth_requested,auth_event);
		belle_sip_tls_channel_set_client_certificates_chain(tls_chan,auth_event->cert);
		belle_sip_tls_channel_set_client_certificate_key(tls_chan,auth_event->key);
		belle_sip_auth_event_destroy(auth_event);
	}
	return 0;
}

static void channel_on_sending(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_message_t *msg){
	belle_sip_header_contact_t* contact;
	belle_sip_header_content_length_t* content_length = (belle_sip_header_content_length_t*)belle_sip_message_get_header(msg,"Content-Length");
	belle_sip_uri_t* contact_uri;
	const belle_sip_list_t *contacts;
	const char *ip=NULL;
	int port=0;
	belle_sip_provider_t *prov=BELLE_SIP_PROVIDER(obj);

	if (belle_sip_message_is_request(msg)){
		const belle_sip_list_t *rroutes;
		/*probably better to be in channel*/
		fix_outgoing_via(prov, chan, msg);

		for (rroutes=belle_sip_message_get_headers(msg,"Record-Route");rroutes!=NULL;rroutes=rroutes->next){
			belle_sip_header_record_route_t* rr=(belle_sip_header_record_route_t*)rroutes->data;
			if (belle_sip_header_record_route_get_auto_outgoing(rr)) {
				belle_sip_uri_t *rr_uri = belle_sip_channel_create_routable_uri(chan);
				belle_sip_header_address_set_uri((belle_sip_header_address_t*) rr, rr_uri);
			}
		}
	}

	for (contacts=belle_sip_message_get_headers(msg,"Contact");contacts!=NULL;contacts=contacts->next){
		const char *transport;
		contact=(belle_sip_header_contact_t*)contacts->data;

		if (belle_sip_header_contact_is_wildcard(contact)) continue;
		/* fix the contact if in automatic mode or null uri (for backward compatibility)*/
		if (!(contact_uri = belle_sip_header_address_get_uri((belle_sip_header_address_t*)contact))) {
			contact_uri = belle_sip_uri_new();
			belle_sip_header_address_set_uri((belle_sip_header_address_t*)contact,contact_uri);
			belle_sip_header_contact_set_automatic(contact,TRUE);
		}else if (belle_sip_uri_get_host(contact_uri)==NULL){
			belle_sip_header_contact_set_automatic(contact,TRUE);
		}
		if (!belle_sip_header_contact_get_automatic(contact)) continue;

		if (ip==NULL){
			if (prov->nat_helper){
				ip=chan->public_ip ? chan->public_ip : chan->local_ip;
				port=chan->public_port ? chan->public_port : chan->local_port;
				belle_sip_header_contact_set_unknown(contact,!chan->learnt_ip_port);
			}else{
				ip=chan->local_ip;
				port=chan->local_port;
			}
		}

		belle_sip_uri_set_host(contact_uri,ip);
		transport=belle_sip_channel_get_transport_name_lower_case(chan);
		
		/* Enforce a transport name in "sip" scheme.
		 * RFC3263 (locating SIP servers) says that UDP SHOULD be used in absence of transport parameter,
		 * when port or numeric IP are provided. It is a SHOULD, not a must.
		 * We need in this case that the automatic Contact exactly matches the socket that is going 
		 * to be used for sending the messages.
		 * TODO: we may need to do the same for sips, but dtls is currently not supported.
		 **/
		
		if (!belle_sip_uri_is_secure(contact_uri))
			belle_sip_uri_set_transport_param(contact_uri,transport);

		if (port!=belle_sip_listening_point_get_well_known_port(transport)) {
			belle_sip_uri_set_port(contact_uri,port);
		}else{
			belle_sip_uri_set_port(contact_uri,0);
		}
	}

/*
 * According to RFC3261, content-length is mandatory for stream based transport, but optional for datagram transport.
 * However some servers (opensips) are confused when they receive a SIP/UDP packet without Content-Length (they shouldn't).
 */
	if (!content_length
		&& belle_sip_message_get_body_size(msg) == 0 /*if body present, content_length is automatically added at channel level*/
#ifndef BELLE_SIP_FORCE_CONTENT_LENGTH
		&& strcasecmp("udp",belle_sip_channel_get_transport_name(chan))!=0
#endif
	) {
		content_length = belle_sip_header_content_length_create(0);
		belle_sip_message_add_header(msg,(belle_sip_header_t*)content_length);
	}
}

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_sip_provider_t,belle_sip_channel_listener_t)
	channel_state_changed,
	channel_on_message_headers,
	channel_on_message,
	channel_on_sending,
	channel_on_auth_requested
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_sip_provider_t,belle_sip_channel_listener_t);

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_provider_t,belle_sip_object_t,belle_sip_provider_uninit,NULL,NULL,FALSE);

belle_sip_provider_t *belle_sip_provider_new(belle_sip_stack_t *s, belle_sip_listening_point_t *lp){
	belle_sip_provider_t *p=belle_sip_object_new(belle_sip_provider_t);
	p->stack=s;
	p->rport_enabled=1;
	p->unconditional_answer = 480;
	if (lp) belle_sip_provider_add_listening_point(p,lp);
	return p;
}

/* This function is used by a proxy to set its call side record route.
 * It must be called before adding any VIA header to the message. */
belle_sip_uri_t *belle_sip_provider_create_inbound_record_route(belle_sip_provider_t *p, belle_sip_request_t *req) {
	belle_sip_uri_t* origin = belle_sip_request_extract_origin(req);
	belle_sip_hop_t *hop = belle_sip_hop_new_from_uri(origin);
	belle_sip_channel_t *inChan = belle_sip_provider_get_channel(p, hop);
	return belle_sip_channel_create_routable_uri(inChan);
}

static belle_sip_channel_t* _belle_sip_provider_find_channel_using_routable(belle_sip_provider_t *p, const belle_sip_uri_t* routable_uri) {
	const char *transport;
	belle_sip_listening_point_t *lp;
	belle_sip_list_t *elem;
	belle_sip_channel_t *chan;
	belle_sip_uri_t* chan_uri;

	if (!routable_uri) return NULL;

	transport = belle_sip_uri_is_secure(routable_uri) ? "TLS" : belle_sip_uri_get_transport_param(routable_uri);
	lp = belle_sip_provider_get_listening_point(p, transport);
	if (!lp) return NULL;


	for(elem=lp->channels; elem ;elem=elem->next){
		chan=(belle_sip_channel_t*)elem->data;
		chan_uri = belle_sip_channel_create_routable_uri(chan);
		if (belle_sip_uri_get_port(routable_uri) == belle_sip_uri_get_port(chan_uri) &&
			0 == strcmp(belle_sip_uri_get_host(routable_uri), belle_sip_uri_get_host(chan_uri))) {
			return chan;
		}
	}
	return NULL;
}

/*
 * This function is not efficient at all, REVISIT.
 * Its goal is to determine whether a routable (route or record route) matches the local provider instance.
 * In order to do that, we go through all the channels and ask them their routable uri, and see if it matches the uri passed in argument.
 * This creates a lot of temporary objects and iterates through a potentially long list of routables.
 * Some more efficient solutions could be:
 * 1- insert a magic cookie parameter in each routable created by the provider, so that recognition is immediate.
 *    Drawback: use of non-standard, possibly conflicting parameter.
 * 2- check the listening point's uri first (but need to match the ip address to any local ip if it is INADDR_ANY), then use belle_sip_listening_point_get_channel()
 *    to see if a channel is matching.
 *    belle_sip_listening_point_get_channel() is not optimized currently but will have to be, so at least we leverage on something that will be optimized.
**/
int belle_sip_provider_is_us(belle_sip_provider_t *p, belle_sip_uri_t* uri) {
	belle_sip_channel_t* chan = _belle_sip_provider_find_channel_using_routable(p, uri);
	return !!chan;
}


int belle_sip_provider_add_listening_point(belle_sip_provider_t *p, belle_sip_listening_point_t *lp){
	if (lp == NULL) {
		belle_sip_error("Cannot add NULL lp to provider [%p]",p);
		return -1;
	}
	belle_sip_listening_point_set_channel_listener(lp,BELLE_SIP_CHANNEL_LISTENER(p));
	p->lps=belle_sip_list_append(p->lps,belle_sip_object_ref(lp));
	return 0;
}

void belle_sip_provider_remove_listening_point(belle_sip_provider_t *p, belle_sip_listening_point_t *lp) {
	p->lps=belle_sip_list_remove(p->lps,lp);
	belle_sip_object_unref(lp);
	return;
}

belle_sip_listening_point_t *belle_sip_provider_get_listening_point(belle_sip_provider_t *p, const char *transport){
	belle_sip_list_t *l;
	for(l=p->lps;l!=NULL;l=l->next){
		belle_sip_listening_point_t *lp=(belle_sip_listening_point_t*)l->data;
		if (strcasecmp(belle_sip_listening_point_get_transport(lp),transport)==0)
			return lp;
	}
	return NULL;
}

const belle_sip_list_t *belle_sip_provider_get_listening_points(belle_sip_provider_t *p){
	return p->lps;
}

void belle_sip_provider_add_internal_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l, int prepend){
	if (prepend)
		p->internal_listeners=belle_sip_list_prepend(p->internal_listeners,l);
	else
		p->internal_listeners=belle_sip_list_append(p->internal_listeners,l);
}

void belle_sip_provider_remove_internal_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l){
	p->internal_listeners=belle_sip_list_remove(p->internal_listeners,l);
}

void belle_sip_provider_add_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l){
	p->listeners=belle_sip_list_append(p->listeners,l);
}

void belle_sip_provider_remove_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l){
	p->listeners=belle_sip_list_remove(p->listeners,l);
}

belle_sip_header_call_id_t * belle_sip_provider_create_call_id(const belle_sip_provider_t *prov){
	belle_sip_header_call_id_t *cid=belle_sip_header_call_id_new();
	char tmp[11];
	belle_sip_header_call_id_set_call_id(cid,belle_sip_random_token(tmp,sizeof(tmp)));
	return cid;
}

belle_sip_dialog_t * belle_sip_provider_create_dialog(belle_sip_provider_t *prov, belle_sip_transaction_t *t) {
	return belle_sip_provider_create_dialog_internal(prov,t,TRUE);
}

belle_sip_dialog_t * belle_sip_provider_create_dialog_internal(belle_sip_provider_t *prov, belle_sip_transaction_t *t,unsigned int check_last_resp){
	belle_sip_dialog_t *dialog=NULL;

	if (check_last_resp && t->last_response){
		int code=belle_sip_response_get_status_code(t->last_response);
		if (code>=200 && code<300){
			belle_sip_fatal("You must not create dialog after sending the response that establish the dialog.");
			return NULL;
		}
	}
	dialog=belle_sip_dialog_new(t);
	if (dialog) {
		belle_sip_transaction_set_dialog(t,dialog);
		belle_sip_provider_add_dialog(prov,dialog);
	}
	return dialog;
}

/*find a dialog given the call id, local-tag and to-tag*/
belle_sip_dialog_t* belle_sip_provider_find_dialog(const belle_sip_provider_t *prov, const char* call_id, const char* local_tag, const char* remote_tag) {
	belle_sip_list_t* iterator;
	belle_sip_dialog_t*returned_dialog=NULL;
	
	if (call_id == NULL || local_tag == NULL || remote_tag == NULL) {
		return NULL;
	}
	
	for(iterator=prov->dialogs;iterator!=NULL;iterator=iterator->next) {
		belle_sip_dialog_t* dialog=(belle_sip_dialog_t*)iterator->data;
		dialog=(belle_sip_dialog_t*)iterator->data;
		/*ignore dialog in state BELLE_SIP_DIALOG_NULL, is it really the correct things to do*/
		if (belle_sip_dialog_get_state(dialog) != BELLE_SIP_DIALOG_NULL && _belle_sip_dialog_match(dialog,call_id,local_tag,remote_tag)) {
			if (!returned_dialog)
				returned_dialog=dialog;
			else
				belle_sip_fatal("More than 1 dialog is matching, check your app");
		}
	}
	return returned_dialog;
}

/*finds an existing dialog for an outgoing or incoming message */
belle_sip_dialog_t *belle_sip_provider_find_dialog_from_message(belle_sip_provider_t *prov, belle_sip_message_t *msg, int as_uas){
	belle_sip_header_call_id_t *call_id;
	belle_sip_header_from_t *from;
	belle_sip_header_to_t *to;
	const char *from_tag;
	const char *to_tag;
	const char *call_id_value;
	const char *local_tag,*remote_tag;

	if (belle_sip_message_is_request(msg)){
		belle_sip_request_t *req=BELLE_SIP_REQUEST(msg);
		if (req->dialog)
			return req->dialog;
	}

	to=belle_sip_message_get_header_by_type(msg,belle_sip_header_to_t);

	if (to==NULL || (to_tag=belle_sip_header_to_get_tag(to))==NULL){
		/* a request without to tag cannot be part of a dialog */
		return NULL;
	}

	call_id=belle_sip_message_get_header_by_type(msg,belle_sip_header_call_id_t);
	from=belle_sip_message_get_header_by_type(msg,belle_sip_header_from_t);

	if (call_id==NULL || from==NULL || (from_tag=belle_sip_header_from_get_tag(from))==NULL) return NULL;

	call_id_value=belle_sip_header_call_id_get_call_id(call_id);
	local_tag=as_uas ? to_tag : from_tag;
	remote_tag=as_uas ? from_tag : to_tag;
	return belle_sip_provider_find_dialog(prov,call_id_value,local_tag,remote_tag);
}

void belle_sip_provider_add_dialog(belle_sip_provider_t *prov, belle_sip_dialog_t *dialog){
	prov->dialogs=belle_sip_list_prepend(prov->dialogs,belle_sip_object_ref(dialog));
}

static void notify_dialog_terminated(belle_sip_dialog_terminated_event_t* ev) {
	BELLE_SIP_PROVIDER_INVOKE_LISTENERS_FOR_DIALOG(ev->dialog,process_dialog_terminated,ev);
	belle_sip_object_unref(ev->dialog);
	belle_sip_free(ev);
}

void belle_sip_provider_remove_dialog(belle_sip_provider_t *prov, belle_sip_dialog_t *dialog){
	belle_sip_dialog_terminated_event_t* ev=belle_sip_malloc(sizeof(belle_sip_dialog_terminated_event_t));
	ev->source=prov;
	ev->dialog=dialog;
	ev->is_expired=dialog->is_expired;
	prov->dialogs=belle_sip_list_remove(prov->dialogs,dialog);
	belle_sip_main_loop_do_later(belle_sip_stack_get_main_loop(prov->stack)
					,(belle_sip_callback_t) notify_dialog_terminated
					, ev);

}

belle_sip_client_transaction_t *belle_sip_provider_create_client_transaction(belle_sip_provider_t *prov, belle_sip_request_t *req){
	const char *method=belle_sip_request_get_method(req);
	belle_sip_client_transaction_t *t;
	belle_sip_client_transaction_t *inv_transaction;
	if (strcmp(method,"INVITE")==0)
		t=(belle_sip_client_transaction_t*)belle_sip_ict_new(prov,req);
	else if (strcmp(method,"ACK")==0){
		belle_sip_error("belle_sip_provider_create_client_transaction() cannot be used for ACK requests.");
		return NULL;
	} else {
		t=(belle_sip_client_transaction_t*)belle_sip_nict_new(prov,req);
		if (strcmp(method,"CANCEL")==0){
			/*force next hop*/
			inv_transaction=belle_sip_provider_find_matching_client_transaction_from_req(prov,req);
			if (inv_transaction && inv_transaction->next_hop) {
				/*found corresponding ict, taking next hop*/
				/*9.1 Client Behavior
				 * The destination address,
				   port, and transport for the CANCEL MUST be identical to those used to
				   send the original request.*/
				t->next_hop=(belle_sip_hop_t*)belle_sip_object_ref(inv_transaction->next_hop);
			} else {
				belle_sip_error ("No corresponding ict nor dest found for cancel request attached to transaction [%p]",t);
			}
		}
	}
	belle_sip_transaction_set_dialog((belle_sip_transaction_t*)t,belle_sip_provider_find_dialog_from_message(prov,(belle_sip_message_t*)req,FALSE));
	belle_sip_request_set_dialog(req,NULL);/*get rid of the reference to the dialog, which is no longer needed in the message.
					This is to avoid circular references.*/
	return t;
}

belle_sip_server_transaction_t *belle_sip_provider_create_server_transaction(belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_server_transaction_t* t;
	belle_sip_response_t *resp = NULL;
	if (strcmp(belle_sip_request_get_method(req),"INVITE")==0){
		t=(belle_sip_server_transaction_t*)belle_sip_ist_new(prov,req);
		/*create a 100 Trying response to immediately stop client retransmissions*/
		resp=belle_sip_response_create_from_request(req,100);
		
	}else if (strcmp(belle_sip_request_get_method(req),"ACK")==0){
		belle_sip_error("Creating a server transaction for an ACK is not a good idea, probably");
		return NULL;
	}else
		t=(belle_sip_server_transaction_t*)belle_sip_nist_new(prov,req);
	belle_sip_transaction_set_dialog((belle_sip_transaction_t*)t,belle_sip_provider_find_dialog_from_message(prov,(belle_sip_message_t*)req,TRUE));
	belle_sip_provider_add_server_transaction(prov,t);
	if (resp){
		/*the response must be sent after the server transaction is refd by belle_sip_provider_add_server_transaction , otherwise
		 * through callbacks we'll reach a point where it is unrefed before leaving from this function*/
		belle_sip_server_transaction_send_response(t, resp);
	}
	return t;
}

belle_sip_stack_t *belle_sip_provider_get_sip_stack(belle_sip_provider_t *p){
	return p->stack;
}

belle_sip_channel_t * belle_sip_provider_get_channel(belle_sip_provider_t *p, const belle_sip_hop_t *hop){
	belle_sip_list_t *l;
	belle_sip_listening_point_t *candidate=NULL,*lp;
	belle_sip_channel_t *chan;

	if (hop->transport!=NULL) {
		for(l=p->lps;l!=NULL;l=l->next){
			lp=(belle_sip_listening_point_t*)l->data;
			if (strcasecmp(belle_sip_listening_point_get_transport(lp),hop->transport)==0){
				chan=belle_sip_listening_point_get_channel(lp,hop);
				if (chan) return chan;
				candidate=lp;
			}
		}
		if (candidate){
			chan=belle_sip_listening_point_create_channel(candidate,hop);
			if (!chan) belle_sip_error("Could not create channel to [%s://%s:%i]",hop->transport,hop->host,hop->port);
			return chan;
		}
	}
	belle_sip_error("No listening point matching for [%s://%s:%i]",hop->transport,hop->host,hop->port);
	return NULL;
}

void belle_sip_provider_release_channel(belle_sip_provider_t *p, belle_sip_channel_t *chan){
	belle_sip_listening_point_remove_channel(chan->lp,chan);
}

void belle_sip_provider_clean_channels(belle_sip_provider_t *p){
	belle_sip_list_t *l;
	belle_sip_listening_point_t *lp;

	for(l=p->lps;l!=NULL;l=l->next){
		lp=(belle_sip_listening_point_t*)l->data;
		belle_sip_listening_point_clean_channels(lp);
	}
}

void belle_sip_provider_send_request(belle_sip_provider_t *p, belle_sip_request_t *req){
	belle_sip_hop_t* hop;
	belle_sip_channel_t *chan;
	hop=belle_sip_stack_get_next_hop(p->stack,req);
	chan=belle_sip_provider_get_channel(p,hop);
	if (chan) {
		belle_sip_channel_queue_message(chan,BELLE_SIP_MESSAGE(req));
	}
}

void belle_sip_provider_send_response(belle_sip_provider_t *p, belle_sip_response_t *resp){
	belle_sip_hop_t* hop;
	belle_sip_channel_t *chan;
	belle_sip_header_to_t *to=(belle_sip_header_to_t*)belle_sip_message_get_header((belle_sip_message_t*)resp,"to");

	if (belle_sip_response_get_status_code(resp)!=100 && to && belle_sip_header_to_get_tag(to)==NULL){
		char token[BELLE_SIP_TAG_LENGTH];
		compute_hash_from_invariants((belle_sip_message_t*)resp,token,sizeof(token),"tag");
		belle_sip_header_to_set_tag(to,token);
	}
	hop=belle_sip_response_get_return_hop(resp);
	if (hop){
		chan=belle_sip_provider_get_channel(p,hop);
		if (chan) belle_sip_channel_queue_message(chan,BELLE_SIP_MESSAGE(resp));
		belle_sip_object_unref(hop);
	}
}


/*private provider API*/

void belle_sip_provider_set_transaction_terminated(belle_sip_provider_t *p, belle_sip_transaction_t *t){
	belle_sip_transaction_terminated_event_t ev;

	BELLE_SIP_OBJECT_VPTR(t,belle_sip_transaction_t)->on_terminate(t);
	ev.source=t->provider;
	ev.transaction=t;
	ev.is_server_transaction=BELLE_SIP_IS_INSTANCE_OF(t,belle_sip_server_transaction_t);
	BELLE_SIP_PROVIDER_INVOKE_LISTENERS_FOR_TRANSACTION(t,process_transaction_terminated,&ev);
	if (!ev.is_server_transaction){
		belle_sip_provider_remove_client_transaction(p,(belle_sip_client_transaction_t*)t);
	}else{
		belle_sip_provider_remove_server_transaction(p,(belle_sip_server_transaction_t*)t);
	}
}

void belle_sip_provider_add_client_transaction(belle_sip_provider_t *prov, belle_sip_client_transaction_t *t){
	prov->client_transactions=belle_sip_list_prepend(prov->client_transactions,belle_sip_object_ref(t));
}

struct client_transaction_matcher{
	const char *branchid;
	const char *method;
};

static int client_transaction_match(const void *p_tr, const void *p_matcher){
	belle_sip_client_transaction_t *tr=(belle_sip_client_transaction_t*)p_tr;
	struct client_transaction_matcher *matcher=(struct client_transaction_matcher*)p_matcher;
	const char *req_method=belle_sip_request_get_method(tr->base.request);
	if (strcmp(matcher->branchid,tr->base.branch_id)==0 && strcmp(matcher->method,req_method)==0) return 0;
	return -1;
}

belle_sip_client_transaction_t * belle_sip_provider_find_matching_client_transaction(belle_sip_provider_t *prov,
																				   belle_sip_response_t *resp){
	struct client_transaction_matcher matcher;
	belle_sip_header_via_t *via=(belle_sip_header_via_t*)belle_sip_message_get_header((belle_sip_message_t*)resp,"via");
	belle_sip_header_cseq_t *cseq=(belle_sip_header_cseq_t*)belle_sip_message_get_header((belle_sip_message_t*)resp,"cseq");
	belle_sip_client_transaction_t *ret=NULL;
	belle_sip_list_t *elem;
	if (via==NULL){
		belle_sip_warning("Response has no via.");
		return NULL;
	}
	if (cseq==NULL){
		belle_sip_warning("Response has no cseq.");
		return NULL;
	}
	matcher.branchid=belle_sip_header_via_get_branch(via);
	matcher.method=belle_sip_header_cseq_get_method(cseq);
	elem=belle_sip_list_find_custom(prov->client_transactions,client_transaction_match,&matcher);
	if (elem){
		ret=(belle_sip_client_transaction_t*)elem->data;
		belle_sip_message("Found transaction matching response.");
	}
	return ret;
}

void belle_sip_provider_remove_client_transaction(belle_sip_provider_t *prov, belle_sip_client_transaction_t *t){
	belle_sip_list_t* elem=belle_sip_list_find(prov->client_transactions,t);
	if (elem) {
		prov->client_transactions=belle_sip_list_delete_link(prov->client_transactions,elem);
		belle_sip_object_unref(t);
	} else {
		belle_sip_error("trying to remove transaction [%p] not part of provider [%p]",t,prov);
	}

}

void belle_sip_provider_add_server_transaction(belle_sip_provider_t *prov, belle_sip_server_transaction_t *t){
	prov->server_transactions=belle_sip_list_prepend(prov->server_transactions,belle_sip_object_ref(t));
}

struct transaction_matcher{
	const char *branchid;
	const char *method;
	const char *sentby;
	int is_ack_or_cancel;
};

static int transaction_match(const void *p_tr, const void *p_matcher){
	belle_sip_transaction_t *tr=(belle_sip_transaction_t*)p_tr;
	struct transaction_matcher *matcher=(struct transaction_matcher*)p_matcher;
	const char *req_method=belle_sip_request_get_method(tr->request);
	if (strcmp(matcher->branchid,tr->branch_id)==0){
		if (strcmp(matcher->method,req_method)==0) return 0;
		if (matcher->is_ack_or_cancel && strcmp(req_method,"INVITE")==0) return 0;
	}
	return -1;
}

belle_sip_transaction_t * belle_sip_provider_find_matching_transaction(belle_sip_list_t *transactions, belle_sip_request_t *req){
	struct transaction_matcher matcher;
	belle_sip_header_via_t *via=(belle_sip_header_via_t*)belle_sip_message_get_header((belle_sip_message_t*)req,"via");
	belle_sip_transaction_t *ret=NULL;
	belle_sip_list_t *elem=NULL;
	const char *branch;
	char token[BELLE_SIP_BRANCH_ID_LENGTH];


	matcher.method=belle_sip_request_get_method(req);
	matcher.is_ack_or_cancel=(strcmp(matcher.method,"ACK")==0 || strcmp(matcher.method,"CANCEL")==0);

	if (via!=NULL && (branch=belle_sip_header_via_get_branch(via))!=NULL &&
		strncmp(branch,BELLE_SIP_BRANCH_MAGIC_COOKIE,strlen(BELLE_SIP_BRANCH_MAGIC_COOKIE))==0){
		matcher.branchid=branch;
	}else{
		/*this request comes from an old equipment, we need to compute our own branch for this request.*/
		matcher.branchid=compute_rfc2543_branch(req,token,sizeof(token));
		belle_sip_request_set_rfc2543_branch(req,token);
		belle_sip_message("Message from old RFC2543 stack, computed branch is %s", token);
	}

	elem=belle_sip_list_find_custom(transactions,transaction_match,&matcher);

	if (elem){
		ret=(belle_sip_transaction_t*)elem->data;
		belle_sip_message("Found transaction [%p] matching request.",ret);
	}
	return ret;
}
belle_sip_server_transaction_t * belle_sip_provider_find_matching_server_transaction(belle_sip_provider_t *prov, belle_sip_request_t *req) {
	belle_sip_transaction_t *ret=belle_sip_provider_find_matching_transaction(prov->server_transactions,req);
	return ret?BELLE_SIP_SERVER_TRANSACTION(ret):NULL;
}
belle_sip_client_transaction_t * belle_sip_provider_find_matching_client_transaction_from_req(belle_sip_provider_t *prov, belle_sip_request_t *req) {
	belle_sip_transaction_t *ret=belle_sip_provider_find_matching_transaction(prov->client_transactions,req);
	return ret?BELLE_SIP_CLIENT_TRANSACTION(ret):NULL;
}

void belle_sip_provider_remove_server_transaction(belle_sip_provider_t *prov, belle_sip_server_transaction_t *t){
	prov->server_transactions=belle_sip_list_remove(prov->server_transactions,t);
	belle_sip_object_unref(t);
}


static void authorization_context_fill_from_auth(authorization_context_t* auth_context,belle_sip_header_www_authenticate_t* authenticate,belle_sip_uri_t *from_uri) {
	authorization_context_set_realm(auth_context,belle_sip_header_www_authenticate_get_realm(authenticate));
	if (auth_context->nonce && strcmp(belle_sip_header_www_authenticate_get_nonce(authenticate),auth_context->nonce)!=0) {
		/*new nonce, resetting nounce_count*/
		auth_context->nonce_count=0;
	}
	authorization_context_set_nonce(auth_context,belle_sip_header_www_authenticate_get_nonce(authenticate));
	authorization_context_set_algorithm(auth_context,belle_sip_header_www_authenticate_get_algorithm(authenticate));
	authorization_context_set_qop(auth_context,belle_sip_header_www_authenticate_get_qop_first(authenticate));
	authorization_context_set_scheme(auth_context,belle_sip_header_www_authenticate_get_scheme(authenticate));
	authorization_context_set_opaque(auth_context,belle_sip_header_www_authenticate_get_opaque(authenticate));
	authorization_context_set_user_id(auth_context, from_uri?belle_sip_uri_get_user(from_uri):NULL);

	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(authenticate,belle_sip_header_proxy_authenticate_t)) {
		auth_context->is_proxy=1;
	}
}

static belle_sip_list_t* belle_sip_provider_get_auth_context_by_realm_or_call_id(belle_sip_provider_t *p,belle_sip_header_call_id_t* call_id,belle_sip_uri_t *from_uri,const char* realm) {
	belle_sip_list_t* auth_context_lst=NULL;
	belle_sip_list_t* result=NULL;
	authorization_context_t* auth_context;

	for (auth_context_lst=p->auth_contexts;auth_context_lst!=NULL;auth_context_lst=auth_context_lst->next) {
		auth_context=(authorization_context_t*)auth_context_lst->data;
		if (belle_sip_header_call_id_equals(auth_context->callid,call_id) ) {
			result=belle_sip_list_append(result,auth_context_lst->data);
		}
	}

	/* According to the RFC3261 22.3, if the outbound proxy realm is set, we could reuse its nonce value:
	 * "If a UA receives a Proxy-Authenticate header field value in a 401/407
	 * response to a request with a particular Call-ID, it should
	 * incorporate credentials for that realm in all subsequent requests
	 * that contain the same Call-ID.  These credentials MUST NOT be cached
	 * across dialogs; however, if a UA is configured with the realm of its
	 * local outbound proxy, when one exists, then the UA MAY cache
	 * credentials for that realm across dialogs."
	*/
	if (result == NULL){
		const char * from_user=from_uri?belle_sip_uri_get_user(from_uri):NULL;

		belle_sip_debug("belle_sip_provider_auth: no auth context registered with [call_id=%s], looking for realm..."
			, call_id?belle_sip_header_call_id_get_call_id(call_id):"(null)");

		for (auth_context_lst=p->auth_contexts;auth_context_lst!=NULL;auth_context_lst=auth_context_lst->next) {
			auth_context=(authorization_context_t*)auth_context_lst->data;

			belle_sip_debug("belle_sip_provider_auth: \t[realm=%s] [user_id=%s] [call_id=%s]",
				auth_context->realm?auth_context->realm:"(null)",
				auth_context->user_id?auth_context->user_id:"(null)",
				auth_context->callid?belle_sip_header_call_id_get_call_id(auth_context->callid):"(null)"
			);
			/* We also verify that user matches in case of multi-account to avoid use nonce from another account. For a
			 * single user, from_uri user id and realm user id COULD be different but we assume here that this is not the case
			 * in order to avoid adding another field in auth_context struct.
			**/
			if ((realm && strcmp(auth_context->realm,realm)==0)
				&& (from_user && auth_context->user_id && strcmp(auth_context->user_id,from_user)==0)) {

				result=belle_sip_list_append(result,auth_context_lst->data);
				belle_sip_debug("belle_sip_provider_auth: found a MATCHING realm auth context!");
			}
		}
	}
	return result;
}

static void  belle_sip_provider_update_or_create_auth_context(belle_sip_provider_t *p,belle_sip_header_call_id_t* call_id,belle_sip_header_www_authenticate_t* authenticate,belle_sip_uri_t *from_uri,const char* realm) {
	belle_sip_list_t* auth_context_lst = NULL;
	authorization_context_t* auth_context;

	for (auth_context_lst=belle_sip_provider_get_auth_context_by_realm_or_call_id(p,call_id,from_uri,realm);auth_context_lst!=NULL;auth_context_lst=auth_context_lst->next) {
		auth_context=(authorization_context_t*)auth_context_lst->data;
		if (strcmp(auth_context->realm,belle_sip_header_www_authenticate_get_realm(authenticate))==0) {
			authorization_context_fill_from_auth(auth_context,authenticate,from_uri);
			if (auth_context_lst) belle_sip_free(auth_context_lst);
			return; /*only one realm is supposed to be found for now*/
		}
	}

	/*no auth context found, creating one*/
	auth_context=belle_sip_authorization_create(call_id);
	belle_sip_debug("belle_sip_provider_auth: no matching auth context, creating one for [realm=%s][user_id=%s][call_id=%s]"
		, belle_sip_header_www_authenticate_get_realm(authenticate)?belle_sip_header_www_authenticate_get_realm(authenticate):"(null)"
		, from_uri?belle_sip_uri_get_user(from_uri):"(null)"
		, call_id?belle_sip_header_call_id_get_call_id(call_id):"(null)");
	authorization_context_fill_from_auth(auth_context,authenticate,from_uri);

	p->auth_contexts=belle_sip_list_append(p->auth_contexts,auth_context);
	if (auth_context_lst) belle_sip_free(auth_context_lst);
	return;
}

int belle_sip_provider_add_authorization(belle_sip_provider_t *p, belle_sip_request_t* request, belle_sip_response_t *resp,
					 belle_sip_uri_t *from_uri, belle_sip_list_t** auth_infos, const char* realm) {
	belle_sip_header_call_id_t* call_id;
	belle_sip_list_t* auth_context_iterator;
	belle_sip_list_t* authenticate_lst;
	belle_sip_list_t* head;
	belle_sip_header_www_authenticate_t* authenticate;
	belle_sip_header_authorization_t* authorization;
	belle_sip_header_from_t* from;
	belle_sip_auth_event_t* auth_event;
	authorization_context_t* auth_context;
	const char* ha1;
	char computed_ha1[33];
	int result=0;
	const char* request_method;
	/*check params*/
	if (!p || !request) {
		belle_sip_error("belle_sip_provider_add_authorization bad parameters");
		return-1;
	}
	request_method=belle_sip_request_get_method(request);

	/*22 Usage of HTTP Authentication
		22.1 Framework
		While a server can legitimately challenge most SIP requests, there
		are two requests defined by this document that require special
		handling for authentication: ACK and CANCEL.
		Under an authentication scheme that uses responses to carry values
		used to compute nonces (such as Digest), some problems come up for
		any requests that take no response, including ACK.  For this reason,
		any credentials in the INVITE that were accepted by a server MUST be
		accepted by that server for the ACK.  UACs creating an ACK message
		will duplicate all of the Authorization and Proxy-Authorization
		header field values that appeared in the INVITE to which the ACK
		corresponds.  Servers MUST NOT attempt to challenge an ACK.

		Although the CANCEL method does take a response (a 2xx), servers MUST
		NOT attempt to challenge CANCEL requests since these requests cannot
		be resubmitted.  Generally, a CANCEL request SHOULD be accepted by a
		server if it comes from the same hop that sent the request being
		canceled (provided that some sort of transport or network layer
		security association, as described in Section 26.2.1, is in place).
	*/

	if (strcmp("CANCEL",request_method)==0 || strcmp("ACK",request_method)==0) {
		belle_sip_debug("no authorization header needed for method [%s]",request_method);
		return 0;
	}

	if (from_uri==NULL){
		from = belle_sip_message_get_header_by_type(request,belle_sip_header_from_t);
		from_uri=belle_sip_header_address_get_uri((belle_sip_header_address_t*)from);
	}

	/*get authenticates value from response*/
	if (resp) {
		belle_sip_list_t *it;
		call_id = belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(resp),belle_sip_header_call_id_t);
		/*searching for authentication headers*/
		authenticate_lst = belle_sip_list_copy(belle_sip_message_get_headers(BELLE_SIP_MESSAGE(resp),BELLE_SIP_WWW_AUTHENTICATE));
		/*search for proxy authenticate*/
		authenticate_lst=belle_sip_list_concat(authenticate_lst,belle_sip_list_copy(belle_sip_message_get_headers(BELLE_SIP_MESSAGE(resp),BELLE_SIP_PROXY_AUTHENTICATE)));
		/*update auth contexts with authenticate headers from response*/
		for (it=authenticate_lst;it!=NULL;it=it->next) {
			authenticate=BELLE_SIP_HEADER_WWW_AUTHENTICATE(it->data);
			belle_sip_provider_update_or_create_auth_context(p,call_id,authenticate,from_uri,realm);
		}
		belle_sip_list_free(authenticate_lst);
	}

	/*put authorization header if passwd found*/
	call_id = belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(request),belle_sip_header_call_id_t);

	belle_sip_debug("belle_sip_provider_auth: looking an auth context for [method=%s][realm=%s][user_id=%s][call_id=%s]"
		, request_method
		, realm?realm:"(null)"
		, from_uri?belle_sip_uri_get_user(from_uri):"(null)"
		, call_id?belle_sip_header_call_id_get_call_id(call_id):"(null)"
	);
	head=belle_sip_provider_get_auth_context_by_realm_or_call_id(p,call_id,from_uri,realm);
	/*we assume there no existing auth headers*/
	for (auth_context_iterator=head;auth_context_iterator!=NULL;auth_context_iterator=auth_context_iterator->next) {
		/*clear auth info*/
		auth_context=(authorization_context_t*)auth_context_iterator->data;
		auth_event = belle_sip_auth_event_create((belle_sip_object_t*)p,auth_context->realm,from_uri);
		/*put data*/
		/*call listener*/
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(p->listeners,process_auth_requested,auth_event);
		if (auth_event->passwd || auth_event->ha1) {
			if (!auth_event->userid) {
				/*if no userid, username = userid*/

				belle_sip_auth_event_set_userid(auth_event,auth_event->username);
			}
			belle_sip_message("Auth info found for [%s] realm [%s]",auth_event->userid,auth_event->realm);
			if (auth_context->is_proxy ||
				(!belle_sip_header_call_id_equals(call_id,auth_context->callid)
						&&realm
						&&strcmp(realm,auth_context->realm)==0
						&&from_uri
						&&strcmp(auth_event->username,belle_sip_uri_get_user(from_uri))==0
						&&strcmp("REGISTER",request_method)!=0)) /* Relying on method name for choosing between authorization and proxy-authorization
						is not very strong but I don't see any better solution as we don't know all the time what type of challange we will have*/{
				authorization=BELLE_SIP_HEADER_AUTHORIZATION(belle_sip_header_proxy_authorization_new());
			} else {
				authorization=belle_sip_header_authorization_new();
			}
			belle_sip_header_authorization_set_scheme(authorization,auth_context->scheme);
			belle_sip_header_authorization_set_realm(authorization,auth_context->realm);
			belle_sip_header_authorization_set_username(authorization,auth_event->userid);
			belle_sip_header_authorization_set_nonce(authorization,auth_context->nonce);
			belle_sip_header_authorization_set_qop(authorization,auth_context->qop);
			belle_sip_header_authorization_set_opaque(authorization,auth_context->opaque);
			belle_sip_header_authorization_set_algorithm(authorization,auth_context->algorithm);
			belle_sip_header_authorization_set_uri(authorization,(belle_sip_uri_t*)belle_sip_request_get_uri(request));
			if (auth_context->qop){
				++auth_context->nonce_count;
				belle_sip_header_authorization_set_nonce_count(authorization,auth_context->nonce_count);
			}

			if (auth_event->ha1) {
				ha1=auth_event->ha1;
			} else {
				belle_sip_auth_helper_compute_ha1(auth_event->userid,auth_context->realm,auth_event->passwd, computed_ha1);
				ha1=computed_ha1;
			}
			if (belle_sip_auth_helper_fill_authorization(authorization
														,belle_sip_request_get_method(request)
														,ha1)) {
				belle_sip_object_unref(authorization);
			} else
				belle_sip_message_add_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_HEADER(authorization));
			result=1;
		} else {
			belle_sip_message("No auth info found for call id [%s]",belle_sip_header_call_id_get_call_id(call_id));
		}
		/*provides auth info in any cases, usefull even if found because auth info can contain wrong password*/
		if (auth_infos) {
			/*stored to give user information on realm/username which requires authentications*/
			*auth_infos=belle_sip_list_append(*auth_infos,auth_event);
		} else {
			belle_sip_auth_event_destroy(auth_event);
		}
	}
	belle_sip_list_free(head);
	return result;
}

void belle_sip_provider_set_recv_error(belle_sip_provider_t *prov, int recv_error) {
	belle_sip_list_t *lps;
	belle_sip_list_t *channels;
	for(lps=prov->lps;lps!=NULL;lps=lps->next){
		for(channels=((belle_sip_listening_point_t*)lps->data)->channels;channels!=NULL;channels=channels->next){
			((belle_sip_channel_t*)channels->data)->simulated_recv_return=recv_error;
			((belle_sip_source_t*)channels->data)->notify_required=(recv_error<=0);
		}
	}
}
void belle_sip_provider_enable_rport(belle_sip_provider_t *prov, int enable) {
	prov->rport_enabled=enable;
}

int belle_sip_provider_is_rport_enabled(belle_sip_provider_t *prov) {
	return prov->rport_enabled;
}

void belle_sip_provider_enable_nat_helper(belle_sip_provider_t *prov, int enabled){
	prov->nat_helper=enabled;
}

int belle_sip_provider_nat_helper_enabled(const belle_sip_provider_t *prov){
	return prov->nat_helper;
}
void belle_sip_provider_enable_unconditional_answer(belle_sip_provider_t *prov, int enable) {
	prov->unconditional_answer_enabled=enable;
}
void belle_sip_provider_set_unconditional_answer(belle_sip_provider_t *prov, unsigned short code) {
	prov->unconditional_answer=code;
}
