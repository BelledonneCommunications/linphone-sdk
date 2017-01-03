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
#include "belle-sip/mainloop.h"
#include "stream_channel.h"

static void set_tcp_nodelay(belle_sip_socket_t sock){
	int tmp=1;
	int err=setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,(char*)&tmp,sizeof(tmp));
	if (err == -1){
		belle_sip_warning ("Fail to set TCP_NODELAY: %s.", belle_sip_get_socket_error_string());
	}
}

/*************TCP********/

static int stream_channel_process_data(belle_sip_stream_channel_t *obj,unsigned int revents);


static void stream_channel_uninit(belle_sip_stream_channel_t *obj){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	if (sock!=(belle_sip_socket_t)-1) stream_channel_close(obj);
}

int stream_channel_send(belle_sip_stream_channel_t *obj, const void *buf, size_t buflen){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	int err=bctbx_send(sock,buf,buflen,0);
	if (err==(belle_sip_socket_t)-1){
		int errnum=get_socket_error();
		if (!belle_sip_error_code_is_would_block(errnum)){
			belle_sip_error("Could not send stream packet on channel [%p]: %s",obj,belle_sip_get_socket_error_string_from_code(errnum));
		}
		return -errnum;
	}
	return err;
}

int stream_channel_recv(belle_sip_stream_channel_t *obj, void *buf, size_t buflen){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	int err=bctbx_recv(sock,buf,buflen,0);

	if (err==(belle_sip_socket_t)-1){
		int errnum=get_socket_error();
		if (errnum == BCTBX_ENOTCONN) { //Do NOT treat it as an error
			belle_sip_message("Socket is not connected because of IOS10 background policy");
			obj->base.closed_by_remote = TRUE;
			return 0;
		}

		if (!belle_sip_error_code_is_would_block(errnum)){
			belle_sip_error("Could not receive stream packet: %s",belle_sip_get_socket_error_string_from_code(errnum));
		}
		return -errnum;
	}
	return err;
}

void stream_channel_close(belle_sip_stream_channel_t *obj){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	if (sock!=(belle_sip_socket_t)-1){
#if TARGET_OS_IPHONE
		if (obj->read_stream != NULL) {
			CFReadStreamClose (obj->read_stream);
			CFRelease (obj->read_stream);
			obj->read_stream=NULL;
		}
		if (obj->write_stream != NULL) {
			CFWriteStreamClose (obj->write_stream);
			CFRelease (obj->write_stream);
			obj->write_stream=NULL;
		}
#endif
		belle_sip_close_socket(sock);
	}
}

#if TARGET_OS_IPHONE
static void stream_channel_enable_ios_background_mode(belle_sip_stream_channel_t *obj){
	int sock=belle_sip_source_get_socket((belle_sip_source_t*)obj);
	
	CFStreamCreatePairWithSocket(kCFAllocatorDefault, sock, &obj->read_stream, &obj->write_stream);
	if (obj->read_stream){
		if (!CFReadStreamSetProperty (obj->read_stream, kCFStreamNetworkServiceType, kCFStreamNetworkServiceTypeVoIP)){
			belle_sip_warning("CFReadStreamSetProperty() could not set VoIP service type on read stream.");
		}
	}else belle_sip_warning("CFStreamCreatePairWithSocket() could not create the read stream.");
	if (obj->write_stream){
		if (!CFWriteStreamSetProperty (obj->write_stream, kCFStreamNetworkServiceType, kCFStreamNetworkServiceTypeVoIP)){
			belle_sip_warning("CFReadStreamSetProperty() could not set VoIP service type on write stream.");
		}
	}else belle_sip_warning("CFStreamCreatePairWithSocket() could not create the write stream.");
	
	if (!CFReadStreamOpen (obj->read_stream)) {
		belle_sip_warning("CFReadStreamOpen() failed.");
	}
	
	if (!CFWriteStreamOpen (obj->write_stream)) {
		belle_sip_warning("CFWriteStreamOpen() failed.");
	}
}

#endif

int stream_channel_connect(belle_sip_stream_channel_t *obj, const struct addrinfo *ai){
	int err;
	int tmp;
	belle_sip_socket_t sock;
	tmp=1;
	
	obj->base.ai_family=ai->ai_family;
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
	if (ai->ai_family==AF_INET6){
		belle_sip_socket_enable_dual_stack(sock);
	}
	
	err = bctbx_connect(sock,ai->ai_addr,(socklen_t)ai->ai_addrlen);
	if (err != 0 && get_socket_error()!=BELLESIP_EINPROGRESS && get_socket_error()!=BELLESIP_EWOULDBLOCK) {
		belle_sip_error("stream connect failed %s",belle_sip_get_socket_error_string());
		belle_sip_close_socket(sock);
		return -1;
	}
	belle_sip_channel_set_socket((belle_sip_channel_t*)obj,sock,(belle_sip_source_func_t)stream_channel_process_data);
	belle_sip_source_set_events((belle_sip_source_t*)obj,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_WRITE|BELLE_SIP_EVENT_ERROR);
	belle_sip_source_set_timeout((belle_sip_source_t*)obj,belle_sip_stack_get_transport_timeout(obj->base.stack));
	belle_sip_main_loop_add_source(obj->base.stack->ml,(belle_sip_source_t*)obj);
	return 0;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_stream_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_stream_channel_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_stream_channel_t,belle_sip_channel_t,FALSE),
			(belle_sip_object_destroy_t)stream_channel_uninit,
			NULL,
			NULL,
			BELLE_SIP_DEFAULT_BUFSIZE_HINT
		},
		"TCP",
		1, /*is_reliable*/
		(int (*)(belle_sip_channel_t *, const struct addrinfo *))stream_channel_connect,
		(int (*)(belle_sip_channel_t *, const void *, size_t ))stream_channel_send,
		(int (*)(belle_sip_channel_t *, void *, size_t ))stream_channel_recv,
		(void (*)(belle_sip_channel_t *))stream_channel_close,
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

