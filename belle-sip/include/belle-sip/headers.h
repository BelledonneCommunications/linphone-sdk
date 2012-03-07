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

#ifndef HEADERS_H_
#define HEADERS_H_

#include "belle-sip/uri.h"



/***************************************************************************************
 * header address
 *
 **************************************************************************************/

typedef struct _belle_sip_header_address belle_sip_header_address_t;

belle_sip_header_address_t* belle_sip_header_address_new();

belle_sip_header_address_t* belle_sip_header_address_parse (const char* address) ;

/**
 *
 */
belle_sip_uri_t* belle_sip_header_address_get_uri(const belle_sip_header_address_t* address);
/**
 *
 */
void belle_sip_header_address_set_uri(belle_sip_header_address_t* address, belle_sip_uri_t* uri);

/**
 *
 */
const char* belle_sip_header_address_get_displayname(const belle_sip_header_address_t* address);
/**
 *
 */
void belle_sip_header_address_set_displayname(belle_sip_header_address_t* address, const char* uri);

#define BELLE_SIP_HEADER_ADDRESS(t) BELLE_SIP_CAST(t,belle_sip_header_address_t)



/***************************************************************************************
 * header common
 *
 **************************************************************************************/

typedef struct _belle_sip_header belle_sip_header_t;
const char* belle_sip_header_get_name (const belle_sip_header_t* obj);
void belle_sip_header_set_name (belle_sip_header_t* obj,const char* value);
int belle_sip_header_marshal(belle_sip_header_t* header, char* buff, unsigned int offset,unsigned int buff_size);

#define BELLE_SIP_HEADER(t) BELLE_SIP_CAST(t,belle_sip_header_t)

/******************************
 *
 * Allow header inherit from header
 *
 ******************************/
typedef struct _belle_sip_header_allow belle_sip_header_allow_t;

belle_sip_header_allow_t* belle_sip_header_allow_new();

belle_sip_header_allow_t* belle_sip_header_allow_parse (const char* allow) ;
const char* belle_sip_header_allow_get_method(const belle_sip_header_allow_t* allow);
void belle_sip_header_allow_set_method(belle_sip_header_allow_t* allow,const char* method);
#define BELLE_SIP_HEADER_ALLOW(t) BELLE_SIP_CAST(t,belle_sip_header_allow_t)

/***********************
 * Contact header object
 ************************/
typedef struct _belle_sip_header_contact belle_sip_header_contact_t;

belle_sip_header_contact_t* belle_sip_header_contact_new();


belle_sip_header_contact_t* belle_sip_header_contact_parse (const char* contact) ;


/**
* Returns the value of the expires parameter or -1 if no expires parameter was specified or if the parameter value cannot be parsed as an int.
*@returns value of the expires parameter measured in delta-seconds, O implies removal of Registration specified in Contact Header.
*
*/
 int	belle_sip_header_contact_get_expires(const belle_sip_header_contact_t* contact);
/**
 * Returns the value of the q-value parameter of this ContactHeader. The q-value parameter indicates the relative preference amongst a set of locations. q-values are decimal numbers from 0 to 1, with higher values indicating higher preference.
 * @return the q-value parameter of this ContactHeader, -1 if the q-value is not set.
 */
 float	belle_sip_header_contact_get_qvalue(const belle_sip_header_contact_t* contact);
 /**
  * Returns a boolean value that indicates if the contact header has the format of Contact: *.
  * @return true if this is a wildcard address, false otherwise.
  */
 unsigned int belle_sip_header_contact_is_wildcard(const belle_sip_header_contact_t* contact);
 /**
 *
 */
 int belle_sip_header_contact_set_expires(belle_sip_header_contact_t* contact, int expires);
/**
 *  Sets the qValue value of the Name Address.
 */
 int belle_sip_header_contact_set_qvalue(belle_sip_header_contact_t* contact, float qvalue);
/**
 * Sets a wildcard on this contact address that is "*" is assigned to the contact header so that the header will have the format of Contact: *.
 *
 */
 void belle_sip_header_contact_set_wildcard(belle_sip_header_contact_t* contact,unsigned int is_wildcard);

