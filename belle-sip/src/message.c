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
#include "grammars/belle_sip_messageLexer.h"
#include "grammars/belle_sip_messageParser.h"

#include "belle_sip_internal.h"


typedef struct _headers_container {
	char* name;
	belle_sip_list_t* header_list;
} headers_container_t;

/*reference is 
 * http://www.iana.org/assignments/sip-parameters/sip-parameters.xhtml#sip-parameters-2
 */
static const char * expand_name(const char *name){
	const char *full_name=NULL;
	
	if (strlen(name)>1) return name;
	switch(name[0]){
		case 'a':
			full_name="Accept-Contact";
			break;
		case 'u':
			full_name="Allow-Events";
			break;
		case 'e':
			full_name="Content-Encoding";
			break;
		case 'o':
			full_name="Event";
			break;
		case 'y':
			full_name="Identity";
			break;
		case 'n':
			full_name="Identity-Info";
			break;
		case 'r':
			full_name="Refer-To";
			break;
		case 'b':
			full_name="Referred-By";
			break;
		case 'j':
			full_name="Reject-Contact";
			break;
		case 'd':
			full_name="Request-Disposition";
			break;
		case 'x':
			full_name="Session-Expires";
			break;
		case 's':
			full_name="Subject";
			break;
		case 'k':
			full_name="Supported";
			break;
		default:
			full_name=name;
	}
	return full_name;
}

static headers_container_t* belle_sip_message_headers_container_new(const char* name) {
	headers_container_t* headers_container = belle_sip_new0(headers_container_t);
	headers_container->name = belle_sip_strdup(expand_name(name));
	return  headers_container;
}

static void belle_sip_headers_container_delete(headers_container_t *obj){
	belle_sip_free(obj->name);
	belle_sip_list_free_with_data(obj->header_list,(void (*)(void*))belle_sip_object_unref);
	belle_sip_free(obj);
}

static void belle_sip_message_destroy(belle_sip_message_t *msg){
	belle_sip_list_free_with_data(msg->header_list,(void (*)(void*))belle_sip_headers_container_delete);
	if (msg->body_handler)
		belle_sip_object_unref(msg->body_handler);
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

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_message_t,belle_sip_object_t,belle_sip_message_destroy,belle_sip_message_clone,NULL,TRUE);

belle_sip_message_t* belle_sip_message_parse (const char* value) {
	size_t message_length;
	return belle_sip_message_parse_raw(value,strlen(value),&message_length);
}

