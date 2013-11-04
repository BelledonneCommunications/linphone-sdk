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
GET_SET_STRING(belle_sip_auth_event,domain)
GET_SET_STRING(belle_sip_auth_event,passwd)
GET_SET_STRING(belle_sip_auth_event,ha1)
GET_SET_STRING(belle_sip_auth_event,distinguished_name)

belle_sip_auth_event_t* belle_sip_auth_event_create(const char* realm, const belle_sip_header_from_t *from) {
	belle_sip_auth_event_t* result = belle_sip_new0(belle_sip_auth_event_t);
	belle_sip_uri_t *uri=belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(from));

	belle_sip_auth_event_set_realm(result,realm);
	belle_sip_auth_event_set_username(result,belle_sip_uri_get_user(uri));
	belle_sip_auth_event_set_domain(result,belle_sip_uri_get_host(uri));
	return result;
}

void belle_sip_auth_event_destroy(belle_sip_auth_event_t* event) {
	DESTROY_STRING(event,username);
	DESTROY_STRING(event,userid);
	DESTROY_STRING(event,realm);
	DESTROY_STRING(event,domain);
	DESTROY_STRING(event,passwd);
	DESTROY_STRING(event,ha1);
	DESTROY_STRING(event,distinguished_name);
	if (event->cert) belle_sip_object_unref(event->cert);
	if (event->key) belle_sip_object_unref(event->key);

	belle_sip_free(event);
}


belle_sip_certificates_chain_t* belle_sip_auth_event_get_client_certificates_chain(const belle_sip_auth_event_t* event) {
	return event->cert;
}

void belle_sip_auth_event_set_client_certificates_chain(belle_sip_auth_event_t* event, belle_sip_certificates_chain_t* value) {
	if (event->cert) belle_sip_object_unref(event->cert);
	event->cert=value;
	if (event->cert) belle_sip_object_ref(event->cert);
}

belle_sip_signing_key_t* belle_sip_auth_event_get_signing_key(const belle_sip_auth_event_t* event) {
	return event->key;
}

void belle_sip_auth_event_set_signing_key(belle_sip_auth_event_t* event, belle_sip_signing_key_t* value) {
	if (event->key) belle_sip_object_unref(event->key);
	event->key=value;
	if (event->key) belle_sip_object_ref(event->key);
}

belle_sip_auth_mode_t belle_sip_auth_event_get_mode(const belle_sip_auth_event_t* event) {
	return event->mode;
}

