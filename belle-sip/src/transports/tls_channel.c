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

#include "bctoolbox/crypto.h"

static int belle_sip_tls_channel_init_bctbx_ssl(belle_sip_tls_channel_t *obj);
static void belle_sip_tls_channel_deinit_bctbx_ssl(belle_sip_tls_channel_t *obj);

/*****************************************************************************/
/***               signing key structure and methods                       ***/
/*****************************************************************************/
struct belle_sip_signing_key {
	belle_sip_object_t objet;
	bctbx_signing_key_t *key;
};
/*************************************/
/***     Internal functions        ***/
static void belle_sip_signing_key_destroy(belle_sip_signing_key_t *signing_key){
	bctbx_signing_key_free(signing_key->key);
}

static void belle_sip_signing_key_clone(belle_sip_signing_key_t *signing_key, const belle_sip_signing_key_t *orig){
	belle_sip_error("belle_sip_signing_key_clone not supported");
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_signing_key_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_signing_key_t,belle_sip_object_t,belle_sip_signing_key_destroy,belle_sip_signing_key_clone,NULL,TRUE);

/*************************************/
/**       Exposed functions         **/
/**
 * Retrieve key in a string(PEM format)
 */
char *belle_sip_signing_key_get_pem(belle_sip_signing_key_t *key) {
	if (key == NULL) return NULL;
	return bctbx_signing_key_get_pem(key->key);
}

belle_sip_signing_key_t *belle_sip_signing_key_new(void) {

	belle_sip_signing_key_t* key = belle_sip_object_new(belle_sip_signing_key_t);
	key->key  = bctbx_signing_key_new();
	return key;
}

belle_sip_signing_key_t* belle_sip_signing_key_parse(const char* buff, size_t size, const char* passwd) {
	belle_sip_signing_key_t *signing_key = belle_sip_signing_key_new();
	int ret;

	/* check size, buff is the key in PEM format and thus shall include a NULL termination char, make size includes this termination */
	if (strlen(buff) == size) {
		size++;
	}

	ret = bctbx_signing_key_parse(signing_key->key, buff, size, (const unsigned char *)passwd, passwd?strlen(passwd):0);

	if (ret < 0) {
		char tmp[128];
		bctbx_strerror(ret,tmp,sizeof(tmp));
		belle_sip_error("cannot parse x509 signing key because [%s]",tmp);
		belle_sip_object_unref(signing_key);
		return NULL;
	}
	return signing_key;
}

belle_sip_signing_key_t* belle_sip_signing_key_parse_file(const char* path,const char* passwd) {
	belle_sip_signing_key_t *signing_key = belle_sip_signing_key_new();
	int ret;

	ret = bctbx_signing_key_parse_file(signing_key->key, path, passwd);

	if (ret < 0) {
		char tmp[128];
		bctbx_strerror(ret,tmp,sizeof(tmp));
		belle_sip_error("cannot parse x509 signing key because [%s]",tmp);
		belle_sip_object_unref(signing_key);
		return NULL;
	}
	return signing_key;
}
/*****************************************************************************/
/***            end of signing key structure and methods                   ***/
/*****************************************************************************/


/*****************************************************************************/
/***               certificate structure and methods                       ***/
/*****************************************************************************/
struct belle_sip_certificates_chain {
	belle_sip_object_t objet;
	bctbx_x509_certificate_t *cert;
};

/*************************************/
/***     Internal functions        ***/
static void belle_sip_certificates_chain_destroy(belle_sip_certificates_chain_t *certificate){
	bctbx_x509_certificate_free(certificate->cert);
}

static void belle_sip_certificates_chain_clone(belle_sip_certificates_chain_t *certificate, const belle_sip_certificates_chain_t *orig){
	belle_sip_error("belle_sip_certificate_clone not supported");
}

