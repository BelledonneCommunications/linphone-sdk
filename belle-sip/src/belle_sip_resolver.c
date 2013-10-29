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
#include "dns.h"

#include <stdlib.h>
#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#define DNS_EAGAIN  EAGAIN


struct belle_sip_resolver_context{
	belle_sip_source_t source;
	belle_sip_stack_t *stack;
	belle_sip_main_loop_t *ml;
	belle_sip_resolver_callback_t cb;
	belle_sip_resolver_srv_callback_t srv_cb;
	void *cb_data;
	struct dns_resolv_conf *resconf;
	struct dns_hosts *hosts;
	struct dns_resolver *R;
	enum dns_type type;
	char *name;
	int port;
	struct addrinfo *ai_list;
	belle_sip_list_t *srv_list;
	int family;
	uint8_t cancelled;
	uint8_t started;
	uint8_t done;
};


static struct dns_resolv_conf *resconf(belle_sip_resolver_context_t *ctx) {
#if !_WIN32 && !HAVE_RESINIT
/*#if !_WIN32 && (!HAVE_RESINIT || !TARGET_OS_IPHONE)*/
	const char *path;
#endif
	int error;

	if (ctx->resconf)
		return ctx->resconf;

	if (!(ctx->resconf = dns_resconf_open(&error))) {
		belle_sip_error("%s dns_resconf_open error: %s", __FUNCTION__, dns_strerror(error));
		return NULL;
	}

#ifdef _WIN32
	error = dns_resconf_loadwin(ctx->resconf);
	if (error) {
		belle_sip_error("%s dns_resconf_loadwin error", __FUNCTION__);
	}
#elif ANDROID
	error = dns_resconf_loadandroid(ctx->resconf);
	if (error) {
		belle_sip_error("%s dns_resconf_loadandroid error", __FUNCTION__);
	}
#elif HAVE_RESINIT
/*#elif HAVE_RESINIT && TARGET_OS_IPHONE*/
	error = dns_resconf_loadfromresolv(ctx->resconf);
	if (error) {
		belle_sip_error("%s dns_resconf_loadfromresolv error", __FUNCTION__);
	}
#else
	path = "/etc/resolv.conf";
	error = dns_resconf_loadpath(ctx->resconf, path);
	if (error) {
		belle_sip_error("%s dns_resconf_loadpath error [%s]: %s", __FUNCTION__, path, dns_strerror(error));
		return NULL;
	}

	path = "/etc/nsswitch.conf";
	error = dns_nssconf_loadpath(ctx->resconf, path);
	if (error) {
		belle_sip_message("%s dns_nssconf_loadpath error [%s]: %s", __FUNCTION__, path, dns_strerror(error));
	}
#endif

	return ctx->resconf;
}

static struct dns_hosts *hosts(belle_sip_resolver_context_t *ctx) {
	int error;

	if (ctx->hosts)
		return ctx->hosts;

	if (!(ctx->hosts = dns_hosts_local(&error))) {
		belle_sip_error("%s dns_hosts_local error: %s", __FUNCTION__, dns_strerror(error));
		return NULL;
	}

	if (ctx->stack->dns_user_hosts_file) {
		error = dns_hosts_loadpath(ctx->hosts, ctx->stack->dns_user_hosts_file);
		if (error) {
			belle_sip_error("%s dns_hosts_loadfile(\"%s\"): %s", __FUNCTION__,ctx->stack->dns_user_hosts_file,dns_strerror(error));
		}
	}

	return ctx->hosts;
}

struct dns_cache *cache(belle_sip_resolver_context_t *ctx) {
	return NULL;
}

static struct addrinfo * ai_list_append(struct addrinfo *ai_list, struct addrinfo *ai_to_append) {
	struct addrinfo *ai_current = ai_list;
	if (ai_to_append == NULL) return ai_list;
	if (ai_list == NULL) return ai_to_append;
	while (ai_current->ai_next != NULL) {
		ai_current = ai_current->ai_next;
	}
	ai_current->ai_next = ai_to_append;
	return ai_list;
}

static int srv_compare_prio(const void *psrv1, const void *psrv2){
	struct dns_srv *srv1=(struct dns_srv*)psrv1;
	struct dns_srv *srv2=(struct dns_srv*)psrv2;
	if (srv1->priority < srv2->priority) return -1;
	if (srv1->priority == srv2->priority) return 0;
	return 1;
}

