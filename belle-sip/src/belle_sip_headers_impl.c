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



#include "belle-sip/headers.h"
#include "belle-sip/parameters.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "belle_sip_messageParser.h"
#include "belle_sip_messageLexer.h"
#include "belle_sip_internal.h"

/************************
 * header
 ***********************/

GET_SET_STRING(belle_sip_header,name);

void belle_sip_header_init(belle_sip_header_t *header) {

}

static void belle_sip_header_destroy(belle_sip_header_t *header){
	if (header->name) belle_sip_free((void*)header->name);
	if (header->next) belle_sip_object_unref(BELLE_SIP_OBJECT(header->next));
}
void belle_sip_header_set_next(belle_sip_header_t* header,belle_sip_header_t* next) {
	header->next = next;
}
belle_sip_header_t* belle_sip_header_get_next(const belle_sip_header_t* header) {
	return header->next;
}

int belle_sip_header_marshal(belle_sip_header_t* header, char* buff,unsigned int offset,unsigned int buff_size) {
	if (header->name) {
		return snprintf(buff+offset,buff_size-offset,"%s: ",header->name);
	} else {
		belle_sip_warning("no header name found");
		return 0;
	}
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_header_t);

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_header_t,belle_sip_object_t,belle_sip_header_destroy,NULL,belle_sip_header_marshal);


/************************
 * header_address
 ***********************/
struct _belle_sip_header_address {
	belle_sip_parameters_t base;
	const char* displayname;
	belle_sip_uri_t* uri;
};

static void belle_sip_header_address_init(belle_sip_header_address_t* object){
	belle_sip_parameters_init((belle_sip_parameters_t*)object); /*super*/
}

static void belle_sip_header_address_destroy(belle_sip_header_address_t* contact) {
	if (contact->displayname) belle_sip_free((void*)(contact->displayname));
	if (contact->uri) belle_sip_object_unref(BELLE_SIP_OBJECT(contact->uri));
}

static void belle_sip_header_address_clone(belle_sip_header_address_t *addr, const belle_sip_header_address_t *orig){
	addr->displayname=belle_sip_strdup(orig->displayname);
	addr->uri=(belle_sip_uri_t*)belle_sip_object_clone(BELLE_SIP_OBJECT(orig->uri));
}

int belle_sip_header_address_marshal(belle_sip_header_address_t* header, char* buff,unsigned int offset,unsigned int buff_size) {
	/*1 display name*/
	unsigned int current_offset=offset;
	if (header->displayname) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"\"%s\" ",header->displayname);;
	}
	if (header->uri) {
		if (header->displayname || belle_sip_parameters_get_parameter_names(&header->base) !=NULL) {
			current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","<");
		}
		current_offset+=belle_sip_uri_marshal(header->uri,buff,current_offset,buff_size);
		if (header->displayname || belle_sip_parameters_get_parameter_names(&header->base) !=NULL) {
			current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",">");
		}
	}
	current_offset+=belle_sip_parameters_marshal(&header->base,buff,current_offset,buff_size);
	return current_offset-offset;
}

BELLE_SIP_NEW(header_address,parameters)
GET_SET_STRING(belle_sip_header_address,displayname);

void belle_sip_header_address_set_quoted_displayname(belle_sip_header_address_t* address,const char* value) {
		if (address->displayname != NULL) belle_sip_free((void*)(address->displayname));
		address->displayname=_belle_sip_str_dup_and_unquote_string(value);
}
belle_sip_uri_t* belle_sip_header_address_get_uri(belle_sip_header_address_t* address) {
	return address->uri;
}

void belle_sip_header_address_set_uri(belle_sip_header_address_t* address, belle_sip_uri_t* uri) {
	address->uri=(belle_sip_uri_t*)belle_sip_object_ref(uri);
}


/******************************
 * Extension header hinerite from header
 *
 ******************************/
struct _belle_sip_header_allow  {
	belle_sip_header_t header;
	const char* method;
};

