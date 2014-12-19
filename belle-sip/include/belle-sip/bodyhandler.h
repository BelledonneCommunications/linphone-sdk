/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2014  Belledonne Communications SARL

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


#ifndef belle_sip_body_handler_h
#define belle_sip_body_handler_h


BELLE_SIP_BEGIN_DECLS

#define BELLE_SIP_BODY_HANDLER(obj)	BELLE_SIP_CAST(obj,belle_sip_body_handler_t)
/*
 * Body handler base class.
**/

typedef void (*belle_sip_body_handler_progress_callback_t)(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, void *user_data, size_t transfered, size_t expected_total);

BELLESIP_EXPORT void belle_sip_body_handler_add_header(belle_sip_body_handler_t *obj, belle_sip_header_t *header);
BELLESIP_EXPORT size_t belle_sip_body_handler_get_size(const belle_sip_body_handler_t *obj);
BELLESIP_EXPORT void belle_sip_body_handler_set_size(belle_sip_body_handler_t *obj, size_t size);
BELLESIP_EXPORT size_t belle_sip_body_handler_get_transfered_size(const belle_sip_body_handler_t *obj);


/*
 * body handler that read/write from a memory buffer.
**/

#define BELLE_SIP_MEMORY_BODY_HANDLER(obj)	BELLE_SIP_CAST(obj,belle_sip_memory_body_handler_t)

BELLESIP_EXPORT belle_sip_memory_body_handler_t *belle_sip_memory_body_handler_new(belle_sip_body_handler_progress_callback_t progress_cb, void *data);
BELLESIP_EXPORT belle_sip_memory_body_handler_t *belle_sip_memory_body_handler_new_copy_from_buffer(const void *buffer, size_t bufsize, 
							belle_sip_body_handler_progress_callback_t cb, void *user_data);
BELLESIP_EXPORT belle_sip_memory_body_handler_t *belle_sip_memory_body_handler_new_from_buffer(void *buffer, size_t bufsize,
						belle_sip_body_handler_progress_callback_t cb, void *user_data);

BELLESIP_EXPORT const void *belle_sip_memory_body_handler_get_buffer(const belle_sip_memory_body_handler_t *obj);

/*
 * body handler that get/puts data from application.
**/

#define BELLE_SIP_USER_BODY_HANDLER(obj)	BELLE_SIP_CAST(obj,belle_sip_user_body_handler_t)

typedef void (*belle_sip_user_body_handler_recv_callback_t)(belle_sip_user_body_handler_t *obj, belle_sip_message_t *msg, void *user_data, size_t offset, const uint8_t* buffer, size_t size);

typedef int (*belle_sip_user_body_handler_send_callback_t)(belle_sip_user_body_handler_t *obj, belle_sip_message_t *msg, void *user_data, size_t offset, uint8_t* buffer, size_t *size);

BELLESIP_EXPORT belle_sip_user_body_handler_t *belle_sip_user_body_handler_new(
	size_t total_size,
	belle_sip_body_handler_progress_callback_t progress_cb,
	belle_sip_user_body_handler_recv_callback_t recv_cb,
	belle_sip_user_body_handler_send_callback_t send_cb,
	void *data);


/**
 * Body handler that gets/puts data from/to a file.
**/

#define BELLE_SIP_FILE_BODY_HANDLER(obj)	BELLE_SIP_CAST(obj, belle_sip_file_body_handler_t)

BELLESIP_EXPORT belle_sip_file_body_handler_t *belle_sip_file_body_handler_new(const char *filepath, belle_sip_body_handler_progress_callback_t progress_cb, void *data);


/*
 * Multipart body handler
 */
#define BELLE_SIP_MULTIPART_BODY_HANDLER(obj)	BELLE_SIP_CAST(obj,belle_sip_multipart_body_handler_t)

BELLESIP_EXPORT belle_sip_multipart_body_handler_t *belle_sip_multipart_body_handler_new(belle_sip_body_handler_progress_callback_t progress_cb, void *data, belle_sip_body_handler_t *first_part);
BELLESIP_EXPORT void belle_sip_multipart_body_handler_add_part(belle_sip_multipart_body_handler_t *obj, belle_sip_body_handler_t *part);

BELLE_SIP_END_DECLS

#endif
