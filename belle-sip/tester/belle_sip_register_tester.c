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
#include "pthread.h"

const char *test_domain="test.linphone.org";
const char *auth_domain="sip.linphone.org";
static int is_register_ok;
static int number_of_challenge;
static int using_transaction;
belle_sip_stack_t * stack;
belle_sip_provider_t *prov;
static belle_sip_listener_t* l;

static void process_dialog_terminated(belle_sip_listener_t *obj, const belle_sip_dialog_terminated_event_t *event){
	belle_sip_message("process_dialog_terminated called");
}
static void process_io_error(belle_sip_listener_t *obj, const belle_sip_io_error_event_t *event){
	belle_sip_warning("process_io_error");
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
	/*CU_ASSERT(CU_FALSE);*/
}
static void process_request_event(belle_sip_listener_t *obj, const belle_sip_request_event_t *event){
	belle_sip_message("process_request_event");
}
belle_sip_request_t* authorized_request;
static void process_response_event(belle_sip_listener_t *obj, const belle_sip_response_event_t *event){
	int status;
	belle_sip_request_t* request;
	CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_response_event_get_response(event));
	belle_sip_message("process_response_event [%i] [%s]"
					,status=belle_sip_response_get_status_code(belle_sip_response_event_get_response(event))
					,belle_sip_response_get_reason_phrase(belle_sip_response_event_get_response(event)));
	if (status==401) {
		CU_ASSERT_NOT_EQUAL_FATAL(number_of_challenge,2);
		CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_response_event_get_client_transaction(event)); /*require transaction mode*/
		request=belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(belle_sip_response_event_get_client_transaction(event)));
		belle_sip_header_cseq_t* cseq=(belle_sip_header_cseq_t*)belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_CSEQ);
		belle_sip_header_cseq_set_seq_number(cseq,belle_sip_header_cseq_get_seq_number(cseq)+1);
		CU_ASSERT_TRUE_FATAL(belle_sip_provider_add_authorization(prov,request,belle_sip_response_event_get_response(event),NULL));
		belle_sip_client_transaction_t *t=belle_sip_provider_create_client_transaction(prov,request);
		belle_sip_client_transaction_send_request(t);
		number_of_challenge++;
		authorized_request=request;
		belle_sip_object_ref(authorized_request);
	}  else {
		CU_ASSERT_EQUAL(status,200);
		is_register_ok=1;
		using_transaction=belle_sip_response_event_get_client_transaction(event)!=NULL;
		belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
	}
}
static void process_timeout(belle_sip_listener_t *obj, const belle_sip_timeout_event_t *event){
	belle_sip_message("process_timeout");
}
static void process_transaction_terminated(belle_sip_listener_t *obj, const belle_sip_transaction_terminated_event_t *event){
	belle_sip_message("process_transaction_terminated");
}
static void process_auth_requested(belle_sip_listener_t *obj, belle_sip_auth_event_t *event){
	belle_sip_message("process_auth_requested requested for [%s@%s]"
			,belle_sip_auth_event_get_username(event)
			,belle_sip_auth_event_get_realm(event));
	belle_sip_auth_event_set_passwd(event,"secret");
}
/*this would normally go to a .h file*/
struct test_listener{
	belle_sip_object_t base;
	void *some_context;
};

typedef struct test_listener test_listener_t;

BELLE_SIP_DECLARE_TYPES_BEGIN(test,0x1000)
	BELLE_SIP_TYPE_ID(test_listener_t)
BELLE_SIP_DECLARE_TYPES_END

BELLE_SIP_DECLARE_VPTR(test_listener_t);

/*the following would go to .c file */

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(test_listener_t,belle_sip_listener_t)
	process_dialog_terminated,
	process_io_error,
	process_request_event,
	process_response_event,
	process_timeout,
	process_transaction_terminated,
	process_auth_requested
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(test_listener_t,belle_sip_listener_t);

BELLE_SIP_INSTANCIATE_VPTR(test_listener_t,belle_sip_object_t,NULL,NULL,NULL,FALSE);

static test_listener_t *listener;

int register_init(void) {
	stack=belle_sip_stack_new(NULL);
	belle_sip_listening_point_t *lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"UDP");
	prov=belle_sip_stack_create_provider(stack,lp);
	
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"TCP");
	belle_sip_provider_add_listening_point(prov,lp);
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7061,"TLS");
	if (lp) {
		belle_sip_provider_add_listening_point(prov,lp);
	}
	listener=belle_sip_object_new(test_listener_t);

	return 0;
}
int register_uninit(void) {
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
	belle_sip_object_unref(listener);
	return 0;
}

