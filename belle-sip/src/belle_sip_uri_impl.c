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

#include "belle-sip/sip-uri.h"
#include "belle-sip/parameters.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "grammars/belle_sip_messageParser.h"
#include "grammars/belle_sip_messageLexer.h"
#include "belle_sip_internal.h"
#include "listeningpoint_internal.h"


#define SIP_URI_GET_SET_STRING(attribute) GET_SET_STRING(belle_sip_uri,attribute)
#define SIP_URI_GET_SET_STRING_PARAM(attribute) GET_SET_STRING_PARAM2(belle_sip_uri,attribute,attribute##_param)


#define SIP_URI_GET_SET_UINT(attribute) GET_SET_INT(belle_sip_uri,attribute,unsigned int)
#define SIP_URI_GET_SET_INT(attribute) GET_SET_INT(belle_sip_uri,attribute,int)
#define SIP_URI_GET_SET_INT_PARAM(attribute) GET_SET_INT_PARAM2(belle_sip_uri,attribute,int,attribute##_param)


#define SIP_URI_GET_SET_BOOL(attribute) GET_SET_BOOL(belle_sip_uri,attribute,is)
#define SIP_URI_HAS_SET_BOOL(attribute) GET_SET_BOOL(belle_sip_uri,attribute,has)
#define SIP_URI_HAS_SET_BOOL_PARAM(attribute) GET_SET_BOOL_PARAM2(belle_sip_uri,attribute,has,attribute##_param)



struct _belle_sip_uri {
	belle_sip_parameters_t params;
	unsigned int secure;
	char* user;
	char* user_password;
	char* host;
	int port;
	belle_sip_parameters_t * header_list;
};

static void belle_sip_uri_destroy(belle_sip_uri_t* uri) {
	if (uri->user) belle_sip_free (uri->user);
	if (uri->host) belle_sip_free (uri->host);
	if (uri->user_password) belle_sip_free (uri->user_password);
	belle_sip_object_unref(BELLE_SIP_OBJECT(uri->header_list));
}

static void belle_sip_uri_clone(belle_sip_uri_t* uri, const belle_sip_uri_t *orig){
	uri->secure=orig->secure;
	uri->user=orig->user?belle_sip_strdup(orig->user):NULL;
	uri->user_password=orig->user_password?belle_sip_strdup(orig->user_password):NULL;
	uri->host=orig->host?belle_sip_strdup(orig->host):NULL;
	uri->port=orig->port;
	if (orig->header_list){
		uri->header_list=(belle_sip_parameters_t*)belle_sip_object_clone(BELLE_SIP_OBJECT(orig->header_list));
		belle_sip_object_ref(uri->header_list);
	}

}

static void encode_params(belle_sip_param_pair_t* container, belle_sip_list_t** newlist) {
	char *escapedName = belle_sip_uri_to_escaped_parameter(container->name);
	char *escapedValue = container->value? belle_sip_uri_to_escaped_parameter(container->value) : NULL;
	*newlist = belle_sip_list_append(*newlist, belle_sip_param_pair_new(escapedName, escapedValue));
	if (escapedName) free(escapedName);
	if (escapedValue) free(escapedValue);
}

static void encode_headers(belle_sip_param_pair_t* container, belle_sip_list_t** newlist) {
	char *escapedName = belle_sip_uri_to_escaped_header(container->name);
	char *escapedValue = container->value? belle_sip_uri_to_escaped_header(container->value) : NULL;
	*newlist = belle_sip_list_append(*newlist, belle_sip_param_pair_new(escapedName, escapedValue));
	if (escapedName) free(escapedName);
	if (escapedValue) free(escapedValue);
}

