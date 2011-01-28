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
#include "sender_task.h"

struct belle_sip_transaction{
	belle_sip_object_t base;
	belle_sip_provider_t *provider; /*the provider that created this transaction */
	belle_sip_request_t *request;
	char *branch_id;
	belle_sip_transaction_state_t state;
	belle_sip_sender_task_t *stask;
	void *appdata;
};


static unsigned long transaction_add_timeout(belle_sip_transaction_t *t, belle_sip_source_func_t func, unsigned int time_ms){
	belle_sip_stack_t *stack=belle_sip_provider_get_sip_stack(t->provider);
	return belle_sip_main_loop_add_timeout (stack->ml,func,t,time_ms);
}

/*
static void transaction_remove_timeout(belle_sip_transaction_t *t, unsigned long id){
	belle_sip_stack_t *stack=belle_sip_provider_get_sip_stack(t->provider);
	belle_sip_main_loop_cancel_source (stack->ml,id);
}
*/

static void belle_sip_transaction_init(belle_sip_transaction_t *t, belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_object_init_type(t,belle_sip_transaction_t);
	if (req) belle_sip_object_ref(req);
	t->request=req;
	t->provider=prov;
}

static void transaction_destroy(belle_sip_transaction_t *t){
	if (t->request) belle_sip_object_unref(t->request);
	if (t->stask) belle_sip_object_unref(t->stask);
}

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
	
}

belle_sip_request_t *belle_sip_transaction_get_request(belle_sip_transaction_t *t){
	return t->request;
}

/*
 Server transaction
*/

struct belle_sip_server_transaction{
	belle_sip_transaction_t base;
};

void belle_sip_server_transaction_send_response(belle_sip_server_transaction_t *t){
}

static void server_transaction_destroy(belle_sip_server_transaction_t *t){
	transaction_destroy((belle_sip_transaction_t*)t);
}

belle_sip_server_transaction_t * belle_sip_server_transaction_new(belle_sip_provider_t *prov,belle_sip_request_t *req){
	belle_sip_server_transaction_t *t=belle_sip_object_new(belle_sip_server_transaction_t,(belle_sip_object_destroy_t)server_transaction_destroy);
	belle_sip_transaction_init((belle_sip_transaction_t*)t,prov,req);
	return t;
}

/*
 Client transaction
*/

struct belle_sip_client_transaction{
	belle_sip_transaction_t base;
	unsigned long timer_id;
	uint64_t start_time;
	uint64_t time_F;
	uint64_t time_E;
};

belle_sip_request_t * belle_sip_client_transaction_create_cancel(belle_sip_client_transaction_t *t){
	return NULL;
}

static int on_client_transaction_timer(void *data, unsigned int revents){
	return BELLE_SIP_CONTINUE;
}

static void client_transaction_cb(belle_sip_sender_task_t *task, void *data, int retcode){
	belle_sip_client_transaction_t *t=(belle_sip_client_transaction_t*)data;
	const belle_sip_timer_config_t *tc=belle_sip_stack_get_timer_config (belle_sip_provider_get_sip_stack (t->base.provider));
	if (retcode==0){
		t->base.state=BELLE_SIP_TRANSACTION_TRYING;
		t->timer_id=transaction_add_timeout(&t->base,on_client_transaction_timer,tc->T1);
	}else{
		belle_sip_transaction_terminated_event_t ev;
		ev.source=t->base.provider;
		ev.transaction=(belle_sip_transaction_t*)t;
		ev.is_server_transaction=FALSE;
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(t->base.provider,process_transaction_terminated,&ev);
	}
}


void belle_sip_client_transaction_send_request(belle_sip_client_transaction_t *t){
	t->base.stask=belle_sip_sender_task_new(t->base.provider,BELLE_SIP_MESSAGE(t->base.request),client_transaction_cb,t);
	belle_sip_sender_task_send(t->base.stask);
}

static void client_transaction_destroy(belle_sip_client_transaction_t *t ){
	transaction_destroy((belle_sip_transaction_t*)t);
}


belle_sip_client_transaction_t * belle_sip_client_transaction_new(belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_client_transaction_t *t=belle_sip_object_new(belle_sip_client_transaction_t,(belle_sip_object_destroy_t)client_transaction_destroy);
	belle_sip_transaction_init((belle_sip_transaction_t*)t,prov,req);
	return t;
}



