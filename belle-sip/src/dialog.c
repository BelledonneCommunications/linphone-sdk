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

static void belle_sip_dialog_init_200Ok_retrans(belle_sip_dialog_t *obj, belle_sip_response_t *resp);
static void belle_sip_dialog_stop_200Ok_retrans(belle_sip_dialog_t *obj);
static void belle_sip_dialog_handle_200Ok(belle_sip_dialog_t *obj, belle_sip_response_t *msg);

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
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_dialog_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_dialog_t)={ 
		BELLE_SIP_VPTR_INIT(belle_sip_dialog_t, belle_sip_object_t,FALSE),
		(belle_sip_object_destroy_t)belle_sip_dialog_uninit,
		NULL,
		NULL
};
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

static int belle_sip_dialog_init_as_uas(belle_sip_dialog_t *obj, belle_sip_request_t *req, belle_sip_response_t *resp){
	const belle_sip_list_t *elem;
	belle_sip_header_contact_t *ct=belle_sip_message_get_header_by_type(req,belle_sip_header_contact_t);
	belle_sip_header_cseq_t *cseq=belle_sip_message_get_header_by_type(req,belle_sip_header_cseq_t);
	belle_sip_header_to_t *to=belle_sip_message_get_header_by_type(resp,belle_sip_header_to_t);
	belle_sip_header_via_t *via=belle_sip_message_get_header_by_type(req,belle_sip_header_via_t);
	belle_sip_uri_t *requri=belle_sip_request_get_uri(req);

	if (!ct){
		belle_sip_error("No contact in request.");
		return -1;
	}
	if (!to){
		belle_sip_error("No to in response.");
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
	/*call id already set */
	/*remote party already set */
	obj->local_party=(belle_sip_header_address_t*)belle_sip_object_ref(to);
	if (strcmp(belle_sip_request_get_method(req),"INVITE")==0){
		belle_sip_dialog_init_200Ok_retrans(obj,resp);
		obj->needs_ack=TRUE;
	}
	return 0;
}

static void set_last_out_invite(belle_sip_dialog_t *obj, belle_sip_request_t *req){
	if (obj->last_out_invite)
		belle_sip_object_unref(obj->last_out_invite);
	obj->last_out_invite=(belle_sip_request_t*)belle_sip_object_ref(req);
}

static int belle_sip_dialog_init_as_uac(belle_sip_dialog_t *obj, belle_sip_request_t *req, belle_sip_response_t *resp){
	const belle_sip_list_t *elem;
	belle_sip_header_contact_t *ct=belle_sip_message_get_header_by_type(resp,belle_sip_header_contact_t);
	belle_sip_header_cseq_t *cseq=belle_sip_message_get_header_by_type(req,belle_sip_header_cseq_t);
	belle_sip_header_to_t *to=belle_sip_message_get_header_by_type(resp,belle_sip_header_to_t);
	belle_sip_header_via_t *via=belle_sip_message_get_header_by_type(req,belle_sip_header_via_t);
	belle_sip_uri_t *requri=belle_sip_request_get_uri(req);

	if (!ct){
		belle_sip_error("No contact in response.");
		return -1;
	}
	if (!to){
		belle_sip_error("No to in response.");
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
	if (strcasecmp(belle_sip_header_via_get_protocol(via),"TLS")==0
	    && belle_sip_uri_is_secure(requri)){
		obj->is_secure=TRUE;
	}
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
	obj->remote_target=(belle_sip_header_address_t*)belle_sip_object_ref(ct);
	obj->local_cseq=belle_sip_header_cseq_get_seq_number(cseq);
	/*call id is already set */
	/*local_tag is already set*/
	obj->remote_party=(belle_sip_header_address_t*)belle_sip_object_ref(to);
	/*local party is already set*/
	if (strcmp(belle_sip_request_get_method(req),"INVITE")==0){
		set_last_out_invite(obj,req);
		obj->needs_ack=TRUE;
	}
	return 0;
}

int belle_sip_dialog_establish_full(belle_sip_dialog_t *obj, belle_sip_request_t *req, belle_sip_response_t *resp){
	int err;
	if (obj->is_server)
		err= belle_sip_dialog_init_as_uas(obj,req,resp);
	else
		err= belle_sip_dialog_init_as_uac(obj,req,resp);
	if (err==0) set_state(obj,BELLE_SIP_DIALOG_CONFIRMED);
	return err;
}

int belle_sip_dialog_establish(belle_sip_dialog_t *obj, belle_sip_request_t *req, belle_sip_response_t *resp){
	int code=belle_sip_response_get_status_code(resp);
	belle_sip_header_to_t *to=belle_sip_message_get_header_by_type(resp,belle_sip_header_to_t);
	belle_sip_header_call_id_t *call_id=belle_sip_message_get_header_by_type(req,belle_sip_header_call_id_t);

	if (!to){
		belle_sip_error("No to in response.");
		return -1;
	}
	if (!call_id){
		belle_sip_error("No call-id in response.");
		return -1;
	}
	
	if (code>100 && code<200){
		if (obj->state==BELLE_SIP_DIALOG_NULL){
			set_to_tag(obj,to);
			obj->call_id=(belle_sip_header_call_id_t*)belle_sip_object_ref(call_id);
			set_state(obj,BELLE_SIP_DIALOG_EARLY);
		}
		return -1;
	}else if (code>=200 && code<300){
		if (obj->state==BELLE_SIP_DIALOG_NULL){
			set_to_tag(obj,to);
			obj->call_id=(belle_sip_header_call_id_t*)belle_sip_object_ref(call_id);
		}
		if (belle_sip_dialog_establish_full(obj,req,resp)==-1){
			return -1;
		}
	} else if (code>=300 && obj->state!=BELLE_SIP_DIALOG_CONFIRMED) {
		/*12.3 Termination of a Dialog
   	   	   Independent of the method, if a request outside of a dialog generates
   	   	   a non-2xx final response, any early dialogs created through
   	   	   provisional responses to that request are terminated.  The mechanism
   	   	   for terminating confirmed dialogs is method specific.*/
		belle_sip_dialog_delete(obj);
	}
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
	belle_sip_source_set_timeout(dialog->timer_200Ok,MIN(2*prev_timeout,cfg->T2));
	belle_sip_message("Dialog sending retransmission of 200Ok");
	belle_sip_provider_send_response(dialog->provider,dialog->last_200Ok);
	return BELLE_SIP_CONTINUE;
}

static int dialog_on_200Ok_end(belle_sip_dialog_t *dialog){
	belle_sip_request_t *bye;
	belle_sip_client_transaction_t *trn;
	belle_sip_dialog_stop_200Ok_retrans(dialog);
	belle_sip_error("Dialog was not ACK'd within T1*64 seconds, it is going to be terminated.");
	dialog->state=BELLE_SIP_DIALOG_CONFIRMED;
	bye=belle_sip_dialog_create_request(dialog,"BYE");
	trn=belle_sip_provider_create_client_transaction(dialog->provider,bye);
	belle_sip_client_transaction_send_request(trn);
	return BELLE_SIP_STOP;
}

static void belle_sip_dialog_init_200Ok_retrans(belle_sip_dialog_t *obj, belle_sip_response_t *resp){
	const belle_sip_timer_config_t *cfg=belle_sip_stack_get_timer_config(obj->provider->stack);
	obj->timer_200Ok=belle_sip_timeout_source_new((belle_sip_source_func_t)dialog_on_200Ok_timer,obj,cfg->T1);
	belle_sip_object_set_name((belle_sip_object_t*)obj->timer_200Ok,"dialog_200Ok_timer");
	belle_sip_main_loop_add_source(obj->provider->stack->ml,obj->timer_200Ok);
	
	obj->timer_200Ok_end=belle_sip_timeout_source_new((belle_sip_source_func_t)dialog_on_200Ok_end,obj,cfg->T1*64);
	belle_sip_object_set_name((belle_sip_object_t*)obj->timer_200Ok_end,"dialog_200Ok_timer_end");
	belle_sip_main_loop_add_source(obj->provider->stack->ml,obj->timer_200Ok_end);
	
	obj->last_200Ok=(belle_sip_response_t*)belle_sip_object_ref(resp);
}

static void belle_sip_dialog_stop_200Ok_retrans(belle_sip_dialog_t *obj){
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

int belle_sip_dialog_update(belle_sip_dialog_t *obj,belle_sip_request_t *req, belle_sip_response_t *resp, int as_uas){
	int code;
	switch (obj->state){
		case BELLE_SIP_DIALOG_NULL:
		case BELLE_SIP_DIALOG_EARLY:
			return belle_sip_dialog_establish(obj,req,resp);
		case BELLE_SIP_DIALOG_CONFIRMED:
			code=belle_sip_response_get_status_code(resp);
			if (strcmp(belle_sip_request_get_method(req),"INVITE")==0 && code>=200 && code<300){
				if (!obj->needs_ack){
					/*case of a target refresh request */
					/*refresh the remote_target*/
					belle_sip_header_contact_t *ct;
					if (as_uas){
						ct=belle_sip_message_get_header_by_type(req,belle_sip_header_contact_t);
					}else{
						set_last_out_invite(obj,req);
						ct=belle_sip_message_get_header_by_type(resp,belle_sip_header_contact_t);
					}
					if (ct){
						belle_sip_object_unref(obj->remote_target);
						obj->remote_target=(belle_sip_header_address_t*)belle_sip_object_ref(ct);
					}
					obj->needs_ack=TRUE;
				} else {
					/*retransmission of 200Ok */
					if (!as_uas) belle_sip_dialog_handle_200Ok(obj,resp);
				}
			}else if (strcmp(belle_sip_request_get_method(req),"BYE")==0 && ((code>=200 && code<300) || code==481 || code==408)){
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

				if (obj->terminate_on_bye) belle_sip_dialog_delete(obj);
				obj->needs_ack=FALSE; /*no longuer need ACK*/
			}
		break;
		case BELLE_SIP_DIALOG_TERMINATED:
			/*ignore*/
		break;
	}
	return 0;
}

belle_sip_dialog_t *belle_sip_dialog_new(belle_sip_transaction_t *t){
	belle_sip_dialog_t *obj;
	belle_sip_header_from_t *from;
	const char *from_tag;
	belle_sip_header_to_t *to;
	const char *to_tag=NULL;

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

	if (t->last_response) {
		to=belle_sip_message_get_header_by_type(t->last_response,belle_sip_header_to_t);
		if (to==NULL){
			belle_sip_error("belle_sip_dialog_new(): no to!");
			return NULL;
		}
		to_tag=belle_sip_header_to_get_tag(to);
	}
	obj=belle_sip_object_new(belle_sip_dialog_t);
	obj->terminate_on_bye=1;
	obj->provider=t->provider;
	
	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(t,belle_sip_server_transaction_t)){
		obj->remote_tag=belle_sip_strdup(from_tag);
		obj->local_tag=to_tag?belle_sip_strdup(to_tag):NULL; /*might be null at dialog creation*/
		obj->remote_party=(belle_sip_header_address_t*)belle_sip_object_ref(from);
		obj->is_server=TRUE;
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
	}
	belle_sip_message("New %s dialog [%x] , local tag [%s], remote tag [%s]"
			,obj->is_server?"server":"client"
			,obj
			,obj->local_tag
			,obj->remote_tag);
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
	ack=belle_sip_dialog_create_request(obj,"ACK");
	if (ack){
		const belle_sip_list_t *aut=belle_sip_message_get_headers((belle_sip_message_t*)obj->last_out_invite,"Authorization");
		const belle_sip_list_t *prx_aut=belle_sip_message_get_headers((belle_sip_message_t*)obj->last_out_invite,"Proxy-Authorization");
		if (aut)
			belle_sip_message_add_headers((belle_sip_message_t*)ack,aut);
		if (prx_aut)
			belle_sip_message_add_headers((belle_sip_message_t*)ack,prx_aut);
	}
	return ack;
}

belle_sip_request_t *belle_sip_dialog_create_request(belle_sip_dialog_t *obj, const char *method){
	if (obj->state != BELLE_SIP_DIALOG_CONFIRMED && obj->state != BELLE_SIP_DIALOG_EARLY) {
		belle_sip_error("Cannot create method [%s] from dialog [%p] in state [%s]",method,obj,belle_sip_dialog_state_to_string(obj->state));
		return NULL;
	}
	if (obj->local_cseq==0) obj->local_cseq=110;
	if (strcmp(method,"ACK")!=0) obj->local_cseq++;
	belle_sip_request_t *req=belle_sip_request_create(belle_sip_header_address_get_uri(obj->remote_target),
	                                                method,
	                                                obj->call_id,
	                                                belle_sip_header_cseq_create(obj->local_cseq,method),
	                                                belle_sip_header_from_create(obj->local_party,NULL),
	                                                belle_sip_header_to_create(obj->remote_party,NULL),
	                                                belle_sip_header_via_new(),
	                                                0);
	if (obj->route_set) {
		belle_sip_message_add_headers((belle_sip_message_t*)req,obj->route_set);
	}
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

belle_sip_request_t *belle_sip_dialog_create_request_from(belle_sip_dialog_t *obj, const belle_sip_request_t *initial_req){
	belle_sip_request_t* req = belle_sip_dialog_create_request(obj, belle_sip_request_get_method(initial_req));
	belle_sip_header_content_length_t* content_lenth = belle_sip_message_get_header_by_type(initial_req,belle_sip_header_content_length_t);
	/*first copy non system headers*/
	belle_sip_list_t* headers = belle_sip_message_get_all_headers(BELLE_SIP_MESSAGE(initial_req));
	belle_sip_list_for_each2(headers,(void (*)(void *, void *))copy_non_system_headers,req);
	belle_sip_list_free(headers);
	/*copy body*/
	if (content_lenth && belle_sip_header_content_length_get_content_length(content_lenth)>0) {
		belle_sip_message_set_body(BELLE_SIP_MESSAGE(req),belle_sip_message_get_body(BELLE_SIP_MESSAGE(initial_req)),belle_sip_header_content_length_get_content_length(content_lenth));
	}
	return req;
}

void belle_sip_dialog_delete(belle_sip_dialog_t *obj){
	belle_sip_dialog_stop_200Ok_retrans(obj); /*if any*/
	set_state(obj,BELLE_SIP_DIALOG_TERMINATED);
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

const char *belle_sip_dialog_get_dialog_id(const belle_sip_dialog_t *dialog){
	return NULL;
}

const belle_sip_header_address_t *belle_sip_get_local_party(const belle_sip_dialog_t *dialog){
	return dialog->local_party;
}

const belle_sip_header_address_t *belle_sip_get_remote_party(const belle_sip_dialog_t *dialog){
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
	if (obj->state==BELLE_SIP_DIALOG_NULL) belle_sip_fatal("_belle_sip_dialog_match() must not be used for dialog in null state.");
	dcid=belle_sip_header_call_id_get_call_id(obj->call_id);
	return strcmp(dcid,call_id)==0 && strcmp(obj->local_tag,local_tag)==0 && strcmp(obj->remote_tag,remote_tag)==0;
}

void belle_sip_dialog_check_ack_sent(belle_sip_dialog_t*obj){
	belle_sip_client_transaction_t* client_trans;
	if (obj->needs_ack){
		belle_sip_request_t *req;
		belle_sip_error("Your listener did not ACK'd the 200Ok for your INVITE request. The dialog will be terminated.");
		req=belle_sip_dialog_create_request(obj,"BYE");
		client_trans=belle_sip_provider_create_client_transaction(obj->provider,req);
		BELLE_SIP_TRANSACTION(client_trans)->is_internal=TRUE; /*internal transaction, don't bother user with 200ok*/
		belle_sip_client_transaction_send_request(client_trans);

	}
}

static void belle_sip_dialog_handle_200Ok(belle_sip_dialog_t *obj, belle_sip_response_t *msg){
	if (obj->last_out_ack){
		belle_sip_header_cseq_t *cseq=belle_sip_message_get_header_by_type(msg,belle_sip_header_cseq_t);
		if (cseq){
			belle_sip_header_cseq_t *ack_cseq=belle_sip_message_get_header_by_type(msg,belle_sip_header_cseq_t);
			if (belle_sip_header_cseq_get_seq_number(cseq)==belle_sip_header_cseq_get_seq_number(ack_cseq)){
				/*pass for retransmission*/
				belle_sip_message("Dialog retransmitting last ack automatically");
				belle_sip_provider_send_request(obj->provider,obj->last_out_ack);
			}else belle_sip_warning("No ACK to retransmit matching 200Ok");
		}
	}
}

int belle_sip_dialog_handle_ack(belle_sip_dialog_t *obj, belle_sip_request_t *ack){
	belle_sip_header_cseq_t *cseq=belle_sip_message_get_header_by_type(ack,belle_sip_header_cseq_t);
	if (obj->needs_ack && belle_sip_header_cseq_get_seq_number(cseq)==obj->remote_cseq){
		belle_sip_message("Incoming INVITE has ACK, dialog is happy");
		obj->needs_ack=FALSE;
		belle_sip_dialog_stop_200Ok_retrans(obj);
		return 0;
	}
	belle_sip_message("Dialog ignoring incoming ACK (surely a retransmission)");
	return -1;
}
