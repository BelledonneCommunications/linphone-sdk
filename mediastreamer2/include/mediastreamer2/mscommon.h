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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#ifndef mscommon_h
#define mscommon_h

#include <ortp/logging.h>
#include <ortp/port.h>
#include <ortp/str_utils.h>
#include <ortp/payloadtype.h>
#include <time.h>
#if defined(__APPLE__) 
#include "TargetConditionals.h"
#endif

#define MS_UNUSED(x) ((void)(x))

#define ms_malloc	ortp_malloc
#define ms_malloc0	ortp_malloc0
#define ms_realloc	ortp_realloc
#define ms_new		ortp_new
#define ms_new0		ortp_new0
#define ms_free		ortp_free
#define ms_strdup	ortp_strdup
#define ms_strdup_printf	ortp_strdup_printf

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

#if defined(_MSC_VER)
#define MS2_PUBLIC	__declspec(dllexport)
#else
#define MS2_PUBLIC
#endif

#if defined(_WIN32_WCE)
time_t ms_time (time_t *t);
#else
#define ms_time time
#endif

#ifdef WIN32
static inline void ms_debug(const char *fmt,...)
{
  va_list args;
  va_start (args, fmt);
  ortp_logv(ORTP_DEBUG, fmt, args);
  va_end (args);
}
#else
#ifdef DEBUG
static inline void ms_debug(const char *fmt,...)
{
  va_list args;
  va_start (args, fmt);
  ortp_logv(ORTP_DEBUG, fmt, args);
  va_end (args);
}
#else
#define ms_debug(...)
#endif	
#endif

#define ms_message	ortp_message
#define ms_warning	ortp_warning
#define ms_error	ortp_error
#define ms_fatal	ortp_fatal

#define ms_return_val_if_fail(_expr_,_ret_)\
	if (!(_expr_)) { ms_error("assert "#_expr_ "failed"); return (_ret_);}

#define ms_return_if_fail(_expr_) \
	if (!(_expr_)){ ms_error("assert "#_expr_ "failed"); return ;}

#define ms_thread_t		ortp_thread_t
#define ms_thread_create 	ortp_thread_create
#define ms_thread_join		ortp_thread_join

typedef ortpTimeSpec MSTimeSpec;

#define ms_get_cur_time ortp_get_cur_time

struct _MSList {
	struct _MSList *next;
	struct _MSList *prev;
	void *data;
};

typedef struct _MSList MSList;


#define ms_list_next(elem) ((elem)->next)


typedef int (*MSCompareFunc)(const void *a, const void *b);
typedef void (*MSIterateFunc)(void *a);
typedef void (*MSIterate2Func)(void *a, void *b);

#ifdef __cplusplus
extern "C"{
#endif

MS2_PUBLIC void ms_thread_exit(void* ret_val);
MS2_PUBLIC MSList * ms_list_append(MSList *elem, void * data);
MS2_PUBLIC MSList *ms_list_append_link(MSList *elem, MSList *new_elem);
MS2_PUBLIC MSList * ms_list_prepend(MSList *elem, void * data);
MS2_PUBLIC MSList * ms_list_free(MSList *elem);
MS2_PUBLIC MSList * ms_list_concat(MSList *first, MSList *second);
MS2_PUBLIC MSList * ms_list_remove(MSList *first, void *data);
MS2_PUBLIC int ms_list_size(const MSList *first);
MS2_PUBLIC void ms_list_for_each(const MSList *list, MSIterateFunc iterate_func);
MS2_PUBLIC void ms_list_for_each2(const MSList *list, MSIterate2Func iterate_func, void *user_data);
MS2_PUBLIC MSList *ms_list_remove_link(MSList *list, MSList *elem);
MS2_PUBLIC MSList *ms_list_find(MSList *list, void *data);
MS2_PUBLIC MSList *ms_list_find_custom(MSList *list, MSCompareFunc compare_func, const void *user_data);
MS2_PUBLIC void * ms_list_nth_data(const MSList *list, int index);
MS2_PUBLIC int ms_list_position(const MSList *list, MSList *elem);
MS2_PUBLIC int ms_list_index(const MSList *list, void *data);
MS2_PUBLIC MSList *ms_list_insert_sorted(MSList *list, void *data, MSCompareFunc compare_func);
MS2_PUBLIC MSList *ms_list_insert(MSList *list, MSList *before, void *data);
MS2_PUBLIC MSList *ms_list_copy(const MSList *list);

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
 * @defgroup mediastreamer2_init Init API - manage mediastreamer2 library.
 * @ingroup mediastreamer2_api
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
#define ms_exit()	ms_voip_exit(), ms_base_exit()


/**
 * Initialize the mediastreamer2 base library.
 *
 * This must be called once before calling any other API.
 */
MS2_PUBLIC void ms_base_init(void);

/**
 * Initialize the mediastreamer2 VoIP library.
 *
 * This must be called one before calling any other API.
 */
MS2_PUBLIC void ms_voip_init(void);

/**
 * Load the plugins from the default plugin directory.
 *
 * This is just a wrapper around ms_load_plugins().
 * This must be called after ms_base_init() and after ms_voip_init().
 */
MS2_PUBLIC void ms_plugins_init(void);

/**
 * Load plugins from a specific directory.
 * This method basically loads all libraries in the specified directory and attempts to call a C function called
 * \<libraryname\>_init. For example if a library 'libdummy.so' or 'libdummy.dll' is found, then the loader tries to locate
 * a C function called 'libdummy_init()' and calls it if it exists.
 * ms_load_plugins() can be used to load non-mediastreamer2 plugins as it does not expect mediastreamer2 specific entry points.
 *
 * @param directory   A directory where plugins library are available.
 *
 * Returns: >0 if successfull, 0 if not plugins loaded, -1 otherwise.
 */
MS2_PUBLIC int ms_load_plugins(const char *directory);

/**
 * Release resource allocated in the mediastreamer2 base library.
 *
 * This must be called once before closing program.
 */
MS2_PUBLIC void ms_base_exit(void);

/**
 * Release resource allocated in the mediastreamer2 VoIP library.
 *
 * This must be called once before closing program.
 */
MS2_PUBLIC void ms_voip_exit(void);

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
 * 
**/
MS2_PUBLIC int ms_get_payload_max_size();

MS2_PUBLIC void ms_set_payload_max_size(int size);

/**
 * Returns the network Max Transmission Unit to reach destination_host.
 * This will attempt to send one or more big packets to destination_host, to a random port.
 * Those packets are filled with zeroes.
**/
MS2_PUBLIC int ms_discover_mtu(const char *destination_host);

/**
 * Set mediastreamer default mtu, used to compute the default RTP max payload size.
 * This function will call ms_set_payload_max_size(mtu-[ipv6 header size]).
**/
MS2_PUBLIC void ms_set_mtu(int mtu);


/**
 * Get mediastreamer default mtu, used to compute the default RTP max payload size.
**/
MS2_PUBLIC int ms_get_mtu(void);

/**
 * Declare how many cpu (cores) are available on the platform
 */
MS2_PUBLIC void ms_set_cpu_count(unsigned int c);
 
MS2_PUBLIC unsigned int ms_get_cpu_count();

/** @} */

#ifdef __cplusplus
}
#endif

#ifdef MS2_INTERNAL
#  ifdef HAVE_CONFIG_H
#  include "mediastreamer-config.h" /*necessary to know if ENABLE_NLS is there*/
#  endif

#ifdef WIN32
#include <malloc.h> //for alloca
#ifdef _MSC_VER
#define alloca _alloca
#endif
#endif

#  if defined(ENABLE_NLS)
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