void unregister_user(belle_sip_stack_t * stack
					,belle_sip_provider_t *prov
					,belle_sip_request_t* initial_request
					,int use_transaction) {
	belle_sip_request_t *req;
	belle_sip_provider_add_sip_listener(prov,l);
	is_register_ok=0;
	using_transaction=0;
	req=(belle_sip_request_t*)belle_sip_object_clone((belle_sip_object_t*)initial_request);
	belle_sip_header_cseq_t* cseq=(belle_sip_header_cseq_t*)belle_sip_message_get_header((belle_sip_message_t*)req,BELLE_SIP_CSEQ);
	belle_sip_header_cseq_set_seq_number(cseq,belle_sip_header_cseq_get_seq_number(cseq)+2); /*+2 if initial reg was challenged*/
	belle_sip_header_expires_t* expires_header=(belle_sip_header_expires_t*)belle_sip_message_get_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_EXPIRES);
	belle_sip_header_expires_set_expires(expires_header,0);
	if (use_transaction){
		belle_sip_message_remove_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_AUTHORIZATION);
		belle_sip_message_remove_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_PROXY_AUTHORIZATION);
		belle_sip_provider_add_authorization(prov,req,NULL,NULL); /*just in case*/
		belle_sip_client_transaction_t *t=belle_sip_provider_create_client_transaction(prov,req);
		belle_sip_client_transaction_send_request(t);
	}else belle_sip_provider_send_request(prov,req);
	int i;
	for(i=0;!is_register_ok && i<2 ;i++)
		belle_sip_stack_sleep(stack,5000);
	CU_ASSERT_EQUAL(is_register_ok,1);
	CU_ASSERT_EQUAL(using_transaction,use_transaction);
	belle_sip_provider_remove_sip_listener(prov,l);
}
belle_sip_request_t* register_user_at_domain(belle_sip_stack_t * stack
					,belle_sip_provider_t *prov
					,const char *transport
					,int use_transaction
					,const char* username
					,const char* domain) {
	belle_sip_request_t *req,*copy;
	char identity[256];
	char uri[256];
	number_of_challenge=0;
	if (transport)
		snprintf(uri,sizeof(uri),"sip:%s;transport=%s",domain,transport);
	else snprintf(uri,sizeof(uri),"sip:%s",domain);

	if (transport && strcasecmp("tls",transport)==0 && belle_sip_provider_get_listening_point(prov,"tls")==NULL){
		belle_sip_error("No TLS support, test skipped.");
		return NULL;
	}

	snprintf(identity,sizeof(identity),"Tester <sip:%s@%s>",username,domain);
	req=belle_sip_request_create(
	                    belle_sip_uri_parse(uri),
	                    "REGISTER",
	                    belle_sip_provider_create_call_id(prov),
	                    belle_sip_header_cseq_create(20,"REGISTER"),
	                    belle_sip_header_from_create2(identity,BELLE_SIP_RANDOM_TAG),
	                    belle_sip_header_to_create2(identity,NULL),
	                    belle_sip_header_via_new(),
	                    70);
	is_register_ok=0;
	using_transaction=0;
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_expires_create(600)));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_contact_new()));
	copy=(belle_sip_request_t*)belle_sip_object_ref(belle_sip_object_clone((belle_sip_object_t*)req));
	belle_sip_provider_add_sip_listener(prov,l=BELLE_SIP_LISTENER(listener));
	if (use_transaction){
		belle_sip_client_transaction_t *t=belle_sip_provider_create_client_transaction(prov,req);
		belle_sip_client_transaction_send_request(t);
	}else belle_sip_provider_send_request(prov,req);
	int i;
	for(i=0;!is_register_ok && i<2 ;i++)
		belle_sip_stack_sleep(stack,5000);
	CU_ASSERT_EQUAL(is_register_ok,1);
	CU_ASSERT_EQUAL(using_transaction,use_transaction);

	belle_sip_provider_remove_sip_listener(prov,l);

	return copy;
}

belle_sip_request_t* register_user(belle_sip_stack_t * stack
					,belle_sip_provider_t *prov
					,const char *transport
					,int use_transaction
					,const char* username) {
	return register_user_at_domain(stack,prov,transport,use_transaction,username,test_domain);
}

