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

#include "belle_sip_internal.h"
#include "listeningpoint_internal.h"
#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#endif
static void belle_sip_stack_destroy(belle_sip_stack_t *stack){
	belle_sip_object_unref(stack->ml);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_stack_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_stack_t,belle_sip_object_t,belle_sip_stack_destroy,NULL,NULL,FALSE);
#ifdef HAVE_GNUTLS
static void _gnutls_log_func( int level, const char* log) {
	belle_sip_log_level belle_sip_level;
	switch(level) {
	case 1: belle_sip_level=BELLE_SIP_LOG_ERROR;break;
	case 2: belle_sip_level=BELLE_SIP_LOG_WARNING;break;
	case 3: belle_sip_level=BELLE_SIP_LOG_MESSAGE;break;
	default:belle_sip_level=BELLE_SIP_LOG_MESSAGE;break;
	}
	belle_sip_log(belle_sip_level,"gnutls:%s",log);
}
#endif /*HAVE_GNUTLS*/
belle_sip_stack_t * belle_sip_stack_new(const char *properties){
	int result;
	belle_sip_stack_t *stack=belle_sip_object_new(belle_sip_stack_t);
	stack->ml=belle_sip_main_loop_new ();
	stack->timer_config.T1=500;
	stack->timer_config.T2=4000;
	stack->timer_config.T4=5000;
#ifdef HAVE_OPENSSL
	SSL_library_init();
	SSL_load_error_strings();
	/*CRYPTO_set_id_callback(&threadid_cb);
	CRYPTO_set_locking_callback(&locking_function);*/
#endif
#ifdef HAVE_GNUTLS
	/*gnutls_global_set_log_level(9);*/
	gnutls_global_set_log_function(_gnutls_log_func);
	if ((result = gnutls_global_init ()) <0) {
		belle_sip_fatal("Cannot initialize gnu tls vaused by [%s]",gnutls_strerror(result));
	}
#endif
	return stack;
}

const belle_sip_timer_config_t *belle_sip_stack_get_timer_config(const belle_sip_stack_t *stack){
	return &stack->timer_config;
}

belle_sip_listening_point_t *belle_sip_stack_create_listening_point(belle_sip_stack_t *s, const char *ipaddress, int port, const char *transport){
	belle_sip_listening_point_t *lp=NULL;
	if (strcasecmp(transport,"UDP")==0) {
		lp=belle_sip_udp_listening_point_new(s,ipaddress,port);
	} else if (strcasecmp(transport,"TCP") == 0) {
		lp=belle_sip_stream_listening_point_new(s,ipaddress,port);
	}else if (strcasecmp(transport,"TLS") == 0) {
		lp=belle_sip_tls_listening_point_new(s,ipaddress,port);
	} else {
		belle_sip_fatal("Unsupported transport %s",transport);
	}
	return lp;
}

void belle_sip_stack_delete_listening_point(belle_sip_stack_t *s, belle_sip_listening_point_t *lp){
	belle_sip_object_unref(lp);
}

belle_sip_provider_t *belle_sip_stack_create_provider(belle_sip_stack_t *s, belle_sip_listening_point_t *lp){
	belle_sip_provider_t *p=belle_sip_provider_new(s,lp);
	return p;
}

void belle_sip_stack_delete_provider(belle_sip_stack_t *s, belle_sip_provider_t *p){
	belle_sip_object_unref(p);
}

belle_sip_main_loop_t * belle_sip_stack_get_main_loop(belle_sip_stack_t *stack){
	return stack->ml;
}

void belle_sip_stack_main(belle_sip_stack_t *stack){
	belle_sip_main_loop_run(stack->ml);
}

void belle_sip_stack_sleep(belle_sip_stack_t *stack, unsigned int milliseconds){
	belle_sip_main_loop_sleep (stack->ml,milliseconds);
}

void belle_sip_stack_get_next_hop(belle_sip_stack_t *stack, belle_sip_request_t *req, belle_sip_hop_t *hop){
	belle_sip_header_route_t *route=BELLE_SIP_HEADER_ROUTE(belle_sip_message_get_header(BELLE_SIP_MESSAGE(req),"route"));
	belle_sip_uri_t *uri;
	if (route!=NULL){
		uri=belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(route));
	}else{
		uri=belle_sip_request_get_uri(req);
	}
	hop->transport=belle_sip_uri_get_transport_param(uri);
	if (hop->transport==NULL) hop->transport="UDP";
	hop->host=belle_sip_uri_get_host(uri);
	hop->port=belle_sip_uri_get_listening_port(uri);
}