belle_sip_error_code belle_sip_uri_marshal(const belle_sip_uri_t* uri, char* buff, size_t buff_size, size_t *offset) {
	const belle_sip_list_t* list;
	belle_sip_error_code error=BELLE_SIP_OK;

	error=belle_sip_snprintf(buff,buff_size,offset,"%s:",uri->secure?"sips":"sip");
	if (error!=BELLE_SIP_OK) return error;

	if (uri->user && uri->user[0]!='\0') {
		char* escaped_username=belle_sip_uri_to_escaped_username(uri->user);
		error=belle_sip_snprintf(buff,buff_size,offset,"%s",escaped_username);
		belle_sip_free(escaped_username);
		if (error!=BELLE_SIP_OK) return error;

		if (uri->user_password) {
			char* escaped_password=belle_sip_uri_to_escaped_userpasswd(uri->user_password);
			error=belle_sip_snprintf(buff,buff_size,offset,":%s",escaped_password);
			belle_sip_free(escaped_password);
			if (error!=BELLE_SIP_OK) return error;
		}
		error=belle_sip_snprintf(buff,buff_size,offset,"@");
		if (error!=BELLE_SIP_OK) return error;

	}

	if (uri->host) {
		if (strchr(uri->host,':')) { /*ipv6*/
			error=belle_sip_snprintf(buff,buff_size,offset,"[%s]",uri->host);
		} else {
			error=belle_sip_snprintf(buff,buff_size,offset,"%s",uri->host);
		}
		if (error!=BELLE_SIP_OK) return error;
	} else {
		belle_sip_warning("no host found in this uri");
	}

	if (uri->port!=0) {
		error=belle_sip_snprintf(buff,buff_size,offset,":%i",uri->port);
		if (error!=BELLE_SIP_OK) return error;
	}

	{
		belle_sip_parameters_t *encparams = belle_sip_parameters_new();
		belle_sip_list_for_each2(uri->params.param_list, (void (*)(void *, void *))encode_params, &encparams->param_list);
		error=belle_sip_parameters_marshal(encparams,buff,buff_size,offset);
		belle_sip_object_unref(encparams);
		if (error!=BELLE_SIP_OK) return error;
	}

	{
		belle_sip_list_t * encheaders = NULL;
		belle_sip_list_for_each2(uri->header_list->param_list, (void (*)(void *, void *))encode_headers, &encheaders);

		for(list=encheaders;list!=NULL;list=list->next){
			belle_sip_param_pair_t* container = list->data;
			if (list == encheaders) {
				//first case
				error=belle_sip_snprintf(buff,buff_size,offset,"?%s=%s",container->name,container->value?container->value:"");
			} else {
				//subsequent headers
				error=belle_sip_snprintf(buff,buff_size,offset,"&%s=%s",container->name,container->value?container->value:"");
			}
			if (error!=BELLE_SIP_OK) break;
		}
		belle_sip_list_free_with_data(encheaders,(void (*)(void*))belle_sip_param_pair_destroy);
	}

	return error;
}

BELLE_SIP_PARSE(uri);

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_uri_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_uri_t,belle_sip_parameters_t,belle_sip_uri_destroy,belle_sip_uri_clone,belle_sip_uri_marshal,TRUE);


belle_sip_uri_t* belle_sip_uri_new () {
	belle_sip_uri_t* l_object = belle_sip_object_new(belle_sip_uri_t);
	belle_sip_parameters_init((belle_sip_parameters_t*)l_object); /*super*/
	l_object->header_list = belle_sip_parameters_new();
	belle_sip_object_ref(l_object->header_list);
	return l_object;
}

belle_sip_uri_t* belle_sip_uri_create (const char* username,const char* host) {
	belle_sip_uri_t* uri = belle_sip_uri_new();
	belle_sip_uri_set_user(uri,username);
	belle_sip_uri_set_host(uri,host);
	return uri;
}


char* belle_sip_uri_to_string(const belle_sip_uri_t* uri)  {
	return belle_sip_object_to_string(BELLE_SIP_OBJECT(uri));
}


const char* belle_sip_uri_get_header(const belle_sip_uri_t* uri,const char* name) {
	return belle_sip_parameters_get_parameter(uri->header_list,name);
}

void belle_sip_uri_set_header(belle_sip_uri_t* uri,const char* name,const char* value) {
	belle_sip_parameters_set_parameter(uri->header_list,name,value);
}

void belle_sip_uri_remove_header(belle_sip_uri_t *uri, const char *name){
	belle_sip_parameters_remove_parameter(uri->header_list,name);
}

const belle_sip_list_t*	belle_sip_uri_get_header_names(const belle_sip_uri_t* uri) {
	return belle_sip_parameters_get_parameter_names(uri->header_list);
}

