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
#include "belle_sip_internal.h"
#include "belle_sip_tester.h"

#include "register_tester.h"


const char *test_domain="sip2.linphone.org";
const char *auth_domain="sip.linphone.org";
const char *client_auth_domain="client.example.org";
const char *client_auth_outbound_proxy="sips:sip2.linphone.org:5063";
const char *no_server_running_here="sip:test.linphone.org:3;transport=tcp";
const char *no_response_here="sip:78.220.48.77:3;transport=%s";
const char *test_domain_tls_to_tcp="sip:sip2.linphone.org:5060;transport=tls";
const char *test_http_proxy_addr="sip.linphone.org";
int test_http_proxy_port = 3128 ;

static int is_register_ok;
static int number_of_challenge;
static int using_transaction;
static int io_error_count=0;

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
	belle_sip_message("process_io_error, exiting main loop");
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
	io_error_count++;
	/*BC_ASSERT(CU_FALSE);*/
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
	BC_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_response_event_get_response(event));
	belle_sip_message("process_response_event [%i] [%s]"
					,status=belle_sip_response_get_status_code(belle_sip_response_event_get_response(event))
					,belle_sip_response_get_reason_phrase(belle_sip_response_event_get_response(event)));


	if (status==401){
		belle_sip_header_cseq_t* cseq;
		belle_sip_client_transaction_t *t;
		belle_sip_uri_t *dest;
		// BC_ASSERT_NOT_EQUAL_FATAL(number_of_challenge,2);
		BC_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_response_event_get_client_transaction(event)); /*require transaction mode*/
		dest=belle_sip_client_transaction_get_route(belle_sip_response_event_get_client_transaction(event));
		request=belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(belle_sip_response_event_get_client_transaction(event)));
		cseq=(belle_sip_header_cseq_t*)belle_sip_message_get_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_CSEQ);
		belle_sip_header_cseq_set_seq_number(cseq,belle_sip_header_cseq_get_seq_number(cseq)+1);
		belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_AUTHORIZATION);
		belle_sip_message_remove_header(BELLE_SIP_MESSAGE(request),BELLE_SIP_PROXY_AUTHORIZATION);
		BC_ASSERT_TRUE_FATAL(belle_sip_provider_add_authorization(prov,request,belle_sip_response_event_get_response(event),NULL,NULL,auth_domain));

		t=belle_sip_provider_create_client_transaction(prov,request);
		belle_sip_client_transaction_send_request_to(t,dest);
		number_of_challenge++;
		authorized_request=request;
		belle_sip_object_ref(authorized_request);
	}  else {
		BC_ASSERT_EQUAL(status,200,int,"%d");
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

const char* belle_sip_tester_client_cert = /*for URI:sip:tester@client.example.org*/
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

/* fingerprint of certificate generated using openssl x509 -fingerprint */
const char* belle_sip_tester_client_cert_fingerprint =
		"SHA-1 79:2F:9E:8B:28:CC:38:53:90:1D:71:DC:8F:70:66:75:E5:34:CE:C4";

const char* belle_sip_tester_private_key =
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

const char* belle_sip_tester_private_key_passwd="secret";


static void process_auth_requested(void *user_ctx, belle_sip_auth_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	if (belle_sip_auth_event_get_mode(event) == BELLE_SIP_AUTH_MODE_HTTP_DIGEST) {
		const char *username = belle_sip_auth_event_get_username(event);
		const char *realm = belle_sip_auth_event_get_realm(event);
		belle_sip_message("process_auth_requested requested for [%s@%s]"
				,username?username:""
				,realm?realm:"");
		belle_sip_auth_event_set_passwd(event,"secret");
	} else if (belle_sip_auth_event_get_mode(event) == BELLE_SIP_AUTH_MODE_TLS) {
		const char *distinguished_name = NULL;
		belle_sip_certificates_chain_t* cert = belle_sip_certificates_chain_parse(belle_sip_tester_client_cert,strlen(belle_sip_tester_client_cert),BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
		belle_sip_signing_key_t* key = belle_sip_signing_key_parse(belle_sip_tester_private_key,strlen(belle_sip_tester_private_key),belle_sip_tester_private_key_passwd);
		belle_sip_auth_event_set_client_certificates_chain(event,cert);
		belle_sip_auth_event_set_signing_key(event,key);
		distinguished_name = belle_sip_auth_event_get_distinguished_name(event);
		belle_sip_message("process_auth_requested requested for  DN[%s]",distinguished_name?distinguished_name:"");

	} else {
		belle_sip_error("Unexpected auth mode");
	}
}

int register_before_all(void) {
	belle_sip_listening_point_t *lp;
	stack=belle_sip_stack_new(NULL);
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"UDP");
	prov=belle_sip_stack_create_provider(stack,lp);

	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"TCP");
	belle_sip_provider_add_listening_point(prov,lp);
	lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7061,"TLS");
	if (lp) {
		belle_tls_crypto_config_t *crypto_config=belle_tls_crypto_config_new();
		/* since test.linphone.org does not have proper certificates, don't verify anything*/
		belle_tls_crypto_config_set_verify_exceptions(crypto_config, BELLE_TLS_VERIFY_ANY_REASON);
		if (belle_sip_tester_get_root_ca_path() != NULL) {
			belle_tls_crypto_config_set_root_ca(crypto_config,belle_sip_tester_get_root_ca_path());
		}
		belle_sip_tls_listening_point_set_crypto_config(BELLE_SIP_TLS_LISTENING_POINT(lp), crypto_config);
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

int register_after_all(void) {
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
	belle_sip_object_ref(req);
	cseq=(belle_sip_header_cseq_t*)belle_sip_message_get_header((belle_sip_message_t*)req,BELLE_SIP_CSEQ);
	belle_sip_header_cseq_set_seq_number(cseq,belle_sip_header_cseq_get_seq_number(cseq)+2); /*+2 if initial reg was challenged*/
	expires_header=(belle_sip_header_expires_t*)belle_sip_message_get_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_EXPIRES);
	belle_sip_header_expires_set_expires(expires_header,0);
	if (use_transaction){
		belle_sip_client_transaction_t *t;
		belle_sip_provider_add_authorization(prov,req,NULL,NULL,NULL,NULL); /*just in case*/
		t=belle_sip_provider_create_client_transaction(prov,req);
		belle_sip_client_transaction_send_request(t);
	}else belle_sip_provider_send_request(prov,req);
	for(i=0;!is_register_ok && i<20 ;i++) {
		belle_sip_stack_sleep(stack,500);
		if (!use_transaction && !is_register_ok) {
			belle_sip_object_ref(req);
			belle_sip_provider_send_request(prov,req); /*manage retransmitions*/
		}
	}

	BC_ASSERT_EQUAL(is_register_ok,1,int,"%d");
	BC_ASSERT_EQUAL(using_transaction,use_transaction,int,"%d");
	belle_sip_object_unref(req);
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
	int do_manual_retransmissions = FALSE;

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
	belle_sip_object_ref(req);
	is_register_ok=0;
	io_error_count=0;
	using_transaction=0;
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_expires_create(600)));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_contact_new()));
	copy=(belle_sip_request_t*)belle_sip_object_ref(belle_sip_object_clone((belle_sip_object_t*)req));
	belle_sip_provider_add_sip_listener(prov,l=BELLE_SIP_LISTENER(listener));
	if (use_transaction){
		belle_sip_client_transaction_t *t=belle_sip_provider_create_client_transaction(prov,req);
		belle_sip_client_transaction_send_request_to(t,outbound?belle_sip_uri_parse(outbound):NULL);
	}else{
		belle_sip_provider_send_request(prov,req);
		do_manual_retransmissions = (transport == NULL) || (strcasecmp(transport,"udp") == 0);
	}
	for(i=0;!is_register_ok && i<20 && io_error_count==0;i++) {
		belle_sip_stack_sleep(stack,500);
		if (do_manual_retransmissions && !is_register_ok) {
			belle_sip_object_ref(req);
			belle_sip_provider_send_request(prov,req); /*manage retransmitions*/
		}
	}
	BC_ASSERT_EQUAL(is_register_ok,success_expected,int,"%d");
	if (success_expected) BC_ASSERT_EQUAL(using_transaction,use_transaction,int,"%d");

	belle_sip_object_unref(req);
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
					,const char* outbound) {
	return try_register_user_at_domain(stack,prov,transport,use_transaction,username,domain,outbound,1);

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
	belle_sip_request_t *req;
	belle_sip_stack_set_send_error(stack,-1);
	req=try_register_user_at_domain(stack, prov, NULL,1,"tester",test_domain,NULL,0);
	belle_sip_stack_set_send_error(stack,0);
	if (req) belle_sip_object_unref(req);
}