static void belle_sip_header_allow_destroy(belle_sip_header_allow_t* allow) {
	if (allow->method) belle_sip_free((void*)allow->method);
}

static void belle_sip_header_allow_clone(belle_sip_header_allow_t* allow, const belle_sip_header_allow_t* orig){
}
int belle_sip_header_allow_marshal(belle_sip_header_allow_t* allow, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(allow), buff,current_offset, buff_size);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",allow->method);
	return current_offset-offset;

}
BELLE_SIP_NEW_HEADER(header_allow,header,"Allow")
BELLE_SIP_PARSE(header_allow)
GET_SET_STRING(belle_sip_header_allow,method);



/************************
 * header_contact
 ***********************/
struct _belle_sip_header_contact {
	belle_sip_header_address_t address;
	unsigned int wildcard;
 };

void belle_sip_header_contact_destroy(belle_sip_header_contact_t* contact) {
}

void belle_sip_header_contact_clone(belle_sip_header_contact_t *contact, const belle_sip_header_contact_t *orig){
}
int belle_sip_header_contact_marshal(belle_sip_header_contact_t* contact, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(contact), buff,offset, buff_size);
	if (contact->wildcard) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","*");
	} else {
		current_offset+=belle_sip_header_address_marshal(&contact->address, buff,current_offset, buff_size);
	}
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_contact,header_address,"Contact")
BELLE_SIP_PARSE(header_contact)

GET_SET_INT_PARAM_PRIVATE(belle_sip_header_contact,expires,int,_)
GET_SET_INT_PARAM_PRIVATE(belle_sip_header_contact,q,float,_);
GET_SET_BOOL(belle_sip_header_contact,wildcard,is);


int belle_sip_header_contact_set_expires(belle_sip_header_contact_t* contact, int expires) {
	if (expires < 0 ) {
		 belle_sip_error("bad expires value [%i] for contact",expires);
		return -1;
	}
	_belle_sip_header_contact_set_expires(contact,expires);
	return 0;
 }
int belle_sip_header_contact_set_qvalue(belle_sip_header_contact_t* contact, float qValue) {
	 if (qValue != -1 && qValue < 0 && qValue >1) {
		 belle_sip_error("bad q value [%f] for contact",qValue);
		 return -1;
	 }
	 _belle_sip_header_contact_set_q(contact,qValue);
	 return 0;
}
float	belle_sip_header_contact_get_qvalue(const belle_sip_header_contact_t* contact) {
	return belle_sip_header_contact_get_q(contact);
}
/**************************
* From header object inherent from header_address
****************************
*/
#define BELLE_SIP_FROM_LIKE_MARSHAL(header) \
		unsigned int current_offset=offset; \
		current_offset+=belle_sip_##header_marshal(BELLE_SIP_HEADER(header), buff,current_offset, buff_size);\
		current_offset+=belle_sip_header_address_marshal(&header->address, buff,current_offset, buff_size); \
		return current_offset-offset;

struct _belle_sip_header_from  {
	belle_sip_header_address_t address;
};

static void belle_sip_header_from_destroy(belle_sip_header_from_t* from) {
}

static void belle_sip_header_from_clone(belle_sip_header_from_t* from, const belle_sip_header_from_t* cloned) {
}
int belle_sip_header_from_marshal(belle_sip_header_from_t* from, char* buff,unsigned int offset,unsigned int buff_size) {
	BELLE_SIP_FROM_LIKE_MARSHAL(from);
}

belle_sip_header_from_t* belle_sip_header_from_create(const char *address, const char *tag){
	char *tmp=belle_sip_strdup_printf("From: %s",address);
	belle_sip_header_from_t *from=belle_sip_header_from_parse(tmp);
	if (tag) belle_sip_header_from_set_tag(from,tag);
	belle_sip_free(tmp);
	return from;
}

BELLE_SIP_NEW_HEADER(header_from,header_address,"From")
BELLE_SIP_PARSE(header_from)
GET_SET_STRING_PARAM(belle_sip_header_from,tag);

