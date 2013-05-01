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
#include "listeningpoint_internal.h"

/************************
 * header
 ***********************/

GET_SET_STRING(belle_sip_header,name);

belle_sip_header_t* belle_sip_header_create (const char* name,const char* value) {
	return BELLE_SIP_HEADER(belle_sip_header_extension_create(name,value));
}
void belle_sip_header_init(belle_sip_header_t *header) {

}
static void belle_sip_header_clone(belle_sip_header_t *header, const belle_sip_header_t *orig){
	CLONE_STRING(belle_sip_header,name,header,orig)
	if (belle_sip_header_get_next(orig)) {
		belle_sip_header_set_next(header,BELLE_SIP_HEADER(belle_sip_object_clone(BELLE_SIP_OBJECT(belle_sip_header_get_next(orig))))) ;
	}
}
static void belle_sip_header_destroy(belle_sip_header_t *header){
	if (header->name) belle_sip_free((void*)header->name);
	if (header->next) belle_sip_object_unref(BELLE_SIP_OBJECT(header->next));
}
void belle_sip_header_set_next(belle_sip_header_t* header,belle_sip_header_t* next) {
	if (next) belle_sip_object_ref(next);
	if(header->next) belle_sip_object_unref(header->next);
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

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_header_t,belle_sip_object_t,belle_sip_header_destroy,belle_sip_header_clone,belle_sip_header_marshal,TRUE);


/************************
 * header_address
 ***********************/
struct _belle_sip_header_address {
	belle_sip_parameters_t base;
	char* displayname;
	belle_sip_uri_t* uri;
};

static void belle_sip_header_address_init(belle_sip_header_address_t* object){
	belle_sip_parameters_init((belle_sip_parameters_t*)object); /*super*/
}

static void belle_sip_header_address_destroy(belle_sip_header_address_t* address) {
	if (address->displayname) belle_sip_free(address->displayname);
	if (address->uri) belle_sip_object_unref(address->uri);
}

static void belle_sip_header_address_clone(belle_sip_header_address_t *addr, const belle_sip_header_address_t *orig){
	CLONE_STRING(belle_sip_header_address,displayname,addr,orig)
	if (belle_sip_header_address_get_uri(orig)) {
		belle_sip_header_address_set_uri(addr,BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(belle_sip_header_address_get_uri(orig)))));
	}
}

int belle_sip_header_address_marshal(belle_sip_header_address_t* header, char* buff,unsigned int offset,unsigned int buff_size) {
	/*1 display name*/
	unsigned int current_offset=offset;
	if (header->displayname) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"\"%s\" ",header->displayname);
		if (current_offset>=buff_size) goto end;
	}
	if (header->uri) {
		/*cases where < is required*/
		if (header->displayname
			|| belle_sip_parameters_get_parameter_names((belle_sip_parameters_t*)header->uri)
			|| belle_sip_uri_get_header_names(header->uri)
			|| belle_sip_parameters_get_parameter_names(&header->base)) {
			current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","<");
			if (current_offset>=buff_size) goto end;
		}
		current_offset+=belle_sip_uri_marshal(header->uri,buff,current_offset,buff_size);
		if (header->displayname
				|| belle_sip_parameters_get_parameter_names((belle_sip_parameters_t*)header->uri)
				|| belle_sip_uri_get_header_names(header->uri)
				|| belle_sip_parameters_get_parameter_names(&header->base)) {
			current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",">");
			if (current_offset>=buff_size) goto end;
		}
	}
	current_offset+=belle_sip_parameters_marshal(&header->base,buff,current_offset,buff_size);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;
}

BELLE_SIP_NEW_HEADER(header_address,parameters,"header_address")
BELLE_SIP_PARSE(header_address)
GET_SET_STRING(belle_sip_header_address,displayname);

void belle_sip_header_address_set_quoted_displayname(belle_sip_header_address_t* address,const char* value) {
		if (address->displayname != NULL) belle_sip_free(address->displayname);
		if (strlen(value)>2)
			address->displayname=_belle_sip_str_dup_and_unquote_string(value);
		else
			address->displayname=NULL;
}

belle_sip_uri_t* belle_sip_header_address_get_uri(const belle_sip_header_address_t* address) {
	return address->uri;
}

void belle_sip_header_address_set_uri(belle_sip_header_address_t* address, belle_sip_uri_t* uri) {
	belle_sip_object_ref(uri);
	if (address->uri){
		belle_sip_object_unref(address->uri);
	}
	address->uri=uri;
}

belle_sip_header_address_t* belle_sip_header_address_create(const char* display, belle_sip_uri_t* uri) {
	belle_sip_header_address_t* address = belle_sip_header_address_new();
	belle_sip_header_address_set_displayname(address,display);
	belle_sip_header_address_set_uri(address,uri);
	return address;
}

/******************************
 * Extension header inherits from header
 *
 ******************************/
struct _belle_sip_header_allow  {
	belle_sip_header_t header;
	const char* method;
};
static void belle_sip_header_allow_clone(belle_sip_header_allow_t *allow, const belle_sip_header_allow_t *orig){
	CLONE_STRING(belle_sip_header_allow,method,allow,orig)
}
static void belle_sip_header_allow_destroy(belle_sip_header_allow_t* allow) {
	if (allow->method) belle_sip_free((void*)allow->method);
}


