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
	return  headers_container;
}

static void belle_sip_headers_container_delete(headers_container_t *obj){
	belle_sip_free(obj->name);
	belle_sip_list_free_with_data(obj->header_list,(void (*)(void*))belle_sip_object_unref);
	belle_sip_free(obj);
}

struct _belle_sip_message {
	belle_sip_object_t base;
	belle_sip_list_t* header_list;
	char* body;
	unsigned int body_length;
};

static void belle_sip_message_destroy(belle_sip_message_t *msg){
	belle_sip_list_for_each (msg->header_list,(void (*)(void*))belle_sip_headers_container_delete);
	belle_sip_list_free(msg->header_list);
}

/*very sub-optimal clone method */
static void belle_sip_message_clone(belle_sip_message_t *obj, const belle_sip_message_t *orig){
	headers_container_t *c;
	const belle_sip_list_t *l;
	for(l=orig->header_list;l!=NULL;l=l->next){
		c=(headers_container_t*)l->data;
		if (c->header_list){
			belle_sip_list_t * ll=belle_sip_list_copy_with_data(c->header_list,(void *(*)(void*))belle_sip_object_clone);
			belle_sip_message_add_headers(obj,ll);
			belle_sip_list_free(ll);
		}
	}
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_message_t);

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_message_t,belle_sip_object_t,belle_sip_message_destroy,belle_sip_message_clone,NULL,FALSE);

belle_sip_message_t* belle_sip_message_parse (const char* value) {
	size_t message_length;
	return belle_sip_message_parse_raw(value,strlen(value),&message_length);
}

belle_sip_message_t* belle_sip_message_parse_raw (const char* buff, size_t buff_length,size_t* message_length ) { \
	pANTLR3_INPUT_STREAM           input;
	pbelle_sip_messageLexer               lex;
	pANTLR3_COMMON_TOKEN_STREAM    tokens;
	pbelle_sip_messageParser              parser;
	input  = antlr3NewAsciiStringCopyStream	(
			(pANTLR3_UINT8)buff,
			(ANTLR3_UINT32)buff_length,
			((void *)0));
	lex    = belle_sip_messageLexerNew                (input);
	tokens = antlr3CommonTokenStreamSourceNew  (1025, lex->pLexer->rec->state->tokSource);
	parser = belle_sip_messageParserNew               (tokens);
	belle_sip_message_t* l_parsed_object = parser->message_raw(parser,message_length);
/*	if (*message_length < buff_length) {*/
		/*there is a body*/
/*		l_parsed_object->body_length=buff_length-*message_length;
		l_parsed_object->body = belle_sip_malloc(l_parsed_object->body_length+1);
		memcpy(l_parsed_object->body,buff+*message_length,l_parsed_object->body_length);
		l_parsed_object->body[l_parsed_object->body_length]='\0';
	}*/
	parser ->free(parser);
	tokens ->free(tokens);
	lex    ->free(lex);
	input  ->close(input);
	return l_parsed_object;
}

static int belle_sip_headers_container_comp_func(const headers_container_t *a, const char*b) {
	return strcasecmp(a->name,b);
}

static void belle_sip_message_init(belle_sip_message_t *message){
	
}

headers_container_t* belle_sip_headers_container_get(const belle_sip_message_t* message,const char* header_name) {
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
		message->header_list=belle_sip_list_append(message->header_list,headers_container);
	}
	return headers_container;
}

void belle_sip_message_add_header(belle_sip_message_t *message,belle_sip_header_t* header) {
	headers_container_t *headers_container=get_or_create_container(message,belle_sip_header_get_name(header));
	headers_container->header_list=belle_sip_list_append(headers_container->header_list,belle_sip_object_ref(header));
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
		headers_container->header_list=belle_sip_list_append(headers_container->header_list,belle_sip_object_ref(h));
	}
}

void belle_sip_message_set_header(belle_sip_message_t *msg, belle_sip_header_t* header){
	headers_container_t *headers_container=get_or_create_container(msg,belle_sip_header_get_name(header));
	belle_sip_object_ref(header);
	headers_container->header_list=belle_sip_list_free_with_data(headers_container->header_list,belle_sip_object_unref);
	headers_container->header_list=belle_sip_list_append(headers_container->header_list,header);
}

const belle_sip_list_t* belle_sip_message_get_headers(const belle_sip_message_t *message,const char* header_name) {
	headers_container_t* headers_container = belle_sip_headers_container_get(message,header_name);
	return headers_container ? headers_container->header_list:NULL;
}

void belle_sip_message_remove_first(belle_sip_message_t *msg, const char *header_name){
	headers_container_t* headers_container = belle_sip_headers_container_get(msg,header_name);
	if (headers_container && headers_container->header_list){
		belle_sip_list_t *to_be_removed=headers_container->header_list;
		headers_container->header_list=belle_sip_list_remove_link(headers_container->header_list,to_be_removed);
		belle_sip_list_free_with_data(to_be_removed,belle_sip_object_unref);
	}
}

