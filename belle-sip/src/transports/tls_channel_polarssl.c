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
#include <polarssl/pem.h>
#if POLARSSL_VERSION_NUMBER >= 0x01030000
#include <polarssl/x509.h>
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/sha1.h>
#include <polarssl/sha256.h>
#include <polarssl/sha512.h>
#include "polarssl/base64.h"
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


#if POLARSSL_VERSION_NUMBER < 0x01030000
/*stubs*/
char *belle_sip_certificates_chain_get_pem(belle_sip_certificates_chain_t *cert) {
	return NULL;
}
char *belle_sip_signing_key_get_pem(belle_sip_signing_key_t *key) {
	return NULL;
}
#endif

#ifdef HAVE_POLARSSL

/**
 * Retrieve key or certificate in a string(PEM format)
 */
#if POLARSSL_VERSION_NUMBER >= 0x01030000

char *belle_sip_certificates_chain_get_pem(belle_sip_certificates_chain_t *cert) {
	char *pem_certificate = NULL;
	size_t olen=0;
	if (cert == NULL) return NULL;

	pem_certificate = (char*)belle_sip_malloc(4096);
	pem_write_buffer("-----BEGIN CERTIFICATE-----\n", "-----END CERTIFICATE-----\n", cert->cert.raw.p, cert->cert.raw.len, (unsigned char*)pem_certificate, 4096, &olen );
	return pem_certificate;
}

char *belle_sip_signing_key_get_pem(belle_sip_signing_key_t *key) {
	char *pem_key;
	if (key == NULL) return NULL;
	pem_key = (char *)belle_sip_malloc(4096);
	pk_write_key_pem( &(key->key), (unsigned char *)pem_key, 4096);
	return pem_key;
}
#endif /* POLARSSL_VERSION_NUMBER >= 0x01030000 */

/*************tls********/
// SSL verification callback prototype
// der - raw certificate data, in DER format
// length - length of certificate DER data
// depth - position of certificate in cert chain, ending at 0 = root or top
// flags - verification state for CURRENT certificate only
typedef int (*verify_cb_error_cb_t)(unsigned char* der, int length, int depth, int* flags);
static verify_cb_error_cb_t tls_verify_cb_error_cb = NULL;

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
	int http_proxy_connected;
	belle_sip_resolver_context_t *http_proxy_resolver_ctx;
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
	if (obj->http_proxy_resolver_ctx) belle_sip_object_unref(obj->http_proxy_resolver_ctx);
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

static int tls_channel_connect_to(belle_sip_channel_t *obj, const struct addrinfo *ai){
	int err;
	err= stream_channel_connect((belle_sip_stream_channel_t*)obj,ai);
	if (err==0){
		belle_sip_source_set_notify((belle_sip_source_t *)obj, (belle_sip_source_func_t)tls_process_data);
		return 0;
	}
	return -1;
}

static void http_proxy_res_done(void *data, const char *name, struct addrinfo *ai_list){
	belle_sip_tls_channel_t *obj=(belle_sip_tls_channel_t*)data;
	if (obj->http_proxy_resolver_ctx){
		belle_sip_object_unref(obj->http_proxy_resolver_ctx);
		obj->http_proxy_resolver_ctx=NULL;
	}
	if (ai_list){
		tls_channel_connect_to((belle_sip_channel_t *)obj,ai_list);
		belle_sip_freeaddrinfo(ai_list);
	}else{
		belle_sip_error("%s: DNS resolution failed for %s", __FUNCTION__, name);
		channel_set_state((belle_sip_channel_t*)obj,BELLE_SIP_CHANNEL_ERROR);
	}
}