int belle_sip_header_allow_marshal(belle_sip_header_allow_t* allow, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(allow), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",allow->method);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;

}
BELLE_SIP_NEW_HEADER(header_allow,header,"Allow")
BELLE_SIP_PARSE(header_allow)
belle_sip_header_allow_t* belle_sip_header_allow_create (const char* methods) {
	belle_sip_header_allow_t* allow = belle_sip_header_allow_new();
	belle_sip_header_allow_set_method(allow,methods);
	return allow;
}
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
	contact->wildcard=orig->wildcard;
}
int belle_sip_header_contact_marshal(belle_sip_header_contact_t* contact, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(contact), buff,offset, buff_size);
	if (contact->wildcard) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","*");
	} else {
		current_offset+=belle_sip_header_address_marshal(&contact->address, buff,current_offset, buff_size);
	}
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_contact,header_address,BELLE_SIP_CONTACT)
BELLE_SIP_PARSE(header_contact)
belle_sip_header_contact_t* belle_sip_header_contact_create (const belle_sip_header_address_t* contact) {
	belle_sip_header_contact_t* header = belle_sip_header_contact_new();
	_belle_sip_object_copy(BELLE_SIP_OBJECT(header),BELLE_SIP_OBJECT(contact));
	belle_sip_header_set_name(BELLE_SIP_HEADER(header),BELLE_SIP_CONTACT); /*restaure header name*/
	return header;
}
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
unsigned int belle_sip_header_contact_equals(const belle_sip_header_contact_t* a,const belle_sip_header_contact_t* b) {
	if (!a | !b) return 0;
	return belle_sip_uri_equals(belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(a))
								,belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(b)));
}
unsigned int belle_sip_header_contact_not_equals(const belle_sip_header_contact_t* a,const belle_sip_header_contact_t* b) {
	return !belle_sip_header_contact_equals(a,b);
}
/**************************
* From header object inherent from header_address
****************************
*/
#define BELLE_SIP_FROM_LIKE_MARSHAL(header) \
		unsigned int current_offset=offset; \
		current_offset+=belle_sip_##header_marshal(BELLE_SIP_HEADER(header), buff,current_offset, buff_size);\
		if (current_offset>=buff_size) goto end;\
		current_offset+=belle_sip_header_address_marshal(&header->address, buff,current_offset, buff_size); \
		if (current_offset>=buff_size) goto end;\
		end:\
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

belle_sip_header_from_t* belle_sip_header_from_create2(const char *address, const char *tag){
	char *tmp=belle_sip_strdup_printf("From: %s",address);
	belle_sip_header_from_t *from=belle_sip_header_from_parse(tmp);
	if (from){
		if (tag) belle_sip_header_from_set_tag(from,tag);
	}
	belle_sip_free(tmp);
	return from;
}
belle_sip_header_from_t* belle_sip_header_from_create(const belle_sip_header_address_t* address, const char *tag) {
	belle_sip_header_from_t* header= belle_sip_header_from_new();
	_belle_sip_object_copy((belle_sip_object_t*)header,(belle_sip_object_t*)address);
	belle_sip_header_set_name(BELLE_SIP_HEADER(header),BELLE_SIP_FROM); /*restore header name*/
	if (tag) belle_sip_header_from_set_tag(header,tag);
	return header;
}
BELLE_SIP_NEW_HEADER(header_from,header_address,BELLE_SIP_FROM)
BELLE_SIP_PARSE(header_from)
GET_SET_STRING_PARAM2(belle_sip_header_from,tag,raw_tag);

void belle_sip_header_from_set_random_tag(belle_sip_header_from_t *obj){
	char tmp[BELLE_SIP_TAG_LENGTH];
	belle_sip_header_from_set_raw_tag(obj,belle_sip_random_token(tmp,sizeof(tmp)));
}

void belle_sip_header_from_set_tag(belle_sip_header_from_t *obj, const char *tag){
	if (tag==BELLE_SIP_RANDOM_TAG) belle_sip_header_from_set_random_tag(obj);
	else belle_sip_header_from_set_raw_tag(obj,tag);
}

const char *belle_sip_header_from_get_tag(const belle_sip_header_from_t *obj){
	return belle_sip_header_from_get_raw_tag(obj);
}

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

BELLE_SIP_NEW_HEADER(header_to,header_address,BELLE_SIP_TO)
BELLE_SIP_PARSE(header_to)
GET_SET_STRING_PARAM2(belle_sip_header_to,tag,raw_tag);

belle_sip_header_to_t* belle_sip_header_to_create2(const char *address, const char *tag){
	char *tmp=belle_sip_strdup_printf("To: %s",address);
	belle_sip_header_to_t *to=belle_sip_header_to_parse(tmp);
	if (to){
		if (tag) belle_sip_header_to_set_tag(to,tag);
	}
	belle_sip_free(tmp);
	return to;
}
belle_sip_header_to_t* belle_sip_header_to_create(const belle_sip_header_address_t* address, const char *tag) {
	belle_sip_header_to_t* header= belle_sip_header_to_new();
	_belle_sip_object_copy((belle_sip_object_t*)header,(belle_sip_object_t*)address);
	belle_sip_header_set_name(BELLE_SIP_HEADER(header),BELLE_SIP_TO); /*restaure header name*/
	if (tag) belle_sip_header_to_set_tag(header,tag);
	return header;
}
void belle_sip_header_to_set_random_tag(belle_sip_header_to_t *obj){
	char tmp[8];
	/*not less than 32bit */
	belle_sip_header_to_set_tag(obj,belle_sip_random_token(tmp,sizeof(tmp)));
}

void belle_sip_header_to_set_tag(belle_sip_header_to_t *obj, const char *tag){
	if (tag==BELLE_SIP_RANDOM_TAG) belle_sip_header_to_set_random_tag(obj);
	else belle_sip_header_to_set_raw_tag(obj,tag);
}

