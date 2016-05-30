/*
bctoolbox
Copyright (C) 2016  Belledonne Communications SARL


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

#include "bctoolbox/logging.h"
#include "bctoolbox/list.h"
#include <time.h>


typedef struct{
	char *domain;
	unsigned int logmask;
}BctoolboxLogDomain;

static void bctbx_log_domain_destroy(BctoolboxLogDomain *obj){
	if (obj->domain) bctbx_free(obj->domain);
	bctbx_free(obj);
}

typedef struct _BctoolboxLogger{
	BctoolboxLogFunc logv_out;
	unsigned int log_mask; /*the default log mask, if no per-domain settings are found*/
	FILE *log_file;
	unsigned long log_thread_id;
	bctbx_list_t *log_stored_messages_list;
	bctbx_list_t *log_domains;
	bctbx_mutex_t log_stored_messages_mutex;
	bctbx_mutex_t domains_mutex;
}BctoolboxLogger;


static BctoolboxLogger __bctbx_logger = { &bctbx_logv_out, BCTBX_LOG_WARNING|BCTBX_LOG_ERROR|BCTBX_LOG_FATAL, 0};

void bctbx_init_logger(void){
	bctbx_mutex_init(&__bctbx_logger.domains_mutex, NULL);
}

void bctbx_uninit_logger(void){
	bctbx_mutex_destroy(&__bctbx_logger.domains_mutex);
	__bctbx_logger.log_domains = bctbx_list_free_with_data(__bctbx_logger.log_domains, (void (*)(void*))bctbx_log_domain_destroy);
}

/**
*@param file a FILE pointer where to output the bctoolbox logs.
*
**/
void bctbx_set_log_file(FILE *file)
{
	__bctbx_logger.log_file=file;
}

/**
*@param func: your logging function, compatible with the BctoolboxLogFunc prototype.
*
**/
void bctbx_set_log_handler(BctoolboxLogFunc func){
	__bctbx_logger.logv_out=func;
}

BctoolboxLogFunc bctbx_get_log_handler(void){
	return __bctbx_logger.logv_out;
}

static BctoolboxLogDomain * get_log_domain(const char *domain){
	bctbx_list_t *it;
	
	if (domain == NULL) return NULL;
	for (it = __bctbx_logger.log_domains; it != NULL; it = bctbx_list_next(it)) {
		BctoolboxLogDomain *ld = (BctoolboxLogDomain*)bctbx_list_get_data(it);
		if (ld->domain && strcmp(ld->domain, domain) == 0 ){
			return ld;
		}
	}
	return NULL;
}

static BctoolboxLogDomain *get_log_domain_rw(const char *domain){
	BctoolboxLogDomain *ret;
	
	if (domain == NULL) return NULL;
	ret = get_log_domain(domain);
	if (ret) return ret;
	/*it does not exist, hence create it by taking the mutex*/
	bctbx_mutex_lock(&__bctbx_logger.domains_mutex);
	ret = get_log_domain(domain);
	if (!ret){
		ret = bctbx_new0(BctoolboxLogDomain,1);
		ret->domain = bctbx_strdup(domain);
		ret->logmask = __bctbx_logger.log_mask;
		__bctbx_logger.log_domains = bctbx_list_prepend(__bctbx_logger.log_domains, ret);
	}
	bctbx_mutex_unlock(&__bctbx_logger.domains_mutex);
	return ret;
}

/**
* @ param levelmask a mask of BCTBX_DEBUG, BCTBX_MESSAGE, BCTBX_WARNING, BCTBX_ERROR
* BCTBX_FATAL .
**/
void bctbx_set_log_level_mask(const char *domain, int levelmask){
	if (domain == NULL) __bctbx_logger.log_mask=levelmask;
	else get_log_domain_rw(domain)->logmask = levelmask;
}


void bctbx_set_log_level(const char *domain, BctbxLogLevel level){
	int levelmask = BCTBX_LOG_FATAL;
	if (level<=BCTBX_LOG_ERROR){
		levelmask |= BCTBX_LOG_ERROR;
	}
	if (level<=BCTBX_LOG_WARNING){
		levelmask |= BCTBX_LOG_WARNING;
	}
	if (level<=BCTBX_LOG_MESSAGE){
		levelmask |= BCTBX_LOG_MESSAGE;
	}
	if (level<=BCTBX_LOG_TRACE)	{
		levelmask |= BCTBX_LOG_TRACE;
	}
	if (level<=BCTBX_LOG_DEBUG){
		levelmask |= BCTBX_LOG_DEBUG;
	}
	bctbx_set_log_level_mask(domain, levelmask);
}

unsigned int bctbx_get_log_level_mask(const char *domain) {
	BctoolboxLogDomain *ld;
	if (domain == NULL || (ld = get_log_domain(domain)) == NULL) return __bctbx_logger.log_mask;
	else return ld->logmask;
}