#define BELLE_SIP_HEADER_CONTACT(t) BELLE_SIP_CAST(t,belle_sip_header_contact_t)

 /******************************
 * From header object inherent from header_address
 *
 ******************************/
 typedef struct _belle_sip_header_from belle_sip_header_from_t;

 belle_sip_header_from_t* belle_sip_header_from_new();

 belle_sip_header_from_t* belle_sip_header_from_create(const char *address, const char *tag);

 belle_sip_header_from_t* belle_sip_header_from_parse (const char* from) ;


 void belle_sip_header_from_set_tag(belle_sip_header_from_t* from, const char* tag);

 const char* belle_sip_header_from_get_tag(const belle_sip_header_from_t* from);

#define BELLE_SIP_HEADER_FROM(t) BELLE_SIP_CAST(t,belle_sip_header_from_t)
 /******************************
 * To header object inherent from header_address
 *
 ******************************/
 typedef struct _belle_sip_header_to belle_sip_header_to_t;

 belle_sip_header_to_t* belle_sip_header_to_new();

 belle_sip_header_to_t* belle_sip_header_to_parse(const char* to) ;

 belle_sip_header_to_t* belle_sip_header_to_create(const char *address, const char *tag);

 void belle_sip_header_to_set_tag(belle_sip_header_to_t* from, const char* tag);

 const char* belle_sip_header_to_get_tag(const belle_sip_header_to_t* from);

#define BELLE_SIP_HEADER_TO(t) BELLE_SIP_CAST(t,belle_sip_header_to_t)

/******************************
 * Via header object inherent from header_address
 *
 ******************************/
typedef struct _belle_sip_header_via belle_sip_header_via_t;

belle_sip_header_via_t* belle_sip_header_via_new();
belle_sip_header_via_t* belle_sip_header_via_create(const char *host, int port, const char *transport, const char *branch);
belle_sip_header_via_t* belle_sip_header_via_parse (const char* via) ;
const char*	belle_sip_header_via_get_branch(const belle_sip_header_via_t* via);
const char*	belle_sip_header_via_get_transport(const belle_sip_header_via_t* via);
const char*	belle_sip_header_via_get_host(const belle_sip_header_via_t* via);
int belle_sip_header_via_get_port(const belle_sip_header_via_t* via);
int belle_sip_header_via_get_listening_port(const belle_sip_header_via_t *via);

const char*	belle_sip_header_via_get_maddr(const belle_sip_header_via_t* via);
const char*	belle_sip_header_via_get_protocol(const belle_sip_header_via_t* via);
const char*	belle_sip_header_via_get_received(const belle_sip_header_via_t* via);
int belle_sip_header_via_get_rport(const belle_sip_header_via_t* via);
int	belle_sip_header_via_get_ttl(const belle_sip_header_via_t* via);

void belle_sip_header_via_set_branch(belle_sip_header_via_t* via,const char* branch);
void belle_sip_header_via_set_host(belle_sip_header_via_t* via, const char* host);
int belle_sip_header_via_set_port(belle_sip_header_via_t* via,int port);
void belle_sip_header_via_set_maddr(belle_sip_header_via_t* via, const char* maddr);
void belle_sip_header_via_set_protocol(belle_sip_header_via_t* via, const char* protocol);
void belle_sip_header_via_set_received(belle_sip_header_via_t* via, const char* received);
int belle_sip_header_via_set_rport(belle_sip_header_via_t* via,int rport);
void belle_sip_header_via_set_transport(belle_sip_header_via_t* via,const char* transport);
int belle_sip_header_via_set_ttl(belle_sip_header_via_t* via, int ttl);
#define BELLE_SIP_HEADER_VIA(t) BELLE_SIP_CAST(t,belle_sip_header_via_t)

/******************************
 * Call id object inherent from object
 *
 ******************************/
typedef struct _belle_sip_header_call_id belle_sip_header_call_id_t;

belle_sip_header_call_id_t* belle_sip_header_call_id_new();

belle_sip_header_call_id_t* belle_sip_header_call_id_parse (const char* call_id) ;
const char*	belle_sip_header_call_id_get_call_id(const belle_sip_header_call_id_t* call_id);
void belle_sip_header_call_id_set_call_id(belle_sip_header_call_id_t* via,const char* call_id);
#define BELLE_SIP_HEADER_CALL_ID(t) BELLE_SIP_CAST(t,belle_sip_header_call_id_t)
/******************************
 * cseq object inherent from object
 *
 ******************************/
typedef struct _belle_sip_header_cseq belle_sip_header_cseq_t;

