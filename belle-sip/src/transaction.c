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

const char *belle_sip_transaction_state_to_string(belle_sip_transaction_state_t state){
	switch(state){
		case BELLE_SIP_TRANSACTION_INIT:
			return "INIT";
		case BELLE_SIP_TRANSACTION_TRYING:
			return "TRYING";
		case BELLE_SIP_TRANSACTION_CALLING:
			return "CALLING";
		case BELLE_SIP_TRANSACTION_COMPLETED:
			return "COMPLETED";
		case BELLE_SIP_TRANSACTION_CONFIRMED:
			return "CONFIRMED";
		case BELLE_SIP_TRANSACTION_ACCEPTED:
			return "ACCEPTED";
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			return "PROCEEDING";
		case BELLE_SIP_TRANSACTION_TERMINATED:
			return "TERMINATED";
	}
	belle_sip_fatal("Invalid transaction state.");
	return "INVALID";
}

void belle_sip_transaction_set_state(belle_sip_transaction_t *t, belle_sip_transaction_state_t state) {
	belle_sip_message("Changing [%s] [%s] transaction [%p], from state [%s] to [%s]",
				BELLE_SIP_OBJECT_IS_INSTANCE_OF(t,belle_sip_client_transaction_t) ? "client" : "server",
				belle_sip_request_get_method(t->request),
				t,
				belle_sip_transaction_state_to_string(t->state),
				belle_sip_transaction_state_to_string(state));
	t->state=state;
}
static void belle_sip_transaction_init(belle_sip_transaction_t *t, belle_sip_provider_t *prov, belle_sip_request_t *req){
	t->request=(belle_sip_request_t*)belle_sip_object_ref(req);
	t->provider=prov;
}

static void transaction_destroy(belle_sip_transaction_t *t){
	if (t->request) belle_sip_object_unref(t->request);
	if (t->last_response) belle_sip_object_unref(t->last_response);
	if (t->channel) belle_sip_object_unref(t->channel);
	if (t->branch_id) belle_sip_free(t->branch_id);
	belle_sip_transaction_set_dialog(t,NULL);

}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_transaction_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_transaction_t)={
	{
		BELLE_SIP_VPTR_INIT(belle_sip_transaction_t,belle_sip_object_t,FALSE),
		(belle_sip_object_destroy_t) transaction_destroy,
		NULL,/*no clone*/
		NULL,/*no marshal*/
	},
	NULL /*on_terminate*/
};

void *belle_sip_transaction_get_application_data(const belle_sip_transaction_t *t){
	return t->appdata;
}

void belle_sip_transaction_set_application_data(belle_sip_transaction_t *t, void *data){
	t->appdata=data;
}

const char *belle_sip_transaction_get_branch_id(const belle_sip_transaction_t *t){
	return t->branch_id;
}

belle_sip_transaction_state_t belle_sip_transaction_get_state(const belle_sip_transaction_t *t){
	return t->state;
}

int belle_sip_transaction_state_is_transient(const belle_sip_transaction_state_t state) {
	switch(state){
		case BELLE_SIP_TRANSACTION_INIT:
		case BELLE_SIP_TRANSACTION_TRYING:
		case BELLE_SIP_TRANSACTION_CALLING:
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			return 1;
		default:
			return 0;
	}
}

void belle_sip_transaction_terminate(belle_sip_transaction_t *t){
	if (belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(t))!=BELLE_SIP_TRANSACTION_TERMINATED) {
		belle_sip_transaction_set_state(t,BELLE_SIP_TRANSACTION_TERMINATED);
		belle_sip_message("%s%s %s transaction [%p] terminated"	,BELLE_SIP_OBJECT_IS_INSTANCE_OF(t,belle_sip_client_transaction_t)?"Client":"Server"
									,t->is_internal?" internal":""
									,belle_sip_request_get_method(belle_sip_transaction_get_request(t))
									,t);
		BELLE_SIP_OBJECT_VPTR(t,belle_sip_transaction_t)->on_terminate(t);
		belle_sip_provider_set_transaction_terminated(t->provider,t);
	}
}

belle_sip_request_t *belle_sip_transaction_get_request(const belle_sip_transaction_t *t){
	return t->request;
}
belle_sip_response_t *belle_sip_transaction_get_response(const belle_sip_transaction_t *t) {
	return t->last_response;
}

