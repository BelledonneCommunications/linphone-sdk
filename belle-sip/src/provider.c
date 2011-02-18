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

belle_sip_provider_t *belle_sip_provider_new(belle_sip_stack_t *s, belle_sip_listening_point_t *lp){
	belle_sip_provider_t *p=belle_sip_object_new(belle_sip_provider_t,belle_sip_provider_uninit);
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
	p->listeners=belle_sip_list_remove_custom(p->listeners,listener_ctx_compare,&ctx);
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

static void sender_task_cb(belle_sip_sender_task_t *t, void *data, int retcode){
	if (retcode!=0){
		/*would need to notify the application of the failure */
	}
	belle_sip_object_unref(t);
}

void belle_sip_provider_send_request(belle_sip_provider_t *p, belle_sip_request_t *req){
	belle_sip_sender_task_t *task;

	task=belle_sip_sender_task_new(p,  sender_task_cb, NULL);
	belle_sip_sender_task_send(task,BELLE_SIP_MESSAGE(req));
}

void belle_sip_provider_send_response(belle_sip_provider_t *p, belle_sip_response_t *resp){
	belle_sip_sender_task_t *task;

	task=belle_sip_sender_task_new(p,  sender_task_cb, NULL);
	belle_sip_sender_task_send(task,BELLE_SIP_MESSAGE(resp));
}

/*private provider API*/

void belle_sip_provider_set_transaction_terminated(belle_sip_provider_t *p, belle_sip_transaction_t *t){
	belle_sip_transaction_terminated_event_t ev;
	ev.source=p;
	ev.transaction=t;
	ev.is_server_transaction=t->is_server;
	BELLE_SIP_PROVIDER_INVOKE_LISTENERS(p,process_transaction_terminated,&ev);
}

