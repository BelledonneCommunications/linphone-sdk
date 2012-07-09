/*
	auth_info.c belle-sip - SIP (RFC3261) library.
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


#include "belle-sip/auth-helper.h"
#include "belle_sip_internal.h"


GET_SET_STRING(belle_sip_auth_event,username)

GET_SET_STRING(belle_sip_auth_event,userid)
GET_SET_STRING(belle_sip_auth_event,realm)
GET_SET_STRING(belle_sip_auth_event,passwd)
GET_SET_STRING(belle_sip_auth_event,ha1)
belle_sip_auth_event_t* belle_sip_auth_event_create(const char* realm,const char* username) {
	belle_sip_auth_event_t* result = belle_sip_malloc(sizeof(belle_sip_auth_event_t));
	memset(result,0,sizeof(belle_sip_auth_event_t));
	belle_sip_auth_event_set_realm(result,realm);
	belle_sip_auth_event_set_username(result,username);
	return result;
}
void belle_sip_auth_event_destroy(belle_sip_auth_event_t* event) {
	DESTROY_STRING(event,username);
	DESTROY_STRING(event,userid);
	DESTROY_STRING(event,realm);
	DESTROY_STRING(event,passwd);
	DESTROY_STRING(event,ha1);
	belle_sip_free(event);
}
