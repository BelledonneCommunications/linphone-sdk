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

#ifndef BCTOOLBOX_PORT_H
#define BCTOOLBOX_PORT_H


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

#ifndef MIN
#define MIN(a,b) (((a)>(b)) ? (b) : (a))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif


typedef int bctoolbox_socket_t;
typedef pthread_t bctoolbox_thread_t;
typedef pthread_mutex_t bctoolbox_mutex_t;
typedef pthread_cond_t bctoolbox_cond_t;

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

#define BCTOOLBOX_PUBLIC
#define BCTOOLBOX_INLINE			inline

#ifdef __cplusplus
extern "C"
{
#endif

int __bctoolbox_thread_join(bctoolbox_thread_t thread, void **ptr);
int __bctoolbox_thread_create(bctoolbox_thread_t *thread, pthread_attr_t *attr, void * (*routine)(void*), void *arg);
unsigned long __bctoolbox_thread_self(void);

#ifdef __cplusplus
}
#endif

#define bctoolbox_thread_create	__bctoolbox_thread_create
#define bctoolbox_thread_join	__bctoolbox_thread_join
#define bctoolbox_thread_self	__bctoolbox_thread_self
#define bctoolbox_thread_exit	pthread_exit
#define bctoolbox_mutex_init		pthread_mutex_init
#define bctoolbox_mutex_lock		pthread_mutex_lock
#define bctoolbox_mutex_unlock	pthread_mutex_unlock
#define bctoolbox_mutex_destroy	pthread_mutex_destroy
#define bctoolbox_cond_init		pthread_cond_init
#define bctoolbox_cond_signal	pthread_cond_signal
#define bctoolbox_cond_broadcast	pthread_cond_broadcast
#define bctoolbox_cond_wait		pthread_cond_wait
#define bctoolbox_cond_destroy	pthread_cond_destroy

#define SOCKET_OPTION_VALUE	void *
#define SOCKET_BUFFER		void *

#define getSocketError() strerror(errno)
#define getSocketErrorCode() (errno)
#define bctoolbox_gettimeofday(tv,tz) gettimeofday(tv,tz)
#define bctoolbox_log10f(x)	log10f(x)


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
#define BCTOOLBOX_WINDOWS_DESKTOP 1
#elif defined(WINAPI_FAMILY_PARTITION)
#if defined(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define BCTOOLBOX_WINDOWS_DESKTOP 1
#elif defined(WINAPI_PARTITION_PHONE_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
#define BCTOOLBOX_WINDOWS_PHONE 1
#elif defined(WINAPI_PARTITION_APP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#define BCTOOLBOX_WINDOWS_UNIVERSAL 1
#endif
#endif

#ifdef _MSC_VER
#ifdef BCTOOLBOX_STATIC
#define BCTOOLBOX_PUBLIC
#else
#ifdef BCTOOLBOX_EXPORTS
#define BCTOOLBOX_PUBLIC	__declspec(dllexport)
#else
#define BCTOOLBOX_PUBLIC	__declspec(dllimport)
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
#else
#include <stdint.h> /*provided by mingw32*/
#include <io.h>
#define BCTOOLBOX_PUBLIC
BCTOOLBOX_PUBLIC char* strtok_r(char *str, const char *delim, char **nextp);
#endif

#define vsnprintf	_vsnprintf

typedef SOCKET bctoolbox_socket_t;
#ifdef BCTOOLBOX_WINDOWS_DESKTOP
typedef HANDLE bctoolbox_cond_t;
typedef HANDLE bctoolbox_mutex_t;
#else
typedef CONDITION_VARIABLE bctoolbox_cond_t;
typedef SRWLOCK bctoolbox_mutex_t;
#endif
typedef HANDLE bctoolbox_thread_t;

#define bctoolbox_thread_create	WIN_thread_create
#define bctoolbox_thread_join	WIN_thread_join
#define bctoolbox_thread_self	WIN_thread_self
#define bctoolbox_thread_exit(arg)
#define bctoolbox_mutex_init		WIN_mutex_init
#define bctoolbox_mutex_lock		WIN_mutex_lock
#define bctoolbox_mutex_unlock	WIN_mutex_unlock
#define bctoolbox_mutex_destroy	WIN_mutex_destroy
#define bctoolbox_cond_init		WIN_cond_init
#define bctoolbox_cond_signal	WIN_cond_signal
#define bctoolbox_cond_broadcast	WIN_cond_broadcast
#define bctoolbox_cond_wait		WIN_cond_wait
#define bctoolbox_cond_destroy	WIN_cond_destroy


