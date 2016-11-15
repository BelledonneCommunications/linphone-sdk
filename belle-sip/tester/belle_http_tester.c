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
#include <belle_sip_internal.h>

typedef struct http_counters{
	int response_headers_count;
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
	BC_ASSERT_PTR_NOT_NULL(event->response);
	if (event->response){
		int code=belle_http_response_get_status_code(event->response);
		belle_sip_body_handler_t *body=belle_sip_message_get_body_handler(BELLE_SIP_MESSAGE(event->response));
		if (code>=200 && code <300)
			counters->two_hundred++;
		else if (code>=300 && code <400)
			counters->three_hundred++;
		else if (code>=300 && code <400)
			counters->four_hundred++;
		BC_ASSERT_PTR_NOT_NULL(body);
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

static int http_before_all(void) {
	stack=belle_sip_stack_new(NULL);
	prov=belle_sip_stack_create_http_provider(stack,"0.0.0.0");
	if (belle_sip_tester_get_root_ca_path() != NULL) {
		belle_tls_crypto_config_t *crypto_config=belle_tls_crypto_config_new();
		belle_tls_crypto_config_set_root_ca(crypto_config,belle_sip_tester_get_root_ca_path());
		belle_http_provider_set_tls_crypto_config(prov,crypto_config);
		belle_sip_object_unref(crypto_config);

	}
	return 0;
}

static int http_after_all(void) {
	belle_sip_object_unref(prov);
	belle_sip_object_unref(stack);
	return 0;
}

static int url_supported(const char *url) {
	if (url && strstr(url,"https://")==url && !belle_sip_stack_tls_available(stack)) {
		belle_sip_error("No TLS support, test skipped.");
		return -1;
	}
	return 0;
}
static int one_get(const char *url,http_counters_t* counters, int *counter){
	if (url_supported(url)==-1) {
		return -1;
	} else {
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
		wait_for(stack,counter,1,10000);

		belle_sip_object_unref(l);
		return 0;
	}
}

static void one_http_get(void){
	http_counters_t counters={0};
	if (one_get("http://smtp.linphone.org",&counters,&counters.response_count) == 0) {
		BC_ASSERT_EQUAL(counters.response_count,1,int,"%d");
		BC_ASSERT_EQUAL(counters.io_error_count,0,int,"%d");
		BC_ASSERT_EQUAL(counters.two_hundred,1,int,"%d");
	}
}

static void http_get_empty_body(void){
	http_counters_t counters={0};
	if (one_get("http://smtp.linphone.org/marie_invalid",&counters,&counters.response_count) == 0) {
		BC_ASSERT_EQUAL(counters.response_count,1,int,"%d");
		BC_ASSERT_EQUAL(counters.io_error_count,0,int,"%d");
		BC_ASSERT_EQUAL(counters.two_hundred,1,int,"%d");
	}
}

static void http_get_io_error(void){
	http_counters_t counters={0};
	if (one_get("http://blablabla.fail",&counters,&counters.io_error_count) == 0) {
		BC_ASSERT_EQUAL(counters.response_count,0,int,"%d");
		BC_ASSERT_EQUAL(counters.io_error_count,1,int,"%d");
	}
	}

static void one_https_get(void){
	http_counters_t counters={0};
	if (one_get("https://smtp.linphone.org",&counters,&counters.response_count) == 0) {
		BC_ASSERT_EQUAL(counters.response_count, 1, int, "%d");
		BC_ASSERT_EQUAL(counters.io_error_count, 0, int, "%d");
		BC_ASSERT_EQUAL(counters.two_hundred,1,int,"%d");
	}
}

static void https_get_long_body(void){
	http_counters_t counters={0};
	if (one_get("https://smtp.linphone.org/linphone.html",&counters, &counters.response_count) == 0) {
		BC_ASSERT_EQUAL(counters.response_count, 1, int, "%d");
		BC_ASSERT_EQUAL(counters.io_error_count, 0, int, "%d");
		BC_ASSERT_EQUAL(counters.two_hundred,1,int,"%d");
	}
}

static void https_digest_get(void){
	http_counters_t counters={0};
	if (one_get("https://pauline:pouet@smtp.linphone.org/restricted",&counters,&counters.response_count) == 0) {
		BC_ASSERT_EQUAL(counters.response_count, 1, int, "%d");
		BC_ASSERT_EQUAL(counters.io_error_count, 0, int, "%d");
		BC_ASSERT_EQUAL(counters.three_hundred,1,int,"%d");
	}
}
#if 0
static void https_client_cert_connection(void){
	belle_tls_verify_policy_t *policy=belle_tls_verify_policy_new();
	http_counters_t counters={0};
	belle_tls_verify_policy_set_exceptions(policy,BELLE_TLS_VERIFY_ANY_REASON);/*ignore the server verification because we don't have a true certificate*/
	belle_http_provider_set_tls_verify_policy(prov,policy);
	if (one_get("https://sip2.linphone.org:5063",&counters) == 0) {
		BC_ASSERT_EQUAL(counters.two_hundred,1,int,"%d");
	}
	belle_tls_verify_policy_set_exceptions(policy,0);
	belle_sip_object_unref(policy);
}
#endif

static void on_progress(belle_sip_body_handler_t *bh, belle_sip_message_t *msg, void *data, size_t offset, size_t total){
	if (total!=0){
		double frac=100.0*(double)offset/(double)total;
		belle_sip_message("transfer %g %% done",frac);
	}else belle_sip_message("%i bytes transfered",(int)offset);
}

#define MULTIPART_BEGIN "somehash.jpg\r\n" \
			"--" MULTIPART_BOUNDARY "\r\n" \
			"Content-Disposition: form-data; name=\"userfile\"; filename=\"belle_http_sip_tester.jpg\"\r\n" \
			"Content-Type: application/octet-stream\r\n\r\n"
#define MULTIPART_END "\r\n--" MULTIPART_BOUNDARY "--\r\n"
const char *multipart_boudary=MULTIPART_BOUNDARY;

const int image_size=250000;

static int on_send_body(belle_sip_user_body_handler_t *bh, belle_sip_message_t *msg, void *data, size_t offset, uint8_t *buffer, size_t *size){
	size_t end_of_img=sizeof(MULTIPART_BEGIN)+image_size;
	if (offset==0){
		size_t partlen=sizeof(MULTIPART_BEGIN);
		BC_ASSERT_LOWER_STRICT((unsigned int)partlen,(unsigned int)*size,unsigned int,"%u");
		memcpy(buffer,MULTIPART_BEGIN,partlen);
		*size=partlen;
	}else if (offset<end_of_img){
		size_t i;
		size_t end=MIN(offset+*size, end_of_img);
		for(i=offset;i<end;++i){
			((char*)buffer)[i-offset]='a'+(i%26);
		}
		*size=i-offset;
	}else{
		*size=sizeof(MULTIPART_END);
		strncpy((char*)buffer,MULTIPART_END,*size);
	}
	return BELLE_SIP_CONTINUE;
}

static void https_post_long_body(void){
	belle_http_request_listener_callbacks_t cbs={0};
	belle_http_request_listener_t *l;
	belle_generic_uri_t *uri;
	belle_http_request_t *req;
	http_counters_t counters={0};
	belle_sip_user_body_handler_t *bh;
	char *content_type;
	const char *url="https://www.linphone.org:444/upload.php";
	if (url_supported(url)==-1) {
		return;
	}
	bh=belle_sip_user_body_handler_new(image_size+sizeof(MULTIPART_BEGIN)+sizeof(MULTIPART_END), on_progress, NULL, NULL, on_send_body, NULL, NULL);
	content_type=belle_sip_strdup_printf("multipart/form-data; boundary=%s",multipart_boudary);

	uri=belle_generic_uri_parse(url);

	req=belle_http_request_create("POST",
				uri,
				belle_sip_header_create("User-Agent","belle-sip/" PACKAGE_VERSION),
				belle_sip_header_create("Content-type",content_type),
				NULL);
	belle_sip_free(content_type);
	belle_sip_message_set_body_handler(BELLE_SIP_MESSAGE(req),BELLE_SIP_BODY_HANDLER(bh));
	cbs.process_response=process_response;
	cbs.process_io_error=process_io_error;
	cbs.process_auth_requested=process_auth_requested;
	l=belle_http_request_listener_create_from_callbacks(&cbs,&counters);
	belle_http_provider_send_request(prov,req,l);
	BC_ASSERT_TRUE(wait_for(stack,&counters.two_hundred,1,20000));

	belle_sip_object_unref(l);
}

static void on_recv_body(belle_sip_user_body_handler_t *bh, belle_sip_message_t *msg, void *data, size_t offset, uint8_t *buffer, size_t size){
	FILE *file=(FILE*)data;
	if (file)
		fwrite(buffer,1,size,file);
}

static void process_response_headers(void *data, const belle_http_response_event_t *event){
	http_counters_t *counters=(http_counters_t*)data;
	counters->response_headers_count++;
	BC_ASSERT_PTR_NOT_NULL(event->response);
	if (event->response){
		/*we are receiving a response, set a specific body handler to acquire the response.
		 * if not done, belle-sip will create a memory body handler, the default*/
		FILE *file=belle_sip_object_data_get(BELLE_SIP_OBJECT(event->request),"file");
		belle_sip_message_set_body_handler(
			(belle_sip_message_t*)event->response,
			(belle_sip_body_handler_t*)belle_sip_user_body_handler_new(0,on_progress, NULL,on_recv_body,NULL, NULL,file)
		);
	}
}

static void http_get_long_user_body(void){
	belle_http_request_listener_callbacks_t cbs={0};
	belle_http_request_listener_t *l;
	belle_generic_uri_t *uri;
	belle_http_request_t *req;
	http_counters_t counters={0};
	const char *url="http://download-mirror.savannah.gnu.org/releases/linphone/belle-sip/belle-sip-1.3.0.tar.gz";
	belle_sip_body_handler_t *bh;
	belle_http_response_t *resp;
	FILE *outfile=fopen("download.tar.gz","w");

	uri=belle_generic_uri_parse(url);

	req=belle_http_request_create("GET",
				uri,
				belle_sip_header_create("User-Agent","belle-sip/" PACKAGE_VERSION),
				NULL);
	cbs.process_response_headers=process_response_headers;
	cbs.process_response=process_response;
	cbs.process_io_error=process_io_error;
	cbs.process_auth_requested=process_auth_requested;
	l=belle_http_request_listener_create_from_callbacks(&cbs,&counters);
	belle_sip_object_ref(req);
	belle_sip_object_data_set(BELLE_SIP_OBJECT(req),"file",outfile,NULL);
	belle_http_provider_send_request(prov,req,l);
	BC_ASSERT_TRUE(wait_for(stack,&counters.two_hundred,1,20000));
	BC_ASSERT_EQUAL(counters.response_headers_count,1,int,"%d");
	resp=belle_http_request_get_response(req);
	BC_ASSERT_PTR_NOT_NULL(resp);
	if (resp){
		bh=belle_sip_message_get_body_handler((belle_sip_message_t*)resp);
		BC_ASSERT_GREATER_STRICT((unsigned int)belle_sip_body_handler_get_size(bh),0,unsigned int,"%u");
	}
	belle_sip_object_unref(req);
	belle_sip_object_unref(l);
	if (outfile) fclose(outfile);
}

extern const char *test_http_proxy_addr;
extern int test_http_proxy_port;

static void one_https_get_with_proxy(void){
	http_counters_t counters={0};
	belle_sip_stack_set_http_proxy_host(stack, test_http_proxy_addr);
	belle_sip_stack_set_http_proxy_port(stack, test_http_proxy_port);

	if (one_get("https://smtp.linphone.org",&counters,&counters.response_count) == 0) {
		BC_ASSERT_EQUAL(counters.response_count, 1, int, "%d");
		BC_ASSERT_EQUAL(counters.io_error_count, 0, int, "%d");
		BC_ASSERT_EQUAL(counters.two_hundred,1,int,"%d");
	}
	belle_sip_stack_set_http_proxy_host(stack, NULL);
	belle_sip_stack_set_http_proxy_port(stack, 0);

}


test_t http_tests[] = {
	TEST_NO_TAG("One http GET", one_http_get),
	TEST_NO_TAG("http GET of empty body", http_get_empty_body),
	TEST_NO_TAG("One https GET", one_https_get),
	TEST_NO_TAG("One https GET with http proxy", one_https_get_with_proxy),
	TEST_NO_TAG("http request with io error", http_get_io_error),
	TEST_NO_TAG("https GET with long body", https_get_long_body),
	TEST_NO_TAG("https digest GET", https_digest_get),/*, FIXME, need a server for testing
	TEST_NO_TAG("https with client certificate", https_client_cert_connection),*/
	TEST_NO_TAG("https POST with long body", https_post_long_body),
	TEST_NO_TAG("http GET with long user body", http_get_long_user_body)
};

test_suite_t http_test_suite = {"HTTP stack", http_before_all, http_after_all, NULL,
								NULL, sizeof(http_tests) / sizeof(http_tests[0]), http_tests};
