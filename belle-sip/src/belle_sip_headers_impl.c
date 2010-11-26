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
	int ref;
	const char* displayname;
	belle_sip_uri_t* uri;
};
belle_sip_header_address_t* belle_sip_header_address_new() {
	return (belle_sip_header_address_t*)belle_sip_new0(belle_sip_header_address_t);
}

void belle_sip_header_address_delete(belle_sip_header_address_t* contact) {
	if (contact->displayname) free((void*)(contact->displayname));
	if (contact->uri) belle_sip_uri_delete(contact->uri);
}

GET_SET_STRING(belle_sip_header_address,displayname);

belle_sip_uri_t* belle_sip_header_address_get_uri(belle_sip_header_address_t* address) {
	return address->uri;
}

void belle_sip_header_address_set_uri(belle_sip_header_address_t* address, belle_sip_uri_t* uri) {
	address->uri=uri;
}

BELLE_SIP_REF(header_address)


/************************
 * header_contact
 ***********************/
struct _belle_sip_header_contact {
	belle_sip_header_address_t address;
	int ref;
	belle_sip_header_address_t* header_address;
	int expires;
	float qvalue;
	unsigned int wildcard;
 };

belle_sip_header_contact_t* belle_sip_header_contact_new() {
	return (belle_sip_header_contact_t*)belle_sip_new0(belle_sip_header_contact_t);
}

void belle_sip_header_contact_delete(belle_sip_header_contact_t* contact) {
	if (contact->header_address) belle_sip_header_address_delete(contact->header_address);
}

BELLE_SIP_PARSE(header_contact);

GET_SET_INT_PRIVATE(belle_sip_header_contact,expires,int,_);
GET_SET_INT_PRIVATE(belle_sip_header_contact,qvalue,float,_);
GET_SET_BOOL(belle_sip_header_contact,wildcard,is);

int belle_sip_header_contact_set_expires(belle_sip_header_contact_t* contact, int expires) {
	if (expires < 0 ) {
		return -1;
	}
	_belle_sip_header_contact_set_expires(contact,expires);
	return 0;
 }
int belle_sip_header_contact_set_qvalue(belle_sip_header_contact_t* contact, float qValue) {
	 if (qValue != -1 || qValue < 0 || qValue >1) {
		 return -1;
	 }
	 _belle_sip_header_contact_set_qvalue(contact,qValue);
	 return 0;
}





