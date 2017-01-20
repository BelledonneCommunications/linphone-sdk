/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifndef mscommon_h
#define mscommon_h

#include <bctoolbox/port.h>
#include <bctoolbox/list.h>
#include <ortp/logging.h>
#include <ortp/port.h>
#include <ortp/str_utils.h>
#include <ortp/payloadtype.h>
#include <time.h>
#if defined(__APPLE__)
#include "TargetConditionals.h"
#endif

#if defined(__arm__) || defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM)
#define MS_HAS_ARM 1
#endif
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#define MS_HAS_ARM_NEON 1
#endif
#if MS_HAS_ARM_NEON && !(defined(__arm64__) || defined(__aarch64__))
#define MS_HAS_ARM_NEON_32 1
#endif

#ifndef MS2_DEPRECATED
#if defined(_MSC_VER)
#define MS2_DEPRECATED __declspec(deprecated)
#else
#define MS2_DEPRECATED __attribute__ ((deprecated))
#endif
#endif
#define MS_UNUSED(x) ((void)(x))

#define ms_malloc	ortp_malloc
#define ms_malloc0	ortp_malloc0
#define ms_realloc	ortp_realloc
#define ms_new		ortp_new
#define ms_new0		ortp_new0
#define ms_free		ortp_free
#define ms_strdup	ortp_strdup
#define ms_strndup	ortp_strndup
#define ms_strdup_printf	ortp_strdup_printf
#define ms_strcat_printf	ortp_strcat_printf

#define ms_mutex_t		ortp_mutex_t
#define ms_mutex_init		ortp_mutex_init
#define ms_mutex_destroy	ortp_mutex_destroy
#define ms_mutex_lock		ortp_mutex_lock
#define ms_mutex_unlock		ortp_mutex_unlock

#define ms_cond_t		ortp_cond_t
#define ms_cond_init		ortp_cond_init
#define ms_cond_wait		ortp_cond_wait
#define ms_cond_signal		ortp_cond_signal
#define ms_cond_broadcast	ortp_cond_broadcast
#define ms_cond_destroy		ortp_cond_destroy

#define MS_DEFAULT_MAX_PAYLOAD_SIZE 1440

#define MS2_INLINE ORTP_INLINE

#ifdef _WIN32
#if defined(__MINGW32__) || !defined(WINAPI_FAMILY_PARTITION) || !defined(WINAPI_PARTITION_DESKTOP)
#define MS2_WINDOWS_DESKTOP 1
#elif defined(WINAPI_FAMILY_PARTITION)
#if defined(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define MS2_WINDOWS_DESKTOP 1
#elif defined(WINAPI_PARTITION_PHONE_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
#define MS2_WINDOWS_PHONE 1
#elif defined(WINAPI_PARTITION_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#define MS2_WINDOWS_UNIVERSAL 1
#endif
#endif
#endif

#if defined(_MSC_VER)
#define MS2_PUBLIC	__declspec(dllexport)
#define MS2_VAR_PUBLIC extern __declspec(dllexport)
#else
#define MS2_PUBLIC
#define MS2_VAR_PUBLIC extern
#endif

#if defined(_WIN32_WCE)
time_t ms_time (time_t *t);
#else
#define ms_time time
#endif

#ifdef DEBUG
static MS2_INLINE void ms_debug(const char *fmt,...)
{
  va_list args;
  va_start (args, fmt);
  ortp_logv(ORTP_LOG_DOMAIN, ORTP_DEBUG, fmt, args);
  va_end (args);
}
#else
#define ms_debug(fmt, ...)
#endif

#define ms_message	ortp_message
#define ms_warning	ortp_warning
#define ms_error	ortp_error
#define ms_fatal	ortp_fatal

