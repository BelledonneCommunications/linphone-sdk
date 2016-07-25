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
#ifndef BELLE_SIP_MESSAGE_H
#define BELLE_SIP_MESSAGE_H

#include "belle-sip/headers.h"

#define BELLE_SIP_MESSAGE(obj)		BELLE_SIP_CAST(obj,belle_sip_message_t)
#define BELLE_SIP_REQUEST(obj)		BELLE_SIP_CAST(obj,belle_sip_request_t)
#define BELLE_SIP_RESPONSE(obj)		BELLE_SIP_CAST(obj,belle_sip_response_t)

BELLE_SIP_BEGIN_DECLS

BELLESIP_EXPORT belle_sip_message_t* belle_sip_message_parse(const char* raw);
/**
 * Parse sip message from a raw buffer
 * @param [in] buff buffer to be parsed
 * @param [in] buff_length size of the buffer to be parsed
 * @param [out] message_length number of bytes read
 * @return parsed message
 */
BELLESIP_EXPORT belle_sip_message_t* belle_sip_message_parse_raw (const char* buff, size_t buff_length,size_t* message_length );



BELLESIP_EXPORT int belle_sip_message_is_request(belle_sip_message_t *msg);
BELLESIP_EXPORT belle_sip_request_t* belle_sip_request_new(void);
BELLESIP_EXPORT belle_sip_request_t* belle_sip_request_parse(const char* raw);

BELLESIP_EXPORT belle_sip_request_t* belle_sip_request_create(belle_sip_uri_t *requri, const char* method,
                                         belle_sip_header_call_id_t *callid,
                                         belle_sip_header_cseq_t *cseq,
                                         belle_sip_header_from_t *from,
                                         belle_sip_header_to_t *to,
                                         belle_sip_header_via_t *via,
                                         int max_forwards);




BELLESIP_EXPORT belle_sip_uri_t* belle_sip_request_get_uri(const belle_sip_request_t* request);
BELLESIP_EXPORT void belle_sip_request_set_uri(belle_sip_request_t* request, belle_sip_uri_t* uri);
BELLESIP_EXPORT const char* belle_sip_request_get_method(const belle_sip_request_t* request);
BELLESIP_EXPORT void belle_sip_request_set_method(belle_sip_request_t* request,const char* method);
/**
 * Guess the origin of the received sip message from VIA header (thanks to received/rport)
 * @param req request to be analyzed
 * @ return a newly allocated uri
 * */
BELLESIP_EXPORT belle_sip_uri_t* belle_sip_request_extract_origin(const belle_sip_request_t* req);

/**
 * Clone all sip headers + body if any
 * @param  req message to be cloned
 * @return newly allocated request
 */
BELLESIP_EXPORT belle_sip_request_t * belle_sip_request_clone_with_body(const belle_sip_request_t *initial_req);

/**
 * returns an absolute uri. A header address cannot have both a sip uri and an absolute uri.
 */
BELLESIP_EXPORT belle_generic_uri_t* belle_sip_request_get_absolute_uri(const belle_sip_request_t* req);
/**
 * set an absolute uri. A header address cannot have both a sip uri and an absolute uri. This function also to uri to NULL
 */
BELLESIP_EXPORT void belle_sip_request_set_absolute_uri(belle_sip_request_t* req, belle_generic_uri_t* uri);





BELLESIP_EXPORT int belle_sip_message_is_response(const belle_sip_message_t *msg);

BELLESIP_EXPORT belle_sip_header_t *belle_sip_message_get_header(const belle_sip_message_t *msg, const char *header_name);

BELLESIP_EXPORT belle_sip_object_t *_belle_sip_message_get_header_by_type_id(const belle_sip_message_t *message, belle_sip_type_id_t id);

#define belle_sip_message_get_header_by_type(msg,header_type)\
	(header_type*)_belle_sip_message_get_header_by_type_id(BELLE_SIP_MESSAGE(msg),BELLE_SIP_TYPE_ID(header_type))

