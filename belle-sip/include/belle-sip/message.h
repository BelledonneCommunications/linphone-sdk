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
#ifndef BELLE_SIP_MESSAGE_H
#define BELLE_SIP_MESSAGE_H

typedef struct _belle_sip_message belle_sip_message_t;
typedef struct _belle_sip_request belle_sip_request_t;
typedef struct _belle_sip_response belle_sip_response_t;

#define BELLE_SIP_MESSAGE(obj)			BELLE_SIP_CAST(obj,belle_sip_message_t)
#define BELLE_SIP_REQUEST(obj)			BELLE_SIP_CAST(obj,belle_sip_request_t)
#define BELLE_SIP_RESPONSE(obj)		BELLE_SIP_CAST(obj,belle_sip_response_t)

BELLE_SIP_BEGIN_DECLS

belle_sip_message_t* belle_sip_message_parse(const char* raw);


int belle_sip_message_is_request(belle_sip_message_t *msg);
belle_sip_request_t* belle_sip_request_new();
belle_sip_request_t* belle_sip_request_parse(const char* raw);






belle_sip_uri_t* belle_sip_request_get_uri(belle_sip_request_t* request);
void belle_sip_request_set_uri(belle_sip_request_t* request,belle_sip_uri_t* uri);
const char* belle_sip_request_get_method(belle_sip_request_t* request);
void belle_sip_request_set_method(belle_sip_request_t* request,const char* method);

int belle_sip_message_is_response(belle_sip_message_t *msg);

belle_sip_header_t *belle_sip_message_get_header(belle_sip_message_t *msg, const char *header_name);

/**
 * add an header to this message
 * @param msg
 * @param header to add, must be one of header type
 */
void belle_sip_message_add_header(belle_sip_message_t *msg, belle_sip_header_t* header);

char *belle_sip_message_to_string(belle_sip_message_t *msg);

int belle_sip_response_get_status_code(const belle_sip_response_t *response);

BELLE_SIP_END_DECLS

#endif

