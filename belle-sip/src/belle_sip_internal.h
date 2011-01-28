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

typedef void (*belle_sip_object_destroy_t)(belle_sip_object_t*);

struct _belle_sip_object{
	uint8_t type_ids[8]; /*table of belle_sip_type_id_t for all inheritance chain*/
	int ref;
	void *vptr;
	belle_sip_object_destroy_t destroy;
	const char* name;
};

belle_sip_object_t * _belle_sip_object_new(size_t objsize, belle_sip_type_id_t id, void *vptr, belle_sip_object_destroy_t destroy_func, int initially_unowed);
void _belle_sip_object_init_type(belle_sip_object_t *obj, belle_sip_type_id_t id);
void belle_sip_object_init(belle_sip_object_t *obj);


#define belle_sip_object_new(_type,destroy) (_type*)_belle_sip_object_new(sizeof(_type),BELLE_SIP_TYPE_ID(_type),NULL,(belle_sip_object_destroy_t)destroy,0)
#define belle_sip_object_new_unowed(_type,destroy) (_type*)_belle_sip_object_new(sizeof(_type),BELLE_SIP_TYPE_ID(_type),NULL,(belle_sip_object_destroy_t)destroy,1)
#define belle_sip_object_init_type(obj, _type) _belle_sip_object_init_type((belle_sip_object_t*)obj, BELLE_SIP_TYPE_ID(_type))

#define belle_sip_object_new_with_vptr(_type,vptr,destroy) (_type*)_belle_sip_object_new(sizeof(_type),BELLE_SIP_TYPE_ID(_type),vptr,(belle_sip_object_destroy_t)destroy,0)
#define belle_sip_object_new_unowed_with_vptr(_type,vptr,destroy) (_type*)_belle_sip_object_new(sizeof(_type),BELLE_SIP_TYPE_ID(_type),vptr,(belle_sip_object_destroy_t)destroy,1)

#define BELLE_SIP_OBJECT_VPTR(obj,vptr_type) ((vptr_type*)(((belle_sip_object_t*)obj)->vptr))
		

struct _belle_sip_list {
	struct _belle_sip_list *next;
	struct _belle_sip_list *prev;
	void *data;
};

typedef void (*belle_sip_source_remove_callback_t)(belle_sip_source_t *);

struct belle_sip_source{
	belle_sip_object_t base;
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
	int cancelled;
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
belle_sip_list_t *belle_sip_list_find_custom(belle_sip_list_t *list, belle_sip_compare_func compare_func, const void *user_data);
belle_sip_list_t *belle_sip_list_remove_custom(belle_sip_list_t *list, belle_sip_compare_func compare_func, const void *user_data);
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

/*parameters accessors*/
#define GET_SET_STRING(object_type,attribute) \
	const char* object_type##_get_##attribute (const object_type##_t* obj) {\
		return obj->attribute;\
	}\
	void object_type##_set_##attribute (object_type##_t* obj,const char* value) {\
		if (obj->attribute != NULL) free((void*)obj->attribute);\
		obj->attribute=malloc(strlen(value)+1);\
		strcpy((char*)(obj->attribute),value);\
	}
#define GET_SET_STRING_PARAM(object_type,attribute) GET_SET_STRING_PARAM2(object_type,attribute,attribute)
#define GET_SET_STRING_PARAM2(object_type,attribute,func_name) \
	const char* object_type##_get_##func_name (const object_type##_t* obj) {\
	const char* l_value = belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(obj),#attribute);\
	if (l_value == NULL) { \
		belle_sip_warning("cannot find parameters [%s]",#attribute);\
		return NULL;\
	}\
	return l_value;\
	}\
	void object_type##_set_##func_name (object_type##_t* obj,const char* value) {\
		belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(obj),#attribute,value);\
	}


#define GET_SET_INT(object_type,attribute,type) GET_SET_INT_PRIVATE(object_type,attribute,type,)

