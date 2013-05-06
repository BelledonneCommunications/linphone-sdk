/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2013  Belledonne Communications SARL

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
#include "stream_channel.h"

#ifdef HAVE_POLARSSL

/* Uncomment to get very verbose polarssl logs*/
//#define ENABLE_POLARSSL_LOGS

#include <polarssl/error.h>

/*************tls********/

static int tls_process_data(belle_sip_channel_t *obj,unsigned int revents);

struct belle_sip_tls_channel{
	belle_sip_stream_channel_t base;
	ssl_context sslctx;
	x509_cert root_ca;
	struct sockaddr_storage ss;
	socklen_t socklen;
	int socket_connected;
	char *cur_debug_msg;
};

static void tls_channel_close(belle_sip_tls_channel_t *obj){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	if (sock!=-1 && belle_sip_channel_get_state((belle_sip_channel_t*)obj)!=BELLE_SIP_CHANNEL_ERROR)
		ssl_close_notify(&obj->sslctx);
	stream_channel_close((belle_sip_stream_channel_t*)obj);
}

static void tls_channel_uninit(belle_sip_tls_channel_t *obj){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	if (sock!=(belle_sip_socket_t)-1)
		tls_channel_close(obj);
	ssl_free(&obj->sslctx);
	x509_free(&obj->root_ca);
	if (obj->cur_debug_msg)
		belle_sip_free(obj->cur_debug_msg);
}

static int tls_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen){
	belle_sip_tls_channel_t* channel = (belle_sip_tls_channel_t*)obj;
	int err = ssl_write(&channel->sslctx,buf,buflen);
	if (err<0){
		belle_sip_error("Channel [%p]: error in ssl_write().",obj);
	}
	return err;
}

static int tls_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	belle_sip_tls_channel_t* channel = (belle_sip_tls_channel_t*)obj;
	int err = ssl_read(&channel->sslctx,buf,buflen);
	if (err<0){
		belle_sip_error("Channel [%p]: error in ssl_read().",obj);
	}
	return err;
}

static int tls_channel_connect(belle_sip_channel_t *obj, const struct addrinfo *ai){
	int err= stream_channel_connect((belle_sip_stream_channel_t*)obj,ai);
	if (err==0){
		belle_sip_socket_t sock=belle_sip_source_get_socket((belle_sip_source_t*)obj);
		belle_sip_channel_set_socket(obj,sock,(belle_sip_source_func_t)tls_process_data);
		return 0;
	}
	return -1;
}

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_tls_channel_t,belle_sip_stream_channel_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_tls_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_tls_channel_t)=
{
	{
		{
			{
				BELLE_SIP_VPTR_INIT(belle_sip_tls_channel_t,belle_sip_stream_channel_t,FALSE),
				(belle_sip_object_destroy_t)tls_channel_uninit,
				NULL,
				NULL
			},
			"TLS",
			1, /*is_reliable*/
			tls_channel_connect,
			tls_channel_send,
			tls_channel_recv,
			(void (*)(belle_sip_channel_t*))tls_channel_close
		}
	}
};

