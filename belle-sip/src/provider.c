/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
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

belle_sip_dialog_t *belle_sip_provider_find_dialog_from_msg(belle_sip_provider_t *prov, belle_sip_request_t *msg, int as_uas);

typedef struct authorization_context {
	belle_sip_header_call_id_t* callid;
	const char* scheme;
	const char* realm;
	const char* nonce;
	const char* qop;
	const char* opaque;
	int nonce_count;
	int is_proxy;
}authorization_context_t;

GET_SET_STRING(authorization_context,realm)
GET_SET_STRING(authorization_context,nonce)
GET_SET_STRING(authorization_context,qop)
GET_SET_STRING(authorization_context,scheme)
GET_SET_STRING(authorization_context,opaque)
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
	belle_sip_object_unref(object->callid);
	belle_sip_free(object);
}

static void belle_sip_provider_uninit(belle_sip_provider_t *p){
	p->listeners=belle_sip_list_free(p->listeners);
	p->internal_listeners=belle_sip_list_free(p->internal_listeners);
	p->client_transactions=belle_sip_list_free_with_data(p->client_transactions,belle_sip_object_unref);
	p->server_transactions=belle_sip_list_free_with_data(p->server_transactions,belle_sip_object_unref);
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

		ev.dialog=belle_sip_provider_find_dialog_from_msg(prov,req,1/*request=uas*/);
		if (ev.dialog){
			if (strcmp("ACK",method)==0){
				if (belle_sip_dialog_handle_ack(ev.dialog,req)==-1){
					/*absorbed ACK retransmission, ignore */
					return;
				}
			}else if (!belle_sip_dialog_is_authorized_transaction(ev.dialog,method)){
				belle_sip_server_transaction_t *tr=belle_sip_provider_create_server_transaction(prov,req);
				belle_sip_server_transaction_send_response(tr,
					belle_sip_response_create_from_request(req,491));
				return;
			}
		}
		if (prov->unconditional_answer_enabled && strcmp("ACK",method)!=0) { /*always answer 480 in this case*/
			belle_sip_server_transaction_t *tr=belle_sip_provider_create_server_transaction(prov,req);
			belle_sip_server_transaction_send_response(tr,belle_sip_response_create_from_request(req,480));
			return;
		} else {
			ev.source=prov;
			ev.server_transaction=NULL;
			ev.request=req;
			BELLE_SIP_PROVIDER_INVOKE_LISTENERS(prov->listeners,process_request_event,&ev);
		}
	}
}

static belle_sip_list_t*  belle_sip_provider_get_auth_context_by_call_id(belle_sip_provider_t *p,belle_sip_header_call_id_t* call_id);

static void belle_sip_provider_dispatch_response(belle_sip_provider_t* prov, belle_sip_response_t *msg){
	belle_sip_client_transaction_t *t;
	t=belle_sip_provider_find_matching_client_transaction(prov,msg);

	/*good opportunity to cleanup auth context if answer = 401|407|403*/

	switch (belle_sip_response_get_status_code(msg)) {
	case 401:
	case 403:
	case 407: {
		belle_sip_header_call_id_t* call_id=call_id = belle_sip_message_get_header_by_type(msg,belle_sip_header_call_id_t);
		belle_sip_list_t* iterator;
		belle_sip_list_t* head=belle_sip_provider_get_auth_context_by_call_id(prov,call_id);
		for (iterator=head;iterator!=NULL;iterator=iterator->next){
			prov->auth_contexts=belle_sip_list_remove(prov->auth_contexts,iterator->data);
		}
		belle_sip_list_free_with_data(head,(void (*)(void *))belle_sip_authorization_destroy);
	}
	}
	/*
	 * If a transaction is found, pass it to the transaction and let it decide what to do.
	 * Else notifies directly.
	 */
	if (t){
		/*since the add_response may indirectly terminate the transaction, we need to guarantee the transaction is not freed
		 * until full completion*/
		belle_sip_object_ref(t);
		belle_sip_client_transaction_add_response(t,msg);
		belle_sip_object_unref(t);
	}else{
		belle_sip_response_event_t event;
		event.source=prov;
		event.client_transaction=NULL;
		event.dialog=NULL;
		event.response=msg;
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(prov->listeners,process_response_event,&event);
	}
}

