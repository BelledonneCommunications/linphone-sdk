/*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc3550) stack.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/* this file is responsible of the portability of the stack */

#ifndef BCTBX_PORT_H
#define BCTBX_PORT_H


#if !defined(_WIN32) && !defined(_WIN32_WCE)
/********************************/
/* definitions for UNIX flavour */
/********************************/

#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef __linux
#include <stdint.h>
#endif


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#if defined(_XOPEN_SOURCE_EXTENDED) || !defined(__hpux)
#include <arpa/inet.h>
#endif

#include <sys/time.h>
#include <netdb.h>


typedef int bctbx_socket_t;
typedef pthread_t bctbx_thread_t;
typedef pthread_mutex_t bctbx_mutex_t;
typedef pthread_cond_t bctbx_cond_t;

#ifdef __INTEL_COMPILER
#pragma warning(disable : 111)		// statement is unreachable
#pragma warning(disable : 181)		// argument is incompatible with corresponding format string conversion
#pragma warning(disable : 188)		// enumerated type mixed with another type
#pragma warning(disable : 593)		// variable "xxx" was set but never used
#pragma warning(disable : 810)		// conversion from "int" to "unsigned short" may lose significant bits
#pragma warning(disable : 869)		// parameter "xxx" was never referenced
#pragma warning(disable : 981)		// operands are evaluated in unspecified order
#pragma warning(disable : 1418)		// external function definition with no prior declaration
#pragma warning(disable : 1419)		// external declaration in primary source file
#pragma warning(disable : 1469)		// "cc" clobber ignored
#endif

#define BCTBX_PUBLIC
#define BCTBX_INLINE			inline

#ifdef __cplusplus
extern "C"
{
#endif

int __bctbx_thread_join(bctbx_thread_t thread, void **ptr);
int __bctbx_thread_create(bctbx_thread_t *thread, pthread_attr_t *attr, void * (*routine)(void*), void *arg);
unsigned long __bctbx_thread_self(void);

#ifdef __cplusplus
}
#endif

#define bctbx_thread_create    __bctbx_thread_create
#define bctbx_thread_join      __bctbx_thread_join
#define bctbx_thread_self      __bctbx_thread_self
#define bctbx_thread_exit      pthread_exit
#define bctbx_mutex_init       pthread_mutex_init
#define bctbx_mutex_lock       pthread_mutex_lock
#define bctbx_mutex_unlock     pthread_mutex_unlock
#define bctbx_mutex_destroy    pthread_mutex_destroy
#define bctbx_cond_init        pthread_cond_init
#define bctbx_cond_signal      pthread_cond_signal
#define bctbx_cond_broadcast   pthread_cond_broadcast
#define bctbx_cond_wait        pthread_cond_wait
#define bctbx_cond_destroy     pthread_cond_destroy
#define bctbx_inet_aton        inet_aton

#define SOCKET_OPTION_VALUE	void *
#define SOCKET_BUFFER		void *

#define getSocketError() strerror(errno)
#define getSocketErrorCode() (errno)
#define bctbx_gettimeofday(tv,tz) gettimeofday(tv,tz)
#define bctbx_log10f(x)	log10f(x)


#else
/*********************************/
/* definitions for WIN32 flavour */
/*********************************/

#include <stdio.h>
#define _CRT_RAND_S
#include <stdlib.h>
#include <stdarg.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#include <io.h>
#endif

#if defined(__MINGW32__) || !defined(WINAPI_FAMILY_PARTITION) || !defined(WINAPI_PARTITION_DESKTOP)
#define BCTBX_WINDOWS_DESKTOP 1
#elif defined(WINAPI_FAMILY_PARTITION)
#if defined(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define BCTBX_WINDOWS_DESKTOP 1
#elif defined(WINAPI_PARTITION_PHONE_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
#define BCTBX_WINDOWS_PHONE 1
#elif defined(WINAPI_PARTITION_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#define BCTBX_WINDOWS_UNIVERSAL 1
#endif
#endif

#if defined(_WIN32) || defined(_WIN32_WCE)
#ifdef BCTBX_STATIC
#define BCTBX_PUBLIC
#define BCTBX_VAR_PUBLIC
#else
#ifdef BCTBX_EXPORTS
#define BCTBX_PUBLIC	__declspec(dllexport)
#define BCTBX_VAR_PUBLIC    extern __declspec(dllexport)
#else
#define BCTBX_PUBLIC	__declspec(dllimport)
#define BCTBX_VAR_PUBLIC    __declspec(dllimport)
#endif
#endif

