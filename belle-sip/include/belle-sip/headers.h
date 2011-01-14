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
belle_sip_uri_t* belle_sip_header_address_get_uri(belle_sip_header_address_t* address);
/**
 *
 */
void belle_sip_header_address_set_uri(belle_sip_header_address_t* address, belle_sip_uri_t* uri);

/**
 *
 */
const char* belle_sip_header_address_get_displayname(belle_sip_header_address_t* address);
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
 int	belle_sip_header_contact_get_expires(belle_sip_header_contact_t* contact);
/**
 * Returns the value of the q-value parameter of this ContactHeader. The q-value parameter indicates the relative preference amongst a set of locations. q-values are decimal numbers from 0 to 1, with higher values indicating higher preference.
 * @return the q-value parameter of this ContactHeader, -1 if the q-value is not set.
 */
 float	belle_sip_header_contact_get_qvalue(belle_sip_header_contact_t* contact);
 /**
  * Returns a boolean value that indicates if the contact header has the format of Contact: *.
  * @return true if this is a wildcard address, false otherwise.
  */
 unsigned int belle_sip_header_contact_is_wildcard(belle_sip_header_contact_t* contact);
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


 belle_sip_header_from_t* belle_sip_header_from_parse (const char* from) ;


 void belle_sip_header_from_set_tag(belle_sip_header_from_t* from, const char* tag);

 const char* belle_sip_header_from_get_tag(belle_sip_header_from_t* from);

#define BELLE_SIP_HEADER_FROM(t) BELLE_SIP_CAST(t,belle_sip_header_from_t)
 /******************************
 * To header object inherent from header_address
 *
 ******************************/
 typedef struct _belle_sip_header_to belle_sip_header_to_t;

 belle_sip_header_to_t* belle_sip_header_to_new();


 belle_sip_header_to_t* belle_sip_header_to_parse (const char* to) ;


 void belle_sip_header_to_set_tag(belle_sip_header_to_t* from, const char* tag);

 const char* belle_sip_header_to_get_tag(belle_sip_header_to_t* from);

#define BELLE_SIP_HEADER_TO(t) BELLE_SIP_CAST(t,belle_sip_header_to_t)

/******************************
 * Via header object inherent from header_address
 *
 ******************************/
typedef struct _belle_sip_header_via belle_sip_header_via_t;

belle_sip_header_via_t* belle_sip_header_via_new();

belle_sip_header_via_t* belle_sip_header_via_parse (const char* via) ;
const char*	belle_sip_header_via_get_branch(belle_sip_header_via_t* via);
const char*	belle_sip_header_via_get_transport(belle_sip_header_via_t* via);
const char*	belle_sip_header_via_get_host(belle_sip_header_via_t* via);
int belle_sip_header_via_get_port(belle_sip_header_via_t* via);

const char*	belle_sip_header_via_get_maddr(belle_sip_header_via_t* via);
const char*	belle_sip_header_via_get_protocol(belle_sip_header_via_t* via);
const char*	belle_sip_header_via_get_received(belle_sip_header_via_t* via);
int belle_sip_header_via_get_rport(belle_sip_header_via_t* via);
int	belle_sip_header_via_get_ttl(belle_sip_header_via_t* via);

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
typedef struct _belle_sip_header_callid belle_sip_header_callid_t;

belle_sip_header_callid_t* belle_sip_header_callid_new();

belle_sip_header_callid_t* belle_sip_header_callid_parse (const char* callid) ;
const char*	belle_sip_header_callid_get_callid(belle_sip_header_callid_t* callid);
void belle_sip_header_callid_set_callid(belle_sip_header_callid_t* via,const char* callid);
#define BELLE_SIP_HEADER_CALLID(t) BELLE_SIP_CAST(t,belle_sip_header_callid_t)
/******************************
 * cseq object inherent from object
 *
 ******************************/
typedef struct _belle_sip_header_cseq belle_sip_header_cseq_t;

belle_sip_header_cseq_t* belle_sip_header_cseq_new();

belle_sip_header_cseq_t* belle_sip_header_cseq_parse (const char* cseq) ;
const char*	belle_sip_header_cseq_get_method(belle_sip_header_cseq_t* cseq);
void belle_sip_header_cseq_set_method(belle_sip_header_cseq_t* cseq,const char* method);
unsigned int	belle_sip_header_cseq_get_seq_number(belle_sip_header_cseq_t* cseq);
void belle_sip_header_cseq_set_seq_number(belle_sip_header_cseq_t* cseq,unsigned int seq_number);
#define BELLE_SIP_HEADER_CSEQ(t) BELLE_SIP_CAST(t,belle_sip_header_cseq_t)
/******************************
 * content type object inherent from parameters
 *
 ******************************/
typedef struct _belle_sip_header_content_type belle_sip_header_content_type_t;

belle_sip_header_content_type_t* belle_sip_header_content_type_new();

belle_sip_header_content_type_t* belle_sip_header_content_type_parse (const char* content_type) ;
const char*	belle_sip_header_content_type_get_type(belle_sip_header_content_type_t* content_type);
void belle_sip_header_content_type_set_type(belle_sip_header_content_type_t* content_type,const char* type);
const char*	belle_sip_header_content_type_get_subtype(belle_sip_header_content_type_t* content_type);
void belle_sip_header_content_type_set_subtype(belle_sip_header_content_type_t* content_type,const char* sub_type);
#define BELLE_SIP_HEADER_CONTENT_TYPE(t) BELLE_SIP_CAST(t,belle_sip_header_content_type_t)
#endif /* HEADERS_H_ */
