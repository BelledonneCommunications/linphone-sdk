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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bctoolbox/logging.h"
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _MSC_VER
#ifndef access
#define access _access
#endif
#ifndef fileno
#define fileno _fileno
#endif
#endif


typedef struct{
	char *domain;
	unsigned int logmask;
}BctoolboxLogDomain;

static void bctbx_log_domain_destroy(BctoolboxLogDomain *obj){
	if (obj->domain) bctbx_free(obj->domain);
	bctbx_free(obj);
}

typedef struct _bctbx_logger_t {
	bctbx_list_t *logv_outs;
	unsigned int log_mask; /*the default log mask, if no per-domain settings are found*/
	unsigned long log_thread_id;
	bctbx_list_t *log_stored_messages_list;
	bctbx_list_t *log_domains;
	bctbx_mutex_t log_stored_messages_mutex;
	bctbx_mutex_t domains_mutex;
	bctbx_mutex_t log_mutex;
} bctbx_logger_t;

struct _bctbx_log_handler_t {
	BctbxLogHandlerFunc func;
	BctbxLogHandlerDestroyFunc destroy;
	char *domain; /*domain this log handler is limited to. NULL for all*/
	void* user_info;
};

typedef struct _bctbx_file_log_handler_t {
	char* path;
	char* name;
	uint64_t max_size;
	uint64_t size;
	FILE* file;
} bctbx_file_log_handler_t;

static bctbx_logger_t __bctbx_logger = { NULL, BCTBX_LOG_WARNING|BCTBX_LOG_ERROR|BCTBX_LOG_FATAL, 0};

static unsigned int bctbx_init_logger_refcount = 0;
void bctbx_logv_out_cb(void* user_info, const char *domain, BctbxLogLevel lev, const char *fmt, va_list args);

static void wrapper(void* info,const char *domain, BctbxLogLevel lev, const char *fmt, va_list args) {
	BctbxLogFunc func = (BctbxLogFunc)info;
	if (func) func(domain, lev, fmt,  args);
}
static bctbx_log_handler_t static_handler;

void bctbx_init_logger(bool_t create){
	if (bctbx_init_logger_refcount++ > 0) return; /*already initialized*/
	static_handler.func=wrapper;
	static_handler.destroy=(BctbxLogHandlerDestroyFunc)bctbx_logv_out_destroy;
	static_handler.user_info=(void*)bctbx_logv_out;
	bctbx_mutex_init(&__bctbx_logger.domains_mutex, NULL);
	bctbx_mutex_init(&__bctbx_logger.log_mutex, NULL);
	bctbx_add_log_handler(&static_handler);
}

void bctbx_log_handlers_free(void) {
	bctbx_list_t *loggers = bctbx_list_first_elem(__bctbx_logger.logv_outs);
	while (loggers) {
		bctbx_log_handler_t* handler = (bctbx_log_handler_t*)loggers->data;
		handler->destroy(handler);
		loggers = loggers->next;
	}
}

void bctbx_uninit_logger(void){
	if (--bctbx_init_logger_refcount <= 0) {
		bctbx_logv_flush();
		bctbx_mutex_destroy(&__bctbx_logger.domains_mutex);
		bctbx_mutex_destroy(&__bctbx_logger.log_mutex);
		bctbx_log_handlers_free();
		__bctbx_logger.logv_outs = bctbx_list_free(__bctbx_logger.logv_outs);
		__bctbx_logger.log_domains = bctbx_list_free_with_data(__bctbx_logger.log_domains, (void (*)(void*))bctbx_log_domain_destroy);
	}
}

bctbx_log_handler_t* bctbx_create_log_handler(BctbxLogHandlerFunc func, BctbxLogHandlerDestroyFunc destroy, void* user_info) {
	bctbx_log_handler_t* handler = (bctbx_log_handler_t*)bctbx_malloc0(sizeof(bctbx_log_handler_t));
	handler->func = func;
	handler->destroy = destroy;
	handler->user_info = user_info;
	return handler;
}

void bctbx_log_handler_set_user_data(bctbx_log_handler_t* log_handler, void* user_data) {
	log_handler->user_info = user_data;
}
void *bctbx_log_handler_get_user_data(const bctbx_log_handler_t* log_handler) {
	return log_handler->user_info;
}

