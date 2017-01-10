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
#include "belle_sip_internal.h"

struct belle_sip_udp_listening_point{
	belle_sip_listening_point_t base;
	belle_sip_socket_t sock;
	belle_sip_source_t *source;
};



static void belle_sip_udp_listening_point_uninit(belle_sip_udp_listening_point_t *lp){
	if (lp->source) {
		belle_sip_main_loop_remove_source(lp->base.stack->ml,lp->source);
		belle_sip_object_unref(lp->source);
		lp->source = NULL;
	}
	if (lp->sock!=-1) belle_sip_close_socket(lp->sock);
}

static belle_sip_channel_t *udp_create_channel(belle_sip_listening_point_t *lp, const belle_sip_hop_t *hop){
	belle_sip_channel_t *chan=belle_sip_channel_new_udp(lp->stack
														,(int)((belle_sip_udp_listening_point_t*)lp)->sock
														,belle_sip_uri_get_host(lp->listening_uri)
														,belle_sip_uri_get_port(lp->listening_uri)
														,hop->host
														,hop->port);
	return chan;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_udp_listening_point_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_udp_listening_point_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_udp_listening_point_t, belle_sip_listening_point_t,TRUE),
			(belle_sip_object_destroy_t)belle_sip_udp_listening_point_uninit,
			NULL,
			NULL,
			BELLE_SIP_DEFAULT_BUFSIZE_HINT
		},
		"UDP",
		udp_create_channel
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END


static belle_sip_socket_t create_udp_socket(const char *addr, int *port, int *family){
	struct addrinfo hints={0};
	struct addrinfo *res=NULL;
	int err;
	belle_sip_socket_t sock;
	char portnum[10];
	int optval=1;
	
	if (*port==-1) *port=0; /*random port for bind()*/

	snprintf(portnum,sizeof(portnum),"%i",*port);
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_DGRAM;
	hints.ai_protocol=IPPROTO_UDP;
	hints.ai_flags=AI_NUMERICSERV;
	err=getaddrinfo(addr,portnum,&hints,&res);
	if (err!=0){
		belle_sip_error("getaddrinfo() failed for %s port %i: %s",addr,*port,gai_strerror(err));
		return -1;
	}
	*family=res->ai_family;
	sock=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
	if (sock==-1){
		belle_sip_error("Cannot create UDP socket: %s",belle_sip_get_socket_error_string());
		freeaddrinfo(res);
		return -1;
	}
	err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			(char*)&optval, sizeof (optval));
	if (err == -1){
		belle_sip_warning ("Fail to set SIP/UDP address reusable: %s.", belle_sip_get_socket_error_string());
	}
	if (res->ai_family==AF_INET6){
		belle_sip_socket_enable_dual_stack(sock);
	}
	
	err=bctbx_bind(sock,res->ai_addr,(socklen_t)res->ai_addrlen);
	if (err==-1){
		belle_sip_error("udp bind() failed for %s port %i: %s",addr,*port,belle_sip_get_socket_error_string());
		belle_sip_close_socket(sock);
		freeaddrinfo(res);
		return -1;
	}
	freeaddrinfo(res);
	if (*port==0){
		struct sockaddr_storage saddr;
		socklen_t saddr_len=sizeof(saddr);
		err=getsockname(sock,(struct sockaddr*)&saddr,&saddr_len);
		if (err==0){
			err=bctbx_getnameinfo((struct sockaddr*)&saddr,saddr_len,NULL,0,portnum,sizeof(portnum),NI_NUMERICSERV|NI_NUMERICHOST);
			if (err==0){
				*port=atoi(portnum);
				belle_sip_message("Random UDP port is %i",*port);
			}else belle_sip_error("udp bind failed, getnameinfo(): %s",gai_strerror(err));
		}else belle_sip_error("udp bind failed, getsockname(): %s",belle_sip_get_socket_error_string());
	}
	return sock;
}

static int on_udp_data(belle_sip_udp_listening_point_t *lp, unsigned int events);