static int tls_channel_connect(belle_sip_channel_t *obj, const struct addrinfo *ai){
	belle_sip_tls_listening_point_t * lp = (belle_sip_tls_listening_point_t * )obj->lp;
	belle_sip_tls_channel_t *channel=(belle_sip_tls_channel_t*)obj;
	if (lp && lp->http_proxy_host) {
		belle_sip_message("Resolving http proxy addr [%s] for channel [%p]",lp->http_proxy_host,obj);
		/*assume ai family is the same*/
		channel->http_proxy_resolver_ctx = belle_sip_stack_resolve_a(obj->stack, lp->http_proxy_host, lp->http_proxy_port, obj->ai_family, http_proxy_res_done, obj);
		if (channel->http_proxy_resolver_ctx) belle_sip_object_ref(channel->http_proxy_resolver_ctx);
		return 0;
	} else {
		return tls_channel_connect_to(obj, ai);
	}
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

static int tls_process_http_connect(belle_sip_tls_channel_t *obj) {
	char* request;
	belle_sip_channel_t *channel = (belle_sip_channel_t *)obj;
	belle_sip_tls_listening_point_t* lp = (belle_sip_tls_listening_point_t*)(channel->lp);
	int err;
	char ip[64];
	int port;
	belle_sip_addrinfo_to_ip(channel->current_peer,ip,sizeof(ip),&port);
	
	request = belle_sip_strdup_printf("CONNECT %s:%i HTTP/1.1\r\nProxy-Connection: keep-alive\r\nConnection: keep-alive\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\n"
									  ,ip
									  ,port
									  ,ip);
	
	if (lp->http_proxy_username && lp->http_proxy_passwd) {
		char *username_passwd = belle_sip_strdup_printf("%s:%s",lp->http_proxy_username,lp->http_proxy_passwd);
		size_t username_passwd_length = strlen(username_passwd);
		size_t encoded_username_paswd_length = username_passwd_length*2;
		unsigned char *encoded_username_paswd = belle_sip_malloc(2*username_passwd_length);
		base64_encode(encoded_username_paswd,&encoded_username_paswd_length,(const unsigned char *)username_passwd,username_passwd_length);
		request = belle_sip_strcat_printf(request, "Proxy-Authorization: Basic %s\r\n",encoded_username_paswd);
		belle_sip_free(username_passwd);
		belle_sip_free(encoded_username_paswd);
	}
	
	request = belle_sip_strcat_printf(request,"\r\n");
	err = send(belle_sip_source_get_socket((belle_sip_source_t*)obj),request,strlen(request),0);
	belle_sip_free(request);
	if (err <= 0) {
		belle_sip_error("tls_process_http_connect: fail to send connect request to http proxy [%s:%i] status [%s]"
						,lp->http_proxy_host
						,lp->http_proxy_port
						,strerror(errno));
		return -1;
	}
	return 0;
}
static int tls_process_data(belle_sip_channel_t *obj,unsigned int revents){
	belle_sip_tls_channel_t* channel=(belle_sip_tls_channel_t*)obj;
	belle_sip_tls_listening_point_t* lp = (belle_sip_tls_listening_point_t*)(obj->lp);
	int err;
	
	if (obj->state == BELLE_SIP_CHANNEL_CONNECTING ) {
		if (!channel->socket_connected) {
			channel->socklen=sizeof(channel->ss);
			if (finalize_stream_connection((belle_sip_stream_channel_t*)obj,revents,(struct sockaddr*)&channel->ss,&channel->socklen)) {
				goto process_error;
			}
			
			channel->socket_connected=1;
			belle_sip_source_set_events((belle_sip_source_t*)channel,BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_ERROR);
			belle_sip_source_set_timeout((belle_sip_source_t*)obj,belle_sip_stack_get_transport_timeout(obj->stack));
			
			if (lp && lp->http_proxy_host) {
				belle_sip_message("Channel [%p]: Connected at TCP level, now doing http proxy connect",obj);
				if (tls_process_http_connect(channel)) goto process_error;
			} else {
				belle_sip_message("Channel [%p]: Connected at TCP level, now doing TLS handshake",obj);
				if (tls_process_handshake(obj)==-1) goto process_error;
			}
		} else if (lp && lp->http_proxy_host && !channel->http_proxy_connected) {
			char response[256];
			err = stream_channel_recv((belle_sip_stream_channel_t*)obj,response,sizeof(response));
			if (err<0 ){
				belle_sip_error("Channel [%p]: connection refused by http proxy [%s:%i] status [%s]"
								,channel
								,lp->http_proxy_host
								,lp->http_proxy_port
								,strerror(errno));
				goto process_error;
			} else if (strstr(response,"407")) {
				belle_sip_error("Channel [%p]: auth requested, provide user/passwd by http proxy [%s:%i]"
								,channel
								,lp->http_proxy_host
								,lp->http_proxy_port);
				goto process_error;
			} else if (strstr(response,"200")) {
				belle_sip_message("Channel [%p]: connected to http proxy, doing TLS handshake [%s:%i] "
								  ,channel
								  ,lp->http_proxy_host
								  ,lp->http_proxy_port);
				channel->http_proxy_connected = 1;
				if (tls_process_handshake(obj)==-1) goto process_error;
			} else {
				belle_sip_error("Channel [%p]: connection refused by http proxy [%s:%i]"
								,channel
								,lp->http_proxy_host
								,lp->http_proxy_port);
				goto process_error;
			}
			
		} else {
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
	size_t i=0;
	
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

// shim the default PolarSSL certificate handling by adding an external callback
// see "verify_cb_error_cb_t" for the function signature
int belle_sip_tls_set_verify_error_cb(void * callback)
{
	if (callback) {
        tls_verify_cb_error_cb = (verify_cb_error_cb_t)callback;
		belle_sip_message("belle_sip_tls_set_verify_error_cb: callback set");
	} else {
        tls_verify_cb_error_cb = NULL;
		belle_sip_message("belle_sip_tls_set_verify_error_cb: callback cleared");
	}
	return 0;
}

//
// Augment certificate verification with certificates stored outside rootca.pem
// PolarSSL calls the verify_cb with each cert in the chain; flags apply to the
// current certificate until depth is 0;
//
// NOTES:
// 1) rootca.pem *must* have at least one valid certificate, or PolarSSL
// does not attempt to verify any certificates
// 2) callback must return 0; non-zero indicates that the verification process failed
// 3) flags should be saved off and cleared for each certificate where depth>0
// 4) return final verification result in *flags when depth == 0
// 5) callback must disable calls to linphone_core_iterate while running
//

#if POLARSSL_VERSION_NUMBER < 0x01030000
int belle_sip_verify_cb_error_wrapper(x509_cert *cert, int depth, int *flags){
#else
int belle_sip_verify_cb_error_wrapper(x509_crt *cert, int depth, int *flags){
#endif
	int rc = 0;
	unsigned char *der = NULL;

	// do nothing if the callback is not set
	if (!tls_verify_cb_error_cb) {
		return 0;
	}

	belle_sip_message("belle_sip_verify_cb_error_wrapper: depth=[%d], flags=[%d]:\n", depth, *flags);

	der = belle_sip_malloc(cert->raw.len + 1);
	if (der == NULL) {
		// leave the flags alone and just return to the library
		belle_sip_error("belle_sip_verify_cb_error_wrapper: memory error\n");
		return 0;
	}

	// copy in and NULL terminate again for safety
	memcpy(der, cert->raw.p, cert->raw.len);
	der[cert->raw.len] = '\0';

	rc = tls_verify_cb_error_cb(der, cert->raw.len, depth, flags);

	belle_sip_message("belle_sip_verify_cb_error_wrapper: callback return rc: %d, flags: %d", rc, *flags);
	belle_sip_free(der);
	return rc;
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

	return belle_sip_verify_cb_error_wrapper(cert, depth, flags);
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


#else /*HAVE_POLARSSL*/
void belle_sip_tls_channel_set_client_certificates_chain(belle_sip_tls_channel_t *obj, belle_sip_certificates_chain_t* cert_chain) {
	belle_sip_error("belle_sip_channel_set_client_certificate_chain requires TLS");
}
void belle_sip_tls_channel_set_client_certificate_key(belle_sip_tls_channel_t *obj, belle_sip_signing_key_t* key) {
	belle_sip_error("belle_sip_channel_set_client_certificate_key requires TLS");
}
unsigned char *belle_sip_get_certificates_pem(belle_sip_certificates_chain_t *cert) {
	return NULL;
}
unsigned char *belle_sip_get_key_pem(belle_sip_signing_key_t *key) {
	return NULL;
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


/*
 * Parse all *.pem files in a given dir(non recursively) and return the one matching the given subject
 */
int belle_sip_get_certificate_and_pkey_in_dir(const char *path, const char *subject, belle_sip_certificates_chain_t **certificate, belle_sip_signing_key_t **pkey, belle_sip_certificate_raw_format_t format) {
#ifdef HAVE_POLARSSL
#if POLARSSL_VERSION_NUMBER < 0x01030000
	return -1;
#else /* POLARSSL_VERSION_NUMBER > 0x01030000 */
	/* get all *.pem file from given path */
	belle_sip_list_t *file_list = belle_sip_parse_directory(path, ".pem");
	char *filename = NULL;

	file_list = belle_sip_list_pop_front(file_list, (void **)&filename);
	while (filename != NULL) {
		belle_sip_certificates_chain_t *found_certificate = belle_sip_certificates_chain_parse_file(filename, format);
		if (found_certificate != NULL) { /* there is a certificate in this file */
			char *subject_CNAME_begin, *subject_CNAME_end;
			belle_sip_signing_key_t *found_key;
			char name[500];
			memset( name, 0, sizeof(name) );
			x509_dn_gets( name, sizeof(name), &(found_certificate->cert.subject)); /* this function is available only in polarssl version >=1.3 */
			/* parse subject to find the CN=xxx, field. There may be no , at the and but a \0 */
			subject_CNAME_begin = strstr(name, "CN=");
			if (subject_CNAME_begin!=NULL) {
				subject_CNAME_begin+=3;
				subject_CNAME_end = strstr(subject_CNAME_begin, ",");
				if (subject_CNAME_end != NULL) {
					*subject_CNAME_end = '\0';
				}
				if (strcmp(subject_CNAME_begin, subject)==0) { /* subject CNAME match the one we are looking for*/
					/* do we have a key too ? */
					found_key = belle_sip_signing_key_parse_file(filename, NULL);
					if (found_key!=NULL) {
						*certificate = found_certificate;
						*pkey = found_key;
						belle_sip_free(filename);
						belle_sip_list_free_with_data(file_list, belle_sip_free); /* free possible rest of list */
						return 0;
					}
				}
			}
		}
		belle_sip_free(filename);
		file_list = belle_sip_list_pop_front(file_list, (void **)&filename);
	}
	return -1;
#endif /* POLARSSL_VERSION_NUMBER >= 0x01030000 */
#else /* ! HAVE_POLARSSL */
	return -1;
#endif
}

int belle_sip_generate_self_signed_certificate(const char* path, const char *subject, belle_sip_certificates_chain_t **certificate, belle_sip_signing_key_t **pkey) {
#ifdef HAVE_POLARSSL
#if POLARSSL_VERSION_NUMBER < 0x01030000
	return -1;
#else /* POLARSSL_VERSION_NUMBER < 0x01030000 */
	entropy_context entropy;
	ctr_drbg_context ctr_drbg;
	int ret;
	mpi serial;
	x509write_cert crt;
	FILE *fd;
	char file_buffer[8192];
	size_t file_buffer_len = 0;
	char *name_with_path;
	int path_length;
	char formatted_subject[512];

	/* subject may be a sip URL or linphone-dtls-default-identity, add CN= before it to make a valid name */
	memcpy(formatted_subject, "CN=", 3);
	memcpy(formatted_subject+3, subject, strlen(subject)+1); /* +1 to get the \0 termination */

	/* allocate certificate and key */
	*pkey = belle_sip_object_new(belle_sip_signing_key_t);
	*certificate = belle_sip_object_new(belle_sip_certificates_chain_t);

	entropy_init( &entropy );
	if( ( ret = ctr_drbg_init( &ctr_drbg, entropy_func, &entropy, NULL, 0 ) )   != 0 )
	{
		belle_sip_error("Certificate generation can't init ctr_drbg: -%x", -ret);
		return -1;
	}

	/* generate 3072 bits RSA public/private key */
	pk_init( &((*pkey)->key) );
	if ( (ret = pk_init_ctx( &((*pkey)->key), pk_info_from_type( POLARSSL_PK_RSA ) )) != 0) {
		belle_sip_error("Certificate generation can't init pk_ctx: -%x", -ret);
		return -1;
	}
	if ( ( ret = rsa_gen_key( pk_rsa( (*pkey)->key ), ctr_drbg_random, &ctr_drbg, 3072, 65537 ) ) != 0) {
		belle_sip_error("Certificate generation can't generate rsa key: -%x", -ret);
		return -1;
	}

	/* if there is no path, don't write a file */
	if (path!=NULL) {
		pk_write_key_pem( &((*pkey)->key), (unsigned char *)file_buffer, 4096);
		file_buffer_len = strlen(file_buffer);
	}

	/* generate the certificate */
	x509write_crt_init( &crt );
	x509write_crt_set_md_alg( &crt, POLARSSL_MD_SHA256 );

	mpi_init( &serial );

	if ( (ret = mpi_read_string( &serial, 10, "1" ) ) != 0 ) {
		belle_sip_error("Certificate generation can't read serial mpi: -%x", -ret);
		return -1;
	}

	x509write_crt_set_subject_key( &crt, &((*pkey)->key) );
	x509write_crt_set_issuer_key( &crt, &((*pkey)->key) );

	if ( (ret = x509write_crt_set_subject_name( &crt, formatted_subject) ) != 0) {
		belle_sip_error("Certificate generation can't set subject name: -%x", -ret);
		return -1;
	}

	if ( (ret = x509write_crt_set_issuer_name( &crt, formatted_subject) ) != 0) {
		belle_sip_error("Certificate generation can't set issuer name: -%x", -ret);
		return -1;
	}

	if ( (ret = x509write_crt_set_serial( &crt, &serial ) ) != 0) {
		belle_sip_error("Certificate generation can't set serial: -%x", -ret);
		return -1;
	}
	mpi_free(&serial);

	if ( (ret = x509write_crt_set_validity( &crt, "20010101000000", "20300101000000" ) ) != 0) {
		belle_sip_error("Certificate generation can't set validity: -%x", -ret);
		return -1;
	}

	/* store anyway certificate in pem format in a string even if we do not have file to write as we need it to get it in a x509_crt structure */
	if ( (ret = x509write_crt_pem( &crt, (unsigned char *)file_buffer+file_buffer_len, 4096, ctr_drbg_random, &ctr_drbg ) ) != 0) {
		belle_sip_error("Certificate generation can't write crt pem: -%x", -ret);
		return -1;
	}

	x509write_crt_free(&crt);

	if ( (ret = x509_crt_parse(&((*certificate)->cert), (unsigned char *)file_buffer, strlen(file_buffer)) ) != 0) {
		belle_sip_error("Certificate generation can't parse crt pem: -%x", -ret);
		return -1;
	}

	/* write the file if needed */
	if (path!=NULL) {
		name_with_path = (char *)belle_sip_malloc(strlen(path)+257); /* max filename is 256 bytes in dirent structure, +1 for / */
		path_length = strlen(path);
		memcpy(name_with_path, path, path_length);
		name_with_path[path_length] = '/';
		path_length++;
		memcpy(name_with_path+path_length, subject, strlen(subject));
		memcpy(name_with_path+path_length+strlen(subject), ".pem", 5);

		/* check if directory exists and if not, create it */
		belle_sip_mkdir(path);

		if ( (fd = fopen(name_with_path, "w") ) == NULL) {
			belle_sip_error("Certificate generation can't open/create file %s", name_with_path);
			free(name_with_path);
			belle_sip_object_unref(*pkey);
			belle_sip_object_unref(*certificate);
			*pkey = NULL;
			*certificate = NULL;
			return -1;
		}
		if ( fwrite(file_buffer, 1, strlen(file_buffer), fd) != strlen(file_buffer) ) {
			belle_sip_error("Certificate generation can't write into file %s", name_with_path);
			fclose(fd);
			belle_sip_object_unref(*pkey);
			belle_sip_object_unref(*certificate);
			*pkey = NULL;
			*certificate = NULL;
			free(name_with_path);
			return -1;
		}
		fclose(fd);
		free(name_with_path);
	}

	return 0;
#endif /* else POLARSSL_VERSION_NUMBER < 0x01030000 */
#else /* ! HAVE_POLARSSL */
	return -1;
#endif
}

/* Note : this code is duplicated in mediastreamer2/src/voip/dtls_srtp.c but get directly a x509_crt as input parameter */
char *belle_sip_certificates_chain_get_fingerprint(belle_sip_certificates_chain_t *certificate) {
#ifdef HAVE_POLARSSL
#if POLARSSL_VERSION_NUMBER < 0x01030000
	return NULL;
#else /* POLARSSL_VERSION_NUMBER < 0x01030000 */
	unsigned char buffer[64]={0}; /* buffer is max length of returned hash, which is 64 in case we use sha-512 */
	size_t hash_length = 0;
	const char *hash_alg_string=NULL;
	char *fingerprint = NULL;
	x509_crt *crt;
	if (certificate == NULL) return NULL;

	crt = &certificate->cert;
	/* fingerprint is a hash of the DER formated certificate (found in crt->raw.p) using the same hash function used by certificate signature */
	switch (crt->sig_md) {
		case POLARSSL_MD_SHA1:
			sha1(crt->raw.p, crt->raw.len, buffer);
			hash_length = 20;
			hash_alg_string="SHA-1";
		break;

		case POLARSSL_MD_SHA224:
			sha256(crt->raw.p, crt->raw.len, buffer, 1); /* last argument is a boolean, indicate to output sha-224 and not sha-256 */
			hash_length = 28;
			hash_alg_string="SHA-224";
		break;

		case POLARSSL_MD_SHA256:
			sha256(crt->raw.p, crt->raw.len, buffer, 0);
			hash_length = 32;
			hash_alg_string="SHA-256";
		break;

		case POLARSSL_MD_SHA384:
			sha512(crt->raw.p, crt->raw.len, buffer, 1); /* last argument is a boolean, indicate to output sha-384 and not sha-512 */
			hash_length = 48;
			hash_alg_string="SHA-384";
		break;

		case POLARSSL_MD_SHA512:
			sha512(crt->raw.p, crt->raw.len, buffer, 1); /* last argument is a boolean, indicate to output sha-384 and not sha-512 */
			hash_length = 64;
			hash_alg_string="SHA-512";
		break;

		default:
			return NULL;
		break;
	}

	if (hash_length>0) {
		size_t i;
		int fingerprint_index = strlen(hash_alg_string);
		size_t size=fingerprint_index+3*hash_length+1;
		char prefix=' ';
		/* fingerprint will be : hash_alg_string+' '+HEX : separated values: length is strlen(hash_alg_string)+3*hash_lenght + 1 for null termination */
		fingerprint = belle_sip_malloc0(size);
		snprintf(fingerprint, size, "%s", hash_alg_string);
		for (i=0; i<hash_length; i++, fingerprint_index+=3) {
			snprintf((char*)fingerprint+fingerprint_index, size-fingerprint_index, "%c%02X", prefix,buffer[i]);
			prefix=':';
		}
		*(fingerprint+fingerprint_index) = '\0';
	}

	return fingerprint;
#endif /* else POLARSSL_VERSION_NUMBER < 0x01030000 */
#else /* ! HAVE_POLARSSL */
	return NULL;
#endif
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

	
GET_SET_STRING(belle_sip_tls_listening_point,http_proxy_host)
GET_SET_INT(belle_sip_tls_listening_point,http_proxy_port, int)

	