const char *belle_sip_header_to_get_tag(const belle_sip_header_to_t *obj){
	return belle_sip_header_to_get_raw_tag(obj);
}


/******************************
 * User-Agent header inherits from header
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
	belle_sip_list_t* list=orig->products;
	for(;list!=NULL;list=list->next){
		belle_sip_header_user_agent_add_product(user_agent,(const char *)list->data);
	}
}
int belle_sip_header_user_agent_marshal(belle_sip_header_user_agent_t* user_agent, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	belle_sip_list_t* list = user_agent->products;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(user_agent), buff,current_offset, buff_size);
	for(;list!=NULL;list=list->next){
		current_offset+=snprintf(	buff+current_offset
									,buff_size-current_offset
									,list==user_agent->products ? "%s" : " %s"
									,(const char *)list->data);
		if (current_offset>=buff_size) goto end;
	}
end:
	return current_offset-offset;

}
BELLE_SIP_NEW_HEADER(header_user_agent,header,"User-Agent")
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
int belle_sip_header_user_agent_get_products_as_string(const belle_sip_header_user_agent_t* user_agent,char* value,unsigned int value_size) {
	int result = 0;
	belle_sip_list_t* list = user_agent->products;
	for(;list!=NULL;list=list->next){
		result+=snprintf(value+result
						,value_size-result
						,"%s "
						,(const char *)list->data);
	}
	if (result>0) value[result]='\0'; /*remove last space */

	return result-1;
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
	char* received;
};

static void belle_sip_header_via_destroy(belle_sip_header_via_t* via) {
	if (via->protocol) belle_sip_free(via->protocol);
	if (via->transport) belle_sip_free(via->transport);
	if (via->host) belle_sip_free(via->host);
	DESTROY_STRING(via,received)
}

static void belle_sip_header_via_clone(belle_sip_header_via_t* via, const belle_sip_header_via_t*orig){
	CLONE_STRING(belle_sip_header_via,protocol,via,orig)
	CLONE_STRING(belle_sip_header_via,transport,via,orig)
	CLONE_STRING(belle_sip_header_via,host,via,orig)
	CLONE_STRING(belle_sip_header_via,received,via,orig)
	via->port=orig->port;
}

int belle_sip_header_via_marshal(belle_sip_header_via_t* via, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(via), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s/%s",via->protocol,via->transport);
	if (current_offset>=buff_size) goto end;

	if (via->host) {
		if (strchr(via->host,':')) { /*ipv6*/
			current_offset+=snprintf(buff+current_offset,buff_size-current_offset," [%s]",via->host);
		} else {
			current_offset+=snprintf(buff+current_offset,buff_size-current_offset," %s",via->host);
		}
		if (current_offset>=buff_size) goto end;
	} else {
		belle_sip_warning("no host found in this via");
	}

	if (via->port > 0) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,":%i",via->port);
		if (current_offset>=buff_size) goto end;
	}
	if (via->received) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,";received=%s",via->received);
		if (current_offset>=buff_size) goto end;
	}

	current_offset+=belle_sip_parameters_marshal(&via->params_list, buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	
end:
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

BELLE_SIP_NEW_HEADER(header_via,parameters,BELLE_SIP_VIA)
BELLE_SIP_PARSE(header_via)
GET_SET_STRING(belle_sip_header_via,protocol);
GET_SET_STRING(belle_sip_header_via,transport);
GET_SET_STRING(belle_sip_header_via,host);
GET_SET_STRING(belle_sip_header_via,received);
GET_SET_INT_PRIVATE(belle_sip_header_via,port,int,_);

GET_SET_STRING_PARAM(belle_sip_header_via,branch);
GET_SET_STRING_PARAM(belle_sip_header_via,maddr);


GET_SET_INT_PARAM_PRIVATE(belle_sip_header_via,rport,int,_)
GET_SET_INT_PARAM_PRIVATE(belle_sip_header_via,ttl,int,_)

int belle_sip_header_via_set_rport (belle_sip_header_via_t* obj,int  value) {
	if (value == -1) {
		belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(obj),"rport",NULL);
		return 0;
	}
	if (value>0 && value<65536) {
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
	if (ret==0) ret=belle_sip_listening_point_get_well_known_port(via->transport);
	return ret;
}

const char* belle_sip_header_via_get_transport_lowercase(const belle_sip_header_via_t* via) {
	if (strcasecmp("udp",via->transport)==0) return "udp";
	else if (strcasecmp("tcp",via->transport)==0) return "tcp";
	else if (strcasecmp("tls",via->transport)==0) return "tls";
	else if (strcasecmp("dtls",via->transport)==0) return "dtls";
	else {
		belle_sip_warning("Cannot convert [%s] to lower case",via->transport);
		return via->transport;
	}
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
	CLONE_STRING(belle_sip_header_call_id,call_id,call_id,orig);
}
int belle_sip_header_call_id_marshal(belle_sip_header_call_id_t* call_id, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(call_id), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",call_id->call_id);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;
}
unsigned int belle_sip_header_call_id_equals(const belle_sip_header_call_id_t* a,const belle_sip_header_call_id_t* b) {
	return strcasecmp(a->call_id,b->call_id) == 0;
}
BELLE_SIP_NEW_HEADER(header_call_id,header,BELLE_SIP_CALL_ID)
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
	CLONE_STRING(belle_sip_header_cseq,method,cseq,orig)
	cseq->seq_number=orig->seq_number;
}
int belle_sip_header_cseq_marshal(belle_sip_header_cseq_t* cseq, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(cseq), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%i %s",cseq->seq_number,cseq->method);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;
}
belle_sip_header_cseq_t * belle_sip_header_cseq_create(unsigned int number, const char *method){
	belle_sip_header_cseq_t *cseq=belle_sip_header_cseq_new();
	belle_sip_header_cseq_set_method(cseq,method);
	cseq->seq_number=number;
	return cseq;
}
BELLE_SIP_NEW_HEADER(header_cseq,header,BELLE_SIP_CSEQ)
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
	CLONE_STRING(belle_sip_header_content_type,type,content_type,orig);
	CLONE_STRING(belle_sip_header_content_type,subtype,content_type,orig);
}
int belle_sip_header_content_type_marshal(belle_sip_header_content_type_t* content_type, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(content_type), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s/%s",content_type->type, content_type->subtype);
	if (current_offset>=buff_size) goto end;
	current_offset+=belle_sip_parameters_marshal(&content_type->params_list, buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	
end:
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_content_type,parameters,BELLE_SIP_CONTENT_TYPE)
BELLE_SIP_PARSE(header_content_type)
belle_sip_header_content_type_t* belle_sip_header_content_type_create (const char* type,const char* sub_type) {
	belle_sip_header_content_type_t* header = belle_sip_header_content_type_new();
	belle_sip_header_content_type_set_type(header,type);
	belle_sip_header_content_type_set_subtype(header,sub_type);
	return header;
}
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
BELLE_SIP_NEW_HEADER(header_route,header_address,BELLE_SIP_ROUTE)
BELLE_SIP_PARSE(header_route)
belle_sip_header_route_t* belle_sip_header_route_create(const belle_sip_header_address_t* route) {
	belle_sip_header_route_t* header= belle_sip_header_route_new();
	_belle_sip_object_copy((belle_sip_object_t*)header,(belle_sip_object_t*)route);
	belle_sip_header_set_name(BELLE_SIP_HEADER(header),BELLE_SIP_ROUTE); /*restore header name*/
	return header;
}
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
BELLE_SIP_NEW_HEADER(header_record_route,header_address,BELLE_SIP_RECORD_ROUTE)
BELLE_SIP_PARSE(header_record_route)
/**************************
* Service route header object inherent from header_address
****************************
*/
struct _belle_sip_header_service_route  {
	belle_sip_header_address_t address;
};

static void belle_sip_header_service_route_destroy(belle_sip_header_service_route_t* service_route) {
}

static void belle_sip_header_service_route_clone(belle_sip_header_service_route_t* service_route,
                                const belle_sip_header_service_route_t* orig               ) {
}
int belle_sip_header_service_route_marshal(belle_sip_header_service_route_t* service_route, char* buff,unsigned int offset,unsigned int buff_size) {
	BELLE_SIP_FROM_LIKE_MARSHAL(service_route)
}
BELLE_SIP_NEW_HEADER(header_service_route,header_address,BELLE_SIP_SERVICE_ROUTE)
BELLE_SIP_PARSE(header_service_route)
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
	content_length->content_length=orig->content_length;
}