static int belle_sip_certificate_fill(belle_sip_certificates_chain_t* certificate, const char* buff, size_t size, belle_sip_certificate_raw_format_t format) {

	int err;
	if (format == BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM) {
		/* if format is PEM, make sure the null termination char is included in the buffer given size */
		if (strlen(buff) == size) {
			size++;
		}
	}
	if ((err=bctbx_x509_certificate_parse(certificate->cert, buff, size)) <0) {
		char tmp[128];
		bctbx_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("cannot parse x509 cert because [%s]",tmp);
		return -1;
	}
	return 0;
}

static int belle_sip_certificate_fill_from_file(belle_sip_certificates_chain_t* certificate, const char* path, belle_sip_certificate_raw_format_t format) {
	int err;
	if ((err=bctbx_x509_certificate_parse_file(certificate->cert, path)) <0) {
		char tmp[128];
		bctbx_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("cannot parse x509 cert because [%s]",tmp);
		return -1;
	}
	return 0;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_certificates_chain_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_certificates_chain_t,belle_sip_object_t,belle_sip_certificates_chain_destroy,belle_sip_certificates_chain_clone,NULL,TRUE);

/*************************************/
/**       Exposed functions         **/

/**
 * Retrieve key or certificate in a string(PEM format)
 */
char *belle_sip_certificates_chain_get_pem(belle_sip_certificates_chain_t *cert) {
	if (cert == NULL) return NULL;
	return bctbx_x509_certificates_chain_get_pem(cert->cert);
}

belle_sip_certificates_chain_t *belle_sip_certificate_chain_new(void) {

	belle_sip_certificates_chain_t* certificate = belle_sip_object_new(belle_sip_certificates_chain_t);
	certificate->cert  = bctbx_x509_certificate_new();
	return certificate;
}

belle_sip_certificates_chain_t* belle_sip_certificates_chain_parse(const char* buff, size_t size,belle_sip_certificate_raw_format_t format) {
	belle_sip_certificates_chain_t* certificate = belle_sip_certificate_chain_new();

	if (belle_sip_certificate_fill(certificate, buff, size, format)) {
		belle_sip_object_unref(certificate);
		certificate=NULL;
	}

	return certificate;
}

belle_sip_certificates_chain_t* belle_sip_certificates_chain_parse_file(const char* path, belle_sip_certificate_raw_format_t format) {
	belle_sip_certificates_chain_t* certificate = belle_sip_certificate_chain_new();

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
			if (bctbx_x509_certificate_get_subject_dn(found_certificate->cert, name, sizeof(name)) > 0) {
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
					} else { /* doesn't match, unref the created certificate */
						belle_sip_object_unref(found_certificate);
					}
				}
			} else { /* no DN, just free it then */
				belle_sip_object_unref(found_certificate);
			}
		}
		belle_sip_free(filename);
		file_list = belle_sip_list_pop_front(file_list, (void **)&filename);
	}
	return -1;
}

int belle_sip_generate_self_signed_certificate(const char* path, const char *subject, belle_sip_certificates_chain_t **certificate, belle_sip_signing_key_t **pkey) {
	char pem_buffer[8192];
	int ret = 0;

	/* allocate certificate and key */
	*pkey = belle_sip_signing_key_new();
	*certificate = belle_sip_certificate_chain_new();

	ret = bctbx_x509_certificate_generate_selfsigned(subject, (*certificate)->cert, (*pkey)->key, (path==NULL)?NULL:pem_buffer, (path==NULL)?0:8192);
	if ( ret != 0) {
		belle_sip_error("Unable to generate self signed certificate : -%x", -ret);
		belle_sip_object_unref(*pkey);
		belle_sip_object_unref(*certificate);
		*pkey = NULL;
		*certificate = NULL;
		return ret;
	}

	/* write the file if needed */
	if (path!=NULL) {
		FILE *fd;
		char *name_with_path;
		size_t path_length;

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
			belle_sip_free(name_with_path);
			return -1;
		}
		if ( fwrite(pem_buffer, 1, strlen(pem_buffer), fd) != strlen(pem_buffer) ) {
			belle_sip_error("Certificate generation can't write into file %s", name_with_path);
			fclose(fd);
			belle_sip_object_unref(*pkey);
			belle_sip_object_unref(*certificate);
			*pkey = NULL;
			*certificate = NULL;
			belle_sip_free(name_with_path);
			return -1;
		}
		fclose(fd);
		belle_sip_free(name_with_path);
	}

	return 0;
}

