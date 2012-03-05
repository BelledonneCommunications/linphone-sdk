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

static const char *channel_state_to_string(belle_sip_channel_state_t state){
	switch(state){
		case BELLE_SIP_CHANNEL_INIT:
			return "INIT";
		case BELLE_SIP_CHANNEL_RES_IN_PROGRESS:
			return "RES_IN_PROGRESS";
		case BELLE_SIP_CHANNEL_RES_DONE:
			return "RES_DONE";
		case BELLE_SIP_CHANNEL_CONNECTING:
			return "CONNECTING";
		case BELLE_SIP_CHANNEL_READY:
			return "READY";
		case BELLE_SIP_CHANNEL_ERROR:
			return "ERROR";
	}
	return "BAD";
}

static void belle_sip_channel_destroy(belle_sip_channel_t *obj){
	if (obj->peer) freeaddrinfo(obj->peer);
	belle_sip_free(obj->peer_name);
	if (obj->local_ip) belle_sip_free(obj->local_ip);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_channel_t)=
{
	{
		BELLE_SIP_VPTR_INIT(belle_sip_channel_t,belle_sip_source_t,FALSE),
		(belle_sip_object_destroy_t)belle_sip_channel_destroy,
		NULL, /*clone*/
		NULL, /*marshall*/
	}
};
static void fix_incoming_via(belle_sip_request_t *msg, const struct addrinfo* origin){
	char received[NI_MAXHOST];
	char rport[NI_MAXSERV];
	belle_sip_header_via_t *via;
	int err=getnameinfo(origin->ai_addr,origin->ai_addrlen,received,sizeof(received),
	                rport,sizeof(rport),NI_NUMERICHOST|NI_NUMERICSERV);
	if (err!=0){
		belle_sip_error("fix_via: getnameinfo() failed: %s",gai_strerror(errno));
		return;
	}
	via=BELLE_SIP_HEADER_VIA(belle_sip_message_get_header((belle_sip_message_t*)msg,"via"));
	if (via){
		belle_sip_header_via_set_received(via,received);
		belle_sip_header_via_set_rport(via,atoi(rport));
	}
}
void belle_sip_channel_process_data(belle_sip_channel_t *obj,unsigned int revents){
	int err;
	err=belle_sip_channel_recv(obj,&obj->input_stream.buff,MAX_CHANNEL_BUFF_SIZE-1);
	if (err>0){
		obj->input_stream.buff[err]='\0';
		belle_sip_message("read message from %s:%i\n%s",obj->peer_name,obj->peer_port,&obj->input_stream.buff);
		obj->input_stream.msg=belle_sip_message_parse(obj->input_stream.buff);
		if (obj->input_stream.msg){
			if (belle_sip_message_is_request(obj->input_stream.msg)) fix_incoming_via(BELLE_SIP_REQUEST(obj->input_stream.msg),obj->peer);
		}else{
			belle_sip_error("Could not parse this message.");
		}
	}
	BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(obj->listeners,belle_sip_channel_listener_t,on_event,obj,revents);
}

static void belle_sip_channel_init(belle_sip_channel_t *obj, belle_sip_stack_t *stack, int fd, const char *bindip,int localport,const char *peername, int peer_port){
	obj->peer_name=belle_sip_strdup(peername);
	obj->peer_port=peer_port;
	obj->peer=NULL;
	obj->stack=stack;
	if (strcmp(bindip,"::0")!=0 && strcmp(bindip,"0.0.0.0")!=0)
		obj->local_ip=belle_sip_strdup(bindip);
	obj->local_port=localport;
		
	belle_sip_fd_source_init((belle_sip_source_t*)obj,(belle_sip_source_func_t)belle_sip_channel_process_data,obj,fd,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_ERROR,-1);
}

void belle_sip_channel_add_listener(belle_sip_channel_t *obj, belle_sip_channel_listener_t *l){
	obj->listeners=belle_sip_list_append(obj->listeners,
	                belle_sip_object_weak_ref(l,
	                (belle_sip_object_destroy_notify_t)belle_sip_channel_remove_listener,obj));
}