static void stateful_register_tcp(void){
	register_test("tcp",1);
}

static void stateful_register_tls(void){
	register_test("tls",1);
}

static void stateful_register_tls_with_http_proxy(void) {
	belle_sip_tls_listening_point_t * lp = (belle_sip_tls_listening_point_t*)belle_sip_provider_get_listening_point(prov, "tls");
	if (!lp) {
		belle_sip_error("No TLS support, test skipped.");
		return;
	}
	belle_sip_provider_clean_channels(prov);
	belle_sip_stack_set_http_proxy_host(stack, test_http_proxy_addr);
	belle_sip_stack_set_http_proxy_port(stack, test_http_proxy_port);
	register_test("tls",1);
	belle_sip_stack_set_http_proxy_host(stack, NULL);
	belle_sip_stack_set_http_proxy_port(stack, 0);

}

static void stateful_register_tls_with_wrong_http_proxy(void){
	
	belle_sip_tls_listening_point_t * lp = (belle_sip_tls_listening_point_t*)belle_sip_provider_get_listening_point(prov, "tls");
	if (!lp) {
		belle_sip_error("No TLS support, test skipped.");
		return;
	}
	belle_sip_provider_clean_channels(prov);
	belle_sip_stack_set_http_proxy_host(stack, "mauvaisproxy.linphone.org");
	belle_sip_stack_set_http_proxy_port(stack, test_http_proxy_port);
	try_register_user_at_domain(stack,prov,"tls",1,"tester",test_domain,NULL,0);
	belle_sip_stack_set_http_proxy_host(stack, NULL);
	belle_sip_stack_set_http_proxy_port(stack, 0);
	
}


