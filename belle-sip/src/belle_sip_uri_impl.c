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

int belle_sip_uri_marshal(belle_sip_uri_t* uri, char* buff,unsigned int offset,unsigned int buff_size) {
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
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_uri_t,belle_sip_parameters_t,belle_sip_uri_destroy,belle_sip_uri_clone,belle_sip_uri_marshal);


belle_sip_uri_t* belle_sip_uri_new () {
	belle_sip_uri_t* l_object = (belle_sip_uri_t*)belle_sip_object_new(belle_sip_uri_t);
	belle_sip_parameters_init((belle_sip_parameters_t*)l_object); /*super*/
	l_object->header_list = belle_sip_parameters_new();
	return l_object;
}



char*	belle_sip_uri_to_string(belle_sip_uri_t* uri)  {
	return belle_sip_concat(	"sip:"
					,(uri->user?uri->user:"")
					,(uri->user?"@":"")
					,uri->host
					,NULL);
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