void bctbx_log_handler_set_domain(bctbx_log_handler_t * log_handler,const char *domain) {
	if (log_handler->domain) bctbx_free(log_handler->domain);
	if (domain) {
		log_handler->domain = bctbx_strdup(domain);
	} else {
		log_handler->domain = NULL ;
	}
	
}
bctbx_log_handler_t* bctbx_create_file_log_handler(uint64_t max_size, const char* path, const char* name, FILE* f) {
	bctbx_log_handler_t* handler = (bctbx_log_handler_t*)bctbx_malloc0(sizeof(bctbx_log_handler_t));
	bctbx_file_log_handler_t* filehandler = (bctbx_file_log_handler_t*)bctbx_malloc0(sizeof(bctbx_file_log_handler_t));
	char *full_name = bctbx_strdup_printf("%s/%s",
									path,
									name);
	struct stat buf;
	memset(&buf, 0, sizeof(buf));
	handler->func=bctbx_logv_file;
	handler->destroy=bctbx_logv_file_destroy;
	filehandler->max_size = max_size;
	// init with actual file size
	if(!f && stat(full_name, &buf) != 0) {
		fprintf(stderr,"Error while creating file log handler. \n");
		return NULL;
	}
	bctbx_free(full_name);
	filehandler->size = buf.st_size;
	filehandler->path = bctbx_strdup(path);
	filehandler->name = bctbx_strdup(name);
	filehandler->file = f;
	handler->user_info=(void*) filehandler;
	return handler;
}

/**
*@param func: your logging function, compatible with the BctoolboxLogFunc prototype.
*
**/
void bctbx_add_log_handler(bctbx_log_handler_t* handler){
	if (handler && !bctbx_list_find(__bctbx_logger.logv_outs, handler))
		__bctbx_logger.logv_outs = bctbx_list_append(__bctbx_logger.logv_outs, (void*)handler);
	/*else, already in*/
}

void bctbx_remove_log_handler(bctbx_log_handler_t* handler){
	__bctbx_logger.logv_outs = bctbx_list_remove(__bctbx_logger.logv_outs,  handler);
	handler->destroy(handler);
	return;
}


void bctbx_set_log_handler(BctbxLogFunc func){
	bctbx_set_log_handler_for_domain(func,NULL);
}

void bctbx_set_log_handler_for_domain(BctbxLogFunc func, const char* domain){
	if (bctbx_init_logger_refcount == 0) {
		bctbx_init_logger(TRUE);
	}
	static_handler.user_info=(void*)func;
	bctbx_log_handler_set_domain(&static_handler, domain);
}

void bctbx_set_log_file(FILE* f){
	static bctbx_file_log_handler_t filehandler;
	static bctbx_log_handler_t handler;
	handler.func=bctbx_logv_file;
	handler.destroy=(BctbxLogHandlerDestroyFunc)bctbx_logv_file_destroy;
	filehandler.max_size = -1;
	filehandler.file = f;
	handler.user_info=(void*) &filehandler;
	bctbx_add_log_handler(&handler);
}

