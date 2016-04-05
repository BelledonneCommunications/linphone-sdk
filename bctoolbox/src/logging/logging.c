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

static void bctoolbox_log_domain_destroy(BctoolboxLogDomain *obj){
	if (obj->domain) bctoolbox_free(obj->domain);
	bctoolbox_free(obj);
}

typedef struct _BctoolboxLogger{
	BctoolboxLogFunc logv_out;
	unsigned int log_mask; /*the default log mask, if no per-domain settings are found*/
	FILE *log_file;
	unsigned long log_thread_id;
	bctoolbox_list_t *log_stored_messages_list;
	bctoolbox_list_t *log_domains;
	bctoolbox_mutex_t log_stored_messages_mutex;
	bctoolbox_mutex_t domains_mutex;
}BctoolboxLogger;


static BctoolboxLogger __bctoolbox_logger = { &bctoolbox_logv_out, BCTOOLBOX_WARNING|BCTOOLBOX_ERROR|BCTOOLBOX_FATAL, 0};

void bctoolbox_init_logger(void){
	bctoolbox_mutex_init(&__bctoolbox_logger.domains_mutex, NULL);
}

void bctoolbox_uninit_logger(void){
	bctoolbox_mutex_destroy(&__bctoolbox_logger.domains_mutex);
	__bctoolbox_logger.log_domains = bctoolbox_list_free_with_data(__bctoolbox_logger.log_domains, (void (*)(void*))bctoolbox_log_domain_destroy);
}

/**
*@param file a FILE pointer where to output the bctoolbox logs.
*
**/
void bctoolbox_set_log_file(FILE *file)
{
	__bctoolbox_logger.log_file=file;
}

/**
*@param func: your logging function, compatible with the BctoolboxLogFunc prototype.
*
**/
void bctoolbox_set_log_handler(BctoolboxLogFunc func){
	__bctoolbox_logger.logv_out=func;
}

BctoolboxLogFunc bctoolbox_get_log_handler(void){
	return __bctoolbox_logger.logv_out;
}

static BctoolboxLogDomain * get_log_domain(const char *domain){
	bctoolbox_list_t *it;
	
	if (domain == NULL) return NULL;
	for (it = __bctoolbox_logger.log_domains; it != NULL; it = bctoolbox_list_next(it)) {
		BctoolboxLogDomain *ld = (BctoolboxLogDomain*)bctoolbox_list_get_data(it);
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
	bctoolbox_mutex_lock(&__bctoolbox_logger.domains_mutex);
	ret = get_log_domain(domain);
	if (!ret){
		ret = bctoolbox_new0(BctoolboxLogDomain,1);
		ret->domain = bctoolbox_strdup(domain);
		ret->logmask = __bctoolbox_logger.log_mask;
		__bctoolbox_logger.log_domains = bctoolbox_list_prepend(__bctoolbox_logger.log_domains, ret);
	}
	bctoolbox_mutex_unlock(&__bctoolbox_logger.domains_mutex);
	return ret;
}

/**
* @ param levelmask a mask of BCTOOLBOX_DEBUG, BCTOOLBOX_MESSAGE, BCTOOLBOX_WARNING, BCTOOLBOX_ERROR
* BCTOOLBOX_FATAL .
**/
void bctoolbox_set_log_level_mask(const char *domain, int levelmask){
	if (domain == NULL) __bctoolbox_logger.log_mask=levelmask;
	else get_log_domain_rw(domain)->logmask = levelmask;
}


void bctoolbox_set_log_level(const char *domain, BctoolboxLogLevel level){
	int levelmask = BCTOOLBOX_FATAL;
	if (level<=BCTOOLBOX_ERROR){
		levelmask |= BCTOOLBOX_ERROR;
	}
	if (level<=BCTOOLBOX_WARNING){
		levelmask |= BCTOOLBOX_WARNING;
	}
	if (level<=BCTOOLBOX_MESSAGE){
		levelmask |= BCTOOLBOX_MESSAGE;
	}
	if (level<=BCTOOLBOX_TRACE){
		levelmask |= BCTOOLBOX_TRACE;
	}
	if (level<=BCTOOLBOX_DEBUG){
		levelmask |= BCTOOLBOX_DEBUG;
	}
	bctoolbox_set_log_level_mask(domain, levelmask);
}

unsigned int bctoolbox_get_log_level_mask(const char *domain) {
	BctoolboxLogDomain *ld;
	if (domain == NULL || (ld = get_log_domain(domain)) == NULL) return __bctoolbox_logger.log_mask;
	else return ld->logmask;
}

void bctoolbox_set_log_thread_id(unsigned long thread_id) {
	if (thread_id == 0) {
		bctoolbox_logv_flush();
		bctoolbox_mutex_destroy(&__bctoolbox_logger.log_stored_messages_mutex);
	} else {
		bctoolbox_mutex_init(&__bctoolbox_logger.log_stored_messages_mutex, NULL);
	}
	__bctoolbox_logger.log_thread_id = thread_id;
}

char * bctoolbox_strdup_vprintf(const char *fmt, va_list ap)
{
/* Guess we need no more than 100 bytes. */
	int n, size = 200;
	char *p,*np;
#ifndef _WIN32
	va_list cap;/*copy of our argument list: a va_list cannot be re-used (SIGSEGV on linux 64 bits)*/
#endif
	if ((p = (char *) bctoolbox_malloc (size)) == NULL)
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
		if ((np = (char *) bctoolbox_realloc (p, size)) == NULL)
		{
			free(p);
			return NULL;
		} else {
			p = np;
		}
	}
}

