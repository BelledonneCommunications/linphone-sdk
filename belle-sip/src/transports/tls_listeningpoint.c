/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2013  Belledonne Communications SARL

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

#include "belle_sip_internal.h"

static void belle_sip_tls_listening_point_uninit(belle_sip_tls_listening_point_t *lp){
	belle_sip_object_unref(lp->crypto_config);
}

static belle_sip_channel_t *tls_create_channel(belle_sip_listening_point_t *lp, const belle_sip_hop_t *hop){
	belle_sip_channel_t *chan=belle_sip_channel_new_tls(lp->stack, ((belle_sip_tls_listening_point_t*) lp)->crypto_config
				,belle_sip_uri_get_host(lp->listening_uri)
				,belle_sip_uri_get_port(lp->listening_uri)
				,hop->cname
				,hop->host,hop->port);
	return chan;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_tls_listening_point_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_tls_listening_point_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_tls_listening_point_t, belle_sip_stream_listening_point_t,TRUE),
			(belle_sip_object_destroy_t)belle_sip_tls_listening_point_uninit,
			NULL,
			NULL,
			BELLE_SIP_DEFAULT_BUFSIZE_HINT
		},
		"TLS",
		tls_create_channel
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END


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
	belle_sip_close_socket(child);
	return BELLE_SIP_CONTINUE;
}

belle_sip_listening_point_t * belle_sip_tls_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port){
	belle_sip_tls_listening_point_t *lp=belle_sip_object_new(belle_sip_tls_listening_point_t);

	belle_sip_stream_listening_point_init((belle_sip_stream_listening_point_t*)lp,s,ipaddress,port,on_new_connection);
	lp->crypto_config=belle_tls_crypto_config_new();
	return BELLE_SIP_LISTENING_POINT(lp);
}

int belle_sip_tls_listening_point_set_root_ca(belle_sip_tls_listening_point_t *lp, const char *path){
	return belle_tls_crypto_config_set_root_ca(lp->crypto_config,path);
}

int belle_sip_tls_listening_point_set_verify_exceptions(belle_sip_tls_listening_point_t *lp, int flags){
	belle_tls_crypto_config_set_verify_exceptions(lp->crypto_config,flags);
	return 0;
}

int belle_sip_tls_listening_point_set_verify_policy(belle_sip_tls_listening_point_t *s, belle_tls_verify_policy_t *pol){
	SET_OBJECT_PROPERTY(s,crypto_config,(belle_tls_crypto_config_t *)pol);
	return 0;
}

int belle_sip_tls_listening_point_set_crypto_config(belle_sip_tls_listening_point_t *s, belle_tls_crypto_config_t *crypto_config){
	SET_OBJECT_PROPERTY(s,crypto_config,crypto_config);
	return 0;
}

belle_tls_crypto_config_t *belle_sip_tls_listening_point_get_crypto_config(belle_sip_tls_listening_point_t *s){
	return s->crypto_config;
}

int belle_sip_tls_listening_point_available(void){
	return TRUE;
}