static void notify_timeout(belle_sip_transaction_t *t){
	belle_sip_timeout_event_t ev;
	ev.source=t->provider;
	ev.transaction=t;
	ev.is_server_transaction=BELLE_SIP_OBJECT_IS_INSTANCE_OF(t,belle_sip_server_transaction_t);
	BELLE_SIP_PROVIDER_INVOKE_LISTENERS_FOR_TRANSACTION(t,process_timeout,&ev);
}

void belle_sip_transaction_notify_timeout(belle_sip_transaction_t *t){
	/*report the channel as possibly dead. If an alternate IP can be tryied, the channel will notify us with the RETRY state.
	 * Otherwise it will report the error.
	 * We limit this dead channel reporting to REGISTER transactions, who are unlikely to be unresponded.
	**/
	belle_sip_warning("Transaction [%p] reporting timeout, reporting to channel.",t);
	
	if (strcmp(belle_sip_request_get_method(t->request),"REGISTER")==0 && belle_sip_channel_notify_timeout(t->channel)==TRUE){
		t->timed_out=TRUE;
	}else {
		notify_timeout(t);
		belle_sip_transaction_terminate(t);
	}
}

belle_sip_dialog_t*  belle_sip_transaction_get_dialog(const belle_sip_transaction_t *t) {
	return t->dialog;
}

static void belle_sip_transaction_reset_dialog(belle_sip_transaction_t *tr, belle_sip_dialog_t *dialog_disapeearing){
	if (tr->dialog!=dialog_disapeearing) belle_sip_error("belle_sip_transaction_reset_dialog(): inconsistency.");
	tr->dialog=NULL;
}

void belle_sip_transaction_set_dialog(belle_sip_transaction_t *t, belle_sip_dialog_t *dialog){
	if (dialog) belle_sip_object_weak_ref(dialog,(belle_sip_object_destroy_notify_t)belle_sip_transaction_reset_dialog,t);
	if (t->dialog) belle_sip_object_weak_unref(t->dialog,(belle_sip_object_destroy_notify_t)belle_sip_transaction_reset_dialog,t);
	t->dialog=dialog;
}

/*
 * Server transaction
 */

static void server_transaction_destroy(belle_sip_server_transaction_t *t){
}


BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_server_transaction_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_server_transaction_t)={
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_server_transaction_t,belle_sip_transaction_t,FALSE),
			(belle_sip_object_destroy_t) server_transaction_destroy,
			NULL,
			NULL
		},
		NULL
	}
};

void belle_sip_server_transaction_init(belle_sip_server_transaction_t *t, belle_sip_provider_t *prov,belle_sip_request_t *req){
	const char *branch;
	belle_sip_header_via_t *via=BELLE_SIP_HEADER_VIA(belle_sip_message_get_header((belle_sip_message_t*)req,"via"));
	
	branch=belle_sip_header_via_get_branch(via);
	if (branch==NULL){
		branch=req->rfc2543_branch;
		if (branch==NULL) belle_sip_fatal("No computed branch for RFC2543 style of message, this should never happen."); 
	}
	t->base.branch_id=belle_sip_strdup(branch);
	belle_sip_transaction_init((belle_sip_transaction_t*)t,prov,req);
	belle_sip_random_token(t->to_tag,sizeof(t->to_tag));
}

