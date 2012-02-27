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

#ifndef belle_sip_object_h
#define belle_sip_object_h

/*
 * typedefs, macros and functions for object definition and manipulation.
 */

#define BELLE_SIP_TYPE_ID(_type) _type##_id

typedef unsigned int belle_sip_type_id_t;

#define BELLE_SIP_DECLARE_TYPES_BEGIN(namezpace,unique_namespace_id) \
	enum namezspace##type_ids{\
		namezspace##type_id_first=unique_namespace_id,

#define BELLE_SIP_DECLARE_TYPES_END };



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


typedef void (*belle_sip_object_destroy_notify_t)(void *userpointer, belle_sip_object_t *obj_being_destroyed);
/**
 * Add a weak reference to object.
 * When object will be destroyed, then the destroy_notify callback will be called.
 * This allows another object to be informed when object is destroyed, and then possibly
 * cleanups pointer it holds to this object.
**/
belle_sip_object_t *belle_sip_object_weak_ref(void *obj, belle_sip_object_destroy_notify_t destroy_notify, void *userpointer);

/**
 * Remove a weak reference to object.
**/
void belle_sip_object_weak_unref(void *obj, belle_sip_object_destroy_notify_t destroy_notify, void *userpointer);

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

/**
 * Returns a string describing the inheritance diagram and implemented interfaces of object obj.
**/
char *belle_sip_object_describe(void *obj);

/**
 * Returns a string describing the inheritance diagram and implemented interfaces of an object given its type name.
**/
char *belle_sip_object_describe_type_from_name(const char *name);

void *belle_sip_object_cast(belle_sip_object_t *obj, belle_sip_type_id_t id, const char *castname, const char *file, int fileno);

char* belle_sip_object_to_string(belle_sip_object_t* obj);

int belle_sip_object_marshal(belle_sip_object_t* obj, char* buff,unsigned int offset,size_t buff_size);

int belle_sip_object_is_instance_of(belle_sip_object_t * obj,belle_sip_type_id_t id);

BELLE_SIP_END_DECLS

#define BELLE_SIP_CAST(obj,_type) 		((_type*)belle_sip_object_cast((belle_sip_object_t *)(obj), _type##_id, #_type, __FILE__, __LINE__))
#define BELLE_SIP_IMPLEMENTS(obj,_iface)		belle_sip_object_implements((belle_sip_object_t*)obj,_iface##_id)

#define BELLE_SIP_OBJECT(obj) BELLE_SIP_CAST(obj,belle_sip_object_t)
#define BELLE_SIP_IS_INSTANCE_OF(obj,_type) belle_sip_object_is_instance_of((belle_sip_object_t*)obj,_type##_id)

#define belle_sip_object_describe_type(type) \
	belle_sip_object_describe_type_from_name(#type)

/*
 * typedefs, macros and functions for interface definition and manipulation.
 */

#define BELLE_SIP_INTERFACE_ID(_interface) _interface##_id

typedef unsigned int belle_sip_interface_id_t;

BELLE_SIP_BEGIN_DECLS

void *belle_sip_object_interface_cast(belle_sip_object_t *obj, belle_sip_interface_id_t id, const char *castname, const char *file, int fileno);

int belle_sip_object_implements(belle_sip_object_t *obj, belle_sip_interface_id_t id);

BELLE_SIP_END_DECLS


#define BELLE_SIP_INTERFACE_METHODS_TYPE(interface_name) methods_##interface_name

#define BELLE_SIP_INTERFACE_CAST(obj,_iface) ((_iface*)belle_sip_object_interface_cast((belle_sip_object_t*)(obj),_iface##_id,#_iface,__FILE__,__LINE__))


typedef struct belle_sip_interface_desc{
	belle_sip_interface_id_t id;
	const char *ifname;
}belle_sip_interface_desc_t;

#define BELLE_SIP_DECLARE_INTERFACE_BEGIN(interface_name) \
	typedef struct struct##interface_name interface_name;\
	typedef struct struct_methods_##interface_name BELLE_SIP_INTERFACE_METHODS_TYPE(interface_name);\
	struct struct_methods_##interface_name {\
		belle_sip_interface_desc_t desc;\
		

#define BELLE_SIP_DECLARE_INTERFACE_END };


#endif

