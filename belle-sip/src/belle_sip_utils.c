/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belle_sip_internal.h"

#include <time.h>
#include "clock_gettime.h" /*for apple*/

static FILE *__log_file=0;

/**
 *@param file a FILE pointer where to output the belle logs.
 *
**/
void belle_sip_set_log_file(FILE *file)
{
        __log_file=file;
}

static void __belle_sip_logv_out(belle_sip_log_level lev, const char *fmt, va_list args);

belle_sip_log_function_t belle_sip_logv_out=__belle_sip_logv_out;

/**
 *@param func: your logging function, compatible with the OrtpLogFunc prototype.
 *
**/
void belle_sip_set_log_handler(belle_sip_log_function_t func){
        belle_sip_logv_out=func;
}


unsigned int __belle_sip_log_mask=BELLE_SIP_WARNING|BELLE_SIP_ERROR|BELLE_SIP_FATAL;

/**
 * @ param levelmask a mask of BELLE_SIP_DEBUG, BELLE_SIP_MESSAGE, BELLE_SIP_WARNING, BELLE_SIP_ERROR
 * BELLE_SIP_FATAL .
**/
void belle_sip_set_log_level_mask(int levelmask){
        __belle_sip_log_mask=levelmask;
}

char * belle_sip_strdup_vprintf(const char *fmt, va_list ap)
{
        /* Guess we need no more than 100 bytes. */
        int n, size = 200;
        char *p,*np;
#ifndef WIN32
        va_list cap;/*copy of our argument list: a va_list cannot be re-used (SIGSEGV on linux 64 bits)*/
#endif
        if ((p = (char *) malloc (size)) == NULL)
                return NULL;
        while (1)
        {
                /* Try to print in the allocated space. */
#ifndef WIN32
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
                if (n > -1)     /* glibc 2.1 */
                        size = n + 1;   /* precisely what is needed */
                else            /* glibc 2.0 */
                        size *= 2;      /* twice the old size */
                if ((np = (char *) realloc (p, size)) == NULL)
                  {
                    free(p);
                    return NULL;
                  }
                else
                  {
                    p = np;
                  }
        }
}

char *belle_sip_strdup_printf(const char *fmt,...){
        char *ret;
        va_list args;
        va_start (args, fmt);
        ret=belle_sip_strdup_vprintf(fmt, args);
        va_end (args);
        return ret;
}

#if     defined(WIN32) || defined(_WIN32_WCE)
#define ENDLINE "\r\n"
#else
#define ENDLINE "\n"
#endif

#if     defined(WIN32) || defined(_WIN32_WCE)
void belle_sip_logv(int level, const char *fmt, va_list args)
{
        if (belle_sip_logv_out!=NULL && belle_sip_log_level_enabled(level))
                belle_sip_logv_out(level,fmt,args);
#if !defined(_WIN32_WCE)
        if ((level)==BELLE_SIP_FATAL) abort();
#endif
}
#endif

static void __belle_sip_logv_out(belle_sip_log_level lev, const char *fmt, va_list args){
        const char *lname="undef";
        char *msg;
        if (__log_file==NULL) __log_file=stderr;
        switch(lev){
                case BELLE_SIP_DEBUG:
                        lname="debug";
                        break;
                case BELLE_SIP_MESSAGE:
                        lname="message";
                        break;
                case BELLE_SIP_WARNING:
                        lname="warning";
                        break;
                case BELLE_SIP_ERROR:
                        lname="error";
                        break;
                case BELLE_SIP_FATAL:
                        lname="fatal";
                        break;
                default:
                        belle_sip_fatal("Bad level !");
        }
        msg=belle_sip_strdup_vprintf(fmt,args);
#if defined(_MSC_VER) && !defined(_WIN32_WCE)
        OutputDebugString(msg);
        OutputDebugString("\r\n");
#endif
        fprintf(__log_file,"belle-sip-%s-%s" ENDLINE,lname,msg);
        fflush(__log_file);
        free(msg);
}

belle_sip_list_t* belle_sip_list_new(void *data){
	belle_sip_list_t* new_elem=belle_sip_new0(belle_sip_list_t);
	new_elem->data=data;
	return new_elem;
}

belle_sip_list_t*  belle_sip_list_append_link(belle_sip_list_t* elem,belle_sip_list_t *new_elem){
	belle_sip_list_t* it=elem;
	if (elem==NULL) return new_elem;
	while (it->next!=NULL) it=belle_sip_list_next(it);
	it->next=new_elem;
	new_elem->prev=it;
	return elem;
}

belle_sip_list_t*  belle_sip_list_append(belle_sip_list_t* elem, void * data){
	belle_sip_list_t* new_elem=belle_sip_list_new(data);
	return belle_sip_list_append_link(elem,new_elem);
}