void belle_sip_server_transaction_send_response(belle_sip_server_transaction_t *t, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)t;
	belle_sip_header_to_t *to=(belle_sip_header_to_t*)belle_sip_message_get_header((belle_sip_message_t*)resp,"to");
	belle_sip_dialog_t *dialog=base->dialog;
	int status_code;
	
	belle_sip_object_ref(resp);
	if (!base->last_response){
		belle_sip_hop_t* hop=belle_sip_response_get_return_hop(resp);
		base->channel=belle_sip_provider_get_channel(base->provider,hop);
		belle_sip_object_unref(hop);
		if (!base->channel){
			belle_sip_error("Transaction [%p]: No channel available for sending response.",t);
			return;
		}
		belle_sip_object_ref(base->channel);
	}
	status_code=belle_sip_response_get_status_code(resp);
	if (status_code!=100){
		if (belle_sip_header_to_get_tag(to)==NULL){
			//add a random to tag
			belle_sip_header_to_set_tag(to,t->to_tag);
		}
		/*12.1 Creation of a Dialog
		   Dialogs are created through the generation of non-failure responses
		   to requests with specific methods.  Within this specification, only
		   2xx and 101-199 responses with a To tag, where the request was
		   INVITE, will establish a dialog.*/
		if (dialog && status_code>100 && status_code<300){
			belle_sip_response_fill_for_dialog(resp,base->request);
		}
	}
	if (BELLE_SIP_OBJECT_VPTR(t,belle_sip_server_transaction_t)->send_new_response(t,resp)==0){
		if (base->last_response)
			belle_sip_object_unref(base->last_response);
		base->last_response=resp;
	}
	if (dialog)
		belle_sip_dialog_update(dialog,BELLE_SIP_TRANSACTION(t),TRUE);
}

static void server_transaction_notify(belle_sip_server_transaction_t *t, belle_sip_request_t *req, belle_sip_dialog_t *dialog){
	belle_sip_request_event_t event;

	event.source=t->base.provider;
	event.server_transaction=t;
	event.dialog=dialog;
	event.request=req;
	BELLE_SIP_PROVIDER_INVOKE_LISTENERS_FOR_TRANSACTION(((belle_sip_transaction_t*) t),process_request_event,&event);
}

void belle_sip_server_transaction_on_request(belle_sip_server_transaction_t *t, belle_sip_request_t *req){
	const char *method=belle_sip_request_get_method(req);
	if (strcmp(method,"ACK")==0){
		/*this must be for an INVITE server transaction */
		if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(t,belle_sip_ist_t)){
			belle_sip_ist_t *ist=(belle_sip_ist_t*)t;
			if (belle_sip_ist_process_ack(ist,(belle_sip_message_t*)req)==0){
				belle_sip_dialog_t *dialog=t->base.dialog;
				if (dialog && belle_sip_dialog_handle_ack(dialog,req)==0)
					server_transaction_notify(t,req,dialog);
				/*else nothing to do because retransmission of ACK*/

			}
		}else{
			belle_sip_warning("ACK received for non-invite server transaction ?");
		}
	}else if (strcmp(method,"CANCEL")==0){
		server_transaction_notify(t,req,t->base.dialog);
	}else
		BELLE_SIP_OBJECT_VPTR(t,belle_sip_server_transaction_t)->on_request_retransmission(t);
}

/*
 * client transaction
 */



belle_sip_request_t * belle_sip_client_transaction_create_cancel(belle_sip_client_transaction_t *t){
	belle_sip_message_t *orig=(belle_sip_message_t*)t->base.request;
	belle_sip_request_t *req;
	const char *orig_method=belle_sip_request_get_method((belle_sip_request_t*)orig);
	if (strcmp(orig_method,"ACK")==0 || strcmp(orig_method,"INVITE")!=0){
		belle_sip_error("belle_sip_client_transaction_create_cancel() cannot be used for ACK or non-INVITE transactions.");
		return NULL;
	}
	if (t->base.state!=BELLE_SIP_TRANSACTION_PROCEEDING){
		belle_sip_error("belle_sip_client_transaction_create_cancel() can only be used in state BELLE_SIP_TRANSACTION_PROCEEDING"
		               " but current transaction state is %s",belle_sip_transaction_state_to_string(t->base.state));
		return NULL;
	}
	req=belle_sip_request_new();
	belle_sip_request_set_method(req,"CANCEL");
/*	9.1 Client Behavior
	   Since requests other than INVITE are responded to immediately,
	   sending a CANCEL for a non-INVITE request would always create a
	   race condition.
	   The following procedures are used to construct a CANCEL request.  The
	   Request-URI, Call-ID, To, the numeric part of CSeq, and From header
	   fields in the CANCEL request MUST be identical to those in the
	   request being cancelled, including tags.  A CANCEL constructed by a
	   client MUST have only a single Via header field value matching the
	   top Via value in the request being cancelled.*/
	belle_sip_request_set_uri(req,(belle_sip_uri_t*)belle_sip_object_clone((belle_sip_object_t*)belle_sip_request_get_uri((belle_sip_request_t*)orig)));
	belle_sip_util_copy_headers(orig,(belle_sip_message_t*)req,"via",FALSE);
	belle_sip_util_copy_headers(orig,(belle_sip_message_t*)req,"call-id",FALSE);
	belle_sip_util_copy_headers(orig,(belle_sip_message_t*)req,"from",FALSE);
	belle_sip_util_copy_headers(orig,(belle_sip_message_t*)req,"to",FALSE);
	belle_sip_util_copy_headers(orig,(belle_sip_message_t*)req,"route",TRUE);
	belle_sip_message_add_header((belle_sip_message_t*)req,
		(belle_sip_header_t*)belle_sip_header_cseq_create(
			belle_sip_header_cseq_get_seq_number((belle_sip_header_cseq_t*)belle_sip_message_get_header(orig,"cseq")),
		    "CANCEL"));
	return req;
}