int belle_sip_uri_get_listening_port(const belle_sip_uri_t *uri){
	int port=belle_sip_uri_get_port(uri);
	const char *transport=belle_sip_uri_get_transport_param(uri);
	if (!transport) {
		transport=belle_sip_uri_is_secure(uri)?"tls":"udp";
	}
	if (port==0)
		port=belle_sip_listening_point_get_well_known_port(transport);
	return port;
}

void belle_sip_uri_fix(belle_sip_uri_t *uri){
	/*nop, to be removed*/
}

SIP_URI_GET_SET_BOOL(secure)

SIP_URI_GET_SET_STRING(user)
SIP_URI_GET_SET_STRING(user_password)
SIP_URI_GET_SET_STRING(host)
SIP_URI_GET_SET_INT(port)

SIP_URI_GET_SET_STRING_PARAM(transport)
SIP_URI_GET_SET_STRING_PARAM(user)
SIP_URI_GET_SET_STRING_PARAM(method)
SIP_URI_GET_SET_STRING_PARAM(maddr)
SIP_URI_GET_SET_INT_PARAM(ttl)
SIP_URI_HAS_SET_BOOL_PARAM(lr)


const belle_sip_parameters_t*	belle_sip_uri_get_headers(const belle_sip_uri_t* uri) {
	return uri->header_list;
}


void belle_sip_uri_headers_clean(belle_sip_uri_t* uri) {
	belle_sip_parameters_clean(uri->header_list);
}


static int uri_strcmp(const char*a,const char*b,int case_sensitive) {
	int result = 0;
	size_t index_a=0,index_b=0;
	char char_a,char_b;
	
	if (a == NULL && b == NULL) {
		goto end;
	}
	if ((a != NULL && b == NULL) || (a == NULL && b != NULL)){
		result = 1;
		goto end;
	}

	do {
		index_a+=belle_sip_get_char(a+index_a,&char_a);
		index_b+=belle_sip_get_char(b+index_b,&char_b);
		if (!case_sensitive && char_a<0x7B && char_a>0x60) char_a-=0x20;
		if (!case_sensitive && char_b<0x7B && char_b>0x60) char_b-=0x20;
		result=(char_a!=char_b);
		if (result) break;
		if (char_a == '\0' || char_b == '\0') break;
	}while(1);
end:
	return result;
}

#define IS_EQUAL(a,b) (uri_strcmp(a,b,TRUE)==0)

#define IS_EQUAL_CASE(a,b) (uri_strcmp(a,b,FALSE)==0)
#define PARAM_CASE_CMP(uri_a,uri_b,param) \
		a_param=belle_sip_parameters_get_case_parameter((belle_sip_parameters_t*) uri_a,param); \
		b_param=belle_sip_parameters_get_case_parameter((belle_sip_parameters_t*) uri_b,param);\
		if (!IS_EQUAL_CASE(a_param,b_param)) return 0;

