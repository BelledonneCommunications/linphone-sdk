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

#ifndef belle_sip_object_h
#define belle_sip_object_h

#include "belle-sip/defs.h"
#include "belle-sip/utils.h"
#include "belle-sip/list.h"

/*
 * typedefs, macros and functions for object definition and manipulation.
 */

#define BELLE_SIP_DEFAULT_BUFSIZE_HINT 0

#define BELLE_SIP_TYPE_ID(_type) _type##_id

typedef unsigned int belle_sip_type_id_t;

#define BELLE_SIP_DECLARE_TYPES_BEGIN(namezpace,unique_namespace_id) \
	enum namezpace##type_ids{\
		namezpace##type_id_first=unique_namespace_id,

#define BELLE_SIP_DECLARE_TYPES_END };

#define BELLE_SIP_OBJECT_VPTR_NAME(object_type)	object_type##_vptr

#define BELLE_SIP_OBJECT_GET_VPTR_FUNC(object_type) object_type##_vptr_get

#define BELLE_SIP_OBJECT_VPTR_TYPE(object_type)	object_type##_vptr_t

#define BELLE_SIP_DECLARE_VPTR(object_type) \
	typedef belle_sip_object_vptr_t BELLE_SIP_OBJECT_VPTR_TYPE(object_type);\
	BELLESIP_EXPORT BELLE_SIP_OBJECT_VPTR_TYPE(object_type) * BELLE_SIP_OBJECT_GET_VPTR_FUNC(object_type)(void);

#define BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(object_type, parent_type) \
	typedef struct object_type##_vptr_struct BELLE_SIP_OBJECT_VPTR_TYPE(object_type);\
	BELLESIP_EXPORT BELLE_SIP_OBJECT_VPTR_TYPE(object_type) * BELLE_SIP_OBJECT_GET_VPTR_FUNC(object_type)(void); \
	struct object_type##_vptr_struct{\
		BELLE_SIP_OBJECT_VPTR_TYPE(parent_type) base;

#define BELLE_SIP_DECLARE_CUSTOM_VPTR_END };

#define BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(object_type) \
	extern BELLE_SIP_OBJECT_VPTR_TYPE(object_type) BELLE_SIP_OBJECT_VPTR_NAME(object_type);\
	BELLE_SIP_OBJECT_VPTR_TYPE(object_type) * BELLE_SIP_OBJECT_GET_VPTR_FUNC(object_type)(void){\
		return &BELLE_SIP_OBJECT_VPTR_NAME(object_type); \
	}\
	BELLE_SIP_OBJECT_VPTR_TYPE(object_type) BELLE_SIP_OBJECT_VPTR_NAME(object_type)={

#define BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END };

#define BELLE_SIP_VPTR_INIT(object_type,parent_type,unowned) \
		BELLE_SIP_TYPE_ID(object_type), \
		sizeof(object_type), \
		#object_type,\
		unowned,\
		(belle_sip_object_get_vptr_t)BELLE_SIP_OBJECT_GET_VPTR_FUNC(parent_type), \
		(belle_sip_interface_desc_t**)object_type##interfaces_table


#define BELLE_SIP_INSTANCIATE_VPTR(object_type,parent_type,destroy,clone,marshal,unowned) \
		static BELLE_SIP_OBJECT_VPTR_TYPE(object_type) BELLE_SIP_OBJECT_VPTR_NAME(object_type)={ \
		BELLE_SIP_VPTR_INIT(object_type,parent_type,unowned), \
		(belle_sip_object_destroy_t)destroy,	\
		(belle_sip_object_clone_t)clone,	\
		(belle_sip_object_marshal_t)marshal,\
		BELLE_SIP_DEFAULT_BUFSIZE_HINT\
		}; \
		BELLE_SIP_OBJECT_VPTR_TYPE(object_type) * BELLE_SIP_OBJECT_GET_VPTR_FUNC(object_type)(void){\
			return &BELLE_SIP_OBJECT_VPTR_NAME(object_type); \
		}

/**
 * belle_sip_object_t is the base object.
 * It is the base class for all belle sip non trivial objects.
 * It owns a reference count which allows to trigger the destruction of the object when the last
 * user of it calls belle_sip_object_unref().
 *
 * It contains a generic data store that allows users to store named data in it and retrieve them afterwards.
 *
 * About object lifecycle<br>
 * In belle-sip, objects can be, depending on their types, initially owned, that there are created with a ref count of 1, or
 * initially unowned, that is with reference count of 0. Such objets are also referred as "floating object". They are automatically destroyed
 * by the main loop iteration, so a floating object can be seen as a temporary object, until someones calls belle_sip_object_ref() on it.
 *
 * In order to know whether a kind of object is initially owned or initially unowned, you can use the test program tester/belle_sip_object_describe.
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
typedef belle_sip_error_code (*belle_sip_object_marshal_t)(belle_sip_object_t* obj, char* buff, size_t buff_size, size_t *offset);
typedef struct _belle_sip_object_vptr *(*belle_sip_object_get_vptr_t)(void);

struct _belle_sip_object_vptr{
	belle_sip_type_id_t id;
	size_t size; /*the size of the object - not the vptr size*/
	const char *type_name;
	int initially_unowned;
	belle_sip_object_get_vptr_t get_parent;
	struct belle_sip_interface_desc **interfaces; /*NULL terminated table of */
	belle_sip_object_destroy_t destroy;
	belle_sip_object_clone_t clone;
	belle_sip_object_marshal_t marshal;
	int tostring_bufsize_hint; /*optimization: you can suggest here the typical size for a to_string() result.*/
};

