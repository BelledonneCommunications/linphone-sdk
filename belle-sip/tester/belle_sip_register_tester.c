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
#include "belle_sip_tester.h"

#include "register_tester.h"


const char *test_domain="test.linphone.org";
const char *auth_domain="sip.linphone.org";
const char *client_auth_domain="client.example.org";
const char *client_auth_outband_proxy="sips:sip2.linphone.org:5063";

static int is_register_ok;
static int number_of_challenge;
static int using_transaction;
belle_sip_stack_t * stack;
belle_sip_provider_t *prov;
static belle_sip_listener_t* l;
belle_sip_request_t* authorized_request;
belle_sip_listener_callbacks_t listener_callbacks;
belle_sip_listener_t *listener;


static void process_dialog_terminated(void *user_ctx, const belle_sip_dialog_terminated_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	belle_sip_message("process_dialog_terminated called");
}

static void process_io_error(void *user_ctx, const belle_sip_io_error_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	belle_sip_message("process_io_error");
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
	/*CU_ASSERT(CU_FALSE);*/
}

static void process_request_event(void *user_ctx, const belle_sip_request_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	belle_sip_message("process_request_event");
}

static void process_response_event(void *user_ctx, const belle_sip_response_event_t *event){
	int status;
	belle_sip_request_t* request;
	BELLESIP_UNUSED(user_ctx);
	CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_response_event_get_response(event));
	belle_sip_message("process_response_event [%i] [%s]"
					,status=belle_sip_response_get_status_code(belle_sip_response_event_get_response(event))
					,belle_sip_response_get_reason_phrase(belle_sip_response_event_get_response(event)));
	if (status==401) {
		belle_sip_header_cseq_t* cseq;
		belle_sip_client_transaction_t *t;
		CU_ASSERT_NOT_EQUAL_FATAL(number_of_challenge,2);
		CU_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_response_event_get_client_transaction(event)); /*require transaction mode*/
		request=belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(belle_sip_response_event_get_client_transaction(event)));
		cseq=(belle_sip_header_cseq_t*)belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_CSEQ);
		belle_sip_header_cseq_set_seq_number(cseq,belle_sip_header_cseq_get_seq_number(cseq)+1);
		CU_ASSERT_TRUE_FATAL(belle_sip_provider_add_authorization(prov,request,belle_sip_response_event_get_response(event),NULL));
		t=belle_sip_provider_create_client_transaction(prov,request);
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

static void process_timeout(void *user_ctx, const belle_sip_timeout_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	belle_sip_message("process_timeout");
}

static void process_transaction_terminated(void *user_ctx, const belle_sip_transaction_terminated_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	belle_sip_message("process_transaction_terminated");
}