belle_sip_header_cseq_t* belle_sip_header_cseq_new();
belle_sip_header_cseq_t* belle_sip_header_cseq_create(unsigned int number, const char *method);
belle_sip_header_cseq_t* belle_sip_header_cseq_parse (const char* cseq) ;
const char*	belle_sip_header_cseq_get_method(const belle_sip_header_cseq_t* cseq);
void belle_sip_header_cseq_set_method(belle_sip_header_cseq_t* cseq,const char* method);
unsigned int	belle_sip_header_cseq_get_seq_number(const belle_sip_header_cseq_t* cseq);
void belle_sip_header_cseq_set_seq_number(belle_sip_header_cseq_t* cseq,unsigned int seq_number);
#define BELLE_SIP_HEADER_CSEQ(t) BELLE_SIP_CAST(t,belle_sip_header_cseq_t)
/******************************
 * content type object inherent from parameters
 *
 ******************************/
typedef struct _belle_sip_header_content_type belle_sip_header_content_type_t;

belle_sip_header_content_type_t* belle_sip_header_content_type_new();

belle_sip_header_content_type_t* belle_sip_header_content_type_parse (const char* content_type) ;
const char*	belle_sip_header_content_type_get_type(const belle_sip_header_content_type_t* content_type);
void belle_sip_header_content_type_set_type(belle_sip_header_content_type_t* content_type,const char* type);
const char*	belle_sip_header_content_type_get_subtype(const belle_sip_header_content_type_t* content_type);
void belle_sip_header_content_type_set_subtype(belle_sip_header_content_type_t* content_type,const char* sub_type);
#define BELLE_SIP_HEADER_CONTENT_TYPE(t) BELLE_SIP_CAST(t,belle_sip_header_content_type_t)
/******************************
 *
 * Expires inherit from header
 *
 ******************************/
typedef struct _belle_sip_header_expires belle_sip_header_expires_t;

belle_sip_header_expires_t* belle_sip_header_expires_new();

belle_sip_header_expires_t* belle_sip_header_expires_parse (const char* expires) ;
int belle_sip_header_expires_get_expires(const belle_sip_header_expires_t* expires);
void belle_sip_header_expires_set_expires(belle_sip_header_expires_t* expires,int value);
int belle_sip_header_expires_decrement_expires(belle_sip_header_expires_t* expires);
belle_sip_header_expires_t* belle_sip_header_expires_create(int expires);
#define BELLE_SIP_HEADER_EXPIRES(t) BELLE_SIP_CAST(t,belle_sip_header_expires_t)

/******************************
 * Route header object inherent from header_address
 *
 ******************************/
 typedef struct _belle_sip_header_route belle_sip_header_route_t;

 belle_sip_header_route_t* belle_sip_header_route_new();
 belle_sip_header_route_t* belle_sip_header_route_parse (const char* route) ;

#define BELLE_SIP_HEADER_ROUTE(t) BELLE_SIP_CAST(t,belle_sip_header_route_t)
/******************************
 * Record route header object inherent from header_address
 *
 ******************************/
 typedef struct _belle_sip_header_record_route belle_sip_header_record_route_t;

 belle_sip_header_record_route_t* belle_sip_header_record_route_new();
 belle_sip_header_record_route_t* belle_sip_header_record_route_parse (const char* route) ;

#define BELLE_SIP_HEADER_RECORD_ROUTE(t) BELLE_SIP_CAST(t,belle_sip_header_record_route_t)
 /******************************
  *
  * user-Agent header inherit from header
  *
  ******************************/
 typedef struct _belle_sip_header_user_agent belle_sip_header_user_agent_t;

 belle_sip_header_user_agent_t* belle_sip_header_user_agent_new();

 belle_sip_header_user_agent_t* belle_sip_header_user_agent_parse (const char* user_agent) ;
 belle_sip_list_t* belle_sip_header_user_agent_get_products(const belle_sip_header_user_agent_t* user_agent);
 void belle_sip_header_user_agent_set_products(belle_sip_header_user_agent_t* user_agent,belle_sip_list_t* value);
 void belle_sip_header_user_agent_add_product(belle_sip_header_user_agent_t* user_agent,const char* product);
 #define BELLE_SIP_HEADER_USER_AGENT(t) BELLE_SIP_CAST(t,belle_sip_header_user_agent_t)

 /******************************
 * Content length inherent from object
 *
 ******************************/
typedef struct _belle_sip_header_content_length belle_sip_header_content_length_t;

belle_sip_header_content_length_t* belle_sip_header_content_length_new();

