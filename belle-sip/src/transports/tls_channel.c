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


#include <sys/socket.h>
#include <netinet/tcp.h>

#include "belle_sip_internal.h"
#include "belle-sip/mainloop.h"
#include "stream_channel.h"

/*************tls********/

struct belle_sip_tls_channel{
	belle_sip_channel_t base;
};


static void tls_channel_uninit(belle_sip_tls_channel_t *obj){
	belle_sip_fd_t sock = belle_sip_source_get_fd((belle_sip_source_t*)obj);
	if (sock!=-1)
		close_socket(sock);
	 belle_sip_main_loop_remove_source(obj->base.stack->ml,(belle_sip_source_t*)obj);
}

static int tls_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen){
	belle_sip_fd_t sock = belle_sip_source_get_fd((belle_sip_source_t*)obj);
	int err;
	err=send(sock,buf,buflen,0);
	if (err==-1){
		belle_sip_fatal("Could not send tls packet on channel [%p]: %s",obj,strerror(errno));
		return -errno;
	}
	return err;
}

static int tls_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	belle_sip_fd_t sock = belle_sip_source_get_fd((belle_sip_source_t*)obj);
	int err;
	err=recv(sock,buf,buflen,MSG_DONTWAIT);
	if (err==-1 && errno!=EWOULDBLOCK){
		belle_sip_error("Could not receive tls packet: %s",strerror(errno));
		return -errno;
	}
	return err;
}

int tls_channel_connect(belle_sip_channel_t *obj, const struct sockaddr *addr, socklen_t socklen){
	return stream_channel_connect(obj,addr,socklen);
}

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_tls_channel_t,belle_sip_channel_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_tls_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_tls_channel_t)=
{
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_tls_channel_t,belle_sip_channel_t,FALSE),
			(belle_sip_object_destroy_t)tls_channel_uninit,
			NULL,
			NULL
		},
		"TLS",
		1, /*is_reliable*/
		tls_channel_connect,
		tls_channel_send,
		tls_channel_recv
	}
};

static int process_data(belle_sip_channel_t *obj,unsigned int revents){
	struct sockaddr_storage ss;
	socklen_t addrlen=sizeof(ss);
	belle_sip_fd_t fd=belle_sip_source_get_fd((belle_sip_source_t*)obj);
	if (obj->state == BELLE_SIP_CHANNEL_CONNECTING && (revents&BELLE_SIP_EVENT_WRITE)) {

		if (finalize_stream_connection(fd,(struct sockaddr*)&ss,&addrlen)) {
			belle_sip_error("Cannot connect to [%s://%s:%s]",belle_sip_channel_get_transport_name(obj),obj->peer_name,obj->peer_port);
			channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
			channel_process_queue(obj);
			return BELLE_SIP_STOP;
		}
		/*connected, now etablishing TLS connection*/
		belle_sip_source_set_events((belle_sip_source_t*)obj,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_ERROR);
		belle_sip_channel_set_ready(obj,(struct sockaddr*)&ss,addrlen);
		return BELLE_SIP_CONTINUE;

	} else if ( obj->state == BELLE_SIP_CHANNEL_READY) {
		belle_sip_channel_process_data(obj,revents);
	} else {
		belle_sip_warning("Unexpected event [%i], for channel [%p]",revents,obj);
	}
	return BELLE_SIP_CONTINUE;

}
belle_sip_channel_t * belle_sip_channel_new_tls(belle_sip_stack_t *stack,const char *bindip, int localport, const char *dest, int port){
	belle_sip_tls_channel_t *obj=belle_sip_object_new(belle_sip_tls_channel_t);
	belle_sip_channel_init((belle_sip_channel_t*)obj
							,stack
							,socket(AF_INET, SOCK_STREAM, 0)
							,(belle_sip_source_func_t)process_data
							,bindip,localport,dest,port);
	return (belle_sip_channel_t*)obj;
}















