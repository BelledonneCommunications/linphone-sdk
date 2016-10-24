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

static void belle_sip_dialog_init_200Ok_retrans(belle_sip_dialog_t *obj, belle_sip_response_t *resp);
static int belle_sip_dialog_handle_200Ok(belle_sip_dialog_t *obj, belle_sip_response_t *msg);
static void belle_sip_dialog_process_queue(belle_sip_dialog_t* dialog);
static belle_sip_request_t *create_request(belle_sip_dialog_t *obj, const char *method, int full);

static void belle_sip_dialog_uninit(belle_sip_dialog_t *obj){
	if (obj->route_set)
		belle_sip_list_free_with_data(obj->route_set,belle_sip_object_unref);
	if (obj->remote_target)
		belle_sip_object_unref(obj->remote_target);
	if (obj->call_id)
		belle_sip_object_unref(obj->call_id);
	if (obj->local_party)
		belle_sip_object_unref(obj->local_party);
	if (obj->remote_party)
		belle_sip_object_unref(obj->remote_party);
	if (obj->local_tag)
		belle_sip_free(obj->local_tag);
	if (obj->remote_tag)
		belle_sip_free(obj->remote_tag);
	if (obj->last_out_invite)
		belle_sip_object_unref(obj->last_out_invite);
	if (obj->last_out_ack)
		belle_sip_object_unref(obj->last_out_ack);
	if (obj->last_transaction)
		belle_sip_object_unref(obj->last_transaction);
	if(obj->privacy)
		belle_sip_object_unref(obj->privacy);
	if (obj->expiration_timer){ /*In some situations, dialog deleted might not be called*/
		belle_sip_main_loop_remove_source(obj->provider->stack->ml, obj->expiration_timer);
		belle_sip_object_unref(obj->expiration_timer);
		obj->expiration_timer = NULL;
	}
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_dialog_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_dialog_t) 
		BELLE_SIP_VPTR_INIT(belle_sip_dialog_t, belle_sip_object_t,TRUE),
		(belle_sip_object_destroy_t)belle_sip_dialog_uninit,
		NULL,
		NULL,
		BELLE_SIP_DEFAULT_BUFSIZE_HINT
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

const char* belle_sip_dialog_state_to_string(const belle_sip_dialog_state_t state) {
	switch(state) {
	case BELLE_SIP_DIALOG_NULL: return "BELLE_SIP_DIALOG_NULL";
	case BELLE_SIP_DIALOG_EARLY: return "BELLE_SIP_DIALOG_EARLY";
	case BELLE_SIP_DIALOG_CONFIRMED: return "BELLE_SIP_DIALOG_CONFIRMED";
	case BELLE_SIP_DIALOG_TERMINATED: return "BELLE_SIP_DIALOG_TERMINATED";
	default: return "Unknown state";
	}
}

static void set_state(belle_sip_dialog_t *obj,belle_sip_dialog_state_t state) {
	obj->previous_state=obj->state;
	obj->state=state;
}

static void set_to_tag(belle_sip_dialog_t *obj, belle_sip_header_to_t *to){
	const char *to_tag=belle_sip_header_to_get_tag(to);
	if (obj->is_server){
		if (to_tag && !obj->local_tag)
			obj->local_tag=belle_sip_strdup(to_tag);
	}else{
		if (to_tag && !obj->remote_tag)
			obj->remote_tag=belle_sip_strdup(to_tag);
	}
}

static void check_route_set(belle_sip_list_t *rs){
	if (rs){
		belle_sip_header_route_t *r=(belle_sip_header_route_t*)rs->data;
		if (!belle_sip_uri_has_lr_param(belle_sip_header_address_get_uri((belle_sip_header_address_t*)r))){
			belle_sip_warning("top uri of route set does not contain 'lr', not really supported.");
		}
	}
}

static int belle_sip_dialog_init_as_uas(belle_sip_dialog_t *obj, belle_sip_request_t *req){
	const belle_sip_list_t *elem;
	belle_sip_header_contact_t *ct=belle_sip_message_get_header_by_type(req,belle_sip_header_contact_t);
	belle_sip_header_cseq_t *cseq=belle_sip_message_get_header_by_type(req,belle_sip_header_cseq_t);
	belle_sip_header_via_t *via=belle_sip_message_get_header_by_type(req,belle_sip_header_via_t);
	belle_sip_uri_t *requri=belle_sip_request_get_uri(req);
	belle_sip_header_to_t *to=belle_sip_message_get_header_by_type(req,belle_sip_header_to_t);

	if (!ct){
		belle_sip_error("No contact in request.");
		return -1;
	}
	if (!cseq){
		belle_sip_error("No cseq in request.");
		return -1;
	}
	if (!via){
		belle_sip_error("No via in request.");
		return -1;
	}
	if (!to){
		belle_sip_error("No to in request.");
		return -1;
	}

	if (strcasecmp(belle_sip_header_via_get_protocol(via),"TLS")==0
	    && belle_sip_uri_is_secure(requri)){
		obj->is_secure=TRUE;
	}
	/* 12.1.1
	*The route set MUST be set to the list of URIs in the Record-Route
	* header field from the request, taken in order and preserving all URI
	* parameters.  If no Record-Route header field is present in the
	*request, the route set MUST be set to the empty set.
	*/
	obj->route_set=belle_sip_list_free_with_data(obj->route_set,belle_sip_object_unref);
	for(elem=belle_sip_message_get_headers((belle_sip_message_t*)req,BELLE_SIP_RECORD_ROUTE);elem!=NULL;elem=elem->next){
		obj->route_set=belle_sip_list_append(obj->route_set,belle_sip_object_ref(belle_sip_header_route_create(
		                                     (belle_sip_header_address_t*)elem->data)));
	}
	check_route_set(obj->route_set);
	obj->remote_target=(belle_sip_header_address_t*)belle_sip_object_ref(ct);
	obj->remote_cseq=belle_sip_header_cseq_get_seq_number(cseq);
	if (strcmp(belle_sip_request_get_method(req),"INVITE")==0){
		obj->remote_invite_cseq = belle_sip_header_cseq_get_seq_number(cseq);
	}

	/*remote party already set */
	obj->local_party=(belle_sip_header_address_t*)belle_sip_object_ref(to);
	
	return 0;
}

static void set_last_out_invite(belle_sip_dialog_t *obj, belle_sip_request_t *req){
	if (obj->last_out_invite)
		belle_sip_object_unref(obj->last_out_invite);
	obj->last_out_invite=(belle_sip_request_t*)belle_sip_object_ref(req);
}

