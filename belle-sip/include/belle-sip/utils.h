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

#ifndef BELLE_SIP_UTILS_H
#define BELLE_SIP_UTILS_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "belle-sip/defs.h"


BELLE_SIP_BEGIN_DECLS

BELLESIP_EXPORT void *belle_sip_malloc(size_t size);
BELLESIP_EXPORT void *belle_sip_malloc0(size_t size);
BELLESIP_EXPORT void *belle_sip_realloc(void *ptr, size_t size);
BELLESIP_EXPORT void belle_sip_free(void *ptr);
BELLESIP_EXPORT char * belle_sip_strdup(const char *s);

BELLE_SIP_END_DECLS

/***************/
/* logging api */
/***************/

typedef enum {
	BELLE_SIP_LOG_FATAL=1,
	BELLE_SIP_LOG_ERROR=1<<1,
	BELLE_SIP_LOG_WARNING=1<<2,
	BELLE_SIP_LOG_MESSAGE=1<<3,
	BELLE_SIP_LOG_DEBUG=1<<4,
	BELLE_SIP_LOG_END=1<<5
} belle_sip_log_level;


typedef void (*belle_sip_log_function_t)(belle_sip_log_level lev, const char *fmt, va_list args);


typedef enum {
	BELLE_SIP_NOT_IMPLEMENTED = -2,
	BELLE_SIP_BUFFER_OVERFLOW = -1,
	BELLE_SIP_OK = 0
} belle_sip_error_code;


#ifdef __GNUC__
#define BELLE_SIP_CHECK_FORMAT_ARGS(m,n) __attribute__((format(printf,m,n)))
#else
#define BELLE_SIP_CHECK_FORMAT_ARGS(m,n)
#endif

BELLE_SIP_BEGIN_DECLS

BELLESIP_VAR_EXPORT belle_sip_log_function_t belle_sip_logv_out;

BELLESIP_VAR_EXPORT unsigned int __belle_sip_log_mask;

#define belle_sip_log_level_enabled(level)   (__belle_sip_log_mask & (level))

#if !defined(_WIN32) && !defined(_WIN32_WCE)
static BELLESIP_INLINE void belle_sip_logv(belle_sip_log_level level, const char *fmt, va_list args) {
        if (belle_sip_logv_out!=NULL && belle_sip_log_level_enabled(level))
                belle_sip_logv_out(level,fmt,args);
        if (level==BELLE_SIP_LOG_FATAL) abort();
}
#else
BELLESIP_EXPORT void belle_sip_logv(int level, const char *fmt, va_list args);
#endif


#ifdef BELLE_SIP_DEBUG_MODE
static BELLESIP_INLINE void belle_sip_debug(const char *fmt,...)
{
  va_list args;
  va_start (args, fmt);
  belle_sip_logv(BELLE_SIP_LOG_DEBUG, fmt, args);
  va_end (args);
}
#else

#define belle_sip_debug(...)

#endif

#ifdef BELLE_SIP_NOMESSAGE_MODE

#define belle_sip_log(...)
#define belle_sip_message(...)
#define belle_sip_warning(...)

#else

static BELLESIP_INLINE void BELLE_SIP_CHECK_FORMAT_ARGS(2,3) belle_sip_log(belle_sip_log_level lev, const char *fmt,...){
        va_list args;
        va_start (args, fmt);
        belle_sip_logv(lev, fmt, args);
        va_end (args);
}

static BELLESIP_INLINE void BELLE_SIP_CHECK_FORMAT_ARGS(1,2) belle_sip_message(const char *fmt,...)
{
        va_list args;
        va_start (args, fmt);
        belle_sip_logv(BELLE_SIP_LOG_MESSAGE, fmt, args);
        va_end (args);
}

static BELLESIP_INLINE void BELLE_SIP_CHECK_FORMAT_ARGS(1,2) belle_sip_warning(const char *fmt,...)
{
        va_list args;
        va_start (args, fmt);
        belle_sip_logv(BELLE_SIP_LOG_WARNING, fmt, args);
        va_end (args);
}

#endif

static BELLESIP_INLINE void BELLE_SIP_CHECK_FORMAT_ARGS(1,2) belle_sip_error(const char *fmt,...)
{
        va_list args;
        va_start (args, fmt);
        belle_sip_logv(BELLE_SIP_LOG_ERROR, fmt, args);
        va_end (args);
}

static BELLESIP_INLINE void BELLE_SIP_CHECK_FORMAT_ARGS(1,2) belle_sip_fatal(const char *fmt,...)
{
        va_list args;
        va_start (args, fmt);
        belle_sip_logv(BELLE_SIP_LOG_FATAL, fmt, args);
        va_end (args);
}



BELLESIP_EXPORT void belle_sip_set_log_file(FILE *file);
BELLESIP_EXPORT void belle_sip_set_log_handler(belle_sip_log_function_t func);
BELLESIP_EXPORT belle_sip_log_function_t belle_sip_get_log_handler(void);

BELLESIP_EXPORT char * BELLE_SIP_CHECK_FORMAT_ARGS(1,2) belle_sip_strdup_printf(const char *fmt,...);
BELLESIP_EXPORT char * belle_sip_strcat_vprintf(char* dst, const char *fmt, va_list ap);
BELLESIP_EXPORT char * BELLE_SIP_CHECK_FORMAT_ARGS(2,3) belle_sip_strcat_printf(char* dst, const char *fmt,...);

BELLESIP_EXPORT belle_sip_error_code BELLE_SIP_CHECK_FORMAT_ARGS(4,5) belle_sip_snprintf(char *buff, size_t buff_size, size_t *offset, const char *fmt, ...);
BELLESIP_EXPORT belle_sip_error_code belle_sip_snprintf_valist(char *buff, size_t buff_size, size_t *offset, const char *fmt, va_list args);

BELLESIP_EXPORT void belle_sip_set_log_level(int level);

BELLESIP_EXPORT char * belle_sip_random_token(char *ret, size_t size);

BELLESIP_EXPORT unsigned char * belle_sip_random_bytes(unsigned char *ret, size_t size);

BELLESIP_EXPORT char * belle_sip_octets_to_text(const unsigned char *hash, size_t hash_len, char *ret, size_t size);

BELLESIP_EXPORT char * belle_sip_create_tag(char *ret, size_t size);

BELLESIP_EXPORT const char* belle_sip_version_to_string(void);

/**
 * Returns string without surrounding quotes if any, else just call belle_sip_strdup().
**/
BELLESIP_EXPORT char *belle_sip_unquote_strdup(const char *str);

BELLESIP_EXPORT uint64_t belle_sip_time_ms(void);

BELLESIP_EXPORT unsigned int belle_sip_random(void);

#if defined(_WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET belle_sip_socket_t;
typedef HANDLE belle_sip_fd_t;
#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

typedef int belle_sip_socket_t;
typedef int belle_sip_fd_t;

#endif

BELLESIP_EXPORT int belle_sip_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
BELLESIP_EXPORT void belle_sip_freeaddrinfo(struct addrinfo *res);


typedef void (*belle_sip_background_task_end_callback_t)(void *);
BELLESIP_EXPORT unsigned long belle_sip_begin_background_task(const char *name, belle_sip_background_task_end_callback_t cb, void *data);
BELLESIP_EXPORT void belle_sip_end_background_task(unsigned long id);

/**
 * create a directory if it doesn't already exists
 *
 * @param[in]   path        The directory to be created
 * @return 0 in case of succes, -1 otherwise, note it returns -1 if the directory already exists
 */
BELLESIP_EXPORT int belle_sip_mkdir(const char *path);

BELLE_SIP_END_DECLS

#endif

