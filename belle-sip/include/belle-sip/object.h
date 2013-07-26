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

#include "belle-sip/defs.h"
#include "belle-sip/utils.h"

/*
 * typedefs, macros and functions for object definition and manipulation.
 */

#define BELLE_SIP_TYPE_ID(_type) _type##_id

typedef unsigned int belle_sip_type_id_t;

#define BELLE_SIP_DECLARE_TYPES_BEGIN(namezpace,unique_namespace_id) \
	enum namezpace##type_ids{\
		namezpace##type_id_first=unique_namespace_id,

#define BELLE_SIP_DECLARE_TYPES_END };

#define BELLE_SIP_OBJECT_VPTR_NAME(object_type)	object_type##_vptr

#define BELLE_SIP_OBJECT_VPTR_TYPE(object_type)	object_type##_vptr_t

#define BELLE_SIP_DECLARE_VPTR(object_type) \
	typedef belle_sip_object_vptr_t BELLE_SIP_OBJECT_VPTR_TYPE(object_type);\
	BELLESIP_VAR_EXPORT BELLE_SIP_OBJECT_VPTR_TYPE(object_type) BELLE_SIP_OBJECT_VPTR_NAME(object_type);

#define BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(object_type, parent_type) \
	typedef struct object_type##_vptr_struct BELLE_SIP_OBJECT_VPTR_TYPE(object_type);\
	extern BELLE_SIP_OBJECT_VPTR_TYPE(object_type) BELLE_SIP_OBJECT_VPTR_NAME(object_type);\
	struct object_type##_vptr_struct{\
		BELLE_SIP_OBJECT_VPTR_TYPE(parent_type) base;

#define BELLE_SIP_DECLARE_CUSTOM_VPTR_END };

#define BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(object_type) \
	BELLE_SIP_OBJECT_VPTR_TYPE(object_type) BELLE_SIP_OBJECT_VPTR_NAME(object_type)


#define BELLE_SIP_VPTR_INIT(object_type,parent_type,unowned) \
		BELLE_SIP_TYPE_ID(object_type), \
		#object_type,\
		unowned,\
		(belle_sip_object_vptr_t*)&BELLE_SIP_OBJECT_VPTR_NAME(parent_type), \
		(belle_sip_interface_desc_t**)object_type##interfaces_table


#define BELLE_SIP_INSTANCIATE_VPTR(object_type,parent_type,destroy,clone,marshal,unowned) \
		BELLE_SIP_OBJECT_VPTR_TYPE(object_type) BELLE_SIP_OBJECT_VPTR_NAME(object_type)={ \
		BELLE_SIP_VPTR_INIT(object_type,parent_type,unowned), \
		(belle_sip_object_destroy_t)destroy,	\
		(belle_sip_object_clone_t)clone,	\
		(belle_sip_object_marshal_t)marshal\
		}

/**
 * belle_sip_object_t is the base object.
 * It is the base class for all belle sip non trivial objects.
 * It owns a reference count which allows to trigger the destruction of the object when the last
 * user of it calls belle_sip_object_unref().
 * 
 * About object lifecycle<br>
 * In belle-sip, objects can be, depending on their types, initially owned, that there are created with a ref count of 1, or
 * initially unowned, that is with reference count of 0. Such objets are also referred as "floating object". They are automatically destroyed
 * by the main loop iteration, so a floating object can be seen as a temporary object, until someones calls belle_sip_object_ref() on it.
 * 
 * In order to know whether a kind of object is initially owned or initially unowned, you can use the test program tester/belle_sip_object_destribe.
 * This tool gives the hierarchy and properties of the object type whose name is supplied in argument. For example:
 * 
 * <pre>./tester/belle_sip_object_describe belle_sip_request_t</pre>
 * 
 * The object memory management depends slightly on whether an object type is created initially owned or not.
 * In order not to be lost and make memory fault or leaks, consider the following rules:
 * 
 * When an object is of type initially unowned:
 * * call belle_sip_object_ref() on it only if you need a pointer to this object to be used outside the scope of the current function.
 * * call belle_sip_object_unref() on it only if you previously called belle_sip_object_ref().
 * 
 * When an object is of type initially owned:
 * * you can safely store its pointer.
 * * use belle_sip_object_unref() when you no longer need it.
 * 
 * Also, keep in mind that most objects of belle-sip are initially unowned, especially 
 * * all objects who are usually required to be used inside another object (for example: an URI is part of a from header, a contact header is part of a message)
 * * all objects whose lifecyle is maintained by the stack: transactions, dialogs.
 * 
 * On the contrary, top level objects whose lifecyle belongs only to the application are initially owned:
 * * belle_sip_provider_t, belle_sip_stack_t, belle_sip_source_t.
 * 
 * Internally, belle-sip objects containing pointers to other objects must take a reference count on the other objects they hold; and leave this reference
 * when they no longer need it. This rule must be strictly followed by developers doing things inside belle-sip.
**/
typedef struct _belle_sip_object belle_sip_object_t;
		

