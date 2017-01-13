/*
	belle-sip - SIP (RFC3261) library.
	Copyright (C) 2010  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
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
#include "bctoolbox/map.h"

#include "port.h"
#include <bctoolbox/port.h>

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

#define BELLE_SIP_INVOKE_LISTENER_ARG(listener,interface_name,method,arg) \
	((BELLE_SIP_INTERFACE_GET_METHODS((listener),interface_name)->method!=NULL)  ? \
		BELLE_SIP_INTERFACE_GET_METHODS((listener),interface_name)->method(listener,(arg)) : 0 )

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

#define belle_sip_object_init(obj)		/*nothing*/


/*list of all vptrs (classes) used in belle-sip*/
BELLE_SIP_DECLARE_VPTR(belle_sip_stack_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_datagram_listening_point_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_provider_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_main_loop_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_source_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_dialog_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_address_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_contact_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_from_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_to_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_via_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_diversion_t);
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
BELLE_SIP_DECLARE_VPTR(belle_sdp_raw_attribute_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_repeate_time_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_rtcp_fb_attribute_t);
BELLE_SIP_DECLARE_VPTR(belle_sdp_rtcp_xr_attribute_t);
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
BELLE_SIP_DECLARE_VPTR(belle_sip_dns_srv_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_dict_t)
BELLE_SIP_DECLARE_VPTR(belle_http_provider_t);
BELLE_SIP_DECLARE_VPTR(belle_http_channel_context_t);
BELLE_SIP_DECLARE_VPTR(belle_http_request_t);
BELLE_SIP_DECLARE_VPTR(belle_http_response_t);
BELLE_SIP_DECLARE_VPTR(belle_generic_uri_t);
BELLE_SIP_DECLARE_VPTR(belle_http_callbacks_t);
BELLE_SIP_DECLARE_VPTR(belle_tls_crypto_config_t);
BELLE_SIP_DECLARE_VPTR(belle_http_header_authorization_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_event_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_supported_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_content_disposition_t);
BELLE_SIP_DECLARE_VPTR(belle_sip_header_accept_t);

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_resolver_context_t,belle_sip_source_t)
	void (*cancel)(belle_sip_resolver_context_t *);
	void (*notify)(belle_sip_resolver_context_t *);
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_simple_resolver_context_t,belle_sip_resolver_context_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_combined_resolver_context_t,belle_sip_resolver_context_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_dual_resolver_context_t,belle_sip_resolver_context_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

void belle_sip_resolver_context_notify(belle_sip_resolver_context_t *ctx);

struct belle_sip_source{
	belle_sip_object_t base;
	belle_sip_list_t node;
	unsigned long id;
	belle_sip_fd_t fd;
	unsigned short events,revents;
#ifdef _WIN32
	long armed_events;
	unsigned short pad;
#endif
	int timeout;
	void *data;
	uint64_t expire_ms;
	int index; /* index in pollfd table */
	belle_sip_source_func_t notify;
	belle_sip_source_remove_callback_t on_remove;
	belle_sip_socket_t sock;
	unsigned char cancelled;
	unsigned char expired;
	unsigned char oneshot;
	unsigned char notify_required; /*for testing purpose, use to ask for being scheduled*/
	bctbx_iterator_t *it; /*for fast removal*/
	belle_sip_main_loop_t *ml; 
};

void belle_sip_socket_source_init(belle_sip_source_t *s, belle_sip_source_func_t func, void *data, belle_sip_socket_t fd, unsigned int events, unsigned int timeout_value_ms);
void belle_sip_fd_source_init(belle_sip_source_t *s, belle_sip_source_func_t func, void *data, belle_sip_fd_t fd, unsigned int events, unsigned int timeout_value_ms);
belle_sip_source_t * belle_sip_fd_source_new(belle_sip_source_func_t func, void *data, belle_sip_fd_t fd, unsigned int events, unsigned int timeout_value_ms);
void belle_sip_source_uninit(belle_sip_source_t *s);
void belle_sip_source_set_notify(belle_sip_source_t *s, belle_sip_source_func_t func);



