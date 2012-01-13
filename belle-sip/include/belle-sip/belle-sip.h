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
#ifndef BELLE_SIP_H
#define BELLE_SIP_H

#include <stdlib.h>

#ifdef __cplusplus
#define BELLE_SIP_BEGIN_DECLS		extern "C"{
#define BELLE_SIP_END_DECLS		}
#else
#define BELLE_SIP_BEGIN_DECLS
#define BELLE_SIP_END_DECLS
#endif

#define BELLE_SIP_TYPE_ID(_type) _type##_id

/**
 * This enum declares all object types used in belle-sip (see belle_sip_object_t)
**/
typedef enum belle_sip_type_id{
	belle_sip_type_id_first=1,
	BELLE_SIP_TYPE_ID(belle_sip_stack_t),
	BELLE_SIP_TYPE_ID(belle_sip_listening_point_t),
	BELLE_SIP_TYPE_ID(belle_sip_datagram_listening_point_t),
	BELLE_SIP_TYPE_ID(belle_sip_udp_listening_point_t),
	BELLE_SIP_TYPE_ID(belle_sip_channel_t),
	BELLE_SIP_TYPE_ID(belle_sip_udp_channel_t),
	BELLE_SIP_TYPE_ID(belle_sip_provider_t),
	BELLE_SIP_TYPE_ID(belle_sip_main_loop_t),
	BELLE_SIP_TYPE_ID(belle_sip_source_t),
	BELLE_SIP_TYPE_ID(belle_sip_resolver_context_t),
	BELLE_SIP_TYPE_ID(belle_sip_transaction_t),
	BELLE_SIP_TYPE_ID(belle_sip_server_transaction_t),
	BELLE_SIP_TYPE_ID(belle_sip_client_transaction_t),
	BELLE_SIP_TYPE_ID(belle_sip_dialog_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_address_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_contact_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_from_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_to_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_via_t),
	BELLE_SIP_TYPE_ID(belle_sip_uri_t),
	BELLE_SIP_TYPE_ID(belle_sip_message_t),
	BELLE_SIP_TYPE_ID(belle_sip_request_t),
	BELLE_SIP_TYPE_ID(belle_sip_response_t),
	BELLE_SIP_TYPE_ID(belle_sip_object_t),
	BELLE_SIP_TYPE_ID(belle_sip_parameters_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_call_id_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_cseq_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_content_type_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_route_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_record_route_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_user_agent_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_content_length_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_extension_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_authorization_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_www_authenticate_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_proxy_authorization_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_max_forwards_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_expires_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_allow_t),
	BELLE_SIP_TYPE_ID(belle_sdp_attribute_t),
	BELLE_SIP_TYPE_ID(belle_sdp_bandwidth_t),
	BELLE_SIP_TYPE_ID(belle_sdp_connection_t),
	BELLE_SIP_TYPE_ID(belle_sdp_email_t),
	BELLE_SIP_TYPE_ID(belle_sdp_info_t),
	BELLE_SIP_TYPE_ID(belle_sdp_key_t),
	BELLE_SIP_TYPE_ID(belle_sdp_media_t),
	BELLE_SIP_TYPE_ID(belle_sdp_media_description_t),
	BELLE_SIP_TYPE_ID(belle_sdp_origin_t),
	BELLE_SIP_TYPE_ID(belle_sdp_phone_t),
	BELLE_SIP_TYPE_ID(belle_sdp_repeate_time_t),
	BELLE_SIP_TYPE_ID(belle_sdp_session_description_t),
	BELLE_SIP_TYPE_ID(belle_sdp_session_name_t),
	BELLE_SIP_TYPE_ID(belle_sdp_time_t),
	BELLE_SIP_TYPE_ID(belle_sdp_time_description_t),
	BELLE_SIP_TYPE_ID(belle_sdp_uri_t),
	BELLE_SIP_TYPE_ID(belle_sdp_version_t),
	BELLE_SIP_TYPE_ID(belle_sdp_base_description_t),
	BELLE_SIP_TYPE_ID(belle_sdp_mime_parameter_t),
	belle_sip_type_id_end
}belle_sip_type_id_t;


/**
 * belle_sip_object_t is the base object.
 * It is the base class for all belle sip non trivial objects.
 * It owns a reference count which allows to trigger the destruction of the object when the last
 * user of it calls belle_sip_object_unref().
**/

typedef struct _belle_sip_object belle_sip_object_t;

BELLE_SIP_BEGIN_DECLS

int belle_sip_object_is_unowed(const belle_sip_object_t *obj);

/**
 * Increments reference counter, which prevents the object from being destroyed.
 * If the object is initially unowed, this acquires the first reference.
**/
belle_sip_object_t * belle_sip_object_ref(void *obj);

/**
 * Decrements the reference counter. When it drops to zero, the object is destroyed.
**/
void belle_sip_object_unref(void *obj);
/**
 * Set object name.
**/
void belle_sip_object_set_name(belle_sip_object_t *obj,const char* name);
/**
 * Get object name.
**/
const char* belle_sip_object_get_name(belle_sip_object_t *obj);

belle_sip_object_t *belle_sip_object_clone(const belle_sip_object_t *obj);

/**
 * Delete the object: this function is intended for unowed object, that is objects
 * that were created with a 0 reference count. For all others, use belle_sip_object_unref().
**/
void belle_sip_object_delete(void *obj);

void *belle_sip_object_cast(belle_sip_object_t *obj, belle_sip_type_id_t id, const char *castname, const char *file, int fileno);

char* belle_sip_object_to_string(belle_sip_object_t* obj);

unsigned int belle_sip_object_is_instance_of(belle_sip_object_t * obj,belle_sip_type_id_t id);

void *belle_sip_malloc(size_t size);
void *belle_sip_malloc0(size_t size);
void *belle_sip_realloc(void *ptr, size_t size);
void belle_sip_free(void *ptr);
char * belle_sip_strdup(const char *s);

BELLE_SIP_END_DECLS

#define BELLE_SIP_CAST(obj,_type) 		((_type*)belle_sip_object_cast((belle_sip_object_t *)(obj), _type##_id, #_type, __FILE__, __LINE__))
#define BELLE_SIP_OBJECT(obj) BELLE_SIP_CAST(obj,belle_sip_object_t)
#define BELLE_SIP_IS_INSTANCE_OF(obj,_type) belle_sip_object_is_instance_of(obj,_type##_id)


typedef struct belle_sip_listening_point belle_sip_listening_point_t;
typedef struct belle_sip_stack belle_sip_stack_t;
typedef struct belle_sip_provider belle_sip_provider_t;
typedef struct belle_sip_listener belle_sip_listener_t;
typedef struct belle_sip_dialog belle_sip_dialog_t;

#include "belle-sip/utils.h"
#include "belle-sip/list.h"
#include "belle-sip/mainloop.h"
#include "belle-sip/uri.h"
#include "belle-sip/headers.h"
#include "belle-sip/parameters.h"
#include "belle-sip/message.h"
#include "belle-sip/transaction.h"
#include "belle-sip/dialog.h"
#include "belle-sip/sipstack.h"
#include "belle-sip/listeningpoint.h"
#include "belle-sip/provider.h"
#include "belle-sip/listener.h"

#undef TRUE
#define TRUE 1


#undef FALSE
#define FALSE 0


#define BELLE_SIP_POINTER_TO_INT(p)	((int)(long)(p))
#define BELLE_SIP_INT_TO_POINTER(i)	((void*)(long)(i))

#endif

