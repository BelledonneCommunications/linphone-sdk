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

static void belle_sip_transaction_init(belle_sip_transaction_t *t, belle_sip_provider_t *prov, belle_sip_request_t *req){
	if (req) belle_sip_object_ref(req);
	t->request=req;
	t->provider=prov;
}

static void transaction_destroy(belle_sip_transaction_t *t){
	if (t->request) belle_sip_object_unref(t->request);
	if (t->prov_response) belle_sip_object_unref(t->prov_response);
	if (t->final_response) belle_sip_object_unref(t->final_response);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_transaction_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_transaction_t)={
		BELLE_SIP_VPTR_INIT(belle_sip_transaction_t,belle_sip_object_t,FALSE),
		(belle_sip_object_destroy_t) transaction_destroy,
		NULL,/*no clone*/
		NULL,/*no marshall*/
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
	belle_sip_provider_set_transaction_terminated(t->provider,t);
}

belle_sip_request_t *belle_sip_transaction_get_request(belle_sip_transaction_t *t){
	return t->request;
}

/*
 * Server transaction
 */

static void server_transaction_destroy(belle_sip_server_transaction_t *t){
}


BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_server_transaction_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_server_transaction_t)={
		{
			BELLE_SIP_VPTR_INIT(belle_sip_server_transaction_t,belle_sip_transaction_t,FALSE),
			(belle_sip_object_destroy_t) server_transaction_destroy,
			NULL,
			NULL
		}
};

belle_sip_server_transaction_t * belle_sip_server_transaction_new(belle_sip_provider_t *prov,belle_sip_request_t *req){
	belle_sip_server_transaction_t *t=belle_sip_object_new(belle_sip_server_transaction_t);
	belle_sip_transaction_init((belle_sip_transaction_t*)t,prov,req);
	return t;
}

void belle_sip_server_transaction_send_response(belle_sip_server_transaction_t *t, belle_sip_response_t *resp){
	//BELLE_SIP_OBJECT_VPTR(t,belle_sip_transaction_t)->on_response((belle_sip_transaction_t*)t,resp);
}


belle_sip_request_t * belle_sip_client_transaction_create_cancel(belle_sip_client_transaction_t *t){
	return NULL;
}

/*
 * client transaction
 */

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
		belle_sip_channel_add_listener(chan,BELLE_SIP_CHANNEL_LISTENER(t));
		t->base.channel=chan;
		if (belle_sip_channel_get_state(chan)==BELLE_SIP_CHANNEL_READY){
			BELLE_SIP_OBJECT_VPTR(t,belle_sip_client_transaction_t)->send_request(t);
		}
	}
}

static void client_transaction_destroy(belle_sip_client_transaction_t *t ){
}

static void on_channel_state_changed(belle_sip_channel_listener_t *l, belle_sip_channel_t *chan, belle_sip_channel_state_t state){
	belle_sip_client_transaction_t *t=(belle_sip_client_transaction_t*)l;
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
		BELLE_SIP_VPTR_INIT(belle_sip_client_transaction_t,belle_sip_transaction_t,FALSE),
		(belle_sip_object_destroy_t)client_transaction_destroy,
		NULL,
		NULL
	},
	NULL,
	NULL
};

void belle_sip_client_transaction_init(belle_sip_client_transaction_t *obj, belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_transaction_init((belle_sip_transaction_t*)obj, prov,req);
}


