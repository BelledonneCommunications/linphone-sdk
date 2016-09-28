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

#ifndef HEADERS_H_
#define HEADERS_H_

#include "belle-sip/defs.h"
#include "belle-sip/sip-uri.h"
#include "belle-sip/generic-uri.h"
#include "belle-sip/utils.h"
#include "belle-sip/parameters.h"

#include <time.h>

BELLE_SIP_BEGIN_DECLS


/***************************************************************************************
 * header address
 *
 **************************************************************************************/

typedef struct _belle_sip_header_address belle_sip_header_address_t;

BELLESIP_EXPORT belle_sip_header_address_t* belle_sip_header_address_new(void);
/*
 * creates an address from a display name and an uri
 * Note the uri not copied but only its ref count is incremented
 * @param  display display name. May be null.
 * @param uri uri set to the newly created header_address
 * @return
 * */
BELLESIP_EXPORT belle_sip_header_address_t* belle_sip_header_address_create(const char* display, belle_sip_uri_t* uri);

/*
 * creates an address from a display name and an absolute uri
 * Note the uri not copied but only its ref count is incremented
 * @param  display display name. May be null.
 * @param uri uri set to the newly created header_address
 * @return
 * */
BELLESIP_EXPORT belle_sip_header_address_t* belle_sip_header_address_create2(const char* display, belle_generic_uri_t* uri);


BELLESIP_EXPORT belle_sip_header_address_t* belle_sip_header_address_parse (const char* address) ;

/**
 * returns a sip uri. A header address cannot have both a sip uri and an absolute uri.
 */
BELLESIP_EXPORT belle_sip_uri_t* belle_sip_header_address_get_uri(const belle_sip_header_address_t* address);
/**
 * set an absolute uri. A header address cannot have both a sip uri and an absolute uri. This function also to absolute uri to NULL
 */
BELLESIP_EXPORT void belle_sip_header_address_set_uri(belle_sip_header_address_t* address, belle_sip_uri_t* uri);

/**
 * returns an absolute uri. A header address cannot have both a sip uri and an absolute uri.
 */
BELLESIP_EXPORT belle_generic_uri_t* belle_sip_header_address_get_absolute_uri(const belle_sip_header_address_t* address);
/**
 * set an absolute uri. A header address cannot have both a sip uri and an absolute uri. This function also to uri to NULL
 */
BELLESIP_EXPORT void belle_sip_header_address_set_absolute_uri(belle_sip_header_address_t* address, belle_generic_uri_t* uri);

/**
 *
 */
BELLESIP_EXPORT const char* belle_sip_header_address_get_displayname(const belle_sip_header_address_t* address);
/**
 *
 */
BELLESIP_EXPORT void belle_sip_header_address_set_displayname(belle_sip_header_address_t* address, const char* uri);

#define BELLE_SIP_HEADER_ADDRESS(t) BELLE_SIP_CAST(t,belle_sip_header_address_t)



/***************************************************************************************
 * header common
 *
 **************************************************************************************/

BELLESIP_EXPORT belle_sip_header_t* belle_sip_header_parse (const char* header);
BELLESIP_EXPORT belle_sip_header_t* belle_sip_header_create (const char* name,const char* value);
BELLESIP_EXPORT belle_sip_header_t* belle_http_header_create (const char* name,const char* value);
BELLESIP_EXPORT const char* belle_sip_header_get_name (const belle_sip_header_t* obj);
BELLESIP_EXPORT void belle_sip_header_set_name (belle_sip_header_t* obj,const char* value);
BELLESIP_EXPORT belle_sip_error_code belle_sip_header_marshal(belle_sip_header_t* header, char* buff, size_t buff_size, size_t *offset);
BELLESIP_EXPORT const char *belle_sip_header_get_unparsed_value(belle_sip_header_t* obj);

#define BELLE_SIP_HEADER(t) BELLE_SIP_CAST(t,belle_sip_header_t)

/******************************
 *
 * Allow header inherit from header
 *
 ******************************/
typedef struct _belle_sip_header_allow belle_sip_header_allow_t;

belle_sip_header_allow_t* belle_sip_header_allow_new(void);

BELLESIP_EXPORT belle_sip_header_allow_t* belle_sip_header_allow_parse (const char* allow) ;
BELLESIP_EXPORT belle_sip_header_allow_t* belle_sip_header_allow_create (const char* methods) ;

BELLESIP_EXPORT const char* belle_sip_header_allow_get_method(const belle_sip_header_allow_t* allow);
BELLESIP_EXPORT void belle_sip_header_allow_set_method(belle_sip_header_allow_t* allow,const char* method);
#define BELLE_SIP_HEADER_ALLOW(t) BELLE_SIP_CAST(t,belle_sip_header_allow_t)
#define BELLE_SIP_ALLOW "Allow"

/***********************
 * Contact header object
 ************************/
typedef struct _belle_sip_header_contact belle_sip_header_contact_t;

BELLESIP_EXPORT belle_sip_header_contact_t* belle_sip_header_contact_new(void);


BELLESIP_EXPORT belle_sip_header_contact_t* belle_sip_header_contact_parse (const char* contact) ;

BELLESIP_EXPORT belle_sip_header_contact_t* belle_sip_header_contact_create (const belle_sip_header_address_t* contact) ;


/**
* Returns the value of the expires parameter or -1 if no expires parameter was specified or if the parameter value cannot be parsed as an int.
*@returns value of the expires parameter measured in delta-seconds, O implies removal of Registration specified in Contact Header.
*
*/
 BELLESIP_EXPORT int	belle_sip_header_contact_get_expires(const belle_sip_header_contact_t* contact);
/**
 * Returns the value of the q-value parameter of this ContactHeader. The q-value parameter indicates the relative preference amongst a set of locations. q-values are decimal numbers from 0 to 1, with higher values indicating higher preference.
 * @return the q-value parameter of this ContactHeader, -1 if the q-value is not set.
 */
 BELLESIP_EXPORT float	belle_sip_header_contact_get_qvalue(const belle_sip_header_contact_t* contact);
 /**
  * Returns a boolean value that indicates if the contact header has the format of Contact: *.
  * @return true if this is a wildcard address, false otherwise.
  */
 BELLESIP_EXPORT unsigned int belle_sip_header_contact_is_wildcard(const belle_sip_header_contact_t* contact);
 /**
 *
 */
 BELLESIP_EXPORT int belle_sip_header_contact_set_expires(belle_sip_header_contact_t* contact, int expires);