/*
 * RFC 3261            SIP: Session Initiation Protocol           June 2002
 * 19.1.4 URI Comparison

   Some operations in this specification require determining whether two
   SIP or SIPS URIs are equivalent.  In this specification, registrars
   need to compare bindings in Contact URIs in REGISTER requests (see
   Section 10.3.).  SIP and SIPS URIs are compared for equality
   according to the following rules:
*/
int belle_sip_uri_equals(const belle_sip_uri_t* uri_a,const belle_sip_uri_t* uri_b) {
	const belle_sip_list_t *	params;
	const char* b_param;
	const char* a_param;
/*
      o  A SIP and SIPS URI are never equivalent.
*/
	if (belle_sip_uri_is_secure(uri_a)!=belle_sip_uri_is_secure(uri_b)) {
		return 0;
	}
/*
	o  Comparison of the userinfo of SIP and SIPS URIs is case-
         sensitive.  This includes userinfo containing passwords or
         formatted as telephone-subscribers.  Comparison of all other
         components of the URI is case-insensitive unless explicitly
         defined otherwise.
*/
	if (!IS_EQUAL(uri_a->user,uri_b->user)) return 0;
	
/*
      o  The ordering of parameters and header fields is not significant
         in comparing SIP and SIPS URIs.

      o  Characters other than those in the "reserved" set (see RFC 2396
         [5]) are equivalent to their ""%" HEX HEX" encoding.

      o  An IP address that is the result of a DNS lookup of a host name
         does not match that host name.

      o  For two URIs to be equal, the user, password, host, and port
         components must match.
*/
	if (!IS_EQUAL_CASE(uri_a->host,uri_b->host)) {
		return 0;
	}
	if (uri_a->port != uri_b->port) return 0;
/*
         A URI omitting the user component will not match a URI that
         includes one.  A URI omitting the password component will not
         match a URI that includes one.

         A URI omitting any component with a default value will not
         match a URI explicitly containing that component with its
         default value.  For instance, a URI omitting the optional port
         component will not match a URI explicitly declaring port 5060.
         The same is true for the transport-parameter, ttl-parameter,
         user-parameter, and method components.

            Defining sip:user@host to not be equivalent to
            sip:user@host:5060 is a change from RFC 2543.  When deriving
            addresses from URIs, equivalent addresses are expected from
            equivalent URIs.  The URI sip:user@host:5060 will always
            resolve to port 5060.  The URI sip:user@host may resolve to
            other ports through the DNS SRV mechanisms detailed in [4].

      o  URI uri-parameter components are compared as follows:

         -  Any uri-parameter appearing in both URIs must match.
*/
/*
 *         -  A user, ttl, or method uri-parameter appearing in only one
            URI never matches, even if it contains the default value.
           -  A URI that includes an maddr parameter will not match a URI
            that contains no maddr parameter.
 * */
	PARAM_CASE_CMP(uri_a,uri_b,"transport")
	PARAM_CASE_CMP(uri_a,uri_b,"user")
	PARAM_CASE_CMP(uri_a,uri_b,"ttl")
	PARAM_CASE_CMP(uri_a,uri_b,"method")
	PARAM_CASE_CMP(uri_a,uri_b,"maddr")


	for(params=belle_sip_parameters_get_parameters((belle_sip_parameters_t*) uri_a);params!=NULL;params=params->next) {
		if ((b_param=belle_sip_parameters_get_parameter((belle_sip_parameters_t*) uri_b,(const char*)params->data)) != NULL) {
			if (!IS_EQUAL_CASE(b_param,(const char*)params->data)) return 0;
		}
	}

 /*


         -  All other uri-parameters appearing in only one URI are
            ignored when comparing the URIs.
*/
/* *fixme ignored for now*/
/*
      o  URI header components are never ignored.  Any present header
         component MUST be present in both URIs and match for the URIs
         to match.  The matching rules are defined for each header field
         in Section 20.
 */
	return 1;
}

/*uri checker*/


/*
 * From 19.1.1 SIP and SIPS URI Components
 * 									   				dialog
										reg./redir. Contact/
			default  Req.-URI  To  From  Contact   R-R/Route  external
user          --          o      o    o       o          o         o
password      --          o      o    o       o          o         o
host          --          m      m    m       m          m         m
port          (1)         o      -    -       o          o         o
user-param    ip          o      o    o       o          o         o
method        INVITE      -      -    -       -          -         o
maddr-param   --          o      -    -       o          o         o
ttl-param     1           o      -    -       o          -         o
transp.-param (2)         o      -    -       o          o         o
lr-param      --          o      -    -       -          o         o
other-param   --          o      o    o       o          o         o
headers       --          -      -    -       o          -         o

(1): The default port value is transport and scheme dependent.  The
default  is  5060  for  sip: using UDP, TCP, or SCTP.  The default is
5061 for sip: using TLS over TCP and sips: over TCP.

(2): The default transport is scheme dependent.  For sip:, it is UDP.
For sips:, it is TCP.

Table 1: Use and default values of URI components for SIP header
field values, Request-URI and references*/


typedef enum {
	m /*mandotory*/
	, o /*optionnal*/
	, na /*not allowd*/
} mark;

static const char* mark_to_string(mark value) {
	switch (value) {
	case o: return "optionnal";
	case m: return "mandatory";
	case na: return "not allowed";
	}
	return "unknown";
}

typedef struct uri_components {
	const char * name;
	mark user;
	mark password;
	mark host;
	mark port;
	mark user_param;
	mark method;
	mark maddr_param;
	mark ttl_param;
	mark transp_param;
	mark lr_param;
	mark other_param;
	mark headers;
} uri_components_t;