void belle_sip_message_remove_last(belle_sip_message_t *msg, const char *header_name){
	headers_container_t* headers_container = belle_sip_headers_container_get(msg,header_name);
	if (headers_container && headers_container->header_list){
		belle_sip_list_t *to_be_removed=belle_sip_list_last_elem(headers_container->header_list);
		headers_container->header_list=belle_sip_list_remove_link(headers_container->header_list,to_be_removed);
		belle_sip_list_free_with_data(to_be_removed,belle_sip_object_unref);
	}
}

void belle_sip_message_remove_header(belle_sip_message_t *msg, const char *header_name){
	headers_container_t* headers_container = belle_sip_headers_container_get(msg,header_name);
	if (headers_container){
		belle_sip_headers_container_delete(headers_container);
		belle_sip_list_remove(msg->header_list,headers_container);
	}
}


/*
int belle_sip_message_named_headers_marshal(belle_sip_message_t *message, const char* header_name, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	belle_sip_list_t* header_list = belle_sip_message_get_headers(message,header_name);
	if (!header_list) {
		belle_sip_error("headers [%s] not found",header_name);
		return 0;
	}
	for(;header_list!=NULL;header_list=header_list->next){
		belle_sip_header_t *h=BELLE_SIP_HEADER(header_list->data);
		current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(h),buff,current_offset,buff_size);
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","\r\n");
	}
	return current_offset-offset;
}

#define MARSHAL_AND_CHECK_HEADER(header) \
		if (current_offset == (current_offset+=(header))) {\
			belle_sip_error("missing mandatory header");\
			return current_offset;\
		} else {\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","\r\n");\
		}
*/
int belle_sip_headers_marshal(belle_sip_message_t *message, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	belle_sip_list_t* headers_list;
	belle_sip_list_t* header_list;
	for(headers_list=message->header_list;headers_list!=NULL;headers_list=headers_list->next){
		for(header_list=((headers_container_t*)(headers_list->data))->header_list
				;header_list!=NULL
				;header_list=header_list->next)	{
			belle_sip_header_t *h=BELLE_SIP_HEADER(header_list->data);
			current_offset+=belle_sip_object_marshal(BELLE_SIP_OBJECT(h),buff,current_offset,buff_size);
			current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","\r\n");
		}
	}
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","\r\n");
	return current_offset-offset;
}

struct _belle_sip_request {
	belle_sip_message_t message;
	const char* method;
	belle_sip_uri_t* uri;
};

static void belle_sip_request_destroy(belle_sip_request_t* request) {
	if (request->method) belle_sip_free((void*)(request->method));
	if (request->uri) belle_sip_object_unref(request->uri);
}

static void belle_sip_request_init(belle_sip_request_t *message){	
}

static void belle_sip_request_clone(belle_sip_request_t *request, const belle_sip_request_t *orig){
	if (orig->method) request->method=belle_sip_strdup(orig->method);
	if (orig->uri) request->uri=(belle_sip_uri_t*)belle_sip_object_clone((belle_sip_object_t*)orig->uri);
}
int belle_sip_request_marshal(belle_sip_request_t* request, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s ",belle_sip_request_get_method(request));
	current_offset+=belle_sip_uri_marshal(belle_sip_request_get_uri(request),buff,current_offset,buff_size);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset," %s","SIP/2.0\r\n");
	current_offset+=belle_sip_headers_marshal(BELLE_SIP_MESSAGE(request),buff,current_offset,buff_size);
	if (BELLE_SIP_MESSAGE(request)->body) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset, "%s",BELLE_SIP_MESSAGE(request)->body);
	}
	return current_offset-offset;
}

BELLE_SIP_NEW(request,message)
BELLE_SIP_PARSE(request)
GET_SET_STRING(belle_sip_request,method);

void belle_sip_request_set_uri(belle_sip_request_t* request,belle_sip_uri_t* uri) {
	if (request->uri) {
		belle_sip_object_unref(request->uri);
	}
	request->uri=BELLE_SIP_URI(belle_sip_object_ref(uri));
}

belle_sip_uri_t * belle_sip_request_get_uri(belle_sip_request_t *request){
	return request->uri;
}

int belle_sip_message_is_request(belle_sip_message_t *msg){
	return BELLE_SIP_IS_INSTANCE_OF(BELLE_SIP_OBJECT(msg),belle_sip_request_t);
}

int belle_sip_message_is_response(const belle_sip_message_t *msg){
	return BELLE_SIP_IS_INSTANCE_OF(BELLE_SIP_OBJECT(msg),belle_sip_response_t);
}

belle_sip_header_t *belle_sip_message_get_header(const belle_sip_message_t *msg, const char *header_name){
	const belle_sip_list_t *l=belle_sip_message_get_headers(msg,header_name);
	if (l!=NULL)
		return (belle_sip_header_t*)l->data;
	return NULL;
}

char *belle_sip_message_to_string(belle_sip_message_t *msg){
	return belle_sip_object_to_string(BELLE_SIP_OBJECT(msg));
}

const char* belle_sip_message_get_body(belle_sip_message_t *msg) {
	return msg->body;
}