/**************************
* To header object inherits from header_address
****************************
*/
struct _belle_sip_header_to  {
	belle_sip_header_address_t address;
};

static void belle_sip_header_to_destroy(belle_sip_header_to_t* to) {
}

void belle_sip_header_to_clone(belle_sip_header_to_t *contact, const belle_sip_header_to_t *orig){
}
int belle_sip_header_to_marshal(belle_sip_header_to_t* to, char* buff,unsigned int offset,unsigned int buff_size) {
	BELLE_SIP_FROM_LIKE_MARSHAL(to)
}

BELLE_SIP_NEW_HEADER(header_to,header_address,"To")
BELLE_SIP_PARSE(header_to)
GET_SET_STRING_PARAM(belle_sip_header_to,tag);

belle_sip_header_to_t* belle_sip_header_to_create(const char *address, const char *tag){
	char *tmp=belle_sip_strdup_printf("To: %s",address);
	belle_sip_header_to_t *to=belle_sip_header_to_parse(tmp);
	if (tag) belle_sip_header_to_set_tag(to,tag);
	belle_sip_free(tmp);
	return to;
}

/******************************
 * User-Agent header hinerite from header
 *
 ******************************/
struct _belle_sip_header_user_agent  {
	belle_sip_header_t header;
	belle_sip_list_t* products;
};

static void belle_sip_header_user_agent_destroy(belle_sip_header_user_agent_t* user_agent) {
	belle_sip_header_user_agent_set_products(user_agent,NULL);
}

static void belle_sip_header_user_agent_clone(belle_sip_header_user_agent_t* user_agent, const belle_sip_header_user_agent_t* orig){
}
int belle_sip_header_user_agent_marshal(belle_sip_header_user_agent_t* user_agent, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	belle_sip_list_t* list = user_agent->products;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(user_agent), buff,current_offset, buff_size);
	for(;list!=NULL;list=list->next){
		current_offset+=snprintf(	buff+current_offset
									,buff_size-current_offset
									," %s"
									,(const char *)list->data);
	}
	return current_offset-offset;

}
BELLE_SIP_NEW_HEADER(header_user_agent,header_address,"User-Agent")
BELLE_SIP_PARSE(header_user_agent)
belle_sip_list_t* belle_sip_header_user_agent_get_products(const belle_sip_header_user_agent_t* user_agent) {
	return user_agent->products;
}
void belle_sip_header_user_agent_set_products(belle_sip_header_user_agent_t* user_agent,belle_sip_list_t* products) {
	belle_sip_list_t* list;
	if (user_agent->products) {
		for (list=user_agent->products;list !=NULL; list=list->next) {
			belle_sip_free((void*)list->data);

		}
		belle_sip_list_free(user_agent->products);
	}
	user_agent->products=products;
}
void belle_sip_header_user_agent_add_product(belle_sip_header_user_agent_t* user_agent,const char* product) {
	user_agent->products = belle_sip_list_append(user_agent->products ,belle_sip_strdup(product));
}

/**************************
* Via header object inherits from parameters
****************************
*/
struct _belle_sip_header_via  {
	belle_sip_parameters_t params_list;
	char* protocol;
	char* transport;
	char* host;
	int port;
};

static void belle_sip_header_via_destroy(belle_sip_header_via_t* via) {
	if (via->host) belle_sip_free(via->host);
	if (via->protocol) belle_sip_free(via->protocol);
}

static void belle_sip_header_via_clone(belle_sip_header_via_t* via, const belle_sip_header_via_t*orig){
}

int belle_sip_header_via_marshal(belle_sip_header_via_t* via, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(via), buff,current_offset, buff_size);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s/%s",via->protocol,via->transport);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset," %s",via->host);
	if (via->port > 0) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,":%i",via->port);
	}
	current_offset+=belle_sip_parameters_marshal(&via->params_list, buff,current_offset, buff_size);
	return current_offset-offset;
}