void belle_sip_channel_remove_listener(belle_sip_channel_t *obj, belle_sip_channel_listener_t *l){
	belle_sip_object_weak_unref(l,(belle_sip_object_destroy_notify_t)belle_sip_channel_remove_listener,obj);
	obj->listeners=belle_sip_list_remove(obj->listeners,l);
}

int belle_sip_channel_matches(const belle_sip_channel_t *obj, const char *peername, int peerport, const struct addrinfo *addr){
	if (peername && strcmp(peername,obj->peer_name)==0 && peerport==obj->peer_port)
		return 1;
	if (addr && obj->peer) 
		return addr->ai_addrlen==obj->peer->ai_addrlen && memcmp(addr->ai_addr,obj->peer->ai_addr,addr->ai_addrlen)==0;
	return 0;
}

const char *belle_sip_channel_get_local_address(belle_sip_channel_t *obj, int *port){
	if (port) *port=obj->local_port;
	return obj->local_ip;
}

int belle_sip_channel_is_reliable(const belle_sip_channel_t *obj){
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->reliable;
}

const char * belle_sip_channel_get_transport_name(const belle_sip_channel_t *obj){
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->transport;
}

int belle_sip_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen){
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->channel_send(obj,buf,buflen);
}

int belle_sip_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->channel_recv(obj,buf,buflen);
}

const struct addrinfo * belle_sip_channel_get_peer(belle_sip_channel_t *obj){
	return obj->peer;
}

belle_sip_message_t* belle_sip_channel_pick_message(belle_sip_channel_t *obj) {
	/*FIXME should be synchronized*/
	belle_sip_message_t* result = obj->input_stream.msg;
	obj->input_stream.msg=NULL;
	return result;
}

static void channel_set_state(belle_sip_channel_t *obj, belle_sip_channel_state_t state){
	belle_sip_message("channel %p: state %s",obj,channel_state_to_string(state));
	obj->state=state;
	BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(obj->listeners,belle_sip_channel_listener_t,on_state_changed,obj,state);
}

static void send_message(belle_sip_channel_t *obj, belle_sip_message_t *msg){
	char buffer[belle_sip_network_buffer_size];
	int len;
	BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(obj->listeners,belle_sip_channel_listener_t,on_sending,obj,msg);
	len=belle_sip_object_marshal((belle_sip_object_t*)msg,buffer,0,sizeof(buffer));
	if (len>0){
		int ret=belle_sip_channel_send(obj,buffer,len);
		if (ret==-1){
			channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
		}else{
			belle_sip_message("channel %p: message sent: \n%s",obj,buffer);
		}
	}
}


static void channel_process_queue(belle_sip_channel_t *obj){
	if (obj->msg){
		switch(obj->state){
			case BELLE_SIP_CHANNEL_INIT:
				belle_sip_channel_resolve(obj);
			break;
			case BELLE_SIP_CHANNEL_RES_DONE:
				belle_sip_channel_connect(obj);
			break;
			case BELLE_SIP_CHANNEL_READY:
				send_message(obj, obj->msg);
			case BELLE_SIP_CHANNEL_ERROR:
				belle_sip_object_unref(obj->msg);
				obj->msg=NULL;
			break;
			default:
			break;
		}
	}
}

static void belle_sip_channel_set_ready(belle_sip_channel_t *obj, const struct sockaddr *addr, socklen_t slen){
	char name[NI_MAXHOST];
	char serv[NI_MAXSERV];

	if (obj->local_ip==NULL){
		int err=getnameinfo(addr,slen,name,sizeof(name),serv,sizeof(serv),NI_NUMERICHOST|NI_NUMERICSERV);
		if (err!=0){
			belle_sip_error("belle_sip_channel_connect(): getnameinfo() failed: %s",gai_strerror(err));
		}else{
			obj->local_ip=belle_sip_strdup(name);
			obj->local_port=atoi(serv);
			belle_sip_message("Channel has local address %s:%s",name,serv);
		}
	}
	channel_set_state(obj,BELLE_SIP_CHANNEL_READY);
	channel_process_queue(obj);
}

