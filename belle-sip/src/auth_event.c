/*
	auth_info.c belle-sip - SIP (RFC3261) library.
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


#include "belle-sip/auth-helper.h"
#include "belle_sip_internal.h"


GET_SET_STRING(belle_sip_auth_event,username)

GET_SET_STRING(belle_sip_auth_event,userid)
GET_SET_STRING(belle_sip_auth_event,realm)
GET_SET_STRING(belle_sip_auth_event,domain)
GET_SET_STRING(belle_sip_auth_event,passwd)
GET_SET_STRING(belle_sip_auth_event,ha1)
GET_SET_STRING(belle_sip_auth_event,distinguished_name)

belle_sip_auth_event_t* belle_sip_auth_event_create(belle_sip_object_t *source, const char* realm, const belle_sip_uri_t *from_uri) {
	belle_sip_auth_event_t* result = belle_sip_new0(belle_sip_auth_event_t);
	result->source=source;
	belle_sip_auth_event_set_realm(result,realm);
	
	if (from_uri){
		belle_sip_auth_event_set_username(result,belle_sip_uri_get_user(from_uri));
		belle_sip_auth_event_set_domain(result,belle_sip_uri_get_host(from_uri));
	}
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
	SET_OBJECT_PROPERTY(event,key,value);
}

belle_sip_auth_mode_t belle_sip_auth_event_get_mode(const belle_sip_auth_event_t* event) {
	return event->mode;
}


/* deprecated on 2016/02/02 */
belle_tls_verify_policy_t *belle_tls_verify_policy_new(){
	return (belle_tls_verify_policy_t *)belle_tls_crypto_config_new();
}

int belle_tls_verify_policy_set_root_ca(belle_tls_verify_policy_t *obj, const char *path) {
	return belle_tls_crypto_config_set_root_ca(obj, path);
}

void belle_tls_verify_policy_set_exceptions(belle_tls_verify_policy_t *obj, int flags){
	belle_tls_crypto_config_set_verify_exceptions(obj, flags);
}

unsigned int belle_tls_verify_policy_get_exceptions(const belle_tls_verify_policy_t *obj){
	return belle_tls_crypto_config_get_verify_exceptions(obj);
}
/* end of deprecated on 2016/02/02 */

static void crypto_config_uninit(belle_tls_crypto_config_t *obj) {
	if (obj->root_ca) belle_sip_free(obj->root_ca);
	if (obj->root_ca_data) belle_sip_free(obj->root_ca_data);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_tls_crypto_config_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_tls_crypto_config_t,belle_sip_object_t,crypto_config_uninit,NULL,NULL,FALSE);

belle_tls_crypto_config_t *belle_tls_crypto_config_new(void){
	belle_tls_crypto_config_t *obj=belle_sip_object_new(belle_tls_crypto_config_t);
	
	/*default to "system" default root ca, wihtout warranty...*/
#ifdef __linux
	belle_tls_crypto_config_set_root_ca(obj,"/etc/ssl/certs");
#elif defined(__APPLE__)
	belle_tls_crypto_config_set_root_ca(obj,"/opt/local/share/curl/curl-ca-bundle.crt");
#elif __QNX__
	belle_tls_crypto_config_set_root_ca(obj,"/var/certs/web_trusted@personal@certmgr");
#endif
	obj->ssl_config = NULL;
	obj->exception_flags = BELLE_TLS_VERIFY_NONE;

	return obj;
}

int belle_tls_crypto_config_set_root_ca(belle_tls_crypto_config_t *obj, const char *path){
	if (obj->root_ca) {
		belle_sip_free(obj->root_ca);
		obj->root_ca = NULL;
	}
	if (path) {
		obj->root_ca = belle_sip_strdup(path);
		belle_sip_message("Root ca path set to %s", obj->root_ca);
	} else {
		belle_sip_message("Root ca path disabled");
	}
	return 0;
}

int belle_tls_crypto_config_set_root_ca_data(belle_tls_crypto_config_t *obj, const char *data) {
	if (obj->root_ca) {
		belle_sip_free(obj->root_ca);
		obj->root_ca = NULL;
	}
	if (obj->root_ca_data) {
		belle_sip_free(obj->root_ca_data);
		obj->root_ca_data = NULL;
	}
	if (data) {
		obj->root_ca_data = belle_sip_strdup(data);
		belle_sip_message("Root ca data set to %s", obj->root_ca_data);
	} else {
		belle_sip_message("Root ca data disabled");
	}
	return 0;
}

void belle_tls_crypto_config_set_verify_exceptions(belle_tls_crypto_config_t *obj, int flags){
	obj->exception_flags=flags;
}

unsigned int belle_tls_crypto_config_get_verify_exceptions(const belle_tls_crypto_config_t *obj){
	return obj->exception_flags;
}

void belle_tls_crypto_config_set_ssl_config(belle_tls_crypto_config_t *obj, void *ssl_config) {
	obj->ssl_config = ssl_config;
}

