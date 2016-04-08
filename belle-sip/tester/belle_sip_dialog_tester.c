/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belle-sip/belle-sip.h"
#include "belle_sip_tester.h"

#include "register_tester.h"


static const char* sdp = 		"v=0\r\n"\
								"o=jehan-mac 1239 1239 IN IP4 192.168.0.18\r\n"\
								"s=Talk\r\n"\
								"c=IN IP4 192.168.0.18\r\n"\
								"t=0 0\r\n"\
								"m=audio 7078 RTP/AVP 111 110 3 0 8 101\r\n"\
								"a=rtpmap:111 speex/16000\r\n"\
								"a=fmtp:111 vbr=on\r\n"\
								"a=rtpmap:110 speex/8000\r\n"\
								"a=fmtp:110 vbr=on\r\n"\
								"a=rtpmap:101 telephone-event/8000\r\n"\
								"a=fmtp:101 0-11\r\n"\
								"m=video 8078 RTP/AVP 99 97 98\r\n"\
								"c=IN IP4 192.168.0.18\r\n"\
								"b=AS:380\r\n"\
								"a=rtpmap:99 MP4V-ES/90000\r\n"\
								"a=fmtp:99 profile-level-id=3\r\n"\
								"a=rtpmap:97 theora/90000\r\n"\
								"a=rtpmap:98 H263-1998/90000\r\n"\
								"a=fmtp:98 CIF=1;QCIF=1\r\n";

static belle_sip_dialog_t* caller_dialog;
static belle_sip_dialog_t* callee_dialog;
static belle_sip_response_t* ok_response;
static belle_sip_server_transaction_t* inserv_transaction;


belle_sip_request_t* build_request(belle_sip_stack_t * stack
									, belle_sip_provider_t *prov
									,belle_sip_header_address_t* from
									,belle_sip_header_address_t* to
									,belle_sip_header_address_t* route
									,const char* method) {
	belle_sip_header_from_t* from_header;
	belle_sip_header_to_t* to_header;
	belle_sip_request_t *req;
	belle_sip_uri_t* req_uri;
	belle_sip_header_contact_t* contact_header;
	BELLESIP_UNUSED(stack);

	from_header = belle_sip_header_from_create(from,BELLE_SIP_RANDOM_TAG);
	to_header = belle_sip_header_to_create(to,NULL);
	req_uri = (belle_sip_uri_t*)belle_sip_object_clone((belle_sip_object_t*)belle_sip_header_address_get_uri((belle_sip_header_address_t*)to_header));

	contact_header= belle_sip_header_contact_new();
	belle_sip_header_address_set_uri((belle_sip_header_address_t*)contact_header,belle_sip_uri_new());
	belle_sip_uri_set_user(belle_sip_header_address_get_uri((belle_sip_header_address_t*)contact_header),belle_sip_uri_get_user(req_uri));
	req=belle_sip_request_create(
							req_uri,
							method,
		                    belle_sip_provider_create_call_id(prov),
		                    belle_sip_header_cseq_create(20,method),
		                    from_header,
		                    to_header,
		                    belle_sip_header_via_new(),
		                    70);
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(contact_header));
	if (route) {
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_route_create(route)));
	}
	return req;
}

static void process_dialog_terminated(void *user_ctx, const belle_sip_dialog_terminated_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	belle_sip_message("process_dialog_terminated not implemented yet");
}

static void process_io_error(void *user_ctx, const belle_sip_io_error_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	belle_sip_message("process_io_error not implemented yet");
}

static void caller_process_request_event(void *user_ctx, const belle_sip_request_event_t *event) {
	belle_sip_server_transaction_t* server_transaction;
	belle_sip_response_t* resp;
	belle_sip_dialog_t* dialog;
	belle_sip_header_to_t* to=belle_sip_message_get_header_by_type(belle_sip_request_event_get_request(event),belle_sip_header_to_t);
	if (!belle_sip_uri_equals(BELLE_SIP_URI(user_ctx),belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(to)))) {
		belle_sip_message("Message [%p] not for caller, skipping",belle_sip_request_event_get_request(event));
		return; /*not for the caller*/
	}
	belle_sip_message("caller_process_request_event received [%s] message",belle_sip_request_get_method(belle_sip_request_event_get_request(event)));
	server_transaction=belle_sip_provider_create_server_transaction(prov,belle_sip_request_event_get_request(event));
	BC_ASSERT_STRING_EQUAL("BYE",belle_sip_request_get_method(belle_sip_request_event_get_request(event)));
	dialog =  belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(server_transaction));
	if (BC_ASSERT_PTR_NOT_NULL(dialog)) {
		BC_ASSERT_EQUAL(belle_sip_dialog_get_state(dialog) , BELLE_SIP_DIALOG_CONFIRMED, int, "%d");
	}
	resp=belle_sip_response_create_from_request(belle_sip_request_event_get_request(event),200);
	belle_sip_server_transaction_send_response(server_transaction,resp);

}