static int belle_sip_dialog_init_as_uac(belle_sip_dialog_t *obj, belle_sip_request_t *req){
	belle_sip_header_cseq_t *cseq=belle_sip_message_get_header_by_type(req,belle_sip_header_cseq_t);
	belle_sip_header_to_t *to=belle_sip_message_get_header_by_type(req,belle_sip_header_to_t);
	belle_sip_header_via_t *via=belle_sip_message_get_header_by_type(req,belle_sip_header_via_t);
	belle_sip_uri_t *requri=belle_sip_request_get_uri(req);

	if (!to){
		belle_sip_error("No to in request.");
		return -1;
	}
	if (!cseq){
		belle_sip_error("No cseq in request.");
		return -1;
	}
	if (!via){
		belle_sip_error("No via in request.");
		return -1;
	}

	if (belle_sip_header_via_get_protocol(via) /*might be null at dialog creation (uac case)*/
		&& strcasecmp(belle_sip_header_via_get_protocol(via),"TLS")==0
	    && belle_sip_uri_is_secure(requri)){
		obj->is_secure=TRUE;
	}
	
	obj->local_cseq=belle_sip_header_cseq_get_seq_number(cseq);
	/*call id is already set */
	/*local_tag is already set*/
	obj->remote_party=(belle_sip_header_address_t*)belle_sip_object_ref(to);
	/*local party is already set*/
	if (strcmp(belle_sip_request_get_method(req),"INVITE")==0){
		set_last_out_invite(obj,req);
	}
	
	return 0;
}

static int belle_sip_dialog_on_expired(belle_sip_dialog_t *dialog){
	belle_sip_message("Dialog [%p] expired", dialog);
	dialog->is_expired = TRUE;
	belle_sip_dialog_delete(dialog);
	return BELLE_SIP_STOP;
}

int belle_sip_dialog_expired(const belle_sip_dialog_t *dialog){
	return dialog->is_expired;
}

/*returns BELLE_SIP_STOP if the dialog is to be terminated, BELLE_SIP_CONTINUE otherwise*/
static int belle_sip_dialog_schedule_expiration(belle_sip_dialog_t *dialog, belle_sip_message_t *request){
	belle_sip_header_expires_t *expires = belle_sip_message_get_header_by_type(request, belle_sip_header_expires_t);
	int expires_value;
	
	if (!expires) return BELLE_SIP_CONTINUE;
	expires_value = belle_sip_header_expires_get_expires(expires);
	if (dialog->expiration_timer){
		belle_sip_main_loop_remove_source(dialog->provider->stack->ml, dialog->expiration_timer);
		belle_sip_object_unref(dialog->expiration_timer);
		dialog->expiration_timer = NULL;
	}
	belle_sip_message("belle_sip_dialog_schedule_expiration() dialog=%p expires_value=%i", dialog, expires_value);
	if (expires_value == 0) return BELLE_SIP_STOP;
	dialog->expiration_timer = belle_sip_main_loop_create_timeout(dialog->provider->stack->ml, 
					(belle_sip_source_func_t) belle_sip_dialog_on_expired,
					dialog,
					expires_value * 1000,
					"Dialog expiration");
	return BELLE_SIP_CONTINUE;
}

int belle_sip_dialog_establish_full(belle_sip_dialog_t *obj, belle_sip_request_t *req, belle_sip_response_t *resp){
	belle_sip_header_contact_t *ct=belle_sip_message_get_header_by_type(resp,belle_sip_header_contact_t);
	belle_sip_header_to_t *to=belle_sip_message_get_header_by_type(resp,belle_sip_header_to_t);
	const belle_sip_list_t *elem;

	if (strcmp(belle_sip_request_get_method(req),"INVITE")==0)
		obj->needs_ack=TRUE;

	if (obj->is_server){
		if (strcmp(belle_sip_request_get_method(req),"INVITE")==0){
			belle_sip_dialog_init_200Ok_retrans(obj,resp);
		}
	} else {
		if (!ct && !obj->remote_target) {
			belle_sip_error("Missing contact header in resp [%p] cannot set remote target for dialog [%p]",resp,obj);
			return -1;
		}
		/* 13.2.2.4 2xx Responses
		   ...
		   If the dialog identifier in the 2xx response matches the dialog
		   identifier of an existing dialog, the dialog MUST be transitioned to
		   the "confirmed" state, and the route set for the dialog MUST be
		   recomputed based on the 2xx response using the procedures of Section
		   12.2.1.2.
		 */
		obj->route_set=belle_sip_list_free_with_data(obj->route_set,belle_sip_object_unref);
		for(elem=belle_sip_message_get_headers((belle_sip_message_t*)resp,BELLE_SIP_RECORD_ROUTE);elem!=NULL;elem=elem->next){
			obj->route_set=belle_sip_list_prepend(obj->route_set,belle_sip_object_ref(belle_sip_header_route_create(
			                                     (belle_sip_header_address_t*)elem->data)));
		}

		if (ct) {
			/*remote Contact header may have changed between early dialog to confirmed*/
			if (obj->remote_target) belle_sip_object_unref(obj->remote_target);
			obj->remote_target=(belle_sip_header_address_t*)belle_sip_object_ref(ct);
		}
	}
	/*update to tag*/
	set_to_tag(obj,to);
	set_state(obj,BELLE_SIP_DIALOG_CONFIRMED);
	return 0;
}

int belle_sip_dialog_establish(belle_sip_dialog_t *obj, belle_sip_request_t *req, belle_sip_response_t *resp){
	belle_sip_header_to_t *to=belle_sip_message_get_header_by_type(resp,belle_sip_header_to_t);
	belle_sip_header_via_t *via=belle_sip_message_get_header_by_type(req,belle_sip_header_via_t);
	belle_sip_uri_t *requri=belle_sip_request_get_uri(req);

	if (obj->state != BELLE_SIP_DIALOG_NULL) {
		belle_sip_error("Dialog [%p] already established.",obj);
		return -1;
	}
	if (!to){
		belle_sip_error("No to in response.");
		return -1;
	}
	
	if (!obj->is_server) {
		belle_sip_header_contact_t *ct=belle_sip_message_get_header_by_type(resp,belle_sip_header_contact_t);
		const belle_sip_list_t* elem;
		/*contact might be provided later*/
		if (ct)
			obj->remote_target=(belle_sip_header_address_t*)belle_sip_object_ref(ct);

		/**12.1.2
		 *  The route set MUST be set to the list of URIs in the Record-Route
		 *header field from the response, taken in reverse order and preserving
		 *all URI parameters.  If no Record-Route header field is present in
		 *the response, the route set MUST be set to the empty set.
		 **/
		obj->route_set=belle_sip_list_free_with_data(obj->route_set,belle_sip_object_unref);
		for(elem=belle_sip_message_get_headers((belle_sip_message_t*)resp,BELLE_SIP_RECORD_ROUTE);elem!=NULL;elem=elem->next){
			obj->route_set=belle_sip_list_prepend(obj->route_set,belle_sip_object_ref(belle_sip_header_route_create(
																													(belle_sip_header_address_t*)elem->data)));
		}
		
		check_route_set(obj->route_set);
		/*via might be unknown at dialog creation*/
		if (strcasecmp(belle_sip_header_via_get_protocol(via),"TLS")==0
			&& belle_sip_uri_is_secure(requri)){
			obj->is_secure=TRUE;
		}

	}

	set_to_tag(obj,to);

	return 0;
}

