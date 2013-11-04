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
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

/* include all public headers*/
#include "belle-sip/belle-sip.h"

#include "port.h"

#ifdef HAVE_CONFIG_H

#ifdef PACKAGE
#undef PACKAGE
#endif
#ifdef PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#endif
#ifdef PACKAGE_NAME
#undef PACKAGE_NAME
#endif
#ifdef PACKAGE_STRING
#undef PACKAGE_STRING
#endif
#ifdef PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#endif
#ifdef VERSION
#undef VERSION
#endif
#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#endif

#include "config.h"

#else

#ifndef PACKAGE_VERSION
#error "PACKAGE_VERSION must be defined and equal to the VERSION file included in the belle-sip repository"
#endif

#endif

#include "port.h"

/*etc*/

#define BELLE_SIP_INTERFACE_GET_METHODS(obj,interface) \
	((BELLE_SIP_INTERFACE_METHODS_TYPE(interface)*)belle_sip_object_get_interface_methods((belle_sip_object_t*)obj,BELLE_SIP_INTERFACE_ID(interface)))

#define __BELLE_SIP_INVOKE_LISTENER_BEGIN(list,interface_name,method) \
	if (list!=NULL) {\
		belle_sip_list_t *__copy=belle_sip_list_copy_with_data((list), (void* (*)(void*))belle_sip_object_ref);\
		const belle_sip_list_t *__elem=__copy;\
		do{\
			void *__method;\
			interface_name *__obj=(interface_name*)__elem->data;\
			__method=BELLE_SIP_INTERFACE_GET_METHODS(__obj,interface_name)->method;\
			if (__method) BELLE_SIP_INTERFACE_GET_METHODS(__obj,interface_name)->

#define __BELLE_SIP_INVOKE_LISTENER_END \
			__elem=__elem->next;\
		}while(__elem!=NULL);\
		belle_sip_list_free_with_data(__copy,belle_sip_object_unref);\
	}

#define BELLE_SIP_INVOKE_LISTENERS_VOID(list,interface_name,method) \
			__BELLE_SIP_INVOKE_LISTENER_BEGIN(list,interface_name,method)\
			method(__obj);\
			__BELLE_SIP_INVOKE_LISTENER_END

#define BELLE_SIP_INVOKE_LISTENERS_ARG(list,interface_name,method,arg) \
	__BELLE_SIP_INVOKE_LISTENER_BEGIN(list,interface_name,method)\
	method(__obj,arg);\
	__BELLE_SIP_INVOKE_LISTENER_END


#define BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(list,interface_name,method,arg1,arg2) \
			__BELLE_SIP_INVOKE_LISTENER_BEGIN(list,interface_name,method)\
			method(__obj,arg1,arg2);\
			__BELLE_SIP_INVOKE_LISTENER_END

#define BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2_ARG3(list,interface_name,method,arg1,arg2,arg3) \
			__BELLE_SIP_INVOKE_LISTENER_BEGIN(list,interface_name)\
			method(__obj,arg1,arg2,arg3);\
			__BELLE_SIP_INVOKE_LISTENER_END

typedef struct weak_ref{
	struct weak_ref *next;
	belle_sip_object_destroy_notify_t notify;
	void *userpointer;
}weak_ref_t;


void *belle_sip_object_get_interface_methods(belle_sip_object_t *obj, belle_sip_interface_id_t ifid);
/*used internally by unref()*/
void belle_sip_object_delete(void *obj);
void belle_sip_object_pool_add(belle_sip_object_pool_t *pool, belle_sip_object_t *obj);
void belle_sip_object_pool_remove(belle_sip_object_pool_t *pool, belle_sip_object_t *obj);


#define BELLE_SIP_OBJECT_VPTR(obj,object_type) ((BELLE_SIP_OBJECT_VPTR_TYPE(object_type)*)(((belle_sip_object_t*)obj)->vptr))
#define belle_sip_object_init(obj)		/*nothing*/