/**
 *  Sets the qValue value of the Name Address.
 */
 BELLESIP_EXPORT int belle_sip_header_contact_set_qvalue(belle_sip_header_contact_t* contact, float qvalue);
/**
 * Sets a wildcard on this contact address that is "*" is assigned to the contact header so that the header will have the format of Contact: *.
 *
 */
 BELLESIP_EXPORT void belle_sip_header_contact_set_wildcard(belle_sip_header_contact_t* contact,unsigned int is_wildcard);
 /** Contact heaader equality function
  * @return 0 if not equals
  *
  * */
 BELLESIP_EXPORT unsigned int belle_sip_header_contact_equals(const belle_sip_header_contact_t* a,const belle_sip_header_contact_t* b);

 /** Contact heaader equality function, same as #belle_sip_header_contact_equals but return 0 if equals, very useful with #belle_sip_list
   * @return 0 if equals
   *
   * */
 BELLESIP_EXPORT unsigned int belle_sip_header_contact_not_equals(const belle_sip_header_contact_t* a,const belle_sip_header_contact_t* b);
 
 /**
  * Enable automatic filling of the contact ip, port and transport according to the channel that sends this message.
 **/
 BELLESIP_EXPORT void belle_sip_header_contact_set_automatic(belle_sip_header_contact_t *a, int enabled);
 
 BELLESIP_EXPORT int belle_sip_header_contact_get_automatic(const belle_sip_header_contact_t *a);
 
 /**
  * Indicates whether a contact in automatic mode (see belle_sip_header_contact_set_automatic()) could be filled properly when the message was sent.
  * If a message is sent through a connection that has just been initiated, public IP and port are unknown, they will be learned after receiving the first response.
  * This can be used by the upper layer to decide to resubmit the request.
 **/
 BELLESIP_EXPORT int belle_sip_header_contact_is_unknown(const belle_sip_header_contact_t *a);
 
#define BELLE_SIP_RANDOM_TAG ((const char*)-1)
#define BELLE_SIP_HEADER_CONTACT(t) BELLE_SIP_CAST(t,belle_sip_header_contact_t)
#define BELLE_SIP_CONTACT "Contact"
 /******************************
 * From header object inherent from header_address
 *
 ******************************/
 typedef struct _belle_sip_header_from belle_sip_header_from_t;

 BELLESIP_EXPORT belle_sip_header_from_t* belle_sip_header_from_new(void);

 BELLESIP_EXPORT belle_sip_header_from_t* belle_sip_header_from_create(const belle_sip_header_address_t* address, const char *tag);

 BELLESIP_EXPORT belle_sip_header_from_t* belle_sip_header_from_create2(const char *address, const char *tag);

 BELLESIP_EXPORT belle_sip_header_from_t* belle_sip_header_from_parse(const char* from) ;

 BELLESIP_EXPORT void belle_sip_header_from_set_tag(belle_sip_header_from_t* from, const char* tag);

 BELLESIP_EXPORT const char* belle_sip_header_from_get_tag(const belle_sip_header_from_t* from);

 BELLESIP_EXPORT void belle_sip_header_from_set_random_tag(belle_sip_header_from_t *obj);

#define BELLE_SIP_HEADER_FROM(t) BELLE_SIP_CAST(t,belle_sip_header_from_t)
#define BELLE_SIP_FROM "From"
 /******************************
 * To header object inherent from header_address
 *
 ******************************/
 typedef struct _belle_sip_header_to belle_sip_header_to_t;

 BELLESIP_EXPORT belle_sip_header_to_t* belle_sip_header_to_new(void);

 BELLESIP_EXPORT belle_sip_header_to_t* belle_sip_header_to_parse(const char* to) ;

 BELLESIP_EXPORT belle_sip_header_to_t* belle_sip_header_to_create(const belle_sip_header_address_t *address, const char *tag);

 BELLESIP_EXPORT belle_sip_header_to_t* belle_sip_header_to_create2(const char *address, const char *tag);

 BELLESIP_EXPORT void belle_sip_header_to_set_tag(belle_sip_header_to_t* from, const char* tag);

 BELLESIP_EXPORT const char* belle_sip_header_to_get_tag(const belle_sip_header_to_t* from);

 BELLESIP_EXPORT void belle_sip_header_to_set_random_tag(belle_sip_header_to_t *obj);

#define BELLE_SIP_HEADER_TO(t) BELLE_SIP_CAST(t,belle_sip_header_to_t)
#define BELLE_SIP_TO "To"

/******************************
 * Diversion header object inherent from header_address
 *
 ******************************/
 typedef struct _belle_sip_header_diversion belle_sip_header_diversion_t;

 BELLESIP_EXPORT belle_sip_header_diversion_t* belle_sip_header_diversion_new(void);

 BELLESIP_EXPORT belle_sip_header_diversion_t* belle_sip_header_diversion_parse(const char* diversion) ;

 BELLESIP_EXPORT belle_sip_header_diversion_t* belle_sip_header_diversion_create(const belle_sip_header_address_t *address, const char *tag);

 BELLESIP_EXPORT belle_sip_header_diversion_t* belle_sip_header_diversion_create2(const char *address, const char *tag);

 BELLESIP_EXPORT void belle_sip_header_diversion_set_tag(belle_sip_header_diversion_t* diversion, const char* tag);

 BELLESIP_EXPORT const char* belle_sip_header_diversion_get_tag(const belle_sip_header_diversion_t* from);

 BELLESIP_EXPORT void belle_sip_header_diversion_set_random_tag(belle_sip_header_diversion_t *obj);

#define BELLE_SIP_HEADER_DIVERSION(t) BELLE_SIP_CAST(t,belle_sip_header_diversion_t)
#define BELLE_SIP_DIVERSION "Diversion"

/******************************
 * Via header object inherent from header_address
 *
 ******************************/
typedef struct _belle_sip_header_via belle_sip_header_via_t;