belle_sip_header_via_t* belle_sip_header_via_create(const char *host, int port, const char *transport, const char *branch){
	belle_sip_header_via_t *via=belle_sip_header_via_new();
	via->host=belle_sip_strdup(host);
	via->port=port;
	via->transport=belle_sip_strdup(transport);
	via->protocol=belle_sip_strdup("SIP/2.0");
	belle_sip_header_via_set_branch(via,branch);
	return via;
}

BELLE_SIP_NEW_HEADER(header_via,header_address,"Via")
BELLE_SIP_PARSE(header_via)
GET_SET_STRING(belle_sip_header_via,protocol);
GET_SET_STRING(belle_sip_header_via,transport);
GET_SET_STRING(belle_sip_header_via,host);
GET_SET_INT_PRIVATE(belle_sip_header_via,port,int,_);

GET_SET_STRING_PARAM(belle_sip_header_via,branch);
GET_SET_STRING_PARAM(belle_sip_header_via,maddr);
GET_SET_STRING_PARAM(belle_sip_header_via,received);

GET_SET_INT_PARAM_PRIVATE(belle_sip_header_via,rport,int,_)
GET_SET_INT_PARAM_PRIVATE(belle_sip_header_via,ttl,int,_)

int belle_sip_header_via_set_rport (belle_sip_header_via_t* obj,int  value) {
	if (value ==-1 || (value>0 && value<65536)) {
		_belle_sip_header_via_set_rport(obj,value);
		return 0;
	} else {
		belle_sip_error("bad rport value [%i] for via",value);
		return -1;
	}
}
int belle_sip_header_via_set_ttl (belle_sip_header_via_t* obj,int  value) {
	if (value ==-1 || (value>0 && value<=255)) {
		_belle_sip_header_via_set_ttl(obj,value);
		return 0;
	} else {
		belle_sip_error("bad ttl value [%i] for via",value);
		return -1;
	}
}

int belle_sip_header_via_set_port (belle_sip_header_via_t* obj,int  value) {
	if (value ==-1 || (value>0 && value<65536)) {
		_belle_sip_header_via_set_port(obj,value);
		return 0;
	} else {
		belle_sip_error("bad port value [%i] for via",value);
		return -1;
	}
}

int belle_sip_header_via_get_listening_port(const belle_sip_header_via_t *via){
	int ret=belle_sip_header_via_get_port(via);
	if (ret==-1) ret=belle_sip_listening_point_get_well_known_port(via->protocol);
	return ret;
}

/**************************
* call_id header object inherits from object
****************************
*/
struct _belle_sip_header_call_id  {
	belle_sip_header_t header;
	const char* call_id;
};

static void belle_sip_header_call_id_destroy(belle_sip_header_call_id_t* call_id) {
	if (call_id->call_id) belle_sip_free((void*)call_id->call_id);
}

static void belle_sip_header_call_id_clone(belle_sip_header_call_id_t* call_id,const belle_sip_header_call_id_t *orig){
}
int belle_sip_header_call_id_marshal(belle_sip_header_call_id_t* call_id, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(call_id), buff,current_offset, buff_size);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",call_id->call_id);
	return current_offset-offset;
}

BELLE_SIP_NEW_HEADER(header_call_id,header,"Call-ID")
BELLE_SIP_PARSE(header_call_id)
GET_SET_STRING(belle_sip_header_call_id,call_id);
/**************************
* cseq header object inherent from object
****************************
*/
struct _belle_sip_header_cseq  {
	belle_sip_header_t header;
	char* method;
	unsigned int seq_number;
};

static void belle_sip_header_cseq_destroy(belle_sip_header_cseq_t* cseq) {
	if (cseq->method) belle_sip_free(cseq->method);
}