belle_sip_message_t* belle_sip_message_parse_raw (const char* buff, size_t buff_length,size_t* message_length ) { \
	pANTLR3_INPUT_STREAM           input;
	pbelle_sip_messageLexer               lex;
	pANTLR3_COMMON_TOKEN_STREAM    tokens;
	pbelle_sip_messageParser              parser;
	belle_sip_message_t* l_parsed_object;
	input  = ANTLR_STREAM_NEW("message",buff,buff_length);
	lex    = belle_sip_messageLexerNew                (input);
	tokens = antlr3CommonTokenStreamSourceNew  (1025, lex->pLexer->rec->state->tokSource);
	parser = belle_sip_messageParserNew               (tokens);
	l_parsed_object = parser->message_raw(parser,message_length);
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

void belle_sip_message_init(belle_sip_message_t *message){
	
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

void belle_sip_message_add_first(belle_sip_message_t *message,belle_sip_header_t* header) {
	headers_container_t *headers_container=get_or_create_container(message,belle_sip_header_get_name(header));
	headers_container->header_list=belle_sip_list_prepend(headers_container->header_list,belle_sip_object_ref(header));
}

void belle_sip_message_add_header(belle_sip_message_t *message,belle_sip_header_t* header) {
	headers_container_t *headers_container=get_or_create_container(message,belle_sip_header_get_name(header));
	headers_container->header_list=belle_sip_list_append(headers_container->header_list,belle_sip_object_ref(header));
}

void belle_sip_message_add_headers(belle_sip_message_t *message, const belle_sip_list_t *header_list){
	const char *hname;
	headers_container_t *headers_container;
	
	if (header_list == NULL) return;
	
	hname=belle_sip_header_get_name(BELLE_SIP_HEADER((header_list->data)));
	headers_container=get_or_create_container(message,hname);
	
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

belle_sip_object_t *_belle_sip_message_get_header_by_type_id(const belle_sip_message_t *message, belle_sip_type_id_t id){
	const belle_sip_list_t *e1;
	for(e1=message->header_list;e1!=NULL;e1=e1->next){
		headers_container_t* headers_container=(headers_container_t*)e1->data;
		if (headers_container->header_list){
			belle_sip_object_t *ret=headers_container->header_list->data;
			if (ret->vptr->id==id) return ret;
		}
	}
	return NULL;
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
		msg->header_list = belle_sip_list_remove(msg->header_list,headers_container);
		belle_sip_headers_container_delete(headers_container);
	}
}
void belle_sip_message_remove_header_from_ptr(belle_sip_message_t *msg, belle_sip_header_t* header) {
	headers_container_t* headers_container = belle_sip_headers_container_get(msg,belle_sip_header_get_name(header));
	belle_sip_list_t* it;
	it=belle_sip_list_find(headers_container->header_list,header);
	if (it) {
		belle_sip_object_unref(header);
		headers_container->header_list=belle_sip_list_delete_link(headers_container->header_list,it);
		if (belle_sip_list_size(headers_container->header_list) == 0) {
			msg->header_list = belle_sip_list_remove(msg->header_list,headers_container);
			belle_sip_headers_container_delete(headers_container);
		}
	}
}
/*
belle_sip_error_code belle_sip_message_named_headers_marshal(belle_sip_message_t *message, const char* header_name, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_error_code error=BELLE_SIP_OK;
	belle_sip_list_t* header_list = belle_sip_message_get_headers(message,header_name);
	if (!header_list) {
		belle_sip_error("headers [%s] not found",header_name);
		return 0;
	}
	for(;header_list!=NULL;header_list=header_list->next){
		belle_sip_header_t *h=BELLE_SIP_HEADER(header_list->data);
		error=belle_sip_object_marshal(BELLE_SIP_OBJECT(h),buff,buff_size,offset);
		if (error!=BELLE_SIP_OK) return error;
		error=belle_sip_snprintf(buff,buff_size,offset,"%s","\r\n");
		if (error!=BELLE_SIP_OK) return error;
	}
	return error;
}

#define MARSHAL_AND_CHECK_HEADER(header) \
		if (current_offset == (current_offset+=(header))) {\
			belle_sip_error("missing mandatory header");\
			return current_offset;\
		} else {\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","\r\n");\
		}
*/
typedef void (*each_header_cb)(const belle_sip_header_t* header,void* userdata);

static void belle_sip_message_for_each_header(const belle_sip_message_t *message,each_header_cb cb,void* user_data) {
	belle_sip_list_t* headers_list;
	belle_sip_list_t* header_list;
	for(headers_list=message->header_list;headers_list!=NULL;headers_list=headers_list->next){
		for(header_list=((headers_container_t*)(headers_list->data))->header_list
				;header_list!=NULL
				;header_list=header_list->next)	{
			cb(BELLE_SIP_HEADER(header_list->data),user_data);
		}
	}
	return;
}
static void append_header(const belle_sip_header_t* header,void* user_data) {
	*(belle_sip_list_t**)user_data=belle_sip_list_append((*(belle_sip_list_t**)user_data),(void*)header);
}

belle_sip_list_t* belle_sip_message_get_all_headers(const belle_sip_message_t *message) {
	belle_sip_list_t* headers=NULL;
	belle_sip_message_for_each_header(message,append_header,&headers);
	return headers;
}

belle_sip_error_code belle_sip_headers_marshal(belle_sip_message_t *message, char* buff, size_t buff_size, size_t *offset) {
	/*FIXME, replace this code by belle_sip_message_for_each_header*/
	belle_sip_list_t* headers_list;
	belle_sip_list_t* header_list;
	belle_sip_error_code error=BELLE_SIP_OK;
#ifdef BELLE_SIP_WORKAROUND_TECHNICOLOR_SIP_ALG_ROUTER_BUG
	belle_sip_header_t *content_length=NULL;
#endif
	
	for(headers_list=message->header_list;headers_list!=NULL;headers_list=headers_list->next){
		for(header_list=((headers_container_t*)(headers_list->data))->header_list
				;header_list!=NULL
				;header_list=header_list->next)	{
			belle_sip_header_t *h=BELLE_SIP_HEADER(header_list->data);
#ifdef BELLE_SIP_WORKAROUND_TECHNICOLOR_SIP_ALG_ROUTER_BUG
			if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(h,belle_sip_header_content_length_t)){
				content_length=h;
			}else
#endif
			{
				while (h!=NULL) { /*header can be chained*/
					error=belle_sip_object_marshal(BELLE_SIP_OBJECT(h),buff,buff_size,offset);
					if (error!=BELLE_SIP_OK) return error;
					error=belle_sip_snprintf(buff,buff_size,offset,"%s","\r\n");
					if (error!=BELLE_SIP_OK) return error;
					h= belle_sip_header_get_next(h);
				}
			}
		}
	}
#ifdef BELLE_SIP_WORKAROUND_TECHNICOLOR_SIP_ALG_ROUTER_BUG
	if (content_length){
		error=belle_sip_object_marshal(BELLE_SIP_OBJECT(content_length),buff,buff_size,offset);
		if (error!=BELLE_SIP_OK) return error;
		error=belle_sip_snprintf(buff,buff_size,offset,"%s","\r\n");
		if (error!=BELLE_SIP_OK) return error;
	}
#endif
	error=belle_sip_snprintf(buff,buff_size,offset,"%s","\r\n");
	if (error!=BELLE_SIP_OK) return error;
	return error;
}

static void belle_sip_request_destroy(belle_sip_request_t* request) {
	if (request->method) belle_sip_free(request->method);
	if (request->uri) belle_sip_object_unref(request->uri);
	if (request->absolute_uri) belle_sip_object_unref(request->absolute_uri);
	if (request->dialog) belle_sip_object_unref(request->dialog);
	if (request->rfc2543_branch) belle_sip_free(request->rfc2543_branch);
}

static void belle_sip_request_init(belle_sip_request_t *message){
}

static void belle_sip_request_clone(belle_sip_request_t *request, const belle_sip_request_t *orig){
	if (orig->method) request->method=belle_sip_strdup(orig->method);
	if (orig->uri) request->uri=(belle_sip_uri_t*)belle_sip_object_ref(belle_sip_object_clone((belle_sip_object_t*)orig->uri));
	if (orig->absolute_uri) request->absolute_uri=(belle_generic_uri_t*)belle_sip_object_ref(belle_sip_object_clone((belle_sip_object_t*)orig->absolute_uri));
	if (orig->rfc2543_branch) request->rfc2543_branch=belle_sip_strdup(orig->rfc2543_branch);
}

belle_sip_error_code belle_sip_request_marshal(belle_sip_request_t* request, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_error_code error=belle_sip_snprintf(buff,buff_size,offset,"%s ",belle_sip_request_get_method(request) ? belle_sip_request_get_method(request) : "");

	if (error!=BELLE_SIP_OK) return error;
	if (request->uri)
		error=belle_sip_uri_marshal(belle_sip_request_get_uri(request),buff,buff_size,offset);
	else if (request->absolute_uri)
		error=belle_generic_uri_marshal(belle_sip_request_get_absolute_uri(request),buff,buff_size,offset);
	else {
		belle_sip_error("Missing uri for marshaling request [%p]",request);
		/*fixme better to have an error code*/
		error=BELLE_SIP_OK;
	}

	if (error!=BELLE_SIP_OK) return error;
	error=belle_sip_snprintf(buff,buff_size,offset," %s","SIP/2.0\r\n");
	if (error!=BELLE_SIP_OK) return error;
	error=belle_sip_headers_marshal(BELLE_SIP_MESSAGE(request),buff,buff_size,offset);
	if (error!=BELLE_SIP_OK) return error;
	
	return error;
}

BELLE_SIP_NEW(request,message)
BELLE_SIP_PARSE(request)
GET_SET_STRING(belle_sip_request,method);
GET_SET_STRING(belle_sip_request,rfc2543_branch);

/*caching of the dialog in the request, used when creating a request in dialog to avoid dialog lookup*/
void belle_sip_request_set_dialog(belle_sip_request_t *req, belle_sip_dialog_t *dialog){
	SET_OBJECT_PROPERTY(req,dialog,dialog);
}

void belle_sip_request_set_uri(belle_sip_request_t* request,belle_sip_uri_t* uri) {
	SET_OBJECT_PROPERTY(request,uri,uri);
	if (request->absolute_uri && uri) {
		belle_sip_warning("absolute uri [%p] already set for request [%p], cleaning it",request->absolute_uri, request);
		belle_sip_request_set_absolute_uri(request,NULL);
	}
}

belle_sip_uri_t * belle_sip_request_get_uri(const belle_sip_request_t *request){
	return request->uri;
}

void belle_sip_request_set_absolute_uri(belle_sip_request_t* request,belle_generic_uri_t* absolute_uri) {
	SET_OBJECT_PROPERTY(request,absolute_uri,absolute_uri);
	if (request->uri && absolute_uri) {
		belle_sip_warning("sip  uri [%p] already set for request [%p], cleaning it",request->uri, request);
		belle_sip_request_set_uri(request,NULL);
	}
}

belle_generic_uri_t * belle_sip_request_get_absolute_uri(const belle_sip_request_t *request){
	return request->absolute_uri;
}


belle_sip_uri_t* belle_sip_request_extract_origin(const belle_sip_request_t* req) {
	belle_sip_header_via_t* via_header = belle_sip_message_get_header_by_type(req,belle_sip_header_via_t);
	belle_sip_uri_t* uri=NULL;
	const char* received = belle_sip_header_via_get_received(via_header);
	int rport = belle_sip_header_via_get_rport(via_header);
	uri = belle_sip_uri_new();
	if (received!=NULL) {
		belle_sip_uri_set_host(uri,received);
	} else {
		belle_sip_uri_set_host(uri,belle_sip_header_via_get_host(via_header));
	}
	if (rport>0) {
		belle_sip_uri_set_port(uri,rport);
	} else if (belle_sip_header_via_get_port(via_header)) {
		belle_sip_uri_set_port(uri,belle_sip_header_via_get_port(via_header));
	}
	if (belle_sip_header_via_get_transport(via_header)) {
		belle_sip_uri_set_transport_param(uri,belle_sip_header_via_get_transport_lowercase(via_header));
	}
	return uri;
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

belle_sip_body_handler_t *belle_sip_message_get_body_handler(const belle_sip_message_t *msg){
	return msg->body_handler;
}

void belle_sip_message_set_body_handler(belle_sip_message_t *msg, belle_sip_body_handler_t *body_handler){
	belle_sip_header_content_length_t *content_length_header = belle_sip_message_get_header_by_type(msg, belle_sip_header_content_length_t);
	belle_sip_header_content_type_t *content_type_header = belle_sip_message_get_header_by_type(msg, belle_sip_header_content_type_t);

	/* In case of multipart message, we must add the message Content-Type header containing the boundary */
	if (body_handler != NULL) {
		if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(body_handler, belle_sip_multipart_body_handler_t)){
			belle_sip_multipart_body_handler_t *multipart_body_handler = BELLE_SIP_MULTIPART_BODY_HANDLER(body_handler);
			belle_sip_header_content_type_t * content_type = belle_sip_header_content_type_new();
			belle_sip_header_content_type_set_type(content_type, "multipart");
			if (belle_sip_multipart_body_handler_is_related(multipart_body_handler)) {
				const belle_sip_list_t *parts = belle_sip_multipart_body_handler_get_parts(multipart_body_handler);
				if (parts) {
					belle_sip_body_handler_t *first_part=BELLE_SIP_BODY_HANDLER(parts->data);
					const belle_sip_list_t *first_part_headers = belle_sip_body_handler_get_headers(first_part);
					belle_sip_list_t *it;
					belle_sip_header_content_type_t *first_part_content_type=NULL;;
					for(it = (belle_sip_list_t *)first_part_headers;it!=NULL;it=it->next) {
						belle_sip_header_t *header = BELLE_SIP_HEADER(it->data);
						if(strcasecmp("Content-Type",belle_sip_header_get_name(header)) == 0) {
							first_part_content_type=BELLE_SIP_HEADER_CONTENT_TYPE(header);
							break;
						}
					}
					if (first_part_content_type) {
						char *type_slash_subtype = belle_sip_strdup_printf("%s/%s"
																		   , belle_sip_header_content_type_get_type(first_part_content_type)
																		   , belle_sip_header_content_type_get_subtype(first_part_content_type));
						belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(content_type), "type", type_slash_subtype);
						belle_sip_free(type_slash_subtype);
					} else {
						belle_sip_error("Multipart related body handler [%p] cannot be set without first part content type header",body_handler);
					}
				} else {
					belle_sip_error("Multipart related body handler [%p] cannot be set without first part",body_handler);
				}
				belle_sip_header_content_type_set_subtype(content_type, "related");
				
			} else {
				belle_sip_header_content_type_set_subtype(content_type, "form-data");
			}
			belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(content_type), "boundary", BELLESIP_MULTIPART_BOUNDARY);
			belle_sip_message_add_header(BELLE_SIP_MESSAGE(msg), BELLE_SIP_HEADER(content_type));
		} else {
			const belle_sip_list_t *headers = belle_sip_body_handler_get_headers(body_handler);
			for(; headers != NULL; headers = headers->next) {
				belle_sip_header_t *header = BELLE_SIP_HEADER(headers->data);
				if (strcasecmp(belle_sip_header_get_name(header),BELLE_SIP_CONTENT_LENGTH ) == 0 && content_length_header)
					belle_sip_message_remove_header_from_ptr(msg, BELLE_SIP_HEADER(content_length_header));
				
				if (strcasecmp(belle_sip_header_get_name(header),BELLE_SIP_CONTENT_TYPE ) == 0 && content_type_header)
					belle_sip_message_remove_header_from_ptr(msg, BELLE_SIP_HEADER(content_type_header));
				
				belle_sip_message_add_header(BELLE_SIP_MESSAGE(msg), header);
			}
		}
	} else {
		if (content_length_header != NULL) belle_sip_message_remove_header_from_ptr(msg, BELLE_SIP_HEADER(content_length_header));
		if (content_type_header != NULL) belle_sip_message_remove_header_from_ptr(msg, BELLE_SIP_HEADER(content_type_header));
	}

	SET_OBJECT_PROPERTY(msg,body_handler,body_handler);
}