belle_sip_header_content_length_t* belle_sip_header_content_length_parse (const char* content_length) ;
unsigned int belle_sip_header_content_length_get_content_length(const belle_sip_header_content_length_t* content_length);
void belle_sip_header_content_length_set_content_length(belle_sip_header_content_length_t* content_length,unsigned int length);
#define BELLE_SIP_HEADER_CONTENT_LENGTH(t) BELLE_SIP_CAST(t,belle_sip_header_content_length_t)

/******************************
 * authorization header inherit from parameters
 *
 ******************************/
typedef struct _belle_sip_header_authorization belle_sip_header_authorization_t;

belle_sip_header_authorization_t* belle_sip_header_authorization_new();
belle_sip_header_authorization_t* belle_sip_header_authorization_parse(const char* authorization);
const char*	belle_sip_header_authorization_get_algorithm(const belle_sip_header_authorization_t* authorization );
const char*	belle_sip_header_authorization_get_cnonce(const belle_sip_header_authorization_t* authorization );
const char* belle_sip_header_authorization_get_nonce(const belle_sip_header_authorization_t* authorization);
int	belle_sip_header_authorization_get_nonce_count(const belle_sip_header_authorization_t* authorization);
const char*	belle_sip_header_authorization_get_opaque(const belle_sip_header_authorization_t* authorization);
const char*	belle_sip_header_authorization_get_qop(const belle_sip_header_authorization_t* authorization);
const char*	belle_sip_header_authorization_get_realm(const belle_sip_header_authorization_t* authorization);
const char*	belle_sip_header_authorization_get_response(const belle_sip_header_authorization_t* authorization);
const char*	belle_sip_header_authorization_get_scheme(const belle_sip_header_authorization_t* authorization);
belle_sip_uri_t* belle_sip_header_authorization_get_uri(const belle_sip_header_authorization_t* authorization);
const char*	belle_sip_header_authorization_get_username(const belle_sip_header_authorization_t* authorization);
void belle_sip_header_authorization_set_algorithm(belle_sip_header_authorization_t* authorization, const char* algorithm);
void belle_sip_header_authorization_set_cnonce(belle_sip_header_authorization_t* authorization, const char* cNonce);
void belle_sip_header_authorization_set_nonce(belle_sip_header_authorization_t* authorization, const char* nonce);
void belle_sip_header_authorization_set_nonce_count(belle_sip_header_authorization_t* authorization, int nonceCount);
void belle_sip_header_authorization_set_opaque(belle_sip_header_authorization_t* authorization, const char* opaque);
void belle_sip_header_authorization_set_qop(belle_sip_header_authorization_t* authorization, const char* qop);
void belle_sip_header_authorization_add_qop(belle_sip_header_authorization_t* authorization, const char* qop);
void belle_sip_header_authorization_set_realm(belle_sip_header_authorization_t* authorization, const char* realm);
void belle_sip_header_authorization_set_response(belle_sip_header_authorization_t* authorization, const char* response);
void belle_sip_header_authorization_set_scheme(belle_sip_header_authorization_t* authorization, const char* scheme);
void belle_sip_header_authorization_set_uri(belle_sip_header_authorization_t* authorization, belle_sip_uri_t* uri);
void belle_sip_header_authorization_set_username(belle_sip_header_authorization_t* authorization, const char* username);

#define BELLE_SIP_HEADER_AUTHORIZATION(t) BELLE_SIP_CAST(t,belle_sip_header_authorization_t)

/*******************************
 * proxy_authorization inherit from Authorization
 */
typedef struct _belle_sip_header_proxy_authorization belle_sip_header_proxy_authorization_t;
belle_sip_header_proxy_authorization_t* belle_sip_header_proxy_authorization_new();
belle_sip_header_proxy_authorization_t* belle_sip_header_proxy_authorization_parse(const char* proxy_authorization);
#define BELLE_SIP_HEADER_PROXY_AUTHORIZATION(t) BELLE_SIP_CAST(t,belle_sip_header_proxy_authorization_t)

/*******************************
 * www_authenticate inherit from parameters
 */
