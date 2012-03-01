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
#include "belle-sip/belle-sip.h"

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


int main(int argc, char *argv[]){
	belle_sip_stack_t * stack=belle_sip_stack_new(NULL);
	belle_sip_listening_point_t *lp;
	belle_sip_provider_t *prov;
	belle_sip_request_t *req;
	test_listener_t *listener=belle_sip_object_new(test_listener_t);

	belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
	
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"UDP");
	prov=belle_sip_stack_create_provider(stack,lp);
	belle_sip_provider_add_sip_listener(prov,BELLE_SIP_LISTENER(listener));
	req=belle_sip_request_create(
	                    belle_sip_uri_parse("sip:test.linphone.org"),
	                    "REGISTER",
	                    belle_sip_provider_create_call_id(prov),
	                    belle_sip_header_cseq_create(20,"REGISTER"),
	                    belle_sip_header_from_create("Tester <sip:tester@test.linphone.org>","a0dke45"),
	                    belle_sip_header_to_create("Tester <sip:tester@test.linphone.org>",NULL),
	                    belle_sip_header_via_new(),
	                    70);
	char *tmp=belle_sip_object_to_string(BELLE_SIP_OBJECT(req));

	
	printf("Message to send:\n%s\n",tmp);
	belle_sip_free(tmp);
	belle_sip_provider_send_request(prov,req);
	belle_sip_stack_sleep(stack,25000);
	printf("Exiting\n");
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
	return 0;
}