/* include private headers */
#include "channel.h"




#define belle_sip_new(type) (type*)belle_sip_malloc(sizeof(type))
#define belle_sip_new0(type) (type*)belle_sip_malloc0(sizeof(type))


#define belle_sip_list_next(elem) ((elem)->next)


/* dictionary */
struct belle_sip_dict {
	belle_sip_object_t base;
};


#undef MIN
#define MIN(a,b)	((a)>(b) ? (b) : (a))
#undef MAX
#define MAX(a,b)	((a)>(b) ? (a) : (b))


#define belle_sip_concat bctbx_concat


/*parameters accessors*/
#define GET_SET_STRING(object_type,attribute) \
	const char* object_type##_get_##attribute (const object_type##_t* obj) {\
		return obj->attribute;\
	}\
	void object_type##_set_##attribute (object_type##_t* obj,const char* value) {\
		const char* previous_value = obj->attribute;  /*preserve if same value re-asigned*/ \
		if (value) {\
			obj->attribute=belle_sip_strdup(value); \
		} else obj->attribute=NULL;\
		if (previous_value != NULL) belle_sip_free((void*)previous_value);\
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

#ifdef HAVE_ANTLR_STRING_STREAM_NEW
#define ANTLR_STREAM_NEW(object_type, value,length) \
antlr3StringStreamNew((pANTLR3_UINT8)value,ANTLR3_ENC_8BIT,(ANTLR3_UINT32)length,(pANTLR3_UINT8)#object_type)
#else
#define ANTLR_STREAM_NEW(object_type, value, length) \
antlr3NewAsciiStringCopyStream((pANTLR3_UINT8)value,(ANTLR3_UINT32)length,NULL)
#endif /*HAVE_ANTLR_STRING_STREAM_NEW*/


#define BELLE_PARSE(parser_name, object_type_prefix, object_type) \
	object_type_prefix##object_type##_t* object_type_prefix##object_type##_parse (const char* value) { \
	pANTLR3_INPUT_STREAM           input; \
	pbelle_sip_messageLexer               lex; \
	pANTLR3_COMMON_TOKEN_STREAM    tokens; \
	p##parser_name              parser; \
	object_type_prefix##object_type##_t* l_parsed_object; \
	input  = ANTLR_STREAM_NEW(object_type,value,strlen(value));\
	lex    = belle_sip_messageLexerNew                (input);\
	tokens = antlr3CommonTokenStreamSourceNew  (ANTLR3_SIZE_HINT, TOKENSOURCE(lex));\
	parser = parser_name##New               (tokens);\
	l_parsed_object = parser->object_type(parser);\
	parser ->free(parser);\
	tokens ->free(tokens);\
	lex    ->free(lex);\
	input  ->close(input);\
	if (l_parsed_object == NULL) belle_sip_error(#object_type" parser error for [%s]",value);\
	return l_parsed_object;\
}
#define BELLE_SIP_PARSE(object_type) BELLE_PARSE(belle_sip_messageParser,belle_sip_,object_type)

#define BELLE_NEW(object_type,super_type) \
	BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(object_type##_t); \
	BELLE_SIP_INSTANCIATE_VPTR(	object_type##_t\
								, super_type##_t\
								, object_type##_destroy\
								, object_type##_clone\
								, object_type##_marshal, TRUE); \
		object_type##_t* object_type##_new () { \
		object_type##_t* l_object = belle_sip_object_new(object_type##_t);\
		super_type##_init((super_type##_t*)l_object); \
		object_type##_init((object_type##_t*) l_object); \
		return l_object;\
	}

#define BELLE_SIP_NEW(object_type,super_type) BELLE_NEW (belle_sip_##object_type, belle_sip_##super_type)

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
struct belle_sip_param_pair {
	int ref;
	char* name;
	char* value;
} ;

void belle_sip_param_pair_destroy(belle_sip_param_pair_t*  pair) ;

int belle_sip_param_pair_comp_func(const belle_sip_param_pair_t *a, const char*b) ;
int belle_sip_param_pair_case_comp_func(const belle_sip_param_pair_t *a, const char*b) ;

belle_sip_param_pair_t* belle_sip_param_pair_ref(belle_sip_param_pair_t* obj);

void belle_sip_param_pair_unref(belle_sip_param_pair_t* obj);




/*calss header*/
struct _belle_sip_header {
	belle_sip_object_t base;
	belle_sip_header_t* next;
	char *name;
	char *unparsed_value;
};


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
	char *dns_resolv_conf; /*used to load custom resolv.conf, for tests*/
	belle_sip_list_t *dns_servers; /*used when dns servers are supplied by app layer*/
	/*http proxy stuff to be used by both http and sip provider*/
	char *http_proxy_host;
	int http_proxy_port;
	char *http_proxy_username; /*for future use*/
	char *http_proxy_passwd; /*for future use*/

	unsigned char dns_srv_enabled;
	unsigned char dns_search_enabled;
};

BELLESIP_INTERNAL_EXPORT belle_sip_hop_t* belle_sip_hop_new(const char* transport, const char *cname, const char* host,int port);
BELLESIP_INTERNAL_EXPORT belle_sip_hop_t* belle_sip_hop_new_from_uri(const belle_sip_uri_t *uri);
BELLESIP_INTERNAL_EXPORT belle_sip_hop_t* belle_sip_hop_new_from_generic_uri(const belle_generic_uri_t *uri);

BELLESIP_INTERNAL_EXPORT belle_sip_hop_t * belle_sip_stack_get_next_hop(belle_sip_stack_t *stack, belle_sip_request_t *req);

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
	unsigned short unconditional_answer;
};