static int tls_process_data(belle_sip_channel_t *obj,unsigned int revents){
	belle_sip_tls_channel_t* channel=(belle_sip_tls_channel_t*)obj;
	int err;

	if (obj->state == BELLE_SIP_CHANNEL_CONNECTING ) {
		if (!channel->socket_connected && (revents & BELLE_SIP_EVENT_WRITE)) {
			channel->socklen=sizeof(channel->ss);
			if (finalize_stream_connection((belle_sip_stream_channel_t*)obj,(struct sockaddr*)&channel->ss,&channel->socklen)) {
				goto process_error;
			}
			belle_sip_source_set_events((belle_sip_source_t*)channel,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_ERROR);
			channel->socket_connected=1;
			belle_sip_message("Channel [%p]: Connected at TCP level, now doing TLS handshake",obj);
		}
		err=ssl_handshake(&channel->sslctx);
		if (err==0){
			belle_sip_message("Channel [%p]: SSL handshake finished.",obj);
			belle_sip_channel_set_ready(obj,(struct sockaddr*)&channel->ss,channel->socklen);
		}else if (err==POLARSSL_ERR_NET_WANT_READ || err==POLARSSL_ERR_NET_WANT_WRITE){
			belle_sip_message("Channel [%p]: SSL handshake in progress...",obj);
		}else{
			char tmp[128];
			error_strerror(err,tmp,sizeof(tmp));
			belle_sip_error("Channel [%p]: SSL handlshake failed : %s",obj,tmp);
			goto process_error;
		}
		
	} else if ( obj->state == BELLE_SIP_CHANNEL_READY) {
		return belle_sip_channel_process_data(obj,revents);
	} else {
		belle_sip_warning("Unexpected event [%i], for channel [%p]",revents,channel);
		return BELLE_SIP_STOP;
	}
	return BELLE_SIP_CONTINUE;
	
process_error:
	belle_sip_error("Cannot connect to [%s://%s:%i]",belle_sip_channel_get_transport_name(obj),obj->peer_name,obj->peer_port);
	channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
	return BELLE_SIP_STOP;
}

static int polarssl_read(void * ctx, unsigned char *buf, size_t len ){
	belle_sip_stream_channel_t *super=(belle_sip_stream_channel_t *)ctx;
	
	int ret = stream_channel_recv(super,buf,len);

	if (ret<0){
		ret=-ret;
		if (ret==BELLESIP_EWOULDBLOCK || ret==BELLESIP_EINPROGRESS || ret == EINTR )
			return POLARSSL_ERR_NET_WANT_READ;
		return POLARSSL_ERR_NET_CONN_RESET;
	}
	return ret;
}

static int polarssl_write(void * ctx, const unsigned char *buf, size_t len ){
	belle_sip_stream_channel_t *super=(belle_sip_stream_channel_t *)ctx;
	
	int ret = stream_channel_send(super, buf, len);

	if (ret<0){
		ret=-ret;
		if (ret==BELLESIP_EWOULDBLOCK || ret==BELLESIP_EINPROGRESS || ret == EINTR )
			return POLARSSL_ERR_NET_WANT_WRITE;
		return POLARSSL_ERR_NET_CONN_RESET;
	}
	return ret;
}

static int random_generator(void *ctx, unsigned char *ptr, size_t size){
	belle_sip_random_bytes(ptr, size);
	return 0;
}

static const char *polarssl_certflags_to_string(char *buf, size_t size, int flags){
	int i=0;
	
	memset(buf,0,size);
	size--;
	
	if (i<size && (flags & BADCERT_EXPIRED))
		i+=snprintf(buf+i,size-i,"expired ");
	if (i<size && (flags & BADCERT_REVOKED))
		i+=snprintf(buf+i,size-i,"revoked ");
	if (i<size && (flags & BADCERT_CN_MISMATCH))
		i+=snprintf(buf+i,size-i,"CN-mismatch ");
	if (i<size && (flags & BADCERT_NOT_TRUSTED))
		i+=snprintf(buf+i,size-i,"not-trusted ");
	if (i<size && (flags & BADCERT_MISSING))
		i+=snprintf(buf+i,size-i,"missing ");
	if (i<size && (flags & BADCRL_NOT_TRUSTED))
		i+=snprintf(buf+i,size-i,"crl-not-trusted ");
	if (i<size && (flags & BADCRL_EXPIRED))
		i+=snprintf(buf+i,size-i,"crl-not-expired ");
	return buf;
}