#define GET_SET_INT_PRIVATE(object_type,attribute,type,set_prefix) \
	type  object_type##_get_##attribute (const object_type##_t* obj) {\
		return obj->attribute;\
	}\
	void set_prefix##object_type##_set_##attribute (object_type##_t* obj,type  value) {\
		obj->attribute=value;\
	}
#define GET_SET_INT_PARAM(object_type,attribute,type) GET_SET_INT_PARAM_PRIVATE(object_type,attribute,type,)
#define GET_SET_INT_PARAM2(object_type,attribute,type,func_name) GET_SET_INT_PARAM_PRIVATE2(object_type,attribute,type,,func_name)

#define ATO_(type,value) ATO_##type(value)
#define ATO_int(value) atoi(value)
#define ATO_float(value) strtof(value,NULL)
#define FORMAT_(type) FORMAT_##type
#define FORMAT_int    "%i"
#define FORMAT_float  "%f"

#define GET_SET_INT_PARAM_PRIVATE(object_type,attribute,type,set_prefix) GET_SET_INT_PARAM_PRIVATE2(object_type,attribute,type,set_prefix,attribute)
#define GET_SET_INT_PARAM_PRIVATE2(object_type,attribute,type,set_prefix,func_name) \
	type  object_type##_get_##func_name (const object_type##_t* obj) {\
		const char* l_value = belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(obj),#attribute);\
		if (l_value == NULL) { \
			belle_sip_error("cannot find parameters [%s]",#attribute);\
			return -1;\
		}\
		return ATO_(type,l_value);\
	}\
	void set_prefix##object_type##_set_##func_name (object_type##_t* obj,type  value) {\
		char l_str_value[16];\
		if (value == -1) { \
			belle_sip_parameters_remove_parameter(BELLE_SIP_PARAMETERS(obj),#attribute);\
			return;\
		}\
		snprintf(l_str_value,16,FORMAT_(type),value);\
		belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(obj),#attribute,(const char*)l_str_value);\
	}

#define GET_SET_BOOL(object_type,attribute,getter) \
	unsigned int object_type##_##getter##_##attribute (const object_type##_t* obj) {\
		return obj->attribute;\
	}\
	void object_type##_set_##attribute (object_type##_t* obj,unsigned int value) {\
		obj->attribute=value;\
	}
#define GET_SET_BOOL_PARAM2(object_type,attribute,getter,func_name) \
	unsigned int object_type##_##getter##_##func_name (const object_type##_t* obj) {\
		return belle_sip_parameters_is_parameter(BELLE_SIP_PARAMETERS(obj),#attribute);\
	}\
	void object_type##_set_##func_name (object_type##_t* obj,unsigned int value) {\
		belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(obj),#attribute,NULL);\
	}

#define BELLE_SIP_PARSE(object_type) \
belle_sip_##object_type##_t* belle_sip_##object_type##_parse (const char* value) { \
	pANTLR3_INPUT_STREAM           input; \
	pbelle_sip_messageLexer               lex; \
	pANTLR3_COMMON_TOKEN_STREAM    tokens; \
	pbelle_sip_messageParser              parser; \
	input  = antlr3NewAsciiStringCopyStream	(\
			(pANTLR3_UINT8)value,\
			(ANTLR3_UINT32)strlen(value),\
			NULL);\
	lex    = belle_sip_messageLexerNew                (input);\
	tokens = antlr3CommonTokenStreamSourceNew  (ANTLR3_SIZE_HINT, TOKENSOURCE(lex));\
	parser = belle_sip_messageParserNew               (tokens);\
	belle_sip_##object_type##_t* l_parsed_object = parser->object_type(parser);\
	parser ->free(parser);\
	tokens ->free(tokens);\
	lex    ->free(lex);\
	input  ->close(input);\
	return l_parsed_object;\
}

#define BELLE_SIP_NEW(object_type,super_type) BELLE_SIP_NEW_WITH_NAME(object_type,super_type,NULL)