static int resolver_process_data(belle_sip_resolver_context_t *ctx, unsigned int revents) {
	char host[NI_MAXHOST + 1];
	struct dns_packet *ans;
	struct dns_rr_i *I;
	struct dns_rr_i dns_rr_it;
	int error;
	int gai_err;

	if (revents & BELLE_SIP_EVENT_TIMEOUT) {
		belle_sip_error("%s timed-out", __FUNCTION__);
		if ((ctx->type == DNS_T_A) || (ctx->type == DNS_T_AAAA)) {
			ctx->cb(ctx->cb_data, ctx->name, NULL);
		} else if (ctx->type == DNS_T_SRV) {
			ctx->srv_cb(ctx->cb_data, ctx->name, NULL);
		}
		ctx->done=TRUE;
		return BELLE_SIP_STOP;
	}
	if (ctx->cancelled) {
		ctx->done=TRUE;
		return BELLE_SIP_STOP;
	}

	error = dns_res_check(ctx->R);
	if (!error) {
		struct dns_rr rr;
		union dns_any any;
		enum dns_section section = DNS_S_AN;

		ans = dns_res_fetch(ctx->R, &error);
		memset(&dns_rr_it, 0, sizeof dns_rr_it);
		I = dns_rr_i_init(&dns_rr_it, ans);

		while (dns_rr_grep(&rr, 1, I, ans, &error)) {
			if (rr.section == section) {
				if ((error = dns_any_parse(dns_any_init(&any, sizeof(any)), &rr, ans))) {
					belle_sip_error("%s dns_any_parse error: %s", __FUNCTION__, dns_strerror(error));
					break;
				}
				if ((ctx->type == DNS_T_AAAA) && (rr.class == DNS_C_IN) && (rr.type == DNS_T_AAAA)) {
					struct dns_aaaa *aaaa = &any.aaaa;
					struct sockaddr_in6 sin6;
					memset(&sin6, 0, sizeof(sin6));
					memcpy(&sin6.sin6_addr, &aaaa->addr, sizeof(sin6.sin6_addr));
					sin6.sin6_family = AF_INET6;
					sin6.sin6_port = ctx->port;
					if ((gai_err=getnameinfo((struct sockaddr *)&sin6, sizeof(sin6), host, sizeof(host), NULL, 0, NI_NUMERICHOST)) != 0){
						belle_sip_error("resolver_process_data(): getnameinfo() failed for ipv6: %s",gai_strerror(gai_err));
						continue;
					}
					ctx->ai_list = ai_list_append(ctx->ai_list, belle_sip_ip_address_to_addrinfo(ctx->family, host, ctx->port));
					belle_sip_message("%s resolved to %s", ctx->name, host);
				} else if ((ctx->type == DNS_T_A) && (rr.class == DNS_C_IN) && (rr.type == DNS_T_A)) {
					struct dns_a *a = &any.a;
					struct sockaddr_in sin;
					memset(&sin, 0, sizeof(sin));
					memcpy(&sin.sin_addr, &a->addr, sizeof(sin.sin_addr));
					sin.sin_family = AF_INET;
					sin.sin_port = ctx->port;
					if ((gai_err=getnameinfo((struct sockaddr *)&sin, sizeof(sin), host, sizeof(host), NULL, 0, NI_NUMERICHOST)) != 0){
						belle_sip_error("resolver_process_data(): getnameinfo() failed: %s",gai_strerror(gai_err));
						continue;
					}
					ctx->ai_list = ai_list_append(ctx->ai_list, belle_sip_ip_address_to_addrinfo(ctx->family, host, ctx->port));
					belle_sip_message("%s resolved to %s", ctx->name, host);
				} else if ((ctx->type == DNS_T_SRV) && (rr.class == DNS_C_IN) && (rr.type == DNS_T_SRV)) {
					struct dns_srv *srv = &any.srv;
					struct dns_srv *res = belle_sip_malloc(sizeof(struct dns_srv));
					memcpy(res, srv, sizeof(struct dns_srv));
					snprintf(host, sizeof(host), "[target:%s port:%d prio:%d weight:%d]", srv->target, srv->port, srv->priority, srv->weight);
					ctx->srv_list = belle_sip_list_insert_sorted(ctx->srv_list, res, srv_compare_prio);
					belle_sip_message("SRV %s resolved to %s", ctx->name, host);
				}
			}
		}
		free(ans);
		ctx->done=TRUE;
		if ((ctx->type == DNS_T_A) || (ctx->type == DNS_T_AAAA)) {
			ctx->cb(ctx->cb_data, ctx->name, ctx->ai_list);
		} else if (ctx->type == DNS_T_SRV) {
			ctx->srv_cb(ctx->cb_data, ctx->name, ctx->srv_list);
		}
		if (ctx->done == TRUE) {
			return BELLE_SIP_STOP;
		} else {
			return BELLE_SIP_CONTINUE;
		}
	}
	if (error != DNS_EAGAIN) {
		belle_sip_error("%s dns_res_check error: %s (%d)", __FUNCTION__, dns_strerror(error), error);
		ctx->done=TRUE;
		if ((ctx->type == DNS_T_A) || (ctx->type == DNS_T_AAAA)) {
			ctx->cb(ctx->cb_data, ctx->name, NULL);
		} else if (ctx->type == DNS_T_SRV) {
			ctx->srv_cb(ctx->cb_data, ctx->name, NULL);
		}
		return BELLE_SIP_STOP;
	}

	dns_res_poll(ctx->R, 0);
	return BELLE_SIP_CONTINUE;
}