int belle_sip_header_content_length_marshal(belle_sip_header_content_length_t* content_length, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(content_length), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%i",content_length->content_length);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_content_length,header,BELLE_SIP_CONTENT_LENGTH)
BELLE_SIP_PARSE(header_content_length)
GET_SET_INT(belle_sip_header_content_length,content_length,unsigned int)
belle_sip_header_content_length_t* belle_sip_header_content_length_create (int content_length)  {
	belle_sip_header_content_length_t* obj;
	obj = belle_sip_header_content_length_new();
	belle_sip_header_content_length_set_content_length(obj,content_length);
	return obj;
}
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
	expires->expires=orig->expires;
}

int belle_sip_header_expires_marshal(belle_sip_header_expires_t* expires, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(expires), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%i",expires->expires);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_expires,header,BELLE_SIP_EXPIRES)
BELLE_SIP_PARSE(header_expires)
GET_SET_INT(belle_sip_header_expires,expires,int)
belle_sip_header_expires_t* belle_sip_header_expires_create(int expires) {
	belle_sip_header_expires_t* obj = belle_sip_header_expires_new();
	belle_sip_header_expires_set_expires(obj,expires);
	return obj;
}
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
	CLONE_STRING(belle_sip_header_extension,value,extension,orig)
}
int belle_sip_header_extension_marshal(belle_sip_header_extension_t* extension, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(extension), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",extension->value);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;

}
BELLE_SIP_NEW_HEADER(header_extension,header,NULL)

belle_sip_header_extension_t* belle_sip_header_extension_create (const char* name,const char* value) {
	belle_sip_header_extension_t* ext = belle_sip_header_extension_new();
	belle_sip_header_set_name(BELLE_SIP_HEADER(ext),name);
	belle_sip_header_extension_set_value(ext,value);
	return ext;

}
/**
 * special case for this header. I don't know why
 */
