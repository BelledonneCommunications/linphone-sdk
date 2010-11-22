
#ifndef belle_utils_h
#define belle_utils_h

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>

/* include all public headers*/
#include "belle-sip/belle-sip.h"


struct _belle_sip_list {
	struct _belle_sip_list *next;
	struct _belle_sip_list *prev;
	void *data;
};

typedef void (*belle_sip_source_remove_callback_t)(belle_sip_source_t *);

struct belle_sip_source{
	belle_sip_list_t node;
	unsigned long id;
	int fd;
	unsigned int events;
	int timeout;
	void *data;
	uint64_t expire_ms;
	int index; /* index in pollfd table */
	belle_sip_source_func_t notify;
	belle_sip_source_remove_callback_t on_remove;
};

void belle_sip_fd_source_init(belle_sip_source_t *s, belle_sip_source_func_t func, void *data, int fd, unsigned int events, unsigned int timeout_value_ms);

#define belle_list_next(elem) ((elem)->next)


#ifdef __cplusplus
extern "C"{
#endif

void *belle_sip_malloc(size_t size);
void *belle_sip_malloc0(size_t size);
void *belle_sip_realloc(void *ptr, size_t size);
void belle_sip_free(void *ptr);
char * belle_sip_strdup(const char *s);

#define belle_sip_new(type) (type*)belle_sip_malloc(sizeof(type))
#define belle_sip_new0(type) (type*)belle_sip_malloc0(sizeof(type))
	
belle_sip_list_t *belle_sip_list_new(void *data);
belle_sip_list_t*  belle_sip_list_append_link(belle_sip_list_t* elem,belle_sip_list_t *new_elem);
belle_sip_list_t * belle_sip_list_free(belle_sip_list_t *list);
#define belle_sip_list_next(elem) ((elem)->next)
/***************/
/* logging api */
/***************/

typedef enum {
        BELLE_SIP_DEBUG=1,
        BELLE_SIP_MESSAGE=1<<1,
        BELLE_SIP_WARNING=1<<2,
        BELLE_SIP_ERROR=1<<3,
        BELLE_SIP_FATAL=1<<4,
        BELLE_SIP_LOGLEV_END=1<<5
} belle_sip_log_level;


typedef void (*belle_sip_log_function_t)(belle_sip_log_level lev, const char *fmt, va_list args);

void belle_sip_set_log_file(FILE *file);
void belle_sip_set_log_handler(belle_sip_log_function_t func);

extern belle_sip_log_function_t belle_sip_logv_out;

extern unsigned int __belle_sip_log_mask;

#define belle_sip_log_level_enabled(level)   (__belle_sip_log_mask & (level))

#if !defined(WIN32) && !defined(_WIN32_WCE)
#define belle_sip_logv(level,fmt,args) \
{\
        if (belle_sip_logv_out!=NULL && belle_sip_log_level_enabled(level)) \
                belle_sip_logv_out(level,fmt,args);\
        if ((level)==BELLE_SIP_FATAL) abort();\
}while(0)
#else
void belle_sip_logv(int level, const char *fmt, va_list args);
#endif

void belle_sip_set_log_level_mask(int levelmask);

#ifdef BELLE_SIP_DEBUG_MODE
static inline void belle_sip_debug(const char *fmt,...)
{
  va_list args;
  va_start (args, fmt);
  belle_sip_logv(BELLE_SIP_DEBUG, fmt, args);
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

static inline void belle_sip_log(belle_sip_log_level lev, const char *fmt,...){
        va_list args;
        va_start (args, fmt);
        belle_sip_logv(lev, fmt, args);
        va_end (args);
}

static inline void belle_sip_message(const char *fmt,...)
{
        va_list args;
        va_start (args, fmt);
        belle_sip_logv(BELLE_SIP_MESSAGE, fmt, args);
        va_end (args);
}

static inline void belle_sip_warning(const char *fmt,...)
{
        va_list args;
        va_start (args, fmt);
        belle_sip_logv(BELLE_SIP_WARNING, fmt, args);
        va_end (args);
}

#endif

static inline void belle_sip_error(const char *fmt,...)
{
        va_list args;
        va_start (args, fmt);
        belle_sip_logv(BELLE_SIP_ERROR, fmt, args);
        va_end (args);
}

static inline void belle_sip_fatal(const char *fmt,...)
{
        va_list args;
        va_start (args, fmt);
        belle_sip_logv(BELLE_SIP_FATAL, fmt, args);
        va_end (args);
}





#undef MIN
#define MIN(a,b)	((a)>(b) ? (b) : (a))
#undef MAX
#define MAX(a,b)	((a)>(b) ? (a) : (b))


char * belle_sip_concat (const char *str, ...);

uint64_t belle_sip_time_ms(void);

#ifdef __cplusplus
}
#endif

/*include private headers */
#include "belle_sip_resolver.h"

#endif