static void belle_sip_header_cseq_clone(belle_sip_header_cseq_t* cseq, const belle_sip_header_cseq_t *orig) {
	if (cseq->method) belle_sip_free(cseq->method);
	cseq->method=belle_sip_strdup(orig->method);
}
int belle_sip_header_cseq_marshal(belle_sip_header_cseq_t* cseq, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(cseq), buff,current_offset, buff_size);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%i %s",cseq->seq_number,cseq->method);
	return current_offset-offset;
}
belle_sip_header_cseq_t * belle_sip_header_cseq_create(unsigned int number, const char *method){
	belle_sip_header_cseq_t *cseq=belle_sip_header_cseq_new();
	cseq->method=belle_sip_strdup(method);
	cseq->seq_number=number;
	return cseq;
}
BELLE_SIP_NEW_HEADER(header_cseq,header,"CSeq")
BELLE_SIP_PARSE(header_cseq)
GET_SET_STRING(belle_sip_header_cseq,method);
GET_SET_INT(belle_sip_header_cseq,seq_number,unsigned int)
/**************************
* content type header object inherent from parameters
****************************
*/
struct _belle_sip_header_content_type  {
	belle_sip_parameters_t params_list;
	const char* type;
	const char* subtype;
};

static void belle_sip_header_content_type_destroy(belle_sip_header_content_type_t* content_type) {
	if (content_type->type) belle_sip_free((void*)content_type->type);
	if (content_type->subtype) belle_sip_free((void*)content_type->subtype);
}

static void belle_sip_header_content_type_clone(belle_sip_header_content_type_t* content_type, const belle_sip_header_content_type_t* orig){
}
int belle_sip_header_content_type_marshal(belle_sip_header_content_type_t* content_type, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(content_type), buff,current_offset, buff_size);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s/%s",content_type->type, content_type->subtype);
	current_offset+=belle_sip_parameters_marshal(&content_type->params_list, buff,current_offset, buff_size);
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_content_type,parameters,"Content-Type")
BELLE_SIP_PARSE(header_content_type)
GET_SET_STRING(belle_sip_header_content_type,type);
GET_SET_STRING(belle_sip_header_content_type,subtype);
/**************************
* Route header object inherent from header_address
****************************
*/
struct _belle_sip_header_route  {
	belle_sip_header_address_t address;
};

static void belle_sip_header_route_destroy(belle_sip_header_route_t* route) {
}

static void belle_sip_header_route_clone(belle_sip_header_route_t* route, const belle_sip_header_route_t* orig) {
}
int belle_sip_header_route_marshal(belle_sip_header_route_t* route, char* buff,unsigned int offset,unsigned int buff_size) {
	BELLE_SIP_FROM_LIKE_MARSHAL(route)
}
BELLE_SIP_NEW_HEADER(header_route,header_address,"Route")
BELLE_SIP_PARSE(header_route)
/**************************
* Record route header object inherent from header_address
****************************
*/
struct _belle_sip_header_record_route  {
	belle_sip_header_address_t address;
};

static void belle_sip_header_record_route_destroy(belle_sip_header_record_route_t* record_route) {
}

static void belle_sip_header_record_route_clone(belle_sip_header_record_route_t* record_route,
                                const belle_sip_header_record_route_t* orig               ) {
}
int belle_sip_header_record_route_marshal(belle_sip_header_record_route_t* record_route, char* buff,unsigned int offset,unsigned int buff_size) {
	BELLE_SIP_FROM_LIKE_MARSHAL(record_route)
}
BELLE_SIP_NEW_HEADER(header_record_route,header_address,"Record-Route")
BELLE_SIP_PARSE(header_record_route)
/**************************
* content length header object inherent from object
****************************
*/
struct _belle_sip_header_content_length  {
	belle_sip_header_t header;
	unsigned int content_length;
};

static void belle_sip_header_content_length_destroy(belle_sip_header_content_length_t* content_length) {
}

static void belle_sip_header_content_length_clone(belle_sip_header_content_length_t* content_length,
                                                 const belle_sip_header_content_length_t *orig ) {
}