BELLESIP_EXPORT belle_sip_header_via_t* belle_sip_header_via_new(void);
BELLESIP_EXPORT belle_sip_header_via_t* belle_sip_header_via_create(const char *host, int port, const char *transport, const char *branch);
BELLESIP_EXPORT belle_sip_header_via_t* belle_sip_header_via_parse (const char* via) ;
BELLESIP_EXPORT const char*	belle_sip_header_via_get_branch(const belle_sip_header_via_t* via);
BELLESIP_EXPORT const char*	belle_sip_header_via_get_transport(const belle_sip_header_via_t* via);
/**
 * Get lower case version of the transport
 * @return the lower case version of the transport if from tcp,udp,tls or dtls else, return the value from #belle_sip_header_via_get_transport
 */
BELLESIP_EXPORT const char*	belle_sip_header_via_get_transport_lowercase(const belle_sip_header_via_t* via);
BELLESIP_EXPORT const char*	belle_sip_header_via_get_host(const belle_sip_header_via_t* via);
BELLESIP_EXPORT int belle_sip_header_via_get_port(const belle_sip_header_via_t* via);
BELLESIP_EXPORT int belle_sip_header_via_get_listening_port(const belle_sip_header_via_t *via);

BELLESIP_EXPORT const char*	belle_sip_header_via_get_maddr(const belle_sip_header_via_t* via);
BELLESIP_EXPORT const char*	belle_sip_header_via_get_protocol(const belle_sip_header_via_t* via);
BELLESIP_EXPORT const char*	belle_sip_header_via_get_received(const belle_sip_header_via_t* via);
BELLESIP_EXPORT int belle_sip_header_via_get_rport(const belle_sip_header_via_t* via);
BELLESIP_EXPORT int	belle_sip_header_via_get_ttl(const belle_sip_header_via_t* via);

BELLESIP_EXPORT void belle_sip_header_via_set_branch(belle_sip_header_via_t* via,const char* branch);
BELLESIP_EXPORT void belle_sip_header_via_set_host(belle_sip_header_via_t* via, const char* host);
BELLESIP_EXPORT int belle_sip_header_via_set_port(belle_sip_header_via_t* via,int port);
BELLESIP_EXPORT void belle_sip_header_via_set_maddr(belle_sip_header_via_t* via, const char* maddr);
BELLESIP_EXPORT void belle_sip_header_via_set_protocol(belle_sip_header_via_t* via, const char* protocol);
BELLESIP_EXPORT void belle_sip_header_via_set_received(belle_sip_header_via_t* via, const char* received);
BELLESIP_EXPORT int belle_sip_header_via_set_rport(belle_sip_header_via_t* via,int rport);
BELLESIP_EXPORT void belle_sip_header_via_set_transport(belle_sip_header_via_t* via,const char* transport);
BELLESIP_EXPORT int belle_sip_header_via_set_ttl(belle_sip_header_via_t* via, int ttl);
#define BELLE_SIP_HEADER_VIA(t) BELLE_SIP_CAST(t,belle_sip_header_via_t)
#define BELLE_SIP_VIA "Via"

/******************************
 * Call id object inherent from object
 *
 ******************************/
typedef struct _belle_sip_header_call_id belle_sip_header_call_id_t;

BELLESIP_EXPORT belle_sip_header_call_id_t* belle_sip_header_call_id_new(void);

BELLESIP_EXPORT belle_sip_header_call_id_t* belle_sip_header_call_id_parse (const char* call_id) ;
BELLESIP_EXPORT const char*	belle_sip_header_call_id_get_call_id(const belle_sip_header_call_id_t* call_id);
BELLESIP_EXPORT void belle_sip_header_call_id_set_call_id(belle_sip_header_call_id_t* call_id,const char* id);
unsigned int belle_sip_header_call_id_equals(const belle_sip_header_call_id_t* a,const belle_sip_header_call_id_t* b);
#define BELLE_SIP_HEADER_CALL_ID(t) BELLE_SIP_CAST(t,belle_sip_header_call_id_t)
#define BELLE_SIP_CALL_ID "Call-ID"
/******************************
 * cseq object inherent from object
 *
 ******************************/
typedef struct _belle_sip_header_cseq belle_sip_header_cseq_t;

BELLESIP_EXPORT belle_sip_header_cseq_t* belle_sip_header_cseq_new(void);
BELLESIP_EXPORT belle_sip_header_cseq_t* belle_sip_header_cseq_create(unsigned int number, const char *method);
BELLESIP_EXPORT belle_sip_header_cseq_t* belle_sip_header_cseq_parse (const char* cseq) ;
BELLESIP_EXPORT const char*	belle_sip_header_cseq_get_method(const belle_sip_header_cseq_t* cseq);
BELLESIP_EXPORT void belle_sip_header_cseq_set_method(belle_sip_header_cseq_t* cseq,const char* method);
BELLESIP_EXPORT unsigned int	belle_sip_header_cseq_get_seq_number(const belle_sip_header_cseq_t* cseq);
BELLESIP_EXPORT void belle_sip_header_cseq_set_seq_number(belle_sip_header_cseq_t* cseq,unsigned int seq_number);
#define BELLE_SIP_HEADER_CSEQ(t) BELLE_SIP_CAST(t,belle_sip_header_cseq_t)
#define BELLE_SIP_CSEQ "CSeq"
/******************************
 * content type object inherent from parameters
 *
 ******************************/
typedef struct _belle_sip_header_content_type belle_sip_header_content_type_t;

BELLESIP_EXPORT belle_sip_header_content_type_t* belle_sip_header_content_type_new(void);
BELLESIP_EXPORT belle_sip_header_content_type_t* belle_sip_header_content_type_parse (const char* content_type) ;
BELLESIP_EXPORT belle_sip_header_content_type_t* belle_sip_header_content_type_create (const char* type,const char* sub_type) ;

BELLESIP_EXPORT belle_sip_header_content_type_t* belle_sip_header_content_type_parse (const char* content_type) ;
BELLESIP_EXPORT const char*	belle_sip_header_content_type_get_type(const belle_sip_header_content_type_t* content_type);
BELLESIP_EXPORT void belle_sip_header_content_type_set_type(belle_sip_header_content_type_t* content_type,const char* type);
BELLESIP_EXPORT const char*	belle_sip_header_content_type_get_subtype(const belle_sip_header_content_type_t* content_type);
BELLESIP_EXPORT void belle_sip_header_content_type_set_subtype(belle_sip_header_content_type_t* content_type,const char* sub_type);
#define BELLE_SIP_HEADER_CONTENT_TYPE(t) BELLE_SIP_CAST(t,belle_sip_header_content_type_t)
#define BELLE_SIP_CONTENT_TYPE "Content-Type"
/******************************
 *
 * Expires inherit from header
 *
 ******************************/