/*belle sip allows contact header without host because stack will auutomatically put host if missing*/
static  uri_components_t uri_component_use_for_request = 			{"Req.-URI"					,o	,o	,m	,o	,o	,na	,o	,o	,o	,o	,o	,na};
static  uri_components_t uri_component_use_for_header_to = 			{"Header To"				,o	,o	,m	,na	,o	,na	,na	,na	,na	,na	,o	,na};
static  uri_components_t uri_component_use_for_header_from = 		{"Header From"				,o	,o	,m	,na	,o	,na	,na	,na	,na	,na	,o	,na};
static  uri_components_t uri_component_use_for_contact_in_reg =		{"Contact in REG"			,o	,o	,/*m*/o	,o	,o	,na	,o	,o	,o	,na	,o	,o};
static  uri_components_t uri_component_use_for_dialog_ct_rr_ro =	{"Dialog Contact/R-R/Route"	,o	,o	,/*m*/o	,o	,o	,na	,o	,na	,o	,o	,o	,na};
static  uri_components_t uri_component_use_for_external =			{"External"					,o	,o	,m	,o	,o	,o	,o	,o	,o	,o	,o	,o};


static int check_component(int is_present,mark requirement) {
	switch (requirement) {
	case o: return TRUE;
	case m: return is_present;
	case na: return !is_present;
	}
	return 0;
}

#define CHECK_URI_COMPONENT(uri_component,uri_component_name,component_use_rule,component_use_rule_name) \
if (!check_component(uri_component,component_use_rule)) {\
	belle_sip_error("Uri component [%s] does not follow reqs [%s] for context [%s]", uri_component_name,mark_to_string(component_use_rule),component_use_rule_name);\
	return FALSE;\
}

static int check_uri_components(const belle_sip_uri_t* uri,  const uri_components_t* components_use) {

	CHECK_URI_COMPONENT(uri->user!=NULL,"user",components_use->user,components_use->name)
	CHECK_URI_COMPONENT(uri->host!=NULL,"host",components_use->host,components_use->name)
	CHECK_URI_COMPONENT(uri->port>0,"port",components_use->port,components_use->name)
	CHECK_URI_COMPONENT(belle_sip_parameters_has_parameter(&uri->params,"maddr"),"maddr-param",components_use->maddr_param,components_use->name)
	CHECK_URI_COMPONENT(belle_sip_parameters_has_parameter(&uri->params,"ttl"),"ttl-param",components_use->ttl_param,components_use->name)
	CHECK_URI_COMPONENT(belle_sip_parameters_has_parameter(&uri->params,"transport"),"transp.-param",components_use->transp_param,components_use->name)
	CHECK_URI_COMPONENT(belle_sip_parameters_has_parameter(&uri->params,"lr"),"lr-param",components_use->lr_param,components_use->name)
	/*..*/
	CHECK_URI_COMPONENT(belle_sip_list_size(belle_sip_parameters_get_parameters(uri->header_list))>0,"headers",components_use->headers,components_use->name)
	return TRUE;
}

/*return 0 if not compliant*/
int belle_sip_uri_check_components_from_request_uri(const belle_sip_uri_t* uri) {
	return check_uri_components(uri,&uri_component_use_for_request);
}
int belle_sip_uri_check_components_from_context(const belle_sip_uri_t* uri,const char* method,const char* header_name) {

	if (strcasecmp(BELLE_SIP_FROM,header_name)==0)
		return check_uri_components(uri,&uri_component_use_for_header_from);
	else if (strcasecmp(BELLE_SIP_TO,header_name)==0)
		return check_uri_components(uri,&uri_component_use_for_header_to);
	else if (strcasecmp(BELLE_SIP_CONTACT,header_name)==0 && method && strcasecmp("REGISTER",method)==0)
		return check_uri_components(uri,&uri_component_use_for_contact_in_reg);
	else if (strcasecmp(BELLE_SIP_CONTACT,header_name)==0
				|| strcasecmp(BELLE_SIP_RECORD_ROUTE,header_name)==0
				|| strcasecmp(BELLE_SIP_ROUTE,header_name)==0)
		return check_uri_components(uri,&uri_component_use_for_dialog_ct_rr_ro);
	else
		return check_uri_components(uri,&uri_component_use_for_external);


}