int belle_sip_header_content_length_marshal(belle_sip_header_content_length_t* content_length, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(content_length), buff,current_offset, buff_size);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%i",content_length->content_length);
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_content_length,header,"Content-Length")
BELLE_SIP_PARSE(header_content_length)
GET_SET_INT(belle_sip_header_content_length,content_length,unsigned int)
/**************************
* Expires header object inherent from header
****************************
*/
struct _belle_sip_header_expires  {
	belle_sip_header_t header;
	int expires;
};

static void belle_sip_header_expires_destroy(belle_sip_header_expires_t* expires) {
}

static void belle_sip_header_expires_clone(belle_sip_header_expires_t* expires,
                                                 const belle_sip_header_expires_t *orig ) {
}

int belle_sip_header_expires_marshal(belle_sip_header_expires_t* expires, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(expires), buff,current_offset, buff_size);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%i",expires->expires);
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_expires,header,"Expires")
BELLE_SIP_PARSE(header_expires)
GET_SET_INT(belle_sip_header_expires,expires,int)

/******************************
 * Extension header hinerite from header
 *
 ******************************/
struct _belle_sip_header_extension  {
	belle_sip_header_t header;
	const char* value;
};

static void belle_sip_header_extension_destroy(belle_sip_header_extension_t* extension) {
	if (extension->value) belle_sip_free((void*)extension->value);
}

static void belle_sip_header_extension_clone(belle_sip_header_extension_t* extension, const belle_sip_header_extension_t* orig){
}
int belle_sip_header_extension_marshal(belle_sip_header_extension_t* extension, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(extension), buff,current_offset, buff_size);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",extension->value);
	return current_offset-offset;

}
BELLE_SIP_NEW_HEADER(header_extension,header,NULL)

/**
 * special case for this header. I don't know why
 */
belle_sip_header_extension_t* belle_sip_header_extension_parse (const char* value) {
	pANTLR3_INPUT_STREAM           input;
	pbelle_sip_messageLexer               lex;
	pANTLR3_COMMON_TOKEN_STREAM    tokens;
	pbelle_sip_messageParser              parser;
	input  = antlr3NewAsciiStringCopyStream	(
			(pANTLR3_UINT8)value,
			(ANTLR3_UINT32)strlen(value),
			((void *)0));
	lex    = belle_sip_messageLexerNew                (input);
	tokens = antlr3CommonTokenStreamSourceNew  (1025, lex->pLexer->rec->state->tokSource);
	parser = belle_sip_messageParserNew               (tokens);
	belle_sip_messageParser_header_extension_return l_parsed_object = parser->header_extension(parser,FALSE);
	parser ->free(parser);
	tokens ->free(tokens);
	lex    ->free(lex);
	input  ->close(input);
	if (l_parsed_object.ret == NULL) belle_sip_error("Parser error for [%s]",value);\
	return BELLE_SIP_HEADER_EXTENSION(l_parsed_object.ret);
}
GET_SET_STRING(belle_sip_header_extension,value);
/**************************
*Authorization header object inherent from parameters
****************************
*/
#define AUTH_BASE \
	belle_sip_parameters_t params_list; \
	const char* scheme; \
	const char* realm; \
	const char* nonce; \
	const char* algorithm; \
	const char* opaque; \
	const char* qop; \

#define AUTH_BASE_DESTROY(obj) \
	if (obj->scheme) belle_sip_free((void*)obj->scheme);\
	if (obj->realm) belle_sip_free((void*)obj->realm);\
	if (obj->nonce) belle_sip_free((void*)obj->nonce);\
	if (obj->algorithm) belle_sip_free((void*)obj->algorithm);\
	if (obj->opaque) belle_sip_free((void*)obj->opaque);\
	if (obj->qop) belle_sip_free((void*)obj->qop);\
	/*if (obj->params_list) FIXME free list*/