const char* belle_sip_message_get_body(belle_sip_message_t *msg) {
	if (msg->body_handler==NULL) return NULL;
	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(msg->body_handler, belle_sip_memory_body_handler_t)){
		return (const char*)belle_sip_memory_body_handler_get_buffer(
			BELLE_SIP_MEMORY_BODY_HANDLER(msg->body_handler)
		);
	}
	belle_sip_error("belle_sip_message_get_body(): body cannot be returned as pointer.");
	return NULL;
}

size_t belle_sip_message_get_body_size(const belle_sip_message_t *msg){
	if (msg->body_handler==NULL) return 0;
	return belle_sip_body_handler_get_size(msg->body_handler);
}

void belle_sip_message_set_body(belle_sip_message_t *msg, const char* body, size_t size) {
	belle_sip_body_handler_t *bh=NULL;
	if (body && size) bh=(belle_sip_body_handler_t*)belle_sip_memory_body_handler_new_copy_from_buffer(body,size,NULL,NULL);
	belle_sip_message_set_body_handler(msg,bh);
}

void belle_sip_message_assign_body(belle_sip_message_t *msg, char* body, size_t size) {
	belle_sip_body_handler_t *bh=(belle_sip_body_handler_t*)belle_sip_memory_body_handler_new_from_buffer(body,size,NULL,NULL);
	belle_sip_message_set_body_handler(msg,bh);
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
	{	412		,		"Conditional Request Failed" }, /*rfc3903*/
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
	{	489		,		"Bad Event"	}, /*rfc3265*/
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

belle_sip_error_code belle_sip_response_marshal(belle_sip_response_t *resp, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_error_code error=belle_sip_snprintf(	buff
						,buff_size
						,offset
						,"SIP/2.0 %i %s\r\n"
						,belle_sip_response_get_status_code(resp)
						,belle_sip_response_get_reason_phrase(resp)?belle_sip_response_get_reason_phrase(resp):"");
	
	if (error!=BELLE_SIP_OK) return error;
	error=belle_sip_headers_marshal(BELLE_SIP_MESSAGE(resp),buff,buff_size,offset);
	if (error!=BELLE_SIP_OK) return error;
	return error;
}

BELLE_SIP_NEW(response,message);
BELLE_SIP_PARSE(response)
GET_SET_STRING(belle_sip_response,reason_phrase);
GET_SET_INT(belle_sip_response,status_code,int)

static int is_authorized_uri_header(const char* header_name) {
	 /*From, Call-ID, CSeq, Via, and Record-Route*/
	/*Accept, Accept-Encoding, Accept-Language, Allow,
   Contact (in its dialog usage), Organization, Supported, and User-Agent*/
	return (strcasecmp("From",header_name) != 0
			&& strcasecmp("Call-ID",header_name) != 0
			&& strcasecmp("CSeq",header_name) != 0
			&& strcasecmp("Via",header_name) != 0
			&& strcasecmp("Record-Route",header_name) != 0
			&& strcasecmp("Accept",header_name) != 0
			&& strcasecmp("Accept-Encoding",header_name) != 0
			&& strcasecmp("Accept-Language",header_name) != 0
			&& strcasecmp("Allow",header_name) != 0
			&& strcasecmp("Contact",header_name) != 0
			&& strcasecmp("Organization",header_name) != 0
			&& strcasecmp("Supported",header_name) != 0
			&& strcasecmp("User-Agent",header_name) != 0);

}

belle_sip_request_t* belle_sip_request_create(belle_sip_uri_t *requri, const char* method,
                                         belle_sip_header_call_id_t *callid,
                                         belle_sip_header_cseq_t * cseq,
                                         belle_sip_header_from_t *from,
                                         belle_sip_header_to_t *to,
                                         belle_sip_header_via_t *via,
                                         int max_forward)
{
	belle_sip_request_t *ret=belle_sip_request_new();
	belle_sip_header_max_forwards_t *mf=belle_sip_header_max_forwards_new();
	belle_sip_list_t* iterator;
	if (max_forward==0) max_forward=70;
	belle_sip_header_max_forwards_set_max_forwards(mf,max_forward);

	belle_sip_request_set_method(ret,method);
	belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(via));
	belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(from));
	if (to) belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(to)); /*to might be in header uri*/
	belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(cseq));
	belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(callid));
	if (!belle_sip_message_get_header_by_type(ret,belle_sip_header_max_forwards_t))
		belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(mf));
	else
		belle_sip_object_unref(mf);

	/*
	19.1.5 Forming Requests from a URI

   An implementation needs to take care when forming requests directly
   from a URI.  URIs from business cards, web pages, and even from
   sources inside the protocol such as registered contacts may contain
   inappropriate header fields or body parts.

   An implementation MUST include any provided transport, maddr, ttl, or
   user parameter in the Request-URI of the formed request.  If the URI
   contains a method parameter, its value MUST be used as the method of
   the request.  The method parameter MUST NOT be placed in the
   Request-URI.  Unknown URI parameters MUST be placed in the message's
   Request-URI.

   An implementation SHOULD treat the presence of any headers or body
   parts in the URI as a desire to include them in the message, and
   choose to honor the request on a per-component basis.

   An implementation SHOULD NOT honor these obviously dangerous header
   fields: From, Call-ID, CSeq, Via, and Record-Route.

   An implementation SHOULD NOT honor any requested Route header field
   values in order to not be used as an unwitting agent in malicious
   attacks.

   An implementation SHOULD NOT honor requests to include header fields
   that may cause it to falsely advertise its location or capabilities.
   These include: Accept, Accept-Encoding, Accept-Language, Allow,
   Contact (in its dialog usage), Organization, Supported, and User-
   Agent.

   An implementation SHOULD verify the accuracy of any requested
   descriptive header fields, including: Content-Disposition, Content-
   Encoding, Content-Language, Content-Length, Content-Type, Date,
   Mime-Version, and Timestamp.*/

	if (belle_sip_uri_get_header_names(requri)) {
		for (iterator=(belle_sip_list_t*)belle_sip_uri_get_header_names(requri);iterator!=NULL;iterator=iterator->next) {
			const char* header_name=(const char*)iterator->data;
			/*1 check header name*/
			if (is_authorized_uri_header(header_name)) {
				belle_sip_header_extension_t* extended_header = belle_sip_header_extension_create(header_name, belle_sip_uri_get_header(requri, header_name));
				if (extended_header) {
					belle_sip_message_add_header((belle_sip_message_t*)ret,BELLE_SIP_HEADER(extended_header));
				}
			} else {
				belle_sip_warning("Skiping uri header [%s] for request [%p]",header_name,requri);
			}
		}
	}
	belle_sip_uri_headers_clean(requri); /*remove all headers*/

	belle_sip_request_set_uri(ret,requri);


	return ret;
}