#pragma push_macro("_WINSOCKAPI_")
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#define strtok_r strtok_s

typedef  unsigned __int64 uint64_t;
typedef  __int64 int64_t;
typedef  unsigned short uint16_t;
typedef  unsigned int uint32_t;
typedef  int int32_t;
typedef  unsigned char uint8_t;
typedef __int16 int16_t;
typedef long ssize_t;
#else
#include <stdint.h> /*provided by mingw32*/
#include <io.h>
#define BCTBX_PUBLIC
#define BCTBX_VAR_PUBLIC extern
BCTBX_PUBLIC char* strtok_r(char *str, const char *delim, char **nextp);
#endif

#define vsnprintf	_vsnprintf

typedef SOCKET bctbx_socket_t;
#ifdef BCTBX_WINDOWS_DESKTOP
typedef HANDLE bctbx_cond_t;
typedef HANDLE bctbx_mutex_t;
#else
typedef CONDITION_VARIABLE bctbx_cond_t;
typedef SRWLOCK bctbx_mutex_t;
#endif
typedef HANDLE bctbx_thread_t;

#define bctbx_thread_create     __bctbx_WIN_thread_create
#define bctbx_thread_join       __bctbx_WIN_thread_join
#define bctbx_thread_self       __bctbx_WIN_thread_self
#define bctbx_thread_exit(arg)
#define bctbx_mutex_init        __bctbx_WIN_mutex_init
#define bctbx_mutex_lock        __bctbx_WIN_mutex_lock
#define bctbx_mutex_unlock      __bctbx_WIN_mutex_unlock
#define bctbx_mutex_destroy     __bctbx_WIN_mutex_destroy
#define bctbx_cond_init         __bctbx_WIN_cond_init
#define bctbx_cond_signal       __bctbx_WIN_cond_signal
#define bctbx_cond_broadcast    __bctbx_WIN_cond_broadcast
#define bctbx_cond_wait         __bctbx_WIN_cond_wait
#define bctbx_cond_destroy      __bctbx_WIN_cond_destroy
#define bctbx_inet_aton         __bctbx_WIN_inet_aton


#ifdef __cplusplus
extern "C"
{
#endif

BCTBX_PUBLIC int __bctbx_WIN_mutex_init(bctbx_mutex_t *m, void *attr_unused);
BCTBX_PUBLIC int __bctbx_WIN_mutex_lock(bctbx_mutex_t *mutex);
BCTBX_PUBLIC int __bctbx_WIN_mutex_unlock(bctbx_mutex_t *mutex);
BCTBX_PUBLIC int __bctbx_WIN_mutex_destroy(bctbx_mutex_t *mutex);
BCTBX_PUBLIC int __bctbx_WIN_thread_create(bctbx_thread_t *t, void *attr_unused, void *(*func)(void*), void *arg);
BCTBX_PUBLIC int __bctbx_WIN_thread_join(bctbx_thread_t thread, void **unused);
BCTBX_PUBLIC unsigned long __bctbx_WIN_thread_self(void);
BCTBX_PUBLIC int __bctbx_WIN_cond_init(bctbx_cond_t *cond, void *attr_unused);
BCTBX_PUBLIC int __bctbx_WIN_cond_wait(bctbx_cond_t * cond, bctbx_mutex_t * mutex);
BCTBX_PUBLIC int __bctbx_WIN_cond_signal(bctbx_cond_t * cond);
BCTBX_PUBLIC int __bctbx_WIN_cond_broadcast(bctbx_cond_t * cond);
BCTBX_PUBLIC int __bctbx_WIN_cond_destroy(bctbx_cond_t * cond);
BCTBX_PUBLIC int __bctbx_WIN_inet_aton (const char * cp, struct in_addr * addr);

#ifdef __cplusplus
}
#endif

#define SOCKET_OPTION_VALUE	char *
#define BCTBX_INLINE			__inline

#if defined(_WIN32_WCE)

#define bctbx_log10f(x)		(float)log10 ((double)x)

#ifdef assert
	#undef assert
#endif /*assert*/
#define assert(exp)	((void)0)

#ifdef errno
	#undef errno
#endif /*errno*/
#define  errno GetLastError()
#ifdef strerror
		#undef strerror
