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

#include "belle-sip/uri.h"
#include "belle-sip/parameters.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "belle_sip_messageParser.h"
#include "belle_sip_messageLexer.h"
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
	char* host;
	unsigned int port;
	belle_sip_parameters_t * header_list;
};

static void belle_sip_uri_destroy(belle_sip_uri_t* uri) {
	if (uri->user) belle_sip_free (uri->user);
	if (uri->host) belle_sip_free (uri->host);
	belle_sip_object_unref(BELLE_SIP_OBJECT(uri->header_list));
}

static void belle_sip_uri_clone(belle_sip_uri_t* uri, const belle_sip_uri_t *orig){
	uri->secure=orig->secure;
	uri->user=orig->user?belle_sip_strdup(orig->user):NULL;
	uri->host=orig->host?belle_sip_strdup(orig->host):NULL;
	uri->port=orig->port;
	uri->header_list=orig->header_list?(belle_sip_parameters_t*)belle_sip_object_clone(BELLE_SIP_OBJECT(orig->header_list)):NULL;
}

int belle_sip_uri_marshal(const belle_sip_uri_t* uri, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	const belle_sip_list_t* list=belle_sip_parameters_get_parameters(uri->header_list);

	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s:",uri->secure?"sips":"sip");
	if (uri->user) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s@",uri->user);
	}
	if (uri->host) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",uri->host);
	} else {
		belle_sip_warning("no host found in this uri");
	}
	if (uri->port>0) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,":%i",uri->port);
	}
	current_offset+=belle_sip_parameters_marshal(&uri->params,buff,current_offset,buff_size);

	for(;list!=NULL;list=list->next){
		belle_sip_param_pair_t* container = list->data;
		if (list == belle_sip_parameters_get_parameters(uri->header_list)) {
			//first case
			current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"?%s=%s",container->name,container->value);
		} else {
			//subsequent headers
			current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"&%s=%s",container->name,container->value);
		}
	}
	return current_offset-offset;
}
BELLE_SIP_PARSE(uri);

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_uri_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_uri_t,belle_sip_parameters_t,belle_sip_uri_destroy,belle_sip_uri_clone,belle_sip_uri_marshal,TRUE);


belle_sip_uri_t* belle_sip_uri_new () {
	belle_sip_uri_t* l_object = (belle_sip_uri_t*)belle_sip_object_new(belle_sip_uri_t);
	belle_sip_parameters_init((belle_sip_parameters_t*)l_object); /*super*/
	l_object->header_list = belle_sip_parameters_new();
	return l_object;
}
belle_sip_uri_t* belle_sip_uri_create (const char* username,const char* host) {
	belle_sip_uri_t* uri = belle_sip_uri_new();
	belle_sip_uri_set_user(uri,username);
	belle_sip_uri_set_host(uri,host);
	return uri;
}


char*	belle_sip_uri_to_string(belle_sip_uri_t* uri)  {
	return belle_sip_object_to_string(BELLE_SIP_OBJECT(uri));
}


const char*	belle_sip_uri_get_header(const belle_sip_uri_t* uri,const char* name) {
	return belle_sip_parameters_get_parameter(uri->header_list,name);
}
void	belle_sip_uri_set_header(belle_sip_uri_t* uri,const char* name,const char* value) {
	belle_sip_parameters_set_parameter(uri->header_list,name,value);
}

const belle_sip_list_t*	belle_sip_uri_get_header_names(const belle_sip_uri_t* uri) {
	return belle_sip_parameters_get_parameter_names(uri->header_list);
}

int belle_sip_uri_get_listening_port(const belle_sip_uri_t *uri){
	int port=belle_sip_uri_get_port(uri);
	const char *transport=belle_sip_uri_get_transport_param(uri);
	if (port==0)
		port=belle_sip_listening_point_get_well_known_port(transport ? transport : "UDP");
	return port;
}

SIP_URI_GET_SET_BOOL(secure)

SIP_URI_GET_SET_STRING(user)
SIP_URI_GET_SET_STRING(host)
SIP_URI_GET_SET_UINT(port)

SIP_URI_GET_SET_STRING_PARAM(transport)
SIP_URI_GET_SET_STRING_PARAM(user)
SIP_URI_GET_SET_STRING_PARAM(method)
SIP_URI_GET_SET_STRING_PARAM(maddr)
SIP_URI_GET_SET_INT_PARAM(ttl)
SIP_URI_HAS_SET_BOOL_PARAM(lr)


static int get_char (char*a,int n,char*out) {
	char result;
	char tmp;
	if (*a=='%' && n>2) {
		tmp=a[3];
		a[3]='\0';
		*out=(char)((atoi(a+1)*16)/10); /*convert to hexa*/;
		a[3]=tmp;
		return 3;
	} else {
		*out=*a;
		return 1;
	}
return result;
}

static int uri_strncmp_common(const char*a,const char*b,size_t n,int case_sensitive) {
	int result=0;
	int index_a=0,index_b=0;
	char char_a,char_b;

	while (a[index_a]!='\0'&&b[index_b]!='\0'&&index_a<n&&index_b<n) {
		index_a+=get_char((char*)a+index_a,n-index_a,&char_a);
		index_b+=get_char((char*)b+index_b,n-index_b,&char_b);
		if (!case_sensitive && char_a<0x7B && char_a>0x60) char_a-=0x20;
		if (!case_sensitive && char_b<0x7B && char_b>0x60) char_b-=0x20;
		result+=(char_a!=char_b);
	}
	return result;
}
static int uri_strncmp(const char*a,const char*b,size_t n) {
	return uri_strncmp_common(a,b,n,1);
}
static int uri_strncasecmp(const char*a,const char*b,size_t n) {
	return uri_strncmp_common(a,b,n,0);
}
#define IS_EQUAL(a,b) uri_strncmp(a,b,MIN(strlen(a),strlen(b)))!=0

#define IS_EQUAL_CASE(a,b) uri_strncasecmp(a,b,MIN(strlen(a),strlen(b)))!=0
#define PARAM_CASE_CMP(uri_a,uri_b,param) \
		a_param=belle_sip_parameters_get_parameter((belle_sip_parameters_t*) uri_a,param); \
		b_param=belle_sip_parameters_get_parameter((belle_sip_parameters_t*) uri_b,param);\
		if (a_param && b_param) { \
			if (IS_EQUAL_CASE(a_param,b_param)) return 0; \
		} else if (a_param != b_param) {\
			return 0;\
		}

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
	if (uri_a->user && uri_b->user) {
		if (IS_EQUAL(uri_a->user,uri_b->user)) return 0;
	} else if (uri_a->user != uri_a->user) {
		return 0;
	}
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
		if (!uri_a->host || !uri_b->host) {
			return 0;
		} else if  (IS_EQUAL_CASE(uri_a->host,uri_b->host)) {
			return 0;
		}
		if (uri_a->port !=uri_b->port) return 0;
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
			if ((b_param=belle_sip_parameters_get_parameter((belle_sip_parameters_t*) uri_b,(const char*)params->data)) !=NULL) {
				if (IS_EQUAL_CASE(b_param,(const char*)params->data)) return 0;
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
