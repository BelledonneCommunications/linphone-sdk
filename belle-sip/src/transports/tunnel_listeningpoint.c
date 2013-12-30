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

struct belle_sip_tunnel_listening_point{
	belle_sip_listening_point_t base;
	void *tunnelclient;
};


static belle_sip_channel_t *tunnel_create_channel(belle_sip_listening_point_t *lp, const belle_sip_hop_t *hop){
	belle_sip_channel_t *chan=belle_sip_channel_new_tunnel(lp->stack, ((belle_sip_tunnel_listening_point_t*)lp)->tunnelclient,
								belle_sip_uri_get_host(lp->listening_uri), belle_sip_uri_get_port(lp->listening_uri),
								hop->host, hop->port);
	return chan;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_tunnel_listening_point_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_tunnel_listening_point_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_tunnel_listening_point_t, belle_sip_listening_point_t,TRUE),
			NULL,
			NULL,
			NULL
		},
		"UDP",
		tunnel_create_channel
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END


static void belle_sip_tunnel_listening_point_init(belle_sip_tunnel_listening_point_t *lp, belle_sip_stack_t *s, void *tunnelclient) {
	belle_sip_listening_point_init((belle_sip_listening_point_t*)lp,s,"0.0.0.0",5060);
	lp->tunnelclient = tunnelclient;
}


belle_sip_listening_point_t * belle_sip_tunnel_listening_point_new(belle_sip_stack_t *s, void *tunnelclient){
	belle_sip_tunnel_listening_point_t *lp=belle_sip_object_new(belle_sip_tunnel_listening_point_t);
	belle_sip_tunnel_listening_point_init(lp,s,tunnelclient);
	return (belle_sip_listening_point_t*)lp;
}