int belle_sip_dialog_check_incoming_request_ordering(belle_sip_dialog_t *obj, belle_sip_request_t *req){
	belle_sip_header_cseq_t *cseqh=belle_sip_message_get_header_by_type(req,belle_sip_header_cseq_t);
	unsigned int cseq=belle_sip_header_cseq_get_seq_number(cseqh);
	if (obj->remote_cseq==0){
		obj->remote_cseq=cseq;
	}else if (cseq>obj->remote_cseq){
			return 0;
	}
	belle_sip_warning("Ignoring request because cseq is inconsistent.");
	return -1;
}

static int dialog_on_200Ok_timer(belle_sip_dialog_t *dialog){
	/*reset the timer */
	const belle_sip_timer_config_t *cfg=belle_sip_stack_get_timer_config(dialog->provider->stack);
	unsigned int prev_timeout=belle_sip_source_get_timeout(dialog->timer_200Ok);
	belle_sip_source_set_timeout(dialog->timer_200Ok,MIN(2*prev_timeout,(unsigned int)cfg->T2));
	belle_sip_message("Dialog sending retransmission of 200Ok");
	belle_sip_provider_send_response(dialog->provider,dialog->last_200Ok);
	return BELLE_SIP_CONTINUE;
}

static int dialog_on_200Ok_end(belle_sip_dialog_t *dialog){
	belle_sip_request_t *bye;
	belle_sip_client_transaction_t *trn;
	belle_sip_dialog_stop_200Ok_retrans(dialog);
	belle_sip_error("Dialog [%p] was not ACK'd within T1*64 seconds, it is going to be terminated.",dialog);
	dialog->state=BELLE_SIP_DIALOG_CONFIRMED;
	bye=belle_sip_dialog_create_request(dialog,"BYE");
	trn=belle_sip_provider_create_client_transaction(dialog->provider,bye);
	BELLE_SIP_TRANSACTION(trn)->is_internal=1; /*don't bother user with this transaction*/
	belle_sip_client_transaction_send_request(trn);
	return BELLE_SIP_STOP;
}

static void belle_sip_dialog_init_200Ok_retrans(belle_sip_dialog_t *obj, belle_sip_response_t *resp){
	const belle_sip_timer_config_t *cfg=belle_sip_stack_get_timer_config(obj->provider->stack);
	if (obj->timer_200Ok || obj->timer_200Ok_end) {
		belle_sip_error("dialog [%p] already has a 200ok retransmition timer ! skipping",obj);
		return;
	}
	obj->timer_200Ok=belle_sip_timeout_source_new((belle_sip_source_func_t)dialog_on_200Ok_timer,obj,cfg->T1);
	belle_sip_object_set_name((belle_sip_object_t*)obj->timer_200Ok,"dialog_200Ok_timer");
	belle_sip_main_loop_add_source(obj->provider->stack->ml,obj->timer_200Ok);
	
	obj->timer_200Ok_end=belle_sip_timeout_source_new((belle_sip_source_func_t)dialog_on_200Ok_end,obj,cfg->T1*64);
	belle_sip_object_set_name((belle_sip_object_t*)obj->timer_200Ok_end,"dialog_200Ok_timer_end");
	belle_sip_main_loop_add_source(obj->provider->stack->ml,obj->timer_200Ok_end);
	
	obj->last_200Ok=(belle_sip_response_t*)belle_sip_object_ref(resp);
}

void belle_sip_dialog_stop_200Ok_retrans(belle_sip_dialog_t *obj){
	belle_sip_main_loop_t *ml=obj->provider->stack->ml;
	if (obj->timer_200Ok){
		belle_sip_main_loop_remove_source(ml,obj->timer_200Ok);
		belle_sip_object_unref(obj->timer_200Ok);
		obj->timer_200Ok=NULL;
	}
	if (obj->timer_200Ok_end){
		belle_sip_main_loop_remove_source(ml,obj->timer_200Ok_end);
		belle_sip_object_unref(obj->timer_200Ok_end);
		obj->timer_200Ok_end=NULL;
	}
	if (obj->last_200Ok){
		belle_sip_object_unref(obj->last_200Ok);
		obj->last_200Ok=NULL;
	}
}

/* returns true if the dialog is terminated by the NOTIFY request or response*/
static int belle_sip_dialog_should_terminate_by_notify(belle_sip_dialog_t *obj, belle_sip_transaction_t* transaction, int as_uas){
	int should_terminate = FALSE;
	
	if (obj->type == BELLE_SIP_DIALOG_SUBSCRIBE_NOTIFY) {
		belle_sip_request_t *req=belle_sip_transaction_get_request(transaction);
		belle_sip_response_t *resp=belle_sip_transaction_get_response(transaction);
		belle_sip_header_subscription_state_t* subscription_state_header=belle_sip_message_get_header_by_type(req,belle_sip_header_subscription_state_t);
		int code = resp ? belle_sip_response_get_status_code(resp) : 0;
	
		if (!subscription_state_header || strcasecmp(BELLE_SIP_SUBSCRIPTION_STATE_TERMINATED,belle_sip_header_subscription_state_get_state(subscription_state_header)) ==0) {
			if (as_uas){
				/*terminate the dialog when the application replies the 200 Ok*/
				if (code == 200){
					should_terminate = TRUE;
				}
			}else{
				/*terminate the dialog when we receive the 200OK or the transaction terminates by timeout*/
				if (code == 200 || belle_sip_transaction_get_state(transaction) == BELLE_SIP_TRANSACTION_TERMINATED){
					should_terminate = TRUE;
				}
			}
		}
	}
	return should_terminate;
}

static void belle_sip_dialog_update_remote_target(belle_sip_dialog_t *obj, belle_sip_header_contact_t *ct) {
	int remote_target_changed = FALSE;
	if (obj->remote_target) {
		belle_sip_uri_t *remote_target_uri = belle_sip_header_address_get_uri(obj->remote_target);
		belle_sip_uri_t *ct_uri = belle_sip_header_address_get_uri((belle_sip_header_address_t *)ct);
		remote_target_changed = !belle_sip_uri_equals(remote_target_uri, ct_uri);
		belle_sip_object_unref(obj->remote_target);
	}
	obj->remote_target=(belle_sip_header_address_t*)belle_sip_object_ref(ct);
	if (remote_target_changed) {
		belle_sip_message("Dialog [%p]: remote target changed", obj);
		if (obj->last_out_ack) {
			belle_sip_request_set_uri(BELLE_SIP_REQUEST(obj->last_out_ack), belle_sip_header_address_get_uri(obj->remote_target));
		}
	}
}

