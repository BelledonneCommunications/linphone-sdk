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
 * header_address
 ***********************/
struct _belle_sip_header_address {
	belle_sip_parameters_t params;
	const char* displayname;
	belle_sip_uri_t* uri;
};
static void belle_sip_header_address_init(belle_sip_header_address_t* object){
	belle_sip_object_init_type(object,belle_sip_header_address_t);
	belle_sip_parameters_init((belle_sip_parameters_t*)object); /*super*/
}

static void belle_sip_header_address_destroy(belle_sip_header_address_t* contact) {
	if (contact->displayname) belle_sip_free((void*)(contact->displayname));
	if (contact->uri) belle_sip_object_unref(BELLE_SIP_OBJECT(contact->uri));
}

BELLE_SIP_NEW(header_address,object)
GET_SET_STRING(belle_sip_header_address,displayname);

void belle_sip_header_address_set_quoted_displayname(belle_sip_header_address_t* address,const char* value) {
		if (address->displayname != NULL) belle_sip_free((void*)(address->displayname));
		size_t value_size = strlen(value);
		address->displayname=belle_sip_malloc0(value_size-2+1);
		strncpy((char*)(address->displayname),value+1,value_size-2);
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
	belle_sip_header_address_destroy(BELLE_SIP_HEADER_ADDRESS(contact));
}

BELLE_SIP_NEW(header_contact,header_address)
BELLE_SIP_PARSE(header_contact)

GET_SET_INT_PARAM_PRIVATE(belle_sip_header_contact,expires,int,_)
GET_SET_INT_PARAM_PRIVATE(belle_sip_header_contact,q,float,_);
GET_SET_BOOL(belle_sip_header_contact,wildcard,is);


int belle_sip_header_contact_set_expires(belle_sip_header_contact_t* contact, int expires) {
	if (expires < 0 ) {
		return -1;
	}
	_belle_sip_header_contact_set_expires(contact,expires);
	return 0;
 }
int belle_sip_header_contact_set_qvalue(belle_sip_header_contact_t* contact, float qValue) {
	 if (qValue != -1 && qValue < 0 && qValue >1) {
		 return -1;
	 }
	 _belle_sip_header_contact_set_q(contact,qValue);
	 return 0;
}
float	belle_sip_header_contact_get_qvalue(belle_sip_header_contact_t* contact) {
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
	belle_sip_header_address_destroy(BELLE_SIP_HEADER_ADDRESS(from));
}

BELLE_SIP_NEW(header_from,header_address)
BELLE_SIP_PARSE(header_from)
GET_SET_STRING_PARAM(belle_sip_header_from,tag);

/**************************
* To header object inherent from header_address
****************************
*/
struct _belle_sip_header_to  {
	belle_sip_header_address_t address;
};

static void belle_sip_header_to_destroy(belle_sip_header_to_t* to) {
	belle_sip_header_address_destroy(BELLE_SIP_HEADER_ADDRESS(to));
}

BELLE_SIP_NEW(header_to,header_address)
BELLE_SIP_PARSE(header_to)
GET_SET_STRING_PARAM(belle_sip_header_to,tag);
/**************************
* Viq header object inherent from header_address
****************************
*/
struct _belle_sip_header_via  {
	belle_sip_header_address_t address;
};

static void belle_sip_header_via_destroy(belle_sip_header_via_t* to) {
	belle_sip_header_address_destroy(BELLE_SIP_HEADER_ADDRESS(to));
}

BELLE_SIP_NEW(header_via,header_address)
BELLE_SIP_PARSE(header_via)




