/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2013  Belledonne Communications SARL

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
#include "stream_channel.h"

#ifdef HAVE_POLARSSL
/* Uncomment to get very verbose polarssl logs*/
//#define ENABLE_POLARSSL_LOGS
#include <polarssl/ssl.h>
#include <polarssl/version.h>
#include <polarssl/error.h>
#if POLARSSL_VERSION_NUMBER >= 0x01030000
#include <polarssl/x509.h>
#endif
#endif


struct belle_sip_certificates_chain {
	belle_sip_object_t objet;
#ifdef HAVE_POLARSSL
#if POLARSSL_VERSION_NUMBER < 0x01030000
	x509_cert cert;
#else
	x509_crt cert;
#endif
#endif
};

struct belle_sip_signing_key {
	belle_sip_object_t objet;
#ifdef HAVE_POLARSSL
#if POLARSSL_VERSION_NUMBER < 0x01030000
	rsa_context key;
#else
	pk_context key;
#endif
#endif
};

#ifdef HAVE_POLARSSL

/*************tls********/

static int tls_process_data(belle_sip_channel_t *obj,unsigned int revents);

struct belle_sip_tls_channel{
	belle_sip_stream_channel_t base;
	ssl_context sslctx;
#if POLARSSL_VERSION_NUMBER < 0x01030000
	x509_cert root_ca;
#else
	x509_crt root_ca;
#endif
	struct sockaddr_storage ss;
	socklen_t socklen;
	int socket_connected;
	char *cur_debug_msg;
	belle_sip_certificates_chain_t* client_cert_chain;
	belle_sip_signing_key_t* client_cert_key;
	belle_tls_verify_policy_t *verify_ctx;
};

static void tls_channel_close(belle_sip_tls_channel_t *obj){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	if (sock!=-1 && belle_sip_channel_get_state((belle_sip_channel_t*)obj)!=BELLE_SIP_CHANNEL_ERROR)
		ssl_close_notify(&obj->sslctx);
	stream_channel_close((belle_sip_stream_channel_t*)obj);
	ssl_session_reset(&obj->sslctx);
	obj->socket_connected=0;
}

static void tls_channel_uninit(belle_sip_tls_channel_t *obj){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	if (sock!=(belle_sip_socket_t)-1)
		tls_channel_close(obj);
	ssl_free(&obj->sslctx);
#if POLARSSL_VERSION_NUMBER < 0x01030000
	x509_free(&obj->root_ca);
#else
	x509_crt_free(&obj->root_ca);
#endif
	if (obj->cur_debug_msg)
		belle_sip_free(obj->cur_debug_msg);
	belle_sip_object_unref(obj->verify_ctx);
	if (obj->client_cert_chain) belle_sip_object_unref(obj->client_cert_chain);
	if (obj->client_cert_key) belle_sip_object_unref(obj->client_cert_key);
}

static int tls_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen){
	belle_sip_tls_channel_t* channel = (belle_sip_tls_channel_t*)obj;
	int err = ssl_write(&channel->sslctx,buf,buflen);
	if (err<0){
		char tmp[256]={0};
		if (err==POLARSSL_ERR_NET_WANT_WRITE) return -BELLESIP_EWOULDBLOCK;
		error_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("Channel [%p]: ssl_write() error [%i]: %s",obj,err,tmp);
	}
	return err;
}

static int tls_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	belle_sip_tls_channel_t* channel = (belle_sip_tls_channel_t*)obj;
	int err = ssl_read(&channel->sslctx,buf,buflen);
	if (err==POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
	if (err<0){
		char tmp[256]={0};
		if (err==POLARSSL_ERR_NET_WANT_READ) return -BELLESIP_EWOULDBLOCK;
		error_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("Channel [%p]: ssl_read() error [%i]: %s",obj, err, tmp);
	}
	return err;
}

