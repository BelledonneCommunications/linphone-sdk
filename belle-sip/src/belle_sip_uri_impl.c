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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "belle_sip_messageParser.h"
#include "belle_sip_messageLexer.h"
#include "belle_sip_internal.h"


#define SIP_URI_GET_SET_STRING(attribute) GET_SET_STRING(belle_sip_uri,attribute)


#define SIP_URI_GET_SET_UINT(attribute) GET_SET_INT(belle_sip_uri,attribute,unsigned int)
#define SIP_URI_GET_SET_INT(attribute) GET_SET_INT(belle_sip_uri,attribute,int)


#define SIP_URI_GET_SET_BOOL(attribute) GET_SET_BOOL(belle_sip_uri,attribute,is)
#define SIP_URI_HAS_SET_BOOL(attribute) GET_SET_BOOL(belle_sip_uri,attribute,has)




struct _belle_sip_uri {
	int ref;
	unsigned int secure;
	char* user;
	char* host;
	unsigned int port;

	char* transport_param;
	char* user_param;
	char* method_param;
	unsigned int lr_param;
	char* maddr_param;
	int 		ttl_param;

	belle_sip_list_t* header_list;
	belle_sip_list_t* headernames_list;
};
void belle_sip_uri_delete(belle_sip_uri_t* uri) {
	if (uri->user) free (uri->user);
	if (uri->host) free (uri->host);

	if (uri->transport_param) free (uri->transport_param);
	if (uri->user_param) free (uri->user_param);
	if (uri->method_param) free (uri->method_param);
	if (uri->maddr_param) free (uri->maddr_param);

	if (uri->header_list) belle_sip_list_free (uri->header_list);
	if (uri->headernames_list) belle_sip_list_free (uri->headernames_list);
	free(uri);
}

BELLE_SIP_REF(uri)




BELLE_SIP_PARSE(uri);

belle_sip_uri_t* belle_sip_uri_new () {
	belle_sip_uri_t* lUri = belle_sip_new0(belle_sip_uri_t);
	lUri->ttl_param=-1;
	return lUri;
}



char*	belle_sip_uri_to_string(belle_sip_uri_t* uri)  {
	return belle_sip_concat(	"sip:"
					,(uri->user?uri->user:"")
					,(uri->user?"@":"")
					,uri->host
					,(uri->transport_param?";transport=":"")
					,(uri->transport_param?uri->transport_param:"")
					,NULL);
}


const char*	belle_sip_uri_get_header(belle_sip_uri_t* uri,const char* name) {
	belle_sip_list_t *  lResult = belle_sip_list_find_custom(uri->header_list, (belle_sip_compare_func)belle_sip_param_pair_comp_func, name);
	if (lResult) {
		return ((belle_sip_param_pair_t*)(lResult->data))->value;
	}
	else {
		return NULL;
	}
}
void	belle_sip_uri_set_header(belle_sip_uri_t* uri,const char* name,const char* value) {
	/*1 check if present*/
	belle_sip_list_t *  lResult = belle_sip_list_find_custom(uri->headernames_list, (belle_sip_compare_func)strcmp, name);
	/* first remove from header names list*/
	if (lResult) {
		belle_sip_list_remove_link(uri->headernames_list,lResult);
	}
	/* next from header list*/
	lResult = belle_sip_list_find_custom(uri->header_list, (belle_sip_compare_func)belle_sip_param_pair_comp_func, name);
	if (lResult) {
		belle_sip_param_pair_delete(lResult->data);
		belle_sip_list_remove_link(uri->header_list,lResult);
	}
	/* 2 insert*/
	belle_sip_param_pair_t* lNewpair = belle_sip_param_pair_new(name,value);
	uri->header_list=belle_sip_list_append(uri->header_list,lNewpair);
	uri->headernames_list=belle_sip_list_append(uri->headernames_list,lNewpair->name);
}

belle_sip_list_t*	belle_sip_uri_get_header_names(belle_sip_uri_t* uri) {
	return uri->headernames_list;
}



SIP_URI_GET_SET_BOOL(secure)

SIP_URI_GET_SET_STRING(user)
SIP_URI_GET_SET_STRING(host)
SIP_URI_GET_SET_UINT(port)

SIP_URI_GET_SET_STRING(transport_param)
SIP_URI_GET_SET_STRING(user_param)
SIP_URI_GET_SET_STRING(method_param)
SIP_URI_GET_SET_STRING(maddr_param)
SIP_URI_GET_SET_INT(ttl_param)
SIP_URI_HAS_SET_BOOL(lr_param)