typedef void (*belle_sip_object_destroy_t)(belle_sip_object_t*);
typedef void (*belle_sip_object_clone_t)(belle_sip_object_t* obj, const belle_sip_object_t *orig);
typedef int (*belle_sip_object_marshal_t)(belle_sip_object_t* obj, char* buff, size_t buff_size, size_t *offset);

struct _belle_sip_object_vptr{
	belle_sip_type_id_t id;
	const char *type_name;
	int initially_unowned;
	struct _belle_sip_object_vptr *parent;
	struct belle_sip_interface_desc **interfaces; /*NULL terminated table of */
	belle_sip_object_destroy_t destroy;
	belle_sip_object_clone_t clone;
	belle_sip_object_marshal_t marshal;
	int tostring_bufsize_hint; /*optimization: you can suggest here the typical size for a to_string() result.*/
};

typedef struct _belle_sip_object_vptr belle_sip_object_vptr_t;

struct _belle_sip_object{
	belle_sip_object_vptr_t *vptr;
	size_t size;
	int ref;
	char* name;
	struct weak_ref *weak_refs;
	struct belle_sip_object_pool *pool;
	struct _belle_sip_list *pool_iterator;
};


BELLE_SIP_BEGIN_DECLS

BELLESIP_VAR_EXPORT belle_sip_object_vptr_t belle_sip_object_t_vptr;		


BELLESIP_EXPORT belle_sip_object_t * _belle_sip_object_new(size_t objsize, belle_sip_object_vptr_t *vptr);

#define belle_sip_object_new(_type) (_type*)_belle_sip_object_new(sizeof(_type),(belle_sip_object_vptr_t*)&BELLE_SIP_OBJECT_VPTR_NAME(_type))


/**
 * Activates checks on object marshalling.
 * Useful for debug purposes.
 * @param enable TRUE to enable, FALSE to disable.
**/
BELLESIP_EXPORT void belle_sip_object_enable_marshal_check(int enable);

int belle_sip_object_is_unowed(const belle_sip_object_t *obj);

/**
 * Increments reference counter, which prevents the object from being destroyed.
 * If the object is initially unowed, this acquires the first reference.
 * 
**/
BELLESIP_EXPORT belle_sip_object_t * belle_sip_object_ref(void *obj);

/*#define BELLE_SIP_REF(object,type) (type*)belle_sip_object_ref(object);*/
/**
 * Decrements the reference counter. When it drops to zero, the object is destroyed.
**/
BELLESIP_EXPORT void belle_sip_object_unref(void *obj);


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

/*copy the content of ref object to new object, for the part they have in common in their inheritence diagram*/
void _belle_sip_object_copy(belle_sip_object_t *newobj, const belle_sip_object_t *ref);

/**
 * Clone an object.
 * 
 * This clone function makes a deep copy of all object internal structure, so that the new object and the reference object have no dependencies at all.
 * 
**/
BELLESIP_EXPORT belle_sip_object_t *belle_sip_object_clone(const belle_sip_object_t *obj);

/**
 * Same as #belle_sip_object_clone but with ref count set to 1
 *
**/
belle_sip_object_t *belle_sip_object_clone_and_ref(const belle_sip_object_t *obj);


/**
 * Returns a string describing the inheritance diagram and implemented interfaces of object obj.
**/
char *belle_sip_object_describe(void *obj);

/**
 * Returns a string describing the inheritance diagram and implemented interfaces of an object given its type name.
**/
char *belle_sip_object_describe_type_from_name(const char *name);

BELLESIP_EXPORT void *belle_sip_object_cast(belle_sip_object_t *obj, belle_sip_type_id_t id, const char *castname, const char *file, int fileno);