#ifdef __cplusplus
extern "C"
{
#endif

BCTOOLBOX_PUBLIC int WIN_mutex_init(bctoolbox_mutex_t *m, void *attr_unused);
BCTOOLBOX_PUBLIC int WIN_mutex_lock(bctoolbox_mutex_t *mutex);
BCTOOLBOX_PUBLIC int WIN_mutex_unlock(bctoolbox_mutex_t *mutex);
BCTOOLBOX_PUBLIC int WIN_mutex_destroy(bctoolbox_mutex_t *mutex);
BCTOOLBOX_PUBLIC int WIN_thread_create(bctoolbox_thread_t *t, void *attr_unused, void *(*func)(void*), void *arg);
BCTOOLBOX_PUBLIC int WIN_thread_join(bctoolbox_thread_t thread, void **unused);
BCTOOLBOX_PUBLIC unsigned long WIN_thread_self(void);
BCTOOLBOX_PUBLIC int WIN_cond_init(bctoolbox_cond_t *cond, void *attr_unused);
BCTOOLBOX_PUBLIC int WIN_cond_wait(bctoolbox_cond_t * cond, bctoolbox_mutex_t * mutex);
BCTOOLBOX_PUBLIC int WIN_cond_signal(bctoolbox_cond_t * cond);
BCTOOLBOX_PUBLIC int WIN_cond_broadcast(bctoolbox_cond_t * cond);
BCTOOLBOX_PUBLIC int WIN_cond_destroy(bctoolbox_cond_t * cond);

#ifdef __cplusplus
}
#endif

#define SOCKET_OPTION_VALUE	char *
#define BCTOOLBOX_INLINE			__inline

#if defined(_WIN32_WCE)

#define bctoolbox_log10f(x)		(float)log10 ((double)x)

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
const char * bctoolbox_strerror(DWORD value);
#define strerror bctoolbox_strerror


#else /*_WIN32_WCE*/

#define bctoolbox_log10f(x)	log10f(x)

#endif

BCTOOLBOX_PUBLIC const char *getWinSocketError(int error);
#define getSocketErrorCode() WSAGetLastError()
#define getSocketError() getWinSocketError(WSAGetLastError())

#define snprintf _snprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#ifndef F_OK
#define F_OK 00 /* Visual Studio does not define F_OK */
#endif


#ifdef __cplusplus
extern "C"{
#endif
BCTOOLBOX_PUBLIC int bctoolbox_gettimeofday (struct timeval *tv, void* tz);
#ifdef _WORKAROUND_MINGW32_BUGS
char * WSAAPI gai_strerror(int errnum);
#endif
#ifdef __cplusplus
}
#endif

#endif

typedef unsigned char bool_t;
#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

typedef struct _OList OList;

typedef struct bctoolboxTimeSpec{
	int64_t tv_sec;
	int64_t tv_nsec;
}bctoolboxTimeSpec;