static int belle_sip_ssl_verify(void *data , x509_cert *cert , int depth, int *flags){
	belle_sip_tls_listening_point_t *lp=(belle_sip_tls_listening_point_t*)data;
	char tmp[256];
	char flags_str[128];
	
	x509parse_cert_info(tmp,sizeof(tmp),"",cert);
	belle_sip_message("Found certificate depth=[%i], flags=[%s]:\n%s",
		depth,polarssl_certflags_to_string(flags_str,sizeof(flags_str),*flags),tmp);
	if (lp->verify_exceptions==BELLE_SIP_TLS_LISTENING_POINT_BADCERT_ANY_REASON){
		*flags=0;
	}else if (lp->verify_exceptions & BELLE_SIP_TLS_LISTENING_POINT_BADCERT_CN_MISMATCH){
		*flags&=~BADCERT_CN_MISMATCH;
	}
	return 0;
}

static int belle_sip_tls_channel_load_root_ca(belle_sip_tls_channel_t *obj, const char *path){
	struct stat statbuf; 
	if (stat(path,&statbuf)==0){
		if (statbuf.st_mode & S_IFDIR){
			if (x509parse_crtpath(&obj->root_ca,path)<0){
				belle_sip_error("Failed to load root ca from directory %s",path);
				return -1;
			}
		}else{
			if (x509parse_crtfile(&obj->root_ca,path)<0){
				belle_sip_error("Failed to load root ca from file %s",path);
				return -1;
			}
		}
		return 0;
	}
	belle_sip_error("Could not load root ca from %s: %s",path,strerror(errno));
	return -1;
}

#ifdef ENABLE_POLARSSL_LOGS
/*
 * polarssl does a lot of logs, some with newline, some without.
 * We need to concatenate logs without new line until a new line is found.
 */
static void ssl_debug_to_belle_sip(void *context, int level, const char *str){
	belle_sip_tls_channel_t *chan=(belle_sip_tls_channel_t*)context;
	int len=strlen(str);
	
	if (len>0 && (str[len-1]=='\n' || str[len-1]=='\r')){
		/*eliminate the newline*/
		char *tmp=belle_sip_strdup(str);
		tmp[len-1]=0;
		if (chan->cur_debug_msg){
			belle_sip_message("ssl: %s%s",chan->cur_debug_msg,tmp);
			belle_sip_free(chan->cur_debug_msg);
			chan->cur_debug_msg=NULL;
		}else belle_sip_message("ssl: %s",tmp);
		belle_sip_free(tmp);
	}else{
		if (chan->cur_debug_msg){
			char *tmp=belle_sip_strdup_printf("%s%s",chan->cur_debug_msg,str);
			belle_sip_free(chan->cur_debug_msg);
			chan->cur_debug_msg=tmp;
		}else chan->cur_debug_msg=belle_sip_strdup(str);
	}
}

#endif

belle_sip_channel_t * belle_sip_channel_new_tls(belle_sip_tls_listening_point_t *lp,const char *bindip, int localport, const char *peer_cname, const char *dest, int port){
	belle_sip_tls_channel_t *obj=belle_sip_object_new(belle_sip_tls_channel_t);
	belle_sip_stream_channel_t* super=(belle_sip_stream_channel_t*)obj;

	belle_sip_stream_channel_init_client(super
					,((belle_sip_listening_point_t*)lp)->stack
					,bindip,localport,peer_cname,dest,port);
	ssl_init(&obj->sslctx);
#ifdef ENABLE_POLARSSL_LOGS
	ssl_set_dbg(&obj->sslctx,ssl_debug_to_belle_sip,obj);
#endif
	ssl_set_endpoint(&obj->sslctx,SSL_IS_CLIENT);
	ssl_set_authmode(&obj->sslctx,SSL_VERIFY_REQUIRED);
	ssl_set_bio(&obj->sslctx,polarssl_read,obj,polarssl_write,obj);
	if (lp->root_ca && belle_sip_tls_channel_load_root_ca(obj,lp->root_ca)==0){
		ssl_set_ca_chain(&obj->sslctx,&obj->root_ca,NULL,super->base.peer_cname);
	}
	ssl_set_rng(&obj->sslctx,random_generator,NULL);
	ssl_set_verify(&obj->sslctx,belle_sip_ssl_verify,lp);
	return (belle_sip_channel_t*)obj;
}

#endif