/*list of all vptrs (classes) used in belle-sip*/
BELLE_SIP_DECLARE_VPTR(belle_sip_object_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_stack_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_datagram_listening_point_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_provider_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_main_loop_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_source_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_resolver_context_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_dialog_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_address_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_contact_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_from_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_to_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_via_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_uri_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_message_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_request_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_response_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_parameters_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_call_id_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_cseq_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_content_type_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_route_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_record_route_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_user_agent_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_content_length_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_extension_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_authorization_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_www_authenticate_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_proxy_authenticate_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_proxy_authorization_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_max_forwards_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_expires_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_allow_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_attribute_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_bandwidth_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_connection_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_email_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_info_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_key_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_media_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_media_description_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_origin_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_phone_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_repeate_time_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_session_description_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_session_name_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_time_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_time_description_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_uri_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_version_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_base_description_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_mime_parameter_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_refresher_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_subscription_state_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_service_route_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_refer_to_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_referred_by_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_replaces_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_date_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_hop_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_object_pool_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_p_preferred_identity_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_privacy_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_certificates_chain_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_signing_key_t);

typedef void (*belle_sip_source_remove_callback_t)(belle_sip_source_t *);




struct belle_sip_source{
	belle_sip_object_t base;
	belle_sip_list_t node;
	unsigned long id;
	belle_sip_fd_t fd;
	unsigned short events,revents;
	int timeout;
	void *data;
	uint64_t expire_ms;
	int index; /* index in pollfd table */
	belle_sip_source_func_t notify;
	belle_sip_source_remove_callback_t on_remove;
	unsigned char cancelled;
	unsigned char expired;
	unsigned char oneshot;
	unsigned char notify_required; /*for testing purpose, use to ask for being scheduled*/
	belle_sip_socket_t sock;
};

void belle_sip_socket_source_init(belle_sip_source_t *s, belle_sip_source_func_t func, void *data, belle_sip_socket_t fd, unsigned int events, unsigned int timeout_value_ms);
void belle_sip_fd_source_init(belle_sip_source_t *s, belle_sip_source_func_t func, void *data, belle_sip_fd_t fd, unsigned int events, unsigned int timeout_value_ms);
void belle_sip_source_uninit(belle_sip_source_t *s);

#define belle_list_next(elem) ((elem)->next)

/* include private headers */
#include "channel.h"




#define belle_sip_new(type) (type*)belle_sip_malloc(sizeof(type))
#define belle_sip_new0(type) (type*)belle_sip_malloc0(sizeof(type))
	
belle_sip_list_t *belle_sip_list_new(void *data);
belle_sip_list_t*  belle_sip_list_append_link(belle_sip_list_t* elem,belle_sip_list_t *new_elem);
belle_sip_list_t *belle_sip_list_delete_custom(belle_sip_list_t *list, belle_sip_compare_func compare_func, const void *user_data);

#define belle_sip_list_next(elem) ((elem)->next)





#undef MIN
#define MIN(a,b)	((a)>(b) ? (b) : (a))
#undef MAX
#define MAX(a,b)	((a)>(b) ? (a) : (b))


BELLESIP_INTERNAL_EXPORT char * belle_sip_concat (const char *str, ...);

BELLESIP_INTERNAL_EXPORT uint64_t belle_sip_time_ms(void);

BELLESIP_INTERNAL_EXPORT unsigned int belle_sip_random(void);


/*parameters accessors*/
#define GET_SET_STRING(object_type,attribute) \
	const char* object_type##_get_##attribute (const object_type##_t* obj) {\
		return obj->attribute;\
	}\
	void object_type##_set_##attribute (object_type##_t* obj,const char* value) {\
		if (obj->attribute != NULL) belle_sip_free((void*)obj->attribute);\
		if (value) {\
			obj->attribute=belle_sip_strdup(value); \
		} else obj->attribute=NULL;\
	}
/*#define GET_SET_STRING_PARAM_NULL_ALLOWED(object_type,attribute) \
	GET_STRING_PARAM2(object_type,attribute,attribute) \
	void object_type##_set_##func_name (object_type##_t* obj,const char* value) {\
		belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(obj),#attribute,value);\
	}
*/
#define GET_SET_STRING_PARAM(object_type,attribute) GET_SET_STRING_PARAM2(object_type,attribute,attribute)
#define GET_SET_STRING_PARAM2(object_type,attribute,func_name) \
	GET_STRING_PARAM2(object_type,attribute,func_name) \
	void object_type##_set_##func_name (object_type##_t* obj,const char* value) {\
	if (belle_sip_parameters_has_parameter(BELLE_SIP_PARAMETERS(obj),#attribute) && !value) {\
		belle_sip_parameters_remove_parameter(BELLE_SIP_PARAMETERS(obj),#attribute); \
	} else \
		belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(obj),#attribute,value);\
	}

#define GET_STRING_PARAM2(object_type,attribute,func_name) \
	const char* object_type##_get_##func_name (const object_type##_t* obj) {\
	const char* l_value = belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(obj),#attribute);\
	if (l_value == NULL) { \
		/*belle_sip_warning("cannot find parameters [%s]",#attribute);*/\
		return NULL;\
	}\
	return l_value;\
	}