#ifdef __cplusplus
extern "C"{
#endif

BCTOOLBOX_PUBLIC void* bctoolbox_malloc(size_t sz);
BCTOOLBOX_PUBLIC void bctoolbox_free(void *ptr);
BCTOOLBOX_PUBLIC void* bctoolbox_realloc(void *ptr, size_t sz);
BCTOOLBOX_PUBLIC void* bctoolbox_malloc0(size_t sz);
BCTOOLBOX_PUBLIC char * bctoolbox_strdup(const char *tmp);

/*override the allocator with this method, to be called BEFORE bctoolbox_init()*/
typedef struct _BctoolboxMemoryFunctions{
	void *(*malloc_fun)(size_t sz);
	void *(*realloc_fun)(void *ptr, size_t sz);
	void (*free_fun)(void *ptr);
}BctoolboxMemoryFunctions;

void bctoolbox_set_memory_functions(BctoolboxMemoryFunctions *functions);

#define bctoolbox_new(type,count)	(type*)bctoolbox_malloc(sizeof(type)*(count))
#define bctoolbox_new0(type,count)	(type*)bctoolbox_malloc0(sizeof(type)*(count))

BCTOOLBOX_PUBLIC int bctoolbox_socket_close(bctoolbox_socket_t sock);
BCTOOLBOX_PUBLIC int bctoolbox_socket_set_non_blocking(bctoolbox_socket_t sock);

BCTOOLBOX_PUBLIC char *bctoolbox_strndup(const char *str,int n);
BCTOOLBOX_PUBLIC char *bctoolbox_strdup_printf(const char *fmt,...);
BCTOOLBOX_PUBLIC char *bctoolbox_strdup_vprintf(const char *fmt, va_list ap);
BCTOOLBOX_PUBLIC char *bctoolbox_strcat_printf(char *dst, const char *fmt,...);
BCTOOLBOX_PUBLIC char *bctoolbox_strcat_vprintf(char *dst, const char *fmt, va_list ap);

BCTOOLBOX_PUBLIC int bctoolbox_file_exist(const char *pathname);

BCTOOLBOX_PUBLIC void bctoolbox_get_cur_time(bctoolboxTimeSpec *ret);
void _bctoolbox_get_cur_time(bctoolboxTimeSpec *ret, bool_t realtime);
BCTOOLBOX_PUBLIC uint64_t bctoolbox_get_cur_time_ms(void);
BCTOOLBOX_PUBLIC void bctoolbox_sleep_ms(int ms);
BCTOOLBOX_PUBLIC void bctoolbox_sleep_until(const bctoolboxTimeSpec *ts);
BCTOOLBOX_PUBLIC int bctoolbox_timespec_compare(const bctoolboxTimeSpec *s1, const bctoolboxTimeSpec *s2);
BCTOOLBOX_PUBLIC unsigned int bctoolbox_random(void);

/* portable named pipes  and shared memory*/
#if !defined(_WIN32_WCE)
#ifdef _WIN32
typedef HANDLE bctoolbox_pipe_t;
#define BCTOOLBOX_PIPE_INVALID INVALID_HANDLE_VALUE
#else
typedef int bctoolbox_pipe_t;
#define BCTOOLBOX_PIPE_INVALID (-1)
#endif

BCTOOLBOX_PUBLIC bctoolbox_pipe_t bctoolbox_server_pipe_create(const char *name);
/*
 * warning: on win32 bctoolbox_server_pipe_accept_client() might return INVALID_HANDLE_VALUE without
 * any specific error, this happens when bctoolbox_server_pipe_close() is called on another pipe.
 * This pipe api is not thread-safe.
*/
BCTOOLBOX_PUBLIC bctoolbox_pipe_t bctoolbox_server_pipe_accept_client(bctoolbox_pipe_t server);
BCTOOLBOX_PUBLIC int bctoolbox_server_pipe_close(bctoolbox_pipe_t spipe);
BCTOOLBOX_PUBLIC int bctoolbox_server_pipe_close_client(bctoolbox_pipe_t client);

BCTOOLBOX_PUBLIC bctoolbox_pipe_t bctoolbox_client_pipe_connect(const char *name);
BCTOOLBOX_PUBLIC int bctoolbox_client_pipe_close(bctoolbox_pipe_t sock);

BCTOOLBOX_PUBLIC int bctoolbox_pipe_read(bctoolbox_pipe_t p, uint8_t *buf, int len);
BCTOOLBOX_PUBLIC int bctoolbox_pipe_write(bctoolbox_pipe_t p, const uint8_t *buf, int len);

BCTOOLBOX_PUBLIC void *bctoolbox_shm_open(unsigned int keyid, int size, int create);
BCTOOLBOX_PUBLIC void bctoolbox_shm_close(void *memory);

BCTOOLBOX_PUBLIC	bool_t bctoolbox_is_multicast_addr(const struct sockaddr *addr);
	
	
#endif

#ifdef __cplusplus
}

#endif


#if (defined(_WIN32) || defined(_WIN32_WCE)) && !defined(BCTOOLBOX_STATIC)
#ifdef BCTOOLBOX_EXPORTS
   #define BCTOOLBOX_VAR_PUBLIC    extern __declspec(dllexport)
#else
   #define BCTOOLBOX_VAR_PUBLIC    __declspec(dllimport)
#endif
#else
   #define BCTOOLBOX_VAR_PUBLIC    extern
#endif

#ifndef IN6_IS_ADDR_MULTICAST
#define IN6_IS_ADDR_MULTICAST(i)	(((uint8_t *) (i))[0] == 0xff)
#endif

#endif