belle_sip_list_t*  belle_sip_list_prepend(belle_sip_list_t* elem, void *data){
	belle_sip_list_t* new_elem=belle_sip_list_new(data);
	if (elem!=NULL) {
		new_elem->next=elem;
		elem->prev=new_elem;
	}
	return new_elem;
}


belle_sip_list_t*  belle_sip_list_concat(belle_sip_list_t* first, belle_sip_list_t* second){
	belle_sip_list_t* it=first;
	if (it==NULL) return second;
	while(it->next!=NULL) it=belle_sip_list_next(it);
	it->next=second;
	second->prev=it;
	return first;
}

belle_sip_list_t*  belle_sip_list_free(belle_sip_list_t* list){
	belle_sip_list_t* elem = list;
	belle_sip_list_t* tmp;
	if (list==NULL) return NULL;
	while(elem->next!=NULL) {
		tmp = elem;
		elem = elem->next;
		belle_sip_free(tmp);
	}
	belle_sip_free(elem);
	return NULL;
}

belle_sip_list_t * belle_sip_list_free_with_data(belle_sip_list_t *list, void (*freefunc)(void*)){
	belle_sip_list_t* elem = list;
	belle_sip_list_t* tmp;
	if (list==NULL) return NULL;
	while(elem->next!=NULL) {
		tmp = elem;
		elem = elem->next;
		freefunc(tmp->data);
		belle_sip_free(tmp);
	}
	freefunc(elem->data);
	belle_sip_free(elem);
	return NULL;
}


belle_sip_list_t*  belle_sip_list_remove(belle_sip_list_t* first, void *data){
	belle_sip_list_t* it;
	it=belle_sip_list_find(first,data);
	if (it) return belle_sip_list_delete_link(first,it);
	else {
		belle_sip_warning("belle_sip_list_remove: no element with %p data was in the list", data);
		return first;
	}
}

int belle_sip_list_size(const belle_sip_list_t* first){
	int n=0;
	while(first!=NULL){
		++n;
		first=first->next;
	}
	return n;
}

void belle_sip_list_for_each(const belle_sip_list_t* list, void (*func)(void *)){
	for(;list!=NULL;list=list->next){
		func(list->data);
	}
}

void belle_sip_list_for_each2(const belle_sip_list_t* list, void (*func)(void *, void *), void *user_data){
	for(;list!=NULL;list=list->next){
		func(list->data,user_data);
	}
}

belle_sip_list_t* belle_sip_list_remove_link(belle_sip_list_t* list, belle_sip_list_t* elem){
	belle_sip_list_t* ret;
	if (elem==list){
		ret=elem->next;
		elem->prev=NULL;
		elem->next=NULL;
		if (ret!=NULL) ret->prev=NULL;
		return ret;
	}
	elem->prev->next=elem->next;
	if (elem->next!=NULL) elem->next->prev=elem->prev;
	elem->next=NULL;
	elem->prev=NULL;
	return list;
}

belle_sip_list_t * belle_sip_list_delete_link(belle_sip_list_t* list, belle_sip_list_t* elem){
	belle_sip_list_t *ret=belle_sip_list_remove_link(list,elem);
	belle_sip_free(elem);
	return ret;
}

belle_sip_list_t* belle_sip_list_find(belle_sip_list_t* list, void *data){
	for(;list!=NULL;list=list->next){
		if (list->data==data) return list;
	}
	return NULL;
}

belle_sip_list_t* belle_sip_list_find_custom(belle_sip_list_t* list, belle_sip_compare_func compare_func, const void *user_data){
	for(;list!=NULL;list=list->next){
		if (compare_func(list->data,user_data)==0) return list;
	}
	return NULL;
}

belle_sip_list_t *belle_sip_list_delete_custom(belle_sip_list_t *list, belle_sip_compare_func compare_func, const void *user_data){
	belle_sip_list_t *elem=belle_sip_list_find_custom(list,compare_func,user_data);
	if (elem!=NULL){
		list=belle_sip_list_delete_link(list,elem);
	}
	return list;
}

void * belle_sip_list_nth_data(const belle_sip_list_t* list, int index){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (i==index) return list->data;
	}
	belle_sip_error("belle_sip_list_nth_data: no such index in list.");
	return NULL;
}

int belle_sip_list_position(const belle_sip_list_t* list, belle_sip_list_t* elem){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (elem==list) return i;
	}
	belle_sip_error("belle_sip_list_position: no such element in list.");
	return -1;
}

int belle_sip_list_index(const belle_sip_list_t* list, void *data){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (data==list->data) return i;
	}
	belle_sip_error("belle_sip_list_index: no such element in list.");
	return -1;
}