#define DESTROY_STRING(object,attribute) if (object->attribute) belle_sip_free((void*)object->attribute);

#define CLONE_STRING_GENERIC(object_type_src,object_type_dest,attribute,dest,src) \
		if ( object_type_src##_get_##attribute (src)) {\
			object_type_dest##_set_##attribute(dest,object_type_src##_get_##attribute(src));\
		}

#define CLONE_STRING(object_type,attribute,dest,src) CLONE_STRING_GENERIC(object_type,object_type,attribute,dest,src)

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
#define ATO_float(value) (float)strtod(value,NULL)
#define FORMAT_(type) FORMAT_##type
#define FORMAT_int    "%i"
#define FORMAT_float  "%f"

#define GET_SET_INT_PARAM_PRIVATE(object_type,attribute,type,set_prefix) GET_SET_INT_PARAM_PRIVATE2(object_type,attribute,type,set_prefix,attribute)
#define GET_SET_INT_PARAM_PRIVATE2(object_type,attribute,type,set_prefix,func_name) \
	type  object_type##_get_##func_name (const object_type##_t* obj) {\
		const char* l_value = belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(obj),#attribute);\
		if (l_value == NULL) { \
			/*belle_sip_error("cannot find parameters [%s]",#attribute);*/\
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
		return belle_sip_parameters_has_parameter(BELLE_SIP_PARAMETERS(obj),#attribute);\
	}\
	void object_type##_set_##func_name (object_type##_t* obj,unsigned int value) {\
		belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(obj),#attribute,NULL);\
	}