int belle_sip_client_transaction_send_request(belle_sip_client_transaction_t *t){
	return belle_sip_client_transaction_send_request_to(t,NULL);

}

int belle_sip_client_transaction_send_request_to(belle_sip_client_transaction_t *t,belle_sip_uri_t* outbound_proxy) {
	belle_sip_channel_t *chan;
	belle_sip_provider_t *prov=t->base.provider;
	belle_sip_dialog_t *dialog=t->base.dialog;
	int result=-1;
	
	if (t->base.state!=BELLE_SIP_TRANSACTION_INIT){
		belle_sip_error("belle_sip_client_transaction_send_request: bad state.");
		return -1;
	}

	/*check uris components compliance*/
	if (!belle_sip_request_check_uris_components(t->base.request)) {
		belle_sip_error("belle_sip_client_transaction_send_request: bad request for transaction [%p]",t);
		return -1;
	}
	/*store preset route for future use by refresher*/
	if (outbound_proxy){
		t->preset_route=outbound_proxy;
		belle_sip_object_ref(t->preset_route);
	}
	
	if (t->base.request->dialog_queued){
		/*this request was created by belle_sip_dialog_create_queued_request().*/
		if (belle_sip_dialog_request_pending(dialog)){
			/*it cannot be sent immediately, queue the transaction into dialog*/
			belle_sip_message("belle_sip_client_transaction_send_request(): transaction [%p], cannot send request now because dialog is busy, so queuing into dialog.",t);
			belle_sip_dialog_queue_client_transaction(dialog,t);
			return 0;
		}else{
			belle_sip_request_t *req=t->base.request;
			/*it can be sent immediately, so update the request with latest cseq and route_set */
			/*update route and contact just in case they changed*/
			belle_sip_dialog_update_request(dialog,req);
		}
	}
	
	if (dialog){
		belle_sip_dialog_update(dialog,(belle_sip_transaction_t*)t,BELLE_SIP_OBJECT_IS_INSTANCE_OF(t,belle_sip_server_transaction_t));
	}
	
	if (!t->next_hop) {
		if (t->preset_route) {
			t->next_hop=belle_sip_hop_new_from_uri(t->preset_route);
		} else {
			t->next_hop = belle_sip_stack_get_next_hop(prov->stack,t->base.request);
		}
		belle_sip_object_ref(t->next_hop);
	} else {
		/*next hop already preset, probably in case of CANCEL*/
	}
	belle_sip_provider_add_client_transaction(t->base.provider,t); /*add it in any case*/
	chan=belle_sip_provider_get_channel(prov,t->next_hop);
	if (chan){
		belle_sip_object_ref(chan);
		belle_sip_channel_add_listener(chan,BELLE_SIP_CHANNEL_LISTENER(t));
		t->base.channel=chan;
		if (belle_sip_channel_get_state(chan)==BELLE_SIP_CHANNEL_INIT){
			belle_sip_message("belle_sip_client_transaction_send_request(): waiting channel to be ready");
			belle_sip_channel_prepare(chan);
			/*the channel will notify us when it is ready*/
		} else if (belle_sip_channel_get_state(chan)==BELLE_SIP_CHANNEL_READY){
			/*otherwise we can send immediately*/
			BELLE_SIP_OBJECT_VPTR(t,belle_sip_client_transaction_t)->send_request(t);
		}
		result=0;
	}else {
		belle_sip_error("belle_sip_client_transaction_send_request(): no channel available");
		belle_sip_transaction_terminate(BELLE_SIP_TRANSACTION(t));
		result=-1;
	}
	return result;
}