static const char* client_cert = /*for URI:sip:tester@client.example.org*/
		"-----BEGIN CERTIFICATE-----\n"
		"MIIDYzCCAsygAwIBAgIBCDANBgkqhkiG9w0BAQUFADCBuzELMAkGA1UEBhMCRlIx\n"
		"EzARBgNVBAgMClNvbWUtU3RhdGUxETAPBgNVBAcMCEdyZW5vYmxlMSIwIAYDVQQK\n"
		"DBlCZWxsZWRvbm5lIENvbW11bmljYXRpb25zMQwwCgYDVQQLDANMQUIxFjAUBgNV\n"
		"BAMMDUplaGFuIE1vbm5pZXIxOjA4BgkqhkiG9w0BCQEWK2plaGFuLm1vbm5pZXJA\n"
		"YmVsbGVkb25uZS1jb21tdW5pY2F0aW9ucy5jb20wHhcNMTMxMDAzMTQ0MTEwWhcN\n"
		"MjMxMDAxMTQ0MTEwWjCBtTELMAkGA1UEBhMCRlIxDzANBgNVBAgMBkZyYW5jZTER\n"
		"MA8GA1UEBwwIR3Jlbm9ibGUxIjAgBgNVBAoMGUJlbGxlZG9ubmUgQ29tbXVuaWNh\n"
		"dGlvbnMxDDAKBgNVBAsMA0xBQjEUMBIGA1UEAwwLY2xpZW50IGNlcnQxOjA4Bgkq\n"
		"hkiG9w0BCQEWK2plaGFuLm1vbm5pZXJAYmVsbGVkb25uZS1jb21tdW5pY2F0aW9u\n"
		"cy5jb20wgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBALZxC/qBi/zB/4lgI7V7\n"
		"I5TsmMmOp0+R/TCyVnYvKQuaJXh9i+CobVM7wj/pQg8RgsY1x+4mVwH1QbhOdIN0\n"
		"ExYHKgLTPlo9FaN6oHPOcHxU/wt552aZhCHC+ushwUUyjy8+T09UOP+xK9V7y5uD\n"
		"ZY+vIOvi6QNwc5cqyy8TREwNAgMBAAGjezB5MAkGA1UdEwQCMAAwLAYJYIZIAYb4\n"
		"QgENBB8WHU9wZW5TU0wgR2VuZXJhdGVkIENlcnRpZmljYXRlMB0GA1UdDgQWBBTL\n"
		"eWEg7jewRbQbKXSdBvsyygGIFzAfBgNVHSMEGDAWgBQGX13HFq9i+C1ucQOIoNYd\n"
		"KwR/ujANBgkqhkiG9w0BAQUFAAOBgQBTbEoi94pVfevbK22Oj8PJFsuy+el1pJG+\n"
		"h0XGM9SQGfbcS7PsV/MhFXtmpnmj3vQB3u5QtMGtWcLA2uxXi79i82gw+oEpBKHR\n"
		"sLqsNCzWNCL9n1pjpNSdqqBFGUdB9pSpnYalujAbuzkqq1ZLyzsElvK7pCaLQcSs\n"
		"oEncRDdPOA==\n"
		"-----END CERTIFICATE-----";


static const char* private_key =
		"-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
		"MIICxjBABgkqhkiG9w0BBQ0wMzAbBgkqhkiG9w0BBQwwDgQIbEHnQwhgRwoCAggA\n"
		"MBQGCCqGSIb3DQMHBAgmrtCEBCP9kASCAoCq9EKInROalaBSLWY44U4RVAC+CKdx\n"
		"Q8ooT7Bz/grgZuCiaGf0UKINJeV4LYHoP+AWjCH8EeebIA8dldNy5rGcBTt7sXd1\n"
		"QOGmnkBplXTW/NTsb9maYRK56kNJhLE4DR5X5keziV1Tdy2KBmTlpllsCXWsSOBq\n"
		"iI63PTaakIvZxA0TEmie5QQWpH777e/LmW3vVHdH8hhp2zeDDjfSW2E290+ce4Yj\n"
		"SDW9oFXvauzhzhSYRkUdfoJSbpu5MYwyzhjAXQpmBJDauu7+jAU/rQw6TLmYjDNZ\n"
		"3PYHzyD4N7tCG9u4mPBo33dhUirP+8E1BftHB+i/VIn6pI3ypMyiFZ1ZCHqi4vhW\n"
		"z7aChRrUY/8XWCpln3azcfj4SW+Mz62sAChY8rn+yyxFgIno8d9rrx67jyAnYJ6Q\n"
		"sfIMwKp3Sz5oI7IDk8If5SuBVkpqlRV+eZFT6zRRFk65beYpq70BN2mYaKzSV8A7\n"
		"rnciho/dfa9wvyWmkqXciBgWh18UTACOM9HPLmQef3FGaUDLiTAGS1osyypGUEPt\n"
		"Ox3u51qpYkibwyQZo1+ujQkh9PiKfevIAXmty0nTFWMEED15G2SJKjunw5N1rEAh\n"
		"M9jlYpLnATcfigPfGo19QrIPQ1c0LB4BqdwAWN3ZLe0QqYdgwzdcwIoLQRp9iDcw\n"
		"Omc31+38cTc2yGQ2Y2XHZkL8GY/rkqkbhVt9Rnh+VJxFeB6FlsL66EycApe07ngx\n"
		"QimGP57yp4aBzpJyW+6GPf8A/Ogsv3ay1QBLUiGEJtUglRHnl9F6nm5Nxm7wubVx\n"
		"WEuSefVM4xgB+mfQauAJu2N9yKhzXOytslZflpa06qJedlLYFk9njvcv\n"
		"-----END ENCRYPTED PRIVATE KEY-----\n";

static const char* private_key_passwd="secret";


