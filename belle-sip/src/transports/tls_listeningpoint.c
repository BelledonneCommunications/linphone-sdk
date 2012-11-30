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

#ifdef HAVE_OPENSSL
#include "gnutls/openssl.h"
#endif
static void belle_sip_tls_listening_point_uninit(belle_sip_tls_listening_point_t *lp){
}

static belle_sip_channel_t *tls_create_channel(belle_sip_listening_point_t *lp, const char *dest_ip, int port){
#ifdef HAVE_GNUTLS
	belle_sip_channel_t *chan=belle_sip_channel_new_tls(BELLE_SIP_TLS_LISTENING_POINT(lp)
														,belle_sip_uri_get_host(lp->listening_uri)
														,belle_sip_uri_get_port(lp->listening_uri)
														,dest_ip
														,port);
	return chan;
#else
	return NULL;
#endif
	
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_tls_listening_point_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_tls_listening_point_t)={
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_tls_listening_point_t, belle_sip_listening_point_t,FALSE),
			(belle_sip_object_destroy_t)belle_sip_tls_listening_point_uninit,
			NULL,
			NULL
		},
		"TLS",
		tls_create_channel
	}
};



belle_sip_listening_point_t * belle_sip_tls_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port){
#ifdef HAVE_GNUTLS
	belle_sip_tls_listening_point_t *lp=belle_sip_object_new(belle_sip_tls_listening_point_t);
	belle_sip_listening_point_init((belle_sip_listening_point_t*)lp,s,ipaddress,port);
#ifdef HAVE_OPENSSL
	char ssl_error_string[128]; /*see openssl doc for size*/
	lp->ssl_context=SSL_CTX_new(TLSv1_client_method());
	if (!lp->ssl_context) {
		belle_sip_error("belle_sip_listening_point_t: SSL_CTX_new failed caused by [%s]",ERR_error_string(ERR_get_error(),ssl_error_string));
		belle_sip_object_unref(lp);
		return NULL;
	}
	/*SSL_CTX_set_cipher_list(lp->ssl_context,"LOW");*/
#endif /**/
	return BELLE_SIP_LISTENING_POINT(lp);
#else
	belle_sip_error("Cannot create tls listening point because not compile with TLS support");
	return NULL;
#endif
}
