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

#ifndef _MSC_VER
#include <sys/time.h>
#endif


#define USERNAME "toto"
#define SIPDOMAIN "sip.linphone.org"
#define PASSWD "secret"


static char publish_body[]=
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<impp:presence xmlns:impp=\"urn:ietf:params:xml:ns:pidf entity=\"pres:someone@example.com\">\n"
	"	<impp:tuple id=\"sg89ae\">\n"
	"		<impp:status>\n"
	"			<impp:basic>open</impp:basic>\n"
	"		</impp:status>\n"
	"		<impp:contact priority=\"0.8\">tel:+09012345678</impp:contact>\n"
	"	</impp:tuple>\n"
	"</impp:presence>\n";

typedef enum auth_mode {
	none
	,digest
	,digest_auth
}auth_mode_t;

typedef struct _status {
	int twoHundredOk;
	int fourHundredOne;
	int fourHundredEightyOne;
	int refreshOk;
	int refreshKo;
	int dialogTerminated;
	int fiveHundredTree;
}status_t;

typedef struct endpoint {
	belle_sip_stack_t* stack;
	belle_sip_listener_callbacks_t* listener_callbacks;
	belle_sip_provider_t* provider;
	belle_sip_listening_point_t *lp;
	belle_sip_listener_t *listener;
	auth_mode_t auth;
	status_t stat;
	unsigned char expire_in_contact;
	char nonce[32];
	unsigned int nonce_count;
	const char* received;
	int rport;
	unsigned char unreconizable_contact;
	int connection_family;
	int register_count;
	int transiant_network_failure;
	belle_sip_refresher_t* refresher;
	int early_refresher;
	int number_of_body_found;
} endpoint_t;


static unsigned int  wait_for(belle_sip_stack_t*s1, belle_sip_stack_t*s2,int* counter,int value,int timeout) {
	int retry=0;
#define ITER 20
	while (*counter<value && retry++ <(timeout/ITER)) {
		if (s1) belle_sip_stack_sleep(s1,ITER/2);
		if (s2) belle_sip_stack_sleep(s2,ITER/2);
	}
	if(*counter<value) return FALSE;
	else return TRUE;
}

//static void process_dialog_terminated(void *obj, const belle_sip_dialog_terminated_event_t *event){
//	belle_sip_message("process_dialog_terminated called");
//}
//static void process_io_error(void *obj, const belle_sip_io_error_event_t *event){
//	belle_sip_warning("process_io_error");
//	/*belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));*/
//	/*BC_ASSERT(CU_FALSE);*/
//}

static void compute_response(const char* username
									,const char* realm
									,const char* passwd
									,const char* nonce
									,const char* method
									,const char* uri
									,char response[33] ) {
	char ha1[33],ha2[33];
	belle_sip_auth_helper_compute_ha1(username,realm,passwd,ha1);
	belle_sip_auth_helper_compute_ha2(method,uri,ha2);
	belle_sip_auth_helper_compute_response(ha1,nonce,ha2,response);
}

static void compute_response_auth_qop(const char* username
										,const char* realm
										,const char* passwd
										,const char* nonce
										,unsigned int nonce_count
										,const char* cnonce
										,const char* qop
										,const char* method
										,const char* uri
										,char response[33] ) {
	char ha1[33],ha2[33];
	belle_sip_auth_helper_compute_ha1(username,realm,passwd,ha1);
	belle_sip_auth_helper_compute_ha2(method,uri,ha2);
	belle_sip_auth_helper_compute_response_qop_auth(ha1, nonce,nonce_count, cnonce,qop,ha2,response);
}

#define MAX_NC_COUNT 5