static void belle_sip_response_init_default(belle_sip_response_t *resp, int status_code, const char *phrase){
	resp->status_code=status_code;
	resp->sip_version=belle_sip_strdup("SIP/2.0");
	if (phrase==NULL) phrase=belle_sip_get_well_known_reason_phrase(status_code);
	resp->reason_phrase=belle_sip_strdup(phrase);
}

/*
 * note: we must not assume the request to be well formed because this function may be used to generate 400 Bad request response.
 */
belle_sip_response_t *belle_sip_response_create_from_request(belle_sip_request_t *req, int status_code){
	belle_sip_response_t *resp=belle_sip_response_new();
	belle_sip_header_t *h;
	belle_sip_header_to_t *to;
	const belle_sip_list_t *vias;
	
	belle_sip_response_init_default(resp,status_code,NULL);
	if (status_code==100 && (h=belle_sip_message_get_header((belle_sip_message_t*)req,"timestamp"))){
		belle_sip_message_add_header((belle_sip_message_t*)resp,h);
	}
	vias=belle_sip_message_get_headers ((belle_sip_message_t*)req,"via");
	belle_sip_message_add_headers((belle_sip_message_t*)resp,vias);
	h=belle_sip_message_get_header((belle_sip_message_t*)req,"from");
	if (h) belle_sip_message_add_header((belle_sip_message_t*)resp,h);
	h=belle_sip_message_get_header((belle_sip_message_t*)req,"to");
	if (h){
		if (status_code!=100){
			//so that to tag can be added
			to=(belle_sip_header_to_t*)belle_sip_object_clone((belle_sip_object_t*)h);
		}else{
			to=(belle_sip_header_to_t*)h;
		}
		belle_sip_message_add_header((belle_sip_message_t*)resp,(belle_sip_header_t*)to);
	}
	h=belle_sip_message_get_header((belle_sip_message_t*)req,"call-id");
	if (h) belle_sip_message_add_header((belle_sip_message_t*)resp,h);
	h=belle_sip_message_get_header((belle_sip_message_t*)req,"cseq");
	if (h){
		belle_sip_message_add_header((belle_sip_message_t*)resp,h);
	}
	return resp;
}
/*
12.1.1 UAS behavior

   When a UAS responds to a request with a response that establishes a
   dialog (such as a 2xx to INVITE), the UAS MUST copy all Record-Route
   header field values from the request into the response (including the
   URIs, URI parameters, and any Record-Route header field parameters,
   whether they are known or unknown to the UAS) and MUST maintain the
   order of those values.
   */