/*
 * return 0 if message should be delivered to the next listener, otherwise, its a retransmision, just keep it
 * */
int belle_sip_dialog_update(belle_sip_dialog_t *obj, belle_sip_transaction_t* transaction, int as_uas){
	int is_retransmition=FALSE;
	int delete_dialog=FALSE;
	belle_sip_request_t *req=belle_sip_transaction_get_request(transaction);
	belle_sip_response_t *resp=belle_sip_transaction_get_response(transaction);
	int code=0;
	int ret = 0;
	int is_invite = strcmp(belle_sip_request_get_method(req),"INVITE")==0;
	int is_subscribe = strcmp(belle_sip_request_get_method(req),"SUBSCRIBE")==0;
	int is_notify = strcmp(belle_sip_request_get_method(req),"NOTIFY")==0;

	belle_sip_message("Dialog [%p]: now updated by transaction [%p].",obj, transaction);
	
	if (resp)
		code=belle_sip_response_get_status_code(resp);
	
	if (as_uas && code == 491) { /**/
		belle_sip_message("Dialog [%p]: don't update last transaction by transaction [%p].",obj, transaction);
	} else {
		belle_sip_object_ref(transaction);
		if (obj->last_transaction) belle_sip_object_unref(obj->last_transaction);
		obj->last_transaction=transaction;
	}
	if (!as_uas){
		belle_sip_header_privacy_t *privacy_header=belle_sip_message_get_header_by_type(req,belle_sip_header_privacy_t);
		SET_OBJECT_PROPERTY(obj,privacy,privacy_header);
	}
	
	/*first update local/remote cseq*/
	if (as_uas) {
		belle_sip_header_cseq_t* cseq=belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(req),belle_sip_header_cseq_t);
		obj->remote_cseq=belle_sip_header_cseq_get_seq_number(cseq);
		if (is_invite && code>=200 && code<300)
			obj->remote_invite_cseq = belle_sip_header_cseq_get_seq_number(cseq);
		/*else ACK is handled by transaction, not dialog*/
	}

	
	switch (obj->state){
		case BELLE_SIP_DIALOG_NULL:
			/*always establish a dialog*/
			if (code>100 && code<300 && (is_invite || is_subscribe)) {
				belle_sip_dialog_establish(obj,req,resp);
				if (code<200){
					set_state(obj,BELLE_SIP_DIALOG_EARLY);
					break;
				}/* no break  for code >200 because need to call belle_sip_dialog_establish_full*/
			}/* no break*/
		case BELLE_SIP_DIALOG_EARLY:
			/*don't terminate dialog for UPDATE*/
			if (code>=300 && (is_invite || is_subscribe)) {
				/*12.3 Termination of a Dialog
			   	   Independent of the method, if a request outside of a dialog generates
			   	   a non-2xx final response, any early dialogs created through
			   	   provisional responses to that request are terminated.  The mechanism
			   	   for terminating confirmed dialogs is method specific.*/
					delete_dialog=TRUE;
					break;
			}
			if (code>=200 && code<300 && (is_invite || is_subscribe)){
				if (belle_sip_dialog_establish_full(obj,req,resp) != 0){
					/*the dialog found this response invalid so notify the transaction layer by returning -1*/
					ret = -1;
					goto end;
				}
			}
			if (is_subscribe){
				if (belle_sip_dialog_schedule_expiration(obj, (belle_sip_message_t*)req) == BELLE_SIP_STOP
					&& (code>=200 || (code==0 && belle_sip_transaction_get_state(transaction)==BELLE_SIP_TRANSACTION_TERMINATED))){
					/*delete the dialog when the 200Ok for a SUBSCRIBE with expires=0 is received or when
					 * no response is received at all*/
					delete_dialog = TRUE;
				}
			}
			if (code < 200 && belle_sip_transaction_get_state(transaction)==BELLE_SIP_TRANSACTION_TERMINATED){
				/*no response establishing the dialog, and transaction terminated (transport errors)*/
				delete_dialog=TRUE;
			}
			break;
		case BELLE_SIP_DIALOG_CONFIRMED:
			if (code==481 && (is_invite || is_subscribe)) {
				/*Dialog is terminated in such case*/
				delete_dialog=TRUE;
				break;
			}
			/*refreshing target is also true in case of subscribe*/
			if ((is_invite || is_subscribe) && (code>=200 && code<300)) {
				/*refresh the remote_target*/
				belle_sip_header_contact_t *ct;
				if (as_uas){
					ct=belle_sip_message_get_header_by_type(req,belle_sip_header_contact_t);
					
				}else{
					if (is_invite)
						set_last_out_invite(obj,req);
					ct=belle_sip_message_get_header_by_type(resp,belle_sip_header_contact_t);
				}
				if (ct){
					belle_sip_dialog_update_remote_target(obj, ct);
				}
				
			}
			if (is_invite){
				if (code>=200 && code<300){
					/*handle possible retransmission of 200Ok */
					if (!as_uas && (is_retransmition=(belle_sip_dialog_handle_200Ok(obj,resp)==0))) {
						return is_retransmition;
					} else {
						if (as_uas)
							belle_sip_dialog_init_200Ok_retrans(obj,resp);
						obj->needs_ack=TRUE; /*REINVITE case, ack needed by both uas and uac*/
					}
				}else if (code>=300){
					/*final response, ack will be automatically sent by transaction layer*/
					/* do not need to do anything because  not set yet or set by previous invite transaction obj->needs_ack=FALSE;*/
				}
			} else if (strcmp(belle_sip_request_get_method(req),"BYE")==0){
				/*15.1.1 UAC Behavior

				   A BYE request is constructed as would any other request within a
				   dialog, as described in Section 12.

				   Once the BYE is constructed, the UAC core creates a new non-INVITE
				   client transaction, and passes it the BYE request.  The UAC MUST
				   consider the session terminated (and therefore stop sending or
				   listening for media) as soon as the BYE request is passed to the
				   client transaction.  If the response for the BYE is a 481
				   (Call/Transaction Does Not Exist) or a 408 (Request Timeout) or no
				   response at all is received for the BYE (that is, a timeout is
				   returned by the client transaction), the UAC MUST consider the
				   session and the dialog terminated. */
				/*what should we do with other reponse >300 ?? */
				if (code>=200 || (code==0 && belle_sip_transaction_get_state(transaction)==BELLE_SIP_TRANSACTION_TERMINATED)){
					obj->needs_ack=FALSE; /*no longuer need ACK*/
					if (obj->terminate_on_bye) delete_dialog=TRUE;
				}
			}else if (is_subscribe){
				if (belle_sip_dialog_schedule_expiration(obj, (belle_sip_message_t*)req) == BELLE_SIP_STOP
					&& (code>=200 || (code==0 && belle_sip_transaction_get_state(transaction)==BELLE_SIP_TRANSACTION_TERMINATED))){
					delete_dialog = TRUE;
				}else if (!as_uas){
					if (code >= 300 || (code==0 && belle_sip_transaction_get_state(transaction)==BELLE_SIP_TRANSACTION_TERMINATED)){
						/*case of a SUBSCRIBE refresh that is rejected or unanswered*/
						if (code != 491){
							/*request pending is not fatal for the dialog*/
							delete_dialog = TRUE;
						}
					}
				}
			}else if (is_notify){
				delete_dialog = belle_sip_dialog_should_terminate_by_notify(obj, transaction, as_uas);
			}
		break;
		case BELLE_SIP_DIALOG_TERMINATED:
			/*ignore*/
		break;
	}
	