static void server_process_request_event(void *obj, const belle_sip_request_event_t *event){
	endpoint_t* endpoint = (endpoint_t*)obj;
	belle_sip_server_transaction_t* server_transaction =belle_sip_provider_create_server_transaction(endpoint->provider,belle_sip_request_event_get_request(event));
	belle_sip_request_t* req = belle_sip_transaction_get_request(BELLE_SIP_TRANSACTION(server_transaction));
	belle_sip_response_t* resp;
	belle_sip_header_contact_t* contact;
	belle_sip_header_expires_t* expires;
	belle_sip_header_authorization_t* authorization;
	belle_sip_header_via_t* via;
	const char* raw_authenticate_digest = "WWW-Authenticate: Digest "
			"algorithm=MD5, realm=\"" SIPDOMAIN "\", opaque=\"1bc7f9097684320\"";

	belle_sip_header_www_authenticate_t* www_authenticate=NULL;
	const char* auth_uri;
	const char* qop;
	unsigned char auth_ok=0;
	char local_resp[33];

	belle_sip_message("caller_process_request_event received [%s] message",belle_sip_request_get_method(belle_sip_request_event_get_request(event)));

	switch (endpoint->auth) {
	case none: {
		auth_ok=1;
		break;
	}
	case digest_auth:
	case digest: {
		if ((authorization=belle_sip_message_get_header_by_type(req,belle_sip_header_authorization_t)) != NULL){
			qop=belle_sip_header_authorization_get_qop(authorization);

			if (qop && strcmp(qop,"auth")==0) {
				compute_response_auth_qop(	belle_sip_header_authorization_get_username(authorization)
											,belle_sip_header_authorization_get_realm(authorization)
											,PASSWD
											,endpoint->nonce
											,endpoint->nonce_count
											,belle_sip_header_authorization_get_cnonce(authorization)
											,belle_sip_header_authorization_get_qop(authorization)
											,belle_sip_request_get_method(req)
											,auth_uri=belle_sip_uri_to_string(belle_sip_header_authorization_get_uri(authorization))
											,local_resp);
			} else {
				/*digest*/
				compute_response(belle_sip_header_authorization_get_username(authorization)
						,belle_sip_header_authorization_get_realm(authorization)
						,PASSWD
						,endpoint->nonce
						,belle_sip_request_get_method(req)
						,auth_uri=belle_sip_uri_to_string(belle_sip_header_authorization_get_uri(authorization))
						,local_resp);

			}
			belle_sip_free((void*)auth_uri);
			auth_ok=strcmp(belle_sip_header_authorization_get_response(authorization),local_resp)==0;
		}
		if (auth_ok && endpoint->nonce_count<MAX_NC_COUNT ) {/*revoke nonce after MAX_NC_COUNT uses*/
			if (endpoint->auth == digest ) {
				sprintf(endpoint->nonce,"%p",authorization); //*change the nonce for next auth*/
			} else {
				endpoint->nonce_count++;
			}
		} else {
			auth_ok=0;
			www_authenticate=belle_sip_header_www_authenticate_parse(raw_authenticate_digest);
			sprintf(endpoint->nonce,"%p",authorization); //*change the nonce for next auth*/
			belle_sip_header_www_authenticate_set_nonce(www_authenticate,endpoint->nonce);
			if (endpoint->auth == digest_auth) {
				belle_sip_header_www_authenticate_add_qop(www_authenticate,"auth");
				if (endpoint->nonce_count>=MAX_NC_COUNT) {
					belle_sip_header_www_authenticate_set_stale(www_authenticate,1);
				}
				endpoint->nonce_count=1;
			}
		}
	}
	break;
	default:
		break;
	}
	if (auth_ok) {
		resp=belle_sip_response_create_from_request(belle_sip_request_event_get_request(event),200);
		if (!endpoint->expire_in_contact) {
			belle_sip_message_add_header(BELLE_SIP_MESSAGE(resp),BELLE_SIP_HEADER(expires=belle_sip_message_get_header_by_type(req,belle_sip_header_expires_t)));
		}
		if (strcmp(belle_sip_request_get_method(req),"REGISTER")==0) {
			contact=belle_sip_message_get_header_by_type(req,belle_sip_header_contact_t);
		} else {
			contact=belle_sip_header_contact_new();
		}
		if(endpoint->unreconizable_contact) {
			/*put an unexpected address*/
			belle_sip_uri_set_host(belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(contact)),"nimportequoi.com");
		}
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(resp),BELLE_SIP_HEADER(contact));
		if (strcmp(belle_sip_request_get_method(req),"PUBLISH")==0) {

			belle_sip_header_t* sip_if_match=belle_sip_message_get_header(BELLE_SIP_MESSAGE(resp),"SIP-If-Match");
			if (sip_if_match) {
				BC_ASSERT_STRING_EQUAL(belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(sip_if_match)),"blablietag");
			}
			/*check for body*/
			BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_body(BELLE_SIP_MESSAGE(req)));
			if (belle_sip_message_get_body(BELLE_SIP_MESSAGE(req))) {
				BC_ASSERT_STRING_EQUAL(belle_sip_message_get_body(BELLE_SIP_MESSAGE(req)),publish_body);
			}
			BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header_by_type(req,belle_sip_header_content_type_t));
			BC_ASSERT_PTR_NOT_NULL(belle_sip_message_get_header_by_type(req,belle_sip_header_content_length_t));
			belle_sip_message_add_header(BELLE_SIP_MESSAGE(resp),belle_sip_header_create("SIP-ETag","blablietag"));
		}
		if (strcmp(belle_sip_request_get_method(req),"SUBSCRIBE")==0){
			if (belle_sip_request_event_get_dialog(event) == NULL){
				belle_sip_message("creating dialog for incoming SUBSCRIBE");
				belle_sip_provider_create_dialog(endpoint->provider, BELLE_SIP_TRANSACTION(server_transaction));
			}
		}
	} else {
		resp=belle_sip_response_create_from_request(belle_sip_request_event_get_request(event),401);
		if (www_authenticate)
			belle_sip_message_add_header(BELLE_SIP_MESSAGE(resp),BELLE_SIP_HEADER(www_authenticate));
	}
	if (endpoint->received) {
		via=belle_sip_message_get_header_by_type(req,belle_sip_header_via_t);
		belle_sip_header_via_set_received(via,endpoint->received);
	}
	if (belle_sip_message_get_body(BELLE_SIP_MESSAGE(req))) {
		endpoint->number_of_body_found++;
	}
	belle_sip_server_transaction_send_response(server_transaction,resp);
}