bctbx_list_t* bctbx_get_log_handlers(void){
	return __bctbx_logger.logv_outs;
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
	if (bctbx_init_logger_refcount == 0) {
		bctbx_init_logger(TRUE);
	}
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
	if (bctbx_init_logger_refcount == 0) {
		bctbx_init_logger(TRUE);
	}
	bctbx_mutex_lock(&__bctbx_logger.log_stored_messages_mutex);
	msglist = __bctbx_logger.log_stored_messages_list;
	__bctbx_logger.log_stored_messages_list = NULL;
	bctbx_mutex_unlock(&__bctbx_logger.log_stored_messages_mutex);
	for (elem = msglist; elem != NULL; elem = bctbx_list_next(elem)) {
		bctbx_stored_log_t *l = (bctbx_stored_log_t *)bctbx_list_get_data(elem);
		bctbx_list_t *loggers = bctbx_list_first_elem(__bctbx_logger.logv_outs);
#ifdef _WIN32
		while (loggers) {
			bctbx_log_handler_t* handler = (bctbx_log_handler_t*)loggers->data;
			if(handler) {
				va_list cap;
				va_copy(cap, empty_va_list);
				handler->func(handler->user_info, l->domain, l->level, l->msg, cap);
				va_end(cap);
			}
			loggers = loggers->next;
		}
#else
		
		while (loggers) {
			bctbx_log_handler_t* handler = (bctbx_log_handler_t*)loggers->data;
			if(handler) {
				va_list cap;
				va_copy(cap, empty_va_list);
				handler->func(handler->user_info, l->domain, l->level, l->msg, cap);
				va_end(cap);
			}
			loggers = loggers->next;
		}
		
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
	if (bctbx_init_logger_refcount == 0) {
		bctbx_init_logger(TRUE);
	}
	if ((__bctbx_logger.logv_outs != NULL) && bctbx_log_level_enabled(domain, level)) {
		if (__bctbx_logger.log_thread_id == 0) {
			bctbx_list_t *loggers = bctbx_list_first_elem(__bctbx_logger.logv_outs);
			while (loggers) {
				bctbx_log_handler_t* handler = (bctbx_log_handler_t*)loggers->data;
				if(handler && (!handler->domain || !domain || strcmp(handler->domain,domain)==0)) {
					va_list tmp;
					va_copy(tmp, args);
					handler->func(handler->user_info, domain, level, fmt, tmp);
					va_end(tmp);
				}
				loggers = loggers->next;
			}
		} else if (__bctbx_logger.log_thread_id == bctbx_thread_self()) {
			bctbx_list_t *loggers;
			bctbx_logv_flush();
			loggers = bctbx_list_first_elem(__bctbx_logger.logv_outs);
			while (loggers) {
				bctbx_log_handler_t* handler = (bctbx_log_handler_t*)loggers->data;
				if(handler && (!handler->domain || !domain || strcmp(handler->domain,domain)==0)) {
					va_list tmp;
					va_copy(tmp, args);
					handler->func(handler->user_info, domain, level, fmt, tmp);
					va_end(tmp);
				}
				loggers = loggers->next;
			}
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
void bctbx_logv_out( const char *domain, BctbxLogLevel lev, const char *fmt, va_list args){
	bctbx_logv_out_cb(NULL, domain, lev, fmt, args);
}
/*This function does the default formatting and output to file*/
void bctbx_logv_out_cb(void* user_info, const char *domain, BctbxLogLevel lev, const char *fmt, va_list args){
	const char *lname="undef";
	char *msg;
	struct timeval tp;
	struct tm *lt;
#ifndef _WIN32
	struct tm tmbuf;
#endif
	time_t tt;
	FILE *std = stdout;
	bctbx_gettimeofday(&tp,NULL);
	tt = (time_t)tp.tv_sec;

#ifdef _WIN32
	lt = localtime(&tt);
#else
	lt = localtime_r(&tt,&tmbuf);
#endif

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
			std = stderr;
		break;
		case BCTBX_LOG_FATAL:
			lname = "fatal";
			std = stderr;
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
	fprintf(std,"%i-%.2i-%.2i %.2i:%.2i:%.2i:%.3i %s-%s-%s" ENDLINE
			,1900+lt->tm_year,1+lt->tm_mon,lt->tm_mday,lt->tm_hour,lt->tm_min,lt->tm_sec
		,(int)(tp.tv_usec/1000), (domain?domain:"bctoolbox"), lname, msg);
	fflush(std);
	bctbx_free(msg);
}

void bctbx_logv_out_destroy(bctbx_log_handler_t* handler) {
	handler->user_info=NULL;
}

static int _try_open_log_collection_file(bctbx_file_log_handler_t *filehandler) {
	struct stat statbuf;
	char *log_filename;

	log_filename = bctbx_strdup_printf("%s/%s",
		filehandler->path,
		filehandler->name);
	filehandler->file = fopen(log_filename, "a");
	bctbx_free(log_filename);
	if (filehandler->file == NULL) return -1;

	fstat(fileno(filehandler->file), &statbuf);
	if ((uint64_t)statbuf.st_size > filehandler->max_size) {
		fclose(filehandler->file);
		return -1;
	}

	filehandler->size = statbuf.st_size;
	return 0;
}

static void _rotate_log_collection_files(bctbx_file_log_handler_t *filehandler) {
	char *log_filename;
	char *log_filename2;
	char *file_no_extension = bctbx_strdup(filehandler->name);
	char *extension = strrchr(file_no_extension, '.');
	char *extension2 = bctbx_strdup(extension);
	int n = 1;
	file_no_extension[extension - file_no_extension] = '\0';

	log_filename = bctbx_strdup_printf("%s/%s_1%s",
		filehandler->path,
		file_no_extension,
		extension2);
	while(access(log_filename, F_OK) != -1) {
		// file exists
		n++;
		log_filename = bctbx_strdup_printf("%s/%s_%d%s",
		filehandler->path,
		file_no_extension,
		n,
		extension2);
	}

	while(n > 1) {
		log_filename = bctbx_strdup_printf("%s/%s_%d%s",
		filehandler->path,
		file_no_extension,
		n-1,
		extension2);
		log_filename2 = bctbx_strdup_printf("%s/%s_%d%s",
		filehandler->path,
		file_no_extension,
		n,
		extension2);

		n--;
		rename(log_filename, log_filename2);
	}

	log_filename = bctbx_strdup_printf("%s/%s",
	filehandler->path,
	filehandler->name);
	log_filename2 = bctbx_strdup_printf("%s/%s_1%s",
	filehandler->path,
	file_no_extension,
	extension2);
	rename(log_filename, log_filename2);
	bctbx_free(log_filename);
	bctbx_free(log_filename2);
	bctbx_free(extension2);
	bctbx_free(file_no_extension);
}

static void _open_log_collection_file(bctbx_file_log_handler_t *filehandler) {
	if (_try_open_log_collection_file(filehandler) < 0) {
		_rotate_log_collection_files(filehandler);
		_try_open_log_collection_file(filehandler);
	}
}

static void _close_log_collection_file(bctbx_file_log_handler_t *filehandler) {
	if (filehandler->file) {
		fclose(filehandler->file);
		filehandler->file = NULL;
		filehandler->size = 0;
	}
}

void bctbx_logv_file(void* user_info, const char *domain, BctbxLogLevel lev, const char *fmt, va_list args){
	const char *lname="undef";
	char *msg;
	struct timeval tp;
	struct tm *lt;
#ifndef _WIN32
	struct tm tmbuf;
#endif
	time_t tt;
	int ret = -1;
	bctbx_file_log_handler_t *filehandler = (bctbx_file_log_handler_t *) user_info;
	FILE *f;
	if (bctbx_init_logger_refcount == 0) {
		bctbx_init_logger(TRUE);
	}
	bctbx_mutex_lock(&__bctbx_logger.log_mutex);
	if (filehandler != NULL){
		f = filehandler->file;
	}else f = stdout;
	bctbx_gettimeofday(&tp,NULL);
	tt = (time_t)tp.tv_sec;

#ifdef _WIN32
	lt = localtime(&tt);
#else
	lt = localtime_r(&tt,&tmbuf);
#endif

	if(!f) {
		return;
	}
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
	ret = fprintf(f,"%i-%.2i-%.2i %.2i:%.2i:%.2i:%.3i %s-%s-%s" ENDLINE
			,1900+lt->tm_year,1+lt->tm_mon,lt->tm_mday,lt->tm_hour,lt->tm_min,lt->tm_sec
		,(int)(tp.tv_usec/1000), (domain?domain:"bctoolbox"), lname, msg);
	fflush(f);
	if (filehandler && filehandler->max_size > 0 && ret > 0) {
		filehandler->size += ret;
		if (filehandler->size > filehandler->max_size) {
			_close_log_collection_file(filehandler);
			_open_log_collection_file(filehandler);
		}
	}
	bctbx_mutex_unlock(&__bctbx_logger.log_mutex);

	bctbx_free(msg);
}

void bctbx_logv_file_destroy(bctbx_log_handler_t* handler) {
	bctbx_file_log_handler_t *filehandler = (bctbx_file_log_handler_t *) handler->user_info;
	fclose(filehandler->file);
	bctbx_free(filehandler->path);
	bctbx_free(filehandler->name);
	bctbx_logv_out_destroy(handler);
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