typedef struct _belle_sip_object_vptr belle_sip_object_vptr_t;

struct _belle_sip_object{
	belle_sip_object_vptr_t *vptr;
	int ref;
	char* name;
	struct weak_ref *weak_refs;
	struct belle_sip_object_pool *pool;
	belle_sip_list_t *pool_iterator;
	belle_sip_list_t *data_store;
};


BELLE_SIP_BEGIN_DECLS


BELLESIP_EXPORT belle_sip_object_t * _belle_sip_object_new(size_t objsize, belle_sip_object_vptr_t *vptr);

#define belle_sip_object_new(_type) (_type*)_belle_sip_object_new(sizeof(_type),(belle_sip_object_vptr_t*)BELLE_SIP_OBJECT_GET_VPTR_FUNC(_type)())


/**
 * Activates checks on object marshalling.
 * Useful for debug purposes.
 * @param enable TRUE to enable, FALSE to disable.
**/
BELLESIP_EXPORT void belle_sip_object_enable_marshal_check(int enable);

/**
 * Activates an object leak detector. When enabled, belle-sip will reference all created objects.
 * At program termination, application can check if objects remain alive using belle_sip_object_get_object_count() and dump them with
 * belle_sip_object_dump_active_objects().
 * @warning this must not be used in multi-threaded programs (when multiple threads can access belle-sip at the same time)
 * Useful for debug purposes.
 * @param enable TRUE to enable, FALSE to disable.
**/
BELLESIP_EXPORT void belle_sip_object_enable_leak_detector(int enable);

BELLESIP_EXPORT int belle_sip_object_get_object_count(void);

BELLESIP_EXPORT void belle_sip_object_flush_active_objects(void);

BELLESIP_EXPORT void belle_sip_object_dump_active_objects(void);

/**
 * Suspend leak detector from this point. If the leak detector wasn't activated, this function does nothing.
 * This can be useful to make object allocation that have to remain active beyond the scope of a test.
**/
BELLESIP_EXPORT void belle_sip_object_inhibit_leak_detector(int yes);

int belle_sip_object_is_unowed(const belle_sip_object_t *obj);

/**
 * Increments reference counter, which prevents the object from being destroyed.
 * If the object is initially unowed, this acquires the first reference.
 *
**/
BELLESIP_EXPORT belle_sip_object_t * belle_sip_object_ref(void *obj);

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
BELLESIP_EXPORT belle_sip_object_t *belle_sip_object_weak_ref(void *obj, belle_sip_object_destroy_notify_t destroy_notify, void *userpointer);

/**
 * Remove a weak reference to object.
**/
BELLESIP_EXPORT void belle_sip_object_weak_unref(void *obj, belle_sip_object_destroy_notify_t destroy_notify, void *userpointer);

/**
 * Set object name.
**/
BELLESIP_EXPORT void belle_sip_object_set_name(belle_sip_object_t *obj,const char* name);

/**
 * Get object name.
**/
BELLESIP_EXPORT const char* belle_sip_object_get_name(belle_sip_object_t *obj);

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


typedef void  (*belle_sip_data_destroy)(void* data);
typedef void* (*belle_sip_data_clone)(const char* name, void* data);

/**
 * Add an entry to the object's embedded data store, with the key name specified.
 * The destroy function is used when the data is cleaned.
 *
 * If an entry already exists, the existing data will be cleaned by calling its destroy function and the new data will be placed instead.
 *
 * Returns -1 in case of error, 0 in case the insertion was successful, and 1 if existing data was present.
**/
BELLESIP_EXPORT int   belle_sip_object_data_set( belle_sip_object_t *obj, const char* name, void* data, belle_sip_data_destroy destroy_func );

/**
 * Retrieve data that has been stored in the object data store.
**/
BELLESIP_EXPORT void* belle_sip_object_data_get( belle_sip_object_t *obj, const char* name );

/**
  * Return 1 if the key exists in the data store, 0 otherwise
  **/
BELLESIP_EXPORT int belle_sip_object_data_exists( const belle_sip_object_t *obj, const char* name );

/**
  * Destroys the named data associated by the name provided.
  *
  * Returns 0 for success, -1 for error
  **/
BELLESIP_EXPORT int   belle_sip_object_data_remove( belle_sip_object_t *obj, const char* name);

/**
  * Retrieve the data from the data store and removes it from the data store, without calling the destructor.
  * This transfers ownership of the data to the caller, which will be in charge of releasing it.
  **/
BELLESIP_EXPORT void* belle_sip_object_data_grab( belle_sip_object_t* obj, const char* name);

