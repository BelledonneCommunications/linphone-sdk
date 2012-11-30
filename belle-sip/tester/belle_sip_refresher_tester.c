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

#include <stdio.h>
#include "CUnit/Basic.h"
#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"

typedef enum auth_mode {
	none
	,digest
	,digest_auth
}auth_mode_t;

typedef struct _stat {
	int twoHundredOk;
	int refreshOk;
}stat_t;
typedef struct endpoint {
	belle_sip_stack_t* stack;
	belle_sip_listener_callbacks_t* listener_callbacks;
	belle_sip_provider_t* provider;
	belle_sip_listening_point_t *lp;
	auth_mode_t auth;
	stat_t stat;
	unsigned char expire_in_contact;
} endpoint_t;

static unsigned int  wait_for(belle_sip_stack_t*s1, belle_sip_stack_t*s2,int* counter,int value,int timeout) {
	int retry=0;
#define ITER 100
	while (*counter<value && retry++ <(timeout/ITER)) {
		if (s1) belle_sip_stack_sleep(s1,ITER/2);
		if (s2) belle_sip_stack_sleep(s2,ITER/2);
	}
	if(*counter<value) return FALSE;
	else return TRUE;
}

//static void process_dialog_terminated(void *obj, const belle_sip_dialog_terminated_event_t *event){
//	belle_sip_message("process_dialog_terminated called");
//}
//static void process_io_error(void *obj, const belle_sip_io_error_event_t *event){
//	belle_sip_warning("process_io_error");
//	/*belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));*/
//	/*CU_ASSERT(CU_FALSE);*/
//}
static void server_process_request_event(void *obj, const belle_sip_request_event_t *event){
	endpoint_t* endpoint = (endpoint_t*)obj;
	belle_sip_server_transaction_t* server_transaction =belle_sip_provider_create_server_transaction(endpoint->provider,belle_sip_request_event_get_request(event));
	belle_sip_request_t* req = belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(server_transaction));
	belle_sip_response_t* resp;
	belle_sip_message("caller_process_request_event received [%s] message",belle_sip_request_get_method(belle_sip_request_event_get_request(event)));

	switch (endpoint->auth) {
	case none: {
		resp=belle_sip_response_create_from_request(belle_sip_request_event_get_request(event),200);
		break;
	}
	default:
		break;
	}
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(resp),BELLE_SIP_HEADER(belle_sip_message_get_header_by_type(req,belle_sip_header_expires_t)));
	belle_sip_server_transaction_send_response(server_transaction,resp);
}
static void client_process_response_event(void *obj, const belle_sip_response_event_t *event){
	//belle_sip_client_transaction_t* client_transaction = belle_sip_response_event_get_client_transaction(event);
	endpoint_t* endpoint = (endpoint_t*)obj;
	int status = belle_sip_response_get_status_code(belle_sip_response_event_get_response(event));
	belle_sip_message("caller_process_response_event [%i]",status);
	switch (status) {
	case 200:endpoint->stat.twoHundredOk++; break;
	default: break;
	}


}
//static void process_timeout(void *obj, const belle_sip_timeout_event_t *event){
//	belle_sip_message("process_timeout");
//}
//static void process_transaction_terminated(void *obj, const belle_sip_transaction_terminated_event_t *event){
//	belle_sip_message("process_transaction_terminated");
//}
//static void process_auth_requested(void *obj, belle_sip_auth_event_t *event){
//	belle_sip_message("process_auth_requested requested for [%s@%s]"
//			,belle_sip_auth_event_get_username(event)
//			,belle_sip_auth_event_get_realm(event));
//	belle_sip_auth_event_set_passwd(event,"secret");
//}

static void belle_sip_refresher_listener ( const belle_sip_refresher_t* refresher
		,void* user_pointer
		,unsigned int status_code
		,const char* reason_phrase) {
	endpoint_t* endpoint = (endpoint_t*)user_pointer;
	belle_sip_message("belle_sip_refresher_listener [%i] reason [%s]",status_code,reason_phrase);
	switch (status_code) {
		case 200:endpoint->stat.refreshOk++; break;
		default: break;
	}
}

static endpoint_t* create_endpoint(int port,const char* transport,belle_sip_listener_callbacks_t* listener_callbacks) {
	endpoint_t* endpoint = belle_sip_new0(endpoint_t);
	endpoint->stack=belle_sip_stack_new(NULL);
	endpoint->listener_callbacks=listener_callbacks;
	endpoint->lp=belle_sip_stack_create_listening_point(endpoint->stack,"0.0.0.0",port,transport);
	endpoint->provider=belle_sip_stack_create_provider(endpoint->stack,endpoint->lp);
	belle_sip_provider_add_sip_listener(endpoint->provider,belle_sip_listener_create_from_callbacks(endpoint->listener_callbacks,endpoint));
	return endpoint;
}
static endpoint_t* create_udp_endpoint(int port,belle_sip_listener_callbacks_t* listener_callbacks) {
	return create_endpoint(port,"udp",listener_callbacks);
}
static void register_test() {
	belle_sip_listener_callbacks_t client_callbacks;
	belle_sip_listener_callbacks_t server_callbacks;
	belle_sip_request_t* req;
	belle_sip_client_transaction_t* trans;
	belle_sip_header_route_t* destination_route;
	const char* identity="sip:toto@sip.linphone.org";
	const char* domain="sip:sip.linphone.org";
	client_callbacks.process_response_event=client_process_response_event;

	server_callbacks.process_request_event=server_process_request_event;

	endpoint_t* client = create_udp_endpoint(3452,&client_callbacks);
	endpoint_t* server = create_udp_endpoint(6788,&server_callbacks);
	destination_route=belle_sip_header_route_create(belle_sip_header_address_create(NULL,(belle_sip_uri_t*)belle_sip_listening_point_get_uri(server->lp)));


	req=belle_sip_request_create(
		                    belle_sip_uri_parse(domain),
		                    "REGISTER",
		                    belle_sip_provider_create_call_id(client->provider),
		                    belle_sip_header_cseq_create(20,"REGISTER"),
		                    belle_sip_header_from_create2(identity,BELLE_SIP_RANDOM_TAG),
		                    belle_sip_header_to_create2(identity,NULL),
		                    belle_sip_header_via_new(),
		                    70);
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_expires_create(1)));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_contact_new()));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(destination_route));
	trans=belle_sip_provider_create_client_transaction(client->provider,req);
	belle_sip_client_transaction_send_request(trans);


	CU_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.twoHundredOk,1,1000));

	belle_sip_refresher_t* refresher = belle_sip_client_transaction_create_refresher(trans);

	belle_sip_refresher_set_listener(refresher,belle_sip_refresher_listener,client);

	/*fixme mesure time*/
	CU_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.refreshOk,3,3500));


}



int belle_sip_refresher_test_suite(){
	CU_pSuite pSuite = CU_add_suite("Refresher", NULL, NULL);

	if (NULL == CU_add_test(pSuite, "simple_register", register_test)) {
		return CU_get_error();
	}

	return 0;
}

