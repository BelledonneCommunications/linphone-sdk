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

#include "listeningpoint_internal.h"



void belle_sip_listening_point_init(belle_sip_listening_point_t *lp, belle_sip_stack_t *s, const char *address, int port){
	lp->port=port;
	lp->addr=belle_sip_strdup(address);
	lp->stack=s;
}

static void belle_sip_listening_point_uninit(belle_sip_listening_point_t *lp){
	int existing_channels;
	if ((existing_channels=belle_sip_list_size(lp->channels)) > 0) {
		belle_sip_warning("Listening point destroying [%i] channels",existing_channels);
	}
	belle_sip_list_free_with_data(lp->channels,(void (*)(void*))belle_sip_object_unref);
	belle_sip_free(lp->addr);
}


void belle_sip_listening_point_add_channel(belle_sip_listening_point_t *lp, belle_sip_channel_t *chan){
	lp->channels=belle_sip_list_append(lp->channels,chan);/*channel is already owned*/
}

belle_sip_channel_t *belle_sip_listening_point_create_channel(belle_sip_listening_point_t *obj, const char *dest, int port){
	belle_sip_channel_t *chan=BELLE_SIP_OBJECT_VPTR(obj,belle_sip_listening_point_t)->create_channel(obj,dest,port);
	if (chan){
		belle_sip_listening_point_add_channel(obj,chan);
	}
	return chan;
}

#if 0
static void belle_sip_listening_point_remove_channel(belle_sip_listening_point_t *lp, belle_sip_channel_t *chan){
	lp->channels=belle_sip_list_remove(lp->channels,chan);
	belle_sip_object_unref(chan);
}
#endif

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_listening_point_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_listening_point_t)={
	{ 
		BELLE_SIP_VPTR_INIT(belle_sip_listening_point_t, belle_sip_object_t,FALSE),
		(belle_sip_object_destroy_t)belle_sip_listening_point_uninit,
		NULL,
		NULL
	},
	NULL,
	NULL
};

const char *belle_sip_listening_point_get_ip_address(const belle_sip_listening_point_t *lp){
	return lp->addr;
}

int belle_sip_listening_point_get_port(const belle_sip_listening_point_t *lp){
	return lp->port;
}

const char *belle_sip_listening_point_get_transport(const belle_sip_listening_point_t *lp){
	return BELLE_SIP_OBJECT_VPTR(lp,belle_sip_listening_point_t)->transport;
}


int belle_sip_listening_point_get_well_known_port(const char *transport){
	if (strcasecmp(transport,"UDP")==0 || strcasecmp(transport,"TCP")==0 ) return 5060;
	if (strcasecmp(transport,"DTLS")==0 || strcasecmp(transport,"TLS")==0 ) return 5061;
	belle_sip_error("No well known port for transport %s", transport);
	return -1;
}

belle_sip_channel_t *_belle_sip_listening_point_get_channel(belle_sip_listening_point_t *lp,const char *peer_name, int peer_port, const struct addrinfo *addr){
	belle_sip_list_t *elem;
	belle_sip_channel_t *chan;
	
	for(elem=lp->channels;elem!=NULL;elem=elem->next){
		chan=(belle_sip_channel_t*)elem->data;
		if (belle_sip_channel_matches(chan,peer_name,peer_port,addr)){
			return chan;
		}
	}
	return NULL;
}

belle_sip_channel_t *belle_sip_listening_point_get_channel(belle_sip_listening_point_t *lp,const char *peer_name, int peer_port){
	struct addrinfo *res=NULL;
	struct addrinfo hints={0};
	char portstr[20];
	belle_sip_channel_t *chan;

	hints.ai_flags=AI_NUMERICHOST|AI_NUMERICSERV;
	snprintf(portstr,sizeof(portstr),"%i",peer_port);
	getaddrinfo(peer_name,portstr,&hints,&res);
	chan=_belle_sip_listening_point_get_channel(lp,peer_name,peer_port,res);
	if (res) freeaddrinfo(res);
	return chan;
}




