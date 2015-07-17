/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2013  Belledonne Communications SARL, Grenoble, France

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
#include "grammars/belle_sip_messageLexer.h"
#include "grammars/belle_sip_messageParser.h"

#include "belle-sip/generic-uri.h"
#include "belle_sip_internal.h"




struct _belle_generic_uri {
	belle_sip_object_t object;
	char* scheme;
	char* user;
	char* user_password;
	char* host;
	int port;
	char* path;
	char* query;
	char* opaque_part;
};

void belle_generic_uri_init(belle_generic_uri_t *uri) {
	uri->port=-1;
}

static void belle_generic_uri_destroy(belle_generic_uri_t* uri) {
	DESTROY_STRING(uri,scheme)
	DESTROY_STRING(uri,user)
	DESTROY_STRING(uri,user_password)
	DESTROY_STRING(uri,host)
	DESTROY_STRING(uri,path)
	DESTROY_STRING(uri,query)
	DESTROY_STRING(uri,opaque_part)
}
static void belle_generic_uri_clone(belle_generic_uri_t* uri, const belle_generic_uri_t *orig){
	CLONE_STRING(belle_generic_uri,scheme,uri,orig)
	CLONE_STRING(belle_generic_uri,user,uri,orig)
	CLONE_STRING(belle_generic_uri,user_password,uri,orig)
	CLONE_STRING(belle_generic_uri,host,uri,orig)
	uri->port=orig->port;
	CLONE_STRING(belle_generic_uri,path,uri,orig)
	CLONE_STRING(belle_generic_uri,query,uri,orig)
	CLONE_STRING(belle_generic_uri,opaque_part,uri,orig)

}
belle_sip_error_code belle_generic_uri_marshal(const belle_generic_uri_t* uri, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_error_code error=BELLE_SIP_OK;

	if (uri->scheme) {
		error=belle_sip_snprintf(buff,buff_size,offset,"%s:",uri->scheme);
		if (error!=BELLE_SIP_OK) return error;
	}
	if (uri->opaque_part) {
		error=belle_sip_snprintf(buff,buff_size,offset,"%s",uri->opaque_part);
		if (error!=BELLE_SIP_OK) return error;
	} else {
		if (uri->host) {
			error=belle_sip_snprintf(buff,buff_size,offset,"//");
			if (error!=BELLE_SIP_OK) return error;
		}

		if (uri->user) {
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
		}

		if (uri->port>0) {
			error=belle_sip_snprintf(buff,buff_size,offset,":%i",uri->port);
			if (error!=BELLE_SIP_OK) return error;
		}

		if (uri->path) {
			char* escaped_path=belle_generic_uri_to_escaped_path(uri->path);
			error=belle_sip_snprintf(buff,buff_size,offset,"%s",escaped_path);
			belle_sip_free(escaped_path);
			if (error!=BELLE_SIP_OK) return error;
		}

		if (uri->query) {
			char* escaped_query=belle_generic_uri_to_escaped_query(uri->query);
			error=belle_sip_snprintf(buff,buff_size,offset,"?%s",escaped_query);
			belle_sip_free(escaped_query);
			if (error!=BELLE_SIP_OK) return error;
		}
	}

	return BELLE_SIP_OK;
}

GET_SET_STRING(belle_generic_uri,scheme);
GET_SET_STRING(belle_generic_uri,user);
GET_SET_STRING(belle_generic_uri,user_password);
GET_SET_STRING(belle_generic_uri,host);
GET_SET_STRING(belle_generic_uri,path);
GET_SET_STRING(belle_generic_uri,query);
GET_SET_STRING(belle_generic_uri,opaque_part);
GET_SET_INT(belle_generic_uri,port,int)
BELLE_NEW(belle_generic_uri,belle_sip_object)
BELLE_PARSE(belle_sip_messageParser,belle_,generic_uri)


char*	belle_generic_uri_to_string(belle_generic_uri_t* uri)  {
	return  belle_sip_object_to_string(uri);
}