static void client_process_dialog_terminated(void *obj, const belle_sip_dialog_terminated_event_t *event){
	endpoint_t* endpoint = (endpoint_t*)obj;
	endpoint->stat.dialogTerminated++;
}

static void server_process_dialog_terminated(void *obj, const belle_sip_dialog_terminated_event_t *event){
	endpoint_t* endpoint = (endpoint_t*)obj;
	endpoint->stat.dialogTerminated++;
}

static void client_process_response_event(void *obj, const belle_sip_response_event_t *event){
	//belle_sip_client_transaction_t* client_transaction = belle_sip_response_event_get_client_transaction(event);
	endpoint_t* endpoint = (endpoint_t*)obj;
	int status = belle_sip_response_get_status_code(belle_sip_response_event_get_response(event));
	belle_sip_message("caller_process_response_event [%i]",status);
	switch (status) {
	case 200:
		endpoint->stat.twoHundredOk++;
		if (endpoint->connection_family!=AF_UNSPEC){
			const char *host;
			int family_found;
			belle_sip_header_contact_t *ct=belle_sip_message_get_header_by_type(
				(belle_sip_message_t*)belle_sip_response_event_get_response(event),belle_sip_header_contact_t);
			if (BC_ASSERT_PTR_NOT_NULL(ct)) {
				host=belle_sip_uri_get_host(belle_sip_header_address_get_uri((belle_sip_header_address_t*)ct));
				if (strchr(host,':')) family_found=AF_INET6;
				else family_found=AF_INET;
				BC_ASSERT_EQUAL(family_found,endpoint->connection_family,int,"%d");
			}
		}
		break;
	case 401:endpoint->stat.fourHundredOne++; break;
	default: break;
	}


}

//static void process_timeout(void *obj, const belle_sip_timeout_event_t *event){
//	belle_sip_message("process_timeout");
//}
//static void process_transaction_terminated(void *obj, const belle_sip_transaction_terminated_event_t *event){
//	belle_sip_message("process_transaction_terminated");
//}

static void client_process_auth_requested(void *obj, belle_sip_auth_event_t *event){
	BELLESIP_UNUSED(obj);
	belle_sip_message("process_auth_requested requested for [%s@%s]"
			,belle_sip_auth_event_get_username(event)
			,belle_sip_auth_event_get_realm(event));
	belle_sip_auth_event_set_passwd(event,PASSWD);
}

static void belle_sip_refresher_listener (belle_sip_refresher_t* refresher
		,void* user_pointer
		,unsigned int status_code
		,const char* reason_phrase, int will_retry) {
	endpoint_t* endpoint = (endpoint_t*)user_pointer;
	BELLESIP_UNUSED(refresher);
	belle_sip_message("belle_sip_refresher_listener [%i] reason [%s]",status_code,reason_phrase);
	if (status_code >=300)
		endpoint->stat.refreshKo++;
	switch (status_code) {
		case 200:endpoint->stat.refreshOk++; break;
		case 481:endpoint->stat.fourHundredEightyOne++;break;
		case 503:endpoint->stat.fiveHundredTree++;break;
		default:
			/*nop*/
			break;
	}
	if (endpoint->stat.refreshKo==1 && endpoint->transiant_network_failure) {
		belle_sip_stack_set_send_error(endpoint->stack,0);
	} else 	if (endpoint->stat.refreshOk==1
				&& endpoint->stat.refreshKo==0
				&& endpoint->transiant_network_failure) {
		/*generate a network failure*/
		belle_sip_refresher_set_retry_after(endpoint->refresher,100);
		belle_sip_stack_set_send_error(endpoint->stack,-1);
	}
}

static endpoint_t* create_endpoint(const char *ip, int port,const char* transport,belle_sip_listener_callbacks_t* listener_callbacks) {
	endpoint_t* endpoint = belle_sip_new0(endpoint_t);
	endpoint->stack=belle_sip_stack_new(NULL);
	endpoint->listener_callbacks=listener_callbacks;
	endpoint->lp=belle_sip_stack_create_listening_point(endpoint->stack,ip,port,transport);
	endpoint->connection_family=AF_INET;

	if (endpoint->lp) belle_sip_object_ref(endpoint->lp);

	endpoint->provider=belle_sip_stack_create_provider(endpoint->stack,endpoint->lp);
	belle_sip_provider_add_sip_listener(endpoint->provider,(endpoint->listener=belle_sip_listener_create_from_callbacks(endpoint->listener_callbacks,endpoint)));
	sprintf(endpoint->nonce,"%p",endpoint); /*initial nonce*/
	endpoint->nonce_count=1;
	endpoint->register_count=3;
	return endpoint;
}

