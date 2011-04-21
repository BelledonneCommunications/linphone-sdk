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
}

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_header_t,belle_sip_object_t,belle_sip_header_destroy,NULL);


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

static void belle_sip_header_address_clone(belle_sip_header_address_t *addr){
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
	address->uri=uri;
}



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
struct _belle_sip_header_from  {
	belle_sip_header_address_t address;
};

static void belle_sip_header_from_destroy(belle_sip_header_from_t* from) {
}

static void belle_sip_header_from_clone(belle_sip_header_from_t* from, const belle_sip_header_from_t* cloned) {
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

BELLE_SIP_NEW_HEADER(header_to,header_address,"To")
BELLE_SIP_PARSE(header_to)
GET_SET_STRING_PARAM(belle_sip_header_to,tag);

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

BELLE_SIP_NEW_HEADER(header_call_id,header,"Call-ID")
BELLE_SIP_PARSE(header_call_id)
GET_SET_STRING(belle_sip_header_call_id,call_id);
/**************************
* cseq header object inherent from object
****************************
*/
struct _belle_sip_header_cseq  {
	belle_sip_header_t header;
	const char* method;
	unsigned int seq_number;
};

static void belle_sip_header_cseq_destroy(belle_sip_header_cseq_t* cseq) {
	if (cseq->method) belle_sip_free((void*)cseq->method);
}

static void belle_sip_header_cseq_clone(belle_sip_header_cseq_t* cseq, const belle_sip_header_cseq_t *orig) {
	if (cseq->method) belle_sip_free((void*)cseq->method);
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


BELLE_SIP_NEW_HEADER(header_content_length,header,"Content-Length")
BELLE_SIP_PARSE(header_content_length)
GET_SET_INT(belle_sip_header_content_length,content_length,unsigned int)
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
* content length header object inherent from object
****************************
*/
struct _belle_sip_header_authorization  {
	belle_sip_header_t header;
	const char* username;

};


static void belle_sip_header_authorization_destroy(belle_sip_header_authorization_t* authorization) {
	if (authorization->username) belle_sip_free((void*)authorization->username);
}

static void belle_sip_header_authorization_clone(belle_sip_header_authorization_t* authorization,
                                                 const belle_sip_header_authorization_t *orig ) {
}


BELLE_SIP_NEW_HEADER(header_authorization,header,"Authorization")
BELLE_SIP_PARSE(header_authorization)
GET_SET_STRING(belle_sip_header_authorization,username);