static void callee_process_request_event(void *user_ctx, const belle_sip_request_event_t *event) {
	belle_sip_dialog_t* dialog;
	belle_sip_response_t* ringing_response;
	belle_sip_header_content_type_t* content_type ;
	belle_sip_header_content_length_t* content_length;
	belle_sip_server_transaction_t* server_transaction = belle_sip_request_event_get_server_transaction(event);
	belle_sip_header_to_t* to=belle_sip_message_get_header_by_type(belle_sip_request_event_get_request(event),belle_sip_header_to_t);
	const char* method;
	if (!belle_sip_uri_equals(BELLE_SIP_URI(user_ctx),belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(to)))) {
		belle_sip_message("Message [%p] not for callee, skipping",belle_sip_request_event_get_request(event));
		return; /*not for the callee*/
	}
	method = belle_sip_request_get_method(belle_sip_request_event_get_request(event));
	if (!server_transaction && strcmp(method,"ACK")!=0) {
		server_transaction= belle_sip_provider_create_server_transaction(prov,belle_sip_request_event_get_request(event));
	}

	belle_sip_message("callee_process_request_event received [%s] message",method);
	dialog = belle_sip_request_event_get_dialog(event);
	if (!dialog ) {
		BC_ASSERT_STRING_EQUAL("INVITE",method);
		dialog=belle_sip_provider_create_dialog(prov,BELLE_SIP_TRANSACTION(server_transaction));
		callee_dialog=dialog;
		inserv_transaction=server_transaction;
	}
	if (belle_sip_dialog_get_state(dialog) == BELLE_SIP_DIALOG_NULL) {
		ringing_response = belle_sip_response_create_from_request(belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(server_transaction)),180);
		/*prepare 200ok*/
		ok_response = belle_sip_response_create_from_request(belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(server_transaction)),200);
		content_length= belle_sip_header_content_length_create(strlen(sdp));
		content_type = belle_sip_header_content_type_create("application","sdp");
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(ok_response),BELLE_SIP_HEADER(content_type));
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(ok_response),BELLE_SIP_HEADER(content_length));
		belle_sip_message_set_body(BELLE_SIP_MESSAGE(ok_response),sdp,strlen(sdp));
		belle_sip_object_ref(ok_response);
		/*only send ringing*/
		belle_sip_server_transaction_send_response(server_transaction,ringing_response);
	} else if (belle_sip_dialog_get_state(dialog) == BELLE_SIP_DIALOG_CONFIRMED) {
		/*time to send bye*/
		belle_sip_client_transaction_t* client_transaction = belle_sip_provider_create_client_transaction(prov,belle_sip_dialog_create_request(dialog,"BYE"));
		belle_sip_client_transaction_send_request(client_transaction);
	} else {
		belle_sip_warning("Unexpected state [%s] for dialog [%p]",belle_sip_dialog_state_to_string(belle_sip_dialog_get_state(dialog)),dialog );
	}

}

static void caller_process_response_event(void *user_ctx, const belle_sip_response_event_t *event){
	belle_sip_client_transaction_t* client_transaction = belle_sip_response_event_get_client_transaction(event);
	belle_sip_header_from_t* from=belle_sip_message_get_header_by_type(belle_sip_response_event_get_response(event),belle_sip_header_from_t);
	belle_sip_header_cseq_t* invite_cseq=belle_sip_message_get_header_by_type(belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(client_transaction)),belle_sip_header_cseq_t);
	belle_sip_request_t* ack;
	belle_sip_dialog_t* dialog;
	int status;
	if (!belle_sip_uri_equals(BELLE_SIP_URI(user_ctx),belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(from)))) {
		belle_sip_message("Message [%p] not for caller, skipping",belle_sip_response_event_get_response(event));
		return; /*not for the caller*/
	}

	status = belle_sip_response_get_status_code(belle_sip_response_event_get_response(event));
	belle_sip_message("caller_process_response_event [%i]",status);
	if (BC_ASSERT_PTR_NOT_NULL(client_transaction)) {
		dialog = belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(client_transaction));
		if (BC_ASSERT_PTR_NOT_NULL(dialog)) {
			BC_ASSERT_PTR_EQUAL(caller_dialog,dialog);
			if (belle_sip_dialog_get_state(dialog) == BELLE_SIP_DIALOG_NULL) {
				BC_ASSERT_EQUAL(status,100, int, "%d");
			} else if (belle_sip_dialog_get_state(dialog) == BELLE_SIP_DIALOG_EARLY){
				BC_ASSERT_EQUAL(status,180, int, "%d");
				/*send 200ok from callee*/
				belle_sip_server_transaction_send_response(inserv_transaction,ok_response);
				belle_sip_object_unref(ok_response);
				ok_response=NULL;
			} else if (belle_sip_dialog_get_state(dialog) == BELLE_SIP_DIALOG_CONFIRMED) {
				ack=belle_sip_dialog_create_ack(dialog,belle_sip_header_cseq_get_seq_number(invite_cseq));
				belle_sip_dialog_send_ack(dialog,ack);
			}
		}
	}
}