#endif /*strerror*/
const char * bctbx_strerror(DWORD value);
#define strerror bctbx_strerror


#else /*_WIN32_WCE*/

#define bctbx_log10f(x)	log10f(x)

#endif

BCTBX_PUBLIC const char *__bctbx_getWinSocketError(int error);

#ifndef getSocketErrorCode
#define getSocketErrorCode()   WSAGetLastError()
#endif
#ifndef getSocketError
#define getSocketError()       __bctbx_getWinSocketError(WSAGetLastError())
#endif

#ifndef snprintf
#define snprintf _snprintf
#endif
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#ifndef strdup
#define strdup _strdup
#endif
#ifndef unlink
#define unlink _unlink
#endif

#ifndef F_OK
#define F_OK 00 /* Visual Studio does not define F_OK */
#endif


#ifdef __cplusplus
extern "C"{
#endif
BCTBX_PUBLIC int bctbx_gettimeofday (struct timeval *tv, void* tz);
#ifdef _WORKAROUND_MINGW32_BUGS
char * WSAAPI gai_strerror(int errnum);
#endif
#ifdef __cplusplus
}
#endif

#endif

#ifndef _BOOL_T_
#define _BOOL_T_
typedef unsigned char bool_t;
#endif /* _BOOL_T_ */
#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

#ifndef MIN
#define MIN(a,b) (((a)>(b)) ? (b) : (a))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

typedef struct bctoolboxTimeSpec{
	int64_t tv_sec;
	int64_t tv_nsec;
}bctoolboxTimeSpec;