static void bad_req_process_io_error(void *user_ctx, const belle_sip_io_error_event_t *event){
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	belle_sip_message("bad_req_process_io_error not implemented yet");
}

static void bad_req_process_response_event(void *user_ctx, const belle_sip_response_event_t *event){
	belle_sip_response_t *resp=belle_sip_response_event_get_response(event);
	int *bad_request_response_received=(int*)user_ctx;
	if (belle_sip_response_event_get_client_transaction(event) != NULL) {
		BC_ASSERT_TRUE(resp && belle_sip_response_get_status_code(resp)==400);
		*bad_request_response_received=1;
		belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
	}
}

static void test_bad_request(void) {
	belle_sip_request_t *req;
	belle_sip_listener_t *bad_req_listener;
	belle_sip_client_transaction_t *t;
	belle_sip_header_address_t* route_address=belle_sip_header_address_create(NULL,belle_sip_uri_create(NULL,test_domain));
	belle_sip_header_route_t* route;
	belle_sip_header_to_t* to = belle_sip_header_to_create2("sip:toto@titi.com",NULL);
	belle_sip_listener_callbacks_t cbs;
	belle_sip_listening_point_t *lp=belle_sip_provider_get_listening_point(prov,"TCP");
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
	BC_ASSERT_TRUE(bad_request_response_received==1);
	belle_sip_provider_remove_sip_listener(prov,bad_req_listener);
	belle_sip_object_unref(bad_req_listener);

	belle_sip_listening_point_clean_channels(lp);
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
	BC_ASSERT_PTR_NOT_NULL_FATAL(lp);
	belle_sip_stack_set_inactive_transport_timeout(stack,5);
	belle_sip_listening_point_clean_channels(lp);
	BC_ASSERT_EQUAL(belle_sip_listening_point_get_channel_count(lp),0,int,"%d");
	register_test("tcp",1);
	BC_ASSERT_EQUAL(belle_sip_listening_point_get_channel_count(lp),1,int,"%d");
	belle_sip_stack_sleep(stack,5000);
	BC_ASSERT_EQUAL(belle_sip_listening_point_get_channel_count(lp),0,int,"%d");
	belle_sip_stack_set_inactive_transport_timeout(stack,3600);
}



