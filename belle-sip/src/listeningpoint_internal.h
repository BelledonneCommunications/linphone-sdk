/*
	belle-sip - SIP (RFC3261) library.
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

#ifndef LISTENINGPOINT_INTERNAL_H_
#define LISTENINGPOINT_INTERNAL_H_



BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_listening_point_t,belle_sip_object_t)
const char *transport;
belle_sip_channel_t * (*create_channel)(belle_sip_listening_point_t *, const belle_sip_hop_t *hop);
BELLE_SIP_DECLARE_CUSTOM_VPTR_END


#define BELLE_SIP_LISTENING_POINT(obj) BELLE_SIP_CAST(obj,belle_sip_listening_point_t)


/*
 Listening points: base, udp
*/

struct belle_sip_listening_point{
	belle_sip_object_t base;
	belle_sip_stack_t *stack;
	belle_sip_list_t *channels;
	belle_sip_uri_t* listening_uri;
	belle_sip_source_t* keep_alive_timer;
	belle_sip_channel_listener_t* channel_listener; /*initial channel listener used for channel creation, specially for socket server*/
	int ai_family; /*AF_INET or AF_INET6*/
};

BELLE_SIP_BEGIN_DECLS
void belle_sip_listening_point_init(belle_sip_listening_point_t *lp, belle_sip_stack_t *s,  const char *address, int port);
belle_sip_channel_t *_belle_sip_listening_point_get_channel(belle_sip_listening_point_t *lp,const belle_sip_hop_t *hop, const struct addrinfo *addr);
belle_sip_channel_t *belle_sip_listening_point_create_channel(belle_sip_listening_point_t *ip, const belle_sip_hop_t *hop);
void belle_sip_listening_point_remove_channel(belle_sip_listening_point_t *lp, belle_sip_channel_t *chan);
int belle_sip_listening_point_get_well_known_port(const char *transport);
belle_sip_channel_t *belle_sip_listening_point_get_channel(belle_sip_listening_point_t *lp, const belle_sip_hop_t *hop);
void belle_sip_listening_point_add_channel(belle_sip_listening_point_t *lp, belle_sip_channel_t *chan);
void belle_sip_listening_point_set_channel_listener(belle_sip_listening_point_t *lp,belle_sip_channel_listener_t* channel_listener);
BELLE_SIP_END_DECLS

/**udp*/
typedef struct belle_sip_udp_listening_point belle_sip_udp_listening_point_t;
belle_sip_channel_t * belle_sip_channel_new_udp(belle_sip_stack_t *stack, int sock, const char *bindip, int localport, const char *peername, int peerport);
belle_sip_channel_t * belle_sip_channel_new_udp_with_addr(belle_sip_stack_t *stack, int sock, const char *bindip, int localport, const struct addrinfo *ai);
belle_sip_listening_point_t * belle_sip_udp_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port);
BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_udp_listening_point_t,belle_sip_listening_point_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END


/*stream*/
typedef struct belle_sip_stream_listening_point belle_sip_stream_listening_point_t;

struct belle_sip_stream_listening_point{
	belle_sip_listening_point_t base;
	belle_sip_socket_t server_sock;
	belle_sip_source_t *source;
};

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_stream_listening_point_t,belle_sip_listening_point_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END


void belle_sip_stream_listening_point_setup_server_socket(belle_sip_stream_listening_point_t *obj, belle_sip_source_func_t on_new_connection_cb );
void belle_sip_stream_listening_point_destroy_server_socket(belle_sip_stream_listening_point_t *lp);
void belle_sip_stream_listening_point_init(belle_sip_stream_listening_point_t *obj, belle_sip_stack_t *s, const char *ipaddress, int port, belle_sip_source_func_t on_new_connection_cb );

belle_sip_listening_point_t * belle_sip_stream_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port);

/*tls*/

struct belle_sip_tls_listening_point{
	belle_sip_stream_listening_point_t base;
	belle_tls_crypto_config_t *crypto_config;
};

int belle_sip_tls_listening_point_available(void);

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_tls_listening_point_t,belle_sip_listening_point_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END
#define BELLE_SIP_TLS_LISTENING_POINT(obj) BELLE_SIP_CAST(obj,belle_sip_tls_listening_point_t)
belle_sip_listening_point_t * belle_sip_tls_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port);
belle_sip_channel_t * belle_sip_channel_new_tls(belle_sip_stack_t *s, belle_tls_verify_policy_t* verify_ctx, const char *bindip, int localport,const char *cname, const char *name, int port);


/*tunnel*/
#ifdef HAVE_TUNNEL
typedef struct belle_sip_tunnel_listening_point belle_sip_tunnel_listening_point_t;
BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_tunnel_listening_point_t,belle_sip_listening_point_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END
#define BELLE_SIP_TUNNEL_LISTENING_POINT(obj) BELLE_SIP_CAST(obj,belle_sip_tunnel_listening_point_t)
belle_sip_channel_t * belle_sip_channel_new_tunnel(belle_sip_stack_t *s, void *tunnelclient, const char *bindip, int localport, const char *name, int port);
#endif

#include "transports/stream_channel.h"

#endif /* LISTENINGPOINT_INTERNAL_H_ */

