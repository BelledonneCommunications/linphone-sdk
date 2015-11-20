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
#ifndef BELLE_HTTP_MESSAGE_H
#define BELLE_HTTP_MESSAGE_H

#include "belle-sip/generic-uri.h"

#define BELLE_HTTP_REQUEST(obj)		BELLE_SIP_CAST(obj,belle_http_request_t)
#define BELLE_HTTP_RESPONSE(obj)	BELLE_SIP_CAST(obj,belle_http_response_t)

BELLE_SIP_BEGIN_DECLS

/**
 * Create an http request.
 * @param method
 * @param uri the http uri
 * @param ... optional list of belle_sip_header_t* to be included in the request, ending with NULL.
 */
BELLESIP_EXPORT belle_http_request_t *belle_http_request_create(const char *method, belle_generic_uri_t *uri, ...);
BELLESIP_EXPORT belle_http_request_t* belle_http_request_new(void);
BELLESIP_EXPORT belle_http_request_t* belle_http_request_parse(const char* raw);

BELLESIP_EXPORT int belle_http_request_is_cancelled(const belle_http_request_t *req);
BELLESIP_EXPORT void belle_http_request_cancel(belle_http_request_t *req);

BELLESIP_EXPORT belle_generic_uri_t* belle_http_request_get_uri(const belle_http_request_t* request);
BELLESIP_EXPORT void belle_http_request_set_uri(belle_http_request_t* request, belle_generic_uri_t* uri);
BELLESIP_EXPORT const char* belle_http_request_get_method(const belle_http_request_t* request);
BELLESIP_EXPORT void belle_http_request_set_method(belle_http_request_t* request,const char* method);

 BELLESIP_EXPORT belle_http_response_t *belle_http_request_get_response(belle_http_request_t *req);

/**
 * http response
 * */
BELLESIP_EXPORT int belle_http_response_get_status_code(const belle_http_response_t *response);
BELLESIP_EXPORT void belle_http_response_set_status_code(belle_http_response_t *response,int status);

BELLESIP_EXPORT const char* belle_http_response_get_reason_phrase(const belle_http_response_t *response);
BELLESIP_EXPORT void belle_http_response_set_reason_phrase(belle_http_response_t *response,const char* reason_phrase);


BELLESIP_EXPORT belle_http_response_t *belle_http_response_new(void);


BELLE_SIP_END_DECLS

#endif