static int _resolver_send_query(belle_sip_resolver_context_t *ctx, belle_sip_source_func_t datafunc, int timeout) {
	int error;

	if (!ctx->stack->resolver_send_error) {
		error = dns_res_submit(ctx->R, ctx->name, ctx->type, DNS_C_IN);
		if (error)
			belle_sip_error("%s dns_res_submit error [%s]: %s", __FUNCTION__, ctx->name, dns_strerror(error));
	} else {
		/* Error simulation */
		error = ctx->stack->resolver_send_error;
		belle_sip_error("%s dns_res_submit error [%s]: simulated error %d", __FUNCTION__, ctx->name, error);
	}
	if (error < 0) {
		return -1;
	}

	if ((*datafunc)(ctx, 0) == BELLE_SIP_CONTINUE) {
		/*only init source if res inprogress*/
		belle_sip_socket_source_init((belle_sip_source_t*)ctx, datafunc, ctx, dns_res_pollfd(ctx->R), BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_TIMEOUT, timeout);
	}
	return 0;
}

typedef struct delayed_send {
	belle_sip_resolver_context_t *ctx;
	belle_sip_source_func_t datafunc;
	int timeout;
} delayed_send_t;

static int on_delayed_send_do(delayed_send_t *ds) {
	belle_sip_message("%s sending now", __FUNCTION__);
	_resolver_send_query(ds->ctx, ds->datafunc, ds->timeout);
	belle_sip_object_unref(ds->ctx);
	belle_sip_free(ds);
	return FALSE;
}

static int resolver_send_query(belle_sip_resolver_context_t *ctx, belle_sip_source_func_t datafunc, int timeout) {
	struct dns_hints *(*hints)() = &dns_hints_local;
	struct dns_options *opts;
#ifndef HAVE_C99
	struct dns_options opts_st;
#endif
	int error;

	if (!ctx->name) return -1;

	if (resconf(ctx))
		resconf(ctx)->options.recurse = 0;
	else
		return -1;
	if (!hosts(ctx))
		return -1;

	memset(&opts_st, 0, sizeof opts_st);
	opts = &opts_st;

	if (!(ctx->R = dns_res_open(ctx->resconf, ctx->hosts, dns_hints_mortal(hints(ctx->resconf, &error)), cache(ctx), opts, &error))) {
		belle_sip_error("%s dns_res_open error [%s]: %s", __FUNCTION__, ctx->name, dns_strerror(error));
		return -1;
	}

	if (ctx->stack->resolver_tx_delay > 0) {
		delayed_send_t *ds = belle_sip_new(delayed_send_t);
		ds->ctx = (belle_sip_resolver_context_t *)belle_sip_object_ref(ctx);
		ds->datafunc = datafunc;
		ds->timeout = timeout;
		belle_sip_main_loop_add_timeout(ctx->stack->ml, (belle_sip_source_func_t)on_delayed_send_do, ds, ctx->stack->resolver_tx_delay);
		belle_sip_socket_source_init((belle_sip_source_t*)ctx, datafunc, ctx, dns_res_pollfd(ctx->R), BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_TIMEOUT, ctx->stack->resolver_tx_delay + 1000);
		belle_sip_message("%s DNS resolution delayed by %d ms", __FUNCTION__, ctx->stack->resolver_tx_delay);
		return 0;
	} else {
		return _resolver_send_query(ctx, datafunc, timeout);
	}
}