void belle_sip_response_fill_for_dialog(belle_sip_response_t *obj, belle_sip_request_t *req){
	const belle_sip_list_t *rr=belle_sip_message_get_headers((belle_sip_message_t*)req,BELLE_SIP_RECORD_ROUTE);
	belle_sip_header_contact_t *ct=belle_sip_message_get_header_by_type(obj,belle_sip_header_contact_t);
	belle_sip_message_remove_header((belle_sip_message_t*)obj,BELLE_SIP_RECORD_ROUTE);
	if (rr)
		belle_sip_message_add_headers((belle_sip_message_t*)obj,rr);
	if (belle_sip_response_get_status_code(obj)>=200 && belle_sip_response_get_status_code(obj)<300 && !ct){
		const char *method=belle_sip_request_get_method(req);
		if (strcmp(method,"INVITE")==0 || strcmp(method,"SUBSCRIBE")==0){
			/*add a dummy contact to be filled by channel later*/
			belle_sip_message_add_header((belle_sip_message_t*)obj,(belle_sip_header_t*)belle_sip_header_contact_new());
		}
	}	
}

belle_sip_hop_t* belle_sip_response_get_return_hop(belle_sip_response_t *msg){
	belle_sip_header_via_t *via=BELLE_SIP_HEADER_VIA(belle_sip_message_get_header(BELLE_SIP_MESSAGE(msg),"via"));
	if (via){
		const char *host=belle_sip_header_via_get_received(via) ? belle_sip_header_via_get_received(via) : belle_sip_header_via_get_host(via);
		int port=belle_sip_header_via_get_rport(via)>0 ? belle_sip_header_via_get_rport(via) : belle_sip_header_via_get_listening_port(via);
		return belle_sip_hop_new(belle_sip_header_via_get_transport_lowercase(via),NULL,host,port);
	}
	return NULL;
}

