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



static void belle_http_request_init(belle_http_request_t *req){
	/*nop*/
}
static void belle_http_request_listener_destroyed(belle_http_request_t *req){
	req->listener=NULL;
}

static void belle_http_request_destroy(belle_http_request_t *req){
	if (req->req_uri) belle_sip_object_unref(req->req_uri);
	DESTROY_STRING(req,method)
	belle_http_request_set_listener(req,NULL);
	belle_http_request_set_channel(req,NULL);
	SET_OBJECT_PROPERTY(req,orig_uri,NULL);
	SET_OBJECT_PROPERTY(req,response,NULL);
}

static void belle_http_request_clone(belle_http_request_t *obj, const belle_http_request_t *orig){
	if (orig->req_uri) obj->req_uri=(belle_generic_uri_t*)belle_sip_object_clone((belle_sip_object_t*)orig->req_uri);
	CLONE_STRING(belle_http_request,method,obj,orig)
}

static belle_sip_error_code belle_http_request_marshal(const belle_http_request_t* request, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_error_code error=belle_sip_snprintf(buff,buff_size,offset,"%s ",belle_http_request_get_method(request));
	if (error!=BELLE_SIP_OK) return error;
	error=belle_generic_uri_marshal(belle_http_request_get_uri(request),buff,buff_size,offset);
	if (error!=BELLE_SIP_OK) return error;
	error=belle_sip_snprintf(buff,buff_size,offset," %s","HTTP/1.1\r\n");
	if (error!=BELLE_SIP_OK) return error;
	error=belle_sip_headers_marshal(BELLE_SIP_MESSAGE(request),buff,buff_size,offset);
	if (error!=BELLE_SIP_OK) return error;

	return error;
}
GET_SET_STRING(belle_http_request,method);
BELLE_NEW(belle_http_request,belle_sip_message)
BELLE_PARSE(belle_sip_messageParser,belle_,http_request)



belle_http_request_t *belle_http_request_create(const char *method, belle_generic_uri_t *url, ...){
	va_list vl;
	belle_http_request_t *obj;
	belle_sip_header_t *header;

	if (belle_generic_uri_get_host(url) == NULL) {
		belle_sip_error("%s: NULL host in url", __FUNCTION__);
		return NULL;
	}

	obj=belle_http_request_new();
	obj->method=belle_sip_strdup(method);
	obj->req_uri=(belle_generic_uri_t*)belle_sip_object_ref(url);

	va_start(vl,url);
	while((header=va_arg(vl,belle_sip_header_t*))!=NULL){
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(obj),header);
	}
	va_end(vl);
	return obj;
}

int belle_http_request_is_cancelled(const belle_http_request_t *req) {
	return req->cancelled;
}

void belle_http_request_cancel(belle_http_request_t *req) {
	req->cancelled = TRUE;
}

void belle_http_request_set_listener(belle_http_request_t *req, belle_http_request_listener_t *l){
	if (req->listener){
		 belle_sip_object_weak_unref(req->listener,(belle_sip_object_destroy_notify_t)belle_http_request_listener_destroyed,req);
		 req->listener=NULL;
	}
	if (l){
		belle_sip_object_weak_ref(l,(belle_sip_object_destroy_notify_t)belle_http_request_listener_destroyed,req);
		req->listener=l;
	}
}

static void notify_http_request_of_channel_destruction(belle_http_request_t *obj, belle_sip_channel_t *chan_being_destroyed){
	obj->channel=NULL;
}

void belle_http_request_set_channel(belle_http_request_t *req, belle_sip_channel_t* chan){
	if (req->channel){
		belle_sip_object_weak_unref(req->channel, (belle_sip_object_destroy_notify_t)notify_http_request_of_channel_destruction, req);
		req->channel=NULL;
	}
	if (chan){
		belle_sip_object_weak_ref(chan, (belle_sip_object_destroy_notify_t)notify_http_request_of_channel_destruction, req);
		req->channel=chan;
	}
}

belle_http_request_listener_t * belle_http_request_get_listener(const belle_http_request_t *req){
	return req->listener;
}

belle_generic_uri_t *belle_http_request_get_uri(const belle_http_request_t *req){
	return req->req_uri;
}

void belle_http_request_set_uri(belle_http_request_t* request, belle_generic_uri_t* uri) {
	SET_OBJECT_PROPERTY(request,req_uri,uri);
}

void belle_http_request_set_response(belle_http_request_t *req, belle_http_response_t *resp){
	SET_OBJECT_PROPERTY(req,response,resp);
}

belle_http_response_t *belle_http_request_get_response(belle_http_request_t *req){
	return req->response;
}

/*response*/


struct belle_http_response{
	belle_sip_message_t base;
	char *http_version;
	int status_code;
	char *reason_phrase;
};


void belle_http_response_destroy(belle_http_response_t *resp){
	if (resp->http_version) belle_sip_free(resp->http_version);
	if (resp->reason_phrase) belle_sip_free(resp->reason_phrase);
}

static void belle_http_response_init(belle_http_response_t *resp){
}

static void belle_http_response_clone(belle_http_response_t *resp, const belle_http_response_t *orig){
	if (orig->http_version) resp->http_version=belle_sip_strdup(orig->http_version);
	resp->status_code=orig->status_code;
	if (orig->reason_phrase) resp->reason_phrase=belle_sip_strdup(orig->reason_phrase);
}

belle_sip_error_code belle_http_response_marshal(belle_http_response_t *resp, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_error_code error=belle_sip_snprintf(	buff
													,buff_size
													,offset
													,"HTTP/1.1 %i %s\r\n"
													,belle_http_response_get_status_code(resp)
													,belle_http_response_get_reason_phrase(resp)?belle_http_response_get_reason_phrase(resp):"");
	if (error!=BELLE_SIP_OK) return error;
	error=belle_sip_headers_marshal(BELLE_SIP_MESSAGE(resp),buff,buff_size,offset);
	if (error!=BELLE_SIP_OK) return error;

	return error;
}

BELLE_NEW(belle_http_response,belle_sip_message)
BELLE_PARSE(belle_sip_messageParser,belle_,http_response)
GET_SET_STRING(belle_http_response,reason_phrase);
GET_SET_INT(belle_http_response,status_code,int)


