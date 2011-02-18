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

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#else

#endif

/*
 Channels: udp
*/



typedef struct belle_sip_channel_vptr{
	int (*channel_send)(belle_sip_channel_t *obj, const void *buf, size_t buflen);
	int (*channel_recv)(belle_sip_channel_t *obj, void *buf, size_t buflen);
	belle_sip_source_t *(*create_source)(belle_sip_channel_t *obj, unsigned int events, unsigned int timeout, belle_sip_source_func_t callback, void *data);
}belle_sip_channel_vptr_t;

static void belle_sip_channel_init(belle_sip_channel_t *obj, belle_sip_listening_point_t *lp){
	belle_sip_object_init_type(obj,belle_sip_channel_t);
	obj->lp=lp;
	obj->peer.ai_addr=(struct sockaddr*)&obj->peer_addr;
}

int belle_sip_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen){
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_vptr_t)->channel_send(obj,buf,buflen);
}

int belle_sip_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_vptr_t)->channel_recv(obj,buf,buflen);
}

belle_sip_source_t *belle_sip_channel_create_source(belle_sip_channel_t *obj, unsigned int events, int timeout, belle_sip_source_func_t callback, void *data){
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_vptr_t)->create_source(obj,events,timeout,callback,data);
}


const struct addrinfo * belle_sip_channel_get_peer(belle_sip_channel_t *obj){
	return &obj->peer;
}

struct belle_sip_udp_channel{
	belle_sip_channel_t base;
	int sock;
};

typedef struct belle_sip_udp_channel belle_sip_udp_channel_t;

static void udp_channel_uninit(belle_sip_udp_channel_t *obj){
	if (obj->sock!=-1)
		close(obj->sock);
}

static int udp_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen){
	belle_sip_udp_channel_t *chan=(belle_sip_udp_channel_t *)obj;
	int err;
	err=sendto(chan->sock,buf,buflen,0,obj->peer.ai_addr,obj->peer.ai_addrlen);
	if (err==-1){
		belle_sip_error("Could not send UDP packet: %s",strerror(errno));
		return -errno;
	}
	return err;
}

static int udp_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	belle_sip_udp_channel_t *chan=(belle_sip_udp_channel_t *)obj;
	int err;
	obj->peer.ai_addrlen=sizeof(obj->peer_addr);
	err=recvfrom(chan->sock,buf,buflen,MSG_DONTWAIT,obj->peer.ai_addr,&obj->peer.ai_addrlen);
	if (err==-1 && errno!=EWOULDBLOCK){
		belle_sip_error("Could not receive UDP packet: %s",strerror(errno));
		return -errno;
	}
	return err;
}

static belle_sip_source_t *udp_channel_create_source(belle_sip_channel_t *obj, unsigned int events, unsigned int timeout, belle_sip_source_func_t callback, void *data){
	belle_sip_udp_channel_t *chan=(belle_sip_udp_channel_t *)obj;
	return belle_sip_fd_source_new(callback,data,chan->sock,events,timeout);
}

static belle_sip_channel_vptr_t udp_channel_vptr={
	udp_channel_send,
	udp_channel_recv,
	udp_channel_create_source
};

static int create_udp_socket(const char *addr, int port){
	struct addrinfo hints={0};
	struct addrinfo *res=NULL;
	int err;
	int sock;
	char portnum[10];
	
	sock=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if (sock==-1){
		belle_sip_error("Cannot create UDP socket: %s",strerror(errno));
		return -1;
	}
	snprintf(portnum,sizeof(portnum),"%i",port);
	err=getaddrinfo(addr,portnum,&hints,&res);
	if (err!=0){
		belle_sip_error("getaddrinfo() failed for %s port %i: %s",addr,port,strerror(errno));
		close(sock);
		return -1;
	}
	err=bind(sock,res->ai_addr,res->ai_addrlen);
	if (err==-1){
		belle_sip_error("udp bind() failed for %s port %i: %s",addr,port,strerror(errno));
		close(sock);
		return -1;
	}
	return sock;
}

belle_sip_udp_channel_t *belle_sip_udp_channel_new(belle_sip_listening_point_t *lp, int sock, const struct addrinfo *dest){
	belle_sip_udp_channel_t *obj=belle_sip_object_new_with_vptr(belle_sip_udp_channel_t,&udp_channel_vptr,udp_channel_uninit);
	belle_sip_channel_init((belle_sip_channel_t*)obj,lp);
	obj->sock=sock;
	memcpy(obj->base.peer.ai_addr,dest->ai_addr,dest->ai_addrlen);
	obj->base.peer.ai_addrlen=dest->ai_addrlen;
	return obj;
}

/*
 Listening points: base, udp
*/

struct belle_sip_listening_point{
	belle_sip_object_t base;
	belle_sip_stack_t *stack;
	char *transport;
	char *addr;
	int port;
	int is_reliable;
};

typedef struct belle_sip_listening_point_vptr{
	belle_sip_channel_t * (*find_output_channel)(belle_sip_listening_point_t *lp,const struct addrinfo *dest);
} belle_sip_listening_point_vptr_t;

static void belle_sip_listening_point_init(belle_sip_listening_point_t *lp, belle_sip_stack_t *s, const char *transport, const char *address, int port){
	belle_sip_object_init_type(lp,belle_sip_listening_point_t);
	lp->transport=belle_sip_strdup(transport);
	lp->port=port;
	lp->addr=belle_sip_strdup(address);
	lp->stack=s;
}

static void belle_sip_listening_point_uninit(belle_sip_listening_point_t *lp){
	belle_sip_free(lp->addr);
	belle_sip_free(lp->transport);
}

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

belle_sip_channel_t *belle_sip_listening_point_find_output_channel (belle_sip_listening_point_t *lp,const struct addrinfo *dest){
	return ((belle_sip_listening_point_vptr_t*)lp->base.vptr)->find_output_channel(lp,dest);
}

struct belle_sip_udp_listening_point{
	belle_sip_listening_point_t base;
	belle_sip_udp_channel_t *channel;
	int sock;
};


static belle_sip_channel_t *udp_listening_point_find_output_channel(belle_sip_listening_point_t* obj,const struct addrinfo *dest){
	belle_sip_udp_listening_point_t *lp=(belle_sip_udp_listening_point_t*)obj;
	belle_sip_udp_channel_t * chan=belle_sip_udp_channel_new (obj,lp->sock,dest);
	return BELLE_SIP_CHANNEL(chan);
}

static void belle_sip_udp_listening_point_uninit(belle_sip_udp_listening_point_t *lp){
	belle_sip_object_unref(lp->channel);
	belle_sip_listening_point_uninit((belle_sip_listening_point_t*)lp);
}

static belle_sip_listening_point_vptr_t udp_listening_point_vptr={
	udp_listening_point_find_output_channel
};


belle_sip_listening_point_t * belle_sip_udp_listening_point_new(belle_sip_stack_t *s, const char *ipaddress, int port){
	belle_sip_udp_listening_point_t *lp=belle_sip_object_new_with_vptr(belle_sip_udp_listening_point_t,&udp_listening_point_vptr,belle_sip_udp_listening_point_uninit);
	belle_sip_listening_point_init((belle_sip_listening_point_t*)lp,s,"UDP",ipaddress,port);
	lp->sock=create_udp_socket(ipaddress,port);
	if (lp->sock==-1){
		belle_sip_object_unref(s);
		return NULL;
	}
	lp->base.is_reliable=FALSE;
	return BELLE_SIP_LISTENING_POINT(lp);
}