static unsigned int should_dialog_be_created(belle_sip_client_transaction_t *t, belle_sip_response_t *resp){
	belle_sip_request_t* req = belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(t));
	const char* method = belle_sip_request_get_method(req);
	int status_code = belle_sip_response_get_status_code(resp);
	return status_code>=180 && status_code<300 && (strcmp(method,"INVITE")==0 || strcmp(method,"SUBSCRIBE")==0);
}

void belle_sip_client_transaction_notify_response(belle_sip_client_transaction_t *t, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)t;
	belle_sip_request_t* req = belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(t));
	const char* method = belle_sip_request_get_method(req);
	belle_sip_response_event_t event;
	belle_sip_dialog_t *dialog=base->dialog;
	int status_code =  belle_sip_response_get_status_code(resp);
	if (base->last_response)
		belle_sip_object_unref(base->last_response);
	base->last_response=(belle_sip_response_t*)belle_sip_object_ref(resp);

	if (dialog){
		if (status_code>=200 && status_code<300
			&& strcmp(method,"INVITE")==0
			&& (dialog->state==BELLE_SIP_DIALOG_EARLY || dialog->state==BELLE_SIP_DIALOG_CONFIRMED)){
			/*make sure this response matches the current dialog, or creates a new one*/
			if (!belle_sip_dialog_match(dialog,(belle_sip_message_t*)resp,FALSE)){
				dialog=belle_sip_provider_create_dialog_internal(t->base.provider,BELLE_SIP_TRANSACTION(t),FALSE);/*belle_sip_dialog_new(base);*/
				if (dialog){
					/*copy userdata to avoid application from being lost*/
					belle_sip_dialog_set_application_data(dialog,belle_sip_dialog_get_application_data(base->dialog));
					belle_sip_message("Handling response creating a new dialog !");
				}
			}
		}
	} else if (should_dialog_be_created(t,resp)) {
		dialog=belle_sip_provider_create_dialog_internal(t->base.provider,BELLE_SIP_TRANSACTION(t),FALSE);
	}

	if (dialog && belle_sip_dialog_update(dialog,BELLE_SIP_TRANSACTION(t),FALSE)) {
		/* retransmition, just return*/
		belle_sip_message("[%p] is a200 ok retransmition on dialog [%p], skiping",resp,dialog);
		return;
	}

	event.source=base->provider;
	event.client_transaction=t;
	event.dialog=dialog;
	event.response=(belle_sip_response_t*)resp;
	BELLE_SIP_PROVIDER_INVOKE_LISTENERS_FOR_TRANSACTION(((belle_sip_transaction_t*)t),process_response_event,&event);
	/*check that 200Ok for INVITEs have been acknowledged by listener*/
	if (dialog && strcmp(method,"INVITE")==0)
		belle_sip_dialog_check_ack_sent(dialog);
}


void belle_sip_client_transaction_add_response(belle_sip_client_transaction_t *t, belle_sip_response_t *resp){
	BELLE_SIP_OBJECT_VPTR(t,belle_sip_client_transaction_t)->on_response(t,resp);
}

static void client_transaction_destroy(belle_sip_client_transaction_t *t ){
	if (t->preset_route) belle_sip_object_unref(t->preset_route);
	if (t->next_hop) belle_sip_object_unref(t->next_hop);
}