static void test_register_client_authenticated(void) {
	belle_sip_request_t *reg;
	authorized_request=NULL;

	reg=register_user_at_domain(stack, prov, "tls",1,"tester",client_auth_domain,client_auth_outbound_proxy);
	if (authorized_request) {
		unregister_user(stack,prov,authorized_request,1);
		belle_sip_object_unref(authorized_request);
	}
	if (reg) belle_sip_object_unref(reg);
}

static void test_connection_failure(void){
	belle_sip_request_t *req;
	io_error_count=0;
	req=try_register_user_at_domain(stack, prov, "TCP",1,"tester","sip.linphone.org",no_server_running_here,0);
	BC_ASSERT_TRUE(io_error_count>=1);
	if (req) belle_sip_object_unref(req);
}

static void test_connection_too_long(const char *transport){
	belle_sip_request_t *req;
	int orig=belle_sip_stack_get_transport_timeout(stack);
	char *no_response_here_with_transport = belle_sip_strdup_printf(no_response_here, transport);
	io_error_count=0;
	if (transport && strcasecmp("tls",transport)==0 && belle_sip_provider_get_listening_point(prov,"tls")==NULL){
		belle_sip_error("No TLS support, test skipped.");
		return;
	}
	belle_sip_stack_set_transport_timeout(stack,2000);
	req=try_register_user_at_domain(stack, prov, transport,1,"tester","sip.linphone.org",no_response_here_with_transport,0);
	BC_ASSERT_GREATER(io_error_count, 1, int, "%d");
	belle_sip_stack_set_transport_timeout(stack,orig);
	belle_sip_free(no_response_here_with_transport);
	if (req) belle_sip_object_unref(req);
}

static void test_connection_too_long_tcp(void){
	test_connection_too_long("tcp");
}

static void test_connection_too_long_tls(void){
	test_connection_too_long("tls");
}

static void test_tls_to_tcp(void){
	belle_sip_request_t *req;
	int orig=belle_sip_stack_get_transport_timeout(stack);
	io_error_count=0;
	belle_sip_stack_set_transport_timeout(stack,2000);
	req=try_register_user_at_domain(stack, prov, "TLS",1,"tester",test_domain,test_domain_tls_to_tcp,0);
	if (req){
		BC_ASSERT_TRUE(io_error_count>=1);
		belle_sip_object_unref(req);
	}
	belle_sip_stack_set_transport_timeout(stack,orig);
}

static void register_dns_srv_tcp(void){
	belle_sip_request_t *req;
	io_error_count=0;
	req=try_register_user_at_domain(stack, prov, "TCP",1,"tester",client_auth_domain,"sip:linphone.net;transport=tcp",1);
	BC_ASSERT_TRUE(io_error_count==0);
	if (req) belle_sip_object_unref(req);
}

static void register_dns_srv_tls(void){
	belle_sip_request_t *req;
	io_error_count=0;
	req=try_register_user_at_domain(stack, prov, "TLS",1,"tester",client_auth_domain,"sip:linphone.net;transport=tls",1);
	BC_ASSERT_EQUAL(io_error_count, 0, int, "%d");
	if (req) belle_sip_object_unref(req);
}

static void register_dns_srv_tls_with_http_proxy(void){
	belle_sip_request_t *req;
	belle_sip_tls_listening_point_t * lp = (belle_sip_tls_listening_point_t*)belle_sip_provider_get_listening_point(prov, "tls");
	if (!lp) {
		belle_sip_error("No TLS support, test skipped.");
		return;
	}
	io_error_count=0;
	belle_sip_provider_clean_channels(prov);
	belle_sip_stack_set_http_proxy_host(stack, test_http_proxy_addr);
	belle_sip_stack_set_http_proxy_port(stack, test_http_proxy_port);
	req=try_register_user_at_domain(stack, prov, "TLS",1,"tester",client_auth_domain,"sip:linphone.net;transport=tls",1);
	belle_sip_stack_set_http_proxy_host(stack, NULL);
	belle_sip_stack_set_http_proxy_port(stack, 0);
	BC_ASSERT_EQUAL(io_error_count, 0, int, "%d");
	if (req) belle_sip_object_unref(req);
}

static void register_dns_load_balancing(void) {
	belle_sip_request_t *req;
	io_error_count = 0;
	req = try_register_user_at_domain(stack, prov, "TCP", 1, "tester", client_auth_domain, "sip:belle-sip.net;transport=tcp", 1);
	BC_ASSERT_EQUAL(io_error_count, 0, int, "%d");
	if (req) belle_sip_object_unref(req);
}