#define BELLE_SIP_NEW_WITH_NAME(object_type,super_type,name) \
		belle_sip_##object_type##_t* belle_sip_##object_type##_new () { \
		belle_sip_##object_type##_t* l_object = belle_sip_object_new(belle_sip_##object_type##_t, (belle_sip_object_destroy_t)belle_sip_##object_type##_destroy);\
		belle_sip_##super_type##_init((belle_sip_##super_type##_t*)l_object); \
		belle_sip_object_set_name(BELLE_SIP_OBJECT(l_object),name);\
		return l_object;\
	}
typedef struct belle_sip_param_pair_t {
	int ref;
	char* name;
	char* value;
} belle_sip_param_pair_t;


belle_sip_param_pair_t* belle_sip_param_pair_new(const char* name,const char* value);

void belle_sip_param_pair_destroy(belle_sip_param_pair_t*  pair) ;

int belle_sip_param_pair_comp_func(const belle_sip_param_pair_t *a, const char*b) ;

belle_sip_param_pair_t* belle_sip_param_pair_ref(belle_sip_param_pair_t* obj);

void belle_sip_param_pair_unref(belle_sip_param_pair_t* obj);


void belle_sip_header_address_set_quoted_displayname(belle_sip_header_address_t* address,const char* value);

/*class parameters*/
struct _belle_sip_parameters {
	belle_sip_object_t base;

	belle_sip_list_t* param_list;
	belle_sip_list_t* paramnames_list;
};

void belle_sip_parameters_init(belle_sip_parameters_t *obj);
void belle_sip_parameters_destroy(belle_sip_parameters_t* params);

typedef struct belle_sip_udp_listening_point belle_sip_udp_listening_point_t;

#define BELLE_SIP_LISTENING_POINT(obj) BELLE_SIP_CAST(obj,belle_sip_listening_point_t)
#define BELLE_SIP_UDP_LISTENING_POINT(obj) BELLE_SIP_CAST(obj,belle_sip_udp_listening_point_t)
#define BELLE_SIP_CHANNEL(obj)		BELLE_SIP_CAST(obj,belle_sip_channel_t)

belle_sip_listening_point_t * belle_sip_udp_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port);
belle_sip_channel_t *belle_sip_listening_point_find_output_channel(belle_sip_listening_point_t *ip, const struct addrinfo *dest); 
belle_sip_source_t *belle_sip_channel_create_source(belle_sip_channel_t *, unsigned int events, int timeout, belle_sip_source_func_t callback, void *data);
int belle_sip_listening_point_get_well_known_port(const char *transport);


/*
 belle_sip_stack_t
*/
struct belle_sip_stack{
	belle_sip_object_t base;
	belle_sip_main_loop_t *ml;
	belle_sip_list_t *lp;/*list of listening points*/
};

void belle_sip_stack_get_next_hop(belle_sip_stack_t *stack, belle_sip_request_t *req, belle_sip_hop_t *hop);


/*
 belle_sip_provider_t
*/
belle_sip_provider_t *belle_sip_provider_new(belle_sip_stack_t *s, belle_sip_listening_point_t *lp);

typedef struct listener_ctx{
	belle_sip_listener_t *listener;
	void *data;
}listener_ctx_t;

#define BELLE_SIP_PROVIDER_INVOKE_LISTENERS(provider,callback,event) \
{ \
	belle_sip_list_t *_elem; \
	for(_elem=(provider)->listeners;_elem!=NULL;_elem=_elem->next){ \
		listener_ctx_t *_lctx=(listener_ctx_t*)_elem->data; \
		_lctx->##callback(_lctx->data,event); \
	} \
}

/*
 belle_sip_transaction_t
*/

belle_sip_client_transaction_t * belle_sip_client_transaction_new(belle_sip_provider_t *prov,belle_sip_request_t *req);
belle_sip_server_transaction_t * belle_sip_server_transaction_new(belle_sip_provider_t *prov,belle_sip_request_t *req);

/*
 belle_sip_response_t
*/
void belle_sip_response_get_return_hop(belle_sip_response_t *msg, belle_sip_hop_t *hop);

#ifdef __cplusplus
}
#endif

/*include private headers */
#include "belle_sip_resolver.h"
#include "sender_task.h"

#define BELLE_SIP_SOCKET_TIMEOUT 30000

#endif