end:
	if (delete_dialog) belle_sip_dialog_delete(obj);
	else {
		belle_sip_dialog_process_queue(obj);
	}
	return ret;
}

belle_sip_dialog_t *belle_sip_dialog_new(belle_sip_transaction_t *t){
	belle_sip_dialog_t *obj;
	belle_sip_header_from_t *from;
	const char *from_tag;
	belle_sip_header_to_t *to;
	const char *to_tag=NULL;
	belle_sip_header_call_id_t *call_id=belle_sip_message_get_header_by_type(t->request,belle_sip_header_call_id_t);

	from=belle_sip_message_get_header_by_type(t->request,belle_sip_header_from_t);
	if (from==NULL){
		belle_sip_error("belle_sip_dialog_new(): no from!");
		return NULL;
	}
	from_tag=belle_sip_header_from_get_tag(from);
	if (from_tag==NULL){
		belle_sip_error("belle_sip_dialog_new(): no from tag!");
		return NULL;
	}
	
	to = belle_sip_message_get_header_by_type(t->request,belle_sip_header_to_t);
	if (to == NULL){
		belle_sip_error("belle_sip_dialog_new(): no to in request!");
		return NULL;
	}
	to_tag = belle_sip_header_to_get_tag(to);
	if (to_tag){
		belle_sip_error("belle_sip_dialog_new(): there is a to tag in the request. This is not allowed"
			" to create a dialog on such a transaction.");
		return NULL;
	}
	if (!call_id){
		belle_sip_error("No call-id in response.");
		return NULL;
	}

	if (t->last_response) {
		to=belle_sip_message_get_header_by_type(t->last_response,belle_sip_header_to_t);
		if (to==NULL){
			belle_sip_error("belle_sip_dialog_new(): no to!");
			return NULL;
		}
		to_tag = belle_sip_header_to_get_tag(to);
	}
	obj=belle_sip_object_new(belle_sip_dialog_t);
	obj->terminate_on_bye=1;
	obj->provider=t->provider;
	obj->pending_trans_checking_enabled=1;
	obj->call_id=(belle_sip_header_call_id_t*)belle_sip_object_ref(call_id);
	
	if (strcmp(belle_sip_request_get_method(t->request),"INVITE") == 0){
		obj->type = BELLE_SIP_DIALOG_INVITE;
	}else if (strcmp(belle_sip_request_get_method(t->request),"SUBSCRIBE") == 0){
		obj->type = BELLE_SIP_DIALOG_SUBSCRIBE_NOTIFY;
	}else{
		belle_sip_error("belle_sip_dialog_new(): unsupported request [%s] for creating a dialog.", belle_sip_request_get_method(t->request));
	}
	
	belle_sip_object_ref(t);
	obj->last_transaction=t;
	
	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(t,belle_sip_server_transaction_t)){
		obj->remote_tag=belle_sip_strdup(from_tag);
		obj->local_tag=belle_sip_strdup(BELLE_SIP_SERVER_TRANSACTION(t)->to_tag);
		obj->remote_party=(belle_sip_header_address_t*)belle_sip_object_ref(from);
		obj->is_server=TRUE;
		belle_sip_dialog_init_as_uas(obj,belle_sip_transaction_get_request(t));
	}else{
		const belle_sip_list_t *predefined_routes=NULL;
		obj->local_tag=belle_sip_strdup(from_tag);
		obj->remote_tag=to_tag?belle_sip_strdup(to_tag):NULL; /*might be null at dialog creation*/
		obj->local_party=(belle_sip_header_address_t*)belle_sip_object_ref(from);
		obj->is_server=FALSE;
		for(predefined_routes=belle_sip_message_get_headers((belle_sip_message_t*)t->request,BELLE_SIP_ROUTE);
			predefined_routes!=NULL;predefined_routes=predefined_routes->next){
			obj->route_set=belle_sip_list_append(obj->route_set,belle_sip_object_ref(predefined_routes->data));
		}
		belle_sip_dialog_init_as_uac(obj,belle_sip_transaction_get_request(t));
	}
	

	belle_sip_message("New %s dialog [%p] , local tag [%s], remote tag [%s]"
			,obj->is_server?"server":"client"
			,obj
			,obj->local_tag?obj->local_tag:""
			,obj->remote_tag?obj->remote_tag:"");
	set_state(obj,BELLE_SIP_DIALOG_NULL);
	return obj;
}

belle_sip_request_t *belle_sip_dialog_create_ack(belle_sip_dialog_t *obj, unsigned int cseq){
	belle_sip_header_cseq_t *cseqh;
	belle_sip_request_t *invite=obj->last_out_invite;
	belle_sip_request_t *ack;
	
	if (!invite){
		belle_sip_error("No INVITE to ACK.");
		return NULL;
	}
	cseqh=belle_sip_message_get_header_by_type(invite,belle_sip_header_cseq_t);
	if (belle_sip_header_cseq_get_seq_number(cseqh)!=cseq){
		belle_sip_error("No INVITE with cseq %i to create ack for.",cseq);
		return NULL;
	}
	ack=create_request(obj,"ACK",TRUE);
/*
22 Usage of HTTP Authentication
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
  */
	if (ack){
		const belle_sip_list_t *aut=belle_sip_message_get_headers((belle_sip_message_t*)obj->last_out_invite,"Authorization");
		const belle_sip_list_t *prx_aut=belle_sip_message_get_headers((belle_sip_message_t*)obj->last_out_invite,"Proxy-Authorization");
		if (aut)
			belle_sip_message_add_headers((belle_sip_message_t*)ack,aut);
		if (prx_aut)
			belle_sip_message_add_headers((belle_sip_message_t*)ack,prx_aut);
		/*the ack is sent statelessly, the transaction layer doesn't need the dialog information*/
		belle_sip_request_set_dialog(ack,NULL);
	}
	return ack;
}