#if HAVE_ANTLR_STRING_STREAM_NEW
#define ANTLR_STREAM_NEW(object_type, value,length) \
antlr3StringStreamNew((pANTLR3_UINT8)value,ANTLR3_ENC_8BIT,(ANTLR3_UINT32)length,(pANTLR3_UINT8)#object_type)
#else
#define ANTLR_STREAM_NEW(object_type, value, length) \
antlr3NewAsciiStringCopyStream((pANTLR3_UINT8)value,(ANTLR3_UINT32)length,NULL)
#endif /*HAVE_ANTLR_STRING_STREAM_NEW*/


#define BELLE_SIP_PARSE(object_type) \
belle_sip_##object_type##_t* belle_sip_##object_type##_parse (const char* value) { \
	pANTLR3_INPUT_STREAM           input; \
	pbelle_sip_messageLexer               lex; \
	pANTLR3_COMMON_TOKEN_STREAM    tokens; \
	pbelle_sip_messageParser              parser; \
	belle_sip_##object_type##_t* l_parsed_object; \
	input  = ANTLR_STREAM_NEW(object_type,value,strlen(value));\
	lex    = belle_sip_messageLexerNew                (input);\
	tokens = antlr3CommonTokenStreamSourceNew  (ANTLR3_SIZE_HINT, TOKENSOURCE(lex));\
	parser = belle_sip_messageParserNew               (tokens);\
	l_parsed_object = parser->object_type(parser);\
	parser ->free(parser);\
	tokens ->free(tokens);\
	lex    ->free(lex);\
	input  ->close(input);\
	if (l_parsed_object == NULL) belle_sip_error(#object_type" parser error for [%s]",value);\
	return l_parsed_object;\
}

#define BELLE_SIP_NEW(object_type,super_type) \
	BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_##object_type##_t); \
	BELLE_SIP_INSTANCIATE_VPTR(	belle_sip_##object_type##_t\
									, belle_sip_##super_type##_t\
									, belle_sip_##object_type##_destroy\
									, belle_sip_##object_type##_clone\
									, belle_sip_##object_type##_marshal, TRUE); \
	belle_sip_##object_type##_t* belle_sip_##object_type##_new () { \
		belle_sip_##object_type##_t* l_object = belle_sip_object_new(belle_sip_##object_type##_t);\
		belle_sip_##super_type##_init((belle_sip_##super_type##_t*)l_object); \
		belle_sip_##object_type##_init((belle_sip_##object_type##_t*) l_object); \
		return l_object;\
	}

	
#define BELLE_SIP_NEW_HEADER(object_type,super_type,name) BELLE_SIP_NEW_HEADER_INIT(object_type,super_type,name,header)
#define BELLE_SIP_NEW_HEADER_INIT(object_type,super_type,name,init_type) \
	BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_##object_type##_t); \
	BELLE_SIP_INSTANCIATE_VPTR(	belle_sip_##object_type##_t\
								, belle_sip_##super_type##_t\
								, belle_sip_##object_type##_destroy\
								, belle_sip_##object_type##_clone\
								, belle_sip_##object_type##_marshal, TRUE); \
	belle_sip_##object_type##_t* belle_sip_##object_type##_new () { \
		belle_sip_##object_type##_t* l_object = belle_sip_object_new(belle_sip_##object_type##_t);\
		belle_sip_##super_type##_init((belle_sip_##super_type##_t*)l_object); \
		belle_sip_##init_type##_init((belle_sip_##init_type##_t*) l_object); \
		if (name) belle_sip_header_set_name(BELLE_SIP_HEADER(l_object),name);\
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
int belle_sip_param_pair_case_comp_func(const belle_sip_param_pair_t *a, const char*b) ;

belle_sip_param_pair_t* belle_sip_param_pair_ref(belle_sip_param_pair_t* obj);

void belle_sip_param_pair_unref(belle_sip_param_pair_t* obj);


void belle_sip_header_address_set_quoted_displayname(belle_sip_header_address_t* address,const char* value);

/*calss header*/
struct _belle_sip_header {
	belle_sip_object_t base;
	belle_sip_header_t* next;
	char *name;
	char *unparsed_value;
};

void belle_sip_header_set_next(belle_sip_header_t* header,belle_sip_header_t* next);
BELLESIP_INTERNAL_EXPORT belle_sip_header_t* belle_sip_header_get_next(const belle_sip_header_t* headers);
void belle_sip_response_fill_for_dialog(belle_sip_response_t *obj, belle_sip_request_t *req);
void belle_sip_util_copy_headers(belle_sip_message_t *orig, belle_sip_message_t *dest, const char*header, int multiple);

void belle_sip_header_init(belle_sip_header_t* obj);
/*class parameters*/
struct _belle_sip_parameters {
	belle_sip_header_t base;
	belle_sip_list_t* param_list;
	belle_sip_list_t* paramnames_list;
};

void belle_sip_parameters_init(belle_sip_parameters_t *obj);

/*
 * Listening points
*/

#include "listeningpoint_internal.h"


struct belle_sip_hop{
	belle_sip_object_t base;
	char *cname;
	char *host;
	char *transport;
	int port;
};


/*
 belle_sip_stack_t
*/
struct belle_sip_stack{
	belle_sip_object_t base;
	belle_sip_main_loop_t *ml;
	belle_sip_timer_config_t timer_config;
	int transport_timeout;
	int inactive_transport_timeout;
	int dns_timeout;
	int tx_delay; /*used to simulate network transmission delay, for tests*/
	int send_error; /* used to simulate network error. if <0, channel_send will return this value*/
	int resolver_tx_delay; /*used to simulate network transmission delay, for tests*/
	int resolver_send_error;	/* used to simulate network error*/
	int dscp;
	char *dns_user_hosts_file; /* used to load additional hosts file for tests */
};

belle_sip_hop_t* belle_sip_hop_new(const char* transport, const char *cname, const char* host,int port);
belle_sip_hop_t* belle_sip_hop_new_from_uri(const belle_sip_uri_t *uri);

belle_sip_hop_t * belle_sip_stack_get_next_hop(belle_sip_stack_t *stack, belle_sip_request_t *req);
const belle_sip_timer_config_t *belle_sip_stack_get_timer_config(const belle_sip_stack_t *stack);

/*
 belle_sip_provider_t
*/

struct belle_sip_provider{
	belle_sip_object_t base;
	belle_sip_stack_t *stack;
	belle_sip_list_t *lps; /*listening points*/
	belle_sip_list_t *listeners;
	belle_sip_list_t *internal_listeners; /*for transaction internaly managed by belle-sip. I.E by refreshers*/
	belle_sip_list_t *client_transactions;
	belle_sip_list_t *server_transactions;
	belle_sip_list_t *dialogs;
	belle_sip_list_t *auth_contexts;
	unsigned char rport_enabled; /*0 if rport should not be set in via header*/
	unsigned char nat_helper;
	unsigned char unconditional_answer_enabled;
};

belle_sip_provider_t *belle_sip_provider_new(belle_sip_stack_t *s, belle_sip_listening_point_t *lp);
void belle_sip_provider_add_client_transaction(belle_sip_provider_t *prov, belle_sip_client_transaction_t *t);
belle_sip_client_transaction_t *belle_sip_provider_find_matching_client_transaction(belle_sip_provider_t *prov, belle_sip_response_t *resp);
void belle_sip_provider_remove_client_transaction(belle_sip_provider_t *prov, belle_sip_client_transaction_t *t);
void belle_sip_provider_add_server_transaction(belle_sip_provider_t *prov, belle_sip_server_transaction_t *t);
belle_sip_server_transaction_t * belle_sip_provider_find_matching_server_transaction(belle_sip_provider_t *prov, 
                                                                                   belle_sip_request_t *req);
void belle_sip_provider_remove_server_transaction(belle_sip_provider_t *prov, belle_sip_server_transaction_t *t);
void belle_sip_provider_set_transaction_terminated(belle_sip_provider_t *p, belle_sip_transaction_t *t);
belle_sip_channel_t * belle_sip_provider_get_channel(belle_sip_provider_t *p, const belle_sip_hop_t *hop);
void belle_sip_provider_add_dialog(belle_sip_provider_t *prov, belle_sip_dialog_t *dialog);
void belle_sip_provider_remove_dialog(belle_sip_provider_t *prov, belle_sip_dialog_t *dialog);
void belle_sip_provider_release_channel(belle_sip_provider_t *p, belle_sip_channel_t *chan);
void belle_sip_provider_add_internal_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l, int prepend);
void belle_sip_provider_remove_internal_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l);
belle_sip_client_transaction_t * belle_sip_provider_find_matching_client_transaction_from_req(belle_sip_provider_t *prov, belle_sip_request_t *req) ;
/*for testing purpose only:*/
void belle_sip_provider_dispatch_message(belle_sip_provider_t *prov, belle_sip_message_t *msg);