typedef struct _belle_sip_header_expires belle_sip_header_expires_t;

BELLESIP_EXPORT belle_sip_header_expires_t* belle_sip_header_expires_new(void);

BELLESIP_EXPORT belle_sip_header_expires_t* belle_sip_header_expires_parse (const char* expires) ;
BELLESIP_EXPORT int belle_sip_header_expires_get_expires(const belle_sip_header_expires_t* expires);
BELLESIP_EXPORT void belle_sip_header_expires_set_expires(belle_sip_header_expires_t* expires,int value);
BELLESIP_EXPORT int belle_sip_header_expires_decrement_expires(belle_sip_header_expires_t* expires);
BELLESIP_EXPORT belle_sip_header_expires_t* belle_sip_header_expires_create(int expires);
#define BELLE_SIP_HEADER_EXPIRES(t) BELLE_SIP_CAST(t,belle_sip_header_expires_t)
#define BELLE_SIP_EXPIRES "Expires"
/******************************
 * Route header object inherent from header_address
 *
 ******************************/
typedef struct _belle_sip_header_route belle_sip_header_route_t;

BELLESIP_EXPORT belle_sip_header_route_t* belle_sip_header_route_new(void);
BELLESIP_EXPORT belle_sip_header_route_t* belle_sip_header_route_parse (const char* route) ;
BELLESIP_EXPORT belle_sip_header_route_t* belle_sip_header_route_create(const belle_sip_header_address_t* route);

#define BELLE_SIP_HEADER_ROUTE(t) BELLE_SIP_CAST(t,belle_sip_header_route_t)
#define BELLE_SIP_ROUTE "Route"
/******************************
 * Record route header object inherent from header_address
 *
 ******************************/
typedef struct _belle_sip_header_record_route belle_sip_header_record_route_t;

BELLESIP_EXPORT belle_sip_header_record_route_t* belle_sip_header_record_route_new(void);
BELLESIP_EXPORT belle_sip_header_record_route_t* belle_sip_header_record_route_parse (const char* route);
BELLESIP_EXPORT belle_sip_header_record_route_t* belle_sip_header_record_route_new_auto_outgoing(void);

BELLESIP_EXPORT unsigned char belle_sip_header_record_route_get_auto_outgoing(const belle_sip_header_record_route_t *a);
 
 

#define BELLE_SIP_HEADER_RECORD_ROUTE(t) BELLE_SIP_CAST(t,belle_sip_header_record_route_t)
#define BELLE_SIP_RECORD_ROUTE	"Record-route"
 /******************************
  * Service route header object inherent from header_address
  *
  ******************************/
  typedef struct _belle_sip_header_service_route belle_sip_header_service_route_t;

  BELLESIP_EXPORT belle_sip_header_service_route_t* belle_sip_header_service_route_new(void);
  BELLESIP_EXPORT belle_sip_header_service_route_t* belle_sip_header_service_route_parse (const char* route) ;

 #define BELLE_SIP_HEADER_SERVICE_ROUTE(t) BELLE_SIP_CAST(t,belle_sip_header_service_route_t)
 #define BELLE_SIP_SERVICE_ROUTE	"Service-route"
 /******************************
  *
  * user-Agent header inherit from header
  *
  ******************************/
 typedef struct _belle_sip_header_user_agent belle_sip_header_user_agent_t;

 BELLESIP_EXPORT belle_sip_header_user_agent_t* belle_sip_header_user_agent_new(void);

 BELLESIP_EXPORT belle_sip_header_user_agent_t* belle_sip_header_user_agent_parse (const char* user_agent) ;
 BELLESIP_EXPORT belle_sip_list_t* belle_sip_header_user_agent_get_products(const belle_sip_header_user_agent_t* user_agent);
 /**
  * concatenates products
  * @param user_agent [in] user agent header
  * @param value [out]buffer where to put result in
  * @param value_size [in] size of the buffer
  * @return number of written characters or -1 inca se of error;
  */
 BELLESIP_EXPORT int belle_sip_header_user_agent_get_products_as_string(const belle_sip_header_user_agent_t* user_agent,char* value,unsigned int value_size);
 BELLESIP_EXPORT void belle_sip_header_user_agent_set_products(belle_sip_header_user_agent_t* user_agent,belle_sip_list_t* value);
 BELLESIP_EXPORT void belle_sip_header_user_agent_add_product(belle_sip_header_user_agent_t* user_agent,const char* product);
 #define BELLE_SIP_HEADER_USER_AGENT(t) BELLE_SIP_CAST(t,belle_sip_header_user_agent_t)
#define BELLE_SIP_USER_AGENT "User-Agent"

 /******************************
 * Content length inherent from object
 *
 ******************************/
typedef struct _belle_sip_header_content_length belle_sip_header_content_length_t;

BELLESIP_EXPORT belle_sip_header_content_length_t* belle_sip_header_content_length_new(void);

BELLESIP_EXPORT belle_sip_header_content_length_t* belle_sip_header_content_length_parse (const char* content_length) ;
BELLESIP_EXPORT belle_sip_header_content_length_t* belle_sip_header_content_length_create (size_t content_length) ;
BELLESIP_EXPORT size_t belle_sip_header_content_length_get_content_length(const belle_sip_header_content_length_t* content_length);
BELLESIP_EXPORT void belle_sip_header_content_length_set_content_length(belle_sip_header_content_length_t* content_length,size_t length);
#define BELLE_SIP_HEADER_CONTENT_LENGTH(t) BELLE_SIP_CAST(t,belle_sip_header_content_length_t)
#define BELLE_SIP_CONTENT_LENGTH "Content-Length"

/******************************
 * authorization header inherit from parameters
 *
 ******************************/
typedef struct _belle_sip_header_authorization belle_sip_header_authorization_t;

