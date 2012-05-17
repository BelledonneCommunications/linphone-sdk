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
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			return "PROCEEDING";
		case BELLE_SIP_TRANSACTION_TERMINATED:
			return "TERMINATED";
	}
	belle_sip_fatal("Invalid transaction state.");
	return "INVALID";
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
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_transaction_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_transaction_t)={
	{
		BELLE_SIP_VPTR_INIT(belle_sip_transaction_t,belle_sip_object_t,FALSE),
		(belle_sip_object_destroy_t) transaction_destroy,
		NULL,/*no clone*/
		NULL,/*no marshall*/
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

void belle_sip_transaction_terminate(belle_sip_transaction_t *t){
	t->state=BELLE_SIP_TRANSACTION_TERMINATED;
	BELLE_SIP_OBJECT_VPTR(t,belle_sip_transaction_t)->on_terminate(t);
	belle_sip_provider_set_transaction_terminated(t->provider,t);
}

belle_sip_request_t *belle_sip_transaction_get_request(belle_sip_transaction_t *t){
	return t->request;
}

void belle_sip_transaction_notify_timeout(belle_sip_transaction_t *t){
	belle_sip_timeout_event_t ev;
	ev.source=t->provider;
	ev.transaction=t;
	ev.is_server_transaction=BELLE_SIP_OBJECT_IS_INSTANCE_OF(t,belle_sip_server_transaction_t);
	BELLE_SIP_PROVIDER_INVOKE_LISTENERS(t->provider,process_timeout,&ev);
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
	belle_sip_transaction_init((belle_sip_transaction_t*)t,prov,req);
	belle_sip_random_token(t->to_tag,sizeof(t->to_tag));
}

void belle_sip_server_transaction_send_response(belle_sip_server_transaction_t *t, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)t;
	belle_sip_header_to_t *to=(belle_sip_header_to_t*)belle_sip_message_get_header((belle_sip_message_t*)resp,"to");
	belle_sip_dialog_t *dialog=base->dialog;
	
	belle_sip_object_ref(resp);
	if (!base->last_response){
		belle_sip_hop_t hop;
		belle_sip_response_get_return_hop(resp,&hop);
		base->channel=belle_sip_provider_get_channel(base->provider,hop.host, hop.port, hop.transport);
		belle_sip_object_ref(base->channel);
		belle_sip_hop_free(&hop);
	}
	if (belle_sip_header_to_get_tag(to)==NULL && belle_sip_response_get_status_code(resp)!=100){
		//add a random to tag
		belle_sip_header_to_set_tag(to,t->to_tag);
	}
	if (BELLE_SIP_OBJECT_VPTR(t,belle_sip_server_transaction_t)->send_new_response(t,resp)==0){
		if (base->last_response)
			belle_sip_object_unref(base->last_response);
		base->last_response=resp;
	}
	if (dialog)
		belle_sip_dialog_update(dialog,base->request,resp,TRUE);
}

void belle_sip_server_transaction_on_request(belle_sip_server_transaction_t *t, belle_sip_request_t *req){
	const char *method=belle_sip_request_get_method(req);
	if (strcmp(method,"ACK")==0){
		/*this must be for an INVITE server transaction */
		if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(t,belle_sip_ist_t)){
			belle_sip_ist_t *ist=(belle_sip_ist_t*)t;
			belle_sip_ist_process_ack(ist,(belle_sip_message_t*)req);
		}else{
			belle_sip_warning("ACK received for non-invite server transaction ?");
		}
	}else if (strcmp(method,"CANCEL")==0){
		/*just notify the application */
		belle_sip_request_event_t event;

		event.source=t->base.provider;
		event.server_transaction=t;
		event.dialog=t->base.dialog;
		event.request=req;
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(t->base.provider,process_request_event,&event);
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
	if (t->base.state==BELLE_SIP_TRANSACTION_PROCEEDING){
		belle_sip_error("belle_sip_client_transaction_create_cancel() can only be used in state BELLE_SIP_TRANSACTION_PROCEEDING"
		               " but current transaction state is %s",belle_sip_transaction_state_to_string(t->base.state));
	}
	req=belle_sip_request_new();
	belle_sip_request_set_method(req,"CANCEL");
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


void belle_sip_client_transaction_send_request(belle_sip_client_transaction_t *t){
	belle_sip_hop_t hop={0};
	belle_sip_channel_t *chan;
	belle_sip_provider_t *prov=t->base.provider;
	
	if (t->base.state!=BELLE_SIP_TRANSACTION_INIT){
		belle_sip_error("belle_sip_client_transaction_send_request: bad state.");
		return;
	}
	belle_sip_stack_get_next_hop(prov->stack,t->base.request,&hop);
	chan=belle_sip_provider_get_channel(prov,hop.host, hop.port, hop.transport);
	if (chan){
		belle_sip_provider_add_client_transaction(t->base.provider,t);
		belle_sip_object_ref(chan);
		belle_sip_channel_add_listener(chan,BELLE_SIP_CHANNEL_LISTENER(t));
		t->base.channel=chan;
		if (belle_sip_channel_get_state(chan)==BELLE_SIP_CHANNEL_INIT)
			belle_sip_channel_prepare(chan);
		if (belle_sip_channel_get_state(chan)!=BELLE_SIP_CHANNEL_READY){
			belle_sip_message("belle_sip_client_transaction_send_request(): waiting channel to be ready");
		} else {
			BELLE_SIP_OBJECT_VPTR(t,belle_sip_client_transaction_t)->send_request(t);
		}
	}else belle_sip_error("belle_sip_client_transaction_send_request(): no channel available");
	belle_sip_hop_free(&hop);
}

void belle_sip_client_transaction_notify_response(belle_sip_client_transaction_t *t, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)t;
	belle_sip_response_event_t event;
	belle_sip_dialog_t *dialog=base->dialog;
		
	if (base->last_response)
		belle_sip_object_unref(base->last_response);
	base->last_response=(belle_sip_response_t*)belle_sip_object_ref(resp);

	if (dialog){
		if (dialog->state==BELLE_SIP_DIALOG_EARLY || dialog->state==BELLE_SIP_DIALOG_CONFIRMED){
			/*make sure this response matches the current dialog, or creates a new one*/
			if (!belle_sip_dialog_match(dialog,(belle_sip_message_t*)resp,FALSE)){
				dialog=belle_sip_dialog_new(base);
				if (dialog){
					belle_sip_message("Handling response creating a new dialog !");
				}
			}
		}
		if (dialog) belle_sip_dialog_update(dialog,base->request,resp,FALSE);
	}
	event.source=base->provider;
	event.client_transaction=t;
	event.dialog=dialog;
	event.response=(belle_sip_response_t*)resp;
	BELLE_SIP_PROVIDER_INVOKE_LISTENERS(base->provider,process_response_event,&event);
}


void belle_sip_client_transaction_add_response(belle_sip_client_transaction_t *t, belle_sip_response_t *resp){
	BELLE_SIP_OBJECT_VPTR(t,belle_sip_client_transaction_t)->on_response(t,resp);
}

static void client_transaction_destroy(belle_sip_client_transaction_t *t ){
}

static void on_channel_state_changed(belle_sip_channel_listener_t *l, belle_sip_channel_t *chan, belle_sip_channel_state_t state){
	belle_sip_client_transaction_t *t=(belle_sip_client_transaction_t*)l;
	belle_sip_message("transaction on_channel_state_changed");
	switch(state){
		case BELLE_SIP_CHANNEL_READY:
			BELLE_SIP_OBJECT_VPTR(t,belle_sip_client_transaction_t)->send_request(t);
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
	char token[10];

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