belle_sip_list_t* belle_sip_list_insert_sorted(belle_sip_list_t* list, void *data, int (*compare_func)(const void *, const void*)){
	belle_sip_list_t* it,*previt=NULL;
	belle_sip_list_t* nelem;
	belle_sip_list_t* ret=list;
	if (list==NULL) return belle_sip_list_append(list,data);
	else{
		nelem=belle_sip_list_new(data);
		for(it=list;it!=NULL;it=it->next){
			previt=it;
			if (compare_func(data,it->data)<=0){
				nelem->prev=it->prev;
				nelem->next=it;
				if (it->prev!=NULL)
					it->prev->next=nelem;
				else{
					ret=nelem;
				}
				it->prev=nelem;
				return ret;
			}
		}
		previt->next=nelem;
		nelem->prev=previt;
	}
	return ret;
}

belle_sip_list_t* belle_sip_list_insert(belle_sip_list_t* list, belle_sip_list_t* before, void *data){
	belle_sip_list_t* elem;
	if (list==NULL || before==NULL) return belle_sip_list_append(list,data);
	for(elem=list;elem!=NULL;elem=belle_sip_list_next(elem)){
		if (elem==before){
			if (elem->prev==NULL)
				return belle_sip_list_prepend(list,data);
			else{
				belle_sip_list_t* nelem=belle_sip_list_new(data);
				nelem->prev=elem->prev;
				nelem->next=elem;
				elem->prev->next=nelem;
				elem->prev=nelem;
			}
		}
	}
	return list;
}

belle_sip_list_t* belle_sip_list_copy(const belle_sip_list_t* list){
	belle_sip_list_t* copy=NULL;
	const belle_sip_list_t* iter;
	for(iter=list;iter!=NULL;iter=belle_sip_list_next(iter)){
		copy=belle_sip_list_append(copy,iter->data);
	}
	return copy;
}

char * belle_sip_concat (const char *str, ...) {
  va_list ap;
  size_t allocated = 100;
  char *result = (char *) malloc (allocated);

  if (result != NULL)
    {
      char *newp;
      char *wp;
      const char* s;

      va_start (ap, str);

      wp = result;
      for (s = str; s != NULL; s = va_arg (ap, const char *)) {
          size_t len = strlen (s);

          /* Resize the allocated memory if necessary.  */
          if (wp + len + 1 > result + allocated)
            {
              allocated = (allocated + len) * 2;
              newp = (char *) realloc (result, allocated);
              if (newp == NULL)
                {
                  free (result);
                  return NULL;
                }
              wp = newp + (wp - result);
              result = newp;
            }
          memcpy (wp, s, len);
          wp +=len;
        }

      /* Terminate the result string.  */
      *wp++ = '\0';

      /* Resize memory to the optimal size.  */
      newp = realloc (result, wp - result);
      if (newp != NULL)
        result = newp;

      va_end (ap);
    }

	return result;
}

void *belle_sip_malloc(size_t size){
	return malloc(size);
}

void *belle_sip_malloc0(size_t size){
	void *p=malloc(size);
	memset(p,0,size);
	return p;
}

void *belle_sip_realloc(void *ptr, size_t size){
	return realloc(ptr,size);
}

void belle_sip_free(void *ptr){
	free(ptr);
}

char * belle_sip_strdup(const char *s){
	return strdup(s);
}

uint64_t belle_sip_time_ms(void){
	struct timespec ts;
	static int clock_id=CLOCK_MONOTONIC;
	if (clock_gettime(clock_id,&ts)==-1){
		belle_sip_error("clock_gettime() error for clock_id=%i: %s",clock_id,strerror(errno));
		if (clock_id==CLOCK_MONOTONIC){
			clock_id=CLOCK_REALTIME;
			return belle_sip_time_ms();
		}
		return 0;
	}
	return (ts.tv_sec*1000LL) + (ts.tv_nsec/1000000LL);

}


/**
 * parser parameter pair
 */



belle_sip_param_pair_t* belle_sip_param_pair_new(const char* name,const char* value) {
	belle_sip_param_pair_t* lPair = (belle_sip_param_pair_t*)belle_sip_new0(belle_sip_param_pair_t);
	lPair->name=name?belle_sip_strdup(name):NULL;
	lPair->value=value?belle_sip_strdup(value):NULL;
	return lPair;
}

void belle_sip_param_pair_destroy(belle_sip_param_pair_t*  pair) {
	if (pair->name) belle_sip_free(pair->name);
	if (pair->value) belle_sip_free(pair->value);
	belle_sip_free (pair);
}

int belle_sip_param_pair_comp_func(const belle_sip_param_pair_t *a, const char*b) {
	return strcmp(a->name,b);
}

char* _belle_sip_str_dup_and_unquote_string(const char* quoted_string) {
	size_t value_size = strlen(quoted_string);
	char* unquoted_string = belle_sip_malloc0(value_size-2+1);
	strncpy(unquoted_string,quoted_string+1,value_size-2);
	return unquoted_string;
}