BELLESIP_EXPORT belle_sip_header_authorization_t* belle_sip_header_authorization_new(void);
BELLESIP_EXPORT belle_sip_header_authorization_t* belle_sip_header_authorization_parse(const char* authorization);
BELLESIP_EXPORT const char*	belle_sip_header_authorization_get_algorithm(const belle_sip_header_authorization_t* authorization );
BELLESIP_EXPORT const char*	belle_sip_header_authorization_get_cnonce(const belle_sip_header_authorization_t* authorization );
BELLESIP_EXPORT const char* belle_sip_header_authorization_get_nonce(const belle_sip_header_authorization_t* authorization);
/*convert nonce count as string id present
 * @return 0 in case of success
 * */
BELLESIP_EXPORT int belle_sip_header_authorization_get_nonce_count_as_string(const belle_sip_header_authorization_t* authorization,char nounce_count[9]);
BELLESIP_EXPORT int	belle_sip_header_authorization_get_nonce_count(const belle_sip_header_authorization_t* authorization);
BELLESIP_EXPORT const char*	belle_sip_header_authorization_get_opaque(const belle_sip_header_authorization_t* authorization);
BELLESIP_EXPORT const char*	belle_sip_header_authorization_get_qop(const belle_sip_header_authorization_t* authorization);
BELLESIP_EXPORT const char*	belle_sip_header_authorization_get_realm(const belle_sip_header_authorization_t* authorization);
BELLESIP_EXPORT const char*	belle_sip_header_authorization_get_response(const belle_sip_header_authorization_t* authorization);
BELLESIP_EXPORT const char*	belle_sip_header_authorization_get_scheme(const belle_sip_header_authorization_t* authorization);
BELLESIP_EXPORT belle_sip_uri_t* belle_sip_header_authorization_get_uri(const belle_sip_header_authorization_t* authorization);
BELLESIP_EXPORT const char*	belle_sip_header_authorization_get_username(const belle_sip_header_authorization_t* authorization);
BELLESIP_EXPORT void belle_sip_header_authorization_set_algorithm(belle_sip_header_authorization_t* authorization, const char* algorithm);
BELLESIP_EXPORT void belle_sip_header_authorization_set_cnonce(belle_sip_header_authorization_t* authorization, const char* cNonce);
BELLESIP_EXPORT void belle_sip_header_authorization_set_nonce(belle_sip_header_authorization_t* authorization, const char* nonce);
BELLESIP_EXPORT void belle_sip_header_authorization_set_nonce_count(belle_sip_header_authorization_t* authorization, int nonceCount);
BELLESIP_EXPORT void belle_sip_header_authorization_set_opaque(belle_sip_header_authorization_t* authorization, const char* opaque);
BELLESIP_EXPORT void belle_sip_header_authorization_set_qop(belle_sip_header_authorization_t* authorization, const char* qop);
BELLESIP_EXPORT void belle_sip_header_authorization_add_qop(belle_sip_header_authorization_t* authorization, const char* qop);
BELLESIP_EXPORT void belle_sip_header_authorization_set_realm(belle_sip_header_authorization_t* authorization, const char* realm);
BELLESIP_EXPORT void belle_sip_header_authorization_set_response(belle_sip_header_authorization_t* authorization, const char* response);
BELLESIP_EXPORT void belle_sip_header_authorization_set_scheme(belle_sip_header_authorization_t* authorization, const char* scheme);
BELLESIP_EXPORT void belle_sip_header_authorization_set_uri(belle_sip_header_authorization_t* authorization, belle_sip_uri_t* uri);
BELLESIP_EXPORT void belle_sip_header_authorization_set_username(belle_sip_header_authorization_t* authorization, const char* username);

#define BELLE_SIP_HEADER_AUTHORIZATION(t) BELLE_SIP_CAST(t,belle_sip_header_authorization_t)
#define BELLE_SIP_AUTHORIZATION "Authorization"

/*******************************
 * proxy_authorization inherit from Authorization
 */
typedef struct _belle_sip_header_proxy_authorization belle_sip_header_proxy_authorization_t;
BELLESIP_EXPORT belle_sip_header_proxy_authorization_t* belle_sip_header_proxy_authorization_new(void);
BELLESIP_EXPORT belle_sip_header_proxy_authorization_t* belle_sip_header_proxy_authorization_parse(const char* proxy_authorization);
#define BELLE_SIP_HEADER_PROXY_AUTHORIZATION(t) BELLE_SIP_CAST(t,belle_sip_header_proxy_authorization_t)
#define BELLE_SIP_PROXY_AUTHORIZATION "Proxy-Authorization"

/*******************************
 * http_authorization inherit from Authorization
 */
typedef struct _belle_http_header_authorization belle_http_header_authorization_t;
BELLESIP_EXPORT belle_http_header_authorization_t* belle_http_header_authorization_new(void);
/*cannot be parsed for now
BELLESIP_EXPORT belle_http_header_authorization_t* belle_http_header_authorization_parse(const char* proxy_authorization);
*/
BELLESIP_EXPORT void belle_http_header_authorization_set_uri(belle_http_header_authorization_t* authorization, belle_generic_uri_t* uri);
BELLESIP_EXPORT belle_generic_uri_t* belle_http_header_authorization_get_uri(const belle_http_header_authorization_t* authorization);
#define BELLE_HTTP_HEADER_AUTHORIZATION(t) BELLE_SIP_CAST(t,belle_http_header_authorization_t)
#define BELLE_HTTP_AUTHORIZATION "Authorization"


/*******************************
 * www_authenticate inherit from parameters
 */