static belle_sip_request_t *create_request(belle_sip_dialog_t *obj, const char *method, int full){
	belle_sip_request_t *req;
	char* from_tag=NULL, *to_tag=NULL;
	
	if (!obj->remote_target){
		belle_sip_error("dialog [%p]: no remote_target set, unable to create request.", obj);
		return NULL;
	}
	
	if (!belle_sip_parameters_has_parameter(BELLE_SIP_PARAMETERS(obj->local_party), "tag")) {
		/*special case for dialog created by server transaction*/
		from_tag = obj->local_tag;
	}
	if (!belle_sip_parameters_has_parameter(BELLE_SIP_PARAMETERS(obj->remote_party), "tag")) {
		/*special case for dialog created by server transaction*/
		to_tag = obj->remote_tag;
	}

	req=belle_sip_request_create(belle_sip_header_address_get_uri(obj->remote_target),
	                                                method,
	                                                obj->call_id,
	                                                belle_sip_header_cseq_create(obj->local_cseq,method),
	                                                belle_sip_header_from_create(obj->local_party,from_tag),
	                                                belle_sip_header_to_create(obj->remote_party,to_tag),
	                                                belle_sip_header_via_new(),
	                                                0);
	
	if (full && obj->route_set) {
		belle_sip_message_add_headers((belle_sip_message_t*)req,obj->route_set);
	}
	if (obj->privacy) {
		/*repeat the last privacy set in new request. I could not find any requirement for this, but this might be safer
		 * as proxies don't store information about dialogs*/
		belle_sip_message_add_header((belle_sip_message_t*)req,BELLE_SIP_HEADER(obj->privacy));
	}
	belle_sip_request_set_dialog(req,obj);
	return req;
}

static int dialog_can_create_request(belle_sip_dialog_t *obj, const char *method){
	if (obj->state != BELLE_SIP_DIALOG_CONFIRMED && obj->state != BELLE_SIP_DIALOG_EARLY
		&& (strcmp("NOTIFY", method) != 0
			|| !obj->is_server
			|| !obj->last_transaction
			|| strcmp("SUBSCRIBE", belle_sip_transaction_get_method(obj->last_transaction)) !=0)) {
		belle_sip_error("belle_sip_dialog_create_request(): cannot create [%s] request from dialog [%p] in state [%s]",method,obj,belle_sip_dialog_state_to_string(obj->state));
		return FALSE;
	}
	return TRUE;
}

belle_sip_request_t * belle_sip_dialog_create_queued_request(belle_sip_dialog_t *obj, const char *method){
	belle_sip_request_t *req;
	
	if (!dialog_can_create_request(obj, method)) return NULL;
	if (strcmp(method,"INVITE")==0 || strcmp(method,"SUBSCRIBE")==0){
		/*we don't allow requests that can update the dialog's state to be sent asynchronously*/
		belle_sip_error("belle_sip_dialog_create_queued_request([%p]): [%s] requests are forbidden using this method.",obj,method);
		return NULL;
	}
	req=create_request(obj,method,FALSE);
	if (req){
		req->dialog_queued=TRUE;
	}
	return req;
}

static void belle_sip_dialog_update_local_cseq(belle_sip_dialog_t *obj, const char *method){
	if (obj->local_cseq==0) obj->local_cseq=110;
	if (strcmp(method,"ACK")!=0) obj->local_cseq++;
}

belle_sip_request_t *belle_sip_dialog_create_request(belle_sip_dialog_t *obj, const char *method){
	belle_sip_request_t *req;
	
	if (!dialog_can_create_request(obj, method)) return NULL;
	/*don't prevent to send a BYE in any case */
	if (	obj->pending_trans_checking_enabled
			&& strcmp(method,"BYE")!=0
			&& obj->last_transaction
			&& belle_sip_transaction_state_is_transient(belle_sip_transaction_get_state(obj->last_transaction))){

		if (obj->state != BELLE_SIP_DIALOG_EARLY && strcmp(method,"UPDATE")!=0 && strcmp(method,"NOTIFY")!=0) {
			belle_sip_error("belle_sip_dialog_create_request(): cannot create [%s] request from dialog [%p] while pending [%s] transaction in state [%s]",method,obj,belle_sip_transaction_get_method(obj->last_transaction), belle_sip_transaction_state_to_string(belle_sip_transaction_get_state(obj->last_transaction)));
			return NULL;
		} /*else UPDATE transaction can be send in // */
	}
	belle_sip_dialog_update_local_cseq(obj,method);
	
	req=create_request(obj,method,TRUE);
	return req;
}

static unsigned int is_system_header(belle_sip_header_t* header) {
	const char* name=belle_sip_header_get_name(header);
	return strcasecmp(BELLE_SIP_VIA,name) ==0
			|| strcasecmp(BELLE_SIP_FROM,name) ==0
			|| strcasecmp(BELLE_SIP_TO,name) ==0
			|| strcasecmp(BELLE_SIP_CSEQ,name) ==0
			|| strcasecmp(BELLE_SIP_CALL_ID,name) ==0
			|| strcasecmp(BELLE_SIP_PROXY_AUTHORIZATION,name) == 0
			|| strcasecmp(BELLE_SIP_AUTHORIZATION,name) == 0
			|| strcasecmp(BELLE_SIP_MAX_FORWARDS,name) == 0
			|| strcasecmp(BELLE_SIP_ALLOW,name) ==0
			|| strcasecmp(BELLE_SIP_ROUTE,name) ==0;
}
static void copy_non_system_headers(belle_sip_header_t* header,belle_sip_request_t* req ) {
	if (!is_system_header(header)) {
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),header);
	}
}

static belle_sip_request_t *_belle_sip_dialog_create_request_from(belle_sip_dialog_t *obj, const belle_sip_request_t *initial_req, int queued){
	belle_sip_request_t* req;
	const char *method=belle_sip_request_get_method(initial_req);
	belle_sip_header_content_length_t* content_lenth;
	belle_sip_list_t* headers;
	
	if (queued) req=belle_sip_dialog_create_queued_request(obj,method);
	else req=belle_sip_dialog_create_request(obj,method);
	
	if (req==NULL) return NULL;
	
	content_lenth = belle_sip_message_get_header_by_type(initial_req,belle_sip_header_content_length_t);
	/*first copy non system headers*/
	headers = belle_sip_message_get_all_headers(BELLE_SIP_MESSAGE(initial_req));
	belle_sip_list_for_each2(headers,(void (*)(void *, void *))copy_non_system_headers,req);
	belle_sip_list_free(headers);
	
	/*replicate via user parameters, if any, useful for 'alias' parameter in SUBSCRIBE requests*/
	{
		belle_sip_header_via_t *orig_via=belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(initial_req),belle_sip_header_via_t);
		belle_sip_header_via_t *new_via=belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(req),belle_sip_header_via_t);
		belle_sip_parameters_copy_parameters_from(BELLE_SIP_PARAMETERS(new_via),BELLE_SIP_PARAMETERS(orig_via));
	}
	
	/*copy body*/
	if (content_lenth && belle_sip_header_content_length_get_content_length(content_lenth)>0) {
		belle_sip_message_set_body(BELLE_SIP_MESSAGE(req),belle_sip_message_get_body(BELLE_SIP_MESSAGE(initial_req)),belle_sip_header_content_length_get_content_length(content_lenth));
	}
	return req;
}

