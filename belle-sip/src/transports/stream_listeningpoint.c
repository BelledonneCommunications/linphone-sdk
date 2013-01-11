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


struct belle_sip_stream_listening_point{
	belle_sip_listening_point_t base;
};


static void belle_sip_stream_listening_point_uninit(belle_sip_stream_listening_point_t *lp){
}

static belle_sip_channel_t *stream_create_channel(belle_sip_listening_point_t *lp, const char *dest_ip, int port){
	belle_sip_channel_t *chan=belle_sip_channel_new_tcp(lp->stack
														,belle_sip_uri_get_host(lp->listening_uri)
														,belle_sip_uri_get_port(lp->listening_uri)
														,dest_ip,port);
	return chan;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_stream_listening_point_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_stream_listening_point_t)={
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_stream_listening_point_t, belle_sip_listening_point_t,TRUE),
			(belle_sip_object_destroy_t)belle_sip_stream_listening_point_uninit,
			NULL,
			NULL
		},
		"TCP",
		stream_create_channel
	}
};


belle_sip_listening_point_t * belle_sip_stream_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port){
	belle_sip_stream_listening_point_t *lp=belle_sip_object_new(belle_sip_stream_listening_point_t);
	belle_sip_listening_point_init((belle_sip_listening_point_t*)lp,s,ipaddress,port);
	return BELLE_SIP_LISTENING_POINT(lp);
}