BELLESIP_INTERNAL_EXPORT belle_sip_provider_t *belle_sip_provider_new(belle_sip_stack_t *s, belle_sip_listening_point_t *lp);
void belle_sip_provider_add_client_transaction(belle_sip_provider_t *prov, belle_sip_client_transaction_t *t);
belle_sip_client_transaction_t *belle_sip_provider_find_matching_client_transaction(belle_sip_provider_t *prov, belle_sip_response_t *resp);
void belle_sip_provider_remove_client_transaction(belle_sip_provider_t *prov, belle_sip_client_transaction_t *t);
void belle_sip_provider_add_server_transaction(belle_sip_provider_t *prov, belle_sip_server_transaction_t *t);
BELLESIP_INTERNAL_EXPORT belle_sip_server_transaction_t * belle_sip_provider_find_matching_server_transaction(belle_sip_provider_t *prov,belle_sip_request_t *req);
void belle_sip_provider_remove_server_transaction(belle_sip_provider_t *prov, belle_sip_server_transaction_t *t);
void belle_sip_provider_set_transaction_terminated(belle_sip_provider_t *p, belle_sip_transaction_t *t);
void *belle_sip_transaction_get_application_data_internal(const belle_sip_transaction_t *t);
belle_sip_channel_t * belle_sip_provider_get_channel(belle_sip_provider_t *p, const belle_sip_hop_t *hop);
void belle_sip_provider_add_dialog(belle_sip_provider_t *prov, belle_sip_dialog_t *dialog);
void belle_sip_provider_remove_dialog(belle_sip_provider_t *prov, belle_sip_dialog_t *dialog);
void belle_sip_provider_release_channel(belle_sip_provider_t *p, belle_sip_channel_t *chan);
void belle_sip_provider_add_internal_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l, int prepend);
void belle_sip_provider_remove_internal_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l);
belle_sip_client_transaction_t * belle_sip_provider_find_matching_client_transaction_from_req(belle_sip_provider_t *prov, belle_sip_request_t *req);
belle_sip_dialog_t *belle_sip_provider_find_dialog_from_message(belle_sip_provider_t *prov, belle_sip_message_t *msg, int as_uas);
/*for testing purpose only:*/
BELLESIP_INTERNAL_EXPORT void belle_sip_provider_dispatch_message(belle_sip_provider_t *prov, belle_sip_message_t *msg);