static int belle_sip_udp_listening_point_init_socket(belle_sip_udp_listening_point_t *lp){
	int port=belle_sip_uri_get_listening_port(((belle_sip_listening_point_t*)lp)->listening_uri);
	lp->sock=create_udp_socket(belle_sip_uri_get_host(((belle_sip_listening_point_t*)lp)->listening_uri)
					,&port,&lp->base.ai_family);
	if (lp->sock==(belle_sip_socket_t)-1){
		return -1;
	}
	belle_sip_uri_set_port(((belle_sip_listening_point_t*)lp)->listening_uri,port);
	if (lp->base.stack->dscp)
		belle_sip_socket_set_dscp(lp->sock,lp->base.ai_family,lp->base.stack->dscp);
	lp->source=belle_sip_socket_source_new((belle_sip_source_func_t)on_udp_data,lp,lp->sock,BELLE_SIP_EVENT_READ,-1);
	belle_sip_main_loop_add_source(((belle_sip_listening_point_t*)lp)->stack->ml,lp->source);
	return 0;
}

static void belle_sip_udp_listening_point_init(belle_sip_udp_listening_point_t *lp, belle_sip_stack_t *s, const char *ipaddress, int port) {
	belle_sip_listening_point_init((belle_sip_listening_point_t*)lp,s,ipaddress,port);
	belle_sip_udp_listening_point_init_socket(lp);
}

/*peek data from the master socket to see where it comes from, and dispatch to matching channel.
 * If the channel does not exist, create it */
static int on_udp_data(belle_sip_udp_listening_point_t *lp, unsigned int events){
	int err;
	unsigned char buf[4096];
	struct sockaddr_storage addr;
	socklen_t addrlen=sizeof(addr);

	if (events & BELLE_SIP_EVENT_READ){
		belle_sip_debug("udp_listening_point: data to read.");
		err=recvfrom(lp->sock,(char*)buf,sizeof(buf),MSG_PEEK,(struct sockaddr*)&addr,&addrlen);
		if (err==-1){
			char *tmp=belle_sip_object_to_string((belle_sip_object_t*) ((belle_sip_listening_point_t*)lp)->listening_uri);
			belle_sip_error("udp_listening_point: recvfrom() failed on [%s], : [%s] reopening server socket"
					,tmp
					,belle_sip_get_socket_error_string());
			belle_sip_free(tmp);
			belle_sip_udp_listening_point_uninit(lp);
			/*clean all udp channels that are actually sharing the server socket with the listening points*/
			belle_sip_listening_point_clean_channels((belle_sip_listening_point_t*)lp);
			belle_sip_udp_listening_point_init_socket(lp);
		}else{
			belle_sip_channel_t *chan;
			struct addrinfo ai={0};
			/*preserve the V4 mapping*/
			ai.ai_family=addr.ss_family;
			ai.ai_addr=(struct sockaddr*)&addr;
			ai.ai_addrlen=addrlen;
			chan=_belle_sip_listening_point_get_channel((belle_sip_listening_point_t*)lp,NULL,&ai);
			if (chan==NULL){
				/*TODO: should rather create the channel with real local ip and port and not just 0.0.0.0"*/
				chan=belle_sip_channel_new_udp_with_addr(lp->base.stack
														,(int)lp->sock
														,belle_sip_uri_get_host(lp->base.listening_uri)
														,belle_sip_uri_get_port(lp->base.listening_uri)
														,&ai);
				if (chan!=NULL){
					belle_sip_message("udp_listening_point: new channel created to %s:%i",chan->peer_name,chan->peer_port);
					belle_sip_listening_point_add_channel((belle_sip_listening_point_t*)lp,chan);
				}
			}
			if (chan){
				/*notify the channel*/
				belle_sip_debug("Notifying udp channel, local [%s:%i]  remote [%s:%i]"
						,chan->local_ip
						,chan->local_port
						,chan->peer_name
						,chan->peer_port);
				belle_sip_channel_process_data(chan,events);
			}
		}
	}
	return BELLE_SIP_CONTINUE;
}

belle_sip_listening_point_t * belle_sip_udp_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port){
	belle_sip_udp_listening_point_t *lp=belle_sip_object_new(belle_sip_udp_listening_point_t);
	belle_sip_udp_listening_point_init(lp,s,ipaddress, port);
	if (lp->sock==(belle_sip_socket_t)-1){
		belle_sip_object_unref(lp);
		return NULL;
	}
	return (belle_sip_listening_point_t*)lp;
}

