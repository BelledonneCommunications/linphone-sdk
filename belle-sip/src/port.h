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

#ifndef belle_sip_port_h
#define belle_sip_port_h

#include <sys/stat.h>

#ifndef _WIN32
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <strings.h>

#else

#if defined(__MINGW32__) || !defined(WINAPI_FAMILY_PARTITION) || !defined(WINAPI_PARTITION_DESKTOP)
#define BELLE_SIP_WINDOWS_DESKTOP 1
#elif defined(WINAPI_FAMILY_PARTITION)
#if defined(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define BELLE_SIP_WINDOWS_DESKTOP 1
#elif defined(WINAPI_PARTITION_PHONE_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
#define BELLE_SIP_WINDOWS_PHONE 1
#elif defined(WINAPI_PARTITION_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#define BELLE_SIP_WINDOWS_UNIVERSAL 1
#endif
#endif
#ifdef _MSC_VER
#if (_MSC_VER >= 1900)
#define BELLE_SIP_MSC_VER_GREATER_19
#endif
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#define strcasecmp _stricmp
#ifdef _MSC_VER
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strdup _strdup

#else
#include <stdint.h>
#endif

/*AI_NUMERICSERV is not defined for windows XP. Since it is not essential, we define it to 0 (does nothing)*/
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif

#endif

#if defined(_WIN32) || defined(__QNX__)
/* Mingw32 does not define AI_V4MAPPED, however it is supported starting from Windows Vista. QNX also does not define AI_V4MAPPED. */
#	ifndef AI_V4MAPPED
#	define AI_V4MAPPED 0x00000800
#	endif
#	ifndef AI_ALL
#	define AI_ALL 0x00000100
#	endif
#	ifndef IPV6_V6ONLY
#	define IPV6_V6ONLY 27
#	endif
#endif

#if defined(_WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__WIN32__)
#ifdef BELLESIP_INTERNAL_EXPORTS
#define BELLESIP_INTERNAL_EXPORT __declspec(dllexport)
#else
#define BELLESIP_INTERNAL_EXPORT
#endif
#else
#define BELLESIP_INTERNAL_EXPORT extern
#endif

/*
 * Socket abstraction layer
 */

BELLESIP_INTERNAL_EXPORT int belle_sip_init_sockets(void);
BELLESIP_INTERNAL_EXPORT void belle_sip_uninit_sockets(void);
int belle_sip_socket_set_nonblocking (belle_sip_socket_t sock);
int belle_sip_socket_set_dscp(belle_sip_socket_t sock, int ai_family, int dscp);
int belle_sip_socket_enable_dual_stack(belle_sip_socket_t sock);

#if defined(_WIN32)

typedef HANDLE belle_sip_thread_t;

#define belle_sip_thread_self_id()		(unsigned long)GetCurrentThreadId()
#define belle_sip_thread_get_id(thread)			(unsigned long)GetThreadId(thread)

typedef intptr_t belle_sip_thread_key_t;
int belle_sip_thread_key_create(belle_sip_thread_key_t *key, void (*destructor)(void*) );
int belle_sip_thread_setspecific(belle_sip_thread_key_t key,const void *value);
const void* belle_sip_thread_getspecific(belle_sip_thread_key_t key);
int belle_sip_thread_key_delete(belle_sip_thread_key_t key);


static BELLESIP_INLINE void belle_sip_close_socket(belle_sip_socket_t s){
	closesocket(s);
}

static BELLESIP_INLINE int get_socket_error(void){
	return WSAGetLastError();
}

const char *belle_sip_get_socket_error_string();
const char *belle_sip_get_socket_error_string_from_code(int code);

/*
 * Thread abstraction layer
 */

#ifdef _WIN32

typedef HANDLE belle_sip_thread_t;
#ifdef BELLE_SIP_WINDOWS_DESKTOP
typedef HANDLE belle_sip_mutex_t;
#else
typedef SRWLOCK belle_sip_mutex_t;
#endif

int belle_sip_thread_join(belle_sip_thread_t thread, void **retptr);
int belle_sip_thread_create(belle_sip_thread_t *thread, void *attr, void * (*routine)(void*), void *arg);
int belle_sip_mutex_init(belle_sip_mutex_t *m, void *attr_unused);
int belle_sip_mutex_lock(belle_sip_mutex_t *mutex);
int belle_sip_mutex_unlock(belle_sip_mutex_t *mutex);
int belle_sip_mutex_destroy(belle_sip_mutex_t *mutex);

#else

#include <pthread.h>

typedef pthread_t belle_sip_thread_t;
typedef pthread_mutex_t belle_sip_mutex_t;
#define belle_sip_thread_join(thread, retptr) pthread_join(thread, retptr)
#define belle_sip_thread_create(thread, attr, routine, arg) pthread_create(thread, attr, routine, arg)
#define belle_sip_mutex_init pthread_mutex_init
#define belle_sip_mutex_lock pthread_mutex_lock
#define belle_sip_mutex_unlock pthread_mutex_unlock
#define belle_sip_mutex_destroy pthread_mutex_destroy


#endif

#ifndef BELLE_SIP_WINDOWS_DESKTOP
BELLESIP_INTERNAL_EXPORT void belle_sip_sleep(unsigned int ms);
#else
#define belle_sip_sleep Sleep
#endif

#define usleep(us) belle_sip_sleep((us)/1000)
static BELLESIP_INLINE int inet_aton(const char *ip, struct in_addr *p){
	*(long*)p=inet_addr(ip);
	return 0;
}

#else

typedef pthread_t belle_sip_thread_t;
#define belle_sip_thread_self_id()			(unsigned long)pthread_self()
#define belle_sip_thread_get_id(thread)			(unsigned long)thread

typedef pthread_key_t belle_sip_thread_key_t;
#define belle_sip_thread_key_create(key,destructor)		pthread_key_create(key,destructor)
#define belle_sip_thread_setspecific(key,value)			pthread_setspecific(key,value)
#define belle_sip_thread_getspecific(key)			pthread_getspecific(key)
#define belle_sip_thread_key_delete(key)				pthread_key_delete(key)

static BELLESIP_INLINE void belle_sip_close_socket(belle_sip_socket_t s){
	close(s);
}

static BELLESIP_INLINE int get_socket_error(void){
	return errno;
}
#define belle_sip_get_socket_error_string() strerror(errno)
#define belle_sip_get_socket_error_string_from_code(code) strerror(code)

#endif

#define BELLESIP_EWOULDBLOCK BCTBX_EWOULDBLOCK
#define BELLESIP_EINPROGRESS BCTBX_EINPROGRESS
#define belle_sip_error_code_is_would_block(err) ((err)==BELLESIP_EWOULDBLOCK || (err)==BELLESIP_EINPROGRESS)

#endif

