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
#include "belle_sip_messageParser.h"
#include "belle_sip_messageLexer.h"
#include "belle_sip_internal.h"


typedef struct _headers_container {
	char* name;
	belle_sip_list_t* header_list;
} headers_container_t;

static headers_container_t* belle_sip_message_headers_container_new(const char* name) {
	headers_container_t* headers_container = belle_sip_new0(headers_container_t);
	headers_container->name= belle_sip_strdup(name);
	return  NULL; /*FIXME*/
}

static void belle_sip_headers_container_delete(headers_container_t *obj){
	belle_sip_free(obj->name);
	belle_sip_free(obj);
}

struct _belle_sip_message {
	belle_sip_object_t base;
	belle_sip_list_t* header_list;
};

static void belle_sip_message_destroy(belle_sip_message_t *msg){
	belle_sip_list_for_each (msg->header_list,(void (*)(void*))belle_sip_headers_container_delete);
	belle_sip_list_free(msg->header_list);
}

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_message_t,belle_sip_object_t,belle_sip_message_destroy,NULL);

BELLE_SIP_PARSE(message)

static int belle_sip_headers_container_comp_func(const headers_container_t *a, const char*b) {
	return strcmp(a->name,b);
}

static void belle_sip_message_init(belle_sip_message_t *message){
	
}

headers_container_t* belle_sip_headers_container_get(belle_sip_message_t* message,const char* header_name) {
	belle_sip_list_t *  result = belle_sip_list_find_custom(	message->header_list
															, (belle_sip_compare_func)belle_sip_headers_container_comp_func
															, header_name);
	return result?(headers_container_t*)(result->data):NULL;
}
void belle_sip_message_add_header(belle_sip_message_t *message,belle_sip_header_t* header) {
	// first check if already exist
	headers_container_t* headers_container = belle_sip_headers_container_get(message,belle_sip_header_get_name(header));
	if (headers_container == NULL) {
		headers_container = belle_sip_message_headers_container_new(belle_sip_header_get_name(header));
		belle_sip_list_append(message->header_list,headers_container);
	}
	belle_sip_list_append(headers_container->header_list,header);

}
const belle_sip_list_t* belle_sip_message_get_headers(belle_sip_message_t *message,const char* header_name) {
	headers_container_t* headers_container = belle_sip_headers_container_get(message,header_name);
	return headers_container ? headers_container->header_list:NULL;
}
struct _belle_sip_request {
	belle_sip_message_t message;
	const char* method;
};

static void belle_sip_request_destroy(belle_sip_request_t* request) {
	if (request->method) belle_sip_free((void*)(request->method));
}

static void belle_sip_request_clone(belle_sip_request_t *request, const belle_sip_request_t *orig){
		if (orig->method) request->method=belle_sip_strdup(orig->method);
}

BELLE_SIP_NEW(request,message)
BELLE_SIP_PARSE(request)
GET_SET_STRING(belle_sip_request,method);

void belle_sip_request_set_uri(belle_sip_request_t* request,belle_sip_uri_t* uri) {

}

belle_sip_uri_t * belle_sip_request_get_uri(belle_sip_request_t *request){
	return NULL;
}

int belle_sip_message_is_request(belle_sip_message_t *msg){
	return 0;
}

int belle_sip_message_is_response(belle_sip_message_t *msg){
	return 0;
}

belle_sip_header_t *belle_sip_message_get_header(belle_sip_message_t *msg, const char *header_name){
	const belle_sip_list_t *l=belle_sip_message_get_headers(msg,header_name);
	if (l!=NULL)
		return (belle_sip_header_t*)l->data;
	return NULL;
}


char *belle_sip_message_to_string(belle_sip_message_t *msg){
	return NULL;
}

void belle_sip_response_get_return_hop(belle_sip_response_t *msg, belle_sip_hop_t *hop){
	belle_sip_header_via_t *via=BELLE_SIP_HEADER_VIA(belle_sip_message_get_header(BELLE_SIP_MESSAGE(msg),"via"));
	hop->transport=belle_sip_header_via_get_protocol(via);
	hop->host=belle_sip_header_via_get_received(via);
	if (hop->host==NULL)
		hop->host=belle_sip_header_via_get_host(via);
	hop->port=belle_sip_header_via_get_rport(via);
	if (hop->port==-1)
		hop->port=belle_sip_header_via_get_listening_port(via);
}

int belle_sip_response_get_status_code(const belle_sip_response_t *response){
	return 0;
}