belle_sip_header_extension_t* belle_sip_header_extension_parse (const char* value) {
	pANTLR3_INPUT_STREAM           input;
	pbelle_sip_messageLexer               lex;
	pANTLR3_COMMON_TOKEN_STREAM    tokens;
	pbelle_sip_messageParser              parser;
	belle_sip_messageParser_header_extension_return l_parsed_object;
	input  = antlr3StringStreamNew	(
			(pANTLR3_UINT8)value,
			ANTLR3_ENC_8BIT,
			(ANTLR3_UINT32)strlen(value),
			(pANTLR3_UINT8)"header_extension");
	lex    = belle_sip_messageLexerNew                (input);
	tokens = antlr3CommonTokenStreamSourceNew  (1025, lex->pLexer->rec->state->tokSource);
	parser = belle_sip_messageParserNew               (tokens);
	l_parsed_object = parser->header_extension(parser,FALSE);
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
	const char* opaque;




#define AUTH_BASE_DESTROY(obj) \
	if (obj->scheme) belle_sip_free((void*)obj->scheme);\
	if (obj->realm) belle_sip_free((void*)obj->realm);\
	if (obj->nonce) belle_sip_free((void*)obj->nonce);\
	if (obj->algorithm) belle_sip_free((void*)obj->algorithm);\
	if (obj->opaque) belle_sip_free((void*)obj->opaque);\

	/*if (obj->params_list) FIXME free list*/

#define AUTH_BASE_CLONE(object_type,dest,src) \
		CLONE_STRING(object_type,scheme,dest,src)\
		CLONE_STRING(object_type,realm,dest,src)\
		CLONE_STRING(object_type,nonce,dest,src)\
		CLONE_STRING(object_type,algorithm,dest,src)\
		CLONE_STRING(object_type,opaque,dest,src) \


#define AUTH_BASE_MARSHAL(header) \
	unsigned int current_offset=offset;\
	char* border=" ";\
	const belle_sip_list_t* list;\
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(header), buff,current_offset, buff_size);\
	if (current_offset>=buff_size) goto end;\
	list=belle_sip_parameters_get_parameters(&header->params_list);\
	if (header->scheme) { \
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset," %s",header->scheme);\
		if (current_offset>=buff_size) goto end;\
		} else { \
			belle_sip_error("missing mandatory scheme"); \
		} \
	for(;list!=NULL;list=list->next){\
		belle_sip_param_pair_t* container = list->data;\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s%s=%s",border, container->name,container->value);\
		if (current_offset>=buff_size) goto end;\
		border=", ";\
	}\
	if (header->realm) {\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%srealm=\"%s\"",border,header->realm);\
		if (current_offset>=buff_size) goto end;\
		border=", ";\
		}\
	if (header->nonce) {\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%snonce=\"%s\"",border,header->nonce);\
		if (current_offset>=buff_size) goto end;\
		border=", ";\
		}\
	if (header->algorithm) {\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%salgorithm=%s",border,header->algorithm);\
		if (current_offset>=buff_size) goto end;\
		border=", ";\
		}\
	if (header->opaque) {\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sopaque=\"%s\"",border,header->opaque);\
		if (current_offset>=buff_size) goto end;\
		border=", ";\
		}

struct _belle_sip_header_authorization  {
	AUTH_BASE
	const char* username;
	belle_sip_uri_t* uri;
	const char* response;
	const char* cnonce;
	int nonce_count;
	const char* qop;
};


static void belle_sip_header_authorization_destroy(belle_sip_header_authorization_t* authorization) {
	if (authorization->username) belle_sip_free((void*)authorization->username);
	if (authorization->uri) {
			belle_sip_object_unref(authorization->uri);
	}
	if (authorization->cnonce) belle_sip_free((void*)authorization->cnonce);
	AUTH_BASE_DESTROY(authorization)
	DESTROY_STRING(authorization,response);
	DESTROY_STRING(authorization,qop);
}

static void belle_sip_header_authorization_clone(belle_sip_header_authorization_t* authorization,
                                                 const belle_sip_header_authorization_t *orig ) {
	AUTH_BASE_CLONE(belle_sip_header_authorization,authorization,orig)
	CLONE_STRING(belle_sip_header_authorization,username,authorization,orig)
	if (belle_sip_header_authorization_get_uri(orig)) {
		belle_sip_header_authorization_set_uri(authorization,BELLE_SIP_URI(belle_sip_object_clone(BELLE_SIP_OBJECT(belle_sip_header_authorization_get_uri(orig)))));
	}
	CLONE_STRING(belle_sip_header_authorization,response,authorization,orig)
	CLONE_STRING(belle_sip_header_authorization,cnonce,authorization,orig)
	authorization->nonce_count=orig->nonce_count;
	CLONE_STRING(belle_sip_header_authorization,qop,authorization,orig)
}
static void belle_sip_header_authorization_init(belle_sip_header_authorization_t* authorization) {
}

belle_sip_uri_t* belle_sip_header_authorization_get_uri(const belle_sip_header_authorization_t* authorization) {
	return authorization->uri;
}

void belle_sip_header_authorization_set_uri(belle_sip_header_authorization_t* authorization, belle_sip_uri_t* uri) {
	if (uri) belle_sip_object_ref(uri);
	if (authorization->uri) {
		belle_sip_object_unref(BELLE_SIP_OBJECT(authorization->uri));
	}
	authorization->uri=uri;
}
int belle_sip_header_authorization_marshal(belle_sip_header_authorization_t* authorization, char* buff,unsigned int offset,unsigned int buff_size) {
	char nonce_count[10];
	AUTH_BASE_MARSHAL(authorization)
	if (authorization->username) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%susername=\"%s\"",border,authorization->username);
		if (current_offset>=buff_size) goto end;
		border=", ";
		}
	if (authorization->uri) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s uri=\"",border);
		if (current_offset>=buff_size) goto end;
		border=", ";
		current_offset+=belle_sip_uri_marshal(authorization->uri,buff,current_offset,buff_size);
		if (current_offset>=buff_size) goto end;
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s","\"");
		if (current_offset>=buff_size) goto end;
	}
	if (authorization->algorithm) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%salgorithm=%s",border,authorization->algorithm);
		if (current_offset>=buff_size) goto end;
		border=", ";
	}
	if (authorization->response) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sresponse=\"%s\"",border,authorization->response);
		if (current_offset>=buff_size) goto end;
		border=", ";
	}
	if (authorization->cnonce) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%scnonce=\"%s\"",border,authorization->cnonce);
		if (current_offset>=buff_size) goto end;
		border=", ";
		}
	if (authorization->nonce_count>0) {
		belle_sip_header_authorization_get_nonce_count_as_string(authorization,nonce_count);
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%snc=%s",border,nonce_count);
		if (current_offset>=buff_size) goto end;
		border=", ";
	}
	if (authorization->qop) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sqop=%s",border,authorization->qop);
		if (current_offset>=buff_size) goto end;
	}