static int tls_channel_connect(belle_sip_channel_t *obj, const struct addrinfo *ai){
	int err= stream_channel_connect((belle_sip_stream_channel_t*)obj,ai);
	if (err==0){
		belle_sip_source_set_notify((belle_sip_source_t *)obj, (belle_sip_source_func_t)tls_process_data);
		return 0;
	}
	return -1;
}

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_tls_channel_t,belle_sip_stream_channel_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_tls_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_tls_channel_t)
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
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

static int tls_channel_handshake(belle_sip_tls_channel_t *channel) {
	int ret;
	while( channel->sslctx.state != SSL_HANDSHAKE_OVER ) {
		if ((ret = ssl_handshake_step( &channel->sslctx ))) {
			break;
		}
		if (channel->sslctx.state == SSL_CLIENT_CERTIFICATE && channel->sslctx.client_auth >0) {
			BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(	channel->base.base.listeners
					,belle_sip_channel_listener_t
					,on_auth_requested
					,&channel->base.base
					,NULL/*not set yet*/);

			if (channel->client_cert_chain && channel->client_cert_key) {
#if POLARSSL_VERSION_NUMBER >= 0x01030000
				int err;
#endif
				char tmp[512]={0};
#if POLARSSL_VERSION_NUMBER < 0x01030000
				x509parse_cert_info(tmp,sizeof(tmp)-1,"",&channel->client_cert_chain->cert);
#else
				x509_crt_info(tmp,sizeof(tmp)-1,"",&channel->client_cert_chain->cert);
#endif
				belle_sip_message("Channel [%p]  found client  certificate:\n%s",channel,tmp);
#if POLARSSL_VERSION_NUMBER < 0x01030000
				ssl_set_own_cert(&channel->sslctx,&channel->client_cert_chain->cert,&channel->client_cert_key->key);
#else
                                /* allows public keys other than RSA */
				if ((err=ssl_set_own_cert(&channel->sslctx,&channel->client_cert_chain->cert,&channel->client_cert_key->key))) {
					error_strerror(err,tmp,sizeof(tmp)-1);
					belle_sip_error("Channel [%p] cannot ssl_set_own_cert [%s]",channel,tmp);
				}

				/*update own cert see ssl_handshake frompolarssl*/
				channel->sslctx.handshake->key_cert = channel->sslctx.key_cert;
#endif
			}
		}

	}
	return ret;
}

static int tls_process_handshake(belle_sip_channel_t *obj){
	belle_sip_tls_channel_t* channel=(belle_sip_tls_channel_t*)obj;
	int err=tls_channel_handshake(channel);
	if (err==0){
		belle_sip_message("Channel [%p]: SSL handshake finished.",obj);
		belle_sip_source_set_timeout((belle_sip_source_t*)obj,-1);
		belle_sip_channel_set_ready(obj,(struct sockaddr*)&channel->ss,channel->socklen);
	}else if (err==POLARSSL_ERR_NET_WANT_READ || err==POLARSSL_ERR_NET_WANT_WRITE){
		belle_sip_message("Channel [%p]: SSL handshake in progress...",obj);
	}else{
		char tmp[128];
		error_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("Channel [%p]: SSL handshake failed : %s",obj,tmp);
		return -1;
	}
	return 0;
}

