/*
  bctoolobx
  Copyright (C) 2016 Belledonne Communications, France, Grenoble

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

/**
 * \file logging.h
 * \brief Logging API.
 *
**/

#ifndef BCTBX_LOGGING_H
#define BCTBX_LOGGING_H

#include <bctoolbox/port.h>

#ifndef BCTBX_LOG_DOMAIN
#define BCTBX_LOG_DOMAIN NULL
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	BCTBX_LOG_DEBUG=1,
	BCTBX_LOG_TRACE=1<<1,
	BCTBX_LOG_MESSAGE=1<<2,
	BCTBX_LOG_WARNING=1<<3,
	BCTBX_LOG_ERROR=1<<4,
	BCTBX_LOG_FATAL=1<<5,
	BCTBX_LOG_LOGLEV_END=1<<6
} BctbxLogLevel;


typedef void (*BctoolboxLogFunc)(const char *domain, BctbxLogLevel lev, const char *fmt, va_list args);

BCTBX_PUBLIC void bctbx_set_log_file(FILE *file);
BCTBX_PUBLIC void bctbx_set_log_handler(BctoolboxLogFunc func);
BCTBX_PUBLIC BctoolboxLogFunc bctbx_get_log_handler(void);

BCTBX_PUBLIC void bctbx_logv_out(const char *domain, BctbxLogLevel level, const char *fmt, va_list args);

#define bctbx_log_level_enabled(domain, level)	(bctbx_get_log_level_mask(domain) & (level))

BCTBX_PUBLIC void bctbx_logv(const char *domain, BctbxLogLevel level, const char *fmt, va_list args);

/**
 * Flushes the log output queue.
 * WARNING: Must be called from the thread that has been defined with bctbx_set_log_thread_id().
 */
BCTBX_PUBLIC void bctbx_logv_flush(void);

/**
 * Activate all log level greater or equal than specified level argument.
**/
BCTBX_PUBLIC void bctbx_set_log_level(const char *domain, BctbxLogLevel level);

BCTBX_PUBLIC void bctbx_set_log_level_mask(const char *domain, int levelmask);
BCTBX_PUBLIC unsigned int bctbx_get_log_level_mask(const char *domain);

/**
 * Tell oRTP the id of the thread used to output the logs.
 * This is meant to output all the logs from the same thread to prevent deadlock problems at the application level.
 * @param[in] thread_id The id of the thread that will output the logs (can be obtained using bctbx_thread_self()).
 */
BCTBX_PUBLIC void bctbx_set_log_thread_id(unsigned long thread_id);

#ifdef __GNUC__
#define CHECK_FORMAT_ARGS(m,n) __attribute__((format(printf,m,n)))
#else
#define CHECK_FORMAT_ARGS(m,n)
#endif
#ifdef __clang__
/*in case of compile with -g static inline can produce this type of warning*/
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#ifdef BCTBX_DEBUG_MODE
static BCTBX_INLINE void CHECK_FORMAT_ARGS(1,2) bctbx_debug(const char *fmt,...)
{
  va_list args;
  va_start (args, fmt);
  bctbx_logv(BCTBX_LOG_DOMAIN, BCTBX_LOG_DEBUG, fmt, args);
  va_end (args);
}
#else

#define bctbx_debug(...)

#endif

#ifdef BCTBX_NOMESSAGE_MODE

#define bctbx_log(...)
#define bctbx_message(...)
#define bctbx_warning(...)

#else

static BCTBX_INLINE void bctbx_log(const char* domain, BctbxLogLevel lev, const char *fmt,...) {
	va_list args;
	va_start (args, fmt);
	bctbx_logv(domain, lev, fmt, args);
	va_end (args);
}

static BCTBX_INLINE void CHECK_FORMAT_ARGS(1,2) bctbx_message(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	bctbx_logv(BCTBX_LOG_DOMAIN, BCTBX_LOG_MESSAGE, fmt, args);
	va_end (args);
}