static void register_test(const char *transport, int use_transaction) {
	belle_sip_request_t *req;
	req=register_user(stack, prov, transport,use_transaction,"tester");
	if (req) {
		unregister_user(stack,prov,req,use_transaction);
		belle_sip_object_unref(req);
	}
}

static void stateless_register_udp(void){
	register_test(NULL,0);
}

static void stateless_register_tls(void){
	register_test("tls",0);
}

static void stateless_register_tcp(void){
	register_test("tcp",0);
}

static void stateful_register_udp(void){
	register_test(NULL,1);
}

static void stateful_register_udp_delayed(void){
	belle_sip_stack_set_tx_delay(stack,3000);
	register_test(NULL,1);
	belle_sip_stack_set_tx_delay(stack,0);
}

static void stateful_register_tcp(void){
	register_test("tcp",1);
}

static void stateful_register_tls(void){
	register_test("tls",1);
}


static void bad_req_process_io_error(void *user_ctx, const belle_sip_io_error_event_t *event){
	belle_sip_message("bad_req_process_io_error not implemented yet");
}
static void bad_req_process_response_event(void *user_ctx, const belle_sip_response_event_t *event){
	belle_sip_message("bad_req_process_response_event not implemented yet");
}

static void test_bad_request() {
	belle_sip_request_t *req;
	belle_sip_header_address_t* route_address=belle_sip_header_address_create(NULL,belle_sip_uri_create(NULL,test_domain));
	belle_sip_header_route_t* route;
	belle_sip_header_to_t* to = belle_sip_header_to_create2("sip:toto@titi.com",NULL);
	belle_sip_listener_callbacks_t cbs;
	memset(&cbs,0,sizeof(cbs));

	belle_sip_listener_t* bad_req_listener = belle_sip_listener_create_from_callbacks(&cbs,NULL);
	cbs.process_io_error=bad_req_process_io_error;
	cbs.process_response_event=bad_req_process_response_event;

	req=belle_sip_request_create(
	                    BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(belle_sip_header_address_get_uri(route_address)))),
	                    "REGISTER",
	                    belle_sip_provider_create_call_id(prov),
	                    belle_sip_header_cseq_create(20,"REGISTER"),
	                    belle_sip_header_from_create2("sip:toto@titi.com",BELLE_SIP_RANDOM_TAG),
	                    to,
	                    belle_sip_header_via_new(),
	                    70);

	belle_sip_uri_set_transport_param(belle_sip_header_address_get_uri(route_address),"tcp");
	route = belle_sip_header_route_create(route_address);
	belle_sip_header_set_name(BELLE_SIP_HEADER(to),"BrokenHeader");

	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_expires_create(600)));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(route));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_contact_new()));
	belle_sip_provider_add_sip_listener(prov,bad_req_listener);
	belle_sip_client_transaction_t *t=belle_sip_provider_create_client_transaction(prov,req);
	belle_sip_client_transaction_send_request(t);
	belle_sip_stack_sleep(stack,100);
	belle_sip_provider_remove_sip_listener(prov,bad_req_listener);
	belle_sip_object_unref(bad_req_listener);
}
static void test_register_authenticate() {
	belle_sip_request_t *reg;
	number_of_challenge=0;
	authorized_request=NULL;
	reg=register_user_at_domain(stack, prov, "udp",1,"bellesip",auth_domain);
	if (authorized_request) {
		unregister_user(stack,prov,authorized_request,1);
		belle_sip_object_unref(authorized_request);
	}
	belle_sip_object_unref(reg);
}

int belle_sip_register_test_suite(){
	CU_pSuite pSuite = CU_add_suite("Register", register_init, register_uninit);

	if (NULL == CU_add_test(pSuite, "stateful-udp-register", stateful_register_udp)) {
		return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "stateful-udp-register-with-network-delay", stateful_register_udp_delayed)) {
		return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "stateful-tcp-register", stateful_register_tcp)) {
		return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "stateful-tls-register", stateful_register_tls)) {
		return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "stateless udp register", stateless_register_udp)) {
		return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "stateless tcp register", stateless_register_tcp)) {
		return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "stateless tls register", stateless_register_tls)) {
			return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "Bad request tcp", test_bad_request)) {
			return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "authenticate", test_register_authenticate)) {
			return CU_get_error();
	}
	return 0;
}