/**
  * Clears all data in the object's storage, invoking the destroy_func when possible
  **/
BELLESIP_EXPORT void belle_sip_object_data_clear( belle_sip_object_t* obj );

/**
  * clones the object's data store to another one, using the provided clone function to clone individual data items.
  *
  * The destination data store will be cleaned before pushing the source data into it.
  * For a merge, use #belle_sip_object_data_merge.
  * This is equivalent to the following code:
  *    {
  *     belle_sip_object_data_clear(dst);
  *     belle_sip_object_data_merge(src, dst, clone_func);
  *    }
  *
  * Note that providing NULL as a cloning function will simply assign the src object's data to the dst object.
  *
  **/
BELLESIP_EXPORT void belle_sip_object_data_clone( const belle_sip_object_t* src, belle_sip_object_t* dst, belle_sip_data_clone clone_func);

/**
  * Merge the source data store into the destination data store.
  *
  * Same function as #belle_sip_object_data_clone, except the destination data store is not cleared before inserting the source data.
  * This overwrites common keys, and keeps existing keys.
  */
BELLESIP_EXPORT void belle_sip_object_data_merge( const belle_sip_object_t* src, belle_sip_object_t* dst, belle_sip_data_clone clone_func);

/**
  * Apply a function for each entry in the data store
  */
BELLESIP_EXPORT void belle_sip_object_data_foreach( const belle_sip_object_t* obj, void (*apply_func)(const char* key, void* data, void* userdata), void* userdata);
/**
 * Returns a string describing the inheritance diagram and implemented interfaces of object obj.
**/
char *belle_sip_object_describe(void *obj);

/**
 * Returns a string describing the inheritance diagram and implemented interfaces of an object given its type name.
**/
BELLESIP_EXPORT char *belle_sip_object_describe_type_from_name(const char *name);

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

#define BELLE_SIP_OBJECT_VPTR(obj,object_type) ((BELLE_SIP_OBJECT_VPTR_TYPE(object_type)*)(((belle_sip_object_t*)obj)->vptr))

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

belle_sip_object_pool_t * belle_sip_object_pool_get_current(void);
int belle_sip_object_pool_cleanable(belle_sip_object_pool_t *pool);
void belle_sip_object_pool_clean(belle_sip_object_pool_t *obj);

BELLE_SIP_DECLARE_VPTR(belle_sip_object_t)

BELLE_SIP_END_DECLS

/**
 * Adding a new type in belle-sip in 5 steps
 * =========================================
 *
 * Let's suppose you want to add an object called belle_sip_something_t
 * 1) Declare the type in the enum in belle-sip.h:
 * 	BELLE_SIP_TYPE_ID(belle_sip_something_t)
 * 2) Declare the api of the new object in .h, including a typedef and a cast macro:
 * 	typedef struct belle_sip_something belle_sip_something_t;
 * 	#define BELLE_SIP_SOMETHING(obj)	BELLE_SIP_CAST(obj,belle_sip_something_t)
 *
 * 	belle_sip_something_t *belle_sip_something_create(int arg1, int arg2);
 * 	void belle_sip_something_do_cooking(belle_sip_something_t *obj);
 *    Do not add any destructor, belle_sip_object_unref() does it for all objects.
 *
 * 3) in the c file contaning the object's implementation, define the internal structure for your object.
 *   The first field of the struct must be the parent type.
 * 	struct belle_sip_something{
 * 		belle_sip_object_t base;
 * 		int myint1;
 * 		int myint2;
 * 		char *mychar;
 * 	};
 * 4) still in the C file contaning the object's implementation, define a destructor and all functions of its API:
 *    The destructor must only manage the fields from the type, not the parent.
 * 	static void belle_sip_something_destroy(belle_sip_something_t *obj){
 * 		if (obj->mychar) belle_sip_free(obj->mychar);
 * 	}
 *
 * 	belle_sip_something_t *belle_sip_something_create(int arg1, int arg2){
 * 		belle_sip_something_t *obj=belle_sip_object_new(belle_sip_something_t);
 * 		obj->myint1=arg1;
 * 		obj->myint2=arg2;
 * 		obj->mychar=belle_sip_strdup("Hello world");
 * 		return obj;
 * 	}
 *    Declare the interfaces implemented by the object (to be documented) and instanciate its "vptr", necessary for dynamic casting.
 * 	BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_something_t);
 * 	BELLE_SIP_INSTANCIATE_VPTR(belle_sip_something_t, belle_sip_object_t,belle_sip_something_destroy, NULL, NULL,FALSE);
 *
 * 5) in .h file included everywhere in the source (typically belle_sip_internal.h), declare the vptr
 * 	BELLE_SIP_DECLARE_VPTR(belle_sip_dns_srv_t);
 */

#if defined(__cplusplus) && defined(BELLE_SIP_USE_STL)
#include <ostream>
inline   std::ostream&
operator<<( std::ostream& __os, const belle_sip_object_t* object)
{
	char* object_as_string = belle_sip_object_to_string((void*)object);
	__os << object_as_string;
	belle_sip_free(object_as_string);
	return __os;
}
#endif
#endif