/**
 * Returns a newly allocated string representing the object.
 * WHen the object is a sip header, uri or message, this is the textual representation of the header, uri or message.
 * This function internally calls belle_sip_object_marshal().
**/
BELLESIP_EXPORT char* belle_sip_object_to_string(void* obj);

/**
 * Writes a string representation of the object into the supplied buffer.
 * Same as belle_sip_object_to_string(), but without allocating space for the output string.
**/
BELLESIP_EXPORT belle_sip_error_code belle_sip_object_marshal(belle_sip_object_t* obj, char* buff, size_t buff_size, size_t *offset);

/* use BELLE_SIP_OBJECT_IS_INSTANCE_OF macro(), this function is for use by the macro only*/
BELLESIP_EXPORT int _belle_sip_object_is_instance_of(belle_sip_object_t * obj,belle_sip_type_id_t id);

BELLE_SIP_END_DECLS

#define BELLE_SIP_CAST(obj,_type) 		((_type*)belle_sip_object_cast((belle_sip_object_t *)(obj), _type##_id, #_type, __FILE__, __LINE__))

#define BELLE_SIP_OBJECT(obj) BELLE_SIP_CAST(obj,belle_sip_object_t)
#define BELLE_SIP_OBJECT_IS_INSTANCE_OF(obj,_type) _belle_sip_object_is_instance_of((belle_sip_object_t*)obj,_type##_id)

/*deprecated*/
#define BELLE_SIP_IS_INSTANCE_OF(obj,_type)	BELLE_SIP_OBJECT_IS_INSTANCE_OF(obj,_type)

#define belle_sip_object_describe_type(type) \
	belle_sip_object_describe_type_from_name(#type)

/*
 * typedefs, macros and functions for interface definition and manipulation.
 */

#define BELLE_SIP_INTERFACE_ID(_interface) _interface##_id

typedef unsigned int belle_sip_interface_id_t;

BELLE_SIP_BEGIN_DECLS

BELLESIP_EXPORT void *belle_sip_object_interface_cast(belle_sip_object_t *obj, belle_sip_interface_id_t id, const char *castname, const char *file, int fileno);

int belle_sip_object_implements(belle_sip_object_t *obj, belle_sip_interface_id_t id);

BELLE_SIP_END_DECLS


#define BELLE_SIP_INTERFACE_METHODS_TYPE(interface_name) methods_##interface_name

#define BELLE_SIP_INTERFACE_CAST(obj,_iface) ((_iface*)belle_sip_object_interface_cast((belle_sip_object_t*)(obj),_iface##_id,#_iface,__FILE__,__LINE__))

#define BELLE_SIP_IMPLEMENTS(obj,_iface)		belle_sip_object_implements((belle_sip_object_t*)obj,_iface##_id)


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

#define BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(object_type,interface_name) \
	static BELLE_SIP_INTERFACE_METHODS_TYPE(interface_name)  methods_##object_type##_##interface_name={\
		{ BELLE_SIP_INTERFACE_ID(interface_name),\
		#interface_name },

#define BELLE_SIP_IMPLEMENT_INTERFACE_END };

#define BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(object_type)\
	static belle_sip_interface_desc_t * object_type##interfaces_table[]={\
		NULL \
	}

#define BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(object_type,iface1) \
	static belle_sip_interface_desc_t * object_type##interfaces_table[]={\
		(belle_sip_interface_desc_t*)&methods_##object_type##_##iface1, \
		NULL \
	}

#define BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_2(object_type,iface1,iface2) \
	static belle_sip_interface_desc_t * object_type##interfaces_table[]={\
		(belle_sip_interface_desc_t*)&methods_##object_type##_##iface1, \
		(belle_sip_interface_desc_t*)&methods_##object_type##_##iface2, \
		NULL \
	}


/**
 * Object holding unowned objects - used as a kind of garbage collector for temporary objects.
**/
typedef struct belle_sip_object_pool belle_sip_object_pool_t;

BELLE_SIP_BEGIN_DECLS

/**
 * Push a new object pool for use for creation of new objects.
 * When no longer needed, this pool can be destroyed with belle_sip_object_unref().
**/
BELLESIP_EXPORT belle_sip_object_pool_t * belle_sip_object_pool_push(void);

belle_sip_object_pool_t * belle_sip_object_pool_get_current();
int belle_sip_object_pool_cleanable(belle_sip_object_pool_t *pool);
void belle_sip_object_pool_clean(belle_sip_object_pool_t *obj);

BELLE_SIP_END_DECLS

#endif