static void process_auth_requested(void *user_ctx, belle_sip_auth_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	if (belle_sip_auth_event_get_mode(event) == BELLE_SIP_AUTH_MODE_HTTP_DIGEST) {
		belle_sip_message("process_auth_requested requested for [%s@%s]"
				,belle_sip_auth_event_get_username(event)
				,belle_sip_auth_event_get_realm(event));
		belle_sip_auth_event_set_passwd(event,"secret");
	} else if (belle_sip_auth_event_get_mode(event) == BELLE_SIP_AUTH_MODE_TLS) {
		belle_sip_certificates_chain_t* cert = belle_sip_certificates_chain_parse(client_cert,strlen(client_cert),BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
		belle_sip_signing_key_t* key = belle_sip_signing_key_parse(private_key,strlen(private_key),private_key_passwd);
		belle_sip_auth_event_set_client_certificates_chain(event,cert);
		belle_sip_auth_event_set_signing_key(event,key);
		belle_sip_message("process_auth_requested requested for  DN[%s]"
							,belle_sip_auth_event_get_distinguished_name(event));

	} else {
		belle_sip_error("Unexpected auth mode");
	}
}

int register_init(void) {
	belle_sip_listening_point_t *lp;
	stack=belle_sip_stack_new(NULL);
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"UDP");
	prov=belle_sip_stack_create_provider(stack,lp);
	
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"TCP");
	belle_sip_provider_add_listening_point(prov,lp);
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7061,"TLS");
	if (lp) {
		/* since test.linphone.org does not have proper certificates, don't verify anything*/
		belle_sip_tls_listening_point_set_verify_exceptions(BELLE_SIP_TLS_LISTENING_POINT(lp),BELLE_SIP_TLS_LISTENING_POINT_BADCERT_ANY_REASON);
		belle_sip_provider_add_listening_point(prov,lp);
	}

	listener_callbacks.process_dialog_terminated=process_dialog_terminated;
	listener_callbacks.process_io_error=process_io_error;
	listener_callbacks.process_request_event=process_request_event;
	listener_callbacks.process_response_event=process_response_event;
	listener_callbacks.process_timeout=process_timeout;
	listener_callbacks.process_transaction_terminated=process_transaction_terminated;
	listener_callbacks.process_auth_requested=process_auth_requested;
	listener_callbacks.listener_destroyed=NULL;
	listener=belle_sip_listener_create_from_callbacks(&listener_callbacks,NULL);
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
	belle_sip_header_cseq_t* cseq;
	belle_sip_header_expires_t* expires_header;
	int i;
	belle_sip_provider_add_sip_listener(prov,l);
	is_register_ok=0;
	using_transaction=0;
	req=(belle_sip_request_t*)belle_sip_object_clone((belle_sip_object_t*)initial_request);
	cseq=(belle_sip_header_cseq_t*)belle_sip_message_get_header((belle_sip_message_t*)req,BELLE_SIP_CSEQ);
	belle_sip_header_cseq_set_seq_number(cseq,belle_sip_header_cseq_get_seq_number(cseq)+2); /*+2 if initial reg was challenged*/
	expires_header=(belle_sip_header_expires_t*)belle_sip_message_get_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_EXPIRES);
	belle_sip_header_expires_set_expires(expires_header,0);
	if (use_transaction){
		belle_sip_client_transaction_t *t;
		belle_sip_message_remove_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_AUTHORIZATION);
		belle_sip_message_remove_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_PROXY_AUTHORIZATION);
		belle_sip_provider_add_authorization(prov,req,NULL,NULL); /*just in case*/
		t=belle_sip_provider_create_client_transaction(prov,req);
		belle_sip_client_transaction_send_request(t);
	}else belle_sip_provider_send_request(prov,req);
	for(i=0;!is_register_ok && i<2 ;i++)
		belle_sip_stack_sleep(stack,5000);
	CU_ASSERT_EQUAL(is_register_ok,1);
	CU_ASSERT_EQUAL(using_transaction,use_transaction);
	belle_sip_provider_remove_sip_listener(prov,l);
}