typedef struct listener_ctx{
	belle_sip_listener_t *listener;
	void *data;
}listener_ctx_t;

#define BELLE_SIP_PROVIDER_INVOKE_LISTENERS_FOR_TRANSACTION(t,callback,event) \
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS((t)->is_internal?(t)->provider->internal_listeners:(t)->provider->listeners,callback,event)

#define BELLE_SIP_PROVIDER_INVOKE_LISTENERS(listeners,callback,event) \
	BELLE_SIP_INVOKE_LISTENERS_ARG((listeners),belle_sip_listener_t,callback,(event))


struct _belle_sip_message {
	belle_sip_object_t base;
	belle_sip_list_t* header_list;
	char* body;
	unsigned int body_length;
};
	
struct _belle_sip_request {
	belle_sip_message_t message;
	char* method;
	belle_sip_uri_t* uri;
	belle_sip_dialog_t *dialog;/*set if request was created by a dialog to avoid to search in dialog list*/
	char *rfc2543_branch; /*computed 'branch' id in case we receive this request from an old RFC2543 stack*/
	unsigned char dialog_queued;
};
	
/*
 belle_sip_transaction_t
*/

struct belle_sip_transaction{
	belle_sip_object_t base;
	belle_sip_provider_t *provider; /*the provider that created this transaction */
	belle_sip_request_t *request;
	belle_sip_response_t *last_response;
	belle_sip_channel_t *channel;
	belle_sip_dialog_t *dialog;
	char *branch_id;
	belle_sip_transaction_state_t state;
	void *appdata;
	unsigned char is_internal;
	unsigned char timed_out; 
};


BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_transaction_t,belle_sip_object_t)
	void (*on_terminate)(belle_sip_transaction_t *obj);
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

static BELLESIP_INLINE const belle_sip_timer_config_t * belle_sip_transaction_get_timer_config(belle_sip_transaction_t *obj){
	return belle_sip_stack_get_timer_config(obj->provider->stack);
}

static BELLESIP_INLINE void belle_sip_transaction_start_timer(belle_sip_transaction_t *obj, belle_sip_source_t *timer){
	belle_sip_main_loop_add_source(obj->provider->stack->ml,timer);
}
/** */
static BELLESIP_INLINE void belle_sip_transaction_stop_timer(belle_sip_transaction_t *obj, belle_sip_source_t *timer){
	belle_sip_main_loop_remove_source(obj->provider->stack->ml,timer);
}