end:
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_authorization,parameters,BELLE_SIP_AUTHORIZATION)
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

int belle_sip_header_authorization_get_nonce_count_as_string(const belle_sip_header_authorization_t* authorization,char nounce_count[9]) {
	nounce_count[0]='\0';
	if (authorization->nonce_count>0) {
		snprintf(nounce_count,9,"%08x",authorization->nonce_count);
		return 0;
	} else {
		return -1;
	}
}

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
BELLE_SIP_NEW_HEADER(header_proxy_authorization,header_authorization,BELLE_SIP_PROXY_AUTHORIZATION)
BELLE_SIP_PARSE(header_proxy_authorization)
/**************************
*WWW-Authenticate header object inherent from parameters
****************************
*/
struct _belle_sip_header_www_authenticate  {
	AUTH_BASE
	const char* domain;
	int stale;
	belle_sip_list_t* qop;
};


static void belle_sip_header_www_authenticate_destroy(belle_sip_header_www_authenticate_t* www_authenticate) {
	AUTH_BASE_DESTROY(www_authenticate)
	if (www_authenticate->domain) belle_sip_free((void*)www_authenticate->domain);
	if (www_authenticate->qop) belle_sip_list_free_with_data(www_authenticate->qop,belle_sip_free);
}
void belle_sip_header_www_authenticate_init(belle_sip_header_www_authenticate_t* www_authenticate) {
	www_authenticate->stale=-1;
}
static void belle_sip_header_www_authenticate_clone(belle_sip_header_www_authenticate_t* www_authenticate,
                                                 const belle_sip_header_www_authenticate_t *orig ) {
	AUTH_BASE_CLONE(belle_sip_header_www_authenticate,www_authenticate,orig)
	CLONE_STRING(belle_sip_header_www_authenticate,domain,www_authenticate,orig)
	www_authenticate->stale=orig->stale;
	www_authenticate->qop=belle_sip_list_copy_with_data(orig->qop,(void* (*)(void*))belle_sip_strdup);
}
int belle_sip_header_www_authenticate_marshal(belle_sip_header_www_authenticate_t* www_authenticate, char* buff,unsigned int offset,unsigned int buff_size) {
	belle_sip_list_t* qops=www_authenticate->qop;
	AUTH_BASE_MARSHAL(www_authenticate)
	if (www_authenticate->domain) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sdomain=\"%s\"",border,www_authenticate->domain);
		if (current_offset>=buff_size) goto end;
		border=", ";
	}
	if (www_authenticate->stale>=0) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sstale=%s",border,www_authenticate->stale?"true":"false");
		if (current_offset>=buff_size) goto end;
	}
	if (qops!=NULL && qops->data!=NULL) {
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%sqop=\"",border);
		if (current_offset>=buff_size) goto end;
		border="";
		for(;qops!=NULL;qops=qops->next){
			current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s%s",border, (const char*)qops->data);
			if (current_offset>=buff_size) goto end;
			border=",";
		}\
		current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"\"");
		if (current_offset>=buff_size) goto end;
		border=", ";
	}
end:
	return current_offset-offset;
}
#define SET_ADD_STRING_LIST(header,name) \
void header##_set_##name(header##_t* obj, belle_sip_list_t*  value) {\
	if (obj->name) {\
		belle_sip_list_free_with_data(obj->name,belle_sip_free);\
	} \
	obj->name=value;\
}\
void header##_add_##name(header##_t* obj, const char*  value) {\
	obj->name=belle_sip_list_append(obj->name,strdup(value));\
}

BELLE_SIP_NEW_HEADER_INIT(header_www_authenticate,parameters,BELLE_SIP_WWW_AUTHENTICATE,header_www_authenticate)
BELLE_SIP_PARSE(header_www_authenticate)
GET_SET_STRING(belle_sip_header_www_authenticate,scheme);
GET_SET_STRING(belle_sip_header_www_authenticate,realm);
GET_SET_STRING(belle_sip_header_www_authenticate,nonce);
GET_SET_STRING(belle_sip_header_www_authenticate,algorithm);
GET_SET_STRING(belle_sip_header_www_authenticate,opaque);
/*GET_SET_STRING(belle_sip_header_www_authenticate,qop);*/
SET_ADD_STRING_LIST(belle_sip_header_www_authenticate,qop)
GET_SET_STRING(belle_sip_header_www_authenticate,domain)
GET_SET_BOOL(belle_sip_header_www_authenticate,stale,is)
belle_sip_list_t* belle_sip_header_www_authenticate_get_qop(const belle_sip_header_www_authenticate_t* www_authetication) {
	return www_authetication->qop;
}
const char* belle_sip_header_www_authenticate_get_qop_first(const belle_sip_header_www_authenticate_t* www_authetication) {
	return www_authetication->qop?(const char*)www_authetication->qop->data:NULL;
}

/**************************
*Proxy-authenticate header object inherent from www_authenticate
****************************
*/
struct _belle_sip_header_proxy_authenticate  {
	belle_sip_header_www_authenticate_t www_authenticate;
};


static void belle_sip_header_proxy_authenticate_destroy(belle_sip_header_proxy_authenticate_t* proxy_authenticate) {
}

