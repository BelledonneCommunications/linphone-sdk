/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2013  Belledonne Communications SARL

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


#include "belle_sip_internal.h"


typedef struct belle_http_callbacks belle_http_callbacks_t;

struct belle_http_callbacks{
	belle_sip_object_t base;
	belle_http_request_listener_callbacks_t cbs;
	void *user_ctx;
};

static void process_response_headers(belle_http_request_listener_t *l, const belle_http_response_event_t *event){
	belle_http_callbacks_t *obj=(belle_http_callbacks_t*)l;
	if (obj->cbs.process_response_headers)
		obj->cbs.process_response_headers(obj->user_ctx,event);
}

static void process_response_event(belle_http_request_listener_t *l, const belle_http_response_event_t *event){
	belle_http_callbacks_t *obj=(belle_http_callbacks_t*)l;
	if (obj->cbs.process_response)
		obj->cbs.process_response(obj->user_ctx,event);
}

static void process_io_error(belle_http_request_listener_t *l, const belle_sip_io_error_event_t *event){
	belle_http_callbacks_t *obj=(belle_http_callbacks_t*)l;
	if (obj->cbs.process_io_error)
		obj->cbs.process_io_error(obj->user_ctx,event);
}

static void process_timeout(belle_http_request_listener_t *l, const belle_sip_timeout_event_t *event){
	belle_http_callbacks_t *obj=(belle_http_callbacks_t*)l;
	if (obj->cbs.process_timeout)
		obj->cbs.process_timeout(obj->user_ctx,event);
}

static void process_auth_requested(belle_http_request_listener_t *l, belle_sip_auth_event_t *event){
	belle_http_callbacks_t *obj=(belle_http_callbacks_t*)l;
	if (obj->cbs.process_auth_requested)
		obj->cbs.process_auth_requested(obj->user_ctx,event);
}



/*BELLE_SIP_DECLARE_VPTR(belle_http_callbacks_t);*/

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_http_callbacks_t,belle_http_request_listener_t)
	process_response_headers,
	process_response_event,
	process_io_error,
	process_timeout,
	process_auth_requested
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_http_callbacks_t,belle_http_request_listener_t);

static void belle_http_callbacks_destroy(belle_http_callbacks_t *obj){
	if (obj->cbs.listener_destroyed)
		obj->cbs.listener_destroyed(obj->user_ctx);
}

BELLE_SIP_INSTANCIATE_VPTR(belle_http_callbacks_t,belle_sip_object_t,belle_http_callbacks_destroy,NULL,NULL,FALSE);
	
belle_http_request_listener_t *belle_http_request_listener_create_from_callbacks(const belle_http_request_listener_callbacks_t *callbacks, void *user_ctx){
	belle_http_callbacks_t *obj=belle_sip_object_new(belle_http_callbacks_t);
	memcpy(&obj->cbs,callbacks,sizeof(belle_http_request_listener_callbacks_t));
	obj->user_ctx=user_ctx;
	return BELLE_HTTP_REQUEST_LISTENER(obj);
}