#define ms_return_val_if_fail(_expr_,_ret_)\
	if (!(_expr_)) { ms_fatal("assert "#_expr_ "failed"); return (_ret_);}

#define ms_return_if_fail(_expr_) \
	if (!(_expr_)){ ms_fatal("assert "#_expr_ "failed"); return ;}

#define ms_thread_t		ortp_thread_t
#define ms_thread_create 	ortp_thread_create
#define ms_thread_join		ortp_thread_join
#define ms_thread_self		ortp_thread_self

typedef ortpTimeSpec MSTimeSpec;

#define ms_get_cur_time ortp_get_cur_time
#define ms_get_cur_time_ms ortp_get_cur_time_ms

typedef bctbx_compare_func MSCompareFunc;
typedef void (*MSIterateFunc)(void *a);
typedef void (*MSIterate2Func)(void *a, void *b);
typedef void (*MSIterate3Func)(void *a, void *b, void *c);

#ifdef __cplusplus
extern "C"{
#endif
/*for stun*/
typedef struct { unsigned char octet[12]; } UInt96;
typedef struct { unsigned char octet[16]; } UInt128;

MS2_PUBLIC void ms_thread_exit(void* ret_val);


#define MSList bctbx_list_t

/**
 * @addtogroup ms_list
 * @{
**/

/** Inserts a new element containing data to the end of a given list
 * @param list list where data should be added. If NULL, a new list will be created.
 * @param data data to insert into the list
 * @return first element of the list
 * @deprecated Use bctbx_list_append() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList * ms_list_append(MSList *list, void * data);

/** Inserts given element to the end of a given list
 * @param list list where data should be added. If NULL, a new list will be created.
 * @param new_elem element to append
 * @return first element of the list
 * @deprecated Use bctbx_list_append_link() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList *ms_list_append_link(MSList *list, MSList *new_elem);

/** Inserts a new element containing data to the start of a given list
 * @param list list where data should be added. If NULL, a new list will be created.
 * @param data data to insert into the list
 * @return first element of the list - the one which was just created.
 * @deprecated Use bctbx_list_prepend() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList * ms_list_prepend(MSList *list, void * data);

/** Frees all elements of a given list
 * Note that data contained in each element will not be freed. If you need to clean
 * them, consider using @ms_list_free_with_data
 * @param list object to free.
 * @return NULL
 * @deprecated Use bctbx_list_free() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList * ms_list_free(MSList *list);

/** Frees all elements of a given list after having called freefunc on each element
 * @param list object to free.
 * @param freefunc function to invoke on each element data before destroying the element
 * @return NULL
 * @deprecated Use bctbx_list_free_with_data() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList * ms_list_free_with_data(MSList *list, void (*freefunc)(void*));

/** Concatenates second list to the end of first list
 * @param first First list
 * @param second Second list to append at the end of first list.
 * @return first element of the merged list
 * @deprecated Use bctbx_list_concat() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList * ms_list_concat(MSList *first, MSList *second);

/** Finds and remove the first element containing the given data. Nothing is done if element is not found.
 * @param list List in which data must be removed
 * @param data Data to remove
 * @return first element of the modified list
 * @deprecated Use bctbx_list_remove() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList * ms_list_remove(MSList *list, void *data);

/** Finds and remove any elements according to the given predicate function
 * @param list List in which data must be removed
 * @param compare_func Function to invoke on each element. If it returns TRUE, the given element will be deleted.
 * @param user_data User data to pass to compare_func function
 * @return first element of the modified list
 * @deprecated Use bctbx_list_remove_custom() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList * ms_list_remove_custom(MSList *list, MSCompareFunc compare_func, const void *user_data);

/** Returns size of a given list
 * @param list List to measure
 * @return Size of list
 * @deprecated Use bctbx_list_size() instead
**/
MS2_PUBLIC MS2_DEPRECATED int ms_list_size(const MSList *list);

/** Invoke function on each element of the list
 * @param list List object
 * @param iterate_func Function to invoke on each element.
 * @deprecated Use bctbx_list_for_each() instead
**/
MS2_PUBLIC MS2_DEPRECATED void ms_list_for_each(const MSList *list, MSIterateFunc iterate_func);

/** Invoke function on each element of the list
 * @param list List object
 * @param iterate_func Function to invoke on each element.
 * @param user_data User data to pass to iterate_func function.
 * @deprecated Use bctbx_list_for_each2() instead
**/
MS2_PUBLIC MS2_DEPRECATED void ms_list_for_each2(const MSList *list, MSIterate2Func iterate_func, void *user_data);
	
MS2_PUBLIC MS2_DEPRECATED void ms_list_for_each3(const MSList *list, MSIterate3Func iterate_func, void *user_data, void *factory);

/** Finds and remove given element in list.
 * @param list List in which element must be removed
 * @param element element to remove
 * @return first element of the modified list
 * @deprecated Use bctbx_list_remove_link() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList *ms_list_remove_link(MSList *list, MSList *elem);

/** Finds first element containing data in the given list.
 * @param list List in which element must be found
 * @param data data to find
 * @return element containing data, or NULL if not found
 * @deprecated Use bctbx_list_find() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList *ms_list_find(MSList *list, void *data);

/** Finds first element according to the given predicate function
 * @param list List in which element must be found
 * @param compare_func Function to invoke on each element. If it returns TRUE, the given element will be returned.
 * @param user_data User data to pass to compare_func function
 * @return Element matching the predicate, or NULL if none is found.
 * @deprecated Use bctbx_list_find_custom() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList *ms_list_find_custom(MSList *list, MSCompareFunc compare_func, const void *user_data);

/** Returns the nth element data of the list
 * @param list List object
 * @param index data index which must be returned.
 * @return Element at the given index. NULL if index is invalid (negative or greater or equal to ms_list_size).
 * @deprecated Use bctbx_list_nth_data() instead
**/
MS2_PUBLIC MS2_DEPRECATED void * ms_list_nth_data(const MSList *list, int index);

/** Returns the index of the given element
 * @param list List object
 * @param elem Element to search for.
 * @return Index of the given element. -1 if not found.
 * @deprecated Use bctbx_list_position() instead
**/
MS2_PUBLIC MS2_DEPRECATED int ms_list_position(const MSList *list, MSList *elem);

/** Returns the index of the first element containing data
 * @param list List object
 * @param data Data to search for.
 * @return Index of the element containing data. -1 if not found.
 * @deprecated Use bctbx_list_index() instead
**/
MS2_PUBLIC MS2_DEPRECATED int ms_list_index(const MSList *list, void *data);

/** Inserts a new element containing data at the place given by compare_func
 * @param list list where data should be added. If NULL, a new list will be created.
 * @param data data to insert into the list
 * @param compare_func function determining where should the new element be placed
 * @return first element of the list.
 * @deprecated Use bctbx_list_insert_sorted() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList *ms_list_insert_sorted(MSList *list, void *data, MSCompareFunc compare_func);

/** Inserts a new element containing data before the given element
 * @param list list where data should be added. If NULL, a new list will be created.
 * @param before element parent to the one we will insert.
 * @param data data to insert into the list
 * @return first element of the modified list.
 * @deprecated Use bctbx_list_insert() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList *ms_list_insert(MSList *list, MSList *before, void *data);

/** Copies a list in another one, duplicating elements but not data
 * @param list list to copy
 * @return Newly created list
 * @deprecated Use bctbx_list_copy() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList *ms_list_copy(const MSList *list);

/** Copies a list in another one, duplicating elements according to the given function
 * @param list list to copy
 * @param copyfunc function to invoke on each element which will return the new list data value
 * @return Newly created list
 * @deprecated Use bctbx_list_copy_with_data() instead
**/
MS2_PUBLIC MS2_DEPRECATED MSList *ms_list_copy_with_data(const MSList *list, void *(*copyfunc)(void *));

/**
 * @deprecated @deprecated Use bctbx_list_next() instead
 */
MS2_PUBLIC MS2_DEPRECATED MSList* ms_list_next(const MSList *list);

/** @} */

MS2_PUBLIC char * ms_tags_list_as_string(const MSList *list);
MS2_PUBLIC bool_t ms_tags_list_contains_tag(const MSList *list, const char *tag);

#undef MIN
#define MIN(a,b)	((a)>(b) ? (b) : (a))
#undef MAX
#define MAX(a,b)	((a)>(b) ? (a) : (b))

/**
 * @file mscommon.h
 * @brief mediastreamer2 mscommon.h include file
 *
 * This file provide the API needed to initialize
 * and reset the mediastreamer2 library.
 *
 */

/**
 * @addtogroup mediastreamer2_init
 * @{
 */


/**
 * Helper macro for backward compatibility.
 * Use ms_base_init() and ms_voip_init() instead.
 */
#define ms_init()	ms_base_init(), ms_voip_init(), ms_plugins_init()

/**
 * Helper macro for backward compatibility.
 * Use ms_base_exit() and ms_voip_exit() instead.
 */
#define ms_exit()	ms_voip_exit(), ms_plugins_exit(), ms_base_exit()

/**
 * Initialize the mediastreamer2 base library.
 *
 * This must be called once before calling any other API.
 * @deprecated use ms_factory_new()
 */
MS2_PUBLIC MS2_DEPRECATED void ms_base_init(void);

/**
 * Initialize the mediastreamer2 VoIP library.
 *
 * This must be called one before calling any other API.
 * @deprecated use ms_factory_new_with_voip().
 */
MS2_PUBLIC MS2_DEPRECATED void ms_voip_init(void);

/**
 * Load the plugins from the default plugin directory.
 *
 * This is just a wrapper around ms_load_plugins().
 * This must be called after ms_base_init() and after ms_voip_init().
 * @deprecated use ms_factory_init_plugins(), or ms_factory_new_with_voip() that does it automatically.
 */
MS2_PUBLIC MS2_DEPRECATED void ms_plugins_init(void);

/**
 * Set the directory from where the plugins are to be loaded when calling ms_plugins_init().
 * @param[in] path The path to the plugins directory.
 * @deprecated use ms_factory_set_plugins_dir().
 */
MS2_PUBLIC MS2_DEPRECATED void ms_set_plugins_dir(const char *path);

/**
 * Load plugins from a specific directory.
 * This method basically loads all libraries in the specified directory and attempts to call a C function called
 * \<libraryname\>_init. For example if a library 'libdummy.so' or 'libdummy.dll' is found, then the loader tries to locate
 * a C function called 'libdummy_init()' and calls it if it exists.
 * ms_load_plugins() can be used to load non-mediastreamer2 plugins as it does not expect mediastreamer2 specific entry points.
 *
 * @param directory   A directory where plugins library are available.
 *
 * @return >0 if successfull, 0 if not plugins loaded, -1 otherwise.
 * @deprecated use ms_factory_load_plugins().
 */
MS2_PUBLIC MS2_DEPRECATED int ms_load_plugins(const char *directory);

/**
 * Release resource allocated in the mediastreamer2 base library.
 *
 * This must be called once before closing program.
 * @deprecated use ms_factory_destroy().
 */
MS2_PUBLIC MS2_DEPRECATED void  ms_base_exit(void);

/**
 * Release resource allocated in the mediastreamer2 VoIP library.
 *
 * This must be called once before closing program.
 * @deprecated use ms_factory_destroy().
 */
MS2_PUBLIC MS2_DEPRECATED void ms_voip_exit(void);

/**
 * Unload the plugins loaded by ms_plugins_init().
 * @deprecated use ms_factory_destroy().
 */
MS2_PUBLIC MS2_DEPRECATED void ms_plugins_exit(void);

struct _MSSndCardDesc;

MS2_PUBLIC void ms_sleep(int seconds);

MS2_PUBLIC void ms_usleep(uint64_t usec);

/**
 * The max payload size allowed.
 * Filters that generate data that can be sent through RTP should make packets
 * whose size is below ms_get_payload_max_size().
 * The default value is 1440 computed as the standard internet MTU minus IPv6 header,
 * UDP header and RTP header. As IPV4 header is smaller than IPv6 header, this
 * value works for both.
 * @deprecated use ms_factory_get_payload_max_size().
**/
MS2_PUBLIC MS2_DEPRECATED int ms_get_payload_max_size(void);

/**
 * Set the maximum payload size allowed.
 * @deprecated use ms_factory_set_payload_max_size().
**/
MS2_PUBLIC MS2_DEPRECATED void ms_set_payload_max_size(int size);

/**
 * Returns the network Max Transmission Unit to reach destination_host.
 * This will attempt to send one or more big packets to destination_host, to a random port.
 * Those packets are filled with zeroes.
**/
MS2_PUBLIC int ms_discover_mtu(const char *destination_host);

/**
 * Set mediastreamer default mtu, used to compute the default RTP max payload size.
 * This function will call ms_set_payload_max_size(mtu-[ipv6 header size]).
 * @deprecated use ms_factory_set_mtu()
**/
MS2_PUBLIC MS2_DEPRECATED void ms_set_mtu(int mtu);

/**
 * Get mediastreamer default mtu, used to compute the default RTP max payload size.
 * @deprecated use ms_factory_get_mtu().
**/
MS2_PUBLIC MS2_DEPRECATED int ms_get_mtu(void);

/**
 * Declare how many cpu (cores) are available on the platform
 * @deprecated use ms_factory_set_cpu_count().
 */
MS2_PUBLIC MS2_DEPRECATED void ms_set_cpu_count(unsigned int c);

/**
 * @deprecated use ms_factory_get_cpu_count().
**/
MS2_PUBLIC MS2_DEPRECATED unsigned int ms_get_cpu_count(void);

/**
 * Adds a new entry in the SoundDeviceDescription table
 */
MS2_PUBLIC void ms_sound_device_description_add(const char *manufacturer, const char *model, const char *platform, unsigned int flags, int delay, int recommended_rate);

/**
 * @return TRUE if address is ipv6
 */
MS2_PUBLIC bool_t ms_is_ipv6(const char *address);

/**
 * @return TRUE if address is multicast
 */
bool_t ms_is_multicast_addr(const struct sockaddr *address);
/**
 * @return TRUE if address is multicast
 */
MS2_PUBLIC bool_t ms_is_multicast(const char *address);

/**
 * Utility function to load a file into memory.
 * @param file a FILE handle
 * @param nbytes (optional) number of bytes read
**/
MS2_PUBLIC char *ms_load_file_content(FILE *file, size_t *nbytes);

/**
 * Utility function to load a file into memory.
 * @param path a FILE handle
 * @param nbytes (optional) number of bytes read
**/
MS2_PUBLIC char *ms_load_path_content(const char *path, size_t *nbytes);
/** @} */

#ifdef __cplusplus
}
#endif

#ifdef MS2_INTERNAL
#  ifdef HAVE_CONFIG_H
#  include "mediastreamer-config.h" /*necessary to know if ENABLE_NLS is there*/
#  endif

#ifdef _WIN32
#include <malloc.h> //for alloca
#ifdef _MSC_VER
#define alloca _alloca
#endif
#endif

#  if defined(ENABLE_NLS)

#ifdef _MSC_VER
// prevent libintl.h from re-defining fprintf and vfprintf
#ifndef fprintf
#define fprintf fprintf
#endif
#ifndef vfprintf
#define vfprintf vfprintf
#endif
#define _GL_STDIO_H
#endif

#    include <libintl.h>
#    define _(String) dgettext (GETTEXT_PACKAGE, String)
#  else
#    define _(String) (String)
#  endif // ENABLE_NLS
#define N_(String) (String)
#endif // MS2_INTERNAL

#ifdef ANDROID
#include "mediastreamer2/msjava.h"
#endif
#endif