static void channel_res_done(void *data, const char *name, struct addrinfo *res){
	belle_sip_channel_t *obj=(belle_sip_channel_t*)data;
	obj->resolver_id=0;
	if (res){
		obj->peer=res;
		channel_set_state(obj,BELLE_SIP_CHANNEL_RES_DONE);
	}else{
		channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
	}
	channel_process_queue(obj);
}

int belle_sip_channel_resolve(belle_sip_channel_t *obj){
	channel_set_state(obj,BELLE_SIP_CHANNEL_RES_IN_PROGRESS);
	obj->resolver_id=belle_sip_resolve(obj->peer_name, obj->peer_port, 0, channel_res_done, obj, obj->stack->ml);
	return 0;
}

int belle_sip_channel_connect(belle_sip_channel_t *obj){
	int ret=BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->connect(obj,obj->peer->ai_addr,obj->peer->ai_addrlen);
	return ret;
}

int belle_sip_channel_queue_message(belle_sip_channel_t *obj, belle_sip_message_t *msg){
	if (obj->msg!=NULL){
		belle_sip_error("Queue is not a queue.");
		return -1;
	}
	obj->msg=(belle_sip_message_t*)belle_sip_object_ref(msg);
	channel_process_queue(obj);
	return 0;
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
	err=sendto(chan->sock,buf,buflen,0,obj->peer->ai_addr,obj->peer->ai_addrlen);
	if (err==-1){
		belle_sip_fatal("Could not send UDP packet: %s",strerror(errno));
		return -errno;
	}
	return err;
}

static int udp_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	belle_sip_udp_channel_t *chan=(belle_sip_udp_channel_t *)obj;
	int err;
	struct sockaddr_storage addr;
	socklen_t addrlen=sizeof(addr);
	err=recvfrom(chan->sock,buf,buflen,MSG_DONTWAIT,(struct sockaddr*)&addr,&addrlen);
	if (err==-1 && errno!=EWOULDBLOCK){
		belle_sip_error("Could not receive UDP packet: %s",strerror(errno));
		return -errno;
	}
	return err;
}

int udp_channel_connect(belle_sip_channel_t *obj, const struct sockaddr *addr, socklen_t socklen){
	struct sockaddr_storage laddr;
	socklen_t lslen=sizeof(laddr);
	if (obj->local_ip==NULL){
		belle_sip_get_src_addr_for(addr,socklen,(struct sockaddr*)&laddr,&lslen);
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
	belle_sip_channel_init((belle_sip_channel_t*)obj,stack,sock,bindip,localport,dest,port);
	obj->sock=sock;
	return (belle_sip_channel_t*)obj;
}

belle_sip_channel_t * belle_sip_channel_new_udp_with_addr(belle_sip_stack_t *stack, int sock, const char *bindip, int localport, const struct addrinfo *peer){
	belle_sip_udp_channel_t *obj=belle_sip_object_new(belle_sip_udp_channel_t);
	struct addrinfo *ai=belle_sip_new0(struct addrinfo);
	char name[NI_MAXHOST];
	char serv[NI_MAXSERV];
	int err;
	
	obj->sock=sock;
	*ai=*peer;
	err=getnameinfo(ai->ai_addr,ai->ai_addrlen,name,sizeof(name),serv,sizeof(serv),NI_NUMERICHOST|NI_NUMERICSERV);
	if (err!=0){
		belle_sip_error("belle_sip_channel_new_udp_with_addr(): getnameinfo() failed: %s",gai_strerror(err));
		belle_sip_object_unref(obj);
		return NULL;
	}
	belle_sip_channel_init((belle_sip_channel_t*)obj,stack,sock,bindip,localport,name,atoi(serv));
	return (belle_sip_channel_t*)obj;
}