#ifdef __cplusplus
extern "C"{
#endif

BCTBX_PUBLIC void* bctbx_malloc(size_t sz);
BCTBX_PUBLIC void bctbx_free(void *ptr);
BCTBX_PUBLIC void* bctbx_realloc(void *ptr, size_t sz);
BCTBX_PUBLIC void* bctbx_malloc0(size_t sz);
BCTBX_PUBLIC char * bctbx_strdup(const char *tmp);

/*override the allocator with this method, to be called BEFORE bctbx_init()*/
typedef struct _BctoolboxMemoryFunctions{
	void *(*malloc_fun)(size_t sz);
	void *(*realloc_fun)(void *ptr, size_t sz);
	void (*free_fun)(void *ptr);
}BctoolboxMemoryFunctions;

void bctbx_set_memory_functions(BctoolboxMemoryFunctions *functions);

#define bctbx_new(type,count)	(type*)bctbx_malloc(sizeof(type)*(count))
#define bctbx_new0(type,count)	(type*)bctbx_malloc0(sizeof(type)*(count))

BCTBX_PUBLIC int bctbx_socket_close(bctbx_socket_t sock);
BCTBX_PUBLIC int bctbx_socket_set_non_blocking(bctbx_socket_t sock);

BCTBX_PUBLIC char *bctbx_strndup(const char *str,int n);
BCTBX_PUBLIC char *bctbx_strdup_printf(const char *fmt,...);
BCTBX_PUBLIC char *bctbx_strdup_vprintf(const char *fmt, va_list ap);
BCTBX_PUBLIC char *bctbx_strcat_printf(char *dst, const char *fmt,...);
BCTBX_PUBLIC char *bctbx_strcat_vprintf(char *dst, const char *fmt, va_list ap);
BCTBX_PUBLIC char *bctbx_concat (const char *str, ...) ;
	
BCTBX_PUBLIC int bctbx_file_exist(const char *pathname);

BCTBX_PUBLIC void bctbx_get_cur_time(bctoolboxTimeSpec *ret);
void _bctbx_get_cur_time(bctoolboxTimeSpec *ret, bool_t realtime);
BCTBX_PUBLIC uint64_t bctbx_get_cur_time_ms(void);
BCTBX_PUBLIC void bctbx_sleep_ms(int ms);
BCTBX_PUBLIC void bctbx_sleep_until(const bctoolboxTimeSpec *ts);
BCTBX_PUBLIC int bctbx_timespec_compare(const bctoolboxTimeSpec *s1, const bctoolboxTimeSpec *s2);
BCTBX_PUBLIC unsigned int bctbx_random(void);


/* Portable and bug-less getaddrinfo */
BCTBX_PUBLIC int bctbx_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
BCTBX_PUBLIC void bctbx_freeaddrinfo(struct addrinfo *res);
BCTBX_PUBLIC int bctbx_addrinfo_to_ip_address(const struct addrinfo *ai, char *ip, size_t ip_size, int *port);
BCTBX_PUBLIC int bctbx_addrinfo_to_printable_ip_address(const struct addrinfo *ai, char *printable_ip, size_t printable_ip_size);
BCTBX_PUBLIC int bctbx_sockaddr_to_ip_address(struct sockaddr *sa, socklen_t salen, char *ip, size_t ip_size, int *port);
BCTBX_PUBLIC int bctbx_sockaddr_to_printable_ip_address(struct sockaddr *sa, socklen_t salen, char *printable_ip, size_t printable_ip_size);

/**
 * Convert a numeric ip address and port into an addrinfo, whose family will be as specified in the first argument.
 * If AF_INET6 is requested, the returned addrinfo will always be an IPv6 address, possibly V4MAPPED if the 
 * ip address was a v4 address.
 * Passing AF_UNSPEC to this function leads to unspecified results.
**/
BCTBX_PUBLIC struct addrinfo * bctbx_ip_address_to_addrinfo(int family, int socktype, const char *ipaddress, int port);
/**
 * Convert a name or ip address and port into an addrinfo, whose family will be as specified in the first argument.
 * If AF_INET6 is requested, the returned addrinfo will always be an IPv6 address, possibly a V4MAPPED if the 
 * ip address was a v4 address.
 * Passing AF_UNSPEC to this function leads to unspecified results.
**/
BCTBX_PUBLIC struct addrinfo * bctbx_name_to_addrinfo(int family, int socktype, const char *name, int port);

/**
 * This function will transform a V4 to V6 mapped address to a pure V4 and write it into result, or will just copy it otherwise.
 * The memory for v6 and result may be the same, in which case processing is done in place or no copy is done.
 * The pointer to result must have sufficient storage, typically a struct sockaddr_storage.
**/ 
BCTBX_PUBLIC void bctbx_sockaddr_remove_v4_mapping(const struct sockaddr *v6, struct sockaddr *result, socklen_t *result_len);

/**
 * Return TRUE if both families, ports and addresses are equals
 */
BCTBX_PUBLIC bool_t bctbx_sockaddr_equals(const struct sockaddr * sa, const struct sockaddr * sb) ;

/* portable named pipes  and shared memory*/
#if !defined(_WIN32_WCE)
#ifdef _WIN32
typedef HANDLE bctbx_pipe_t;
#define BCTBX_PIPE_INVALID INVALID_HANDLE_VALUE
#else
typedef int bctbx_pipe_t;
#define BCTBX_PIPE_INVALID (-1)
#endif

BCTBX_PUBLIC bctbx_pipe_t bctbx_server_pipe_create(const char *name);
/*
 * warning: on win32 bctbx_server_pipe_accept_client() might return INVALID_HANDLE_VALUE without
 * any specific error, this happens when bctbx_server_pipe_close() is called on another pipe.
 * This pipe api is not thread-safe.
*/
BCTBX_PUBLIC bctbx_pipe_t bctbx_server_pipe_accept_client(bctbx_pipe_t server);
BCTBX_PUBLIC int bctbx_server_pipe_close(bctbx_pipe_t spipe);
BCTBX_PUBLIC int bctbx_server_pipe_close_client(bctbx_pipe_t client);

BCTBX_PUBLIC bctbx_pipe_t bctbx_client_pipe_connect(const char *name);
BCTBX_PUBLIC int bctbx_client_pipe_close(bctbx_pipe_t sock);

BCTBX_PUBLIC int bctbx_pipe_read(bctbx_pipe_t p, uint8_t *buf, int len);
BCTBX_PUBLIC int bctbx_pipe_write(bctbx_pipe_t p, const uint8_t *buf, int len);

BCTBX_PUBLIC void *bctbx_shm_open(unsigned int keyid, int size, int create);
BCTBX_PUBLIC void bctbx_shm_close(void *memory);

BCTBX_PUBLIC bool_t bctbx_is_multicast_addr(const struct sockaddr *addr);
	
	
#endif

#ifdef __cplusplus
}

#endif


#ifndef IN6_IS_ADDR_MULTICAST
#define IN6_IS_ADDR_MULTICAST(i)	(((uint8_t *) (i))[0] == 0xff)
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

#endif