typedef struct _belle_sip_header_www_authenticate belle_sip_header_www_authenticate_t;
belle_sip_header_www_authenticate_t* belle_sip_header_www_authenticate_new();
belle_sip_header_www_authenticate_t* belle_sip_header_www_authenticate_parse(const char* www_authenticate);
const char*	belle_sip_header_www_authenticate_get_algorithm(const belle_sip_header_www_authenticate_t* www_authenticate );
const char* belle_sip_header_www_authenticate_get_nonce(const belle_sip_header_www_authenticate_t* www_authenticate);
const char*	belle_sip_header_www_authenticate_get_opaque(const belle_sip_header_www_authenticate_t* www_authenticate);
belle_sip_list_t* belle_sip_header_www_authenticate_get_qop(const belle_sip_header_www_authenticate_t* www_authetication);
const char*	belle_sip_header_www_authenticate_get_realm(const belle_sip_header_www_authenticate_t* www_authenticate);
const char*	belle_sip_header_www_authenticate_get_scheme(const belle_sip_header_www_authenticate_t* www_authenticate);
const char*	belle_sip_header_www_authenticate_get_domain(const belle_sip_header_www_authenticate_t* www_authenticate);
unsigned int belle_sip_header_www_authenticate_is_stale(const belle_sip_header_www_authenticate_t* www_authenticate);
void belle_sip_header_www_authenticate_set_algorithm(belle_sip_header_www_authenticate_t* www_authenticate, const char* algorithm);
void belle_sip_header_www_authenticate_set_nonce(belle_sip_header_www_authenticate_t* www_authenticate, const char* nonce);
void belle_sip_header_www_authenticate_set_opaque(belle_sip_header_www_authenticate_t* www_authenticate, const char* opaque);
void belle_sip_header_www_authenticate_set_qop(belle_sip_header_www_authenticate_t* www_authentication, belle_sip_list_t*  qop);
void belle_sip_header_www_authenticate_add_qop(belle_sip_header_www_authenticate_t* www_authentication, const char*  qop_param);
void belle_sip_header_www_authenticate_set_realm(belle_sip_header_www_authenticate_t* www_authenticate, const char* realm);
void belle_sip_header_www_authenticate_set_scheme(belle_sip_header_www_authenticate_t* www_authenticate, const char* scheme);
void belle_sip_header_www_authenticate_set_domain(belle_sip_header_www_authenticate_t* www_authenticate,const char* domain);
void belle_sip_header_www_authenticate_set_stale(belle_sip_header_www_authenticate_t* www_authenticate, unsigned int enable);
#define BELLE_SIP_HEADER_WWW_AUTHENTICATE(t) BELLE_SIP_CAST(t,belle_sip_header_www_authenticate_t)

/*******************************
 * proxy_authenticate inherit from www_authenticate
 */
typedef struct _belle_sip_header_proxy_authenticate belle_sip_header_proxy_authenticate_t;
belle_sip_header_proxy_authenticate_t* belle_sip_header_proxy_authenticate_new();
belle_sip_header_proxy_authenticate_t* belle_sip_header_proxy_authenticate_parse(const char* proxy_authenticate);
#define BELLE_SIP_HEADER_PROXY_AUTHENTICATE(t) BELLE_SIP_CAST(t,belle_sip_header_proxy_authenticate_t)


/******************************
 *
 * Extension header inherit from header
 *
 ******************************/
typedef struct _belle_sip_header_extension belle_sip_header_extension_t;

belle_sip_header_extension_t* belle_sip_header_extension_new();

belle_sip_header_extension_t* belle_sip_header_extension_parse (const char* extension) ;
const char* belle_sip_header_extension_get_value(const belle_sip_header_extension_t* extension);
void belle_sip_header_extension_set_value(belle_sip_header_extension_t* extension,const char* value);
#define BELLE_SIP_HEADER_EXTENSION(t) BELLE_SIP_CAST(t,belle_sip_header_extension_t)
/******************************
 *
 * Max forward inherit from header
 *
 ******************************/
typedef struct _belle_sip_header_max_forwards belle_sip_header_max_forwards_t;

belle_sip_header_max_forwards_t* belle_sip_header_max_forwards_new();

belle_sip_header_max_forwards_t* belle_sip_header_max_forwards_parse (const char* max_forwards) ;
int belle_sip_header_max_forwards_get_max_forwards(const belle_sip_header_max_forwards_t* max_forwards);
void belle_sip_header_max_forwards_set_max_forwards(belle_sip_header_max_forwards_t* max_forwards,int value);
int belle_sip_header_max_forwards_decrement_max_forwards(belle_sip_header_max_forwards_t* max_forwards);
#define BELLE_SIP_HEADER_MAX_FORWARDS(t) BELLE_SIP_CAST(t,belle_sip_header_max_forwards_t)


#endif /* HEADERS_H_ */