void belle_sip_provider_dispatch_message(belle_sip_provider_t *prov, belle_sip_message_t *msg){
	if (belle_sip_message_check_headers(msg)){
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
	unsigned int cseq=belle_sip_header_cseq_get_seq_number(belle_sip_message_get_header_by_type(msg,belle_sip_header_cseq_t));
	char tmp[256]={0};
	uint8_t digest[16];
	const char*callid=belle_sip_header_call_id_get_call_id(belle_sip_message_get_header_by_type(msg,belle_sip_header_call_id_t));
	const char *from_tag=belle_sip_header_from_get_tag(belle_sip_message_get_header_by_type(msg,belle_sip_header_from_t));
	const char *to_tag=belle_sip_header_to_get_tag(belle_sip_message_get_header_by_type(msg,belle_sip_header_to_t));
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
		belle_sip_md5_append(&ctx,(uint8_t*)initial,strlen(initial));
	if (requri){
		size_t offset=0;
		belle_sip_object_marshal((belle_sip_object_t*)requri,tmp,sizeof(tmp)-1,&offset);
		belle_sip_md5_append(&ctx,(uint8_t*)tmp,strlen(tmp));
	}
	if (from_tag)
		belle_sip_md5_append(&ctx,(uint8_t*)from_tag,strlen(from_tag));
	if (to_tag)
		belle_sip_md5_append(&ctx,(uint8_t*)to_tag,strlen(to_tag));
	belle_sip_md5_append(&ctx,(uint8_t*)callid,strlen(callid));
	belle_sip_md5_append(&ctx,(uint8_t*)&cseq,sizeof(cseq));
	if (is_request){
		if (prev_via){
			size_t offset=0;
			belle_sip_object_marshal((belle_sip_object_t*)prev_via,tmp,sizeof(tmp)-1,&offset);
			belle_sip_md5_append(&ctx,(uint8_t*)tmp,offset);
		}
	}else{
		if (via){
			size_t offset=0;
			belle_sip_object_marshal((belle_sip_object_t*)via,tmp,sizeof(tmp)-1,&offset);
			belle_sip_md5_append(&ctx,(uint8_t*)tmp,offset);
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
	
	belle_sip_md5_append(&ctx,(uint8_t*)from_str,strlen(from_str));
	belle_sip_md5_append(&ctx,(uint8_t*)to_str,strlen(to_str));
	belle_sip_md5_append(&ctx,(uint8_t*)callid,strlen(callid));
	belle_sip_md5_append(&ctx,(uint8_t*)&cseq,sizeof(cseq));
	belle_sip_free(from_str);
	belle_sip_free(to_str);
	
	if (v_branch)
		belle_sip_md5_append(&ctx,(uint8_t*)v_branch,strlen(v_branch));

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

static int channel_on_event(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, unsigned int revents){
	if (revents & BELLE_SIP_EVENT_READ){
		belle_sip_message_t *msg;
		while ((msg=belle_sip_channel_pick_message(chan)))
			belle_sip_provider_dispatch_message(BELLE_SIP_PROVIDER(obj),msg);
	}
	return 0;
}

static int channel_on_auth_requested(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, const char* distinguished_name){
	if (BELLE_SIP_IS_INSTANCE_OF(chan,belle_sip_tls_channel_t)) {
		belle_sip_provider_t *prov=BELLE_SIP_PROVIDER(obj);
		belle_sip_auth_event_t* auth_event = belle_sip_auth_event_create(NULL,NULL);
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
	belle_sip_header_content_length_t* content_lenght = (belle_sip_header_content_length_t*)belle_sip_message_get_header(msg,"Content-Length");
	belle_sip_uri_t* contact_uri;
	const belle_sip_list_t *contacts;
	const char *ip=NULL;
	int port=0;
	belle_sip_provider_t *prov=BELLE_SIP_PROVIDER(obj);

	if (belle_sip_message_is_request(msg)){
		/*probably better to be in channel*/
		fix_outgoing_via((belle_sip_provider_t*)obj,chan,msg);
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
		if (strcmp(transport,"udp")==0){
			belle_sip_parameters_remove_parameter(BELLE_SIP_PARAMETERS(contact_uri),"transport");
		}else{
			belle_sip_uri_set_transport_param(contact_uri,transport);
		}
		if (port!=belle_sip_listening_point_get_well_known_port(transport)) {
			belle_sip_uri_set_port(contact_uri,port);
		}else{
			belle_sip_uri_set_port(contact_uri,0);
		}
	}
	
	if (!content_lenght && strcasecmp("udp",belle_sip_channel_get_transport_name(chan))!=0) {
		content_lenght = belle_sip_header_content_length_create(0);
		belle_sip_message_add_header(msg,(belle_sip_header_t*)content_lenght);
	}
}

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_sip_provider_t,belle_sip_channel_listener_t)
	channel_state_changed,
	channel_on_event,
	channel_on_sending,
	channel_on_auth_requested
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_sip_provider_t,belle_sip_channel_listener_t);
	
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_provider_t,belle_sip_object_t,belle_sip_provider_uninit,NULL,NULL,FALSE);

belle_sip_provider_t *belle_sip_provider_new(belle_sip_stack_t *s, belle_sip_listening_point_t *lp){
	belle_sip_provider_t *p=belle_sip_object_new(belle_sip_provider_t);
	p->stack=s;
	p->rport_enabled=1;
	if (lp) belle_sip_provider_add_listening_point(p,lp);
	return p;
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

/*finds an existing dialog for an outgoing or incoming request */
belle_sip_dialog_t *belle_sip_provider_find_dialog_from_msg(belle_sip_provider_t *prov, belle_sip_request_t *msg, int as_uas){
	belle_sip_list_t *elem;
	belle_sip_dialog_t *dialog;
	belle_sip_dialog_t *returned_dialog=NULL;
	belle_sip_header_call_id_t *call_id;
	belle_sip_header_from_t *from;
	belle_sip_header_to_t *to;
	const char *from_tag;
	const char *to_tag;
	const char *call_id_value;
	const char *local_tag,*remote_tag;
	
	if (msg->dialog){
		return msg->dialog;
	}
	
	to=belle_sip_message_get_header_by_type(msg,belle_sip_header_to_t);
	
	if (to==NULL || (to_tag=belle_sip_header_to_get_tag(to))==NULL){
		/* a request without to tag cannot be part of a dialog */
		return NULL;
	}
	
	call_id=belle_sip_message_get_header_by_type(msg,belle_sip_header_call_id_t);
	from=belle_sip_message_get_header_by_type(msg,belle_sip_header_from_t);

	if (call_id==NULL || from==NULL) return NULL;

	call_id_value=belle_sip_header_call_id_get_call_id(call_id);
	from_tag=belle_sip_header_from_get_tag(from);
	
	local_tag=as_uas ? to_tag : from_tag;
	remote_tag=as_uas ? from_tag : to_tag;
	
	for (elem=prov->dialogs;elem!=NULL;elem=elem->next){
		dialog=(belle_sip_dialog_t*)elem->data;
		/*ignore dialog in state BELLE_SIP_DIALOG_NULL, is it really the correct things to do*/
		if (belle_sip_dialog_get_state(dialog) != BELLE_SIP_DIALOG_NULL && _belle_sip_dialog_match(dialog,call_id_value,local_tag,remote_tag)) {
			if (!returned_dialog)
				returned_dialog=dialog;
			else
				belle_sip_fatal("More than 1 dialog is matching, check your app");
		}
	}
	return returned_dialog;
}

void belle_sip_provider_add_dialog(belle_sip_provider_t *prov, belle_sip_dialog_t *dialog){
	prov->dialogs=belle_sip_list_prepend(prov->dialogs,belle_sip_object_ref(dialog));
}

static void notify_dialog_terminated(belle_sip_dialog_terminated_event_t* ev) {
	BELLE_SIP_PROVIDER_INVOKE_LISTENERS(((belle_sip_provider_t*)ev->source)->listeners,process_dialog_terminated,ev);
	belle_sip_object_unref(ev->dialog);
	belle_sip_free(ev);
}

void belle_sip_provider_remove_dialog(belle_sip_provider_t *prov, belle_sip_dialog_t *dialog){
	belle_sip_dialog_terminated_event_t* ev=belle_sip_malloc(sizeof(belle_sip_dialog_terminated_event_t));
	ev->source=prov;
	ev->dialog=dialog;
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
	belle_sip_transaction_set_dialog((belle_sip_transaction_t*)t,belle_sip_provider_find_dialog_from_msg(prov,req,FALSE));
	belle_sip_request_set_dialog(req,NULL);/*get rid of the reference to the dialog, which is no longer needed in the message.
					This is to avoid circular references.*/
	return t;
}

belle_sip_server_transaction_t *belle_sip_provider_create_server_transaction(belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_server_transaction_t* t;
	if (strcmp(belle_sip_request_get_method(req),"INVITE")==0){
		t=(belle_sip_server_transaction_t*)belle_sip_ist_new(prov,req);
	}else if (strcmp(belle_sip_request_get_method(req),"ACK")==0){
		belle_sip_error("Creating a server transaction for an ACK is not a good idea, probably");
		return NULL;
	}else 
		t=(belle_sip_server_transaction_t*)belle_sip_nist_new(prov,req);
	belle_sip_transaction_set_dialog((belle_sip_transaction_t*)t,belle_sip_provider_find_dialog_from_msg(prov,req,TRUE));
	belle_sip_provider_add_server_transaction(prov,t);
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


static void authorization_context_fill_from_auth(authorization_context_t* auth_context,belle_sip_header_www_authenticate_t* authenticate) {
	authorization_context_set_realm(auth_context,belle_sip_header_www_authenticate_get_realm(authenticate));
	if (auth_context->nonce && strcmp(belle_sip_header_www_authenticate_get_nonce(authenticate),auth_context->nonce)!=0) {
		/*new nonce, resetting nounce_count*/
		auth_context->nonce_count=0;
	}
	authorization_context_set_nonce(auth_context,belle_sip_header_www_authenticate_get_nonce(authenticate));
	authorization_context_set_qop(auth_context,belle_sip_header_www_authenticate_get_qop_first(authenticate));
	authorization_context_set_scheme(auth_context,belle_sip_header_www_authenticate_get_scheme(authenticate));
	authorization_context_set_opaque(auth_context,belle_sip_header_www_authenticate_get_opaque(authenticate));
	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(authenticate,belle_sip_header_proxy_authenticate_t)) {
		auth_context->is_proxy=1;
	}
}

static belle_sip_list_t*  belle_sip_provider_get_auth_context_by_call_id(belle_sip_provider_t *p,belle_sip_header_call_id_t* call_id) {
	belle_sip_list_t* auth_context_lst=NULL;
	belle_sip_list_t* result=NULL;
	authorization_context_t* auth_context;
	for (auth_context_lst=p->auth_contexts;auth_context_lst!=NULL;auth_context_lst=auth_context_lst->next) {
		auth_context=(authorization_context_t*)auth_context_lst->data;
		if (belle_sip_header_call_id_equals(auth_context->callid,call_id) ) {
			result=belle_sip_list_append(result,auth_context_lst->data);
		}
	}
	return result;
}

static void  belle_sip_provider_update_or_create_auth_context(belle_sip_provider_t *p,belle_sip_header_call_id_t* call_id,belle_sip_header_www_authenticate_t* authenticate) {
	 belle_sip_list_t* auth_context_lst =  belle_sip_provider_get_auth_context_by_call_id(p,call_id);
	 authorization_context_t* auth_context;
	 for (;auth_context_lst!=NULL;auth_context_lst=auth_context_lst->next) {
		 auth_context= (authorization_context_t*)auth_context_lst->data;
		 if (strcmp(auth_context->realm,belle_sip_header_www_authenticate_get_realm(authenticate))==0) {
			 authorization_context_fill_from_auth(auth_context,authenticate);
			 if (auth_context_lst) belle_sip_free(auth_context_lst);
			 return; /*only one realm is supposed to be found for now*/
		 }
	 }
	 /*no auth context found, creating one*/
	 auth_context=belle_sip_authorization_create(call_id);
	 authorization_context_fill_from_auth(auth_context,authenticate);
	 p->auth_contexts=belle_sip_list_append(p->auth_contexts,auth_context);
	 if (auth_context_lst) belle_sip_free(auth_context_lst);
	 return;
}

int belle_sip_provider_add_authorization(belle_sip_provider_t *p, belle_sip_request_t* request,belle_sip_response_t *resp,belle_sip_list_t** auth_infos) {
	belle_sip_header_call_id_t* call_id;
	belle_sip_list_t* auth_context_lst;
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
			belle_sip_provider_update_or_create_auth_context(p,call_id,authenticate);
		}
		belle_sip_list_free(authenticate_lst);
	}

	/*put authorization header if passwd found*/
	call_id = belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(request),belle_sip_header_call_id_t);
	from = belle_sip_message_get_header_by_type(request,belle_sip_header_from_t);
	if ((head=auth_context_lst = belle_sip_provider_get_auth_context_by_call_id(p,call_id))) {
		/*we assume there no existing auth headers*/
		for (;auth_context_lst!=NULL;auth_context_lst=auth_context_lst->next) {
			/*clear auth info*/
			auth_context=(authorization_context_t*)auth_context_lst->data;
			auth_event = belle_sip_auth_event_create(auth_context->realm,from);
			/*put data*/
			/*call listener*/
			BELLE_SIP_PROVIDER_INVOKE_LISTENERS(p->listeners,process_auth_requested,auth_event);
			if (auth_event->passwd || auth_event->ha1) {
				if (!auth_event->userid) {
					/*if no userid, username = userid*/

					belle_sip_auth_event_set_userid(auth_event,(const char*)auth_event->username);
				}
				belle_sip_message("Auth info found for [%s] realm [%s]",auth_event->userid,auth_event->realm);
				if (auth_context->is_proxy) {
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
				belle_sip_header_authorization_set_uri(authorization,(belle_sip_uri_t*)belle_sip_request_get_uri(request));
				if (auth_context->qop)
					belle_sip_header_authorization_set_nonce_count(authorization,++auth_context->nonce_count);
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
		/*provides auth info in any cases, uasefull even if found because auth info can contains wrong password*/
		if (auth_infos) {
			/*stored to give user information on realm/username which requires authentications*/
			*auth_infos=belle_sip_list_append(*auth_infos,auth_event);
		} else {
			belle_sip_auth_event_destroy(auth_event);
		}

		}
		belle_sip_list_free(head);
	} else {
		/*nothing to do*/
	}
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