typedef struct _belle_sip_header_www_authenticate belle_sip_header_www_authenticate_t;
BELLESIP_EXPORT belle_sip_header_www_authenticate_t* belle_sip_header_www_authenticate_new(void);
BELLESIP_EXPORT belle_sip_header_www_authenticate_t* belle_sip_header_www_authenticate_parse(const char* www_authenticate);
BELLESIP_EXPORT const char*	belle_sip_header_www_authenticate_get_algorithm(const belle_sip_header_www_authenticate_t* www_authenticate );
BELLESIP_EXPORT const char* belle_sip_header_www_authenticate_get_nonce(const belle_sip_header_www_authenticate_t* www_authenticate);
BELLESIP_EXPORT const char*	belle_sip_header_www_authenticate_get_opaque(const belle_sip_header_www_authenticate_t* www_authenticate);
BELLESIP_EXPORT belle_sip_list_t* belle_sip_header_www_authenticate_get_qop(const belle_sip_header_www_authenticate_t* www_authetication);
BELLESIP_EXPORT const char* belle_sip_header_www_authenticate_get_qop_first(const belle_sip_header_www_authenticate_t* www_authetication);
BELLESIP_EXPORT const char*	belle_sip_header_www_authenticate_get_realm(const belle_sip_header_www_authenticate_t* www_authenticate);
BELLESIP_EXPORT const char*	belle_sip_header_www_authenticate_get_scheme(const belle_sip_header_www_authenticate_t* www_authenticate);
BELLESIP_EXPORT const char*	belle_sip_header_www_authenticate_get_domain(const belle_sip_header_www_authenticate_t* www_authenticate);
BELLESIP_EXPORT unsigned int belle_sip_header_www_authenticate_is_stale(const belle_sip_header_www_authenticate_t* www_authenticate);
BELLESIP_EXPORT void belle_sip_header_www_authenticate_set_algorithm(belle_sip_header_www_authenticate_t* www_authenticate, const char* algorithm);
BELLESIP_EXPORT void belle_sip_header_www_authenticate_set_nonce(belle_sip_header_www_authenticate_t* www_authenticate, const char* nonce);
BELLESIP_EXPORT void belle_sip_header_www_authenticate_set_opaque(belle_sip_header_www_authenticate_t* www_authenticate, const char* opaque);
BELLESIP_EXPORT void belle_sip_header_www_authenticate_set_qop(belle_sip_header_www_authenticate_t* www_authentication, belle_sip_list_t*  qop);
BELLESIP_EXPORT void belle_sip_header_www_authenticate_add_qop(belle_sip_header_www_authenticate_t* www_authentication, const char*  qop_param);
BELLESIP_EXPORT void belle_sip_header_www_authenticate_set_realm(belle_sip_header_www_authenticate_t* www_authenticate, const char* realm);
BELLESIP_EXPORT void belle_sip_header_www_authenticate_set_scheme(belle_sip_header_www_authenticate_t* www_authenticate, const char* scheme);
BELLESIP_EXPORT void belle_sip_header_www_authenticate_set_domain(belle_sip_header_www_authenticate_t* www_authenticate,const char* domain);
BELLESIP_EXPORT void belle_sip_header_www_authenticate_set_stale(belle_sip_header_www_authenticate_t* www_authenticate, unsigned int enable);
#define BELLE_SIP_HEADER_WWW_AUTHENTICATE(t) BELLE_SIP_CAST(t,belle_sip_header_www_authenticate_t)
#define BELLE_SIP_WWW_AUTHENTICATE "WWW-Authenticate"

/*******************************
 * proxy_authenticate inherit from www_authenticate
 */
typedef struct _belle_sip_header_proxy_authenticate belle_sip_header_proxy_authenticate_t;
BELLESIP_EXPORT belle_sip_header_proxy_authenticate_t* belle_sip_header_proxy_authenticate_new(void);
BELLESIP_EXPORT belle_sip_header_proxy_authenticate_t* belle_sip_header_proxy_authenticate_parse(const char* proxy_authenticate);
#define BELLE_SIP_HEADER_PROXY_AUTHENTICATE(t) BELLE_SIP_CAST(t,belle_sip_header_proxy_authenticate_t)
#define BELLE_SIP_PROXY_AUTHENTICATE "Proxy-Authenticate"

/******************************
 *
 * Max forward inherit from header
 *
 ******************************/
typedef struct _belle_sip_header_max_forwards belle_sip_header_max_forwards_t;

BELLESIP_EXPORT belle_sip_header_max_forwards_t* belle_sip_header_max_forwards_new(void);
BELLESIP_EXPORT belle_sip_header_max_forwards_t* belle_sip_header_max_forwards_create(int value);

BELLESIP_EXPORT belle_sip_header_max_forwards_t* belle_sip_header_max_forwards_parse (const char* max_forwards) ;
BELLESIP_EXPORT int belle_sip_header_max_forwards_get_max_forwards(const belle_sip_header_max_forwards_t* max_forwards);
BELLESIP_EXPORT void belle_sip_header_max_forwards_set_max_forwards(belle_sip_header_max_forwards_t* max_forwards,int value);
BELLESIP_EXPORT int belle_sip_header_max_forwards_decrement_max_forwards(belle_sip_header_max_forwards_t* max_forwards);
#define BELLE_SIP_HEADER_MAX_FORWARDS(t) BELLE_SIP_CAST(t,belle_sip_header_max_forwards_t)
#define BELLE_SIP_MAX_FORWARDS "Max-Forwards"

/******************************
 *
 * Subscription state  inherit from parameters
 *
 ******************************/
typedef struct _belle_sip_header_subscription_state belle_sip_header_subscription_state_t;

BELLESIP_EXPORT belle_sip_header_subscription_state_t* belle_sip_header_subscription_state_new(void);

BELLESIP_EXPORT belle_sip_header_subscription_state_t* belle_sip_header_subscription_state_parse (const char* subscription_state) ;
BELLESIP_EXPORT belle_sip_header_subscription_state_t* belle_sip_header_subscription_state_create (const char* subscription_state,int expires);

BELLESIP_EXPORT const char* belle_sip_header_subscription_state_get_state(const belle_sip_header_subscription_state_t* subscription_state);
BELLESIP_EXPORT int belle_sip_header_subscription_state_get_expires(const belle_sip_header_subscription_state_t* subscription_state);
BELLESIP_EXPORT const char* belle_sip_header_subscription_state_get_reason(const belle_sip_header_subscription_state_t* subscription_state);
BELLESIP_EXPORT int belle_sip_header_subscription_state_get_retry_after(const belle_sip_header_subscription_state_t* subscription_state);

BELLESIP_EXPORT void belle_sip_header_subscription_state_set_state(belle_sip_header_subscription_state_t* subscription_state,const char* state);
BELLESIP_EXPORT void belle_sip_header_subscription_state_set_expires(belle_sip_header_subscription_state_t* subscription_state,int expire);
BELLESIP_EXPORT void belle_sip_header_subscription_state_set_reason(belle_sip_header_subscription_state_t* subscription_state, const char* reason);
BELLESIP_EXPORT void belle_sip_header_subscription_state_set_retry_after(belle_sip_header_subscription_state_t* subscription_state, int retry_after );


