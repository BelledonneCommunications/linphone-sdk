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
#include "belle_sip_internal.h"


belle_sip_response_t* belle_sip_response_event_get_response(const belle_sip_response_event_t* event) {
	return event->response;
}

belle_sip_client_transaction_t *belle_sip_response_event_get_client_transaction(const belle_sip_response_event_t* event){
	return event->client_transaction;
}

belle_sip_dialog_t *belle_sip_response_event_get_dialog(const belle_sip_response_event_t* event){
	return event->dialog;
}

belle_sip_request_t* belle_sip_request_event_get_request(const belle_sip_request_event_t* event){
	return event->request;
}

belle_sip_server_transaction_t *belle_sip_request_event_get_server_transaction(const belle_sip_request_event_t* event){
	return event->server_transaction;
}

belle_sip_dialog_t *belle_sip_request_event_get_dialog(const belle_sip_request_event_t* event){
	return event->dialog;
}

belle_sip_dialog_t* belle_sip_dialog_terminated_event_get_dialog(const belle_sip_dialog_terminated_event_t *event) {
	return event->dialog;
}

int belle_sip_dialog_terminated_event_is_expired(const belle_sip_dialog_terminated_event_t *event){
	return event->is_expired;
}

const char* belle_sip_io_error_event_get_host(const belle_sip_io_error_event_t* event) {
	return event->host;
}

const char* belle_sip_io_error_event_get_transport(const belle_sip_io_error_event_t* event) {
	return event->transport;
}

unsigned int belle_sip_io_error_event_port(const belle_sip_io_error_event_t* event) {
	return event->port;
}

belle_sip_object_t* belle_sip_io_error_event_get_source(const belle_sip_io_error_event_t* event) {
	return event->source;
}



typedef struct belle_sip_callbacks belle_sip_callbacks_t;

struct belle_sip_callbacks{
	belle_sip_object_t base;
	belle_sip_listener_callbacks_t cbs;
	void *user_ctx;
};


static void process_dialog_terminated(belle_sip_listener_t *l, const belle_sip_dialog_terminated_event_t *event){
	belle_sip_callbacks_t *obj=(belle_sip_callbacks_t*)l;
	if (obj->cbs.process_dialog_terminated)
		obj->cbs.process_dialog_terminated(obj->user_ctx,event);
}

static void process_io_error(belle_sip_listener_t *l, const belle_sip_io_error_event_t *event){
	belle_sip_callbacks_t *obj=(belle_sip_callbacks_t*)l;
	if (obj->cbs.process_io_error)
		obj->cbs.process_io_error(obj->user_ctx,event);
}

static void process_request_event(belle_sip_listener_t *l, const belle_sip_request_event_t *event){
	belle_sip_callbacks_t *obj=(belle_sip_callbacks_t*)l;
	if (obj->cbs.process_request_event)
		obj->cbs.process_request_event(obj->user_ctx,event);
}

static void process_response_event(belle_sip_listener_t *l, const belle_sip_response_event_t *event){
	belle_sip_callbacks_t *obj=(belle_sip_callbacks_t*)l;
	if (obj->cbs.process_response_event)
		obj->cbs.process_response_event(obj->user_ctx,event);
}

static void process_timeout(belle_sip_listener_t *l, const belle_sip_timeout_event_t *event){
	belle_sip_callbacks_t *obj=(belle_sip_callbacks_t*)l;
	if (obj->cbs.process_timeout)
		obj->cbs.process_timeout(obj->user_ctx,event);
}

static void process_transaction_terminated(belle_sip_listener_t *l, const belle_sip_transaction_terminated_event_t *event){
	belle_sip_callbacks_t *obj=(belle_sip_callbacks_t*)l;
	if (obj->cbs.process_transaction_terminated)
		obj->cbs.process_transaction_terminated(obj->user_ctx,event);
}
static void process_auth_requested(belle_sip_listener_t *l, belle_sip_auth_event_t *event){
	belle_sip_callbacks_t *obj=(belle_sip_callbacks_t*)l;
	if (obj->cbs.process_auth_requested)
		obj->cbs.process_auth_requested(obj->user_ctx,event);
}

BELLE_SIP_DECLARE_VPTR(belle_sip_callbacks_t);

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_sip_callbacks_t,belle_sip_listener_t)
	process_dialog_terminated,
	process_io_error,
	process_request_event,
	process_response_event,
	process_timeout,
	process_transaction_terminated,
	process_auth_requested
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_sip_callbacks_t,belle_sip_listener_t);

static void belle_sip_callbacks_destroy(belle_sip_callbacks_t *obj){
	if (obj->cbs.listener_destroyed)
		obj->cbs.listener_destroyed(obj->user_ctx);
}

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_callbacks_t,belle_sip_object_t,belle_sip_callbacks_destroy,NULL,NULL,FALSE);


belle_sip_listener_t *belle_sip_listener_create_from_callbacks(const belle_sip_listener_callbacks_t *callbacks, void *user_ctx){
	belle_sip_callbacks_t *obj=belle_sip_object_new(belle_sip_callbacks_t);
	memcpy(&obj->cbs,callbacks,sizeof(belle_sip_listener_callbacks_t));
	obj->user_ctx=user_ctx;
	return BELLE_SIP_LISTENER(obj);
}

belle_sip_client_transaction_t *belle_sip_transaction_terminated_event_get_client_transaction(const belle_sip_transaction_terminated_event_t* event) {
	return event->is_server_transaction ? NULL:BELLE_SIP_CLIENT_TRANSACTION(event->transaction);
}
belle_sip_server_transaction_t *belle_sip_transaction_terminated_event_get_server_transaction(const belle_sip_transaction_terminated_event_t* event) {
	return event->is_server_transaction ? BELLE_SIP_SERVER_TRANSACTION(event->transaction):NULL;
}

belle_sip_client_transaction_t *belle_sip_timeout_event_get_client_transaction(const belle_sip_timeout_event_t* event) {
	return event->is_server_transaction ? NULL:BELLE_SIP_CLIENT_TRANSACTION(event->transaction);
}
belle_sip_server_transaction_t *belle_sip_timeout_event_get_server_transaction(const belle_sip_timeout_event_t* event) {
	return event->is_server_transaction ? BELLE_SIP_SERVER_TRANSACTION(event->transaction):NULL;
}
