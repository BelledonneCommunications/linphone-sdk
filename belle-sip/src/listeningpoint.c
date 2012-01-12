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

/*
 Listening points: base, udp
*/

struct belle_sip_listening_point{
	belle_sip_object_t base;
	belle_sip_stack_t *stack;
	belle_sip_list_t *channels;
	char *transport;
	char *addr;
	int port;
	int is_reliable;
};

static void belle_sip_listening_point_init(belle_sip_listening_point_t *lp, belle_sip_stack_t *s, const char *transport, const char *address, int port){
	lp->transport=belle_sip_strdup(transport);
	lp->port=port;
	lp->addr=belle_sip_strdup(address);
	lp->stack=s;
}

static void belle_sip_listening_point_uninit(belle_sip_listening_point_t *lp){
	belle_sip_list_free_with_data(lp->channels,(void (*)(void*))belle_sip_object_unref);
	belle_sip_free(lp->addr);
	belle_sip_free(lp->transport);
}

#if 0
static void belle_sip_listening_point_add_channel(belle_sip_listening_point_t *lp, belle_sip_channel_t *chan){
	lp->channels=belle_sip_list_append(lp->channels,chan);
}

static void belle_sip_listening_point_remove_channel(belle_sip_listening_point_t *lp, belle_sip_channel_t *chan){
	lp->channels=belle_sip_list_remove(lp->channels,chan);
}
#endif

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_listening_point_t,belle_sip_object_t,belle_sip_listening_point_uninit,NULL,NULL);

const char *belle_sip_listening_point_get_ip_address(const belle_sip_listening_point_t *lp){
	return lp->addr;
}

int belle_sip_listening_point_get_port(const belle_sip_listening_point_t *lp){
	return lp->port;
}

const char *belle_sip_listening_point_get_transport(const belle_sip_listening_point_t *lp){
	return lp->transport;
}

int belle_sip_listening_point_is_reliable(const belle_sip_listening_point_t *lp){
	return lp->is_reliable;
}

int belle_sip_listening_point_get_well_known_port(const char *transport){
	if (strcasecmp(transport,"UDP")==0 || strcasecmp(transport,"TCP")==0 ) return 5060;
	if (strcasecmp(transport,"DTLS")==0 || strcasecmp(transport,"TLS")==0 ) return 5061;
	belle_sip_error("No well known port for transport %s", transport);
	return -1;
}

belle_sip_channel_t *belle_sip_listening_point_find_channel (belle_sip_listening_point_t *lp,const char *peer_name, int peer_port){
	belle_sip_list_t *elem;
	for(elem=lp->channels;elem!=NULL;elem=elem->next){
		belle_sip_channel_t *chan=(belle_sip_channel_t*)elem->data;
		if (belle_sip_channel_matches(chan,peer_name,peer_port))
			return chan;
	}
	return NULL;
}

struct belle_sip_udp_listening_point{
	belle_sip_listening_point_t base;
	belle_sip_channel_t *channel;
	int sock;
};


static void belle_sip_udp_listening_point_uninit(belle_sip_udp_listening_point_t *lp){
	belle_sip_object_unref(lp->channel);
}

BELLE_SIP_INSTANCIATE_VPTR(belle_sip_udp_listening_point_t,belle_sip_listening_point_t,belle_sip_udp_listening_point_uninit,NULL,NULL);


belle_sip_listening_point_t * belle_sip_udp_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port){
	belle_sip_udp_listening_point_t *lp=belle_sip_object_new(belle_sip_udp_listening_point_t);
	belle_sip_listening_point_init((belle_sip_listening_point_t*)lp,s,"UDP",ipaddress,port);
	lp->base.is_reliable=FALSE;
	//belle_sip_listening_point_add_channel(lp,belle_sip_channel_new_udp_master(s->provider
	return BELLE_SIP_LISTENING_POINT(lp);
}