void belle_sip_transaction_notify_timeout(belle_sip_transaction_t *t);

void belle_sip_transaction_set_dialog(belle_sip_transaction_t *t, belle_sip_dialog_t *dialog);

void belle_sip_transaction_set_state(belle_sip_transaction_t *t, belle_sip_transaction_state_t state);

/*
 *
 *
 *	Client transaction
 *
 *
*/

struct belle_sip_client_transaction{
	belle_sip_transaction_t base;
	belle_sip_uri_t* preset_route; /*use to store outbound proxy, will be helpful for refresher*/
	belle_sip_hop_t* next_hop; /*use to send cancel request*/
};

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_client_transaction_t,belle_sip_transaction_t)
	void (*send_request)(belle_sip_client_transaction_t *);
	void (*on_response)(belle_sip_client_transaction_t *obj, belle_sip_response_t *resp);
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

void belle_sip_client_transaction_init(belle_sip_client_transaction_t *obj, belle_sip_provider_t *prov, belle_sip_request_t *req);
void belle_sip_client_transaction_add_response(belle_sip_client_transaction_t *t, belle_sip_response_t *resp);
void belle_sip_client_transaction_notify_response(belle_sip_client_transaction_t *t, belle_sip_response_t *resp);

struct belle_sip_ict{
	belle_sip_client_transaction_t base;
	belle_sip_source_t *timer_A;
	belle_sip_source_t *timer_B;
	belle_sip_source_t *timer_D;
	belle_sip_source_t *timer_M;
	belle_sip_request_t *ack;
};

typedef struct belle_sip_ict belle_sip_ict_t;

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_ict_t,belle_sip_client_transaction_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

belle_sip_ict_t * belle_sip_ict_new(belle_sip_provider_t *prov, belle_sip_request_t *req);

struct belle_sip_nict{
	belle_sip_client_transaction_t base;
	belle_sip_source_t *timer_F;
	belle_sip_source_t *timer_E;
	belle_sip_source_t *timer_K;
};

typedef struct belle_sip_nict belle_sip_nict_t;

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_nict_t,belle_sip_client_transaction_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

belle_sip_nict_t * belle_sip_nict_new(belle_sip_provider_t *prov, belle_sip_request_t *req);


/*
 *
 *
 *	Server transaction
 *
 *
*/

struct belle_sip_server_transaction{
	belle_sip_transaction_t base;
	char to_tag[8];
};

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_server_transaction_t,belle_sip_transaction_t)
	int (*send_new_response)(belle_sip_server_transaction_t *, belle_sip_response_t *resp);
	void (*on_request_retransmission)(belle_sip_server_transaction_t *obj);
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

void belle_sip_server_transaction_init(belle_sip_server_transaction_t *t, belle_sip_provider_t *prov,belle_sip_request_t *req);
void belle_sip_server_transaction_on_request(belle_sip_server_transaction_t *t, belle_sip_request_t *req);

struct belle_sip_ist{
	belle_sip_server_transaction_t base;
	belle_sip_source_t *timer_G;
	belle_sip_source_t *timer_H;
	belle_sip_source_t *timer_I;
	belle_sip_source_t *timer_L;
};

typedef struct belle_sip_ist belle_sip_ist_t;

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_ist_t,belle_sip_server_transaction_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

belle_sip_ist_t * belle_sip_ist_new(belle_sip_provider_t *prov, belle_sip_request_t *req);
/* returns 0 if the ack should be notified to TU, or -1 otherwise*/
int belle_sip_ist_process_ack(belle_sip_ist_t *obj, belle_sip_message_t *ack);

struct belle_sip_nist{
	belle_sip_server_transaction_t base;
	belle_sip_source_t *timer_J;
};

typedef struct belle_sip_nist belle_sip_nist_t;

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_nist_t,belle_sip_server_transaction_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

belle_sip_nist_t * belle_sip_nist_new(belle_sip_provider_t *prov, belle_sip_request_t *req);


/*
 * Dialogs
 */ 
