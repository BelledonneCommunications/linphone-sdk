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

extern belle_sip_stack_t * stack;
extern belle_sip_provider_t *prov;
extern const char *test_domain;
extern int register_init(void);
extern int register_uninit(void);
extern belle_sip_request_t* register_user(belle_sip_stack_t * stack
		,belle_sip_provider_t *prov
		,const char *transport
		,int use_transaction
		,const char* username) ;
extern void unregister_user(belle_sip_stack_t * stack
					,belle_sip_provider_t *prov
					,belle_sip_request_t* initial_request
					,int use_transaction);



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

static int init(void) {
	register_init();
	return 0;
}
static int uninit(void) {
	register_uninit();

	return 0;
}

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

static belle_sip_dialog_t* caller_dialog;
static belle_sip_dialog_t* callee_dialog;
static belle_sip_response_t* ok_response;
static belle_sip_server_transaction_t* inserv_transaction;

static void process_dialog_terminated(void *user_ctx, const belle_sip_dialog_terminated_event_t *event){
	belle_sip_message("process_dialog_terminated not implemented yet");
}
static void process_io_error(void *user_ctx, const belle_sip_io_error_event_t *event){
	belle_sip_message("process_io_error not implemented yet");
}
static void process_request_event(void *user_ctx, const belle_sip_request_event_t *event) {
	belle_sip_server_transaction_t* server_transaction = belle_sip_request_event_get_server_transaction(event);
	if (!server_transaction) {
		server_transaction= belle_sip_provider_create_server_transaction(prov,belle_sip_request_event_get_request(event));
	}
	belle_sip_dialog_t* dialog =  belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(server_transaction));
	belle_sip_response_t* ringing_response;
	belle_sip_header_content_type_t* content_type ;
	belle_sip_header_content_length_t* content_length;
	if (!dialog ) {
		CU_ASSERT_STRING_EQUAL_FATAL("INVITE",belle_sip_request_get_method(belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(server_transaction))))
		dialog=belle_sip_provider_create_dialog(prov,BELLE_SIP_TRANSACTION(server_transaction));
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
		/*only send ringing*/
		belle_sip_server_transaction_send_response(server_transaction,ringing_response);
	}
	belle_sip_message("process_request_event not implemented yet");
}

static void caller_process_response_event(void *user_ctx, const belle_sip_response_event_t *event){
	belle_sip_client_transaction_t* client_transaction = belle_sip_response_event_get_client_transaction(event);
	int status = belle_sip_response_get_status_code(belle_sip_response_event_get_response(event));
	belle_sip_message("caller_process_response_event [%i]",status);
	CU_ASSERT_PTR_NOT_NULL_FATAL(client_transaction);
	belle_sip_dialog_t* dialog =  belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(client_transaction));
	CU_ASSERT_PTR_NOT_NULL_FATAL(dialog);
	if (belle_sip_dialog_get_state(dialog) == BELLE_SIP_DIALOG_NULL) {
		CU_ASSERT_EQUAL(status,100);
		callee_dialog=dialog;
	} else if (belle_sip_dialog_get_state(dialog) == BELLE_SIP_DIALOG_EARLY){
		CU_ASSERT_EQUAL(status,180);
		/*send 200ok from callee*/
		belle_sip_server_transaction_send_response(inserv_transaction,ok_response);
	}



}
static void callee_process_response_event(void *user_ctx, const belle_sip_response_event_t *event){
	/*belle_sip_client_transaction_t* server_transaction = belle_sip_response_event_get_client_transaction(event);*/
	belle_sip_message("callee_process_response_event");

}
static void process_timeout(void *user_ctx, const belle_sip_timeout_event_t *event) {
/*	belle_sip_client_transaction_t* client_transaction = belle_sip_timeout_event_get_client_transaction(event);
	SalOp* op = (SalOp*)belle_sip_transaction_get_application_data(BELLE_SIP_TRANSACTION(client_transaction));
	if (op->callbacks.process_timeout) {
		op->callbacks.process_timeout(op,event);
	} else*/ {
		belle_sip_message("Unhandled event timeout [%p]",event);
	}
}
static void process_transaction_terminated(void *user_ctx, const belle_sip_transaction_terminated_event_t *event) {
/*	belle_sip_client_transaction_t* client_transaction = belle_sip_transaction_terminated_event_get_client_transaction(event);
	SalOp* op = (SalOp*)belle_sip_transaction_get_application_data(client_transaction);
	if (op->calbacks.process_transaction_terminated) {
		op->calbacks.process_transaction_terminated(op,event);
	} else */{
		belle_sip_message("Unhandled transaction terminated [%p]",event);
	}
}



static void simple_call(void) {
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

	caller_listener_callbacks.process_dialog_terminated=process_dialog_terminated;
	caller_listener_callbacks.process_io_error=process_io_error;
	caller_listener_callbacks.process_request_event=NULL;
	caller_listener_callbacks.process_response_event=caller_process_response_event;
	caller_listener_callbacks.process_timeout=process_timeout;
	caller_listener_callbacks.process_transaction_terminated=process_transaction_terminated;

	callee_listener_callbacks.process_dialog_terminated=process_dialog_terminated;
	callee_listener_callbacks.process_io_error=process_io_error;
	callee_listener_callbacks.process_request_event=process_request_event;
	callee_listener_callbacks.process_response_event=callee_process_response_event;
	callee_listener_callbacks.process_timeout=process_timeout;
	callee_listener_callbacks.process_transaction_terminated=process_transaction_terminated;

	pauline_register_req=register_user(stack, prov, "TCP" ,1 ,CALLER);
	marie_register_req=register_user(stack, prov, "TLS" ,1 ,CALLEE);

	belle_sip_provider_add_sip_listener(prov,caller_listener=belle_sip_listener_create_from_callbacks(&caller_listener_callbacks,NULL));
	belle_sip_provider_add_sip_listener(prov,callee_listener=belle_sip_listener_create_from_callbacks(&callee_listener_callbacks,NULL));

	from=belle_sip_header_address_create(NULL,belle_sip_uri_create(CALLER,test_domain));
	to=belle_sip_header_address_create(NULL,belle_sip_uri_create(CALLEE,test_domain));
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
	CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(client_transaction)));
	//belle_sip_transaction_set_application_data(BELLE_SIP_TRANSACTION(client_transaction),op);
	belle_sip_client_transaction_send_request(client_transaction);
	belle_sip_stack_sleep(stack,3000);

	belle_sip_provider_remove_sip_listener(prov,caller_listener);
	belle_sip_provider_remove_sip_listener(prov,callee_listener);

	unregister_user(stack, prov, pauline_register_req ,1);
	unregister_user(stack, prov, marie_register_req ,1);
}
int belle_sip_dialog_test_suite(){
	CU_pSuite pSuite = CU_add_suite("Dialog", init, uninit);

	if (NULL == CU_add_test(pSuite, "simple call", simple_call)) {
		return CU_get_error();
	}
	return 0;
}