static void on_channel_state_changed(belle_sip_channel_listener_t *l, belle_sip_channel_t *chan, belle_sip_channel_state_t state){
	belle_sip_client_transaction_t *t=(belle_sip_client_transaction_t*)l;
	belle_sip_io_error_event_t ev;
	belle_sip_transaction_state_t tr_state=belle_sip_transaction_get_state((belle_sip_transaction_t*)t);
	
	belle_sip_message("transaction [%p] channel state changed to [%s]"
						,t
						,belle_sip_channel_state_to_string(state));
	switch(state){
		case BELLE_SIP_CHANNEL_READY:
			if (tr_state==BELLE_SIP_TRANSACTION_INIT){
				BELLE_SIP_OBJECT_VPTR(t,belle_sip_client_transaction_t)->send_request(t);
			}
		break;
		case BELLE_SIP_CHANNEL_DISCONNECTED:
		case BELLE_SIP_CHANNEL_ERROR:
			ev.transport=belle_sip_channel_get_transport_name(chan);
			ev.source=BELLE_SIP_OBJECT(t);
			ev.port=chan->peer_port;
			ev.host=chan->peer_name;
			if ( tr_state!=BELLE_SIP_TRANSACTION_COMPLETED
				&& tr_state!=BELLE_SIP_TRANSACTION_CONFIRMED
				&& tr_state!=BELLE_SIP_TRANSACTION_ACCEPTED
				&& tr_state!=BELLE_SIP_TRANSACTION_TERMINATED) {
				BELLE_SIP_PROVIDER_INVOKE_LISTENERS_FOR_TRANSACTION(((belle_sip_transaction_t*)t),process_io_error,&ev);
			}
			if (t->base.timed_out)
				notify_timeout((belle_sip_transaction_t*)t);
			
			belle_sip_transaction_terminate(BELLE_SIP_TRANSACTION(t));
		break;
		default:
			/*ignored*/
		break;
	}
}

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_sip_client_transaction_t,belle_sip_channel_listener_t)
on_channel_state_changed,
NULL,
NULL
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_sip_client_transaction_t, belle_sip_channel_listener_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_client_transaction_t)={
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_client_transaction_t,belle_sip_transaction_t,FALSE),
			(belle_sip_object_destroy_t)client_transaction_destroy,
			NULL,
			NULL
		},
		NULL
	},
	NULL,
	NULL
};

void belle_sip_client_transaction_init(belle_sip_client_transaction_t *obj, belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_header_via_t *via=BELLE_SIP_HEADER_VIA(belle_sip_message_get_header((belle_sip_message_t*)req,"via"));
	char token[BELLE_SIP_BRANCH_ID_LENGTH];

	if (!via){
		belle_sip_fatal("belle_sip_client_transaction_init(): No via in request.");
	}
	
	if (strcmp(belle_sip_request_get_method(req),"CANCEL")!=0){
		obj->base.branch_id=belle_sip_strdup_printf(BELLE_SIP_BRANCH_MAGIC_COOKIE ".%s",belle_sip_random_token(token,sizeof(token)));
		belle_sip_header_via_set_branch(via,obj->base.branch_id);
	}else{
		obj->base.branch_id=belle_sip_strdup(belle_sip_header_via_get_branch(via));
	}
	belle_sip_transaction_init((belle_sip_transaction_t*)obj, prov,req);
}

belle_sip_refresher_t* belle_sip_client_transaction_create_refresher(belle_sip_client_transaction_t *t) {
	return belle_sip_refresher_new(t);
}

belle_sip_request_t* belle_sip_client_transaction_create_authenticated_request(belle_sip_client_transaction_t *t,belle_sip_list_t** auth_infos) {
	belle_sip_request_t* initial_request=belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(t));
	belle_sip_request_t* req=belle_sip_request_clone_with_body(initial_request);
	belle_sip_header_cseq_t* cseq=belle_sip_message_get_header_by_type(req,belle_sip_header_cseq_t);
	belle_sip_header_cseq_set_seq_number(cseq,belle_sip_header_cseq_get_seq_number(cseq)+1);
	if (belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(t)) != BELLE_SIP_TRANSACTION_COMPLETED
		&& belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(t)) != BELLE_SIP_TRANSACTION_TERMINATED) {
		belle_sip_error("Invalid state [%s] for transaction [%p], should be BELLE_SIP_TRANSACTION_COMPLETED | BELLE_SIP_TRANSACTION_TERMINATED"
					,belle_sip_transaction_state_to_string(belle_sip_transaction_get_state(BELLE_SIP_TRANSACTION(t)))
					,t);
		belle_sip_object_unref(req);
		return NULL;
	}
	/*remove auth headers*/
	belle_sip_message_remove_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_AUTHORIZATION);
	belle_sip_message_remove_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_PROXY_AUTHORIZATION);

	/*put auth header*/
	belle_sip_provider_add_authorization(t->base.provider,req,t->base.last_response,auth_infos);
	return req;
}

