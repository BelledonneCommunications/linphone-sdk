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
	return strcasecmp(a->name,b);
}

static void belle_sip_message_init(belle_sip_message_t *message){
	
}

headers_container_t* belle_sip_headers_container_get(belle_sip_message_t* message,const char* header_name) {
	belle_sip_list_t *  result = belle_sip_list_find_custom(	message->header_list
															, (belle_sip_compare_func)belle_sip_headers_container_comp_func
															, header_name);
	return result?(headers_container_t*)(result->data):NULL;
}

headers_container_t * get_or_create_container(belle_sip_message_t *message, const char *header_name){
	// first check if already exist
	headers_container_t* headers_container = belle_sip_headers_container_get(message,header_name);
	if (headers_container == NULL) {
		headers_container = belle_sip_message_headers_container_new(header_name);
		belle_sip_list_append(message->header_list,headers_container);
	}
	return headers_container;
}

void belle_sip_message_add_header(belle_sip_message_t *message,belle_sip_header_t* header) {
	headers_container_t *headers_container=get_or_create_container(message,belle_sip_header_get_name(header));
	belle_sip_list_append(headers_container->header_list,belle_sip_object_ref(header));
}

void belle_sip_message_add_headers(belle_sip_message_t *message, const belle_sip_list_t *header_list){
	const char *hname=belle_sip_header_get_name(BELLE_SIP_HEADER((header_list->data)));
	headers_container_t *headers_container=get_or_create_container(message,hname);
	for(;header_list!=NULL;header_list=header_list->next){
		belle_sip_header_t *h=BELLE_SIP_HEADER(header_list->data);
		if (strcmp(belle_sip_header_get_name(h),hname)!=0){
			belle_sip_fatal("Bad use of belle_sip_message_add_headers(): all headers of the list must be of the same type.");
			return ;
		}
		belle_sip_list_append(headers_container->header_list,belle_sip_object_ref(h));
	}
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

struct _belle_sip_response{
	belle_sip_message_t base;
	char *sip_version;
	int status_code;
	char *reason_phrase;
};

typedef struct code_phrase{
	int code;
	const char *phrase;
} code_phrase_t;

static code_phrase_t well_known_codes[]={
	{	100		,		"Trying"	},
	{	101		,		"Dialog establishment"	},
	{	180		,		"Ringing"				},
	{	181		,		"Call is being forwarded"	},
	{	182		,		"Queued"	},
	{	183		,		"Session progress"	},
	{	200		,		"Ok"				},
	{	202		,		"Accepted"	},
	{	300		,		"Multiple choices"	},
	{	301		,		"Moved permanently"	},
	{	302		,		"Moved temporarily"	},
	{	305		,		"Use proxy"	},
	{	380		,		"Alternate contact"	},
	{	400		,		"Bad request"		},
	{	401		,		"Unauthorized"		},
	{	402		,		"Payment required"	},
	{	403		,		"Forbidden"	},
	{	404		,		"Not found"	},
	{	405		,		"Method not allowed"	},
	{	406		,		"Not acceptable"	},
	{	407		,		"Proxy authentication required"	},
	{	408		,		"Request timeout"	},
	{	410		,		"Gone"	},
	{	413		,		"Request entity too large"	},
	{	414		,		"Request-URI too long"	},
	{	415		,		"Unsupported media type"	},
	{	416		,		"Unsupported URI scheme"	},
	{	420		,		"Bad extension"	},
	{	421		,		"Extension required"	},
	{	423		,		"Interval too brief"	},
	{	480		,		"Temporarily unavailable"	},
	{	481		,		"Call/transaction does not exist"	},
	{	482		,		"Loop detected"	},
	{	483		,		"Too many hops"	},
	{	484		,		"Address incomplete"	},
	{	485		,		"Ambiguous"	},
	{	486		,		"Busy here"	},
	{	487		,		"Request terminated"	},
	{	488		,		"Not acceptable here"	},
	{	491		,		"Request pending"	},
	{	493		,		"Undecipherable"	},
	{	500		,		"Server internal error"	},
	{	501		,		"Not implemented"	},
	{	502		,		"Bad gateway"	},
	{	503		,		"Service unavailable"	},
	{	504		,		"Server time-out"	},
	{	505		,		"Version not supported"	},
	{	513		,		"Message too large"	},
	{	600		,		"Busy everywhere"	},
	{	603		,		"Decline"	},
	{	604		,		"Does not exist anywhere"	},
	{	606		,		"Not acceptable"	},
	{	0			,		NULL	}
};

const char *belle_sip_get_well_known_reason_phrase(int status_code){
	int i;
	for(i=0;well_known_codes[i].code!=0;++i){
		if (well_known_codes[i].code==status_code)
			return well_known_codes[i].phrase;
	}
	return "Unknown reason";
}

void belle_sip_response_destroy(belle_sip_response_t *resp){
	if (resp->sip_version) belle_sip_free(resp->sip_version);
	if (resp->reason_phrase) belle_sip_free(resp->reason_phrase);
}

static void belle_sip_response_clone(belle_sip_response_t *resp, const belle_sip_response_t *orig){
	if (orig->sip_version) resp->sip_version=belle_sip_strdup(orig->sip_version);
	if (orig->reason_phrase) resp->reason_phrase=belle_sip_strdup(orig->reason_phrase);
}

BELLE_SIP_NEW(response,message);

static void belle_sip_response_init_default(belle_sip_response_t *resp, int status_code, const char *phrase){
	resp->status_code=status_code;
	resp->sip_version=belle_sip_strdup("SIP/2.0");
	if (phrase==NULL) phrase=belle_sip_get_well_known_reason_phrase(status_code);
	resp->reason_phrase=belle_sip_strdup(phrase);
}

belle_sip_response_t *belle_sip_response_new_from_request(belle_sip_request_t *req, int status_code){
	belle_sip_response_t *resp=belle_sip_response_new();
	belle_sip_header_t *h;
	belle_sip_response_init_default(resp,status_code,NULL);
	belle_sip_message_add_headers((belle_sip_message_t*)resp,belle_sip_message_get_headers ((belle_sip_message_t*)req,"via"));
	belle_sip_message_add_header((belle_sip_message_t*)resp,belle_sip_message_get_header((belle_sip_message_t*)req,"from"));
	belle_sip_message_add_header((belle_sip_message_t*)resp,belle_sip_message_get_header((belle_sip_message_t*)req,"to"));
	belle_sip_message_add_header((belle_sip_message_t*)resp,belle_sip_message_get_header((belle_sip_message_t*)req,"cseq"));
	h=belle_sip_message_get_header((belle_sip_message_t*)req,"call-id");
	if (h) belle_sip_message_add_header((belle_sip_message_t*)resp,h);
	
	return resp;
}

int belle_sip_response_get_status_code(const belle_sip_response_t *response){
	return response->status_code;
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
