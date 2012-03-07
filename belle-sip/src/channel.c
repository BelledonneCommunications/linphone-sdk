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

void belle_sip_channel_init(belle_sip_channel_t *obj, belle_sip_stack_t *stack, int fd, belle_sip_source_func_t process_data,const char *bindip,int localport,const char *peername, int peer_port){
	obj->peer_name=belle_sip_strdup(peername);
	obj->peer_port=peer_port;
	obj->peer=NULL;
	obj->stack=stack;
	if (strcmp(bindip,"::0")!=0 && strcmp(bindip,"0.0.0.0")!=0)
		obj->local_ip=belle_sip_strdup(bindip);
	obj->local_port=localport;
		
	if (process_data) {
		belle_sip_fd_source_init((belle_sip_source_t*)obj,(belle_sip_source_func_t)process_data,obj,fd,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_ERROR,-1);
	}
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
const char * belle_sip_channel_get_transport_name_lower_case(const belle_sip_channel_t *obj){
	const char* transport = belle_sip_channel_get_transport_name(obj);
	if (strcasecmp("udp",transport)==0) return "udp";
	else if (strcasecmp("tcp",transport)==0) return "tcp";
	else if (strcasecmp("tls",transport)==0) return "tls";
	else if (strcasecmp("dtls",transport)==0) return "dtls";
	else {
		belle_sip_message("Cannot convert [%s] to lower case",transport);
		return transport;
	}
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
	belle_sip_message_t* result = obj->input_stream.msg;
	obj->input_stream.msg=NULL;
	return result;
}

void channel_set_state(belle_sip_channel_t *obj, belle_sip_channel_state_t state) {
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


void channel_process_queue(belle_sip_channel_t *obj){
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

void belle_sip_channel_set_ready(belle_sip_channel_t *obj, const struct sockaddr *addr, socklen_t slen){
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

void belle_sip_channel_resolve(belle_sip_channel_t *obj){
	channel_set_state(obj,BELLE_SIP_CHANNEL_RES_IN_PROGRESS);
	obj->resolver_id=belle_sip_resolve(obj->peer_name, obj->peer_port, 0, channel_res_done, obj, obj->stack->ml);
	return ;
}

void belle_sip_channel_connect(belle_sip_channel_t *obj){
	channel_set_state(obj,BELLE_SIP_CHANNEL_CONNECTING);
	if(BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->connect(obj,obj->peer->ai_addr,obj->peer->ai_addrlen)) {
		belle_sip_error("Cannot connect to [%s://%s:%s]",belle_sip_channel_get_transport_name(obj),obj->peer_name,obj->peer_port);
		channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
		channel_process_queue(obj);
	}
	return;
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