static void destroy_endpoint(endpoint_t* endpoint) {
	belle_sip_object_unref(endpoint->lp);
	belle_sip_object_unref(endpoint->provider);
	belle_sip_object_unref(endpoint->stack);
	belle_sip_object_unref(endpoint->listener);
	belle_sip_free(endpoint);
}

static endpoint_t* create_udp_endpoint(int port,belle_sip_listener_callbacks_t* listener_callbacks) {
	endpoint_t *endpoint=create_endpoint("0.0.0.0",port,"udp",listener_callbacks);
	BC_ASSERT_PTR_NOT_NULL(endpoint->lp);
	return endpoint;
}


static void refresher_base_with_body(endpoint_t* client
										,endpoint_t *server
										, const char* method
										, belle_sip_header_content_type_t* content_type
										,const char* body) {
	belle_sip_request_t* req;
	belle_sip_client_transaction_t* trans;
	belle_sip_header_route_t* destination_route;
	belle_sip_refresher_t* refresher;
	const char* identity = "sip:" USERNAME "@" SIPDOMAIN ;
	const char* domain="sip:" SIPDOMAIN ;
	belle_sip_header_contact_t* contact=belle_sip_header_contact_new();
	belle_sip_uri_t *dest_uri;
	uint64_t begin;
	uint64_t end;
	if (client->expire_in_contact) belle_sip_header_contact_set_expires(contact,1);


	dest_uri=(belle_sip_uri_t*)belle_sip_object_clone((belle_sip_object_t*)belle_sip_listening_point_get_uri(server->lp));
	if (client->connection_family==AF_INET6)
		belle_sip_uri_set_host(dest_uri,"::1");
	else
		belle_sip_uri_set_host(dest_uri,"127.0.0.1");
	destination_route=belle_sip_header_route_create(belle_sip_header_address_create(NULL,dest_uri));


	req=belle_sip_request_create(
		                    belle_sip_uri_parse(domain),
		                    method,
		                    belle_sip_provider_create_call_id(client->provider),
		                    belle_sip_header_cseq_create(20,method),
		                    belle_sip_header_from_create2(identity,BELLE_SIP_RANDOM_TAG),
		                    belle_sip_header_to_create2(identity,NULL),
		                    belle_sip_header_via_new(),
		                    70);
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(contact));
	if (!client->expire_in_contact)
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_expires_create(1)));

	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(destination_route));
	if (content_type && body) {
		size_t body_lenth=strlen(body);
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(content_type));
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_content_length_create(body_lenth)));
		belle_sip_message_set_body(BELLE_SIP_MESSAGE(req),body,body_lenth);
	}
	trans=belle_sip_provider_create_client_transaction(client->provider,req);
	belle_sip_object_ref(trans);/*to avoid trans from being deleted before refresher can use it*/
	belle_sip_client_transaction_send_request(trans);
	if (client->early_refresher) {
		client->refresher= refresher = belle_sip_client_transaction_create_refresher(trans);
	} else {
		if (server->auth == none) {
			BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.twoHundredOk,1,1000));
		} else {
			BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.fourHundredOne,1,1000));
			/*update cseq*/
			req=belle_sip_client_transaction_create_authenticated_request(trans,NULL,NULL);
			belle_sip_object_unref(trans);
			trans=belle_sip_provider_create_client_transaction(client->provider,req);
			belle_sip_object_ref(trans);
			belle_sip_client_transaction_send_request(trans);
			BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.twoHundredOk,1,1000));
		}
		client->refresher= refresher = belle_sip_client_transaction_create_refresher(trans);
	}
	if (BC_ASSERT_PTR_NOT_NULL(refresher)) {
		belle_sip_object_unref(trans);
		belle_sip_refresher_set_listener(refresher,belle_sip_refresher_listener,client);

		begin = belle_sip_time_ms();
		BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.refreshOk,client->register_count+(client->early_refresher?1:0),client->register_count*1000 + 1000));
		end = belle_sip_time_ms();
		BC_ASSERT_GREATER((long double)(end-begin),client->register_count*1000*.9,long double,"%Lf"); /*because refresh is at 90% of expire*/
		BC_ASSERT_LOWER_STRICT(end-begin,(client->register_count*1000 + 2000),unsigned long long,"%llu");
		/*unregister twice to make sure refresh operation can be safely cascaded*/
		belle_sip_refresher_refresh(refresher,0);
		belle_sip_refresher_refresh(refresher,0);
		BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.refreshOk,client->register_count+1,1000));
		BC_ASSERT_EQUAL(client->stat.refreshOk,client->register_count+1,int,"%d");
		belle_sip_refresher_stop(refresher);
		belle_sip_object_unref(refresher);
	}
}
static void refresher_base(endpoint_t* client,endpoint_t *server, const char* method) {
	refresher_base_with_body(client,server,method,NULL,NULL);
}
static void register_base(endpoint_t* client,endpoint_t *server) {
	refresher_base(client,server,"REGISTER");
}
static void refresher_base_with_param_and_body(const char* method
												, unsigned char expire_in_contact
												, auth_mode_t auth_mode
												, int early_refresher
												, belle_sip_header_content_type_t* content_type
												,const char* body){
	belle_sip_listener_callbacks_t client_callbacks;
	belle_sip_listener_callbacks_t server_callbacks;
	endpoint_t* client,*server;
	memset(&client_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	memset(&server_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	client_callbacks.process_response_event=client_process_response_event;
	client_callbacks.process_auth_requested=client_process_auth_requested;
	server_callbacks.process_request_event=server_process_request_event;
	client = create_udp_endpoint(3452,&client_callbacks);
	server = create_udp_endpoint(6788,&server_callbacks);
	server->expire_in_contact=client->expire_in_contact=expire_in_contact;
	server->auth=auth_mode;
	client->early_refresher=early_refresher;
	refresher_base_with_body(client,server,method,content_type,body);
	destroy_endpoint(client);
	destroy_endpoint(server);
}
static void refresher_base_with_param(const char* method, unsigned char expire_in_contact,auth_mode_t auth_mode) {
	refresher_base_with_param_and_body(method,expire_in_contact,auth_mode,FALSE,NULL,NULL);

}
static void register_test_with_param(unsigned char expire_in_contact,auth_mode_t auth_mode) {
	refresher_base_with_param("REGISTER",expire_in_contact,auth_mode);
}
static char *list =	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
						"<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\"\n"
						"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
						"<list>\n"
							"\t<entry uri=\"sip:marie@sexample.com\" />\n"
							"\t<entry uri=\"sip:pauline@sexample.com\" />\n"
						"</list>\n"
					"</resource-lists>\n";


static void subscribe_base(int with_resource_lists) {
	belle_sip_listener_callbacks_t client_callbacks;
	belle_sip_listener_callbacks_t server_callbacks;
	belle_sip_request_t* req;
	belle_sip_client_transaction_t* trans;
	belle_sip_header_route_t* destination_route;
	const char* identity = "sip:" USERNAME "@" SIPDOMAIN ;
	const char* domain="sip:" SIPDOMAIN ;
	endpoint_t* client,*server;
	belle_sip_uri_t *dest_uri;
	belle_sip_refresher_t* refresher;
	belle_sip_header_contact_t* contact=belle_sip_header_contact_new();
	belle_sip_dialog_t * client_dialog;
	char *call_id = NULL;
	int dummy = 0;
	uint64_t begin;
	uint64_t end;
	memset(&client_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	memset(&server_callbacks,0,sizeof(belle_sip_listener_callbacks_t));

	client_callbacks.process_dialog_terminated=client_process_dialog_terminated;
	client_callbacks.process_response_event=client_process_response_event;
	client_callbacks.process_auth_requested=client_process_auth_requested;
	server_callbacks.process_request_event=server_process_request_event;
	server_callbacks.process_dialog_terminated=server_process_dialog_terminated;

	client = create_udp_endpoint(3452,&client_callbacks);
	server = create_udp_endpoint(6788,&server_callbacks);
	server->expire_in_contact=0;
	server->auth=digest_auth;

	dest_uri=(belle_sip_uri_t*)belle_sip_object_clone((belle_sip_object_t*)belle_sip_listening_point_get_uri(server->lp));
	belle_sip_uri_set_host(dest_uri,"127.0.0.1");
	destination_route=belle_sip_header_route_create(belle_sip_header_address_create(NULL,dest_uri));


	req=belle_sip_request_create(
		                    belle_sip_uri_parse(domain),
		                    "SUBSCRIBE",
		                    belle_sip_provider_create_call_id(client->provider),
		                    belle_sip_header_cseq_create(20,"SUBSCRIBE"),
		                    belle_sip_header_from_create2(identity,BELLE_SIP_RANDOM_TAG),
		                    belle_sip_header_to_create2(identity,NULL),
		                    belle_sip_header_via_new(),
		                    70);
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(contact));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_expires_create(1)));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_create("Event","Presence")));

	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(destination_route));
	if (with_resource_lists) {
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),BELLE_SIP_HEADER(belle_sip_header_content_type_create("application","resource-lists+xml")));
		belle_sip_message_set_body(BELLE_SIP_MESSAGE(req), list, strlen(list));
	}

	trans=belle_sip_provider_create_client_transaction(client->provider,req);
	belle_sip_object_ref(trans);/*to avoid trans from being deleted before refresher can use it*/
	belle_sip_client_transaction_send_request(trans);

	BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.fourHundredOne,1,1000));

	req=belle_sip_client_transaction_create_authenticated_request(trans,NULL,NULL);
	belle_sip_object_unref(trans);
	trans=belle_sip_provider_create_client_transaction(client->provider,req);
	belle_sip_object_ref(trans);
	belle_sip_client_transaction_send_request(trans);
	BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.twoHundredOk,1,1000));
	 /*maybe dialog should be automatically created*/
	BC_ASSERT_PTR_NOT_NULL(client_dialog = belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(trans)));
	call_id = belle_sip_strdup(belle_sip_header_call_id_get_call_id(belle_sip_dialog_get_call_id(client_dialog)));
	belle_sip_object_ref(client_dialog);

	refresher = belle_sip_client_transaction_create_refresher(trans);
	belle_sip_object_unref(trans);
	belle_sip_refresher_set_listener(refresher,belle_sip_refresher_listener,client);

	begin = belle_sip_time_ms();
	BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.refreshOk,3,4000));
	end = belle_sip_time_ms();
	BC_ASSERT_GREATER((long double)(end-begin),3000*.9,long double,"%Lf");
	BC_ASSERT_LOWER_STRICT(end-begin,5000,unsigned long long,"%llu");

	belle_sip_message("simulating dialog error and recovery");
	belle_sip_stack_set_send_error(client->stack, 1);
	BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.fourHundredEightyOne,1,4000));
	/*let the transaction timeout*/
	wait_for(server->stack,client->stack, &dummy, 1, 32000);
	belle_sip_stack_set_send_error(client->stack, 0);
	

	BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.refreshOk,4,4000));
	BC_ASSERT_EQUAL(client->stat.dialogTerminated, 0, int, "%i");

	/*make sure dialog has changed*/
	BC_ASSERT_PTR_NOT_EQUAL(client_dialog, belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(belle_sip_refresher_get_transaction(refresher))));
	BC_ASSERT_STRING_NOT_EQUAL(call_id, belle_sip_header_call_id_get_call_id(belle_sip_dialog_get_call_id(belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(belle_sip_refresher_get_transaction(refresher))))));
	belle_sip_free(call_id);
	belle_sip_object_unref(client_dialog);
	
	belle_sip_message("simulating dialog terminated server side and recovery");
	
	client_dialog = belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(belle_sip_refresher_get_transaction(refresher)));
	call_id = belle_sip_strdup(belle_sip_header_call_id_get_call_id(belle_sip_dialog_get_call_id(client_dialog)));
	belle_sip_object_ref(client_dialog);
	
	belle_sip_provider_enable_unconditional_answer(server->provider,TRUE);
	belle_sip_provider_set_unconditional_answer(server->provider,481);
	belle_sip_refresher_refresh(refresher, 10);
	
	BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.fourHundredEightyOne,2,4000));
	belle_sip_provider_enable_unconditional_answer(server->provider,FALSE);

	BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&client->stat.refreshOk,5,4000));
	BC_ASSERT_EQUAL(client->stat.dialogTerminated, 0, int, "%i");
	
	/*make sure dialog has changed*/
	BC_ASSERT_PTR_NOT_EQUAL(client_dialog, belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(belle_sip_refresher_get_transaction(refresher))));
	BC_ASSERT_STRING_NOT_EQUAL(call_id, belle_sip_header_call_id_get_call_id(belle_sip_dialog_get_call_id(belle_sip_transaction_get_dialog(BELLE_SIP_TRANSACTION(belle_sip_refresher_get_transaction(refresher))))));
	belle_sip_free(call_id);
	belle_sip_object_unref(client_dialog);
	belle_sip_refresher_refresh(refresher, 0);
	belle_sip_refresher_refresh(refresher, 0);

	belle_sip_refresher_stop(refresher);
	BC_ASSERT_TRUE(wait_for(server->stack,client->stack,&server->stat.dialogTerminated,3,4000));
	
	belle_sip_object_unref(refresher);

	if (with_resource_lists) {
		BC_ASSERT_EQUAL(server->number_of_body_found, (server->auth == none ?1:2), int, "%i");
	}

	
	destroy_endpoint(client);
	destroy_endpoint(server);
}