struct belle_sip_dialog{
	belle_sip_object_t base;
	void *appdata;
	belle_sip_provider_t *provider;
	belle_sip_request_t *last_out_invite;
	belle_sip_request_t *last_out_ack; /*so that it can be retransmitted when needed*/
	belle_sip_response_t *last_200Ok;
	belle_sip_source_t *timer_200Ok;
	belle_sip_source_t *timer_200Ok_end;
	belle_sip_dialog_state_t state;
	belle_sip_dialog_state_t previous_state;
	belle_sip_header_call_id_t *call_id;
	belle_sip_header_address_t *local_party;
	belle_sip_header_address_t *remote_party;
	belle_sip_list_t *route_set;
	belle_sip_header_address_t *remote_target;
	char *local_tag;
	char *remote_tag;
	unsigned int local_cseq;
	unsigned int remote_cseq;
	belle_sip_transaction_t* last_transaction;
	belle_sip_header_privacy_t* privacy;
	belle_sip_list_t *queued_ct;/* queued client transactions*/
	unsigned char is_server;
	unsigned char is_secure;
	unsigned char terminate_on_bye;
	unsigned char needs_ack;
};

belle_sip_dialog_t *belle_sip_dialog_new(belle_sip_transaction_t *t);
belle_sip_dialog_t * belle_sip_provider_create_dialog_internal(belle_sip_provider_t *prov, belle_sip_transaction_t *t,unsigned int check_last_resp);
int belle_sip_dialog_is_authorized_transaction(const belle_sip_dialog_t *dialog,const char* method) ;
/*returns 1 if message belongs to the dialog, 0 otherwise */
int _belle_sip_dialog_match(belle_sip_dialog_t *obj, const char *call_id, const char *local_tag, const char *remote_tag);
int belle_sip_dialog_match(belle_sip_dialog_t *obj, belle_sip_message_t *msg, int as_uas);
int belle_sip_dialog_update(belle_sip_dialog_t *obj,belle_sip_transaction_t* transaction, int as_uas);
void belle_sip_dialog_check_ack_sent(belle_sip_dialog_t*obj);
int belle_sip_dialog_handle_ack(belle_sip_dialog_t *obj, belle_sip_request_t *ack);
void belle_sip_dialog_queue_client_transaction(belle_sip_dialog_t *dialog, belle_sip_client_transaction_t *tr);

/*
 belle_sip_response_t
*/
belle_sip_hop_t* belle_sip_response_get_return_hop(belle_sip_response_t *msg);