belle_sip_request_t* try_register_user_at_domain(belle_sip_stack_t * stack
					,belle_sip_provider_t *prov
					,const char *transport
					,int use_transaction
					,const char* username
					,const char* domain
					,const char* outbound_proxy
					,int success_expected) {
	belle_sip_request_t *req,*copy;
	char identity[256];
	char uri[256];
	int i;
	char *outbound=NULL;
	
	number_of_challenge=0;
	if (transport)
		snprintf(uri,sizeof(uri),"sip:%s;transport=%s",domain,transport);
	else snprintf(uri,sizeof(uri),"sip:%s",domain);

	if (transport && strcasecmp("tls",transport)==0 && belle_sip_provider_get_listening_point(prov,"tls")==NULL){
		belle_sip_error("No TLS support, test skipped.");
		return NULL;
	}
	
	if (outbound_proxy){
		if (strstr(outbound_proxy,"sip:")==NULL && strstr(outbound_proxy,"sips:")==NULL){
			outbound=belle_sip_strdup_printf("sip:%s",outbound_proxy);
		}else outbound=belle_sip_strdup(outbound_proxy);
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
		belle_sip_client_transaction_send_request_to(t,outbound?belle_sip_uri_parse(outbound):NULL);
	}else belle_sip_provider_send_request(prov,req);
	for(i=0;!is_register_ok && i<2 ;i++)
		belle_sip_stack_sleep(stack,5000);
	CU_ASSERT_EQUAL(is_register_ok,success_expected);
	if (success_expected) CU_ASSERT_EQUAL(using_transaction,use_transaction);

	belle_sip_provider_remove_sip_listener(prov,l);
	if (outbound) belle_sip_free(outbound);
	return copy;
}

belle_sip_request_t* register_user_at_domain(belle_sip_stack_t * stack
					,belle_sip_provider_t *prov
					,const char *transport
					,int use_transaction
					,const char* username
					,const char* domain
					,const char* outband) {
	return try_register_user_at_domain(stack,prov,transport,use_transaction,username,domain,outband,1);

}

belle_sip_request_t* register_user(belle_sip_stack_t * stack
					,belle_sip_provider_t *prov
					,const char *transport
					,int use_transaction
					,const char* username
					,const char* outbound) {
	return register_user_at_domain(stack,prov,transport,use_transaction,username,test_domain,outbound);
}

static void register_with_outbound(const char *transport, int use_transaction,const char* outbound ) {
	belle_sip_request_t *req;
	
	req=register_user(stack, prov, transport,use_transaction,"tester",outbound);
	if (req) {
		unregister_user(stack,prov,req,use_transaction);
		belle_sip_object_unref(req);
	}
}