char *belle_sip_certificates_chain_get_fingerprint(belle_sip_certificates_chain_t *certificate) {
	int ret;
	char *fingerprint=belle_sip_malloc0(200);

	/* compute the certificate using the hash algorithm used in the certificate signature */
	ret = bctbx_x509_certificate_get_fingerprint(certificate->cert, fingerprint, 200, BCTBX_MD_UNDEFINED);
	if ( ret <=0) {
		belle_sip_error("Unable to generate fingerprint from certificate [-0x%x]", -ret);
		belle_sip_free(fingerprint);
		return NULL;
	}

	return fingerprint;
}

/*****************************************************************************/
/***              end of certificate structure and methods                 ***/
/*****************************************************************************/

/*************tls********/
// SSL verification callback prototype
// der - raw certificate data, in DER format
// length - length of certificate DER data
// depth - position of certificate in cert chain, ending at 0 = root or top
// flags - verification state for CURRENT certificate only
typedef int (*verify_cb_error_cb_t)(unsigned char* der, int length, int depth, uint32_t* flags);
static verify_cb_error_cb_t tls_verify_cb_error_cb = NULL;

static int tls_process_data(belle_sip_channel_t *obj,unsigned int revents);

struct belle_sip_tls_channel{
	belle_sip_stream_channel_t base;
	bctbx_ssl_context_t *sslctx;
	bctbx_ssl_config_t *sslcfg;
	bctbx_x509_certificate_t *root_ca;

	struct sockaddr_storage ss;
	socklen_t socklen;
	int socket_connected;
	char *cur_debug_msg;
	belle_sip_certificates_chain_t* client_cert_chain;
	belle_sip_signing_key_t* client_cert_key;
	belle_tls_crypto_config_t *crypto_config;
	int http_proxy_connected;
	belle_sip_resolver_context_t *http_proxy_resolver_ctx;
};

static void tls_channel_close(belle_sip_tls_channel_t *obj){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	if (sock!=-1 && belle_sip_channel_get_state((belle_sip_channel_t*)obj)!=BELLE_SIP_CHANNEL_ERROR && !obj->base.base.closed_by_remote) {
		if (obj->sslctx) bctbx_ssl_close_notify(obj->sslctx);
	}
	stream_channel_close((belle_sip_stream_channel_t*)obj);
	belle_sip_tls_channel_deinit_bctbx_ssl(obj);
	obj->socket_connected=0;
}

static void tls_channel_uninit(belle_sip_tls_channel_t *obj){
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t*)obj);
	if (sock!=(belle_sip_socket_t)-1)
		tls_channel_close(obj);
	belle_sip_tls_channel_deinit_bctbx_ssl(obj);

	if (obj->cur_debug_msg)
		belle_sip_free(obj->cur_debug_msg);
	belle_sip_object_unref(obj->crypto_config);
	if (obj->client_cert_chain) belle_sip_object_unref(obj->client_cert_chain);
	if (obj->client_cert_key) belle_sip_object_unref(obj->client_cert_key);
	if (obj->http_proxy_resolver_ctx) belle_sip_object_unref(obj->http_proxy_resolver_ctx);
}

static int tls_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen){
	belle_sip_tls_channel_t* channel = (belle_sip_tls_channel_t*)obj;
	int err = bctbx_ssl_write(channel->sslctx,buf,buflen);
	if (err<0){
		char tmp[256]={0};
		if (err==BCTBX_ERROR_NET_WANT_WRITE) return -BELLESIP_EWOULDBLOCK;
		bctbx_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("Channel [%p]: ssl_write() error [%i]: %s",obj,err,tmp);
	}
	return err;
}

