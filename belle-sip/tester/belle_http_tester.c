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
#include "belle_sip_tester.h"
#include <belle_sip_internal.h>

extern const char *root_ca_path;

typedef struct http_counters{
	int response_count;
	int io_error_count;
	int two_hundred;
	int three_hundred;
	int four_hundred;
}http_counters_t;

static int wait_for(belle_sip_stack_t*s1,int* counter,int value,int timeout) {
	int retry=0;
#define SLEEP_TIME 100
	while (*counter!=value && retry++ <(timeout/SLEEP_TIME)) {
		if (s1) belle_sip_stack_sleep(s1,SLEEP_TIME);
	}
	if (*counter!=value) return FALSE;
	else return TRUE;
}

static void process_response(void *data, const belle_http_response_event_t *event){
	http_counters_t *counters=(http_counters_t*)data;
	counters->response_count++;
	CU_ASSERT_PTR_NOT_NULL(event->response);
	if (event->response){
		int code=belle_http_response_get_status_code(event->response);
		const char *body=belle_sip_message_get_body(BELLE_SIP_MESSAGE(event->response));
		if (code>=200 && code <300)
			counters->two_hundred++;
		else if (code>=300 && code <400)
			counters->three_hundred++;
		else if (code>=300 && code <400)
			counters->four_hundred++;

		CU_ASSERT_PTR_NOT_NULL(body);
	}
}

static void process_io_error(void *data, const belle_sip_io_error_event_t *event){
	http_counters_t *counters=(http_counters_t*)data;
	counters->io_error_count++;
}

static void process_auth_requested(void *data, belle_sip_auth_event_t *event){
	if (belle_sip_auth_event_get_mode(event)==BELLE_SIP_AUTH_MODE_TLS){
		belle_sip_certificates_chain_t* cert = belle_sip_certificates_chain_parse(belle_sip_tester_client_cert,strlen(belle_sip_tester_client_cert),BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
		belle_sip_signing_key_t* key = belle_sip_signing_key_parse(belle_sip_tester_private_key,strlen(belle_sip_tester_private_key),belle_sip_tester_private_key_passwd);
		belle_sip_auth_event_set_client_certificates_chain(event,cert);
		belle_sip_auth_event_set_signing_key(event,key);
		belle_sip_message("process_auth_requested requested for DN [%s]"
							,belle_sip_auth_event_get_distinguished_name(event));
	}
}

static belle_sip_stack_t *stack=NULL;
static belle_http_provider_t *prov=NULL;

static int http_init(void){
	stack=belle_sip_stack_new(NULL);
	prov=belle_sip_stack_create_http_provider(stack,"0.0.0.0");
	if (root_ca_path != NULL) {
		belle_tls_verify_policy_t *policy=belle_tls_verify_policy_new();
		belle_tls_verify_policy_set_root_ca(policy,root_ca_path);
		belle_http_provider_set_tls_verify_policy(prov,policy);
	}
	return 0;
}

static int http_cleanup(void){
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
	return 0;
}

static void one_get(const char *url,http_counters_t* counters, int *counter){
	belle_http_request_listener_callbacks_t cbs={0};
	belle_http_request_listener_t *l;
	belle_generic_uri_t *uri;
	belle_http_request_t *req;
	
	uri=belle_generic_uri_parse(url);
	
	req=belle_http_request_create("GET",
							    uri,
							    belle_sip_header_create("User-Agent","belle-sip/"PACKAGE_VERSION),
							    NULL);
	cbs.process_response=process_response;
	cbs.process_io_error=process_io_error;
	cbs.process_auth_requested=process_auth_requested;
	l=belle_http_request_listener_create_from_callbacks(&cbs,counters);
	belle_http_provider_send_request(prov,req,l);
	wait_for(stack,counter,1,3000);
	
	belle_sip_object_unref(l);
}

static void one_http_get(void){
	http_counters_t counters={0};
	one_get("http://smtp.linphone.org",&counters,&counters.response_count);
	CU_ASSERT_TRUE(counters.response_count==1);
	CU_ASSERT_TRUE(counters.io_error_count==0);
	CU_ASSERT_EQUAL(counters.two_hundred,1);
}

static void http_get_io_error(void){
	http_counters_t counters={0};
	one_get("http://blablabla.cul",&counters,&counters.io_error_count);
	CU_ASSERT_TRUE(counters.response_count==0);
	CU_ASSERT_EQUAL(counters.io_error_count,1);
}

static void one_https_get(void){
	http_counters_t counters={0};
	one_get("https://smtp.linphone.org",&counters,&counters.response_count);
	CU_ASSERT_TRUE(counters.response_count==1);
	CU_ASSERT_TRUE(counters.io_error_count==0);
	CU_ASSERT_EQUAL(counters.two_hundred,1);
}

static void https_get_long_body(void){
	http_counters_t counters={0};
	one_get("https://www.linphone.org/eng/features/",&counters, &counters.response_count);
	CU_ASSERT_TRUE(counters.response_count==1);
	CU_ASSERT_TRUE(counters.io_error_count==0);
	CU_ASSERT_EQUAL(counters.two_hundred,1);
}

static void https_digest_get(void){
	http_counters_t counters={0};
	one_get("https://pauline:pouet@smtp.linphone.org/restricted",&counters,&counters.response_count);
	CU_ASSERT_TRUE(counters.response_count==1);
	CU_ASSERT_TRUE(counters.io_error_count==0);
	CU_ASSERT_EQUAL(counters.three_hundred,1);
}
#if 0
static void https_client_cert_connection(void){
	belle_tls_verify_policy_t *policy=belle_tls_verify_policy_new();
	http_counters_t counters={0};
	belle_tls_verify_policy_set_exceptions(policy,BELLE_TLS_VERIFY_ANY_REASON);/*ignore the server verification because we don't have a true certificate*/
	belle_http_provider_set_tls_verify_policy(prov,policy);
	one_get("https://sip2.linphone.org:5063",&counters);
	CU_ASSERT_EQUAL(counters.two_hundred,1);
	belle_tls_verify_policy_set_exceptions(policy,0);
	belle_sip_object_unref(policy);
}
#endif
test_t http_tests[] = {
	{ "One http GET", one_http_get },
	{ "One https GET", one_https_get },
	{ "http request with io error", http_get_io_error },
	{ "https GET with long body", https_get_long_body },
	{ "https digest GET", https_digest_get }/*, FIXME, need a server for testing
	{ "https with client certificate", https_client_cert_connection }*/
};

test_suite_t http_test_suite = {
	"http",
	http_init,
	http_cleanup,
	sizeof(http_tests) / sizeof(http_tests[0]),
	http_tests
};