typedef struct listener_ctx{
	belle_sip_listener_t *listener;
	void *data;
}listener_ctx_t;

#define BELLE_SIP_PROVIDER_INVOKE_LISTENERS_FOR_TRANSACTION(t,callback,event) \
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS((t)->is_internal?(t)->provider->internal_listeners:(t)->provider->listeners,callback,event)

#define BELLE_SIP_PROVIDER_INVOKE_LISTENERS_FOR_DIALOG(d,callback,event) \
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS((d)->is_internal?(d)->provider->internal_listeners:(d)->provider->listeners,callback,event)

#define BELLE_SIP_PROVIDER_INVOKE_LISTENERS(listeners,callback,event) \
	BELLE_SIP_INVOKE_LISTENERS_ARG((listeners),belle_sip_listener_t,callback,(event))

/*
 * http provider
 */
belle_http_provider_t *belle_http_provider_new(belle_sip_stack_t *s, const char *bind_ip);


/*
 * SIP and http messages
**/
#define BELLESIP_MULTIPART_BOUNDARY "---------------------------14737809831466499882746641449"

void belle_sip_message_init(belle_sip_message_t *message);

struct _belle_sip_message {
	belle_sip_object_t base;
	belle_sip_list_t* header_list;
	belle_sip_body_handler_t *body_handler;
};

struct _belle_sip_request {
	belle_sip_message_t base;
	char* method;
	belle_sip_uri_t* uri;
	belle_generic_uri_t *absolute_uri;
	belle_sip_dialog_t *dialog;/*set if request was created by a dialog to avoid to search in dialog list*/
	char *rfc2543_branch; /*computed 'branch' id in case we receive this request from an old RFC2543 stack*/
	unsigned char dialog_queued;
};


/** HTTP request**/

struct belle_http_request{
	belle_sip_message_t base;
	belle_generic_uri_t *req_uri;
	char* method;
	belle_http_request_listener_t *listener;
	belle_generic_uri_t *orig_uri;/*original uri before removing host and user/passwd*/
	belle_http_response_t *response;
	belle_sip_channel_t *channel;
	int auth_attempt_count;
	int background_task_id;
	int cancelled;
};

void belle_http_request_set_listener(belle_http_request_t *req, belle_http_request_listener_t *l);
void belle_http_request_set_channel(belle_http_request_t *req, belle_sip_channel_t *chan);
void belle_http_request_set_response(belle_http_request_t *req, belle_http_response_t *resp);
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
	belle_sip_source_t *call_repair_timer;
	char *branch_id;
	belle_sip_transaction_state_t state;
	void *appdata;
	unsigned char is_internal;
	unsigned char timed_out;
	unsigned char sent_by_dialog_queue;
	unsigned long bg_task_id;
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

int belle_sip_client_transaction_is_notify_matching_pending_subscribe(belle_sip_client_transaction_t *trans
																	  , belle_sip_request_t *notify);

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


typedef enum belle_sip_dialog_type{
	BELLE_SIP_DIALOG_INVITE,
	BELLE_SIP_DIALOG_SUBSCRIBE_NOTIFY
}belle_sip_dialog_type_t;
/*
 * Dialogs
 */
struct belle_sip_dialog{
	belle_sip_object_t base;
	void *appdata;
	belle_sip_dialog_type_t type;
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
	belle_sip_source_t *expiration_timer;
	char *local_tag;
	char *remote_tag;
	unsigned int local_cseq;
	unsigned int remote_cseq;
	belle_sip_transaction_t* last_transaction;
	belle_sip_header_privacy_t* privacy;
	belle_sip_list_t *queued_ct;/* queued client transactions*/
	unsigned int remote_invite_cseq; /*needed because multiple trans can be handled whithin invite transaction (I.E UPDATE, PRACK,etc*/
	unsigned char is_server;
	unsigned char is_secure;
	unsigned char terminate_on_bye;
	unsigned char needs_ack;
	unsigned char is_expired;
	unsigned char pending_trans_checking_enabled; /*use to disabled pending transaction check at request creation (testing)*/
	unsigned char is_internal; /*Internal dialogs are those created by refreshers. */
};

