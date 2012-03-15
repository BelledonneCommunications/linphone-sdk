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
static int is_register_ok;
static belle_sip_stack_t * stack;
#define TEST_DOMAIN "localhost"
/*#define TEST_DOMAIN "test.linphone.org"*/

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
	belle_sip_object_unref(belle_sip_response_event_get_response(event));
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
static int init(void) {
	stack=belle_sip_stack_new(NULL);
	return 0;
}
static int uninit(void) {
	belle_sip_object_unref(stack);
	return 0;
}
static void register_test(belle_sip_listening_point_t* lp,belle_sip_uri_t* uri) {


	belle_sip_provider_t *prov;
	belle_sip_request_t *req;
	test_listener_t *listener=belle_sip_object_new(test_listener_t);

	belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);

	prov=belle_sip_stack_create_provider(stack,lp);
	belle_sip_provider_add_sip_listener(prov,BELLE_SIP_LISTENER(listener));
	req=belle_sip_request_create(
	                    uri,
	                    "REGISTER",
	                    belle_sip_provider_create_call_id(prov),
	                    belle_sip_header_cseq_create(20,"REGISTER"),
	                    belle_sip_header_from_create("Tester <sip:tester@" TEST_DOMAIN ">","a0dke45"),
	                    belle_sip_header_to_create("Tester <sip:tester@" TEST_DOMAIN ">",NULL),
	                    belle_sip_header_via_new(),
	                    70);

	is_register_ok=0;
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_expires_create(600)));
	belle_sip_provider_send_request(prov,req);
	belle_sip_stack_sleep(stack,25000);
	CU_ASSERT_EQUAL(is_register_ok,1);
	belle_sip_object_unref(prov);
	belle_sip_object_unref(listener);

	return;
}
static void register_udp_test() {
	belle_sip_listening_point_t *lp;
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"UDP");
	CU_ASSERT_PTR_NOT_NULL_FATAL(lp);
	register_test(lp,belle_sip_uri_parse("sip:"TEST_DOMAIN));
}
static void register_tcp_test() {
	belle_sip_listening_point_t *lp;
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"TCP");
	CU_ASSERT_PTR_NOT_NULL_FATAL(lp);
	register_test(lp,belle_sip_uri_parse("sip:"TEST_DOMAIN";transport=tcp"));
}

int belle_sip_register_test_suite(){
	CU_pSuite pSuite = CU_add_suite("Register test suite", init, uninit);

	if (NULL == CU_add_test(pSuite, "simple udp register", register_udp_test)) {
		return CU_get_error();
	}
	if (NULL == CU_add_test(pSuite, "simple tcp register", register_tcp_test)) {
		return CU_get_error();
	}
	return 0;
}