static void callee_process_response_event(void *user_ctx, const belle_sip_response_event_t *event){
	belle_sip_dialog_t* dialog;
	belle_sip_client_transaction_t* client_transaction = belle_sip_response_event_get_client_transaction(event);
	belle_sip_header_from_t* from=belle_sip_message_get_header_by_type(belle_sip_response_event_get_response(event),belle_sip_header_from_t);
	int status = belle_sip_response_get_status_code(belle_sip_response_event_get_response(event));
	if (!belle_sip_uri_equals(BELLE_SIP_URI(user_ctx),belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(from)))) {
		belle_sip_message("Message [%p] not for callee, skipping",belle_sip_response_event_get_response(event));
		return; /*not for the callee*/
	}
	if (BC_ASSERT_PTR_NOT_NULL(client_transaction)) {
		dialog = belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(client_transaction));
		if (BC_ASSERT_PTR_NOT_NULL(dialog)) {
			BC_ASSERT_PTR_EQUAL(callee_dialog,dialog);
			if (belle_sip_dialog_get_state(dialog) == BELLE_SIP_DIALOG_TERMINATED) {
				call_endeed=1;
				belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
			}
			belle_sip_message("callee_process_response_event [%i] on dialog [%p] for state [%s]",status
						,dialog
						,belle_sip_dialog_state_to_string(belle_sip_dialog_get_state(dialog)));
		}
	}

}

static void process_timeout(void *user_ctx, const belle_sip_timeout_event_t *event) {
	BELLESIP_UNUSED(user_ctx);
/*	belle_sip_client_transaction_t* client_transaction = belle_sip_timeout_event_get_client_transaction(event);
	SalOp* op = (SalOp*)belle_sip_transaction_get_application_data(BELLE_SIP_TRANSACTION(client_transaction));
	if (op->callbacks.process_timeout) {
		op->callbacks.process_timeout(op,event);
	} else*/ {
		belle_sip_message("Unhandled event timeout [%p]",event);
	}
}

static void process_transaction_terminated(void *user_ctx, const belle_sip_transaction_terminated_event_t *event) {
	BELLESIP_UNUSED(user_ctx);
/*	belle_sip_client_transaction_t* client_transaction = belle_sip_transaction_terminated_event_get_client_transaction(event);
	SalOp* op = (SalOp*)belle_sip_transaction_get_application_data(client_transaction);
	if (op->calbacks.process_transaction_terminated) {
		op->calbacks.process_transaction_terminated(op,event);
	} else */{
		belle_sip_message("Unhandled transaction terminated [%p]",event);
	}
}

static void listener_destroyed(void *user_ctx){
	belle_sip_object_unref(user_ctx);
}