static int resolver_start_query(belle_sip_resolver_context_t *ctx) {
	if (resolver_send_query(ctx,
			(belle_sip_source_func_t)resolver_process_data,
			belle_sip_stack_get_dns_timeout(ctx->stack)) < 0) {
		belle_sip_object_unref(ctx);
		return 0;
	}
	if ((ctx->done == FALSE) && (ctx->started == FALSE)) {
		/* The resolver context must never be removed manually from the main loop */
		belle_sip_main_loop_add_source(ctx->ml, (belle_sip_source_t *)ctx);
		belle_sip_object_unref(ctx);	/* The main loop has a ref on it */
		ctx->started = TRUE;
		return ctx->source.id;
	} else {
		return 0; /*resolution done synchronously*/
	}
}



int belle_sip_addrinfo_to_ip(const struct addrinfo *ai, char *ip, size_t ip_size, int *port){
	char serv[16];
	int err=getnameinfo(ai->ai_addr,ai->ai_addrlen,ip,ip_size,serv,sizeof(serv),NI_NUMERICHOST|NI_NUMERICSERV);
	if (err!=0){
		belle_sip_error("getnameinfo() error: %s",gai_strerror(err));
		strncpy(ip,"<bug!!>",ip_size);
	}
	if (port) *port=atoi(serv);
	return 0;
}

struct addrinfo * belle_sip_ip_address_to_addrinfo(int family, const char *ipaddress, int port){
	struct addrinfo *res=NULL;
	struct addrinfo hints={0};
	char serv[10];
	int err;

	snprintf(serv,sizeof(serv),"%i",port);
	hints.ai_family=family;
	hints.ai_flags=AI_NUMERICSERV|AI_NUMERICHOST;
	hints.ai_socktype=SOCK_STREAM; //not used but it's needed to specify it because otherwise getaddrinfo returns one struct addrinfo per socktype.
	
	if (family==AF_INET6) hints.ai_flags|=AI_V4MAPPED;
	
	err=getaddrinfo(ipaddress,serv,&hints,&res);
	if (err!=0){
		return NULL;
	}
	return res;
}


static void belle_sip_resolver_context_destroy(belle_sip_resolver_context_t *ctx){
	/* Do not free elements of ctx->ai_list with freeaddrinfo(). Let the caller do it, otherwise
	   it will not be able to use them after the resolver has been destroyed. */
	if (ctx->name != NULL) {
		belle_sip_free(ctx->name);
		ctx->name = NULL;
	}
	if (ctx->R != NULL) {
		dns_res_close(ctx->R);
		ctx->R = NULL;
	}
	if (ctx->hosts != NULL) {
		dns_hosts_close(ctx->hosts);
		ctx->hosts = NULL;
	}
	if (ctx->resconf != NULL) {
		free(ctx->resconf);
		ctx->resconf = NULL;
	}
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_resolver_context_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_resolver_context_t, belle_sip_source_t,belle_sip_resolver_context_destroy, NULL, NULL,FALSE);

struct belle_sip_recursive_resolve_data {
	belle_sip_resolver_context_t *ctx;
	belle_sip_resolver_callback_t cb;
	void *data;
	char *name;
	int port;
	int family;
	belle_sip_list_t *srv_list;
	int next_srv;
	struct addrinfo *ai_list;
};

static char * srv_prefix_from_transport(const char *transport) {
	char *prefix = "";

	if (strcasecmp(transport, "udp") == 0) {
		prefix = "_sip._udp.";
	} else if (strcasecmp(transport, "tcp") == 0) {
		prefix = "_sip._tcp.";
	} else if (strcasecmp(transport, "tls") == 0) {
		prefix = "_sips._tcp.";
	} else {
		prefix = "_sip._udp.";
	}

	return prefix;
}

static void process_a_results(void *data, const char *name, struct addrinfo *ai_list);

