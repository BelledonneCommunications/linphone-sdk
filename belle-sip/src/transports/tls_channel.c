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
#include "listeningpoint_internal.h"
#include "belle_sip_internal.h"
#include "belle-sip/mainloop.h"
#include "stream_channel.h"
#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#else if HAVE_OPENSSL
#include "openssl/ssl.h"
#endif
/*************tls********/

struct belle_sip_tls_channel{
	belle_sip_channel_t base;
	belle_sip_tls_listening_point_t* lp;
	int socket_connected;
#ifdef HAVE_OPENSSL
	SSL *ssl;
#endif
#ifdef HAVE_GNUTLS
	gnutls_session_t session;
	gnutls_certificate_credentials_t xcred;
#endif
	struct sockaddr_storage ss;
};


static void tls_channel_uninit(belle_sip_tls_channel_t *obj){
	belle_sip_fd_t sock = belle_sip_source_get_fd((belle_sip_source_t*)obj);
#ifdef HAVE_GNUTLS
	gnutls_bye (obj->session, GNUTLS_SHUT_RDWR);
	gnutls_deinit (obj->session);
	gnutls_certificate_free_credentials (obj->xcred);

#endif
	if (sock!=-1)
		close_socket(sock);

	belle_sip_main_loop_remove_source(obj->base.stack->ml,(belle_sip_source_t*)obj);
	 belle_sip_object_unref(obj->lp);
}

static int tls_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen){
	belle_sip_tls_channel_t* channel = (belle_sip_tls_channel_t*)obj;
	int err;
	/*fix me, can block, see gnutls doc*/
	err=gnutls_record_send (channel->session, buf, buflen);
	if (err<0){
		belle_sip_error("Could not send tls packet on channel [%p]: %s",obj,gnutls_strerror(err));
		return err;
	}
	return err;
}

