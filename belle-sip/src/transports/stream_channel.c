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
#include "belle-sip/mainloop.h"
#include "stream_channel.h"

/*************TCP********/

static int stream_channel_process_data(belle_sip_channel_t *obj,unsigned int revents);


static void stream_channel_uninit(belle_sip_stream_channel_t *obj){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	if (sock!=(belle_sip_socket_t)-1) stream_channel_close((belle_sip_channel_t*)obj);
}

int stream_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	int err;
	err=send(sock,buf,buflen,0);
	if (err==(belle_sip_socket_t)-1){
		belle_sip_error("Could not send stream packet on channel [%p]: %s",obj,strerror(errno));
		return -errno;
	}
	return err;
}

int stream_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	int err;
	err=recv(sock,buf,buflen,0);
	if (err==(belle_sip_socket_t)-1){
		belle_sip_error("Could not receive stream packet: %s",strerror(errno));
		return -errno;
	}
	return err;
}

void stream_channel_close(belle_sip_channel_t *obj){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	if (sock!=(belle_sip_socket_t)-1){
		close_socket(sock);
		belle_sip_source_uninit((belle_sip_source_t*)obj);
	}
}

int stream_channel_connect(belle_sip_channel_t *obj, const struct addrinfo *ai){
	int err;
	int tmp;
	belle_sip_socket_t sock;
	tmp=1;
	
	sock=socket(ai->ai_family, SOCK_STREAM, IPPROTO_TCP);
	
	if (sock==(belle_sip_socket_t)-1){
		belle_sip_error("Could not create socket: %s",belle_sip_get_socket_error_string());
		return -1;
	}
	
	err=setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,(char*)&tmp,sizeof(tmp));
	if (err!=0){
		belle_sip_error("setsockopt TCP_NODELAY failed: [%s]",belle_sip_get_socket_error_string());
	}
	belle_sip_socket_set_nonblocking(sock);
	belle_sip_channel_set_socket(obj,sock,(belle_sip_source_func_t)stream_channel_process_data);
	belle_sip_source_set_events((belle_sip_source_t*)obj,BELLE_SIP_EVENT_WRITE|BELLE_SIP_EVENT_ERROR);
	
	err = connect(sock,ai->ai_addr,ai->ai_addrlen);
	if (err != 0 && get_socket_error()!=EINPROGRESS && get_socket_error()!=EWOULDBLOCK) {
		belle_sip_error("stream connect failed %s",belle_sip_get_socket_error_string());
		close_socket(sock);
		return -1;
	}
	belle_sip_main_loop_add_source(obj->stack->ml,(belle_sip_source_t*)obj);

	return 0;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_stream_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_stream_channel_t)=
{
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_stream_channel_t,belle_sip_channel_t,FALSE),
			(belle_sip_object_destroy_t)stream_channel_uninit,
			NULL,
			NULL
		},
		"TCP",
		1, /*is_reliable*/
		stream_channel_connect,
		stream_channel_send,
		stream_channel_recv,
		stream_channel_close,
	}
};

int finalize_stream_connection (belle_sip_socket_t sock, struct sockaddr *addr, socklen_t* slen) {
	int err, errnum;
	socklen_t optlen=sizeof(errnum);
	err=getsockopt(sock,SOL_SOCKET,SO_ERROR,(void*)&errnum,&optlen);
	if (err!=0){
		belle_sip_error("Failed to retrieve connection status for fd [%i]: cause [%s]",sock,belle_sip_get_socket_error_string());
		return -1;
	}else{
		if (errnum==0){
			/*obtain bind address for client*/
			err=getsockname(sock,addr,slen);
			if (err<0){
				belle_sip_error("Failed to retrieve sockname  for fd [%i]: cause [%s]",sock,belle_sip_get_socket_error_string());
				return -1;
			}
			return 0;
		}else{
			belle_sip_error("Connection failed  for fd [%i]: cause [%s]",sock,belle_sip_get_socket_error_string_from_code(errnum));
			return -1;
		}
	}
}
static int stream_channel_process_data(belle_sip_channel_t *obj,unsigned int revents){
	struct sockaddr_storage ss;
	socklen_t addrlen=sizeof(ss);
	belle_sip_socket_t fd=belle_sip_source_get_socket((belle_sip_source_t*)obj);

	belle_sip_message("TCP channel process_data");
	
	if (obj->state == BELLE_SIP_CHANNEL_CONNECTING && (revents&BELLE_SIP_EVENT_WRITE)) {

		if (finalize_stream_connection(fd,(struct sockaddr*)&ss,&addrlen)) {
			belle_sip_error("Cannot connect to [%s://%s:%s]",belle_sip_channel_get_transport_name(obj),obj->peer_name,obj->peer_port);
			channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
			channel_process_queue(obj);
			return BELLE_SIP_STOP;
		}
		belle_sip_source_set_events((belle_sip_source_t*)obj,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_ERROR);
		belle_sip_channel_set_ready(obj,(struct sockaddr*)&ss,addrlen);
		return BELLE_SIP_CONTINUE;

	} else if ( obj->state == BELLE_SIP_CHANNEL_READY) {
		return belle_sip_channel_process_data(obj,revents);
	} else {
		belle_sip_warning("Unexpected event [%i], in state [%s] for channel [%p]",revents,belle_sip_channel_state_to_string(obj->state),obj);
		return BELLE_SIP_STOP;
	}
	return BELLE_SIP_CONTINUE;
}

belle_sip_channel_t * belle_sip_channel_new_tcp(belle_sip_stack_t *stack,const char *bindip, int localport, const char *dest, int port){
	belle_sip_stream_channel_t *obj=belle_sip_object_new(belle_sip_stream_channel_t);
	belle_sip_channel_init((belle_sip_channel_t*)obj
							,stack
							,bindip,localport,dest,port);
	return (belle_sip_channel_t*)obj;
}















