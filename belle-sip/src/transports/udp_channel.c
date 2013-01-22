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
#include "channel.h"

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_udp_channel_t,belle_sip_channel_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

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
	err=sendto(chan->sock,buf,buflen,0,obj->peer->ai_addr,obj->peer->ai_addrlen);
	if (err==-1){
		belle_sip_error("channel [%p]: could not send UDP packet because [%s]",strerror(errno));
		return -errno;
	}
	return err;
}

static int udp_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	belle_sip_udp_channel_t *chan=(belle_sip_udp_channel_t *)obj;
	int err;
	struct sockaddr_storage addr;
	socklen_t addrlen=sizeof(addr);
	err=recvfrom(chan->sock,buf,buflen,0,(struct sockaddr*)&addr,&addrlen);

	if (err==-1 && errno!=EWOULDBLOCK){
		belle_sip_error("Could not receive UDP packet: %s",strerror(errno));
		return -errno;
	}
	return err;
}

int udp_channel_connect(belle_sip_channel_t *obj, const struct addrinfo *ai){
	struct sockaddr_storage laddr;
	socklen_t lslen=sizeof(laddr);
	if (obj->local_ip==NULL){
		belle_sip_get_src_addr_for(ai->ai_addr,ai->ai_addrlen,(struct sockaddr*)&laddr,&lslen);
		if (lslen==sizeof(struct sockaddr_in6)){
			struct sockaddr_in6 *sin6=(struct sockaddr_in6*)&laddr;
			sin6->sin6_port=htons(obj->local_port);
		}else{
			struct sockaddr_in *sin=(struct sockaddr_in*)&laddr;
			sin->sin_port=htons(obj->local_port);
		}
	}
	belle_sip_channel_set_ready(obj,(struct sockaddr*)&laddr,lslen);
	return 0;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_udp_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_udp_channel_t)=
{
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_udp_channel_t,belle_sip_channel_t,FALSE),
			(belle_sip_object_destroy_t)udp_channel_uninit,
			NULL,
			NULL
		},
		"UDP",
		0, /*is_reliable*/
		udp_channel_connect,
		udp_channel_send,
		udp_channel_recv
	}
};

belle_sip_channel_t * belle_sip_channel_new_udp(belle_sip_stack_t *stack, int sock, const char *bindip, int localport, const char *dest, int port){
	belle_sip_udp_channel_t *obj=belle_sip_object_new(belle_sip_udp_channel_t);
	belle_sip_channel_init((belle_sip_channel_t*)obj,stack,bindip,localport,dest,port);
	obj->sock=sock;
	return (belle_sip_channel_t*)obj;
}

belle_sip_channel_t * belle_sip_channel_new_udp_with_addr(belle_sip_stack_t *stack, int sock, const char *bindip, int localport, const struct addrinfo *peer){
	belle_sip_udp_channel_t *obj=belle_sip_object_new(belle_sip_udp_channel_t);
	struct addrinfo ai;
	char name[NI_MAXHOST];
	char serv[NI_MAXSERV];
	int err;

	obj->sock=sock;
	ai=*peer;
	err=getnameinfo(ai.ai_addr,ai.ai_addrlen,name,sizeof(name),serv,sizeof(serv),NI_NUMERICHOST|NI_NUMERICSERV);
	if (err!=0){
		belle_sip_error("belle_sip_channel_new_udp_with_addr(): getnameinfo() failed: %s",gai_strerror(err));
		belle_sip_object_unref(obj);
		return NULL;
	}
	belle_sip_channel_init((belle_sip_channel_t*)obj,stack,bindip,localport,name,atoi(serv));
	err=getaddrinfo(name,serv,&ai,&obj->base.peer); /*might be optimized someway ?*/
	if (err!=0){
		belle_sip_error("getaddrinfo() failed for channel [%p] error [%s]",obj,gai_strerror(err));
	}
	return (belle_sip_channel_t*)obj;
}