belle_sip_dialog_t *belle_sip_dialog_new(belle_sip_transaction_t *t);
belle_sip_dialog_t * belle_sip_provider_create_dialog_internal(belle_sip_provider_t *prov, belle_sip_transaction_t *t,unsigned int check_last_resp);
int belle_sip_dialog_is_authorized_transaction(const belle_sip_dialog_t *dialog,const char* method) ;
/*returns 1 if message belongs to the dialog, 0 otherwise */
int belle_sip_dialog_is_null_dialog_with_matching_subscribe(belle_sip_dialog_t *obj, const char *call_id, const char *local_tag, belle_sip_request_t *notify);
int _belle_sip_dialog_match(belle_sip_dialog_t *obj, const char *call_id, const char *local_tag, const char *remote_tag);
int belle_sip_dialog_match(belle_sip_dialog_t *obj, belle_sip_message_t *msg, int as_uas);
int belle_sip_dialog_update(belle_sip_dialog_t *obj,belle_sip_transaction_t* transaction, int as_uas);
void belle_sip_dialog_check_ack_sent(belle_sip_dialog_t*obj);
int belle_sip_dialog_handle_ack(belle_sip_dialog_t *obj, belle_sip_request_t *ack);
void belle_sip_dialog_queue_client_transaction(belle_sip_dialog_t *dialog, belle_sip_client_transaction_t *tr);
void belle_sip_dialog_stop_200Ok_retrans(belle_sip_dialog_t *obj);


/*
 belle_sip_response_t
*/
belle_sip_hop_t* belle_sip_response_get_return_hop(belle_sip_response_t *msg);


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
	int is_expired;
};

struct belle_sip_io_error_event{
	belle_sip_object_t *source;  /*the object impacted by this error*/
	const char *transport;
	const char *host;
	unsigned int port;
};

struct belle_sip_request_event{
	belle_sip_object_t *source;
	belle_sip_server_transaction_t *server_transaction;
	belle_sip_dialog_t *dialog;
	belle_sip_request_t *request;
};

struct belle_sip_response_event{
	belle_sip_object_t *source;
	belle_sip_client_transaction_t *client_transaction;
	belle_sip_dialog_t *dialog;
	belle_sip_response_t *response;
};

struct belle_sip_timeout_event{
	belle_sip_object_t *source;
	belle_sip_transaction_t *transaction;
	int is_server_transaction;
};

struct belle_sip_transaction_terminated_event{
	belle_sip_provider_t *source;
	belle_sip_transaction_t *transaction;
	int is_server_transaction;
};