BELLESIP_EXPORT const belle_sip_list_t* belle_sip_message_get_headers(const belle_sip_message_t *message,const char* header_name);
/**
 * Get list of all headers present in the message.
 * @param message
 * @return a newly allocated list of belle_sip_header_t
 * */
BELLESIP_EXPORT belle_sip_list_t* belle_sip_message_get_all_headers(const belle_sip_message_t *message);

BELLESIP_EXPORT void belle_sip_message_add_first(belle_sip_message_t *msg, belle_sip_header_t* header);

/**
 * add an header to this message
 * @param msg
 * @param header to add, must be one of header type
 */
BELLESIP_EXPORT void belle_sip_message_add_header(belle_sip_message_t *msg, belle_sip_header_t* header);

BELLESIP_EXPORT void belle_sip_message_add_headers(belle_sip_message_t *message, const belle_sip_list_t *header_list);

BELLESIP_EXPORT void belle_sip_message_set_header(belle_sip_message_t *msg, belle_sip_header_t* header);

BELLESIP_EXPORT void belle_sip_message_remove_first(belle_sip_message_t *msg, const char *header_name);

BELLESIP_EXPORT void belle_sip_message_remove_last(belle_sip_message_t *msg, const char *header_name);

BELLESIP_EXPORT void belle_sip_message_remove_header(belle_sip_message_t *msg, const char *header_name);

BELLESIP_EXPORT void belle_sip_message_remove_header_from_ptr(belle_sip_message_t *msg, belle_sip_header_t* header);


BELLESIP_EXPORT char *belle_sip_message_to_string(belle_sip_message_t *msg);

BELLESIP_EXPORT belle_sip_body_handler_t *belle_sip_message_get_body_handler(const belle_sip_message_t *msg);

BELLESIP_EXPORT void belle_sip_message_set_body_handler(belle_sip_message_t *msg, belle_sip_body_handler_t *body_handler);

BELLESIP_EXPORT const char* belle_sip_message_get_body(belle_sip_message_t *msg);

BELLESIP_EXPORT size_t belle_sip_message_get_body_size(const belle_sip_message_t *msg);

BELLESIP_EXPORT void belle_sip_message_set_body(belle_sip_message_t *msg,const char* body,size_t size);

BELLESIP_EXPORT void belle_sip_message_assign_body(belle_sip_message_t *msg, char* body, size_t size);

BELLESIP_EXPORT int belle_sip_response_get_status_code(const belle_sip_response_t *response);
BELLESIP_EXPORT void belle_sip_response_set_status_code(belle_sip_response_t *response,int status);

BELLESIP_EXPORT const char* belle_sip_response_get_reason_phrase(const belle_sip_response_t *response);
BELLESIP_EXPORT void belle_sip_response_set_reason_phrase(belle_sip_response_t *response,const char* reason_phrase);


BELLESIP_EXPORT belle_sip_response_t *belle_sip_response_new(void);

BELLESIP_EXPORT belle_sip_response_t *belle_sip_response_create_from_request(belle_sip_request_t *req, int status_code);
/**
 * This method takes  received/rport/via value of the reponse and update the contact IP/port accordingly
 * @param response use to extract via/received/rport from top most via.
 * @param contact contact to be updated
 * @returns 0 if no error
 * */
BELLESIP_EXPORT int belle_sip_response_fix_contact(const belle_sip_response_t* response,belle_sip_header_contact_t* contact);

/**
 * Check for mandatory headers and parameters.
 * If message does not satisfy minimum requirements return FALSE, otherwise return TRUE.
**/
BELLESIP_EXPORT int belle_sip_message_check_headers(const belle_sip_message_t* message);

/**
 * check uri components of headers and req uri.
 * return 0 if not compliant
 * */
BELLESIP_EXPORT int belle_sip_request_check_uris_components(const belle_sip_request_t* request);


BELLE_SIP_END_DECLS

#endif