#define AUTH_BASE_MARSHAL(header) \
	unsigned int current_offset=offset;\
	char* border=" ";\
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(header), buff,current_offset, buff_size);\
	const belle_sip_list_t* list=belle_sip_parameters_get_parameters(&header->params_list);\
	if (header->scheme) { \
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset," %s",header->scheme);\
		} else { \
			belle_sip_error("missing mandatory scheme"); \
		} \
	for(;list!=NULL;list=list->next){\
		belle_sip_param_pair_t* container = list->data;\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s%s=%s",border, container->name,container->value);\
		border=", ";\
	}\
	if (header->realm) {\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%srealm=\"%s\"",border,header->realm);\
		border=", ";\
		}\
	if (header->nonce) {\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%snonce=\"%s\"",border,header->nonce);\
		border=", ";\
		}\
	if (header->algorithm) {\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%salgorithm=%s",border,header->algorithm);\
		border=", ";\
		}\
	if (header->opaque) {\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sopaque=\"%s\"",border,header->opaque);\
		border=", ";\
		}




	struct _belle_sip_header_authorization  {
	AUTH_BASE
	const char* username;
	belle_sip_uri_t* uri;
	const char* response;
	const char* cnonce;
	int nonce_count;

};


static void belle_sip_header_authorization_destroy(belle_sip_header_authorization_t* authorization) {
	if (authorization->username) belle_sip_free((void*)authorization->username);
	if (authorization->uri) {
			belle_sip_object_unref(BELLE_SIP_OBJECT(authorization->uri));
	}
	if (authorization->cnonce) belle_sip_free((void*)authorization->cnonce);
	AUTH_BASE_DESTROY(authorization)
}

static void belle_sip_header_authorization_clone(belle_sip_header_authorization_t* authorization,
                                                 const belle_sip_header_authorization_t *orig ) {
}
static void belle_sip_header_authorization_init(belle_sip_header_authorization_t* authorization) {
}

belle_sip_uri_t* belle_sip_header_authorization_get_uri(const belle_sip_header_authorization_t* authorization) {
	return authorization->uri;
}

void belle_sip_header_authorization_set_uri(belle_sip_header_authorization_t* authorization, belle_sip_uri_t* uri) {
	if (authorization->uri) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(authorization->uri));
	}
	authorization->uri=uri;
	if (authorization->uri) belle_sip_object_ref(authorization->uri);
}
int belle_sip_header_authorization_marshal(belle_sip_header_authorization_t* authorization, char* buff,unsigned int offset,unsigned int buff_size) {
	AUTH_BASE_MARSHAL(authorization)
	if (authorization->username) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%susername=\"%s\"",border,authorization->username);\
		border=", ";
		}
	if (authorization->uri) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s uri=\"",border);
		border=", ";
		current_offset+=belle_sip_uri_marshal(authorization->uri,buff,current_offset,buff_size);
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","\"");
		}
	if (authorization->algorithm) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%salgorithm=%s",border,authorization->algorithm);
		border=", ";
		}
	if (authorization->response) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sresponse=\"%s\"",border,authorization->response);
		border=", ";
		}
	if (authorization->cnonce) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%scnonce=\"%s\"",border,authorization->cnonce);
		border=", ";
		}
	if (authorization->nonce_count>0) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%snc=%08i",border,authorization->nonce_count);
		border=", ";
	}
	if (authorization->qop) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sqop=%s",border,authorization->qop);
	}
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_authorization,parameters,"Authorization")
BELLE_SIP_PARSE(header_authorization)
GET_SET_STRING(belle_sip_header_authorization,scheme);
GET_SET_STRING(belle_sip_header_authorization,username);
GET_SET_STRING(belle_sip_header_authorization,realm);
GET_SET_STRING(belle_sip_header_authorization,nonce);
GET_SET_STRING(belle_sip_header_authorization,response);
GET_SET_STRING(belle_sip_header_authorization,algorithm);
GET_SET_STRING(belle_sip_header_authorization,cnonce);
GET_SET_STRING(belle_sip_header_authorization,opaque);
GET_SET_STRING(belle_sip_header_authorization,qop);
GET_SET_INT(belle_sip_header_authorization,nonce_count,int)

/**************************
*Proxy-Authorization header object inherent from parameters
****************************
*/
struct _belle_sip_header_proxy_authorization  {
	belle_sip_header_authorization_t authorization;
};


