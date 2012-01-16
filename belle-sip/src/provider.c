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



static int listener_ctx_compare(const void *c1, const void *c2){
	listener_ctx_t *lc1=(listener_ctx_t*)c1;
	listener_ctx_t *lc2=(listener_ctx_t*)c2;
	return !(lc1->listener==lc2->listener && lc1->data==lc2->data);
}


static void belle_sip_provider_uninit(belle_sip_provider_t *p){
	belle_sip_list_for_each (p->listeners,belle_sip_free);
	belle_sip_list_free(p->listeners);
	belle_sip_list_free(p->lps);
}

static void channel_state_changed(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_channel_state_t state){
}

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_sip_provider_t,belle_sip_channel_listener_t)
	channel_state_changed
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_sip_provider_t,belle_sip_channel_listener_t);
	
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_provider_t,belle_sip_object_t,belle_sip_provider_uninit,NULL,NULL);

belle_sip_provider_t *belle_sip_provider_new(belle_sip_stack_t *s, belle_sip_listening_point_t *lp){
	belle_sip_provider_t *p=belle_sip_object_new(belle_sip_provider_t);
	p->stack=s;
	belle_sip_provider_add_listening_point(p,lp);
	return p;
}

int belle_sip_provider_add_listening_point(belle_sip_provider_t *p, belle_sip_listening_point_t *lp){
	p->lps=belle_sip_list_append(p->lps,lp);
	return 0;
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

void belle_sip_provider_add_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l, void *user_ctx){
	listener_ctx_t *lc=belle_sip_new(listener_ctx_t);
	lc->listener=l;
	lc->data=user_ctx;
	p->listeners=belle_sip_list_append(p->listeners,lc);
}

void belle_sip_provider_remove_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l, void *user_ctx){
	listener_ctx_t ctx={l,user_ctx};
	p->listeners=belle_sip_list_delete_custom(p->listeners,listener_ctx_compare,&ctx);
}

belle_sip_header_call_id_t * belle_sip_provider_create_call_id(belle_sip_provider_t *prov){
	belle_sip_header_call_id_t *cid=belle_sip_header_call_id_new();
	char tmp[32];
	snprintf(tmp,sizeof(tmp),"%u",belle_sip_random());
	belle_sip_header_call_id_set_call_id(cid,tmp);
	return cid;
}

belle_sip_client_transaction_t *belle_sip_provider_create_client_transaction(belle_sip_provider_t *p, belle_sip_request_t *req){
	return belle_sip_client_transaction_new(p,req);
}

belle_sip_server_transaction_t *belle_sip_provider_create_server_transaction(belle_sip_provider_t *p, belle_sip_request_t *req){
	return belle_sip_server_transaction_new(p,req);
}

belle_sip_stack_t *belle_sip_provider_get_sip_stack(belle_sip_provider_t *p){
	return p->stack;
}

belle_sip_channel_t * belle_sip_provider_get_channel(belle_sip_provider_t *p, const char *name,
                                                   int port, const char *transport){
	return NULL;
}


void belle_sip_provider_send_request(belle_sip_provider_t *p, belle_sip_request_t *req){
	
}

void belle_sip_provider_send_response(belle_sip_provider_t *p, belle_sip_response_t *resp){
	
}

/*private provider API*/

void belle_sip_provider_set_transaction_terminated(belle_sip_provider_t *p, belle_sip_transaction_t *t){
	belle_sip_transaction_terminated_event_t ev;
	ev.source=p;
	ev.transaction=t;
	ev.is_server_transaction=t->is_server;
	BELLE_SIP_PROVIDER_INVOKE_LISTENERS(p,process_transaction_terminated,&ev);
}

