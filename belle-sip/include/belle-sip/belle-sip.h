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

#define BELLE_SIP_TYPE_ID(_type) _type##_id

typedef enum belle_sip_type_id{
	belle_sip_type_id_first=1,
	BELLE_SIP_TYPE_ID(belle_sip_transaction_t),
	BELLE_SIP_TYPE_ID(belle_sip_server_transaction_t),
	BELLE_SIP_TYPE_ID(belle_sip_client_transaction_t),
	BELLE_SIP_TYPE_ID(belle_sip_transport_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_address_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_contact_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_from_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_to_t),
	BELLE_SIP_TYPE_ID(belle_sip_header_via_t),
	BELLE_SIP_TYPE_ID(belle_sip_uri_t),
	BELLE_SIP_TYPE_ID(belle_sip_object_t),
	BELLE_SIP_TYPE_ID(belle_sip_parameters_t),
	belle_sip_type_id_end
}belle_sip_type_id_t;


/**
 * belle_sip_object_t is the base object.
 * It is the base class for all belle sip non trivial objects.
 * It owns a reference count which allows to trigger the destruction of the object when the last
 * user of it calls belle_sip_object_unref().
**/

typedef struct _belle_sip_object belle_sip_object_t;

int belle_sip_object_is_unowed(const belle_sip_object_t *obj);

/**
 * Increments reference counter, which prevents the object from being destroyed.
 * If the object is initially unowed, this acquires the first reference.
**/
#define belle_sip_object_ref(obj) _belle_sip_object_ref((belle_sip_object_t*)obj)
belle_sip_object_t * _belle_sip_object_ref(belle_sip_object_t *obj);

/**
 * Decrements the reference counter. When it drops to zero, the object is destroyed.
**/
#define belle_sip_object_unref(obj) _belle_sip_object_unref((belle_sip_object_t*)obj)
void _belle_sip_object_unref(belle_sip_object_t *obj);

/**
 * Destroy the object: this function is intended for unowed object, that is objects
 * that were created with a 0 reference count.
**/
#define belle_sip_object_destroy(obj) _belle_sip_object_destroy((belle_sip_object_t*)obj)
void _belle_sip_object_destroy(belle_sip_object_t *obj);

void *belle_sip_object_cast(belle_sip_object_t *obj, belle_sip_type_id_t id, const char *castname, const char *file, int fileno);

#define BELLE_SIP_CAST(obj,_type) (_type*)belle_sip_object_cast((belle_sip_object_t *)(obj), _type##_id, #_type, __FILE__, __LINE__)
#define BELLE_SIP_OBJECT(obj) BELLE_SIP_CAST(obj,belle_sip_object_t)

#include "belle-sip/list.h"
#include "belle-sip/mainloop.h"
#include "belle-sip/uri.h"
#include "belle-sip/headers.h"
#include "belle-sip/parameters.h"
#include "belle-sip/message.h"
#include "belle-sip/transaction.h"
#undef TRUE
#define TRUE 1


#undef FALSE
#define FALSE 0

#endif
