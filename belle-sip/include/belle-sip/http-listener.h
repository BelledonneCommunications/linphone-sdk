
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


#ifndef belle_http_listener_h
#define belle_http_listener_h

struct belle_http_response_event{
	belle_sip_object_t *source;
	belle_http_request_t *request;
	belle_http_response_t *response;
};

typedef struct belle_http_response_event belle_http_response_event_t;


#define BELLE_HTTP_INTERFACE_FUNCS(argT) \
	void (*process_response_headers)(argT *user_ctx, const belle_http_response_event_t *event); \
	void (*process_response)(argT *user_ctx, const belle_http_response_event_t *event); \
	void (*process_io_error)(argT *user_ctx, const belle_sip_io_error_event_t *event); \
	void (*process_timeout)(argT *user_ctx, const belle_sip_timeout_event_t *event); \
	void (*process_auth_requested)(argT *user_ctx, belle_sip_auth_event_t *event);

BELLE_SIP_DECLARE_INTERFACE_BEGIN(belle_http_request_listener_t)
	BELLE_HTTP_INTERFACE_FUNCS(belle_http_request_listener_t)
BELLE_SIP_DECLARE_INTERFACE_END

struct belle_http_request_listener_callbacks{
	BELLE_HTTP_INTERFACE_FUNCS(void)
	void (*listener_destroyed)(void *user_ctx);
};

typedef struct belle_http_request_listener_callbacks belle_http_request_listener_callbacks_t;

#define BELLE_HTTP_REQUEST_LISTENER(obj) BELLE_SIP_INTERFACE_CAST(obj,belle_http_request_listener_t)

BELLE_SIP_BEGIN_DECLS
/**
 * Creates an object implementing the belle_http_request_listener_t interface.
 * This object passes the events to the callbacks, providing also the user context.
**/
BELLESIP_EXPORT belle_http_request_listener_t *belle_http_request_listener_create_from_callbacks(const belle_http_request_listener_callbacks_t *callbacks, void *user_ctx);

BELLE_SIP_END_DECLS

#endif