static void subscribe_test(void) {
	subscribe_base(FALSE);
}
static void subscribe_list_test(void) {
	subscribe_base(TRUE);
}

static void register_expires_header(void) {
	register_test_with_param(0,none);
}

static void register_expires_in_contact(void) {
	register_test_with_param(1,none);
}

static void register_expires_header_digest(void) {
	register_test_with_param(0,digest);
}

static void register_expires_in_contact_header_digest_auth(void) {
	register_test_with_param(1,digest_auth);
}

static void register_with_failure(void) {
	belle_sip_listener_callbacks_t client_callbacks;
	belle_sip_listener_callbacks_t server_callbacks;
	endpoint_t* client,*server;
	memset(&client_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	memset(&server_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	client_callbacks.process_response_event=client_process_response_event;
	client_callbacks.process_auth_requested=client_process_auth_requested;
	server_callbacks.process_request_event=server_process_request_event;
	client = create_udp_endpoint(3452,&client_callbacks);
	server = create_udp_endpoint(6788,&server_callbacks);
	server->expire_in_contact=1;
	server->auth=digest;
	client->transiant_network_failure=1;
	register_base(client,server);
	BC_ASSERT_EQUAL(client->stat.refreshKo,1,int,"%d");
	destroy_endpoint(client);
	destroy_endpoint(server);
}
static void register_with_unrecognizable_contact(void) {
	belle_sip_listener_callbacks_t client_callbacks;
	belle_sip_listener_callbacks_t server_callbacks;
	endpoint_t* client,*server;
	memset(&client_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	memset(&server_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	client_callbacks.process_response_event=client_process_response_event;
	client_callbacks.process_auth_requested=client_process_auth_requested;
	server_callbacks.process_request_event=server_process_request_event;
	client = create_udp_endpoint(3452,&client_callbacks);
	server = create_udp_endpoint(6788,&server_callbacks);
	server->expire_in_contact=1;
	server->unreconizable_contact=1;
	server->auth=digest;
	register_base(client,server);
	destroy_endpoint(client);
	destroy_endpoint(server);
}
static void register_early_refresher(void) {
	belle_sip_listener_callbacks_t client_callbacks;
	belle_sip_listener_callbacks_t server_callbacks;
	endpoint_t* client,*server;
	memset(&client_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	memset(&server_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	client_callbacks.process_response_event=client_process_response_event;
	client_callbacks.process_auth_requested=client_process_auth_requested;
	server_callbacks.process_request_event=server_process_request_event;
	client = create_udp_endpoint(3452,&client_callbacks);
	server = create_udp_endpoint(6788,&server_callbacks);
	server->expire_in_contact=1;
	server->auth=digest;
	client->early_refresher=1;
	register_base(client,server);
	destroy_endpoint(client);
	destroy_endpoint(server);
}

static int register_test_with_interfaces(const char *transport, const char *client_ip, const char *server_ip, int connection_family) {
	int ret=0;
	belle_sip_listener_callbacks_t client_callbacks;
	belle_sip_listener_callbacks_t server_callbacks;
	endpoint_t* client,*server;
	memset(&client_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	memset(&server_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	client_callbacks.process_response_event=client_process_response_event;
	client_callbacks.process_auth_requested=client_process_auth_requested;
	server_callbacks.process_request_event=server_process_request_event;
	client = create_endpoint(client_ip,3452,transport,&client_callbacks);
	client->connection_family=connection_family;
	client->register_count=1;

	server = create_endpoint(server_ip,6788,transport,&server_callbacks);
	server->expire_in_contact=client->expire_in_contact=0;
	server->auth=none;

	if (client->lp==NULL || server->lp==NULL){
		belle_sip_warning("Cannot check ipv6 because host has no ipv6 support.");
		ret=-1;
	}else register_base(client,server);
	destroy_endpoint(client);
	destroy_endpoint(server);
	return ret;
}

static int register_test_with_random_port(const char *transport, const char *client_ip, const char *server_ip, int connection_family) {
	int ret=0;
	belle_sip_listener_callbacks_t client_callbacks;
	belle_sip_listener_callbacks_t server_callbacks;
	endpoint_t* client,*server;
	memset(&client_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	memset(&server_callbacks,0,sizeof(belle_sip_listener_callbacks_t));
	client_callbacks.process_response_event=client_process_response_event;
	client_callbacks.process_auth_requested=client_process_auth_requested;
	server_callbacks.process_request_event=server_process_request_event;
	client = create_endpoint(client_ip,-1,transport,&client_callbacks);
	client->connection_family=connection_family;
	client->register_count=1;

	server = create_endpoint(server_ip,6788,transport,&server_callbacks);
	server->expire_in_contact=client->expire_in_contact=0;
	server->auth=none;

	if (client->lp==NULL || server->lp==NULL){
		belle_sip_warning("Cannot check ipv6 because host has no ipv6 support.");
		ret=-1;
	}else register_base(client,server);
	destroy_endpoint(client);
	destroy_endpoint(server);
	return ret;
}

static void register_test_ipv6_to_ipv4(void){
	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	register_test_with_interfaces("udp","::0","0.0.0.0",AF_INET);
}

static void register_test_ipv4_to_ipv6(void){
	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	register_test_with_interfaces("udp","0.0.0.0","::0",AF_INET);
}

static void register_test_ipv6_to_ipv6_with_ipv4(void){
	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	register_test_with_interfaces("udp","::0","::0",AF_INET);
}

static void register_test_ipv6_to_ipv6_with_ipv6(void){
	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	register_test_with_interfaces("udp","::0","::0",AF_INET6);
}

static void register_tcp_test_ipv6_to_ipv4(void){
	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	register_test_with_interfaces("tcp","::0","0.0.0.0",AF_INET);
}

static void register_tcp_test_ipv4_to_ipv6(void){
	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	register_test_with_interfaces("tcp","0.0.0.0","::0",AF_INET);
}

static void register_tcp_test_ipv6_to_ipv6_with_ipv4(void){
	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	register_test_with_interfaces("tcp","::0","::0",AF_INET);
}

static void register_tcp_test_ipv6_to_ipv6_with_ipv6(void){
	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	register_test_with_interfaces("tcp","::0","::0",AF_INET6);
}

static void register_udp_test_ipv4_random_port(void){
	register_test_with_random_port("udp","0.0.0.0","0.0.0.0",AF_INET);
}

static void register_udp_test_ipv6_random_port(void){
	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	register_test_with_random_port("udp","::0","0.0.0.0",AF_INET);
}

static void register_tcp_test_ipv4_random_port(void){
	register_test_with_random_port("tcp","0.0.0.0","0.0.0.0",AF_INET);
}

static void register_tcp_test_ipv6_random_port(void){
	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	register_test_with_random_port("tcp","::0","0.0.0.0",AF_INET);
}

static void simple_publish(void) {
	belle_sip_header_content_type_t* content_type=belle_sip_header_content_type_create("application","pidf+xml");
	refresher_base_with_param_and_body("PUBLISH",FALSE,TRUE,FALSE, content_type,publish_body);

}
static void simple_publish_with_early_refresher(void) {
	belle_sip_header_content_type_t* content_type=belle_sip_header_content_type_create("application","pidf+xml");
	refresher_base_with_param_and_body("PUBLISH",FALSE,TRUE,TRUE, content_type,publish_body);

}

test_t refresher_tests[] = {
	TEST_NO_TAG("REGISTER Expires header", register_expires_header),
	TEST_NO_TAG("REGISTER Expires in Contact", register_expires_in_contact),
	TEST_NO_TAG("REGISTER Expires header digest", register_expires_header_digest),
	TEST_NO_TAG("REGISTER Expires in Contact digest auth", register_expires_in_contact_header_digest_auth),
	TEST_NO_TAG("REGISTER with failure", register_with_failure),
	TEST_NO_TAG("REGISTER with early refresher",register_early_refresher),
	TEST_NO_TAG("SUBSCRIBE", subscribe_test),
	TEST_NO_TAG("SUBSCRIBE of list" , subscribe_list_test),
	TEST_NO_TAG("PUBLISH", simple_publish),
	TEST_NO_TAG("PUBLISH with early refresher", simple_publish_with_early_refresher),
	TEST_NO_TAG("REGISTER with unrecognizable Contact", register_with_unrecognizable_contact),
	TEST_NO_TAG("REGISTER UDP from ipv6 to ipv4", register_test_ipv6_to_ipv4),
	TEST_NO_TAG("REGISTER UDP from ipv4 to ipv6", register_test_ipv4_to_ipv6),
	TEST_NO_TAG("REGISTER UDP from ipv6 to ipv6 with ipv4", register_test_ipv6_to_ipv6_with_ipv4),
	TEST_NO_TAG("REGISTER UDP from ipv6 to ipv6 with ipv6", register_test_ipv6_to_ipv6_with_ipv6),
	TEST_NO_TAG("REGISTER TCP from ipv6 to ipv4", register_tcp_test_ipv6_to_ipv4),
	TEST_NO_TAG("REGISTER TCP from ipv4 to ipv6", register_tcp_test_ipv4_to_ipv6),
	TEST_NO_TAG("REGISTER TCP from ipv6 to ipv6 with ipv4", register_tcp_test_ipv6_to_ipv6_with_ipv4),
	TEST_NO_TAG("REGISTER TCP from ipv6 to ipv6 with ipv6", register_tcp_test_ipv6_to_ipv6_with_ipv6),
	TEST_NO_TAG("REGISTER UDP from random port using AF_INET", register_udp_test_ipv4_random_port),
	TEST_NO_TAG("REGISTER UDP from random port using AF_INET6", register_udp_test_ipv6_random_port),
	TEST_NO_TAG("REGISTER TCP from random port using AF_INET", register_tcp_test_ipv4_random_port),
	TEST_NO_TAG("REGISTER TCP from random port using AF_INET6", register_tcp_test_ipv6_random_port),
};

test_suite_t refresher_test_suite = {"Refresher", NULL, NULL, belle_sip_tester_before_each, belle_sip_tester_after_each,
									 sizeof(refresher_tests) / sizeof(refresher_tests[0]), refresher_tests};