static void belle_sip_header_proxy_authorization_destroy(belle_sip_header_proxy_authorization_t* proxy_authorization) {
}

static void belle_sip_header_proxy_authorization_clone(belle_sip_header_proxy_authorization_t* proxy_authorization,
                                                 const belle_sip_header_proxy_authorization_t *orig ) {
}
int belle_sip_header_proxy_authorization_marshal(belle_sip_header_proxy_authorization_t* proxy_authorization, char* buff,unsigned int offset,unsigned int buff_size) {
	return belle_sip_header_authorization_marshal(&proxy_authorization->authorization,buff,offset,buff_size);
}
BELLE_SIP_NEW_HEADER(header_proxy_authorization,header_authorization,"Proxy-Authorization")
BELLE_SIP_PARSE(header_proxy_authorization)
/**************************
*WWW-Authorization header object inherent from parameters
****************************
*/
struct _belle_sip_header_www_authenticate  {
	AUTH_BASE
	const char* domain;
	unsigned int stale;
};


static void belle_sip_header_www_authenticate_destroy(belle_sip_header_www_authenticate_t* www_authenticate) {
	if (www_authenticate->domain) belle_sip_free((void*)www_authenticate->domain);
}
void belle_sip_header_www_authenticate_init(belle_sip_header_www_authenticate_t* www_authenticate) {
	www_authenticate->stale=-1;
}
static void belle_sip_header_www_authenticate_clone(belle_sip_header_www_authenticate_t* www_authenticate,
                                                 const belle_sip_header_www_authenticate_t *orig ) {
}
int belle_sip_header_www_authenticate_marshal(belle_sip_header_www_authenticate_t* www_authenticate, char* buff,unsigned int offset,unsigned int buff_size) {
	AUTH_BASE_MARSHAL(www_authenticate)
	if (www_authenticate->domain) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sdomain=\"%s\"",border,www_authenticate->domain);\
		border=", ";
		}
	if (www_authenticate->stale>=0) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sstale=%s",border,www_authenticate->stale?"true":"false");
		}
	if (www_authenticate->qop) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sqop=\"%s\"",border,www_authenticate->qop);
		border=", ";
	}
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER_INIT(header_www_authenticate,parameters,"WWW-Authenticate",header_www_authenticate)
BELLE_SIP_PARSE(header_www_authenticate)
GET_SET_STRING(belle_sip_header_www_authenticate,scheme);
GET_SET_STRING(belle_sip_header_www_authenticate,realm);
GET_SET_STRING(belle_sip_header_www_authenticate,nonce);
GET_SET_STRING(belle_sip_header_www_authenticate,algorithm);
GET_SET_STRING(belle_sip_header_www_authenticate,opaque);
GET_SET_STRING(belle_sip_header_www_authenticate,qop);
GET_SET_STRING(belle_sip_header_www_authenticate,domain)
GET_SET_BOOL(belle_sip_header_www_authenticate,stale,is)
/**************************
* max forwards header object inherent from header
****************************
*/
struct _belle_sip_header_max_forwards  {
	belle_sip_header_t header;
	int max_forwards;
};

static void belle_sip_header_max_forwards_destroy(belle_sip_header_max_forwards_t* max_forwards) {
}

static void belle_sip_header_max_forwards_clone(belle_sip_header_max_forwards_t* max_forwards,
                                                 const belle_sip_header_max_forwards_t *orig ) {
}

int belle_sip_header_max_forwards_marshal(belle_sip_header_max_forwards_t* max_forwards, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(max_forwards), buff,current_offset, buff_size);
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%i",max_forwards->max_forwards);
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_max_forwards,header,"Max-Forwards")
BELLE_SIP_PARSE(header_max_forwards)
GET_SET_INT(belle_sip_header_max_forwards,max_forwards,int)
int belle_sip_header_max_forwards_decrement_max_forwards(belle_sip_header_max_forwards_t* max_forwards) {
	return max_forwards->max_forwards--;
}