char *bctoolbox_strdup_printf(const char *fmt,...){
	char *ret;
	va_list args;
	va_start (args, fmt);
	ret=bctoolbox_strdup_vprintf(fmt, args);
	va_end (args);
	return ret;
}

char * bctoolbox_strcat_vprintf(char* dst, const char *fmt, va_list ap){
	char *ret;
	size_t dstlen, retlen;

	ret=bctoolbox_strdup_vprintf(fmt, ap);
	if (!dst) return ret;

	dstlen = strlen(dst);
	retlen = strlen(ret);

	if ((dst = bctoolbox_realloc(dst, dstlen+retlen+1)) != NULL){
		strncat(dst,ret,retlen);
		dst[dstlen+retlen] = '\0';
		bctoolbox_free(ret);
		return dst;
	} else {
		bctoolbox_free(ret);
		return NULL;
	}
}

char *bctoolbox_strcat_printf(char* dst, const char *fmt,...){
	char *ret;
	va_list args;
	va_start (args, fmt);
	ret=bctoolbox_strcat_vprintf(dst, fmt, args);
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
} bctoolbox_stored_log_t;

void _bctoolbox_logv_flush(int dummy, ...) {
	bctoolbox_list_t *elem;
	bctoolbox_list_t *msglist;
	va_list empty_va_list;
	va_start(empty_va_list, dummy);
	bctoolbox_mutex_lock(&__bctoolbox_logger.log_stored_messages_mutex);
	msglist = __bctoolbox_logger.log_stored_messages_list;
	__bctoolbox_logger.log_stored_messages_list = NULL;
	bctoolbox_mutex_unlock(&__bctoolbox_logger.log_stored_messages_mutex);
	for (elem = msglist; elem != NULL; elem = bctoolbox_list_next(elem)) {
		bctoolbox_stored_log_t *l = (bctoolbox_stored_log_t *)bctoolbox_list_get_data(elem);
#ifdef _WIN32
		__bctoolbox_logger.logv_out(l->domain, l->level, l->msg, empty_va_list);
#else
		va_list cap;
		va_copy(cap, empty_va_list);
		__bctoolbox_logger.logv_out(l->domain, l->level, l->msg, cap);
		va_end(cap);
#endif
		if (l->domain) bctoolbox_free(l->domain);
		bctoolbox_free(l->msg);
		bctoolbox_free(l);
	}
	bctoolbox_list_free(msglist);
	va_end(empty_va_list);
}

void bctoolbox_logv_flush(void) {
	_bctoolbox_logv_flush(0);
}

