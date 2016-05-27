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

#ifdef BCTBX_LOG_DOMAIN
#undef BCTBX_LOG_DOMAIN
#endif
#ifndef BELLE_SIP_LOG_DOMAIN
#define BELLE_SIP_LOG_DOMAIN "belle-sip"
#endif

#define BCTBX_LOG_DOMAIN BELLE_SIP_LOG_DOMAIN


#include "bctoolbox/logging.h"

BELLE_SIP_BEGIN_DECLS

#define belle_sip_malloc bctbx_malloc
#define belle_sip_malloc0 bctbx_malloc0
#define belle_sip_realloc bctbx_realloc
#define belle_sip_free bctbx_free
#define belle_sip_strdup bctbx_strdup

BELLE_SIP_END_DECLS

/***************/
/* logging api */
/***************/

#define BELLE_SIP_LOG_FATAL BCTBX_LOG_FATAL
#define	BELLE_SIP_LOG_ERROR BCTBX_LOG_ERROR
#define	BELLE_SIP_LOG_WARNING BCTBX_LOG_WARNING
#define	BELLE_SIP_LOG_MESSAGE BCTBX_LOG_MESSAGE
#define	BELLE_SIP_LOG_DEBUG	BCTBX_LOG_DEBUG
#define	BELLE_SIP_LOG_END BCTBX_LOG_END
#define  belle_sip_log_level BctbxLogLevel

#define belle_sip_log_function_t BctoolboxLogFunc


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

#define belle_sip_log_level_enabled(level) bctbx_log_level_enabled(BELLE_SIP_LOG_DOMAIN,level)

#ifdef BELLE_SIP_DEBUG_MODE
#define belle_sip_deb(...) bctbx_debug(...)
#else

#define belle_sip_debug(...)

#endif

#ifdef BELLE_SIP_NOMESSAGE_MODE

#define belle_sip_log(...)
#define belle_sip_message(...)
#define belle_sip_warning(...)

#else

#define belle_sip_log bctbx_log
#define belle_sip_message bctbx_message
#define belle_sip_warning bctbx_warning
#define belle_sip_error bctbx_error
#define belle_sip_fatal bctbx_fatal
#define belle_sip_logv bctbx_logv
#endif


#define belle_sip_set_log_file bctbx_set_log_file
#define belle_sip_set_log_handler bctbx_set_log_handler
#define belle_sip_get_log_handler bctbx_get_log_handler

#define belle_sip_strdup_printf bctbx_strdup_printf
#define belle_sip_strcat_vprintf bctbx_strcat_vprintf
#define belle_sip_strcat_printf bctbx_strcat_printf

BELLESIP_EXPORT belle_sip_error_code BELLE_SIP_CHECK_FORMAT_ARGS(4,5) belle_sip_snprintf(char *buff, size_t buff_size, size_t *offset, const char *fmt, ...);
BELLESIP_EXPORT belle_sip_error_code belle_sip_snprintf_valist(char *buff, size_t buff_size, size_t *offset, const char *fmt, va_list args);

#define belle_sip_set_log_level(level) bctbx_set_log_level(BELLE_SIP_LOG_DOMAIN,level);

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