static void register_test(const char *transport, int use_transaction) {
	register_with_outbound(transport,use_transaction,NULL);
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

static void stateful_register_udp_with_keep_alive(void) {
	belle_sip_listening_point_set_keep_alive(belle_sip_provider_get_listening_point(prov,"udp"),200);
	register_test(NULL,1);
	belle_sip_main_loop_sleep(belle_sip_stack_get_main_loop(stack),500);
	belle_sip_listening_point_set_keep_alive(belle_sip_provider_get_listening_point(prov,"udp"),-1);
}

static void stateful_register_udp_with_outbound_proxy(void){
	register_with_outbound("udp",1,test_domain);
}

static void stateful_register_udp_delayed(void){
	belle_sip_stack_set_tx_delay(stack,3000);
	register_test(NULL,1);
	belle_sip_stack_set_tx_delay(stack,0);
}

static void stateful_register_udp_with_send_error(void){
	belle_sip_stack_set_send_error(stack,-1);
	try_register_user_at_domain(stack, prov, NULL,1,"tester",test_domain,NULL,0);
	belle_sip_stack_set_send_error(stack,0);
}

static void stateful_register_tcp(void){
	register_test("tcp",1);
}

static void stateful_register_tls(void){
	register_test("tls",1);
}

static void bad_req_process_io_error(void *user_ctx, const belle_sip_io_error_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	belle_sip_message("bad_req_process_io_error not implemented yet");
}

static void bad_req_process_response_event(void *user_ctx, const belle_sip_response_event_t *event){
	int *bad_request_response_received=(int*)user_ctx;
	belle_sip_response_t *resp=belle_sip_response_event_get_response(event);
	CU_ASSERT_TRUE(resp && belle_sip_response_get_status_code(resp)==400);
	*bad_request_response_received=1;
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
}

static void test_bad_request(void) {
	belle_sip_request_t *req;
	belle_sip_listener_t *bad_req_listener;
	belle_sip_client_transaction_t *t;
	belle_sip_header_address_t* route_address=belle_sip_header_address_create(NULL,belle_sip_uri_create(NULL,test_domain));
	belle_sip_header_route_t* route;
	belle_sip_header_to_t* to = belle_sip_header_to_create2("sip:toto@titi.com",NULL);
	belle_sip_listener_callbacks_t cbs;
	int bad_request_response_received=0;
	memset(&cbs,0,sizeof(cbs));

	cbs.process_io_error=bad_req_process_io_error;
	cbs.process_response_event=bad_req_process_response_event;
	
	bad_req_listener = belle_sip_listener_create_from_callbacks(&cbs,&bad_request_response_received);

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
	t=belle_sip_provider_create_client_transaction(prov,req);
	belle_sip_client_transaction_send_request(t);
	belle_sip_stack_sleep(stack,3000);
	CU_ASSERT_TRUE(bad_request_response_received==1);
	belle_sip_provider_remove_sip_listener(prov,bad_req_listener);
	belle_sip_object_unref(bad_req_listener);
}

static void test_register_authenticate(void) {
	belle_sip_request_t *reg;
	number_of_challenge=0;
	authorized_request=NULL;
	reg=register_user_at_domain(stack, prov, "udp",1,"bellesip",auth_domain,NULL);
	if (authorized_request) {
		unregister_user(stack,prov,authorized_request,1);
		belle_sip_object_unref(authorized_request);
	}
	belle_sip_object_unref(reg);
}

static void test_register_channel_inactive(void){
	belle_sip_listening_point_t *lp=belle_sip_provider_get_listening_point(prov,"TCP");
	CU_ASSERT_PTR_NOT_NULL_FATAL(lp);
	belle_sip_stack_set_inactive_transport_timeout(stack,5);
	belle_sip_listening_point_clean_channels(lp);
	CU_ASSERT_EQUAL(belle_sip_listening_point_get_channel_count(lp),0);
	register_test("tcp",1);
	CU_ASSERT_EQUAL(belle_sip_listening_point_get_channel_count(lp),1);
	belle_sip_stack_sleep(stack,5000);
	CU_ASSERT_EQUAL(belle_sip_listening_point_get_channel_count(lp),0);
	belle_sip_stack_set_inactive_transport_timeout(stack,3600);
}



static void test_register_client_authenticated(void) {
	belle_sip_request_t *reg;
	authorized_request=NULL;
	/*we don't care to check sercer cert*/
	belle_sip_tls_listening_point_set_verify_exceptions(	(belle_sip_tls_listening_point_t*)belle_sip_provider_get_listening_point(prov,"tls")
															,BELLE_SIP_TLS_LISTENING_POINT_BADCERT_ANY_REASON);
	reg=register_user_at_domain(stack, prov, "tls",1,"tester",client_auth_domain,client_auth_outband_proxy);
	if (authorized_request) {
		unregister_user(stack,prov,authorized_request,1);
		belle_sip_object_unref(authorized_request);
	}
	if (reg) belle_sip_object_unref(reg);
}


test_t register_tests[] = {
	{ "Stateful UDP", stateful_register_udp },
	{ "Stateful UDP with keep-alive", stateful_register_udp_with_keep_alive },
	{ "Stateful UDP with network delay", stateful_register_udp_delayed },
	{ "Stateful UDP with send error", stateful_register_udp_with_send_error },
	{ "Stateful UDP with outbound proxy", stateful_register_udp_with_outbound_proxy },
	{ "Stateful TCP", stateful_register_tcp },
	{ "Stateful TLS", stateful_register_tls },
	{ "Stateless UDP", stateless_register_udp },
	{ "Stateless TCP", stateless_register_tcp },
	{ "Stateless TLS", stateless_register_tls },
	{ "Bad TCP request", test_bad_request },
	{ "Authenticate", test_register_authenticate },
	{ "TLS client cert authentication", test_register_client_authenticated },
	{ "Channel inactive", test_register_channel_inactive }
};

test_suite_t register_test_suite = {
	"REGISTER",
	register_init,
	register_uninit,
	sizeof(register_tests) / sizeof(register_tests[0]),
	register_tests
};