static int start_a_query_from_srv_results(struct belle_sip_recursive_resolve_data *rec_data) {
	if (rec_data->srv_list && belle_sip_list_size(rec_data->srv_list) > rec_data->next_srv) {
		struct dns_srv *srv;
		rec_data->next_srv++;
		srv = belle_sip_list_nth_data(rec_data->srv_list, 0);
		rec_data->ctx->cb = process_a_results;
		rec_data->ctx->name = srv->target;
		rec_data->ctx->port = srv->port;
		rec_data->ctx->family = rec_data->family;
		rec_data->ctx->type = (rec_data->family == AF_INET6) ? DNS_T_AAAA : DNS_T_A;
		resolver_start_query(rec_data->ctx);
		return 1;
	}

	return 0;
}

static void process_a_results(void *data, const char *name, struct addrinfo *ai_list) {
	struct belle_sip_recursive_resolve_data *rec_data = (struct belle_sip_recursive_resolve_data *)data;
	rec_data->ai_list = ai_list_append(rec_data->ai_list, ai_list);
	if (!start_a_query_from_srv_results(rec_data)) {
		/* All the SRV results have been queried, return the results */
		(*rec_data->cb)(rec_data->data, rec_data->name, rec_data->ai_list);
		rec_data->ctx->name = NULL;
		if (rec_data->srv_list != NULL) {
			belle_sip_list_for_each(rec_data->srv_list, belle_sip_free);
			belle_sip_list_free(rec_data->srv_list);
			rec_data->srv_list = NULL;
		}
		if (rec_data->name != NULL) {
			belle_sip_free(rec_data->name);
		}
		belle_sip_free(rec_data);
	} else {
		rec_data->ctx->done = FALSE;
	}
}

static void process_srv_results(void *data, const char *name, belle_sip_list_t *srv_list) {
	struct belle_sip_recursive_resolve_data *rec_data = (struct belle_sip_recursive_resolve_data *)data;
	rec_data->srv_list = srv_list;
	rec_data->ctx->done = FALSE;
	belle_sip_resolver_context_destroy(rec_data->ctx);
	if (!start_a_query_from_srv_results(rec_data)) {
		/* There was no SRV results, try to perform the A or AAAA directly. */
		rec_data->ctx->cb = process_a_results;
		rec_data->ctx->name = rec_data->name;
		rec_data->ctx->port = rec_data->port;
		rec_data->ctx->family = rec_data->family;
		rec_data->ctx->type = (rec_data->family == AF_INET6) ? DNS_T_AAAA : DNS_T_A;
		resolver_start_query(rec_data->ctx);
	}
}

unsigned long belle_sip_stack_resolve(belle_sip_stack_t *stack, const char *transport, const char *name, int port, int family, belle_sip_resolver_callback_t cb, void *data) {
	struct addrinfo *res = belle_sip_ip_address_to_addrinfo(family, name, port);
	if (res == NULL) {
		/* Then perform asynchronous DNS SRV query */
		struct belle_sip_recursive_resolve_data *rec_data = belle_sip_malloc0(sizeof(struct belle_sip_recursive_resolve_data));
		belle_sip_resolver_context_t *ctx = belle_sip_object_new(belle_sip_resolver_context_t);
		ctx->stack = stack;
		ctx->ml = stack->ml;
		ctx->cb_data = rec_data;
		ctx->srv_cb = process_srv_results;
		ctx->name = belle_sip_concat(srv_prefix_from_transport(transport), name, NULL);
		ctx->type = DNS_T_SRV;
		rec_data->ctx = ctx;
		rec_data->cb = cb;
		rec_data->data = data;
		rec_data->name = belle_sip_strdup(name);
		rec_data->port = port;
		if (family == 0) family = AF_UNSPEC;
		rec_data->family = family;
		return resolver_start_query(ctx);
	} else {
		/* There is no resolve to be done */
		cb(data, name, res);
		return 0;
	}
}

unsigned long belle_sip_stack_resolve_a(belle_sip_stack_t *stack, const char *name, int port, int family, belle_sip_resolver_callback_t cb , void *data) {
	struct addrinfo *res = belle_sip_ip_address_to_addrinfo(family, name, port);
	if (res == NULL) {
		/* Then perform asynchronous DNS A or AAAA query */
		belle_sip_resolver_context_t *ctx = belle_sip_object_new(belle_sip_resolver_context_t);
		ctx->stack = stack;
		ctx->ml = stack->ml;
		ctx->cb_data = data;
		ctx->cb = cb;
		ctx->name = belle_sip_strdup(name);
		ctx->port = port;
		if (family == 0) family = AF_UNSPEC;
		ctx->family = family;
		ctx->type = (ctx->family == AF_INET6) ? DNS_T_AAAA : DNS_T_A;
		return resolver_start_query(ctx);
	} else {
		/* There is no resolve to be done */
		cb(data, name, res);
		return 0;
	}
}