void bctoolbox_logv(const char *domain, BctoolboxLogLevel level, const char *fmt, va_list args) {
	if ((__bctoolbox_logger.logv_out != NULL) && bctoolbox_log_level_enabled(domain, level)) {
		if (__bctoolbox_logger.log_thread_id == 0) {
			__bctoolbox_logger.logv_out(domain, level, fmt, args);
		} else if (__bctoolbox_logger.log_thread_id == bctoolbox_thread_self()) {
			bctoolbox_logv_flush();
			__bctoolbox_logger.logv_out(domain, level, fmt, args);
		} else {
			bctoolbox_stored_log_t *l = bctoolbox_new(bctoolbox_stored_log_t, 1);
			l->domain = domain ? bctoolbox_strdup(domain) : NULL;
			l->level = level;
			l->msg = bctoolbox_strdup_vprintf(fmt, args);
			bctoolbox_mutex_lock(&__bctoolbox_logger.log_stored_messages_mutex);
			__bctoolbox_logger.log_stored_messages_list = bctoolbox_list_append(__bctoolbox_logger.log_stored_messages_list, l);
			bctoolbox_mutex_unlock(&__bctoolbox_logger.log_stored_messages_mutex);
		}
	}
#if !defined(_WIN32_WCE)
	if (level == BCTOOLBOX_FATAL) {
		bctoolbox_logv_flush();
		abort();
	}
#endif
}

/*This function does the default formatting and output to file*/
void bctoolbox_logv_out(const char *domain, BctoolboxLogLevel lev, const char *fmt, va_list args){
	const char *lname="undef";
	char *msg;
	struct timeval tp;
	struct tm *lt;
#ifndef _WIN32
	struct tm tmbuf;
#endif
	time_t tt;
	bctoolbox_gettimeofday(&tp,NULL);
	tt = (time_t)tp.tv_sec;

#ifdef _WIN32
	lt = localtime(&tt);
#else
	lt = localtime_r(&tt,&tmbuf);
#endif

	if (__bctoolbox_logger.log_file==NULL) __bctoolbox_logger.log_file=stderr;
	switch(lev){
		case BCTOOLBOX_DEBUG:
			lname = "debug";
		break;
		case BCTOOLBOX_MESSAGE:
			lname = "message";
		break;
		case BCTOOLBOX_WARNING:
			lname = "warning";
		break;
		case BCTOOLBOX_ERROR:
			lname = "error";
		break;
		case BCTOOLBOX_FATAL:
			lname = "fatal";
		break;
		default:
			lname = "badlevel";
	}
	
	msg=bctoolbox_strdup_vprintf(fmt,args);
#if defined(_MSC_VER) && !defined(_WIN32_WCE)
#ifndef _UNICODE
	OutputDebugStringA(msg);
	OutputDebugStringA("\r\n");
#else
	{
		size_t len=strlen(msg);
		wchar_t *tmp=(wchar_t*)bctoolbox_malloc0((len+1)*sizeof(wchar_t));
		mbstowcs(tmp,msg,len);
		OutputDebugStringW(tmp);
		OutputDebugStringW(L"\r\n");
		bctoolbox_free(tmp);
	}
#endif
#endif
	fprintf(__bctoolbox_logger.log_file,"%i-%.2i-%.2i %.2i:%.2i:%.2i:%.3i %s-%s-%s" ENDLINE
			,1900+lt->tm_year,1+lt->tm_mon,lt->tm_mday,lt->tm_hour,lt->tm_min,lt->tm_sec
		,(int)(tp.tv_usec/1000), (domain?domain:"bctoolbox"), lname, msg);
	fflush(__bctoolbox_logger.log_file);
	bctoolbox_free(msg);
}


#ifdef __QNX__
#include <slog2.h>

static bool_t slog2_registered = FALSE;
static slog2_buffer_set_config_t slog2_buffer_config;
static slog2_buffer_t slog2_buffer_handle[2];

void bctoolbox_qnx_log_handler(const char *domain, BctoolboxLogLevel lev, const char *fmt, va_list args) {
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
		case BCTOOLBOX_DEBUG:
			severity = SLOG2_DEBUG1;
		break;
		case BCTOOLBOX_MESSAGE:
			severity = SLOG2_INFO;
		break;
		case BCTOOLBOX_WARNING:
			severity = SLOG2_WARNING;
		break;
		case BCTOOLBOX_ERROR:
			severity = SLOG2_ERROR;
		break;
		case BCTOOLBOX_FATAL:
			severity = SLOG2_CRITICAL;
		break;
		default:
			severity = SLOG2_CRITICAL;
	}

	msg = bctoolbox_strdup_vprintf(fmt,args);
	slog2c(slog2_buffer_handle[buffer_idx], 0, severity, msg);
}
#endif /* __QNX__ */
