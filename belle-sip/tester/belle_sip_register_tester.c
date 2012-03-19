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

const char *test_domain="localhost";
static int is_register_ok;
static int using_transaction;
static belle_sip_stack_t * stack;
static belle_sip_provider_t *prov;

static void process_dialog_terminated(belle_sip_listener_t *obj, const belle_sip_dialog_terminated_event_t *event){
	belle_sip_message("process_dialog_terminated called");
}
static void process_io_error(belle_sip_listener_t *obj, const belle_sip_io_error_event_t *event){
	belle_sip_message("process_io_error");
}
static void process_request_event(belle_sip_listener_t *obj, const belle_sip_request_event_t *event){
	belle_sip_message("process_request_event");
}
static void process_response_event(belle_sip_listener_t *obj, const belle_sip_response_event_t *event){
	belle_sip_message("process_response_event");
	CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_response_event_get_response(event));
	CU_ASSERT_EQUAL(belle_sip_response_get_status_code(belle_sip_response_event_get_response(event)),200);
	is_register_ok=1;
	using_transaction=belle_sip_response_event_get_client_transaction(event)!=NULL;
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
}
static void process_timeout(belle_sip_listener_t *obj, const belle_sip_timeout_event_t *event){
	belle_sip_message("process_timeout");
}
static void process_transaction_terminated(belle_sip_listener_t *obj, const belle_sip_transaction_terminated_event_t *event){
	belle_sip_message("process_transaction_terminated");
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
	process_transaction_terminated
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(test_listener_t,belle_sip_listener_t);

BELLE_SIP_INSTANCIATE_VPTR(test_listener_t,belle_sip_object_t,NULL,NULL,NULL,FALSE);

static test_listener_t *listener;

static int init(void) {
	stack=belle_sip_stack_new(NULL);
	belle_sip_listening_point_t *lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"UDP");
	prov=belle_sip_stack_create_provider(stack,lp);
	belle_sip_object_unref(lp);
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"TCP");
	belle_sip_provider_add_listening_point(prov,lp);
	belle_sip_object_unref(lp);
	listener=belle_sip_object_new(test_listener_t);
	belle_sip_provider_add_sip_listener(prov,BELLE_SIP_LISTENER(listener));
	return 0;
}
static int uninit(void) {
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
	belle_sip_object_unref(listener);
	return 0;
}
static void register_test(const char *transport, int use_transaction) {
	belle_sip_request_t *req;
	char token[10];
	char identity[256];
	char uri[256];

	if (transport)
		snprintf(uri,sizeof(uri),"sip:%s;transport=%s",test_domain,transport);
	else snprintf(uri,sizeof(uri),"sip:%s",test_domain);

	snprintf(identity,sizeof(identity),"Tester <sip:tester@%s>",test_domain);
	req=belle_sip_request_create(
	                    belle_sip_uri_parse(uri),
	                    "REGISTER",
	                    belle_sip_provider_create_call_id(prov),
	                    belle_sip_header_cseq_create(20,"REGISTER"),
	                    belle_sip_header_from_create(identity,belle_sip_random_token(token,sizeof(token))),
	                    belle_sip_header_to_create(identity,NULL),
	                    belle_sip_header_via_new(),
	                    70);

	is_register_ok=0;
	using_transaction=0;
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_expires_create(600)));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_contact_new()));
	if (use_transaction){
		belle_sip_client_transaction_t *t=belle_sip_provider_create_client_transaction(prov,req);
		belle_sip_client_transaction_send_request(t);
	}else belle_sip_provider_send_request(prov,req);
	belle_sip_stack_sleep(stack,33000);
	CU_ASSERT_EQUAL(is_register_ok,1);
	CU_ASSERT_EQUAL(using_transaction,use_transaction);
	return;
}


static void stateless_register_udp(void){
	register_test(NULL,0);
}

static void stateless_register_tcp(void){
	register_test("tcp",0);
}

static void stateful_register_udp(void){
	register_test(NULL,1);
}

static void stateful_register_tcp(void){
	register_test("tcp",1);
}

int belle_sip_register_test_suite(){
	CU_pSuite pSuite = CU_add_suite("Register test suite", init, uninit);

	if (NULL == CU_add_test(pSuite, "stateful udp register", stateful_register_udp)) {
		return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "stateful tcp register", stateful_register_tcp)) {
		return CU_get_error();
	}

	if (NULL == CU_add_test(pSuite, "stateless udp register", stateless_register_udp)) {
		return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "stateless tcp register", stateless_register_tcp)) {
		return CU_get_error();
	}
	return 0;
}