void bctbx_set_log_thread_id(unsigned long thread_id) {
	if (thread_id == 0) {
		bctbx_logv_flush();
		bctbx_mutex_destroy(&__bctbx_logger.log_stored_messages_mutex);
	} else {
		bctbx_mutex_init(&__bctbx_logger.log_stored_messages_mutex, NULL);
	}
	__bctbx_logger.log_thread_id = thread_id;
}

char * bctbx_strdup_vprintf(const char *fmt, va_list ap)
{
/* Guess we need no more than 100 bytes. */
	int n, size = 200;
	char *p,*np;
#ifndef _WIN32
	va_list cap;/*copy of our argument list: a va_list cannot be re-used (SIGSEGV on linux 64 bits)*/
#endif
	if ((p = (char *) bctbx_malloc (size)) == NULL)
		return NULL;
	while (1){
/* Try to print in the allocated space. */
#ifndef _WIN32
		va_copy(cap,ap);
		n = vsnprintf (p, size, fmt, cap);
		va_end(cap);
#else
		/*this works on 32 bits, luckily*/
		n = vsnprintf (p, size, fmt, ap);
#endif
		/* If that worked, return the string. */
		if (n > -1 && n < size)
			return p;
//printf("Reallocing space.\n");
		/* Else try again with more space. */
		if (n > -1)	/* glibc 2.1 */
			size = n + 1;	/* precisely what is needed */
		else		/* glibc 2.0 */
			size *= 2;	/* twice the old size */
		if ((np = (char *) bctbx_realloc (p, size)) == NULL)
		{
			free(p);
			return NULL;
		} else {
			p = np;
		}
	}
}

char *bctbx_strdup_printf(const char *fmt,...){
	char *ret;
	va_list args;
	va_start (args, fmt);
	ret=bctbx_strdup_vprintf(fmt, args);
	va_end (args);
	return ret;
}

char * bctbx_strcat_vprintf(char* dst, const char *fmt, va_list ap){
	char *ret;
	size_t dstlen, retlen;

	ret=bctbx_strdup_vprintf(fmt, ap);
	if (!dst) return ret;

	dstlen = strlen(dst);
	retlen = strlen(ret);

	if ((dst = bctbx_realloc(dst, dstlen+retlen+1)) != NULL){
		strncat(dst,ret,retlen);
		dst[dstlen+retlen] = '\0';
		bctbx_free(ret);
		return dst;
	} else {
		bctbx_free(ret);
		return NULL;
	}
}

char *bctbx_strcat_printf(char* dst, const char *fmt,...){
	char *ret;
	va_list args;
	va_start (args, fmt);
	ret=bctbx_strcat_vprintf(dst, fmt, args);
	va_end (args);
	return ret;
}

#if	defined(_WIN32) || defined(_WIN32_WCE)
#define ENDLINE "\r\n"
#else
#define ENDLINE "\n"
#endif

typedef struct {
	int level;
	char *msg;
	char *domain;
} bctbx_stored_log_t;

void _bctbx_logv_flush(int dummy, ...) {
	bctbx_list_t *elem;
	bctbx_list_t *msglist;
	va_list empty_va_list;
	va_start(empty_va_list, dummy);
	bctbx_mutex_lock(&__bctbx_logger.log_stored_messages_mutex);
	msglist = __bctbx_logger.log_stored_messages_list;
	__bctbx_logger.log_stored_messages_list = NULL;
	bctbx_mutex_unlock(&__bctbx_logger.log_stored_messages_mutex);
	for (elem = msglist; elem != NULL; elem = bctbx_list_next(elem)) {
		bctbx_stored_log_t *l = (bctbx_stored_log_t *)bctbx_list_get_data(elem);
#ifdef _WIN32
		__bctbx_logger.logv_out(l->domain, l->level, l->msg, empty_va_list);
#else
		va_list cap;
		va_copy(cap, empty_va_list);
		__bctbx_logger.logv_out(l->domain, l->level, l->msg, cap);
		va_end(cap);
#endif
		if (l->domain) bctbx_free(l->domain);
		bctbx_free(l->msg);
		bctbx_free(l);
	}
	bctbx_list_free(msglist);
	va_end(empty_va_list);
}

void bctbx_logv_flush(void) {
	_bctbx_logv_flush(0);
}

