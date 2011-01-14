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

struct belle_sip_transaction{
	belle_sip_object_t base;
	belle_sip_provider_t *provider; /*the provider that created this transaction */
	char *branch_id;
	belle_sip_transaction_state_t state;
	void *appdata;
	belle_sip_request_t *request;
};

struct belle_sip_server_transaction{
	belle_sip_transaction_t base;
};

struct belle_sip_client_transaction{
	belle_sip_transaction_t base;
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
	
}

belle_sip_request_t *belle_sip_transaction_get_request(belle_sip_transaction_t *t){
	return t->request;
}

void belle_sip_server_transaction_send_response(belle_sip_server_transaction_t *t){
}

belle_sip_request_t * belle_sip_client_transaction_create_cancel(belle_sip_client_transaction_t *t){
	return NULL;
}

void belle_sip_client_transaction_send_request(belle_sip_client_transaction_t *t){
}

static void belle_sip_transaction_init(belle_sip_transaction_t *t, belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_object_init_type(t,belle_sip_transaction_t);
	if (req) belle_sip_object_ref(req);
	t->request=req;
	t->provider=prov;
}

static void transaction_destroy(belle_sip_transaction_t *t){
	if (t->request) belle_sip_object_unref(t->request);
}

static void client_transaction_destroy(belle_sip_client_transaction_t *t ){
	transaction_destroy((belle_sip_transaction_t*)t);
}

static void server_transaction_destroy(belle_sip_server_transaction_t *t){
	transaction_destroy((belle_sip_transaction_t*)t);
}

belle_sip_client_transaction_t * belle_sip_client_transaction_new(belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_client_transaction_t *t=belle_sip_object_new(belle_sip_client_transaction_t,(belle_sip_object_destroy_t)client_transaction_destroy);
	belle_sip_transaction_init((belle_sip_transaction_t*)t,prov,req);
	return t;
}

belle_sip_server_transaction_t * belle_sip_server_transaction_new(belle_sip_provider_t *prov,belle_sip_request_t *req){
	belle_sip_server_transaction_t *t=belle_sip_object_new(belle_sip_server_transaction_t,(belle_sip_object_destroy_t)server_transaction_destroy);
	belle_sip_transaction_init((belle_sip_transaction_t*)t,prov,req);
	return t;
}


