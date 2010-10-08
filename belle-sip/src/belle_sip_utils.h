
#ifndef belle_utils_h
#define belle_utils_h
#include "belle_sip_list.h"

struct _belle_sip_list {
	struct _belle_sip_list *next;
	struct _belle_sip_list *prev;
	void *data;
};




#define belle_list_next(elem) ((elem)->next)


#ifdef __cplusplus
extern "C"{
#endif

belle_sip_list *belle_sip_list_new(void *data);
belle_sip_list * belle_sip_list_free(belle_sip_list *list);
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


typedef void (*belle_sip_log_function)(belle_sip_log_level lev, const char *fmt, va_list args);

void belle_sip_set_log_file(FILE *file);
void belle_sip_set_log_handler(belle_sip_log_function func);

extern belle_sip_log_function belle_sip_logv_out;

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

#ifdef __cplusplus
}
#endif

#endif
