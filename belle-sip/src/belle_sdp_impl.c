/*
	belle-sdp - SIP (RFC4566) library.
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
#include "belle-sip/belle-sdp.h"
#include "belle_sip_internal.h"
#include "belle_sdpParser.h"
#include "belle_sdpLexer.h"

/***************************************************************************************
 * Attribute
 *
 **************************************************************************************/
struct _belle_sdp_attribute {
	belle_sip_object_t base;
	const char* name;
	const char* value;
};
void belle_sdp_attribute_destroy(belle_sdp_attribute_t* attribute) {
}

void belle_sdp_attribute_clone(belle_sdp_attribute_t *attribute, const belle_sdp_attribute_t *orig){
}
int belle_sdp_attribute_marshal(belle_sdp_attribute_t* attribute, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"a=%s"
								,attribute->name);
	if (attribute->value) {
		current_offset+=snprintf(	buff+current_offset
									,buff_size-current_offset
									,":%s"
									,attribute->value);
	}
	return current_offset-offset;
}
BELLE_SDP_NEW(attribute,belle_sip_object)
BELLE_SDP_PARSE(attribute)
GET_SET_STRING(belle_sdp_attribute,name);
GET_SET_STRING(belle_sdp_attribute,value);
unsigned int belle_sdp_attribute_as_value(const belle_sdp_attribute_t* attribute) {
	return attribute->value!=NULL;
}

/************************
 * connection
 ***********************/
struct _belle_sdp_connection {
	belle_sip_object_t base;
	const char* network_type;
	const char* address_type;
	const char* address;
 };

void belle_sdp_connection_destroy(belle_sdp_connection_t* connection) {
}

void belle_sdp_connection_clone(belle_sdp_connection_t *connection, const belle_sdp_connection_t *orig){
}
int belle_sdp_connection_marshal(belle_sdp_connection_t* connection, char* buff,unsigned int offset,unsigned int buff_size) {
	unsigned int current_offset=offset;
	current_offset+=snprintf(	buff+current_offset
								,buff_size-current_offset
								,"c=%s %s %s"
								,connection->network_type
								,connection->address_type
								,connection->address);
	return current_offset-offset;
}
BELLE_SDP_NEW(connection,belle_sip_object)
BELLE_SDP_PARSE(connection)
GET_SET_STRING(belle_sdp_connection,network_type);
GET_SET_STRING(belle_sdp_connection,address_type);
GET_SET_STRING(belle_sdp_connection,address);