static int tls_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	belle_sip_tls_channel_t* channel = (belle_sip_tls_channel_t*)obj;
	int err = bctbx_ssl_read(channel->sslctx,buf,buflen);
	if (err==BCTBX_ERROR_SSL_PEER_CLOSE_NOTIFY) return 0;
	if (err<0){
		char tmp[256]={0};
		if (err==BCTBX_ERROR_NET_WANT_READ) return -BELLESIP_EWOULDBLOCK;
		bctbx_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("Channel [%p]: ssl_read() error [%i]: %s",obj, err, tmp);
	}
	return err;
}

static int tls_channel_connect_to(belle_sip_channel_t *obj, const struct addrinfo *ai){
	int err;
	
	if (belle_sip_tls_channel_init_bctbx_ssl((belle_sip_tls_channel_t*)obj)==-1) return -1;
	
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
		bctbx_freeaddrinfo(ai_list);
	}else{
		belle_sip_error("%s: DNS resolution failed for %s", __FUNCTION__, name);
		channel_set_state((belle_sip_channel_t*)obj,BELLE_SIP_CHANNEL_ERROR);
	}
}

static int tls_channel_connect(belle_sip_channel_t *obj, const struct addrinfo *ai){
	belle_sip_tls_channel_t *channel=(belle_sip_tls_channel_t*)obj;
	if (obj->stack->http_proxy_host) {
		belle_sip_message("Resolving http proxy addr [%s] for channel [%p]",obj->stack->http_proxy_host,obj);
		/*assume ai family is the same*/
		channel->http_proxy_resolver_ctx = belle_sip_stack_resolve_a(obj->stack, obj->stack->http_proxy_host, obj->stack->http_proxy_port, obj->ai_family, http_proxy_res_done, obj);
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
				NULL,
				BELLE_SIP_DEFAULT_BUFSIZE_HINT
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

static int belle_sip_client_certificate_request_callback(void *data, bctbx_ssl_context_t *ssl_ctx, unsigned char *dn, size_t dn_length) {
	belle_sip_tls_channel_t *channel = (belle_sip_tls_channel_t *)data;

	/* ask certificate */
	BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(	channel->base.base.full_listeners
			,belle_sip_channel_listener_t
			,on_auth_requested
			,&channel->base.base
			,(char *)dn);

	/* if we got one, set it in the ssl handshake context */
	if (channel->client_cert_chain && channel->client_cert_key) {
		int err;
		char tmp[512]={0};

		bctbx_x509_certificate_get_info_string(tmp,sizeof(tmp)-1,"",channel->client_cert_chain->cert);
		belle_sip_message("Channel [%p]  found client  certificate:\n%s",channel,tmp);

		if ((err=bctbx_ssl_set_hs_own_cert(channel->sslctx,channel->client_cert_chain->cert,channel->client_cert_key->key))) {
			bctbx_strerror(err,tmp,sizeof(tmp)-1);
			belle_sip_error("Channel [%p] cannot set retrieved ssl own certificate [%s]",channel,tmp);
			return -1; /* we were not able to set the client certificate, something is going wrong, this will abort the handshake*/
		}
		return 0;
	}

	belle_sip_warning("Channel [%p] cannot get client certificate to answer server request for dn [%s]", channel, (dn==NULL)?"null":(char *)dn);

	return 0; /* we couldn't find any certificate, just keep on going, server may decide to abort the handshake */
}

static int tls_process_handshake(belle_sip_channel_t *obj){
	belle_sip_tls_channel_t* channel=(belle_sip_tls_channel_t*)obj;
	int err=bctbx_ssl_handshake(channel->sslctx);
	if (err==0){
		belle_sip_message("Channel [%p]: SSL handshake finished, SSL version is [%s], selected ciphersuite is [%s]",obj,
				  bctbx_ssl_get_version(channel->sslctx), bctbx_ssl_get_ciphersuite(channel->sslctx));
		belle_sip_source_set_timeout((belle_sip_source_t*)obj,-1);
		belle_sip_channel_set_ready(obj,(struct sockaddr*)&channel->ss,channel->socklen);
	}else if (err==BCTBX_ERROR_NET_WANT_READ || err==BCTBX_ERROR_NET_WANT_WRITE){
		belle_sip_message("Channel [%p]: SSL handshake in progress...",obj);
	}else{
		char tmp[128];
		bctbx_strerror(err,tmp,sizeof(tmp));
		belle_sip_error("Channel [%p]: SSL handshake failed : %s",obj,tmp);
		return -1;
	}
	return 0;
}

static int tls_process_http_connect(belle_sip_tls_channel_t *obj) {
	char* request;
	belle_sip_channel_t *channel = (belle_sip_channel_t *)obj;
	int err;
	char ip[64];
	char *host_ip;
	char url_ipport[64];
	int port;
	bctbx_addrinfo_to_printable_ip_address(channel->current_peer,url_ipport,sizeof(url_ipport));
	bctbx_addrinfo_to_ip_address(channel->current_peer,ip,sizeof(ip),&port);
	
	if (channel->current_peer->ai_family == AF_INET6) {
		host_ip = belle_sip_strdup_printf("[%s]",ip);
	} else {
		host_ip = belle_sip_strdup_printf("%s",ip); /*just to simplify code*/
	}

	request = belle_sip_strdup_printf("CONNECT %s HTTP/1.1\r\nProxy-Connection: keep-alive\r\nConnection: keep-alive\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\n"
									  ,url_ipport
									  ,host_ip);
	
	belle_sip_free(host_ip);

	if (channel->stack->http_proxy_username && channel->stack->http_proxy_passwd) {
		char *username_passwd = belle_sip_strdup_printf("%s:%s",channel->stack->http_proxy_username,channel->stack->http_proxy_passwd);
		size_t username_passwd_length = strlen(username_passwd);
		size_t encoded_username_paswd_length = username_passwd_length*2;
		unsigned char *encoded_username_paswd = belle_sip_malloc(2*username_passwd_length);
		bctbx_base64_encode(encoded_username_paswd,&encoded_username_paswd_length,(const unsigned char *)username_passwd,username_passwd_length);
		request = belle_sip_strcat_printf(request, "Proxy-Authorization: Basic %s\r\n",encoded_username_paswd);
		belle_sip_free(username_passwd);
		belle_sip_free(encoded_username_paswd);
	}

	request = belle_sip_strcat_printf(request,"\r\n");
	err = bctbx_send(belle_sip_source_get_socket((belle_sip_source_t*)obj),request,strlen(request),0);
	belle_sip_free(request);
	if (err <= 0) {
		belle_sip_error("tls_process_http_connect: fail to send connect request to http proxy [%s:%i] status [%s]"
						,channel->stack->http_proxy_host
						,channel->stack->http_proxy_port
						,strerror(errno));
		return -1;
	}
	return 0;
}

static int tls_process_data(belle_sip_channel_t *obj,unsigned int revents){
	belle_sip_tls_channel_t* channel=(belle_sip_tls_channel_t*)obj;
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
			if (obj->stack->http_proxy_host) {
				belle_sip_message("Channel [%p]: Connected at TCP level, now doing http proxy connect",obj);
				if (tls_process_http_connect(channel)) goto process_error;
			} else {
				belle_sip_message("Channel [%p]: Connected at TCP level, now doing TLS handshake with cname=%s",obj, 
						  obj->peer_cname ? obj->peer_cname : obj->peer_name);
				if (tls_process_handshake(obj)==-1) goto process_error;
			}
		} else if (obj->stack->http_proxy_host && !channel->http_proxy_connected) {
			char response[256];
			err = stream_channel_recv((belle_sip_stream_channel_t*)obj,response,sizeof(response));
			if (err<0 ){
				belle_sip_error("Channel [%p]: connection refused by http proxy [%s:%i] status [%s]"
								,channel
								,obj->stack->http_proxy_host
								,obj->stack->http_proxy_port
								,strerror(errno));
				goto process_error;
			} else if (strstr(response,"407")) {
				belle_sip_error("Channel [%p]: auth requested, provide user/passwd by http proxy [%s:%i]"
								,channel
								,obj->stack->http_proxy_host
								,obj->stack->http_proxy_port);
				goto process_error;
			} else if (strstr(response,"200")) {
				belle_sip_message("Channel [%p]: connected to http proxy, doing TLS handshake [%s:%i] "
								  ,channel
								  ,obj->stack->http_proxy_host
								  ,obj->stack->http_proxy_port);
				channel->http_proxy_connected = 1;
				if (tls_process_handshake(obj)==-1) goto process_error;
			} else {
				belle_sip_error("Channel [%p]: connection refused by http proxy [%s:%i]"
								,channel
								,obj->stack->http_proxy_host
								,obj->stack->http_proxy_port);
				goto process_error;
			}

		} else {
			if (revents & BELLE_SIP_EVENT_READ){
				if (tls_process_handshake(obj)==-1) goto process_error;
			}else if (revents==BELLE_SIP_EVENT_TIMEOUT){
				belle_sip_error("channel [%p]: SSL handshake took too much time.",obj);
				goto process_error;
			}else{
				belle_sip_warning("channel [%p]: unexpected event [%i] during TLS handshake.",obj,revents);
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

static int tls_callback_read(void * ctx, unsigned char *buf, size_t len ){
	belle_sip_stream_channel_t *super=(belle_sip_stream_channel_t *)ctx;

	int ret = stream_channel_recv(super,buf,len);

	if (ret<0){
		ret=-ret;
		if (ret==BELLESIP_EWOULDBLOCK || ret==BELLESIP_EINPROGRESS || ret == EINTR )
			return BCTBX_ERROR_NET_WANT_READ;
		return BCTBX_ERROR_NET_CONN_RESET;
	}
	return ret;
}

static int tls_callback_write(void * ctx, const unsigned char *buf, size_t len ){
	belle_sip_stream_channel_t *super=(belle_sip_stream_channel_t *)ctx;

	int ret = stream_channel_send(super, buf, len);

	if (ret<0){
		ret=-ret;
		if (ret==BELLESIP_EWOULDBLOCK || ret==BELLESIP_EINPROGRESS || ret == EINTR )
			return BCTBX_ERROR_NET_WANT_WRITE;
		return BCTBX_ERROR_NET_CONN_RESET;
	}
	return ret;
}

static int random_generator(void *ctx, unsigned char *ptr, size_t size){
	belle_sip_random_bytes(ptr, size);
	return 0;
}


// shim the default certificate handling by adding an external callback
// see "verify_cb_error_cb_t" for the function signature
int belle_sip_tls_set_verify_error_cb(void * callback) {

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
int belle_sip_verify_cb_error_wrapper(bctbx_x509_certificate_t *cert, int depth, uint32_t *flags) {

	int rc = 0;
	unsigned char *der = NULL;
	int der_length = 0;

	// do nothing if the callback is not set
	if (!tls_verify_cb_error_cb) {
		return 0;
	}

	belle_sip_message("belle_sip_verify_cb_error_wrapper: depth=[%d], flags=[0x%x]:\n", depth, *flags);

	der_length = bctbx_x509_certificate_get_der_length(cert);
	der = belle_sip_malloc(der_length+1); // +1 for null termination char
	if (der == NULL) {
		// leave the flags alone and just return to the library
		belle_sip_error("belle_sip_verify_cb_error_wrapper: memory error\n");
		return 0;
	}

	bctbx_x509_certificate_get_der(cert, der, der_length+1);

	rc = tls_verify_cb_error_cb(der, der_length, depth, flags);

	belle_sip_message("belle_sip_verify_cb_error_wrapper: callback return rc: %d, flags: 0x%x", rc, *flags);
	belle_sip_free(der);
	return rc;
}


static int belle_sip_ssl_verify(void *data , bctbx_x509_certificate_t *cert , int depth, uint32_t *flags){

	belle_tls_crypto_config_t *crypto_config=(belle_tls_crypto_config_t*)data;
	const int tmp_size = 2048, flags_str_size = 256;
	char *tmp = belle_sip_malloc0(tmp_size);
	char *flags_str = belle_sip_malloc0(flags_str_size);
	int ret;

	bctbx_x509_certificate_get_info_string(tmp, tmp_size-1, "", cert);
	bctbx_x509_certificate_flags_to_string(flags_str, flags_str_size-1, *flags);

	belle_sip_message("Found certificate depth=[%i], flags=[%s]:\n%s", depth, flags_str, tmp);

	if (crypto_config->exception_flags==BELLE_TLS_VERIFY_ANY_REASON){
		belle_sip_warning("Certificate verification bypassed (requested by application).");
		/* verify context ask to ignore any exception: reset all flags */
		bctbx_x509_certificate_unset_flag(flags, BCTBX_CERTIFICATE_VERIFY_ALL_FLAGS);
	}else if (crypto_config->exception_flags & BELLE_TLS_VERIFY_CN_MISMATCH){
		/* verify context ask to ignore CN mismatch exception : reset this flag */
		belle_sip_warning("Allowing CN-mistmatch exception.");
		bctbx_x509_certificate_unset_flag(flags, BCTBX_CERTIFICATE_VERIFY_BADCERT_CN_MISMATCH);
	}

	ret = belle_sip_verify_cb_error_wrapper(cert, depth, flags);

	belle_sip_free(flags_str);
	belle_sip_free(tmp);

	return ret;
}

static int belle_sip_tls_channel_load_root_ca(belle_sip_tls_channel_t *obj, const char *path){
	struct stat statbuf;
	if (stat(path,&statbuf)==0){
		int error;
		if (obj->root_ca) {
			bctbx_x509_certificate_free(obj->root_ca);
		}
		obj->root_ca = bctbx_x509_certificate_new();

		if (statbuf.st_mode & S_IFDIR){
			error = bctbx_x509_certificate_parse_path(obj->root_ca,path);
		}else{
			error = bctbx_x509_certificate_parse_file(obj->root_ca,path);
		}
		if (error<0){
			char errorstr[512];
			bctbx_strerror(error, errorstr, sizeof(errorstr));
			belle_sip_error("Failed to load root ca from %s: %s",path,errorstr);
			return -1;
		} else {
			return 0;
		}
	}
	belle_sip_error("Could not load root ca from %s: %s",path,strerror(errno));
	return -1;
}

static int belle_sip_tls_channel_load_root_ca_from_buffer(belle_sip_tls_channel_t *obj, const char *data) {
	int err = 0;
	if (data != NULL) {
		if (obj->root_ca) {
			bctbx_x509_certificate_free(obj->root_ca);
		}
		obj->root_ca = bctbx_x509_certificate_new();
		//certificate data must to contain in size the NULL character
		err = bctbx_x509_certificate_parse(obj->root_ca, data, strlen(data) + 1);
		if (err) {
			belle_sip_error("Failed to load root ca from string data: 0x%x", err);
			return -1;
		}
		belle_sip_message("Root ca loaded from string data");
		return 0;
	}
	belle_sip_error("Could not load root ca from null string");
	return -1;
}

static void belle_sip_tls_channel_deinit_bctbx_ssl(belle_sip_tls_channel_t *obj){
	if (obj->sslctx) {
		bctbx_ssl_context_free(obj->sslctx);
		obj->sslctx = NULL;
	}
	if (obj->sslcfg) {
		bctbx_ssl_config_free(obj->sslcfg);
		obj->sslcfg = NULL;
	}
	if (obj->root_ca) {
		bctbx_x509_certificate_free(obj->root_ca);
		obj->root_ca = NULL;
	}
	
}

static int belle_sip_tls_channel_init_bctbx_ssl(belle_sip_tls_channel_t *obj){
	belle_sip_stream_channel_t* super=(belle_sip_stream_channel_t*)obj;
	belle_tls_crypto_config_t *crypto_config = obj->crypto_config;
	
	/* create and initialise ssl context and configuration */
	obj->sslctx = bctbx_ssl_context_new();
	obj->sslcfg = bctbx_ssl_config_new();
	if (crypto_config->ssl_config == NULL) {
		bctbx_ssl_config_defaults(obj->sslcfg, BCTBX_SSL_IS_CLIENT, BCTBX_SSL_TRANSPORT_STREAM);
		bctbx_ssl_config_set_authmode(obj->sslcfg, BCTBX_SSL_VERIFY_REQUIRED);
	} else { /* an SSL config is provided, use it*/
		int ret = bctbx_ssl_config_set_crypto_library_config(obj->sslcfg, crypto_config->ssl_config);
		if (ret<0) {
			belle_sip_error("Unable to set external config for SSL context at TLS channel creation ret [-0x%x]", -ret);
			belle_sip_object_unref(obj);
			return -1;
		}
		belle_sip_message("Use externally provided SSL configuration when creating TLS channel [%p]", obj);
	}

	bctbx_ssl_config_set_rng(obj->sslcfg, random_generator, NULL);
	bctbx_ssl_set_io_callbacks(obj->sslctx, obj, tls_callback_write, tls_callback_read);
	if ((crypto_config->root_ca_data && belle_sip_tls_channel_load_root_ca_from_buffer(obj, crypto_config->root_ca_data) == 0)
		||  (crypto_config->root_ca && belle_sip_tls_channel_load_root_ca(obj, crypto_config->root_ca) == 0)){
		bctbx_ssl_config_set_ca_chain(obj->sslcfg, obj->root_ca);
	}
	bctbx_ssl_config_set_callback_verify(obj->sslcfg, belle_sip_ssl_verify, crypto_config);
	bctbx_ssl_config_set_callback_cli_cert(obj->sslcfg, belle_sip_client_certificate_request_callback, obj);

	bctbx_ssl_context_setup(obj->sslctx, obj->sslcfg);
	bctbx_ssl_set_hostname(obj->sslctx, super->base.peer_cname ? super->base.peer_cname : super->base.peer_name);
	return 0;
}

belle_sip_channel_t * belle_sip_channel_new_tls(belle_sip_stack_t *stack, belle_tls_crypto_config_t *crypto_config, const char *bindip, int localport, const char *peer_cname, const char *dest, int port) {
	belle_sip_tls_channel_t *obj=belle_sip_object_new(belle_sip_tls_channel_t);
	belle_sip_stream_channel_t* super=(belle_sip_stream_channel_t*)obj;

	belle_sip_stream_channel_init_client(super
					,stack
					,bindip,localport,peer_cname,dest,port);

	obj->crypto_config=(belle_tls_crypto_config_t*)belle_sip_object_ref(crypto_config);
	return (belle_sip_channel_t*)obj;
}

void belle_sip_tls_channel_set_client_certificates_chain(belle_sip_tls_channel_t *channel, belle_sip_certificates_chain_t* cert_chain) {
	SET_OBJECT_PROPERTY(channel,client_cert_chain,cert_chain);

}
void belle_sip_tls_channel_set_client_certificate_key(belle_sip_tls_channel_t *channel, belle_sip_signing_key_t* key){
	SET_OBJECT_PROPERTY(channel,client_cert_key,key);
}