#define BELLE_SIP_HEADER_SUBSCRIPTION_STATE(t) BELLE_SIP_CAST(t,belle_sip_header_subscription_state_t)
#define BELLE_SIP_SUBSCRIPTION_STATE "Subscription-State"
#define BELLE_SIP_SUBSCRIPTION_STATE_ACTIVE  "active"
#define BELLE_SIP_SUBSCRIPTION_STATE_PENDING "pending"
#define BELLE_SIP_SUBSCRIPTION_STATE_TERMINATED "terminated"

/******************************
 * Refer-To header object inherent from header_address
 *
 ******************************/
 typedef struct _belle_sip_header_refer_to belle_sip_header_refer_to_t;
 BELLESIP_EXPORT belle_sip_header_refer_to_t* belle_sip_header_refer_to_new(void);
 BELLESIP_EXPORT belle_sip_header_refer_to_t* belle_sip_header_refer_to_parse(const char* refer_to) ;
 BELLESIP_EXPORT belle_sip_header_refer_to_t* belle_sip_header_refer_to_create(const belle_sip_header_address_t *address);
#define BELLE_SIP_HEADER_REFER_TO(t) BELLE_SIP_CAST(t,belle_sip_header_refer_to_t)
#define BELLE_SIP_REFER_TO "Refer-To"

 /******************************
  * Referred-by header object inherent from header_address
  *
  ******************************/
  typedef struct _belle_sip_header_referred_by belle_sip_header_referred_by_t;
  BELLESIP_EXPORT belle_sip_header_referred_by_t* belle_sip_header_referred_by_new(void);
  BELLESIP_EXPORT belle_sip_header_referred_by_t* belle_sip_header_referred_by_parse(const char* referred_by) ;
  BELLESIP_EXPORT belle_sip_header_referred_by_t* belle_sip_header_referred_by_create(const belle_sip_header_address_t *address);
 #define BELLE_SIP_HEADER_REFERRED_BY(t) BELLE_SIP_CAST(t,belle_sip_header_referred_by_t)
 #define BELLE_SIP_REFERRED_BY "Referred-By"

  /******************************
   * Replace header object inherent from parameters
   *
   ******************************/
typedef struct _belle_sip_header_replaces belle_sip_header_replaces_t;
BELLESIP_EXPORT belle_sip_header_replaces_t* belle_sip_header_replaces_new(void);
BELLESIP_EXPORT belle_sip_header_replaces_t* belle_sip_header_replaces_parse(const char* replaces) ;

BELLESIP_EXPORT belle_sip_header_replaces_t* belle_sip_header_replaces_create(const char* call_id,const char* from_tag,const char* to_tag);
/*
 * Creates a Eeplaces header from an escaped value that can be found in Referred-by header
 * @param escaped_replace ex : 12345%40192.168.118.3%3Bto-tag%3D12345%3Bfrom-tag%3D5FFE-3994
 * @return a newly allocated Replace header
 * */
BELLESIP_EXPORT belle_sip_header_replaces_t* belle_sip_header_replaces_create2(const char* escaped_replace);
BELLESIP_EXPORT const char* belle_sip_header_replaces_get_call_id(const belle_sip_header_replaces_t* obj);
BELLESIP_EXPORT const char* belle_sip_header_replaces_get_from_tag(const belle_sip_header_replaces_t* obj);
BELLESIP_EXPORT const char* belle_sip_header_replaces_get_to_tag(const belle_sip_header_replaces_t* obj);
BELLESIP_EXPORT void belle_sip_header_replaces_set_call_id(belle_sip_header_replaces_t* obj, const char* callid);
BELLESIP_EXPORT void belle_sip_header_replaces_set_from_tag(belle_sip_header_replaces_t* obj,const char* from_tag);
BELLESIP_EXPORT void belle_sip_header_replaces_set_to_tag(belle_sip_header_replaces_t* obj,const char* to_tag);
/*return a newly allocated string with the content of the header value in escaped form.
 * <br> Purpose of this function is to be used to set Refer-To uri header Replaces
 * @param obj Replaces object
 * @return newly allocated string ex: 12345%40192.168.118.3%3Bto-tag%3D12345%3Bfrom-tag%3D5FFE-3994*/
BELLESIP_EXPORT char* belle_sip_header_replaces_value_to_escaped_string(const belle_sip_header_replaces_t* obj);
#define BELLE_SIP_HEADER_REPLACES(t) BELLE_SIP_CAST(t,belle_sip_header_replaces_t)
#define BELLE_SIP_REPLACES "Replaces"

/*******
 * Date header
 *******/

typedef struct belle_sip_header_date belle_sip_header_date_t;

BELLESIP_EXPORT belle_sip_header_date_t* belle_sip_header_date_new(void);
BELLESIP_EXPORT belle_sip_header_date_t* belle_sip_header_date_parse(const char* date) ;

BELLESIP_EXPORT belle_sip_header_date_t* belle_sip_header_date_create_from_time(const time_t *utc_time);

BELLESIP_EXPORT time_t belle_sip_header_date_get_time(belle_sip_header_date_t *obj);

BELLESIP_EXPORT void belle_sip_header_date_set_time(belle_sip_header_date_t *obj, const time_t *utc_time);

BELLESIP_EXPORT const char * belle_sip_header_date_get_date(const belle_sip_header_date_t *obj);

BELLESIP_EXPORT void belle_sip_header_date_set_date(belle_sip_header_date_t *obj, const char *date);

#define BELLE_SIP_HEADER_DATE(obj)	BELLE_SIP_CAST(obj,belle_sip_header_date_t)
#define BELLE_SIP_DATE "Date"

/******************************
* P-Preferred-Identity header object inherent from header_address
*
******************************/
typedef struct _belle_sip_header_p_preferred_identity belle_sip_header_p_preferred_identity_t;

BELLESIP_EXPORT belle_sip_header_p_preferred_identity_t* belle_sip_header_p_preferred_identity_new(void);

BELLESIP_EXPORT belle_sip_header_p_preferred_identity_t* belle_sip_header_p_preferred_identity_parse(const char* p_preferred_identity) ;