static void do_simple_call(void) {
#define CALLER "marie"
#define CALLEE "pauline"
	belle_sip_request_t *pauline_register_req;
	belle_sip_request_t *marie_register_req;
	belle_sip_listener_callbacks_t caller_listener_callbacks;
	belle_sip_listener_t* caller_listener;
	belle_sip_listener_callbacks_t callee_listener_callbacks;
	belle_sip_listener_t* callee_listener;
	belle_sip_request_t* req;
	belle_sip_header_address_t* from;
	belle_sip_header_address_t* to;
	belle_sip_header_address_t* route;
	belle_sip_header_allow_t* header_allow;
	belle_sip_header_content_type_t* content_type ;
	belle_sip_header_content_length_t* content_length;
	belle_sip_client_transaction_t* client_transaction;
	char  cookie[4];

	caller_listener_callbacks.process_dialog_terminated=process_dialog_terminated;
	caller_listener_callbacks.process_io_error=process_io_error;
	caller_listener_callbacks.process_request_event=caller_process_request_event;
	caller_listener_callbacks.process_response_event=caller_process_response_event;
	caller_listener_callbacks.process_timeout=process_timeout;
	caller_listener_callbacks.process_transaction_terminated=process_transaction_terminated;
	caller_listener_callbacks.listener_destroyed=listener_destroyed;

	callee_listener_callbacks.process_dialog_terminated=process_dialog_terminated;
	callee_listener_callbacks.process_io_error=process_io_error;
	callee_listener_callbacks.process_request_event=callee_process_request_event;
	callee_listener_callbacks.process_response_event=callee_process_response_event;
	callee_listener_callbacks.process_timeout=process_timeout;
	callee_listener_callbacks.process_transaction_terminated=process_transaction_terminated;
	callee_listener_callbacks.listener_destroyed=listener_destroyed;

	pauline_register_req=register_user(stack, prov, "TCP" ,1 ,CALLER,NULL);
	if (belle_sip_provider_get_listening_point(prov, "tls")) {
		marie_register_req=register_user(stack, prov, "TLS" ,1 ,CALLEE,NULL);
	} else {
		marie_register_req=register_user(stack, prov, "TCP" ,1 ,CALLEE,NULL);
	}

	from=belle_sip_header_address_create(NULL,belle_sip_uri_create(CALLER,test_domain));
	/*to make sure unexpected messages are ignored*/
	belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(from),"cookie",belle_sip_random_token(cookie,sizeof(cookie)));
	/*to make sure unexpected messages are ignored*/
	to=belle_sip_header_address_create(NULL,belle_sip_uri_create(CALLEE,test_domain));
	belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(to),"cookie",belle_sip_random_token(cookie,sizeof(cookie)));

	belle_sip_provider_add_sip_listener(prov,caller_listener=belle_sip_listener_create_from_callbacks(&caller_listener_callbacks,belle_sip_object_ref(belle_sip_header_address_get_uri((belle_sip_header_address_t*)from))));
	belle_sip_provider_add_sip_listener(prov,callee_listener=belle_sip_listener_create_from_callbacks(&callee_listener_callbacks,belle_sip_object_ref(belle_sip_header_address_get_uri((belle_sip_header_address_t*)to))));


	route = belle_sip_header_address_create(NULL,belle_sip_uri_create(NULL,test_domain));
	belle_sip_uri_set_transport_param(belle_sip_header_address_get_uri(route),"tcp");

	req=build_request(stack,prov,from,to,route,"INVITE");
	header_allow = belle_sip_header_allow_create("INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO");
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(header_allow));

	content_length= belle_sip_header_content_length_create(strlen(sdp));
	content_type = belle_sip_header_content_type_create("application","sdp");
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(content_type));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(content_length));
	belle_sip_message_set_body(BELLE_SIP_MESSAGE(req),sdp,strlen(sdp));


	client_transaction = belle_sip_provider_create_client_transaction(prov,req);
	caller_dialog=belle_sip_provider_create_dialog(prov,BELLE_SIP_TRANSACTION(client_transaction));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(client_transaction)));
	//belle_sip_transaction_set_application_data(BELLE_SIP_TRANSACTION(client_transaction),op);
	call_endeed=0;
	belle_sip_client_transaction_send_request(client_transaction);
	//int i=0;
	//for(i=0;i<10 &&!call_endeed;i++)
	belle_sip_stack_sleep(stack,30000);

	BC_ASSERT_EQUAL(call_endeed,1, int, "%d");

	belle_sip_provider_remove_sip_listener(prov,caller_listener);
	belle_sip_provider_remove_sip_listener(prov,callee_listener);
	belle_sip_object_unref(caller_listener);
	belle_sip_object_unref(callee_listener);

	unregister_user(stack, prov, pauline_register_req ,1);
	belle_sip_object_unref(pauline_register_req);
	unregister_user(stack, prov, marie_register_req ,1);
	belle_sip_object_unref(marie_register_req);
}

static void simple_call(void){
	belle_sip_stack_set_tx_delay(stack,0);
	do_simple_call();
}

static void simple_call_with_delay(void){
	belle_sip_stack_set_tx_delay(stack,2000);
	do_simple_call();
	belle_sip_stack_set_tx_delay(stack,0);
}
/*static void simple_call_udp_tcp_with_delay(void){
	belle_sip_listening_point_t* lp=belle_sip_provider_get_listening_point(prov,"tls");
	belle_sip_object_ref(lp);
	belle_sip_stack_set_tx_delay(stack,2000);
	belle_sip_provider_remove_listening_point(prov,lp);
	do_simple_call();
	belle_sip_stack_set_tx_delay(stack,0);
	belle_sip_provider_add_listening_point(prov,lp);
	belle_sip_object_unref(lp);
}*/


test_t dialog_tests[] = {
	TEST_ONE_TAG("Simple call", simple_call, "LeaksMemory"),
	TEST_ONE_TAG("Simple call with delay", simple_call_with_delay, "LeaksMemory"),
};

test_suite_t dialog_test_suite = {"Dialog", register_before_all, register_after_all, belle_sip_tester_before_each,
								  belle_sip_tester_after_each, sizeof(dialog_tests) / sizeof(dialog_tests[0]),
								  dialog_tests};