static ssize_t tls_channel_pull_func(gnutls_transport_ptr_t obj, void* buff, size_t bufflen) {
	return stream_channel_recv((belle_sip_channel_t *)obj, buff,bufflen);
}
static int tls_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	belle_sip_tls_channel_t* channel = (belle_sip_tls_channel_t*)obj;
	int err;
	err=gnutls_record_recv(channel->session,buf,buflen);
	if (err<0 ){
		belle_sip_error("Could not receive tls packet: %s",gnutls_strerror(err));
		return err;
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
	belle_sip_tls_channel_t* channel=(belle_sip_tls_channel_t*)obj;
	socklen_t addrlen=sizeof(channel->ss);
	int result;
#ifdef HAVE_OPENSSL
	char ssl_error_string[128];
#endif /*HAVE_OPENSSL*/
	belle_sip_fd_t fd=belle_sip_source_get_fd((belle_sip_source_t*)channel);
	if (obj->state == BELLE_SIP_CHANNEL_CONNECTING) {
		if (!channel->socket_connected) {
			if (finalize_stream_connection(fd,(struct sockaddr*)&channel->ss,&addrlen)) {
				goto process_error;
			}
			belle_sip_source_set_events((belle_sip_source_t*)channel,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_ERROR);
			channel->socket_connected=1;
		}
		/*connected, now establishing TLS connection*/
#if HAVE_GNUTLS
		gnutls_transport_set_ptr2(channel->session, (gnutls_transport_ptr_t)channel,(gnutls_transport_ptr_t) (0xFFFFFFFFULL&fd));
		result = gnutls_handshake(channel->session);
		if ((result < 0 && gnutls_error_is_fatal (result) == 0)) {
			belle_sip_message("TLS connection in progress for channel [%p]",channel);
			return BELLE_SIP_CONTINUE;
		} else if (result<0) {
			belle_sip_error("TLS Handshake failed caused by [%s]",gnutls_strerror(result));
			goto process_error;
		} else {
			belle_sip_channel_set_ready(obj,(struct sockaddr*)&channel->ss,addrlen);
			return BELLE_SIP_CONTINUE;
		}
#else if HAVE_OPENSSL
		if (!channel->ssl) {
			channel->ssl=SSL_new(channel->lp->ssl_context);
			if (!channel->ssl) {
				belle_sip_error("Cannot create TLS channel context");
				goto process_error;
			}
		}
		if (!SSL_set_fd(channel->ssl,fd)) {
			;
			belle_sip_error("TLS connection failed to set fd caused by [%s]",ERR_error_string(ERR_get_error(),ssl_error_string));
			goto process_error;
		}
		result=SSL_connect(channel->ssl);
		result = SSL_get_error(channel->ssl, result);
		if (result == SSL_ERROR_NONE) {
			belle_sip_channel_set_ready(obj,(struct sockaddr*)&channel->ss,addrlen);
			return BELLE_SIP_CONTINUE;
		} else if (result == SSL_ERROR_WANT_READ || result == SSL_ERROR_WANT_WRITE) {
			belle_sip_message("TLS connection in progress for channel [%p]",channel);
			return BELLE_SIP_CONTINUE;
		} else {
			belle_sip_error("TLS connection failed caused by [%s]",ERR_error_string(result,ssl_error_string));
			goto process_error;
		}
#endif /*HAVE_OPENSSL*/

	} else if ( obj->state == BELLE_SIP_CHANNEL_READY) {
		belle_sip_channel_process_data(obj,revents);
	} else {
		belle_sip_warning("Unexpected event [%i], for channel [%p]",revents,channel);
	}
	return BELLE_SIP_CONTINUE;
	process_error:
	belle_sip_error("Cannot connect to [%s://%s:%i]",belle_sip_channel_get_transport_name(obj),obj->peer_name,obj->peer_port);
	channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
	channel_process_queue(obj);
	return BELLE_SIP_STOP;
}
belle_sip_channel_t * belle_sip_channel_new_tls(belle_sip_tls_listening_point_t *lp,const char *bindip, int localport, const char *dest, int port){
	belle_sip_tls_channel_t *obj=belle_sip_object_new(belle_sip_tls_channel_t);
	belle_sip_channel_t* channel=(belle_sip_channel_t*)obj;
	int result;
	#ifdef HAVE_GNUTLS
	const char* err_pos;
	result = gnutls_init (&obj->session, GNUTLS_CLIENT);
	if (result<0) {
		belle_sip_error("Cannot initialize gnu tls session for channel [%p] caused by [%s]",obj,gnutls_strerror(result));
		goto error;
	}
	result = gnutls_certificate_allocate_credentials (&obj->xcred);
	if (result<0) {
		belle_sip_error("Cannot allocate_client_credentials for channel [%p] caused by [%s]",obj,gnutls_strerror(result));
		goto error;
	}
	/* Use default priorities */
	result = gnutls_priority_set_direct (obj->session, "NORMAL"/*"PERFORMANCE:+ANON-DH:!ARCFOUR-128"*/,&err_pos);
	if (result<0) {
		belle_sip_error("Cannot set direct priority for channel [%p] caused by [%s] at position [%s]",obj,gnutls_strerror(result),err_pos);
		goto error;
	}
	/* put the  credentials to the current session
	 */
	 result = gnutls_credentials_set (obj->session, GNUTLS_CRD_CERTIFICATE, obj->xcred);
	if (result<0) {
		belle_sip_error("Cannot set credential for channel [%p] caused by [%s]",obj,gnutls_strerror(result));
		goto error;
	}
	gnutls_transport_set_pull_function(obj->session,tls_channel_pull_func);
#endif
	belle_sip_channel_init(channel
							,((belle_sip_listening_point_t*)lp)->stack
							,socket(AF_INET, SOCK_STREAM, 0)
							,(belle_sip_source_func_t)process_data
							,bindip,localport,dest,port);
	belle_sip_object_ref(obj->lp=lp);
	return (belle_sip_channel_t*)obj;
error:
	belle_sip_error("Cannot create tls channel to [%s://%s:%i]",belle_sip_channel_get_transport_name(channel),channel->peer_name,channel->peer_port);
	belle_sip_object_unref(obj);
	return NULL;
}















