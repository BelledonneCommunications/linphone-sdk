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


unsigned int __belle_sip_log_mask=BELLE_SIP_LOG_WARNING|BELLE_SIP_LOG_ERROR|BELLE_SIP_LOG_FATAL;

/**
 * @ param level: either BELLE_SIP_LOG_DEBUG, BELLE_SIP_LOG_MESSAGE, BELLE_SIP_LOG_WARNING, BELLE_SIP_LOG_ERROR
 * BELLE_SIP_LOG_FATAL .
**/
void belle_sip_set_log_level(int level){
        __belle_sip_log_mask=(level<<1)-1;
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
        if ((level)==BELLE_SIP_LOG_FATAL) abort();
}
#endif

static void __belle_sip_logv_out(belle_sip_log_level lev, const char *fmt, va_list args){
        const char *lname="undef";
        char *msg;
        if (__log_file==NULL) __log_file=stderr;
        switch(lev){
                case BELLE_SIP_LOG_DEBUG:
                        lname="debug";
                        break;
                case BELLE_SIP_LOG_MESSAGE:
                        lname="message";
                        break;
                case BELLE_SIP_LOG_WARNING:
                        lname="warning";
                        break;
                case BELLE_SIP_LOG_ERROR:
                        lname="error";
                        break;
                case BELLE_SIP_LOG_FATAL:
                        lname="fatal";
                        break;
                default:
                        belle_sip_fatal("Bad level !");
        }
        msg=belle_sip_strdup_vprintf(fmt,args);
#if defined(_MSC_VER) && !defined(_WIN32_WCE)
	#ifndef _UNICODE
        OutputDebugString(msg);
        OutputDebugString("\r\n");
	#else
		{
			int len=strlen(msg);
			wchar_t *tmp=(wchar_t*)belle_sip_malloc((len+1)*sizeof(wchar_t));
			mbstowcs(tmp,msg,len);
			OutputDebugString(tmp);
			OutputDebugString(L"\r\n");
			belle_sip_free(tmp);
		}
	#endif
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
	if (elem==NULL)  return new_elem;
	if (new_elem==NULL)  return elem;
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

belle_sip_list_t * belle_sip_list_last_elem(const belle_sip_list_t *l){
	if (l==NULL) return NULL;
	while(l->next){
		l=l->next;
	}
	return (belle_sip_list_t*)l;
}

belle_sip_list_t*  belle_sip_list_concat(belle_sip_list_t* first, belle_sip_list_t* second){
	belle_sip_list_t* it=first;
	if (it==NULL) return second;
	if (second==NULL) return first;
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

belle_sip_list_t* belle_sip_list_copy_with_data(const belle_sip_list_t* list, void* (*copyfunc)(void*)){
	belle_sip_list_t* copy=NULL;
	const belle_sip_list_t* iter;
	for(iter=list;iter!=NULL;iter=belle_sip_list_next(iter)){
		copy=belle_sip_list_append(copy,copyfunc(iter->data));
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

#ifndef WIN32

static int find_best_clock_id () {
	struct timespec ts;
	static int clock_id=-1;
#ifndef ANDROID
#define DEFAULT_CLOCK_MODE CLOCK_MONOTONIC
#else
#define DEFAULT_CLOCK_MODE CLOCK_BOOTTIME
#endif
	if (clock_id==-1) {
		if (clock_gettime(DEFAULT_CLOCK_MODE,&ts)!=1){
			clock_id=CLOCK_MONOTONIC;
		} else if (clock_gettime(CLOCK_REALTIME,&ts)!=1){
			clock_id=CLOCK_REALTIME;
		} else {
			belle_sip_fatal("Cannot find suitable clock mode");
		}
	}
	return clock_id;
}
uint64_t belle_sip_time_ms(void){
	struct timespec ts;
	if (clock_gettime(find_best_clock_id(),&ts)==-1){
		belle_sip_error("clock_gettime() error for clock_id=%i: %s",find_best_clock_id(),strerror(errno));
		return 0;
	}
	return (ts.tv_sec*1000LL) + (ts.tv_nsec/1000000LL);
}
#else
uint64_t belle_sip_time_ms(void){
#ifdef WINAPI_FAMILY_PHONE_APP
	return GetTickCount64();
#else
	return GetTickCount();
#endif
}
#endif

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
int belle_sip_param_pair_case_comp_func(const belle_sip_param_pair_t *a, const char*b) {
	return strcasecmp(a->name,b);
}

char* _belle_sip_str_dup_and_unquote_string(const char* quoted_string) {
	size_t value_size = strlen(quoted_string);
	char* unquoted_string = belle_sip_malloc0(value_size-2+1);
	strncpy(unquoted_string,quoted_string+1,value_size-2);
	return unquoted_string;
}

unsigned int belle_sip_random(void){
#if  defined(__linux) || defined(__APPLE__)
	static int fd=-1;
	if (fd==-1) fd=open("/dev/urandom",O_RDONLY);
	if (fd!=-1){
		unsigned int tmp;
		if (read(fd,&tmp,4)!=4){
			belle_sip_error("Reading /dev/urandom failed.");
		}else return tmp;
	}else belle_sip_error("Could not open /dev/urandom");
#elif defined(WIN32)
	static int initd=0;
	if (!initd) {
		srand((unsigned int)belle_sip_time_ms());
		initd=1;
	}
	return rand()<<16 | rand();
#endif
	/*fallback to random()*/
#ifndef WIN32
	return (unsigned int) random();
#endif
}

static const char *symbols="aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789-~";

/**
 * Write a random text token of supplied size.
**/
char * belle_sip_random_token(char *ret, size_t size){
	unsigned int val;
	unsigned int i,j;
	for(i=0,j=0;i<size-1;++i,++j){
		if (j%5==0) val=belle_sip_random();
		ret[i]=symbols[val & 63];
		val=val>>6;
	}
	ret[i]=0;
	return ret;
}

typedef struct bits_reader{
	const uint8_t *buffer;
	size_t buf_size;
	int bit_index;
}bits_reader_t;

static void bits_reader_init(bits_reader_t *reader, const uint8_t *buffer, size_t bufsize){
	reader->buffer=buffer;
	reader->buf_size=bufsize;
	reader->bit_index=0;
}

static int bits_reader_read(bits_reader_t *reader, int count, unsigned int *ret){
	unsigned int tmp;
	size_t byte_index=reader->bit_index/8;
	size_t bit_index=reader->bit_index % 8;
	int shift=32-bit_index-count;
	
	if (count>=24){
		belle_sip_error("This bit reader cannot read more than 24 bits at once.");
		return -1;
	}
	
	if (byte_index<reader->buf_size)
		tmp=((unsigned int)reader->buffer[byte_index++])<<24;
	else{
		belle_sip_error("Bit reader goes end of stream.");
		return -1;
	}
	if (byte_index<reader->buf_size)
		tmp|=((unsigned int)reader->buffer[byte_index++])<<16;
	if (byte_index<reader->buf_size)
		tmp|=((unsigned int)reader->buffer[byte_index++])<<8;
	if (byte_index<reader->buf_size)
		tmp|=((unsigned int)reader->buffer[byte_index++]);
	
	tmp=tmp>>shift;
	tmp=tmp & ((1<<count)-1);
	reader->bit_index+=count;
	*ret=tmp;
	return 0;
}

char * belle_sip_octets_to_text(const uint8_t *hash, size_t hash_len, char *ret, size_t size){
	int i;
	bits_reader_t bitctx;
	
	bits_reader_init(&bitctx,hash,hash_len);
	
	for(i=0;i<(int)size-1;++i){
		unsigned int val=0;
		if (bits_reader_read(&bitctx,6,&val)==0){
			ret[i]=symbols[val];
		}else break;
	}
	ret[i]=0;
	return ret;
}

void belle_sip_util_copy_headers(belle_sip_message_t *orig, belle_sip_message_t *dest, const char*header, int multiple){
	const belle_sip_list_t *elem;
	elem=belle_sip_message_get_headers(orig,header);
	for (;elem!=NULL;elem=elem->next){
		belle_sip_header_t *ref_header=(belle_sip_header_t*)elem->data;
		if (ref_header){
			ref_header=(belle_sip_header_t*)belle_sip_object_clone((belle_sip_object_t*)ref_header);
			if (!multiple){
				belle_sip_message_set_header(dest,ref_header);
				break;
			}else
				belle_sip_message_add_header(dest,ref_header);
		}
	}
}

int belle_sip_get_char (const char*a,int n,char*out) {
	char result;
	unsigned int tmp;
	if (*a=='%' && n>2) {
		sscanf(a+1,"%02x",&tmp);
		*out=(char)tmp;
		return 3;
	} else {
		*out=*a;
		return 1;
	}
return result;
}
char* belle_sip_to_unescaped_string(const char* buff) {
	char output_buff[BELLE_SIP_MAX_TO_STRING_SIZE];
	unsigned int i;
	unsigned int out_buff_index=0;
	output_buff[BELLE_SIP_MAX_TO_STRING_SIZE-1]='\0';
	for(i=0;buff[i]!='\0' &&i<BELLE_SIP_MAX_TO_STRING_SIZE /*to make sure last param can be stored in escaped form*/;) {
		i+=belle_sip_get_char(buff+i,3,output_buff+out_buff_index++);
	}
	output_buff[out_buff_index]='\0';
	return belle_sip_strdup(output_buff);
}
char* belle_sip_to_escaped_string(const char* buff) {
	char output_buff[BELLE_SIP_MAX_TO_STRING_SIZE];
	unsigned int i;
	unsigned int out_buff_index=0;
	output_buff[BELLE_SIP_MAX_TO_STRING_SIZE-1]='\0';
	for(i=0;buff[i]!='\0' &&i<BELLE_SIP_MAX_TO_STRING_SIZE-4 /*to make sure last param can be stored in escaped form*/;i++) {

		/*hvalue          =  *( hnv-unreserved / unreserved / escaped )
		hnv-unreserved  =  "[" / "]" / "/" / "?" / ":" / "+" / "$"
		unreserved  =  alphanum / mark
      	mark        =  "-" / "_" / "." / "!" / "~" / "*" / "'"
                     / "(" / ")"*/

		switch(buff[i]) {
		case '[' :
		case ']' :
		case '/' :
		case '?' :
		case ':' :
		case '+' :
		case '$' :
		case '-' :
		case '_' :
		case '.' :
		case '!' :
		case '~' :
		case '*' :
		case '\'' :
		case '(' :
		case ')' :
			output_buff[out_buff_index++]=buff[i];
			break;
		default:
			/*serach for alfanum*/
			if ((buff[i]>='0' && buff[i]<='9')
				|| (buff[i]>='A' && buff[i]<='Z')
				|| (buff[i]>='a' && buff[i]<='z')
				|| (buff[i]=='\0')) {
				output_buff[out_buff_index++]=buff[i];
			} else {
				out_buff_index+=sprintf(output_buff+out_buff_index,"%%%02x",buff[i]);
			}
			break;
		}
	}
	output_buff[out_buff_index]='\0';
	return belle_sip_strdup(output_buff);
}