static void process_message_response_event(void *user_ctx, const belle_sip_response_event_t *event){
	int status;
	BELLESIP_UNUSED(user_ctx);
	BC_ASSERT_PTR_NOT_NULL_FATAL(belle_sip_response_event_get_response(event));
	belle_sip_message("process_response_event [%i] [%s]"
					,status=belle_sip_response_get_status_code(belle_sip_response_event_get_response(event))
					,belle_sip_response_get_reason_phrase(belle_sip_response_event_get_response(event)));

	if (status >= 200){
		is_register_ok=status;
		belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
	}
}
static belle_sip_request_t* send_message(belle_sip_request_t *initial_request, const char* realm){
	int i;
	int io_error_count=0;
	belle_sip_request_t *message_request=NULL;
	belle_sip_request_t *clone_request=NULL;
	// belle_sip_header_authorization_t * h=NULL;

	is_register_ok = 0;

	message_request=belle_sip_request_create(
								belle_sip_uri_parse("sip:sip.linphone.org;transport=tcp")
								,"MESSAGE"
								,belle_sip_provider_create_call_id(prov)
								,belle_sip_header_cseq_create(22,"MESSAGE")
								,belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(initial_request), belle_sip_header_from_t)
								,belle_sip_header_to_parse("To: sip:marie@sip.linphone.org")
								,belle_sip_header_via_new()
								,70);
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(message_request),BELLE_SIP_HEADER(belle_sip_header_expires_create(600)));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(message_request),BELLE_SIP_HEADER(belle_sip_header_contact_new()));
	belle_sip_provider_add_authorization(prov,message_request,NULL,NULL,NULL,realm);
	// h = belle_sip_message_get_header_by_type(BELLE_SIP_MESSAGE(message_request), belle_sip_header_authorization_t);
	/*if a matching authorization was found, use it as a proxy authorization*/
	// if (h != NULL){
	// 	belle_sip_header_set_name(BELLE_SIP_HEADER(h), BELLE_SIP_PROXY_AUTHORIZATION);
	// }
	clone_request = (belle_sip_request_t*)belle_sip_object_ref(belle_sip_object_clone((belle_sip_object_t*)message_request));
	belle_sip_client_transaction_send_request_to(belle_sip_provider_create_client_transaction(prov,message_request),NULL);
	for(i=0; i<2 && io_error_count==0 &&is_register_ok==0;i++)
		belle_sip_stack_sleep(stack,5000);

	return clone_request;
}
static void reuse_nonce(void) {
	belle_sip_request_t *register_request;
	int initial_auth_context_count=belle_sip_list_size(prov->auth_contexts);
	register_request=register_user_at_domain(stack, prov, "tcp",1,"marie","sip.linphone.org",NULL);
	if (register_request) {
		char * first_nonce_used;
		belle_sip_header_authorization_t * h = NULL;
		belle_sip_request_t *message_request;
		listener_callbacks.process_dialog_terminated=process_dialog_terminated;
		listener_callbacks.process_io_error=process_io_error;
		listener_callbacks.process_request_event=process_request_event;
		listener_callbacks.process_response_event=process_message_response_event;
		listener_callbacks.process_timeout=process_timeout;
		listener_callbacks.process_transaction_terminated=process_transaction_terminated;
		listener_callbacks.process_auth_requested=process_auth_requested;
		listener_callbacks.listener_destroyed=NULL;
		listener=belle_sip_listener_create_from_callbacks(&listener_callbacks,NULL);

		belle_sip_provider_add_sip_listener(prov,BELLE_SIP_LISTENER(listener));

		/*currently only one nonce should have been used (the one for the REGISTER)*/
		BC_ASSERT_EQUAL(belle_sip_list_size(prov->auth_contexts), initial_auth_context_count+1,int,"%d");

		/*this should reuse previous nonce*/
		message_request=send_message(register_request, auth_domain);
		BC_ASSERT_EQUAL(is_register_ok, 404,int,"%d");
		 h = BELLE_SIP_HEADER_AUTHORIZATION(belle_sip_message_get_header_by_type(
					BELLE_SIP_MESSAGE(message_request), belle_sip_header_proxy_authorization_t
				));
		BC_ASSERT_PTR_NOT_NULL_FATAL(h);
		BC_ASSERT_EQUAL(2, belle_sip_header_authorization_get_nonce_count(h),int,"%d");
		first_nonce_used = belle_sip_strdup(belle_sip_header_authorization_get_nonce(h));

		belle_sip_object_unref(message_request);


		/*new nonce should be created when not using outbound proxy realm*/
		message_request=send_message(register_request, NULL);
		BC_ASSERT_EQUAL(is_register_ok, 407,int,"%d");
		h = BELLE_SIP_HEADER_AUTHORIZATION(belle_sip_message_get_header_by_type(
				BELLE_SIP_MESSAGE(message_request), belle_sip_header_proxy_authorization_t
			));
		BC_ASSERT_PTR_NULL_FATAL(h);
		belle_sip_object_unref(message_request);


		/*new nonce should be created here too*/
		message_request=send_message(register_request, "wrongrealm");
		BC_ASSERT_EQUAL(is_register_ok, 407,int,"%d");
		h = BELLE_SIP_HEADER_AUTHORIZATION(belle_sip_message_get_header_by_type(
				BELLE_SIP_MESSAGE(message_request), belle_sip_header_proxy_authorization_t
			));
		BC_ASSERT_PTR_NULL_FATAL(h);
		belle_sip_object_unref(message_request);


		/*first nonce created should be reused*/
		message_request=send_message(register_request, auth_domain);
		BC_ASSERT_EQUAL(is_register_ok, 404,int,"%d");
		h = BELLE_SIP_HEADER_AUTHORIZATION(belle_sip_message_get_header_by_type(
				BELLE_SIP_MESSAGE(message_request), belle_sip_header_proxy_authorization_t
			));
		BC_ASSERT_PTR_NOT_NULL_FATAL(h);
		BC_ASSERT_EQUAL(3, belle_sip_header_authorization_get_nonce_count(h),int,"%d");
		belle_sip_object_unref(message_request);

		belle_sip_provider_remove_sip_listener(prov,BELLE_SIP_LISTENER(listener));
		unregister_user(stack,prov,register_request,1);
		belle_sip_object_unref(register_request);
		belle_sip_free(first_nonce_used);
	}
}