int belle_sip_response_fix_contact(const belle_sip_response_t* response,belle_sip_header_contact_t* contact) {
	belle_sip_header_via_t* via_header;
	belle_sip_uri_t* contact_uri;
	const char* received;
	int rport;
	int contact_port;
	/*first check received/rport*/
	via_header= (belle_sip_header_via_t*)belle_sip_message_get_header(BELLE_SIP_MESSAGE(response),BELLE_SIP_VIA);
	received = belle_sip_header_via_get_received(via_header);
	rport = belle_sip_header_via_get_rport(via_header);
	contact_uri=belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(contact));
	if (received) {
		/*need to update host*/
		belle_sip_uri_set_host(contact_uri,received);
	} else {
		belle_sip_uri_set_host(contact_uri,belle_sip_header_via_get_host(via_header));
	}
	contact_port =  belle_sip_uri_get_port(contact_uri);
	if (rport>0 ) {
		/*need to update port*/
		if ((rport+contact_port)!=5060) belle_sip_uri_set_port(contact_uri,rport);
	} else if ((belle_sip_header_via_get_port(via_header)+contact_port)!=5060) {
		belle_sip_uri_set_port(contact_uri,belle_sip_header_via_get_port(via_header));
	}
	/*try to fix transport if needed (very unlikely)*/
	if (strcasecmp(belle_sip_header_via_get_transport(via_header),"UDP")!=0) {
		if (!belle_sip_uri_get_transport_param(contact_uri)
				||strcasecmp(belle_sip_uri_get_transport_param(contact_uri),belle_sip_header_via_get_transport(via_header))!=0) {
			belle_sip_uri_set_transport_param(contact_uri,belle_sip_header_via_get_transport_lowercase(via_header));
		}
	} else {
		if (belle_sip_uri_get_transport_param(contact_uri)) {
			belle_sip_uri_set_transport_param(contact_uri,NULL);
		}
	}
	return 0;
}