void belle_sip_message_set_body(belle_sip_message_t *msg,char* body,unsigned int size) {
	if (msg->body) {
		belle_sip_free((void*)body);
	}
	msg->body = belle_sip_malloc(size+1);
	memcpy(msg->body,body,size);
	msg->body[size]='\0';
}
void belle_sip_message_assign_body(belle_sip_message_t *msg, char* body) {
	if (msg->body) {
		belle_sip_free((void*)body);
	}
	msg->body = body;
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

static void belle_sip_response_init(belle_sip_response_t *resp){
}

static void belle_sip_response_clone(belle_sip_response_t *resp, const belle_sip_response_t *orig){
	if (orig->sip_version) resp->sip_version=belle_sip_strdup(orig->sip_version);
	if (orig->reason_phrase) resp->reason_phrase=belle_sip_strdup(orig->reason_phrase);
}
int belle_sip_response_marshal(belle_sip_response_t *resp, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"SIP/2.0 %i %s\r\n"
								,belle_sip_response_get_status_code(resp)
								,belle_sip_response_get_reason_phrase(resp));
	current_offset+=belle_sip_headers_marshal(BELLE_SIP_MESSAGE(resp),buff,current_offset,buff_size);
	if (BELLE_SIP_MESSAGE(resp)->body) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset, "%s",BELLE_SIP_MESSAGE(resp)->body);
	}
	return current_offset-offset;
}
BELLE_SIP_NEW(response,message);
BELLE_SIP_PARSE(response)
GET_SET_STRING(belle_sip_response,reason_phrase);
GET_SET_INT(belle_sip_response,status_code,int)

belle_sip_request_t* belle_sip_request_create(belle_sip_uri_t *requri, const char* method,
                                         belle_sip_header_call_id_t *callid,
                                         belle_sip_header_cseq_t * cseq,
                                         belle_sip_header_from_t *from,
                                         belle_sip_header_to_t *to,
                                         belle_sip_header_via_t *via,
                                         int max_forward /*FIXME*/)
{
	belle_sip_request_t *ret=belle_sip_request_new();
	belle_sip_header_max_forwards_t *mf=belle_sip_header_max_forwards_new();
	if (max_forward==0) max_forward=70;
	belle_sip_header_max_forwards_set_max_forwards(mf,max_forward);

	belle_sip_request_set_uri(ret,requri);
	belle_sip_request_set_method(ret,method);
	belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(via));
	belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(from));
	belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(to));
	belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(cseq));
	belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(callid));
	belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(mf));
	return ret;
}

static void belle_sip_response_init_default(belle_sip_response_t *resp, int status_code, const char *phrase){
	resp->status_code=status_code;
	resp->sip_version=belle_sip_strdup("SIP/2.0");
	if (phrase==NULL) phrase=belle_sip_get_well_known_reason_phrase(status_code);
	resp->reason_phrase=belle_sip_strdup(phrase);
}

belle_sip_response_t *belle_sip_response_create_from_request(belle_sip_request_t *req, int status_code){
	belle_sip_response_t *resp=belle_sip_response_new();
	belle_sip_header_t *h;
	belle_sip_header_to_t *to;
	belle_sip_response_init_default(resp,status_code,NULL);
	if (status_code==100){
		h=belle_sip_message_get_header((belle_sip_message_t*)req,"timestamp");
		belle_sip_message_add_header((belle_sip_message_t*)resp,h);
	}
	belle_sip_message_add_headers((belle_sip_message_t*)resp,belle_sip_message_get_headers ((belle_sip_message_t*)req,"via"));
	belle_sip_message_add_header((belle_sip_message_t*)resp,belle_sip_message_get_header((belle_sip_message_t*)req,"from"));
	h=belle_sip_message_get_header((belle_sip_message_t*)req,"to");
	if (status_code!=100){
		//so that to tag can be added
		to=(belle_sip_header_to_t*)belle_sip_object_clone((belle_sip_object_t*)h);
	}else{
		to=(belle_sip_header_to_t*)h;
	}
	belle_sip_message_add_header((belle_sip_message_t*)req,(belle_sip_header_t*)to);
	h=belle_sip_message_get_header((belle_sip_message_t*)req,"call-id");
	belle_sip_message_add_header((belle_sip_message_t*)resp,h);
	belle_sip_message_add_header((belle_sip_message_t*)resp,belle_sip_message_get_header((belle_sip_message_t*)req,"cseq"));
	return resp;
}

void belle_sip_response_get_return_hop(belle_sip_response_t *msg, belle_sip_hop_t *hop){
	belle_sip_header_via_t *via=BELLE_SIP_HEADER_VIA(belle_sip_message_get_header(BELLE_SIP_MESSAGE(msg),"via"));
	const char *host;
	hop->transport=belle_sip_strdup(belle_sip_header_via_get_protocol(via));
	host=belle_sip_header_via_get_received(via);
	if (host==NULL)
		host=belle_sip_header_via_get_host(via);
	hop->host=belle_sip_strdup(host);
	hop->port=belle_sip_header_via_get_rport(via);
	if (hop->port==-1)
		hop->port=belle_sip_header_via_get_listening_port(via);
}