test_t register_tests[] = {
	{ "Stateful UDP", stateful_register_udp },
	{ "Stateful UDP with keep-alive", stateful_register_udp_with_keep_alive },
	{ "Stateful UDP with network delay", stateful_register_udp_delayed },
	{ "Stateful UDP with send error", stateful_register_udp_with_send_error },
	{ "Stateful UDP with outbound proxy", stateful_register_udp_with_outbound_proxy },
	{ "Stateful TCP", stateful_register_tcp },
	{ "Stateful TLS", stateful_register_tls },
	{ "Stateful TLS with http proxy", stateful_register_tls_with_http_proxy },
	{ "Stateful TLS with wrong http proxy", stateful_register_tls_with_wrong_http_proxy },
	{ "Stateless UDP", stateless_register_udp },
	{ "Stateless TCP", stateless_register_tcp },
	{ "Stateless TLS", stateless_register_tls },
	{ "Bad TCP request", test_bad_request },
	{ "Authenticate", test_register_authenticate },
	{ "TLS client cert authentication", test_register_client_authenticated },
	{ "Channel inactive", test_register_channel_inactive },
	{ "TCP connection failure", test_connection_failure },
	{ "TCP connection too long", test_connection_too_long_tcp },
	{ "TLS connection too long", test_connection_too_long_tls },
	{ "TLS connection to TCP server", test_tls_to_tcp },
	{ "Register with DNS SRV failover TCP", register_dns_srv_tcp },
	{ "Register with DNS SRV failover TLS", register_dns_srv_tls },
	{ "Register with DNS SRV failover TLS with http proxy", register_dns_srv_tls_with_http_proxy },
	{ "Register with DNS load-balancing", register_dns_load_balancing },
	{ "Nonce reutilization", reuse_nonce }
};

test_suite_t register_test_suite = {"REGISTER", register_before_all, register_after_all, NULL,
									NULL, sizeof(register_tests) / sizeof(register_tests[0]),
									register_tests};