static int tls_process_data(belle_sip_channel_t *obj,unsigned int revents){
	belle_sip_tls_channel_t* channel=(belle_sip_tls_channel_t*)obj;

	if (obj->state == BELLE_SIP_CHANNEL_CONNECTING ) {
		if (!channel->socket_connected) {
			channel->socklen=sizeof(channel->ss);
			if (finalize_stream_connection((belle_sip_stream_channel_t*)obj,revents,(struct sockaddr*)&channel->ss,&channel->socklen)) {
				goto process_error;
			}
			belle_sip_message("Channel [%p]: Connected at TCP level, now doing TLS handshake",obj);
			channel->socket_connected=1;
			belle_sip_source_set_events((belle_sip_source_t*)channel,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_ERROR);
			belle_sip_source_set_timeout((belle_sip_source_t*)obj,belle_sip_stack_get_transport_timeout(obj->stack));
			if (tls_process_handshake(obj)==-1) goto process_error;
		}else{
			if (revents & BELLE_SIP_EVENT_READ){
				if (tls_process_handshake(obj)==-1) goto process_error;
			}else if (revents==BELLE_SIP_EVENT_TIMEOUT){
				belle_sip_error("channel [%p]: SSL handshake took too much time.",obj);
				goto process_error;
			}else{
				belle_sip_warning("channeEHHCXCCCl [%p]: unexpected event [%i] during TLS handshake.",obj,revents);
			}
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

#if POLARSSL_VERSION_NUMBER < 0x01030000
static int belle_sip_ssl_verify(void *data , x509_cert *cert , int depth, int *flags){
#else
static int belle_sip_ssl_verify(void *data , x509_crt *cert , int depth, int *flags){
#endif
	belle_tls_verify_policy_t *verify_ctx=(belle_tls_verify_policy_t*)data;
	char tmp[512];
	char flags_str[128];
	
#if POLARSSL_VERSION_NUMBER < 0x01030000
	x509parse_cert_info(tmp,sizeof(tmp),"",cert);
#else
	x509_crt_info(tmp,sizeof(tmp),"",cert);
#endif
	belle_sip_message("Found certificate depth=[%i], flags=[%s]:\n%s",
		depth,polarssl_certflags_to_string(flags_str,sizeof(flags_str),*flags),tmp);
	if (verify_ctx->exception_flags==BELLE_TLS_VERIFY_ANY_REASON){
		*flags=0;
	}else if (verify_ctx->exception_flags & BELLE_TLS_VERIFY_CN_MISMATCH){
		*flags&=~BADCERT_CN_MISMATCH;
	}
	return 0;
}

static int belle_sip_tls_channel_load_root_ca(belle_sip_tls_channel_t *obj, const char *path){
	struct stat statbuf; 
	if (stat(path,&statbuf)==0){
		if (statbuf.st_mode & S_IFDIR){
#if POLARSSL_VERSION_NUMBER < 0x01030000
			if (x509parse_crtpath(&obj->root_ca,path)<0){
#else
			if (x509_crt_parse_path(&obj->root_ca,path)<0){
#endif
				belle_sip_error("Failed to load root ca from directory %s",path);
				return -1;
			}
		}else{
#if POLARSSL_VERSION_NUMBER < 0x01030000
			if (x509parse_crtfile(&obj->root_ca,path)<0){
#else
			if (x509_crt_parse_file(&obj->root_ca,path)<0){
#endif
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

belle_sip_channel_t * belle_sip_channel_new_tls(belle_sip_stack_t *stack, belle_tls_verify_policy_t *verify_ctx,const char *bindip, int localport, const char *peer_cname, const char *dest, int port){
	belle_sip_tls_channel_t *obj=belle_sip_object_new(belle_sip_tls_channel_t);
	belle_sip_stream_channel_t* super=(belle_sip_stream_channel_t*)obj;

	belle_sip_stream_channel_init_client(super
					,stack
					,bindip,localport,peer_cname,dest,port);
	ssl_init(&obj->sslctx);
#ifdef ENABLE_POLARSSL_LOGS
	ssl_set_dbg(&obj->sslctx,ssl_debug_to_belle_sip,obj);
#endif
	ssl_set_endpoint(&obj->sslctx,SSL_IS_CLIENT);
	ssl_set_authmode(&obj->sslctx,SSL_VERIFY_REQUIRED);
	ssl_set_bio(&obj->sslctx,polarssl_read,obj,polarssl_write,obj);
	if (verify_ctx->root_ca && belle_sip_tls_channel_load_root_ca(obj,verify_ctx->root_ca)==0){
		ssl_set_ca_chain(&obj->sslctx,&obj->root_ca,NULL,super->base.peer_cname ? super->base.peer_cname : super->base.peer_name );
	}
	ssl_set_rng(&obj->sslctx,random_generator,NULL);
	ssl_set_verify(&obj->sslctx,belle_sip_ssl_verify,verify_ctx);
	obj->verify_ctx=(belle_tls_verify_policy_t*)belle_sip_object_ref(verify_ctx);
	return (belle_sip_channel_t*)obj;
}

void belle_sip_tls_channel_set_client_certificates_chain(belle_sip_tls_channel_t *channel, belle_sip_certificates_chain_t* cert_chain) {
	SET_OBJECT_PROPERTY(channel,client_cert_chain,cert_chain);

}
void belle_sip_tls_channel_set_client_certificate_key(belle_sip_tls_channel_t *channel, belle_sip_signing_key_t* key){
	SET_OBJECT_PROPERTY(channel,client_cert_key,key);
}


#else /*HAVE_POLLAR_SSL*/
void belle_sip_tls_channel_set_client_certificates_chain(belle_sip_tls_channel_t *obj, belle_sip_certificates_chain_t* cert_chain) {
	belle_sip_error("belle_sip_channel_set_client_certificate_chain requires TLS");
}
void belle_sip_tls_channel_set_client_certificate_key(belle_sip_tls_channel_t *obj, belle_sip_signing_key_t* key) {
	belle_sip_error("belle_sip_channel_set_client_certificate_key requires TLS");
}
#endif

/**************************** belle_sip_certificates_chain_t **/




static int belle_sip_certificate_fill(belle_sip_certificates_chain_t* certificate,const char* buff, size_t size,belle_sip_certificate_raw_format_t format) {
#ifdef HAVE_POLARSSL

	int err;
#if POLARSSL_VERSION_NUMBER < 0x01030000
	if ((err=x509parse_crt(&certificate->cert,(const unsigned char *)buff,size)) <0) {
#else
	if ((err=x509_crt_parse(&certificate->cert,(const unsigned char *)buff,size)) <0) {
#endif
		char tmp[128];
		error_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("cannot parse x509 cert because [%s]",tmp);
		return -1;
	}
	return 0;
#else /*HAVE_POLARSSL*/
	return -1;
#endif
}

static int belle_sip_certificate_fill_from_file(belle_sip_certificates_chain_t* certificate,const char* path,belle_sip_certificate_raw_format_t format) {
#ifdef HAVE_POLARSSL

	int err;
#if POLARSSL_VERSION_NUMBER < 0x01030000
	if ((err=x509parse_crtfile(&certificate->cert, path)) <0) {
#else
	if ((err=x509_crt_parse_file(&certificate->cert, path)) <0) {
#endif
		char tmp[128];
		error_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("cannot parse x509 cert because [%s]",tmp);
		return -1;
	}
	return 0;
#else /*HAVE_POLARSSL*/
	return -1;
#endif
}

/*belle_sip_certificate */
belle_sip_certificates_chain_t* belle_sip_certificates_chain_parse(const char* buff, size_t size,belle_sip_certificate_raw_format_t format) {
	belle_sip_certificates_chain_t* certificate = belle_sip_object_new(belle_sip_certificates_chain_t);

	if (belle_sip_certificate_fill(certificate,buff, size,format)) {
		belle_sip_object_unref(certificate);
		certificate=NULL;
	}

	return certificate;
}

belle_sip_certificates_chain_t* belle_sip_certificates_chain_parse_file(const char* path, belle_sip_certificate_raw_format_t format) {
	belle_sip_certificates_chain_t* certificate = belle_sip_object_new(belle_sip_certificates_chain_t);

	if (belle_sip_certificate_fill_from_file(certificate, path, format)) {
		belle_sip_object_unref(certificate);
		certificate=NULL;
	}

	return certificate;
}


static void belle_sip_certificates_chain_destroy(belle_sip_certificates_chain_t *certificate){
#ifdef HAVE_POLARSSL
#if POLARSSL_VERSION_NUMBER < 0x01030000
	x509_free(&certificate->cert);
#else
	x509_crt_free(&certificate->cert);
#endif
#endif
}

static void belle_sip_certificates_chain_clone(belle_sip_certificates_chain_t *certificate, const belle_sip_certificates_chain_t *orig){
	belle_sip_error("belle_sip_certificate_clone not supported");
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_certificates_chain_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_certificates_chain_t,belle_sip_object_t,belle_sip_certificates_chain_destroy,belle_sip_certificates_chain_clone,NULL,TRUE);




belle_sip_signing_key_t* belle_sip_signing_key_parse(const char* buff, size_t size,const char* passwd) {
#ifdef HAVE_POLARSSL
	belle_sip_signing_key_t* signing_key = belle_sip_object_new(belle_sip_signing_key_t);
	int err;
#if POLARSSL_VERSION_NUMBER < 0x01030000
	if ((err=x509parse_key(&signing_key->key,(const unsigned char *)buff,size,(const unsigned char*)passwd,passwd?strlen(passwd):0)) <0) {
#else
	pk_init(&signing_key->key);
	/* for API v1.3 or greater also parses public keys other than RSA */
	err=pk_parse_key(&signing_key->key,(const unsigned char *)buff,size,(const unsigned char*)passwd,passwd?strlen(passwd):0);
	/* make sure cipher is RSA to be consistent with API v1.2 */
	if(err==0 && !pk_can_do(&signing_key->key,POLARSSL_PK_RSA))
	err=POLARSSL_ERR_PK_TYPE_MISMATCH;
	if (err<0) {
#endif
		char tmp[128];
		error_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("cannot parse public key because [%s]",tmp);
#if POLARSSL_VERSION_NUMBER >= 0x01030000
                pk_free(&signing_key->key);
#endif
		belle_sip_object_unref(signing_key);
		return NULL;
	}
	return signing_key;
#else /*HAVE_POLARSSL*/
	return NULL;
#endif
}

belle_sip_signing_key_t* belle_sip_signing_key_parse_file(const char* path,const char* passwd) {
#ifdef HAVE_POLARSSL
	belle_sip_signing_key_t* signing_key = belle_sip_object_new(belle_sip_signing_key_t);
	int err;
#if POLARSSL_VERSION_NUMBER < 0x01030000
	if ((err=x509parse_keyfile(&signing_key->key,path, passwd)) <0) {
#else
	pk_init(&signing_key->key);
	/* for API v1.3 or greater also parses public keys other than RSA */
	err=pk_parse_keyfile(&signing_key->key,path, passwd);
	/* make sure cipher is RSA to be consistent with API v1.2 */
	if(err==0 && !pk_can_do(&signing_key->key,POLARSSL_PK_RSA))
	err=POLARSSL_ERR_PK_TYPE_MISMATCH;
	if (err<0) {
#endif
		char tmp[128];
		error_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("cannot parse public key because [%s]",tmp);
#if POLARSSL_VERSION_NUMBER >= 0x01030000
        pk_free(&signing_key->key);
#endif
		belle_sip_object_unref(signing_key);
		return NULL;
	}

	return signing_key;
#else /*HAVE_POLARSSL*/
	return NULL;
#endif
}


static void belle_sip_signing_key_destroy(belle_sip_signing_key_t *signing_key){
#ifdef HAVE_POLARSSL
#if POLARSSL_VERSION_NUMBER < 0x01030000
	rsa_free(&signing_key->key);
#else
	pk_free(&signing_key->key);
#endif
#endif
}

static void belle_sip_signing_key_clone(belle_sip_signing_key_t *signing_key, const belle_sip_signing_key_t *orig){
	belle_sip_error("belle_sip_signing_key_clone not supported");
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_signing_key_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_signing_key_t,belle_sip_object_t,belle_sip_signing_key_destroy,belle_sip_signing_key_clone,NULL,TRUE);