static BCTBX_INLINE void CHECK_FORMAT_ARGS(1,2) bctbx_warning(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	bctbx_logv(BCTBX_LOG_DOMAIN, BCTBX_LOG_WARNING, fmt, args);
	va_end (args);
}

#endif

static BCTBX_INLINE void CHECK_FORMAT_ARGS(1,2) bctbx_error(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	bctbx_logv(BCTBX_LOG_DOMAIN, BCTBX_LOG_ERROR, fmt, args);
	va_end (args);
}

static BCTBX_INLINE void CHECK_FORMAT_ARGS(1,2) bctbx_fatal(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	bctbx_logv(BCTBX_LOG_DOMAIN, BCTBX_LOG_FATAL, fmt, args);
	va_end (args);
}


#ifdef __QNX__
void bctbx_qnx_log_handler(const char *domain, BctbxLogLevel lev, const char *fmt, va_list args);
#endif


#ifdef __cplusplus
}

#include <string>
#include <iostream>
#include <sstream>

#if !defined(_WIN32) && !defined(__QNX__)
#include <syslog.h>
#endif

namespace bctoolbox {
	namespace log {
		
		// Here we define our application severity levels.
		enum level { normal, trace, debug, info, warning, error, fatal };
		
		// The formatting logic for the severity level
		template <typename CharT, typename TraitsT>
		inline std::basic_ostream<CharT, TraitsT> &operator<<(std::basic_ostream<CharT, TraitsT> &strm,
															  const bctoolbox::log::level &lvl) {
			static const char *const str[] = {"normal", "trace", "debug", "info", "warning", "error", "fatal"};
			if (static_cast<std::size_t>(lvl) < (sizeof(str) / sizeof(*str)))
				strm << str[lvl];
			else
				strm << static_cast<int>(lvl);
			return strm;
		}
		
		template <typename CharT, typename TraitsT>
		inline std::basic_istream<CharT, TraitsT> &operator>>(std::basic_istream<CharT, TraitsT> &strm,
															  bctoolbox::log::level &lvl) {
			static const char *const str[] = {"normal", "trace", "debug", "info", "warning", "error", "fatal"};
			
			std::string s;
			strm >> s;
			for (unsigned int n = 0; n < (sizeof(str) / sizeof(*str)); ++n) {
				if (s == str[n]) {
					lvl = static_cast<bctoolbox::log::level>(n);
					return strm;
				}
			}
			// Parse error
			strm.setstate(std::ios_base::failbit);
			return strm;
		}
	}
}



#include <ostream>

struct pumpstream : public std::ostringstream {
	const std::string mDomain;
	const BctbxLogLevel level;
	pumpstream(const std::string &domain, BctbxLogLevel l) : mDomain(domain), level(l) {}
	
	~pumpstream() {
		bctbx_log(mDomain.empty()?NULL:mDomain.c_str(), level, "%s", str().c_str());
	}
};

#if (__GNUC__ == 4 && __GNUC_MINOR__ < 5 && __cplusplus > 199711L)
template <typename _Tp> inline pumpstream &operator<<(pumpstream &&__os, const _Tp &__x) {
	(static_cast<std::ostringstream &>(__os)) << __x;
	return __os;
}
#endif

#define BCTBX_SLOG(domain, thelevel) \
\
if (bctbx_log_level_enabled((domain), (thelevel))) \
		pumpstream((domain?domain:""),(thelevel))

#define BCTBX_SLOGD(DOMAIN) BCTBX_SLOG(DOMAIN, BCTBX_LOG_DEBUG)
#define BCTBX_SLOGI(DOMAIN) BCTBX_SLOG((DOMAIN), (BCTBX_LOG_MESSAGE))
#define BCTBX_SLOGW(DOMAIN) BCTBX_SLOG(DOMAIN, BCTBX_LOG_WARNING)
#define BCTBX_SLOGE(DOMAIN) BCTBX_SLOG(DOMAIN, BCTBX_LOG_ERROR)


#endif

#endif
