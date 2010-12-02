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

void belle_sip_header_address_delete(belle_sip_header_address_t* contact);

belle_sip_header_address_t* belle_sip_header_address_parse (const char* address) ;

belle_sip_header_address_t* belle_sip_header_address_ref (belle_sip_header_address_t* address) ;
void belle_sip_header_address_unref (belle_sip_header_address_t* address) ;

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


/***************************************************************************************
 * header common
 *
 **************************************************************************************/

typedef struct _belle_sip_header belle_sip_header_t;

/**
 * Contact header object
 */
typedef struct _belle_sip_header_contact belle_sip_header_contact_t;

belle_sip_header_contact_t* belle_sip_header_contact_new();

void belle_sip_header_contact_delete(belle_sip_header_contact_t* contact);

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

 /**
 * From header object inherent from header_address
 *
 */
 typedef struct _belle_sip_header_from belle_sip_header_from_t;

 belle_sip_header_from_t* belle_sip_header_from_new();

 void belle_sip_header_from_delete(belle_sip_header_from_t* from);

 belle_sip_header_from_t* belle_sip_header_from_parse (const char* from) ;

 belle_sip_header_from_t* belle_sip_header_from_ref (belle_sip_header_from_t* from) ;

 void belle_sip_header_from_unref (belle_sip_header_from_t* from) ;

 void belle_sip_header_from_set_tag(belle_sip_header_from_t* from, const char* tag);

 const char* belle_sip_header_from_get_tag(belle_sip_header_from_t* from);




#endif /* HEADERS_H_ */
