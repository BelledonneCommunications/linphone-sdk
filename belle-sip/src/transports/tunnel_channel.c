/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010-2013  Belledonne Communications SARL

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
#include "channel.h"

#define TUNNEL_POLLING_DURATION	20 /* in ms */

void * tunnel_client_create_socket(void *tunnelclient, int minLocalPort, int maxLocalPort);
void tunnel_client_close_socket(void *tunnelclient, void *tunnelsocket);
int tunnel_socket_has_data(void *tunnelsocket);
int tunnel_socket_sendto(void *tunnelsocket, const void *buffer, size_t bufsize, const struct sockaddr *dest, socklen_t socklen);
int tunnel_socket_recvfrom(void *tunnelsocket, void *buffer, size_t bufsize, struct sockaddr *src, socklen_t socklen);

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_tunnel_channel_t, belle_sip_channel_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

struct belle_sip_tunnel_channel {
	belle_sip_channel_t base;
	belle_sip_source_t *pollingtimer;
	void *tunnelclient;
	void *tunnelsocket;
};

typedef struct belle_sip_tunnel_channel belle_sip_tunnel_channel_t;


static int tunnel_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen) {
	belle_sip_tunnel_channel_t *chan = (belle_sip_tunnel_channel_t *)obj;
	return tunnel_socket_sendto(chan->tunnelsocket, buf, buflen, obj->current_peer->ai_addr, obj->current_peer->ai_addrlen);
}

static int tunnel_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen) {
	belle_sip_tunnel_channel_t *chan = (belle_sip_tunnel_channel_t *)obj;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	return tunnel_socket_recvfrom(chan->tunnelsocket, buf, buflen, (struct sockaddr *)&addr, addrlen);
}

static int tunnel_channel_connect(belle_sip_channel_t *obj, const struct addrinfo *ai) {
	struct sockaddr_storage laddr;
	socklen_t lslen = sizeof(laddr);
	if (obj->local_ip == NULL) {
		belle_sip_get_src_addr_for(ai->ai_addr, ai->ai_addrlen, (struct sockaddr *)&laddr, &lslen, obj->local_port);
	}
	belle_sip_channel_set_ready(obj, (struct sockaddr *)&laddr, lslen);
	return 0;
}

static void tunnel_channel_close(belle_sip_channel_t *obj) {
	belle_sip_tunnel_channel_t *chan = (belle_sip_tunnel_channel_t *)obj;
	if( chan->tunnelsocket != NULL ){
		tunnel_client_close_socket(chan->tunnelclient, chan->tunnelsocket);
		chan->tunnelsocket = NULL;
	}
}

static void tunnel_channel_uninit(belle_sip_channel_t *obj) {
	belle_sip_tunnel_channel_t *chan = (belle_sip_tunnel_channel_t *)obj;
	if (chan->tunnelsocket != NULL) {
		tunnel_channel_close(obj);
	}
	if (chan->pollingtimer != NULL) {
		belle_sip_main_loop_remove_source(obj->stack->ml, chan->pollingtimer);
		belle_sip_object_unref(chan->pollingtimer);
		chan->pollingtimer = NULL;
	}
}

static int tunnel_polling_timer(belle_sip_tunnel_channel_t *chan) {
	if ((chan->tunnelsocket != NULL) && tunnel_socket_has_data(chan->tunnelsocket)) {
		belle_sip_channel_process_data((belle_sip_channel_t *)chan, BELLE_SIP_EVENT_READ);
	}
	return BELLE_SIP_CONTINUE;
}


BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_tunnel_channel_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_tunnel_channel_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_tunnel_channel_t,belle_sip_channel_t,FALSE),
			(belle_sip_object_destroy_t)tunnel_channel_uninit,
			NULL,
			NULL
		},
		"UDP",
		0, /*is_reliable*/
		tunnel_channel_connect,
		tunnel_channel_send,
		tunnel_channel_recv,
		tunnel_channel_close
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

belle_sip_channel_t * belle_sip_channel_new_tunnel(belle_sip_stack_t *stack, void *tunnelclient, const char *bindip, int localport, const char *dest, int port){
	belle_sip_tunnel_channel_t *obj = belle_sip_object_new(belle_sip_tunnel_channel_t);
	belle_sip_channel_init((belle_sip_channel_t*)obj, stack, bindip, localport, NULL, dest, port);
	obj->tunnelclient = tunnelclient;
	obj->tunnelsocket = tunnel_client_create_socket(tunnelclient, 5060, 6060);
	obj->pollingtimer = belle_sip_timeout_source_new((belle_sip_source_func_t)tunnel_polling_timer, obj, TUNNEL_POLLING_DURATION);
	belle_sip_object_set_name((belle_sip_object_t*)obj->pollingtimer, "tunnel_polling_timer");
	belle_sip_main_loop_add_source(stack->ml, obj->pollingtimer);
	return (belle_sip_channel_t*)obj;
}