struct belle_sip_auth_event {
	belle_sip_object_t *source;
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

belle_sip_auth_event_t* belle_sip_auth_event_create(belle_sip_object_t *source, const char* realm,const belle_sip_uri_t * from_uri);

void belle_sip_auth_event_set_distinguished_name(belle_sip_auth_event_t* event,const char* value);

/*
 * refresher
 * */
belle_sip_refresher_t* belle_sip_refresher_new(belle_sip_client_transaction_t* transaction);


/*
 * returns a char, even if entry is escaped*/
size_t belle_sip_get_char (const char*a,char*out);
/*return an escaped string*/
BELLESIP_INTERNAL_EXPORT	char* belle_sip_uri_to_escaped_username(const char* buff) ;
BELLESIP_INTERNAL_EXPORT	char* belle_sip_uri_to_escaped_userpasswd(const char* buff) ;
BELLESIP_INTERNAL_EXPORT	char* belle_sip_uri_to_escaped_parameter(const char* buff) ;
BELLESIP_INTERNAL_EXPORT	char* belle_sip_uri_to_escaped_header(const char* buff) ;


/*(uri RFC 2396)*/

BELLESIP_INTERNAL_EXPORT char* belle_generic_uri_to_escaped_query(const char* buff);
BELLESIP_INTERNAL_EXPORT char* belle_generic_uri_to_escaped_path(const char* buff);

#define BELLE_SIP_SOCKET_TIMEOUT 30000

#define BELLE_SIP_BRANCH_ID_LENGTH 10
/*Shall not be less than 32bit */
#define BELLE_SIP_TAG_LENGTH 6
#define BELLE_SIP_MAX_TO_STRING_SIZE 2048

void belle_sip_header_contact_set_unknown(belle_sip_header_contact_t *a, int value);
void belle_sip_request_set_dialog(belle_sip_request_t *req, belle_sip_dialog_t *dialog);
void belle_sip_request_set_rfc2543_branch(belle_sip_request_t *req, const char *rfc2543branch);
void belle_sip_dialog_update_request(belle_sip_dialog_t *dialog, belle_sip_request_t *req);

belle_sip_error_code belle_sip_headers_marshal(belle_sip_message_t *message, char* buff, size_t buff_size, size_t *offset);

#define SET_OBJECT_PROPERTY(obj,property_name,new_value) \
	if (new_value) belle_sip_object_ref(new_value); \
	if (obj->property_name){ \
		belle_sip_object_unref(obj->property_name); \
	}\
	obj->property_name=new_value;

#include "parserutils.h"

/******************************
 *
 * private Extension header inherit from header
 *
 ******************************/
typedef struct _belle_sip_header_extension belle_sip_header_extension_t;

belle_sip_header_extension_t* belle_sip_header_extension_new(void);

belle_sip_header_extension_t* belle_sip_header_extension_parse (const char* extension) ;
belle_sip_header_extension_t* belle_sip_header_extension_create (const char* name,const char* value);
BELLESIP_INTERNAL_EXPORT const char* belle_sip_header_extension_get_value(const belle_sip_header_extension_t* extension);
void belle_sip_header_extension_set_value(belle_sip_header_extension_t* extension,const char* value);
#define BELLE_SIP_HEADER_EXTENSION(t) BELLE_SIP_CAST(t,belle_sip_header_extension_t)


/****************
 * belle_sip_body_handler_t object
 ***************/


BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_body_handler_t,belle_sip_object_t)
	void (*begin_recv_transfer)(belle_sip_body_handler_t *obj);
	void (*begin_send_transfer)(belle_sip_body_handler_t *obj);
	void (*end_transfer)(belle_sip_body_handler_t *obj);
	void (*chunk_recv)(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, off_t offset, uint8_t *buf, size_t size);
	int (*chunk_send)(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, off_t offset, uint8_t *buf, size_t * size);
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

void belle_sip_body_handler_begin_recv_transfer(belle_sip_body_handler_t *obj);
void belle_sip_body_handler_begin_send_transfer(belle_sip_body_handler_t *obj);
void belle_sip_body_handler_recv_chunk(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, uint8_t *buf, size_t size);
int belle_sip_body_handler_send_chunk(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, uint8_t *buf, size_t *size);
void belle_sip_body_handler_end_transfer(belle_sip_body_handler_t *obj);


BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_memory_body_handler_t,belle_sip_body_handler_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_user_body_handler_t,belle_sip_body_handler_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_file_body_handler_t,belle_sip_body_handler_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_multipart_body_handler_t,belle_sip_body_handler_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

void belle_sip_multipart_body_handler_progress_cb(belle_sip_body_handler_t *obj, belle_sip_message_t *msg, void *user_data, size_t transfered, size_t expected_total);

/**
 * file manipulation
 */
/**
 * Parse a directory and return all files in it.
 *
 * @param[in]	path		The directory to be parsed
 * @param[in]	file_type	if not NULL return only the file with the given extension, must include the '.', ex:".pem"
 * @return		a belle_sip list containing all found file or NULL if no file were found or directory doesn't exist. List must be destroyed using belle_sip_list_free_with_data(<ret_list>, belle_sip_free)
 */
belle_sip_list_t *belle_sip_parse_directory(const char *path, const char *file_type);

#endif