#define IS_TOKEN(token) \
		(INPUT->toStringTT(INPUT,LT(1),LT(strlen(#token)))->chars ?\
		strcasecmp(#token,(const char*)(INPUT->toStringTT(INPUT,LT(1),LT(strlen(#token)))->chars)) == 0:0)
char* _belle_sip_str_dup_and_unquote_string(const char* quoted_string);

#define IS_HEADER_NAMED(name,compressed_name) (IS_TOKEN(compressed_name) || IS_TOKEN(name))

#define STRCASECMP_HEADER_NAMED(name,compressed_name,value) \
		(strcasecmp(compressed_name,(const char*)value) == 0 || strcasecmp(name,(const char*)value) == 0 )

/*********************************************************
 * SDP
 */
#define BELLE_SDP_PARSE(object_type) \
belle_sdp_##object_type##_t* belle_sdp_##object_type##_parse (const char* value) { \
	pANTLR3_INPUT_STREAM           input; \
	pbelle_sdpLexer               lex; \
	pANTLR3_COMMON_TOKEN_STREAM    tokens; \
	pbelle_sdpParser              parser; \
	belle_sdp_##object_type##_t* l_parsed_object; \
	input  = ANTLR_STREAM_NEW(object_type, value,strlen(value));\
	lex    = belle_sdpLexerNew                (input);\
	tokens = antlr3CommonTokenStreamSourceNew  (ANTLR3_SIZE_HINT, TOKENSOURCE(lex));\
	parser = belle_sdpParserNew               (tokens);\
	l_parsed_object = parser->object_type(parser).ret;\
	parser ->free(parser);\
	tokens ->free(tokens);\
	lex    ->free(lex);\
	input  ->close(input);\
	if (l_parsed_object == NULL) belle_sip_error(#object_type" parser error for [%s]",value);\
	return l_parsed_object;\
}
#define BELLE_SDP_NEW(object_type,super_type) \
		BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sdp_##object_type##_t); \
		BELLE_SIP_INSTANCIATE_VPTR(	belle_sdp_##object_type##_t\
									, super_type##_t\
									, belle_sdp_##object_type##_destroy\
									, belle_sdp_##object_type##_clone\
									, belle_sdp_##object_type##_marshal, TRUE); \
		belle_sdp_##object_type##_t* belle_sdp_##object_type##_new () { \
		belle_sdp_##object_type##_t* l_object = belle_sip_object_new(belle_sdp_##object_type##_t);\
		super_type##_init((super_type##_t*)l_object); \
		return l_object;\
	}
#define BELLE_SDP_NEW_WITH_CTR(object_type,super_type) \
		BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sdp_##object_type##_t); \
		BELLE_SIP_INSTANCIATE_VPTR(	belle_sdp_##object_type##_t\
									, super_type##_t\
									, belle_sdp_##object_type##_destroy\
									, belle_sdp_##object_type##_clone\
									, belle_sdp_##object_type##_marshal,TRUE); \
		belle_sdp_##object_type##_t* belle_sdp_##object_type##_new () { \
		belle_sdp_##object_type##_t* l_object = belle_sip_object_new(belle_sdp_##object_type##_t);\
		super_type##_init((super_type##_t*)l_object); \
		belle_sdp_##object_type##_init(l_object); \
		return l_object;\
	}



struct belle_sip_dialog_terminated_event{
	belle_sip_provider_t *source;
	belle_sip_dialog_t *dialog;
};

struct belle_sip_io_error_event{
	belle_sip_object_t *source;  /*the object impacted by this error*/
	const char *transport;
	const char *host;
	unsigned int port;
};

struct belle_sip_request_event{
	belle_sip_provider_t *source;
	belle_sip_server_transaction_t *server_transaction;
	belle_sip_dialog_t *dialog;
	belle_sip_request_t *request;
};

struct belle_sip_response_event{
	belle_sip_provider_t *source;
	belle_sip_client_transaction_t *client_transaction;
	belle_sip_dialog_t *dialog;
	belle_sip_response_t *response;
};

struct belle_sip_timeout_event{
	belle_sip_provider_t *source;
	belle_sip_transaction_t *transaction;
	int is_server_transaction;
};

struct belle_sip_transaction_terminated_event{
	belle_sip_provider_t *source;
	belle_sip_transaction_t *transaction;
	int is_server_transaction;
};

struct belle_sip_auth_event {
	belle_sip_auth_mode_t mode;
	char* username;
	char* userid;
	char* realm;
	char* passwd;
	char* ha1;
	char* domain;
	char* distinguished_name;
	belle_sip_certificates_chain_t * cert;
	belle_sip_signing_key_t* key;

};

belle_sip_auth_event_t* belle_sip_auth_event_create(const char* realm,const belle_sip_header_from_t * from);

void belle_sip_auth_event_set_distinguished_name(belle_sip_auth_event_t* event,const char* value);

/*
 * refresher
 * */
belle_sip_refresher_t* belle_sip_refresher_new(belle_sip_client_transaction_t* transaction);


/*
 * returns a char, even if entry is escaped*/
int belle_sip_get_char (const char*a,int n,char*out);
/*return an escaped string*/
BELLESIP_INTERNAL_EXPORT	char* belle_sip_uri_to_escaped_username(const char* buff) ;
BELLESIP_INTERNAL_EXPORT	char* belle_sip_uri_to_escaped_userpasswd(const char* buff) ;
BELLESIP_INTERNAL_EXPORT	char* belle_sip_uri_to_escaped_parameter(const char* buff) ;
BELLESIP_INTERNAL_EXPORT	char* belle_sip_uri_to_escaped_header(const char* buff) ;
BELLESIP_INTERNAL_EXPORT	char* belle_sip_to_unescaped_string(const char* buff) ;


#define BELLE_SIP_SOCKET_TIMEOUT 30000

#define BELLE_SIP_BRANCH_ID_LENGTH 10
/*Shall not be less than 32bit */
#define BELLE_SIP_TAG_LENGTH 6
#define BELLE_SIP_MAX_TO_STRING_SIZE 2048

void belle_sip_header_contact_set_unknown(belle_sip_header_contact_t *a, int value);
void belle_sip_request_set_dialog(belle_sip_request_t *req, belle_sip_dialog_t *dialog);
void belle_sip_request_set_rfc2543_branch(belle_sip_request_t *req, const char *rfc2543branch);
void belle_sip_dialog_update_request(belle_sip_dialog_t *dialog, belle_sip_request_t *req);

#endif