int finalize_stream_connection(belle_sip_stream_channel_t *obj, unsigned int revents, struct sockaddr *addr, socklen_t* slen) {
	int err, errnum;
	socklen_t optlen=sizeof(errnum);
	belle_sip_socket_t sock=belle_sip_source_get_socket((belle_sip_source_t*)obj);
	
	if (revents==BELLE_SIP_EVENT_TIMEOUT){
		belle_sip_warning("channel [%p]: user-defined transport timeout.",obj);
		return -1;
	}
	if (!(revents & BELLE_SIP_EVENT_WRITE) && !(revents & BELLE_SIP_EVENT_READ)){
		belle_sip_warning("channel [%p]: getting unexpected event while connecting",obj);
		return -1;
	}
	
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
#if TARGET_OS_IPHONE
			stream_channel_enable_ios_background_mode(obj);
#endif
			if (obj->base.stack->dscp && obj->base.lp){
				/*apply dscp only to channel belonging to a SIP listening point*/
				belle_sip_socket_set_dscp(sock,obj->base.ai_family,obj->base.stack->dscp);
			}
			set_tcp_nodelay(sock);
			return 0;
		}else{
			belle_sip_error("Connection failed  for fd [%i]: cause [%s]",sock,belle_sip_get_socket_error_string_from_code(errnum));
			return -1;
		}
	}
}

static int stream_channel_process_data(belle_sip_stream_channel_t *obj,unsigned int revents){
	struct sockaddr_storage ss;
	socklen_t addrlen=sizeof(ss);
	belle_sip_channel_state_t state=belle_sip_channel_get_state((belle_sip_channel_t*)obj);
	belle_sip_channel_t *base=(belle_sip_channel_t*)obj;

	/*belle_sip_message("TCP channel process_data");*/
	
	if (state == BELLE_SIP_CHANNEL_CONNECTING ) {
		if (finalize_stream_connection(obj,revents,(struct sockaddr*)&ss,&addrlen)) {
			belle_sip_error("Cannot connect to [%s://%s:%i]",belle_sip_channel_get_transport_name(base),base->peer_name,base->peer_port);
			channel_set_state(base,BELLE_SIP_CHANNEL_ERROR);
			return BELLE_SIP_STOP;
		}
		belle_sip_source_set_events((belle_sip_source_t*)obj,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_ERROR);
		belle_sip_source_set_timeout((belle_sip_source_t*)obj,-1);
		belle_sip_channel_set_ready(base,(struct sockaddr*)&ss,addrlen);
		return BELLE_SIP_CONTINUE;
	} else if (state == BELLE_SIP_CHANNEL_READY) {
		return belle_sip_channel_process_data(base,revents);
	} else {
		belle_sip_warning("Unexpected event [%i], in state [%s] for channel [%p]",revents,belle_sip_channel_state_to_string(state),obj);
		return BELLE_SIP_STOP;
	}
	return BELLE_SIP_CONTINUE;
}

void belle_sip_stream_channel_init_client(belle_sip_stream_channel_t *obj, belle_sip_stack_t *stack, const char *bindip, int localport, const char *peer_cname, const char *dest, int port){
	belle_sip_channel_init((belle_sip_channel_t*)obj, stack
					,bindip,localport,peer_cname,dest,port);
}

belle_sip_channel_t * belle_sip_stream_channel_new_client(belle_sip_stack_t *stack,const char *bindip, int localport, const char *peer_cname, const char *dest, int port){
	belle_sip_stream_channel_t *obj=belle_sip_object_new(belle_sip_stream_channel_t);
	belle_sip_stream_channel_init_client(obj,stack,bindip,localport,peer_cname,dest,port);
	return (belle_sip_channel_t*)obj;
}

/*child of server socket*/
belle_sip_channel_t * belle_sip_stream_channel_new_child(belle_sip_stack_t *stack, belle_sip_socket_t sock, struct sockaddr *remote_addr, socklen_t slen){
	struct sockaddr_storage localaddr;
	socklen_t local_len=sizeof(localaddr);
	belle_sip_stream_channel_t *obj;
	int err;
	int optval=1;
	
	err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			(char*)&optval, sizeof (optval));
	if (err == -1){
		belle_sip_warning ("Fail to set SIP/TCP address reusable: %s.", belle_sip_get_socket_error_string());
	}
	
	set_tcp_nodelay(sock);
	
	if (getsockname(sock,(struct sockaddr*)&localaddr,&local_len)==-1){
		belle_sip_error("getsockname() failed: %s",belle_sip_get_socket_error_string());
		return NULL;
	}
	
	obj=belle_sip_object_new(belle_sip_stream_channel_t);
	belle_sip_channel_init_with_addr((belle_sip_channel_t*)obj,stack,NULL,0,remote_addr,slen);
	belle_sip_socket_set_nonblocking(sock);
	belle_sip_channel_set_socket((belle_sip_channel_t*)obj,sock,(belle_sip_source_func_t)stream_channel_process_data);
	belle_sip_source_set_events((belle_sip_source_t*)obj,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_ERROR);
	belle_sip_channel_set_ready((belle_sip_channel_t*)obj,(struct sockaddr*)&localaddr,local_len);
	belle_sip_main_loop_add_source(stack->ml,(belle_sip_source_t*)obj);
	return (belle_sip_channel_t*)obj;
}