belle_sip_request_t *belle_sip_dialog_create_request_from(belle_sip_dialog_t *obj, const belle_sip_request_t *initial_req){
	return _belle_sip_dialog_create_request_from(obj,initial_req,FALSE);
}

belle_sip_request_t *belle_sip_dialog_create_queued_request_from(belle_sip_dialog_t *obj, const belle_sip_request_t *initial_req){
	return _belle_sip_dialog_create_request_from(obj,initial_req,TRUE);
}

void belle_sip_dialog_delete(belle_sip_dialog_t *obj){
	size_t dropped_transactions;
	
	if (obj->expiration_timer){
		belle_sip_main_loop_remove_source(obj->provider->stack->ml, obj->expiration_timer);
		belle_sip_object_unref(obj->expiration_timer);
		obj->expiration_timer = NULL;
	}
	belle_sip_message("Dialog [%p] deleted (is_expired=%i)",obj, obj->is_expired);
	belle_sip_dialog_stop_200Ok_retrans(obj); /*if any*/
	set_state(obj,BELLE_SIP_DIALOG_TERMINATED);
	dropped_transactions=belle_sip_list_size(obj->queued_ct);
	if (dropped_transactions>0) belle_sip_warning("dialog [%p]: leaves %u queued transaction aborted.",obj,(unsigned int)dropped_transactions);
	belle_sip_list_for_each(obj->queued_ct,(void(*)(void*))belle_sip_transaction_terminate);
	obj->queued_ct=belle_sip_list_free_with_data(obj->queued_ct,belle_sip_object_unref);
	belle_sip_provider_remove_dialog(obj->provider,obj);
}

void *belle_sip_dialog_get_application_data(const belle_sip_dialog_t *dialog){
	return dialog->appdata;
}

void belle_sip_dialog_set_application_data(belle_sip_dialog_t *dialog, void *data){
	dialog->appdata=data;
}

const belle_sip_header_call_id_t *belle_sip_dialog_get_call_id(const belle_sip_dialog_t *dialog){
	return dialog->call_id;
}

const belle_sip_header_address_t *belle_sip_dialog_get_local_party(const belle_sip_dialog_t *dialog){
	return dialog->local_party;
}

const belle_sip_header_address_t *belle_sip_dialog_get_remote_party(const belle_sip_dialog_t *dialog){
	return dialog->remote_party;
}

unsigned int belle_sip_dialog_get_local_seq_number(const belle_sip_dialog_t *dialog){
	return dialog->local_cseq;
}

unsigned int belle_sip_dialog_get_remote_seq_number(const belle_sip_dialog_t *dialog){
	return dialog->remote_cseq;
}

const char *belle_sip_dialog_get_local_tag(const belle_sip_dialog_t *dialog){
	return dialog->local_tag;
}

const char *belle_sip_dialog_get_remote_tag(const belle_sip_dialog_t *dialog){
	return dialog->remote_tag;
}

const belle_sip_header_address_t *belle_sip_dialog_get_remote_target(belle_sip_dialog_t *dialog){
	return dialog->remote_target;
}

const belle_sip_list_t* belle_sip_dialog_get_route_set(belle_sip_dialog_t *dialog){
	return dialog->route_set;
}

belle_sip_dialog_state_t belle_sip_dialog_get_state(const belle_sip_dialog_t *dialog){
	return dialog->state;
}

belle_sip_dialog_state_t belle_sip_dialog_get_previous_state(const belle_sip_dialog_t *dialog) {
	return dialog->previous_state;
}
int belle_sip_dialog_is_server(const belle_sip_dialog_t *dialog){
	return dialog->is_server;
}

int belle_sip_dialog_is_secure(const belle_sip_dialog_t *dialog){
	return dialog->is_secure;
}

void belle_sip_dialog_send_ack(belle_sip_dialog_t *obj, belle_sip_request_t *request){
	if (obj->needs_ack){
		obj->needs_ack=FALSE;
		if (obj->last_out_ack)
			belle_sip_object_unref(obj->last_out_ack);
		obj->last_out_ack=(belle_sip_request_t*)belle_sip_object_ref(request);
		belle_sip_provider_send_request(obj->provider,request);
		belle_sip_dialog_process_queue(obj);
	}else{
		belle_sip_error("Why do you want to send an ACK ?");
	}
}

void belle_sip_dialog_terminate_on_bye(belle_sip_dialog_t *obj, int val){
	obj->terminate_on_bye=val;
}

/*returns 1 if message belongs to the dialog, 0 otherwise */
int belle_sip_dialog_match(belle_sip_dialog_t *obj, belle_sip_message_t *msg, int as_uas){
	belle_sip_header_call_id_t *call_id=belle_sip_message_get_header_by_type(msg,belle_sip_header_call_id_t);
	belle_sip_header_from_t *from=belle_sip_message_get_header_by_type(msg,belle_sip_header_from_t);
	belle_sip_header_to_t *to=belle_sip_message_get_header_by_type(msg,belle_sip_header_to_t);
	const char *from_tag;
	const char *to_tag;
	const char *call_id_value;

	if (call_id==NULL || from==NULL || to==NULL) return 0;

	call_id_value=belle_sip_header_call_id_get_call_id(call_id);
	from_tag=belle_sip_header_from_get_tag(from);
	to_tag=belle_sip_header_to_get_tag(to);
	
	return _belle_sip_dialog_match(obj,call_id_value,as_uas ? to_tag : from_tag, as_uas ? from_tag : to_tag);
}

int _belle_sip_dialog_match(belle_sip_dialog_t *obj, const char *call_id, const char *local_tag, const char *remote_tag){
	const char *dcid;
	/*Dialog created by notify matching subscription are still in NULL state if (obj->state==BELLE_SIP_DIALOG_NULL) belle_sip_fatal("_belle_sip_dialog_match() must not be used for dialog in null state.");*/
	dcid=belle_sip_header_call_id_get_call_id(obj->call_id);
	return strcmp(dcid,call_id)==0
		&& strcmp(obj->local_tag,local_tag)==0 
		&& obj->remote_tag /* handle 180 without to tag */ && strcmp(obj->remote_tag,remote_tag)==0;
}