static void belle_sip_header_proxy_authenticate_clone(belle_sip_header_proxy_authenticate_t* proxy_authenticate,
                                                 const belle_sip_header_proxy_authenticate_t *orig ) {
}
int belle_sip_header_proxy_authenticate_marshal(belle_sip_header_proxy_authenticate_t* proxy_authenticate, char* buff,unsigned int offset,unsigned int buff_size) {
	return belle_sip_header_www_authenticate_marshal(&proxy_authenticate->www_authenticate,buff,offset,buff_size);
}
BELLE_SIP_NEW_HEADER(header_proxy_authenticate,header_www_authenticate,BELLE_SIP_PROXY_AUTHENTICATE)
BELLE_SIP_PARSE(header_proxy_authenticate)

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
	max_forwards->max_forwards=orig->max_forwards;
}

int belle_sip_header_max_forwards_marshal(belle_sip_header_max_forwards_t* max_forwards, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(max_forwards), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%i",max_forwards->max_forwards);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_max_forwards,header,"Max-Forwards")
BELLE_SIP_PARSE(header_max_forwards)
GET_SET_INT(belle_sip_header_max_forwards,max_forwards,int)
int belle_sip_header_max_forwards_decrement_max_forwards(belle_sip_header_max_forwards_t* max_forwards) {
	return max_forwards->max_forwards--;
}
/**************************
* Subscription state header object inherent from parameters
****************************
*/
struct _belle_sip_header_subscription_state  {
	belle_sip_parameters_t parameters;
	const char* state;
};

static void belle_sip_header_subscription_state_destroy(belle_sip_header_subscription_state_t* subscription_state) {
	DESTROY_STRING(subscription_state,state);
}

static void belle_sip_header_subscription_state_clone(belle_sip_header_subscription_state_t* subscription_state,
                                                 const belle_sip_header_subscription_state_t *orig ) {
	CLONE_STRING(belle_sip_header_subscription_state,state,subscription_state,orig)
}

int belle_sip_header_subscription_state_marshal(belle_sip_header_subscription_state_t* subscription_state, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(subscription_state), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",subscription_state->state);
	if (current_offset>=buff_size) goto end;
	current_offset+=belle_sip_parameters_marshal(BELLE_SIP_PARAMETERS(subscription_state), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_subscription_state,parameters,BELLE_SIP_SUBSCRIPTION_STATE)
BELLE_SIP_PARSE(header_subscription_state)
GET_SET_STRING(belle_sip_header_subscription_state,state);
GET_SET_STRING_PARAM(belle_sip_header_subscription_state,reason);
GET_SET_INT_PARAM2(belle_sip_header_subscription_state,retry-after,int,retry_after);
GET_SET_INT_PARAM(belle_sip_header_subscription_state,expires,int)
belle_sip_header_subscription_state_t* belle_sip_header_subscription_state_create (const char* subscription_state,int expires)  {
	belle_sip_header_subscription_state_t* sub_state=belle_sip_header_subscription_state_new();
	belle_sip_header_subscription_state_set_state(sub_state,subscription_state);
	belle_sip_header_subscription_state_set_expires(sub_state,expires);
	return sub_state;
}


#define HEADER_TO_LIKE_IMPL(name,header_name) \
struct _belle_sip_header_##name  { \
	belle_sip_header_address_t address; \
}; \
\
static void belle_sip_header_##name##_destroy(belle_sip_header_##name##_t * obj) { \
} \
void belle_sip_header_##name##_clone(belle_sip_header_##name##_t *contact, const belle_sip_header_##name##_t *orig){ }\
int belle_sip_header_##name##_marshal(belle_sip_header_##name##_t* name, char* buff,unsigned int offset,unsigned int buff_size) {\
	BELLE_SIP_FROM_LIKE_MARSHAL(name)\
}\
BELLE_SIP_NEW_HEADER(header_##name,header_address,header_name)\
BELLE_SIP_PARSE(header_##name)\
belle_sip_header_##name##_t* belle_sip_header_##name##_create(const belle_sip_header_address_t* address) { \
	belle_sip_header_##name##_t* header= belle_sip_header_##name##_new();\
	_belle_sip_object_copy((belle_sip_object_t*)header,(belle_sip_object_t*)address);\
	belle_sip_header_set_name(BELLE_SIP_HEADER(header),header_name);  \
	return header;\
}

/**************************
* Refer-To header object inherits from header_address
****************************
*/
HEADER_TO_LIKE_IMPL(refer_to,BELLE_SIP_REFER_TO)

/**************************
* Referred-By header object inherits from header_address
****************************
*/
HEADER_TO_LIKE_IMPL(referred_by,BELLE_SIP_REFERRED_BY)

/**************************
* Replaces state header object inherent from parameters
****************************
*/
struct _belle_sip_header_replaces  {
	belle_sip_parameters_t parameters;
	char* call_id;
};

static void belle_sip_header_replaces_destroy(belle_sip_header_replaces_t* replaces) {
	DESTROY_STRING(replaces,call_id);
}

static void belle_sip_header_replaces_clone(belle_sip_header_replaces_t* replaces,
                                                 const belle_sip_header_replaces_t *orig ) {
	CLONE_STRING(belle_sip_header_replaces,call_id,replaces,orig)
}

int belle_sip_header_replaces_marshal(belle_sip_header_replaces_t* replaces, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(replaces), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",replaces->call_id);
	if (current_offset>=buff_size) goto end;
	current_offset+=belle_sip_parameters_marshal(BELLE_SIP_PARAMETERS(replaces), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;
}
BELLE_SIP_NEW_HEADER(header_replaces,parameters,BELLE_SIP_REPLACES)
BELLE_SIP_PARSE(header_replaces)

GET_SET_STRING(belle_sip_header_replaces,call_id);
GET_SET_STRING_PARAM2(belle_sip_header_replaces,to-tag,to_tag);
GET_SET_STRING_PARAM2(belle_sip_header_replaces,from-tag,from_tag);

static void escaped_to_ascii(const char*a,char*b,size_t n) {
	size_t index_a=0,index_b=0;

	while (a[index_a]!='\0'&& index_a<n)
		index_a+=belle_sip_get_char(a+index_a,n-index_a,b+index_b++);
}
#define REPLACES_PREF_OFFSET 10
belle_sip_header_replaces_t* belle_sip_header_replaces_create2(const char* escaped_replace) {
	belle_sip_header_replaces_t* replaces;
	size_t len=strlen(escaped_replace);
	char* out=belle_sip_malloc(REPLACES_PREF_OFFSET+len);
	strcpy(out,BELLE_SIP_REPLACES ": ");
	escaped_to_ascii(escaped_replace,out+REPLACES_PREF_OFFSET,len);
	/*now we can parse*/
	replaces= belle_sip_header_replaces_parse(out);
	belle_sip_free(out);
	return replaces;
}
char* belle_sip_header_replaces_value_to_escaped_string(const belle_sip_header_replaces_t* replaces) {
	char buff[BELLE_SIP_MAX_TO_STRING_SIZE];
	size_t buff_size=sizeof(buff);
	unsigned int current_offset=0;
	/*first, marshall callid/from/to tags*/
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",replaces->call_id);
	if (current_offset>=buff_size) goto end;
	current_offset+=belle_sip_parameters_marshal(BELLE_SIP_PARAMETERS(replaces), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	buff[current_offset]='\0';
	return belle_sip_to_escaped_string(buff);
	
end:
	return NULL;
}

belle_sip_header_replaces_t* belle_sip_header_replaces_create(const char* call_id,const char* from_tag,const char* to_tag) {
	belle_sip_header_replaces_t* replaces=belle_sip_header_replaces_new();
	belle_sip_header_replaces_set_call_id(replaces,call_id);
	belle_sip_header_replaces_set_from_tag(replaces,from_tag);
	belle_sip_header_replaces_set_to_tag(replaces,to_tag);
	return replaces;
}

struct belle_sip_header_date{
	belle_sip_header_t base;
	char *date;
};

static void belle_sip_header_date_destroy(belle_sip_header_date_t* obj) {
	DESTROY_STRING(obj,date);
}

static void belle_sip_header_date_clone(belle_sip_header_date_t* obj,
                                                 const belle_sip_header_date_t *orig ) {
	CLONE_STRING(belle_sip_header_date,date,obj,orig);
}

int belle_sip_header_date_marshal(belle_sip_header_date_t* obj, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=belle_sip_header_marshal(BELLE_SIP_HEADER(obj), buff,current_offset, buff_size);
	if (current_offset>=buff_size) goto end;
	current_offset+=snprintf(buff+current_offset,buff_size-current_offset,"%s",obj->date);
	if (current_offset>=buff_size) goto end;
end:
	return current_offset-offset;
}

BELLE_SIP_NEW_HEADER(header_date,header,BELLE_SIP_DATE)
BELLE_SIP_PARSE(header_date)

BELLESIP_EXPORT belle_sip_header_date_t* belle_sip_header_date_create_from_time(const time_t *utc_time){
	belle_sip_header_date_t *obj=belle_sip_header_date_new();
	belle_sip_header_date_set_time(obj,utc_time);
	return obj;
}

static const char *days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

BELLESIP_EXPORT time_t belle_sip_header_date_get_time(belle_sip_header_date_t *obj){
	struct tm ret={0};
	char tmp1[16]={0};
	char tmp2[16]={0};
	int i,j;
	time_t seconds;
	
	sscanf(obj->date,"%3c,%d %16s %d %d:%d:%d",tmp1,&ret.tm_mday,tmp2,
		&ret.tm_year,&ret.tm_hour,&ret.tm_min,&ret.tm_sec);
	ret.tm_year-=1900;
	for(i=0;i<7;i++) { 
		if(strcmp(tmp1,days[i])==0) {
			ret.tm_wday=i;
			for(j=0;j<12;j++) { 
				if(strcmp(tmp2,months[j])==0) {
					ret.tm_mon=j;
					goto success;
				}
			}
		}
	}
	belle_sip_warning("Failed to parse date %s",obj->date);
	return (time_t)-1;
success:
	
	ret.tm_isdst=0;
	seconds=mktime(&ret);
	if (seconds==(time_t)-1){
		belle_sip_error("mktime() failed: %s",strerror(errno));
		return (time_t)-1;
	}
	return seconds-timezone;
}

BELLESIP_EXPORT void belle_sip_header_date_set_time(belle_sip_header_date_t *obj, const time_t *utc_time){
	
	struct tm *ret;
#ifndef WIN32
	struct tm gmt;
	ret=gmtime_r(utc_time,&gmt);
#else
	ret=gmtime(utc_time);
#endif
	/*cannot use strftime because it is locale dependant*/
	if (obj->date){
		belle_sip_free(obj->date);
	}
	obj->date=belle_sip_strdup_printf("%s, %i %s %i %02i:%02i:%02i GMT",
			days[ret->tm_wday],ret->tm_mday,months[ret->tm_mon],1900+ret->tm_year,ret->tm_hour,ret->tm_min,ret->tm_sec);
}

GET_SET_STRING(belle_sip_header_date,date);