belle_sip_request_t * belle_sip_request_clone_with_body(const belle_sip_request_t *initial_req) {
	belle_sip_request_t* req=BELLE_SIP_REQUEST(belle_sip_object_clone(BELLE_SIP_OBJECT(initial_req)));
	if (initial_req->base.body_handler) req->base.body_handler=BELLE_SIP_BODY_HANDLER(belle_sip_object_clone_and_ref(
		(belle_sip_object_t*)initial_req->base.body_handler));
	return req;
}

typedef struct message_header_list {
	const char* method;
	const char* headers[10]; /*MAX headers*/
} message_header_list_t;
/**
 RFC 3261            SIP: Session Initiation Protocol           June 2002

   Header field          where           proxy ACK BYE CAN INV OPT REG
   __________________________________________________________________
   Accept                          R            -   o   -   o   m*  o
   Accept                         2xx           -   -   -   o   m*  o
   Accept                         415           -   c   -   c   c   c
   Accept-Encoding                 R            -   o   -   o   o   o
   Accept-Encoding                2xx           -   -   -   o   m*  o
   Accept-Encoding                415           -   c   -   c   c   c
   Accept-Language                 R            -   o   -   o   o   o
   Accept-Language                2xx           -   -   -   o   m*  o
   Accept-Language                415           -   c   -   c   c   c
   Alert-Info                      R      ar    -   -   -   o   -   -
   Alert-Info                     180     ar    -   -   -   o   -   -
   Allow                           R            -   o   -   o   o   o
   Allow                          2xx           -   o   -   m*  m*  o
   Allow                           r            -   o   -   o   o   o
   Allow                          405           -   m   -   m   m   m
   Authentication-Info            2xx           -   o   -   o   o   o
   Authorization                   R            o   o   o   o   o   o
   Call-ID                         c       r    m   m   m   m   m   m
   Call-Info                              ar    -   -   -   o   o   o
   Contact                         R            o   -   -   m   o   o
   Contact                        1xx           -   -   -   o   -   -
   Contact                        2xx           -   -   -   m   o   o
   Contact                        3xx      d    -   o   -   o   o   o
   Contact                        485           -   o   -   o   o   o
   Content-Disposition                          o   o   -   o   o   o
   Content-Encoding                             o   o   -   o   o   o
   Content-Language                             o   o   -   o   o   o
   Content-Length                         ar    t   t   t   t   t   t
   Content-Type                                 *   *   -   *   *   *
   CSeq                            c       r    m   m   m   m   m   m
   Date                                    a    o   o   o   o   o   o
   Error-Info                   300-699    a    -   o   o   o   o   o
   Expires                                      -   -   -   o   -   o
   From                            c       r    m   m   m   m   m   m
   In-Reply-To                     R            -   -   -   o   -   -
   Max-Forwards                    R      amr   m   m   m   m   m   m
   Min-Expires                    423           -   -   -   -   -   m
   MIME-Version                                 o   o   -   o   o   o
   Organization                           ar    -   -   -   o   o   o
   Priority                    R          ar    -   -   -   o   -   -
   Proxy-Authenticate         407         ar    -   m   -   m   m   m
   Proxy-Authenticate         401         ar    -   o   o   o   o   o
   Proxy-Authorization         R          dr    o   o   -   o   o   o
   Proxy-Require               R          ar    -   o   -   o   o   o
   Record-Route                R          ar    o   o   o   o   o   -
   Record-Route             2xx,18x       mr    -   o   o   o   o   -
   Reply-To                                     -   -   -   o   -   -
   Require                                ar    -   c   -   c   c   c
   Retry-After          404,413,480,486         -   o   o   o   o   o
                            500,503             -   o   o   o   o   o
                            600,603             -   o   o   o   o   o
   Route                       R          adr   c   c   c   c   c   c
   Server                      r                -   o   o   o   o   o
   Subject                     R                -   -   -   o   -   -
   Supported                   R                -   o   o   m*  o   o
   Supported                  2xx               -   o   o   m*  m*  o
   Timestamp                                    o   o   o   o   o   o
   To                        c(1)          r    m   m   m   m   m   m
   Unsupported                420               -   m   -   m   m   m
   User-Agent                                   o   o   o   o   o   o
   Via                         R          amr   m   m   m   m   m   m
   Via                        rc          dr    m   m   m   m   m   m
   Warning                     r                -   o   o   o   o   o
   WWW-Authenticate           401         ar    -   m   -   m   m   m
   WWW-Authenticate           407         ar    -   o   -   o   o   o

   Table 3: Summary of header fields, A--Z; (1): copied with possible
   addition of tag

 */