void belle_sip_dialog_check_ack_sent(belle_sip_dialog_t*obj){
	belle_sip_client_transaction_t* client_trans;
	if (obj->needs_ack){
		belle_sip_request_t *req;
		belle_sip_error("Your listener did not ACK'd the 200Ok for your INVITE request. The dialog will be terminated.");
		req=belle_sip_dialog_create_request(obj,"BYE");
		if (req){
			client_trans=belle_sip_provider_create_client_transaction(obj->provider,req);
			BELLE_SIP_TRANSACTION(client_trans)->is_internal=TRUE; /*internal transaction, don't bother user with 200ok*/
			belle_sip_client_transaction_send_request(client_trans);
			/*call dialog terminated*/
		}else{
			/*If there is no way to signal the failure to the other party, then force immediate dialog deletion*/
			belle_sip_dialog_delete(obj);
		}
	}
}
/*
 * return 0 if dialog handle the 200ok
 * */
static int belle_sip_dialog_handle_200Ok(belle_sip_dialog_t *obj, belle_sip_response_t *msg){
	if (obj->last_out_ack){
		belle_sip_header_cseq_t *cseq=belle_sip_message_get_header_by_type(msg,belle_sip_header_cseq_t);
		if (cseq){
			belle_sip_header_cseq_t *ack_cseq=belle_sip_message_get_header_by_type(obj->last_out_ack,belle_sip_header_cseq_t);
			if (belle_sip_header_cseq_get_seq_number(cseq)==belle_sip_header_cseq_get_seq_number(ack_cseq)){
				/*pass for retransmission*/
				belle_sip_message("Dialog retransmitting last ack automatically");
				belle_sip_provider_send_request(obj->provider,obj->last_out_ack);
				return 0;
			}else belle_sip_message("No already created ACK matching 200Ok for dialog [%p]",obj);
		}
	}
	return -1;
}

int belle_sip_dialog_handle_ack(belle_sip_dialog_t *obj, belle_sip_request_t *ack){
	belle_sip_header_cseq_t *cseq=belle_sip_message_get_header_by_type(ack,belle_sip_header_cseq_t);
	if (obj->needs_ack && belle_sip_header_cseq_get_seq_number(cseq)==obj->remote_invite_cseq){
		belle_sip_message("Incoming INVITE has ACK, dialog is happy");
		obj->needs_ack=FALSE;
		belle_sip_dialog_stop_200Ok_retrans(obj);
		belle_sip_dialog_process_queue(obj);
		return 0;
	}
	belle_sip_message("Dialog ignoring incoming ACK (surely a retransmission)");
	return -1;
}

belle_sip_transaction_t* belle_sip_dialog_get_last_transaction(const belle_sip_dialog_t *dialog) {
	return dialog->last_transaction;
}

int belle_sip_dialog_request_pending(const belle_sip_dialog_t *dialog){
	return dialog->needs_ack || (dialog->last_transaction ? belle_sip_transaction_state_is_transient(belle_sip_transaction_get_state(dialog->last_transaction)) : FALSE);
}

/* for notify exception
As per RFC 3265;
3.3.4. Dialog creation and termination

If an initial SUBSCRIBE request is not sent on a pre-existing dialog,
   the subscriber will wait for a response to the SUBSCRIBE request or a
   matching NOTIFY.
...
...

If an initial SUBSCRIBE is sent on a pre-existing dialog, a matching
   200-class response or successful NOTIFY request merely creates a new
   subscription associated with that dialog.
   */

int belle_sip_dialog_is_authorized_transaction(const belle_sip_dialog_t *dialog,const char* method) {
	if (belle_sip_dialog_request_pending(dialog)){
		const char* last_transaction_request;
		if (strcasecmp(method,"BYE")==0)
			return TRUE; /*don't reject a BYE*/
		
		last_transaction_request = belle_sip_request_get_method(belle_sip_transaction_get_request(dialog->last_transaction));
		if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(dialog->last_transaction,belle_sip_client_transaction_t)
			&& strcmp(last_transaction_request,"SUBSCRIBE")==0 && strcmp(method,"NOTIFY")==0){
			/*stupid as it is, you have to accept a NOTIFY for a SUBSCRIBE for which no answer is received yet...*/
			return TRUE;
		}
		if (strcmp(last_transaction_request,"INVITE")==0 && (strcmp(method,"PRACK")==0 || strcmp(method,"UPDATE")==0)){
			/*PRACK /UPDATE needs to be sent or received during reINVITEs.*/
			return TRUE;
		}
		return FALSE;
	} else {
		return TRUE;
	}
}

void belle_sip_dialog_queue_client_transaction(belle_sip_dialog_t *dialog, belle_sip_client_transaction_t *tr){
	dialog->queued_ct=belle_sip_list_append(dialog->queued_ct, belle_sip_object_ref(tr));
}

static void _belle_sip_dialog_process_queue(belle_sip_dialog_t* dialog){
	belle_sip_client_transaction_t *tr=NULL;
	
	if (dialog->state==BELLE_SIP_DIALOG_TERMINATED || belle_sip_dialog_request_pending(dialog)) goto end;
	
	dialog->queued_ct=belle_sip_list_pop_front(dialog->queued_ct,(void**)&tr);
	if (tr){
		belle_sip_message("Dialog [%p]: sending queued request.",dialog);
		tr->base.sent_by_dialog_queue=TRUE;
		belle_sip_client_transaction_send_request(tr);
		belle_sip_object_unref(tr);
	}
end:
	belle_sip_object_unref(dialog);
}

void belle_sip_dialog_process_queue(belle_sip_dialog_t* dialog){
	/*process queue does not process synchronously.
	 * This is to let the application handle responses and eventually submit new requests without being blocked.
	 * Typically when a reINVITE is challenged, we want a chance to re-submit an authenticated request before processing
	 * queued requests*/
	if (dialog->queued_ct){
		belle_sip_main_loop_do_later(dialog->provider->stack->ml,(belle_sip_callback_t)_belle_sip_dialog_process_queue,belle_sip_object_ref(dialog));
	}
}

void belle_sip_dialog_update_request(belle_sip_dialog_t *dialog, belle_sip_request_t *req){
	belle_sip_header_cseq_t *cseq=belle_sip_message_get_header_by_type(req,belle_sip_header_cseq_t);
	belle_sip_dialog_update_local_cseq(dialog,belle_sip_request_get_method(req));
	if (dialog->route_set)
		belle_sip_message_add_headers((belle_sip_message_t*)req,dialog->route_set);
	belle_sip_request_set_uri(req,belle_sip_header_address_get_uri(dialog->remote_target));
	belle_sip_header_cseq_set_seq_number(cseq,dialog->local_cseq);
}

int belle_sip_dialog_pending_trans_checking_enabled( const belle_sip_dialog_t *dialog) {
	return dialog->pending_trans_checking_enabled;
}

int belle_sip_dialog_enable_pending_trans_checking(belle_sip_dialog_t *dialog, int value) {
	dialog->pending_trans_checking_enabled = value;
	return 0;
}
