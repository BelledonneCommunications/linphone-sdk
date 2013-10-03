/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2013  Belledonne Communications SARL

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

#ifdef HAVE_POLARSSL

#include <polarssl/ssl.h>

static void belle_sip_tls_listening_point_uninit(belle_sip_tls_listening_point_t *lp){
	belle_sip_free(lp->root_ca);
}

static belle_sip_channel_t *tls_create_channel(belle_sip_listening_point_t *lp, const belle_sip_hop_t *hop){
	belle_sip_channel_t *chan=belle_sip_channel_new_tls(BELLE_SIP_TLS_LISTENING_POINT(lp)
				,belle_sip_uri_get_host(lp->listening_uri)
				,belle_sip_uri_get_port(lp->listening_uri)
				,hop->cname
				,hop->host,hop->port);
	return chan;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_tls_listening_point_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_tls_listening_point_t)={
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_tls_listening_point_t, belle_sip_stream_listening_point_t,TRUE),
			(belle_sip_object_destroy_t)belle_sip_tls_listening_point_uninit,
			NULL,
			NULL
		},
		"TLS",
		tls_create_channel
	}
};

static int on_new_connection(void *userdata, unsigned int revents){
	belle_sip_socket_t child;
	struct sockaddr_storage addr;
	socklen_t slen=sizeof(addr);
	belle_sip_tls_listening_point_t *lp=(belle_sip_tls_listening_point_t*)userdata;
	belle_sip_stream_listening_point_t *super=(belle_sip_stream_listening_point_t*)lp;
	
	child=accept(super->server_sock,(struct sockaddr*)&addr,&slen);
	if (child==(belle_sip_socket_t)-1){
		belle_sip_error("Listening point [%p] accept() failed on TLS server socket: %s",lp,belle_sip_get_socket_error_string());
		belle_sip_stream_listening_point_destroy_server_socket(super);
		belle_sip_stream_listening_point_setup_server_socket(super,on_new_connection);
		return BELLE_SIP_STOP;
	}
	belle_sip_message("New connection arriving on TLS, not handled !");
	close_socket(child);
	return BELLE_SIP_CONTINUE;
}

belle_sip_listening_point_t * belle_sip_tls_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port){
	belle_sip_tls_listening_point_t *lp=belle_sip_object_new(belle_sip_tls_listening_point_t);
	belle_sip_stream_listening_point_init((belle_sip_stream_listening_point_t*)lp,s,ipaddress,port,on_new_connection);
	
	lp->verify_exceptions=0;
	/*try to load "system" default root ca, wihtout warranty...*/
#ifdef __linux
	belle_sip_tls_listening_point_set_root_ca(lp,"/etc/ssl/certs");
#elif defined(__APPLE__)
	belle_sip_tls_listening_point_set_root_ca(lp,"/opt/local/share/curl/curl-ca-bundle.crt");
#endif
	return BELLE_SIP_LISTENING_POINT(lp);
}

int belle_sip_tls_listening_point_set_root_ca(belle_sip_tls_listening_point_t *lp, const char *path){
	if (lp->root_ca){
		belle_sip_free(lp->root_ca);
		lp->root_ca=NULL;
	}
	if (path){
		lp->root_ca=belle_sip_strdup(path);
		belle_sip_message("Root ca path set to %s",lp->root_ca);
	} else {
		belle_sip_message("Root ca path disabled");
	}
	return 0;
}

int belle_sip_tls_listening_point_set_verify_exceptions(belle_sip_tls_listening_point_t *lp, int flags){
	lp->verify_exceptions=flags;
	return 0;
}

#else

belle_sip_listening_point_t * belle_sip_tls_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port){
	return NULL;
}

int belle_sip_tls_listening_point_set_root_ca(belle_sip_tls_listening_point_t *s, const char *path){
	return -1;
}

int belle_sip_tls_listening_point_set_verify_exceptions(belle_sip_tls_listening_point_t *s, int value){
	return -1;
}

#endif

