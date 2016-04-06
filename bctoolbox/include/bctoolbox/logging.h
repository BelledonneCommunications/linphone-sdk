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

#ifndef BCTOOLBOX_LOGGING_H
#define BCTOOLBOX_LOGGING_H

#include <bctoolbox/port.h>

#ifndef BCTOOLBOX_LOG_DOMAIN
#define BCTOOLBOX_LOG_DOMAIN NULL
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	BCTOOLBOX_DEBUG=1,
	BCTOOLBOX_TRACE=1<<1,
	BCTOOLBOX_MESSAGE=1<<2,
	BCTOOLBOX_WARNING=1<<3,
	BCTOOLBOX_ERROR=1<<4,
	BCTOOLBOX_FATAL=1<<5,
	BCTOOLBOX_LOGLEV_END=1<<6
} BctoolboxLogLevel;


typedef void (*BctoolboxLogFunc)(const char *domain, BctoolboxLogLevel lev, const char *fmt, va_list args);

BCTOOLBOX_PUBLIC void bctoolbox_set_log_file(FILE *file);
BCTOOLBOX_PUBLIC void bctoolbox_set_log_handler(BctoolboxLogFunc func);
BCTOOLBOX_PUBLIC BctoolboxLogFunc bctoolbox_get_log_handler(void);

BCTOOLBOX_PUBLIC void bctoolbox_logv_out(const char *domain, BctoolboxLogLevel level, const char *fmt, va_list args);

#define bctoolbox_log_level_enabled(domain, level)	(bctoolbox_get_log_level_mask(domain) & (level))

BCTOOLBOX_PUBLIC void bctoolbox_logv(const char *domain, BctoolboxLogLevel level, const char *fmt, va_list args);

/**
 * Flushes the log output queue.
 * WARNING: Must be called from the thread that has been defined with bctoolbox_set_log_thread_id().
 */
BCTOOLBOX_PUBLIC void bctoolbox_logv_flush(void);

/**
 * Activate all log level greater or equal than specified level argument.
**/
BCTOOLBOX_PUBLIC void bctoolbox_set_log_level(const char *domain, BctoolboxLogLevel level);

BCTOOLBOX_PUBLIC void bctoolbox_set_log_level_mask(const char *domain, int levelmask);
BCTOOLBOX_PUBLIC unsigned int bctoolbox_get_log_level_mask(const char *domain);

/**
 * Tell oRTP the id of the thread used to output the logs.
 * This is meant to output all the logs from the same thread to prevent deadlock problems at the application level.
 * @param[in] thread_id The id of the thread that will output the logs (can be obtained using bctoolbox_thread_self()).
 */
BCTOOLBOX_PUBLIC void bctoolbox_set_log_thread_id(unsigned long thread_id);

#ifdef __GNUC__
#define CHECK_FORMAT_ARGS(m,n) __attribute__((format(printf,m,n)))
#else
#define CHECK_FORMAT_ARGS(m,n)
#endif
#ifdef __clang__
/*in case of compile with -g static inline can produce this type of warning*/
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#ifdef BCTOOLBOX_DEBUG_MODE
static BCTOOLBOX_INLINE void CHECK_FORMAT_ARGS(1,2) bctoolbox_debug(const char *fmt,...)
{
  va_list args;
  va_start (args, fmt);
  bctoolbox_logv(BCTOOLBOX_LOG_DOMAIN, BCTOOLBOX_DEBUG, fmt, args);
  va_end (args);
}
#else

#define bctoolbox_debug(...)

#endif

#ifdef BCTOOLBOX_NOMESSAGE_MODE

#define bctoolbox_log(...)
#define bctoolbox_message(...)
#define bctoolbox_warning(...)

#else

static BCTOOLBOX_INLINE void bctoolbox_log(const char* domain, BctoolboxLogLevel lev, const char *fmt,...) {
	va_list args;
	va_start (args, fmt);
	bctoolbox_logv(domain, lev, fmt, args);
	va_end (args);
}

static BCTOOLBOX_INLINE void CHECK_FORMAT_ARGS(1,2) bctoolbox_message(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	bctoolbox_logv(BCTOOLBOX_LOG_DOMAIN, BCTOOLBOX_MESSAGE, fmt, args);
	va_end (args);
}

static BCTOOLBOX_INLINE void CHECK_FORMAT_ARGS(1,2) bctoolbox_warning(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	bctoolbox_logv(BCTOOLBOX_LOG_DOMAIN, BCTOOLBOX_WARNING, fmt, args);
	va_end (args);
}

#endif

static BCTOOLBOX_INLINE void CHECK_FORMAT_ARGS(1,2) bctoolbox_error(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	bctoolbox_logv(BCTOOLBOX_LOG_DOMAIN, BCTOOLBOX_ERROR, fmt, args);
	va_end (args);
}

static BCTOOLBOX_INLINE void CHECK_FORMAT_ARGS(1,2) bctoolbox_fatal(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	bctoolbox_logv(BCTOOLBOX_LOG_DOMAIN, BCTOOLBOX_FATAL, fmt, args);
	va_end (args);
}


#ifdef __QNX__
void bctoolbox_qnx_log_handler(const char *domain, BctoolboxLogLevel lev, const char *fmt, va_list args);
#endif


#ifdef __cplusplus
}

#include <string>
#include <iostream>
#include <sstream>
#include <syslog.h>

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
	const BctoolboxLogLevel level;
	pumpstream(std::string domain, BctoolboxLogLevel l) : mDomain(domain), level(l) {
	}
	
	~pumpstream() {
		bctoolbox_log(mDomain.c_str(), level, "%s", str().c_str());
	}
};

//#if (__GNUC__ == 4 && __GNUC_MINOR__ < 5)
//template <typename _Tp> inline pumpstream &operator<<(pumpstream &&__os, const _Tp &__x) {
//	(static_cast<std::ostringstream &>(__os)) << __x;
//	return __os;
//}
//#endif

#define BCTOOLBOX_SLOG(domain, thelevel) \
\
if (bctoolbox_log_level_enabled((domain), (thelevel))) \
	pumpstream((domain),(thelevel))

#define BCTOOLBOX_SLOGD(DOMAIN) BCTOOLBOX_SLOG(DOMAIN, BCTOOLBOX_DEBUG)
#define BCTOOLBOX_SLOGI(DOMAIN) BCTOOLBOX_SLOG((DOMAIN), (BCTOOLBOX_MESSAGE))
#define BCTOOLBOX_SLOGW(DOMAIN) BCTOOLBOX_SLOG(DOMAIN, BCTOOLBOX_WARNING)
#define BCTOOLBOX_SLOGE(DOMAIN) BCTOOLBOX_SLOG(DOMAIN, BCTOOLBOX_ERROR)

#endif

#endif