void bctbx_logv(const char *domain, BctbxLogLevel level, const char *fmt, va_list args) {
	if ((__bctbx_logger.logv_out != NULL) && bctbx_log_level_enabled(domain, level)) {
		if (__bctbx_logger.log_thread_id == 0) {
			__bctbx_logger.logv_out(domain, level, fmt, args);
		} else if (__bctbx_logger.log_thread_id == bctbx_thread_self()) {
			bctbx_logv_flush();
			__bctbx_logger.logv_out(domain, level, fmt, args);
		} else {
			bctbx_stored_log_t *l = bctbx_new(bctbx_stored_log_t, 1);
			l->domain = domain ? bctbx_strdup(domain) : NULL;
			l->level = level;
			l->msg = bctbx_strdup_vprintf(fmt, args);
			bctbx_mutex_lock(&__bctbx_logger.log_stored_messages_mutex);
			__bctbx_logger.log_stored_messages_list = bctbx_list_append(__bctbx_logger.log_stored_messages_list, l);
			bctbx_mutex_unlock(&__bctbx_logger.log_stored_messages_mutex);
		}
	}
#if !defined(_WIN32_WCE)
	if (level == BCTBX_LOG_FATAL) {
		bctbx_logv_flush();
		abort();
	}
#endif
}

/*This function does the default formatting and output to file*/
void bctbx_logv_out(const char *domain, BctbxLogLevel lev, const char *fmt, va_list args){
	const char *lname="undef";
	char *msg;
	struct timeval tp;
	struct tm *lt;
#ifndef _WIN32
	struct tm tmbuf;
#endif
	time_t tt;
	bctbx_gettimeofday(&tp,NULL);
	tt = (time_t)tp.tv_sec;

#ifdef _WIN32
	lt = localtime(&tt);
#else
	lt = localtime_r(&tt,&tmbuf);
#endif

	if (__bctbx_logger.log_file==NULL) __bctbx_logger.log_file=stderr;
	switch(lev){
		case BCTBX_LOG_DEBUG:
			lname = "debug";
		break;
		case BCTBX_LOG_MESSAGE:
			lname = "message";
		break;
		case BCTBX_LOG_WARNING:
			lname = "warning";
		break;
		case BCTBX_LOG_ERROR:
			lname = "error";
		break;
		case BCTBX_LOG_FATAL:
			lname = "fatal";
		break;
		default:
			lname = "badlevel";
	}
	
	msg=bctbx_strdup_vprintf(fmt,args);
#if defined(_MSC_VER) && !defined(_WIN32_WCE)
#ifndef _UNICODE
	OutputDebugStringA(msg);
	OutputDebugStringA("\r\n");
#else
	{
		size_t len=strlen(msg);
		wchar_t *tmp=(wchar_t*)bctbx_malloc0((len+1)*sizeof(wchar_t));
		mbstowcs(tmp,msg,len);
		OutputDebugStringW(tmp);
		OutputDebugStringW(L"\r\n");
		bctbx_free(tmp);
	}
#endif
#endif
	fprintf(__bctbx_logger.log_file,"%i-%.2i-%.2i %.2i:%.2i:%.2i:%.3i %s-%s-%s" ENDLINE
			,1900+lt->tm_year,1+lt->tm_mon,lt->tm_mday,lt->tm_hour,lt->tm_min,lt->tm_sec
		,(int)(tp.tv_usec/1000), (domain?domain:"bctoolbox"), lname, msg);
	fflush(__bctbx_logger.log_file);
	bctbx_free(msg);
}


#ifdef __QNX__
#include <slog2.h>

static bool_t slog2_registered = FALSE;
static slog2_buffer_set_config_t slog2_buffer_config;
static slog2_buffer_t slog2_buffer_handle[2];

void bctbx_qnx_log_handler(const char *domain, BctbxLogLevel lev, const char *fmt, va_list args) {
	uint8_t severity = SLOG2_DEBUG1;
	uint8_t buffer_idx = 1;
	char* msg;

	if (slog2_registered != TRUE) {
		slog2_buffer_config.buffer_set_name = domain;
		slog2_buffer_config.num_buffers = 2;
		slog2_buffer_config.verbosity_level = SLOG2_DEBUG2;
		slog2_buffer_config.buffer_config[0].buffer_name = "hi_rate";
		slog2_buffer_config.buffer_config[0].num_pages = 6;
		slog2_buffer_config.buffer_config[1].buffer_name = "lo_rate";
		slog2_buffer_config.buffer_config[1].num_pages = 2;
		if (slog2_register(&slog2_buffer_config, slog2_buffer_handle, 0) == 0) {
			slog2_registered = TRUE;
		} else {
			fprintf(stderr, "Error registering slogger2 buffer!\n");
			return;
		}
	}

	switch(lev){
		case BCTBX_LOG_DEBUG:
			severity = SLOG2_DEBUG1;
		break;
		case BCTBX_LOG_MESSAGE:
			severity = SLOG2_INFO;
		break;
		case BCTBX_LOG_WARNING:
			severity = SLOG2_WARNING;
		break;
		case BCTBX_LOG_ERROR:
			severity = SLOG2_ERROR;
		break;
		case BCTBX_LOG_FATAL:
			severity = SLOG2_CRITICAL;
		break;
		default:
			severity = SLOG2_CRITICAL;
	}

	msg = bctbx_strdup_vprintf(fmt,args);
	slog2c(slog2_buffer_handle[buffer_idx], 0, severity, msg);
}
#endif /* __QNX__ */