unsigned long belle_sip_stack_resolve_srv(belle_sip_stack_t *stack, const char *name, const char *transport, belle_sip_resolver_srv_callback_t cb, void *data) {
	belle_sip_resolver_context_t *ctx = belle_sip_object_new(belle_sip_resolver_context_t);
	ctx->stack = stack;
	ctx->ml = stack->ml;
	ctx->cb_data = data;
	ctx->srv_cb = cb;
	ctx->name = belle_sip_concat(srv_prefix_from_transport(transport), name, NULL);
	ctx->type = DNS_T_SRV;
	return resolver_start_query(ctx);
}

void belle_sip_stack_resolve_cancel(belle_sip_stack_t *stack, unsigned long id){
	if (id!=0){
		belle_sip_source_t *s=belle_sip_main_loop_find_source(stack->ml,id);
		if (s){
			belle_sip_resolver_context_t *res=BELLE_SIP_RESOLVER_CONTEXT(s);
			res->cancelled=1;
		}
	}
}


void belle_sip_get_src_addr_for(const struct sockaddr *dest, socklen_t destlen, struct sockaddr *src, socklen_t *srclen, int local_port){
	int af_type=dest->sa_family;
	int sock=socket(af_type,SOCK_DGRAM,IPPROTO_UDP);
	
	if (sock==(belle_sip_socket_t)-1){
		belle_sip_fatal("Could not create socket: %s",belle_sip_get_socket_error_string());
		goto fail;
	}
	if (connect(sock,dest,destlen)==-1){
		belle_sip_error("belle_sip_get_src_addr_for: connect() failed: %s",belle_sip_get_socket_error_string());
		goto fail;
	}
	if (getsockname(sock,src,srclen)==-1){
		belle_sip_error("belle_sip_get_src_addr_for: getsockname() failed: %s",belle_sip_get_socket_error_string());
		goto fail;
	}
	
	if (af_type==AF_INET6){
		struct sockaddr_in6 *sin6=(struct sockaddr_in6*)src;
		sin6->sin6_port=htons(local_port);
	}else{
		struct sockaddr_in *sin=(struct sockaddr_in*)src;
		sin->sin_port=htons(local_port);
	}
	
	close_socket(sock);
	return;
fail:
	{
		struct addrinfo hints={0},*res=NULL;
		int err;
		hints.ai_family=af_type;
		err=getaddrinfo(af_type==AF_INET ? "0.0.0.0" : "::0","0",&hints,&res);
		if (err!=0) belle_sip_fatal("belle_sip_get_src_addr_for(): getaddrinfo failed: %s",belle_sip_get_socket_error_string_from_code(err));
		memcpy(src,res->ai_addr,MIN((size_t)*srclen,res->ai_addrlen));
		*srclen=res->ai_addrlen;
		freeaddrinfo(res);
	}
	if (sock!=(belle_sip_socket_t)-1) close_socket(sock);
}

#ifndef IN6_GET_ADDR_V4MAPPED
#define IN6_GET_ADDR_V4MAPPED(sin6_addr)	*(unsigned int*)((unsigned char*)(sin6_addr)+12)
#endif


void belle_sip_address_remove_v4_mapping(const struct sockaddr *v6, struct sockaddr *result, socklen_t *result_len){
	if (v6->sa_family==AF_INET6){
		struct sockaddr_in6 *in6=(struct sockaddr_in6*)v6;
		
		if (IN6_IS_ADDR_V4MAPPED(&in6->sin6_addr)){
			struct sockaddr_in *in=(struct sockaddr_in*)result;
			result->sa_family=AF_INET;
			in->sin_addr.s_addr = IN6_GET_ADDR_V4MAPPED(&in6->sin6_addr);
			in->sin_port=in6->sin6_port;
			*result_len=sizeof(struct sockaddr_in);
		}else{
			if (v6!=result) memcpy(result,v6,sizeof(struct sockaddr_in6));
			*result_len=sizeof(struct sockaddr_in6);
		}
		
	}else{
		*result_len=sizeof(struct sockaddr_in);
		if (v6!=result) memcpy(result,v6,sizeof(struct sockaddr_in));
	}
}