static message_header_list_t mandatory_headers[] = {
		{"REGISTER",{"Call-ID","CSeq","From", "Max-Forwards","To","Via",NULL}},
		{"INVITE",{"Contact","Call-ID","CSeq","From", "Max-Forwards","To","Via",NULL}},
		{"CANCEL",{"Call-ID","CSeq","From", "Max-Forwards","To","Via",NULL}},
		{"BYE",{"Call-ID","CSeq","From", "Max-Forwards","To","Via",NULL}},
		{"ACK",{"Call-ID","CSeq","From", "Max-Forwards","To","Via",NULL}},
		{"*", { "To", "From", "CSeq", "Via", NULL}}, /* catch-all, these fields are required all the time. */
		{NULL,{NULL}}
};

/*static int belle_sip_message_is_mandatody_header(const char* method, const char* header_name) {
	int i;
	for (i=0;mandatory_headers[i].method!=NULL;i++) {
		if (strcasecmp(method,mandatory_headers[i].method)==0) {
			int j;
			for(j=0;mandatory_headers[i].headers[j]!=NULL;j++) {
				if (strcasecmp(header_name,mandatory_headers[i].headers[j])==0) {
					return 1;
				}
			}
		}
	}
	return 0;
}
*/
int belle_sip_message_check_headers(const belle_sip_message_t* message) {
	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(message,belle_sip_request_t)) {
		int i;
		belle_sip_header_via_t *via;
		const char * method = belle_sip_request_get_method(BELLE_SIP_REQUEST(message));
		
		for (i=0;mandatory_headers[i].method!=NULL;i++) {
			if ( (strcasecmp(method,mandatory_headers[i].method)==0) ||
				 (mandatory_headers[i].method[0] == '*') ){
				int j;
				for(j=0;mandatory_headers[i].headers[j]!=NULL;j++) {
					if (belle_sip_message_get_header(message,mandatory_headers[i].headers[j])==NULL) {
						belle_sip_error("Missing mandatory header [%s] for message [%s]",mandatory_headers[i].headers[j],method);
						return 0;
					}
				}
				return 1;
			}
		}
		via=belle_sip_message_get_header_by_type(message,belle_sip_header_via_t);
		if (!via || belle_sip_header_via_get_branch(via)==NULL) return 0;
	}
	/*else fixme should also check responses*/
	return 1;

}

int belle_sip_request_check_uris_components(const belle_sip_request_t* request) {
	belle_sip_list_t* new_list = belle_sip_message_get_all_headers(BELLE_SIP_MESSAGE(request));
	belle_sip_list_t* iterator = new_list;

	for (;iterator!=NULL;iterator=iterator->next) {
		belle_sip_header_t* header=(belle_sip_header_t*)iterator->data;
		if (BELLE_SIP_IS_INSTANCE_OF(header,belle_sip_header_address_t)) {
			belle_sip_uri_t* uri=belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(header));
			if (uri && !belle_sip_uri_check_components_from_context(uri,belle_sip_request_get_method(request),belle_sip_header_get_name(header))) {
				char* header_string=belle_sip_object_to_string(header);
				belle_sip_error("Malformed header [%s] for request [%p]",header_string,request);
				belle_sip_free(header_string);
				belle_sip_list_free(new_list);
				return FALSE;
			}
		}
	}

	belle_sip_list_free(new_list);
	return belle_sip_uri_check_components_from_request_uri(belle_sip_request_get_uri((const belle_sip_request_t*)request));
}