BELLESIP_EXPORT belle_sip_header_p_preferred_identity_t* belle_sip_header_p_preferred_identity_create(const belle_sip_header_address_t *address);

#define BELLE_SIP_HEADER_P_PREFERRED_IDENTITY(t) BELLE_SIP_CAST(t,belle_sip_header_p_preferred_identity_t)
#define BELLE_SIP_P_PREFERRED_IDENTITY "P-Preferred-Identity"

/******************************
* Privacy header object inherent from header
*
******************************/
typedef struct _belle_sip_header_privacy belle_sip_header_privacy_t;

BELLESIP_EXPORT belle_sip_header_privacy_t* belle_sip_header_privacy_new(void);

BELLESIP_EXPORT belle_sip_header_privacy_t* belle_sip_header_privacy_parse(const char* privacy) ;

BELLESIP_EXPORT belle_sip_header_privacy_t* belle_sip_header_privacy_create(const char* privacy);

BELLESIP_EXPORT void belle_sip_header_privacy_add_privacy(belle_sip_header_privacy_t* privacy, const char* value);

BELLESIP_EXPORT void belle_sip_header_privacy_set_privacy(belle_sip_header_privacy_t* privacy, belle_sip_list_t* privacy_values);

BELLESIP_EXPORT belle_sip_list_t* belle_sip_header_privacy_get_privacy(const belle_sip_header_privacy_t* privacy);


#define BELLE_SIP_HEADER_PRIVACY(t) BELLE_SIP_CAST(t,belle_sip_header_privacy_t)
#define BELLE_SIP_PRIVACY "Privacy"


/******************************
* Event header object inherent from parameters
*
******************************/
typedef struct _belle_sip_header_event belle_sip_header_event_t;
BELLESIP_EXPORT belle_sip_header_event_t* belle_sip_header_event_new(void);
BELLESIP_EXPORT belle_sip_header_event_t* belle_sip_header_event_parse(const char* event) ;
BELLESIP_EXPORT belle_sip_header_event_t* belle_sip_header_event_create(const char* event);
BELLESIP_EXPORT const char* belle_sip_header_event_get_package_name(const belle_sip_header_event_t* event);
BELLESIP_EXPORT void belle_sip_header_event_set_package_name(belle_sip_header_event_t* event, const char* package_name);
BELLESIP_EXPORT const char* belle_sip_header_event_get_id(const belle_sip_header_event_t* event);
BELLESIP_EXPORT void belle_sip_header_event_set_id(belle_sip_header_event_t* event, const char* id);
#define BELLE_SIP_HEADER_EVENT(t) BELLE_SIP_CAST(t,belle_sip_header_event_t)
#define BELLE_SIP_EVENT "Event"


/******************************
 * Supported header object inherent from header
 *
 ******************************/
typedef struct _belle_sip_header_supported belle_sip_header_supported_t;
BELLESIP_EXPORT belle_sip_header_supported_t* belle_sip_header_supported_new(void);
BELLESIP_EXPORT belle_sip_header_supported_t* belle_sip_header_supported_parse(const char* supported) ;
BELLESIP_EXPORT belle_sip_header_supported_t* belle_sip_header_supported_create(const char* supported);
BELLESIP_EXPORT void belle_sip_header_supported_add_supported(belle_sip_header_supported_t* supported, const char* value);
BELLESIP_EXPORT void belle_sip_header_supported_set_supported(belle_sip_header_supported_t* supported, belle_sip_list_t* supported_values);
BELLESIP_EXPORT belle_sip_list_t* belle_sip_header_supported_get_supported(const belle_sip_header_supported_t* supported);
#define BELLE_SIP_HEADER_SUPPORTED(t) BELLE_SIP_CAST(t,belle_sip_header_supported_t)
#define BELLE_SIP_SUPPORTED "Supported"

/******************************
 * Content Disposition header object inherent from header
 *
 ******************************/
typedef struct _belle_sip_header_content_disposition belle_sip_header_content_disposition_t;
BELLESIP_EXPORT belle_sip_header_content_disposition_t* belle_sip_header_content_disposition_new(void);
BELLESIP_EXPORT belle_sip_header_content_disposition_t* belle_sip_header_content_disposition_parse (const char* content_disposition) ;
BELLESIP_EXPORT belle_sip_header_content_disposition_t* belle_sip_header_content_disposition_create (const char* content_disposition);
BELLESIP_EXPORT const char* belle_sip_header_content_disposition_get_content_disposition(const belle_sip_header_content_disposition_t* content_disposition);
BELLESIP_EXPORT void belle_sip_header_content_disposition_set_content_disposition(belle_sip_header_content_disposition_t* obj,const char* content_disposition);
#define BELLE_SIP_HEADER_CONTENT_DISPOSITION(t) BELLE_SIP_CAST(t,belle_sip_header_content_disposition_t)
#define BELLE_SIP_CONTENT_DISPOSITION "Content-Disposition"

/******************************
 * Accept header object inherent from parameters
 *
 ******************************/
typedef struct _belle_sip_header_accept belle_sip_header_accept_t;
BELLESIP_EXPORT belle_sip_header_accept_t* belle_sip_header_accept_new(void);
BELLESIP_EXPORT belle_sip_header_accept_t* belle_sip_header_accept_parse (const char* accept) ;
BELLESIP_EXPORT belle_sip_header_accept_t* belle_sip_header_accept_create (const char* type,const char* sub_type) ;
BELLESIP_EXPORT belle_sip_header_accept_t* belle_sip_header_accept_parse (const char* accept) ;
BELLESIP_EXPORT const char*	belle_sip_header_accept_get_type(const belle_sip_header_accept_t* accept);
BELLESIP_EXPORT void belle_sip_header_accept_set_type(belle_sip_header_accept_t* accept,const char* type);
BELLESIP_EXPORT const char*	belle_sip_header_accept_get_subtype(const belle_sip_header_accept_t* accept);
BELLESIP_EXPORT void belle_sip_header_accept_set_subtype(belle_sip_header_accept_t* accept,const char* sub_type);
#define BELLE_SIP_HEADER_ACCEPT(t) BELLE_SIP_CAST(t,belle_sip_header_accept_t)
#define BELLE_SIP_ACCEPT "Accept"


BELLE_SIP_END_DECLS


#endif /* HEADERS_H_ */
