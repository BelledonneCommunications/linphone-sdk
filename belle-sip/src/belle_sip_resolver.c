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
#include "dns.h"

#include <stdlib.h>
#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#define DNS_EAGAIN  EAGAIN




typedef struct belle_sip_simple_resolver_context belle_sip_simple_resolver_context_t;
#define BELLE_SIP_SIMPLE_RESOLVER_CONTEXT(obj) BELLE_SIP_CAST(obj,belle_sip_simple_resolver_context_t)

typedef struct belle_sip_combined_resolver_context belle_sip_combined_resolver_context_t;
#define BELLE_SIP_COMBINED_RESOLVER_CONTEXT(obj) BELLE_SIP_CAST(obj,belle_sip_combined_resolver_context_t)

typedef struct belle_sip_dual_resolver_context belle_sip_dual_resolver_context_t;
#define BELLE_SIP_DUAL_RESOLVER_CONTEXT(obj) BELLE_SIP_CAST(obj,belle_sip_dual_resolver_context_t)


struct belle_sip_dns_srv{
	belle_sip_object_t base;
	unsigned short priority;
	unsigned short weight;
	unsigned short port;
	unsigned char a_done;
	unsigned char pad;
	int cumulative_weight; /*used only temporarily*/
	char *target;
	belle_sip_combined_resolver_context_t *root_resolver;/* used internally to combine SRV and A queries*/
	belle_sip_resolver_context_t *a_resolver; /* used internally to combine SRV and A queries*/
	struct addrinfo *a_results; /* used internally to combine SRV and A queries*/
	
};

static void belle_sip_dns_srv_destroy(belle_sip_dns_srv_t *obj){
	if (obj->target) {
		belle_sip_free(obj->target);
		obj->target=NULL;
	}
	if (obj->a_resolver){
		belle_sip_resolver_context_cancel(obj->a_resolver);
		belle_sip_object_unref(obj->a_resolver);
		obj->a_resolver=NULL;
	}
	if (obj->a_results){
		bctbx_freeaddrinfo(obj->a_results);
		obj->a_results=NULL;
	}
}

belle_sip_dns_srv_t *belle_sip_dns_srv_create(struct dns_srv *srv){
	belle_sip_dns_srv_t *obj=belle_sip_object_new(belle_sip_dns_srv_t);
	obj->priority=srv->priority;
	obj->weight=srv->weight;
	obj->port=srv->port;
	obj->target=belle_sip_strdup(srv->target);
	return obj;
}

const char *belle_sip_dns_srv_get_target(const belle_sip_dns_srv_t *obj){
	return obj->target;
}

unsigned short belle_sip_dns_srv_get_priority(const belle_sip_dns_srv_t *obj){
	return obj->priority;
}

unsigned short belle_sip_dns_srv_get_weight(const belle_sip_dns_srv_t *obj){
	return obj->weight;
}

unsigned short belle_sip_dns_srv_get_port(const belle_sip_dns_srv_t *obj){
	return obj->port;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_dns_srv_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_dns_srv_t, belle_sip_object_t,belle_sip_dns_srv_destroy, NULL, NULL,TRUE);


struct belle_sip_resolver_context{
	belle_sip_source_t source;
	belle_sip_stack_t *stack;
	uint8_t notified;
	uint8_t cancelled;
	uint8_t pad[2];
};

struct belle_sip_simple_resolver_context{
	belle_sip_resolver_context_t base;
	belle_sip_resolver_callback_t cb;
	belle_sip_resolver_srv_callback_t srv_cb;
	void *cb_data;
	void *srv_cb_data;
	struct dns_resolv_conf *resconf;
	struct dns_hosts *hosts;
	struct dns_resolver *R;
	enum dns_type type;
	char *name;
	int port;
	struct addrinfo *ai_list;
	belle_sip_list_t *srv_list; /*list of belle_sip_dns_srv_t*/
	int family;
	int flags;
	uint64_t start_time;
#ifdef USE_GETADDRINFO_FALLBACK
	struct addrinfo *getaddrinfo_ai_list;
	belle_sip_source_t *getaddrinfo_source;
	belle_sip_thread_t getaddrinfo_thread;
	unsigned char getaddrinfo_done;
	unsigned char getaddrinfo_cancelled;
#ifdef _WIN32
	HANDLE ctlevent;
#else
	int ctlpipe[2];
#endif
#endif
};

struct belle_sip_combined_resolver_context{
	belle_sip_resolver_context_t base;
	belle_sip_resolver_callback_t cb;
	void *cb_data;
	char *name;
	int port;
	int family;
	struct addrinfo *final_results;
	belle_sip_list_t *srv_results;
	belle_sip_resolver_context_t *srv_ctx;
	belle_sip_resolver_context_t *a_fallback_ctx;
};

struct belle_sip_dual_resolver_context{
	belle_sip_resolver_context_t base;
	belle_sip_resolver_callback_t cb;
	void *cb_data;
	char *name;
	belle_sip_resolver_context_t *a_ctx;
	belle_sip_resolver_context_t *aaaa_ctx;
	struct addrinfo *a_results;
	struct addrinfo *aaaa_results;
	uint8_t a_notified;
	uint8_t aaaa_notified;
	uint8_t pad[2];
};

void belle_sip_resolver_context_init(belle_sip_resolver_context_t *obj, belle_sip_stack_t *stack){
	obj->stack=stack;
	belle_sip_init_sockets(); /* Need to be called for DNS resolution to work on Windows platform. */
}

static int dns_resconf_nameservers_from_list(struct dns_resolv_conf *resconf, const belle_sip_list_t *l) {
	int max_servers = sizeof(resconf->nameserver)/sizeof(struct sockaddr_storage);
	int i;
	const belle_sip_list_t *elem;

	for (i = 0, elem = l; i < max_servers && elem != NULL; elem = elem->next) {
		int error = dns_resconf_pton(&resconf->nameserver[i], (const char *) elem->data);
		if (error == 0) ++i;
	}

	return i > 0 ? 0 : -1;
}

static struct dns_resolv_conf *resconf(belle_sip_simple_resolver_context_t *ctx) {
	const char *path;
	const belle_sip_list_t *servers;
	int error;

	if (ctx->resconf)
		return ctx->resconf;

	if (!(ctx->resconf = dns_resconf_open(&error))) {
		belle_sip_error("%s dns_resconf_open error: %s", __FUNCTION__, dns_strerror(error));
		return NULL;
	}
	
	path = belle_sip_stack_get_dns_resolv_conf_file(ctx->base.stack);
	servers = ctx->base.stack->dns_servers;
	
	if (servers){
		belle_sip_message("%s using application supplied dns server list.", __FUNCTION__);
		error = dns_resconf_nameservers_from_list(ctx->resconf, servers);
	}else if (!path){
#if defined(USE_FIXED_NAMESERVERS)
		error = dns_resconf_load_fixed_nameservers(ctx->resconf);
		if (error) {
			belle_sip_error("%s dns_resconf_load_fixed_nameservers error", __FUNCTION__);
		}
#elif defined(USE_STRUCT_RES_STATE_NAMESERVERS)
		error = dns_resconf_load_struct_res_state_nameservers(ctx->resconf);
		if (error) {
			belle_sip_error("%s dns_resconf_load_struct_res_state_nameservers error", __FUNCTION__);
		}
#elif defined(_WIN32)
		error = dns_resconf_loadwin(ctx->resconf);
		if (error) {
			belle_sip_error("%s dns_resconf_loadwin error", __FUNCTION__);
		}
#elif defined(ANDROID)
		error = dns_resconf_loadandroid(ctx->resconf);
		if (error) {
			belle_sip_error("%s dns_resconf_loadandroid error", __FUNCTION__);
		}
#elif defined(HAVE_RESINIT)
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
	}else{
		error = dns_resconf_loadpath(ctx->resconf, path);
		if (error) {
			belle_sip_error("%s dns_resconf_loadpath() of custom file error [%s]: %s", __FUNCTION__, path, dns_strerror(error));
			return NULL;
		}
	}
	
	if (error==0){
		char ip[64];
		char serv[10];
		int using_ipv6=FALSE;
		size_t i;
		
		belle_sip_message("Resolver is using DNS server(s):");
		for(i=0;i<sizeof(ctx->resconf->nameserver)/sizeof(ctx->resconf->nameserver[0]);++i){
			struct sockaddr *ns_addr=(struct sockaddr*)&ctx->resconf->nameserver[i];
			if (ns_addr->sa_family==AF_UNSPEC) break;
			bctbx_getnameinfo(ns_addr,ns_addr->sa_family==AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr)
					,ip,sizeof(ip),serv,sizeof(serv),NI_NUMERICHOST|NI_NUMERICSERV);
			belle_sip_message("\t%s",ip);
			if (ns_addr->sa_family==AF_INET6) using_ipv6=TRUE;
		}
		ctx->resconf->iface.ss_family=using_ipv6 ? AF_INET6 : AF_INET;
		if (i==0) {
			belle_sip_error("- no DNS servers available - resolution aborted.");
			return NULL;
		}
	}else{
		belle_sip_error("Error loading dns server addresses.");
		return NULL;
	}

	return ctx->resconf;
}

static struct dns_hosts *hosts(belle_sip_simple_resolver_context_t *ctx) {
	int error;

	if (ctx->hosts)
		return ctx->hosts;

	if (!(ctx->hosts = dns_hosts_local(&error))) {
		belle_sip_warning("%s dns_hosts_local error: %s", __FUNCTION__, dns_strerror(error));
		/*in case of failure, create an empty host object to make further processing happy, knowing that we can live without /etc/hosts.*/
		ctx->hosts=dns_hosts_open(&error);
	}

	if (ctx->base.stack->dns_user_hosts_file) {
		error = dns_hosts_loadpath(ctx->hosts, ctx->base.stack->dns_user_hosts_file);
		if (error) {
			belle_sip_error("%s dns_hosts_loadfile(\"%s\"): %s", __FUNCTION__,ctx->base.stack->dns_user_hosts_file,dns_strerror(error));
		}
	}

	return ctx->hosts;
}

struct dns_cache *cache(belle_sip_simple_resolver_context_t *ctx) {
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
	belle_sip_dns_srv_t *srv1=(belle_sip_dns_srv_t*)psrv1;
	belle_sip_dns_srv_t *srv2=(belle_sip_dns_srv_t*)psrv2;
	if (srv1->priority < srv2->priority) return -1;
	if (srv1->priority == srv2->priority) return 0;
	return 1;
}

/*
 * see https://www.ietf.org/rfc/rfc2782.txt
 * 0 weighted must just appear first.
**/
static int srv_sort_weight(const void *psrv1, const void *psrv2){
	belle_sip_dns_srv_t *srv1=(belle_sip_dns_srv_t*)psrv1;
	if (srv1->weight==0) return -1;
	return 1;
}

static belle_sip_dns_srv_t *srv_elect_one(belle_sip_list_t *srv_list){
	int sum=0;
	belle_sip_list_t *elem;
	belle_sip_dns_srv_t *srv;
	int rand_number;
	
	for(elem=srv_list;elem!=NULL;elem=elem->next){
		srv=(belle_sip_dns_srv_t*)elem->data;
		sum+=srv->weight;
		srv->cumulative_weight=sum;
	}
	/*no weights given, return the first one*/
	if (sum==0) return (belle_sip_dns_srv_t*)srv_list->data;
	rand_number=belle_sip_random() % sum; /*random number choosen in the range of the sum of weights*/
	for(elem=srv_list;elem!=NULL;elem=elem->next){
		srv=(belle_sip_dns_srv_t*)elem->data;
		if (rand_number<=srv->cumulative_weight)
			return srv;
	}
	return (belle_sip_dns_srv_t*)srv_list->data;
}

/*
 * Order an SRV list with entries having the same priority according to their weight
 */
static belle_sip_list_t *srv_elect(belle_sip_list_t **srv_list) {
	belle_sip_list_t *result = NULL;

	while (*srv_list != NULL) {
		belle_sip_list_t *it;
		belle_sip_dns_srv_t *entry = srv_elect_one(*srv_list);
		result = belle_sip_list_append(result, belle_sip_object_ref(entry));
		it = belle_sip_list_find(*srv_list, entry);
		if (it) {
			*srv_list = belle_sip_list_remove_link(*srv_list, it);
			belle_sip_free(it);
		}
	}
	return result;
}

/*
 * this function will return a list of SRV, with only one SRV record per priority.
 */
static belle_sip_list_t *srv_select_by_weight(belle_sip_list_t *srv_list){
	belle_sip_list_t *same_prio=NULL;
	belle_sip_list_t *elem;
	belle_sip_dns_srv_t *prev_srv=NULL;
	belle_sip_list_t *result=NULL;

	for (elem=srv_list;elem!=NULL;elem=elem->next){
		belle_sip_dns_srv_t *srv=(belle_sip_dns_srv_t*)elem->data;
		if (prev_srv){
			if (prev_srv->priority==srv->priority){
				if (!same_prio){
					same_prio=belle_sip_list_append(same_prio,prev_srv);
				}
				same_prio=belle_sip_list_insert_sorted(same_prio,srv,srv_sort_weight);
			}else{
				if (same_prio){
					result=belle_sip_list_concat(result,srv_elect(&same_prio));
				}
			}
		}
		prev_srv=srv;
	}
	if (same_prio){
		result=belle_sip_list_concat(result,srv_elect(&same_prio));
	}
	if (result){
		belle_sip_list_free_with_data(srv_list,belle_sip_object_unref);
		return result;
	}
	return srv_list;/*no weight election was necessary, return original list*/
}

static void simple_resolver_context_notify(belle_sip_resolver_context_t *obj) {
	belle_sip_simple_resolver_context_t *ctx = BELLE_SIP_SIMPLE_RESOLVER_CONTEXT(obj);
	if ((ctx->type == DNS_T_A) || (ctx->type == DNS_T_AAAA)) {
		struct addrinfo **ai_list = &ctx->ai_list;
#if USE_GETADDRINFO_FALLBACK
		if (ctx->getaddrinfo_ai_list != NULL) ai_list = &ctx->getaddrinfo_ai_list;
#endif
		ctx->cb(ctx->cb_data, ctx->name, *ai_list);
		*ai_list = NULL;
	} else if (ctx->type == DNS_T_SRV) {
		ctx->srv_list = srv_select_by_weight(ctx->srv_list);
		ctx->srv_cb(ctx->srv_cb_data, ctx->name, ctx->srv_list);
	}
}

static void dual_resolver_context_notify(belle_sip_resolver_context_t *obj) {
	belle_sip_dual_resolver_context_t *ctx = BELLE_SIP_DUAL_RESOLVER_CONTEXT(obj);
	struct addrinfo *results = ctx->aaaa_results;
	results = ai_list_append(results, ctx->a_results);
	ctx->a_results = NULL;
	ctx->aaaa_results = NULL;
	ctx->cb(ctx->cb_data, ctx->name, results);
}

static void combined_resolver_context_cleanup(belle_sip_combined_resolver_context_t *ctx) {
	if (ctx->srv_ctx) {
		belle_sip_object_unref(ctx->srv_ctx);
		ctx->srv_ctx = NULL;
	}
	if (ctx->a_fallback_ctx) {
		belle_sip_object_unref(ctx->a_fallback_ctx);
		ctx->a_fallback_ctx = NULL;
	}
	belle_sip_list_free_with_data(ctx->srv_results, belle_sip_object_unref);
	ctx->srv_results = NULL;
}

static void combined_resolver_context_notify(belle_sip_resolver_context_t *obj) {
	belle_sip_combined_resolver_context_t *ctx = BELLE_SIP_COMBINED_RESOLVER_CONTEXT(obj);
	ctx->cb(ctx->cb_data, ctx->name, ctx->final_results);
	ctx->final_results = NULL;
	combined_resolver_context_cleanup(ctx);
}

static void append_dns_result(belle_sip_simple_resolver_context_t *ctx, struct addrinfo **ai_list, struct sockaddr *addr, socklen_t addrlen){
	char host[NI_MAXHOST + 1];
	int gai_err;
	int family=ctx->family;
	
	if ((gai_err=bctbx_getnameinfo(addr, addrlen, host, sizeof(host), NULL, 0, NI_NUMERICHOST)) != 0){
		belle_sip_error("append_dns_result(): getnameinfo() failed: %s",gai_strerror(gai_err));
		return;
	}
	if (ctx->flags & AI_V4MAPPED) family=AF_INET6;
	*ai_list = ai_list_append(*ai_list, bctbx_ip_address_to_addrinfo(family, SOCK_STREAM, host, ctx->port));
	belle_sip_message("%s resolved to %s", ctx->name, host);
}

static int resolver_process_data(belle_sip_simple_resolver_context_t *ctx, unsigned int revents) {
	struct dns_packet *ans;
	struct dns_rr_i *I;
	struct dns_rr_i dns_rr_it;
	int error;
	unsigned char simulated_timeout=0;
	int timeout=belle_sip_stack_get_dns_timeout(ctx->base.stack);
	unsigned char search_enabled = belle_sip_stack_dns_search_enabled(ctx->base.stack);

	/*Setting timeout to 0 can be used to simulate DNS timeout*/
	if ((revents!=0) && timeout==0){
		belle_sip_warning("Simulating DNS timeout");
		simulated_timeout=1;
	}
	
	if (simulated_timeout || ((revents & BELLE_SIP_EVENT_TIMEOUT) && ((int)(belle_sip_time_ms()-ctx->start_time)>=timeout))) {
		belle_sip_error("%s timed-out", __FUNCTION__);
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(ctx));
		return BELLE_SIP_STOP;
	}
	dns_res_enable_search(ctx->R, search_enabled);
	/*belle_sip_message("resolver_process_data(): revents=%i",revents);*/
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
					append_dns_result(ctx,&ctx->ai_list,(struct sockaddr*)&sin6,sizeof(sin6));
				} else if ((ctx->type == DNS_T_A) && (rr.class == DNS_C_IN) && (rr.type == DNS_T_A)) {
					struct dns_a *a = &any.a;
					struct sockaddr_in sin;
					memset(&sin, 0, sizeof(sin));
					memcpy(&sin.sin_addr, &a->addr, sizeof(sin.sin_addr));
					sin.sin_family = AF_INET;
					sin.sin_port = ctx->port;
					append_dns_result(ctx,&ctx->ai_list,(struct sockaddr*)&sin,sizeof(sin));
				} else if ((ctx->type == DNS_T_SRV) && (rr.class == DNS_C_IN) && (rr.type == DNS_T_SRV)) {
					char host[NI_MAXHOST + 1];
					struct dns_srv *srv = &any.srv;
					belle_sip_dns_srv_t * b_srv=belle_sip_dns_srv_create(srv);
					snprintf(host, sizeof(host), "[target:%s port:%d prio:%d weight:%d]", srv->target, srv->port, srv->priority, srv->weight);
					ctx->srv_list = belle_sip_list_insert_sorted(ctx->srv_list, belle_sip_object_ref(b_srv), srv_compare_prio);
					belle_sip_message("SRV %s resolved to %s", ctx->name, host);
				}
			}
		}
		free(ans);
#ifdef USE_GETADDRINFO_FALLBACK
		ctx->getaddrinfo_cancelled = TRUE;
#endif
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(ctx));
		return BELLE_SIP_STOP;
	}
	if (error != DNS_EAGAIN) {
		belle_sip_error("%s dns_res_check() error: %s (%d)", __FUNCTION__, dns_strerror(error), error);
#ifdef USE_GETADDRINFO_FALLBACK
		if (ctx->getaddrinfo_done) {
			return BELLE_SIP_STOP;
		} else {
			// Wait for the getaddrinfo result
			return BELLE_SIP_CONTINUE;
		}
#else
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(ctx));
		return BELLE_SIP_STOP;
#endif
	} else {
#ifdef USE_GETADDRINFO_FALLBACK
		if (ctx->getaddrinfo_done) {
			return BELLE_SIP_STOP;
		} else
#endif
		{
			belle_sip_message("%s dns_res_check() in progress", __FUNCTION__);
		}
	}
	return BELLE_SIP_CONTINUE;
}

#ifdef USE_GETADDRINFO_FALLBACK
static void * _resolver_getaddrinfo_thread(void *ptr) {
	belle_sip_simple_resolver_context_t *ctx = (belle_sip_simple_resolver_context_t *)ptr;
	struct addrinfo *res = NULL;
	struct addrinfo hints = { 0 };
	char serv[10];
	int err;

	belle_sip_message("Resolver getaddrinfo thread started.");
	snprintf(serv, sizeof(serv), "%i", ctx->port);
	hints.ai_family = ctx->family;
	hints.ai_flags = AI_NUMERICSERV;
	err = getaddrinfo(ctx->name, serv, &hints, &res);
	if (err != 0) {
		belle_sip_error("getaddrinfo DNS resolution of %s failed: %s", ctx->name, gai_strerror(err));
	} else if (!ctx->getaddrinfo_cancelled) {
		do {
			append_dns_result(ctx, &ctx->getaddrinfo_ai_list, res->ai_addr, (socklen_t)res->ai_addrlen);
			res = res->ai_next;
		} while (res != NULL);
	}
	if (res) freeaddrinfo(res);
	ctx->getaddrinfo_done = TRUE;
#ifdef _WIN32
	SetEvent(ctx->ctlevent);
#else
	if (write(ctx->ctlpipe[1], "q", 1) == -1) {
		belle_sip_error("_resolver_getaddrinfo_thread(): Fail to write on pipe.");
	}
#endif
	return NULL;
}

static int _resolver_getaddrinfo_callback(belle_sip_simple_resolver_context_t *ctx, unsigned int revents) {
	if (!ctx->getaddrinfo_cancelled) {
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(ctx));
	}
	belle_sip_object_unref(ctx);
	return BELLE_SIP_STOP;
}

static void _resolver_getaddrinfo_start(belle_sip_simple_resolver_context_t *ctx) {
	belle_sip_fd_t fd = (belle_sip_fd_t)-1;

	belle_sip_object_ref(ctx);
#ifdef _WIN32
	ctx->ctlevent = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
	fd = (HANDLE)ctx->ctlevent;
#else
	if (pipe(ctx->ctlpipe) == -1) {
		belle_sip_fatal("pipe() failed: %s", strerror(errno));
	}
	fd = ctx->ctlpipe[0];
#endif
	belle_sip_thread_create(&ctx->getaddrinfo_thread, NULL, _resolver_getaddrinfo_thread, ctx);
	ctx->getaddrinfo_source = belle_sip_fd_source_new((belle_sip_source_func_t)_resolver_getaddrinfo_callback, ctx, fd, BELLE_SIP_EVENT_READ, -1);
	belle_sip_main_loop_add_source(ctx->base.stack->ml, ctx->getaddrinfo_source);
}
#endif

static int _resolver_send_query(belle_sip_simple_resolver_context_t *ctx) {
	int error = 0;

	if (!ctx->base.stack->resolver_send_error) {
		error = dns_res_submit(ctx->R, ctx->name, ctx->type, DNS_C_IN);
		if (error)
			belle_sip_error("%s dns_res_submit error [%s]: %s", __FUNCTION__, ctx->name, dns_strerror(error));
	} else {
		/* Error simulation */
		error = ctx->base.stack->resolver_send_error;
		belle_sip_error("%s dns_res_submit error [%s]: simulated error %d", __FUNCTION__, ctx->name, error);
	}
	if (error < 0) {
		return -1;
	}

	if (resolver_process_data(ctx, 0) == BELLE_SIP_CONTINUE) {
		ctx->start_time=belle_sip_time_ms();
		belle_sip_message("DNS resolution awaiting response, queued to main loop");
		/*only init source if res inprogress*/
		/*the timeout set to the source is 1 s, this is to allow dns.c to send request retransmissions*/
		belle_sip_socket_source_init((belle_sip_source_t*)ctx, (belle_sip_source_func_t)resolver_process_data, ctx, dns_res_pollfd(ctx->R), BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_TIMEOUT, 1000);
#ifdef USE_GETADDRINFO_FALLBACK
		{
			int timeout = belle_sip_stack_get_dns_timeout(ctx->base.stack);
			if ((timeout != 0) && ((ctx->type == DNS_T_A) || (ctx->type == DNS_T_AAAA))) {
				_resolver_getaddrinfo_start(ctx);
			}
		}
#endif
	}
	return 0;
}

static int resolver_process_data_delayed(belle_sip_simple_resolver_context_t *ctx, unsigned int revents) {
	int err=_resolver_send_query(ctx);
	if (err==0) return BELLE_SIP_CONTINUE;
	
	return BELLE_SIP_STOP;
}

static int _resolver_start_query(belle_sip_simple_resolver_context_t *ctx) {
	struct dns_options opts;
	int error;
	struct dns_resolv_conf *conf;

	if (!ctx->name) return -1;

	conf=resconf(ctx);
	if (conf){
		conf->options.recurse = 0;
		conf->options.timeout=2;
		conf->options.attempts=5;
	}else
		return -1;
	if (!hosts(ctx))
		return -1;

	memset(&opts, 0, sizeof opts);

	if (!(ctx->R = dns_res_open(ctx->resconf, ctx->hosts, dns_hints_mortal(dns_hints_local(ctx->resconf, &error)), cache(ctx), &opts, &error))) {
		belle_sip_error("%s dns_res_open error [%s]: %s", __FUNCTION__, ctx->name, dns_strerror(error));
		return -1;
	}
	error=0;
	if (ctx->base.stack->resolver_tx_delay > 0) {
		belle_sip_socket_source_init((belle_sip_source_t*)ctx, (belle_sip_source_func_t)resolver_process_data_delayed, ctx, -1, BELLE_SIP_EVENT_TIMEOUT, ctx->base.stack->resolver_tx_delay + 1000);
		belle_sip_message("%s DNS resolution delayed by %d ms", __FUNCTION__, ctx->base.stack->resolver_tx_delay);
	} else {
		error=_resolver_send_query(ctx);
	}
	if (error==0 && !ctx->base.notified) belle_sip_main_loop_add_source(ctx->base.stack->ml,(belle_sip_source_t*)ctx);
	return error;
}

static belle_sip_simple_resolver_context_t * resolver_start_query(belle_sip_simple_resolver_context_t *ctx) {
	int error;

	/* Take a ref for this part of code because _resolver_start_query() can notify the results and free the ctx if this is not the case. */
	belle_sip_object_ref(ctx);
	error = _resolver_start_query(ctx);
	if (error == 0) {
		if (!ctx->base.notified) {
			/* The resolution could not be done synchronously, return the context */
			belle_sip_object_unref(ctx);
			return ctx;
		}
		/* Otherwise, resolution could be done synchronously */
	} else {
		/* An error occured. We must notify the app. */
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(ctx));
	}
	belle_sip_object_unref(ctx);
	return NULL;
}


static void belle_sip_combined_resolver_context_destroy(belle_sip_combined_resolver_context_t *obj){
	if (obj->name != NULL) {
		belle_sip_free(obj->name);
		obj->name = NULL;
	}
	if (obj->srv_ctx){
		belle_sip_object_unref(obj->srv_ctx);
		obj->srv_ctx=NULL;
	}
	if (obj->a_fallback_ctx){
		belle_sip_object_unref(obj->a_fallback_ctx);
		obj->a_fallback_ctx=NULL;
	}
}

static void belle_sip_simple_resolver_context_destroy(belle_sip_simple_resolver_context_t *ctx){
	/* Do not free elements of ctx->ai_list with bctbx_freeaddrinfo(). Let the caller do it, otherwise
	   it will not be able to use them after the resolver has been destroyed. */
#ifdef USE_GETADDRINFO_FALLBACK
	if (ctx->getaddrinfo_thread != 0) {
		belle_sip_thread_join(ctx->getaddrinfo_thread, NULL);
	}
	if (ctx->getaddrinfo_source) belle_sip_object_unref(ctx->getaddrinfo_source);
#ifdef _WIN32
	if (ctx->ctlevent != (belle_sip_fd_t)-1)
		CloseHandle(ctx->ctlevent);
#else
	close(ctx->ctlpipe[0]);
	close(ctx->ctlpipe[1]);
#endif

#endif
	if (ctx->ai_list != NULL) {
		bctbx_freeaddrinfo(ctx->ai_list);
		ctx->ai_list = NULL;
	}
#ifdef USE_GETADDRINFO_FALLBACK
	if (ctx->getaddrinfo_ai_list != NULL) {
		bctbx_freeaddrinfo(ctx->getaddrinfo_ai_list);
		ctx->getaddrinfo_ai_list = NULL;
	}
#endif
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

static void belle_sip_dual_resolver_context_destroy(belle_sip_dual_resolver_context_t *obj){
	if (obj->a_ctx){
		belle_sip_object_unref(obj->a_ctx);
		obj->a_ctx=NULL;
	}
	if (obj->aaaa_ctx){
		belle_sip_object_unref(obj->aaaa_ctx);
		obj->aaaa_ctx=NULL;
	}
	if (obj->a_results){
		bctbx_freeaddrinfo(obj->a_results);
		obj->a_results=NULL;
	}
	if (obj->aaaa_results){
		bctbx_freeaddrinfo(obj->aaaa_results);
		obj->aaaa_results=NULL;
	}
	if (obj->name){
		belle_sip_free(obj->name);
		obj->name=NULL;
	}
}

static void simple_resolver_context_cancel(belle_sip_resolver_context_t *obj) {
	belle_sip_main_loop_remove_source(obj->stack->ml, (belle_sip_source_t *)obj);
}

static void combined_resolver_context_cancel(belle_sip_resolver_context_t *obj) {
	belle_sip_combined_resolver_context_t *ctx = BELLE_SIP_COMBINED_RESOLVER_CONTEXT(obj);
	if (ctx->srv_ctx) {
		belle_sip_resolver_context_cancel(ctx->srv_ctx);
		belle_sip_object_unref(ctx->srv_ctx);
		ctx->srv_ctx = NULL;
	}
	if (ctx->a_fallback_ctx) {
		belle_sip_resolver_context_cancel(ctx->a_fallback_ctx);
		belle_sip_object_unref(ctx->a_fallback_ctx);
		ctx->a_fallback_ctx = NULL;
	}
	combined_resolver_context_cleanup(ctx);
}

static void dual_resolver_context_cancel(belle_sip_resolver_context_t *obj) {
	belle_sip_dual_resolver_context_t *ctx= BELLE_SIP_DUAL_RESOLVER_CONTEXT(obj);
	if (ctx->a_ctx) {
		belle_sip_resolver_context_cancel(ctx->a_ctx);
		belle_sip_object_unref(ctx->a_ctx);
		ctx->a_ctx = NULL;
	}
	if (ctx->aaaa_ctx) {
		belle_sip_resolver_context_cancel(ctx->aaaa_ctx);
		belle_sip_object_unref(ctx->aaaa_ctx);
		ctx->aaaa_ctx = NULL;
	}
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_resolver_context_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_resolver_context_t)
	{
		BELLE_SIP_VPTR_INIT(belle_sip_resolver_context_t,belle_sip_source_t,TRUE),
		(belle_sip_object_destroy_t) NULL,
		NULL,
		NULL,
		BELLE_SIP_DEFAULT_BUFSIZE_HINT
	},
	NULL
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END


BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_simple_resolver_context_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_simple_resolver_context_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_simple_resolver_context_t,belle_sip_resolver_context_t,TRUE),
			(belle_sip_object_destroy_t) belle_sip_simple_resolver_context_destroy,
			NULL,
			NULL,
			BELLE_SIP_DEFAULT_BUFSIZE_HINT
		},
		simple_resolver_context_cancel,
		simple_resolver_context_notify
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_dual_resolver_context_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_dual_resolver_context_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_dual_resolver_context_t,belle_sip_resolver_context_t,TRUE),
			(belle_sip_object_destroy_t) belle_sip_dual_resolver_context_destroy,
			NULL,
			NULL,
			BELLE_SIP_DEFAULT_BUFSIZE_HINT
		},
		dual_resolver_context_cancel,
		dual_resolver_context_notify
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_combined_resolver_context_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_combined_resolver_context_t)
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_combined_resolver_context_t,belle_sip_resolver_context_t,TRUE),
			(belle_sip_object_destroy_t) belle_sip_combined_resolver_context_destroy,
			NULL,
			NULL,
			BELLE_SIP_DEFAULT_BUFSIZE_HINT
		},
		combined_resolver_context_cancel,
		combined_resolver_context_notify
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

static char * srv_prefix_from_service_and_transport(const char *service, const char *transport) {
	if (service == NULL) service = "sip";
	if (strcasecmp(transport, "udp") == 0) {
		return belle_sip_strdup_printf("_%s._udp.", service);
	} else if (strcasecmp(transport, "tcp") == 0) {
		return belle_sip_strdup_printf("_%s._tcp.", service);
	} else if (strcasecmp(transport, "tls") == 0) {
		return belle_sip_strdup_printf("_%ss._tcp.", service);
	}
	return belle_sip_strdup_printf("_%s._udp.", service);
}

static void process_a_fallback_result(void *data, const char *name, struct addrinfo *ai_list){
	belle_sip_combined_resolver_context_t *ctx=(belle_sip_combined_resolver_context_t *)data;
	ctx->final_results=ai_list;
	belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(ctx));
}

static void combined_resolver_context_check_finished(belle_sip_combined_resolver_context_t *obj){
	belle_sip_list_t *elem;
	struct addrinfo *final=NULL;
	unsigned char finished=TRUE;
	
	for(elem=obj->srv_results;elem!=NULL;elem=elem->next){
		belle_sip_dns_srv_t *srv=(belle_sip_dns_srv_t*)elem->data;
		if (!srv->a_done) {
			finished=FALSE;
			break;
		}
	}
	if (finished){
		belle_sip_message("All A/AAAA results for combined resolution have arrived.");
		for(elem=obj->srv_results;elem!=NULL;elem=elem->next){
			belle_sip_dns_srv_t *srv=(belle_sip_dns_srv_t*)elem->data;
			final=ai_list_append(final,srv->a_results);
			srv->a_results=NULL;
		}
		belle_sip_list_free_with_data(obj->srv_results,belle_sip_object_unref);
		obj->srv_results=NULL;
		obj->final_results=final;
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(obj));
	}
}

static void process_a_from_srv(void *data, const char *name, struct addrinfo *ai_list){
	belle_sip_dns_srv_t *srv=(belle_sip_dns_srv_t*)data;
	srv->a_results=ai_list;
	srv->a_done=TRUE;
	belle_sip_message("A query finished for srv result [%s]",srv->target);
	combined_resolver_context_check_finished(srv->root_resolver);
}

static void srv_resolve_a(belle_sip_combined_resolver_context_t *obj, belle_sip_dns_srv_t *srv){
	belle_sip_message("Starting A/AAAA query for srv result [%s]",srv->target);
	srv->root_resolver=obj;
	/* take a ref of the srv object because the A resolution may terminate synchronously and destroy the srv
	 object before to store the returned value of belle_sip_stack_resolve_a(). That would lead to an invalid write */
	belle_sip_object_ref(srv);
	srv->a_resolver=belle_sip_stack_resolve_a(obj->base.stack,srv->target,srv->port,obj->family,process_a_from_srv,srv);
	if (srv->a_resolver){
		belle_sip_object_ref(srv->a_resolver);
	}
	belle_sip_object_unref(srv);
}

static void process_srv_results(void *data, const char *name, belle_sip_list_t *srv_results){
	belle_sip_combined_resolver_context_t *ctx=(belle_sip_combined_resolver_context_t *)data;
	/*take a ref here, because the A resolution might succeed synchronously and terminate the context before exiting this function*/
	belle_sip_object_ref(ctx);
	if (srv_results){
		belle_sip_list_t *elem;
		/* take a ref of each srv_results because the last A resolution may terminate synchronously
		 and destroy the list before the loop terminate */
		ctx->srv_results = belle_sip_list_copy(srv_results);
		belle_sip_list_for_each(srv_results, (void(*)(void *))belle_sip_object_ref);
		for(elem=srv_results;elem!=NULL;elem=elem->next){
			belle_sip_dns_srv_t *srv=(belle_sip_dns_srv_t*)elem->data;
			srv_resolve_a(ctx,srv);
		}
		srv_results = belle_sip_list_free_with_data(srv_results, belle_sip_object_unref);
	}else{
		/*no SRV results, perform A query */
		belle_sip_message("No SRV result for [%s], trying A/AAAA.",name);
		ctx->a_fallback_ctx=belle_sip_stack_resolve_a(ctx->base.stack,ctx->name,ctx->port,ctx->family,process_a_fallback_result,ctx);
		if (ctx->a_fallback_ctx) belle_sip_object_ref(ctx->a_fallback_ctx);
	}
	belle_sip_object_unref(ctx);
}

/**
 * Perform combined SRV + A / AAAA resolution.
**/
belle_sip_resolver_context_t * belle_sip_stack_resolve(belle_sip_stack_t *stack, const char *service, const char *transport, const char *name, int port, int family, belle_sip_resolver_callback_t cb, void *data) {
	struct addrinfo *res = bctbx_ip_address_to_addrinfo(family, SOCK_STREAM, name, port);
	if (res == NULL) {
		/* First perform asynchronous DNS SRV query */
		belle_sip_combined_resolver_context_t *ctx = belle_sip_object_new(belle_sip_combined_resolver_context_t);
		belle_sip_resolver_context_init((belle_sip_resolver_context_t*)ctx,stack);
		belle_sip_object_ref(ctx);/*we don't want the object to be destroyed until the end of this function*/
		ctx->cb=cb;
		ctx->cb_data = data;
		ctx->name = belle_sip_strdup(name);
		ctx->port=port;
		belle_sip_object_set_name((belle_sip_object_t*)ctx, ctx->name);
		if (family == 0) family = AF_UNSPEC;
		ctx->family = family;
		/* Take a ref for the entire duration of the DNS procedure, it will be released when it is finished */
		belle_sip_object_ref(ctx);
		ctx->srv_ctx=belle_sip_stack_resolve_srv(stack,service,transport,name,process_srv_results,ctx);
		if (ctx->srv_ctx) belle_sip_object_ref(ctx->srv_ctx);
		if (ctx->base.notified) {
			belle_sip_object_unref(ctx);
			return NULL;
		}
		belle_sip_object_unref(ctx);
		return BELLE_SIP_RESOLVER_CONTEXT(ctx);
	} else {
		/* There is no resolve to be done */
		cb(data, name, res);
		return NULL;
	}
}

static belle_sip_resolver_context_t * belle_sip_stack_resolve_single(belle_sip_stack_t *stack, const char *name, int port, int family, int flags, belle_sip_resolver_callback_t cb , void *data){
	/* Then perform asynchronous DNS A or AAAA query */
	belle_sip_simple_resolver_context_t *ctx = belle_sip_object_new(belle_sip_simple_resolver_context_t);
	belle_sip_resolver_context_init((belle_sip_resolver_context_t*)ctx,stack);
	ctx->cb_data = data;
	ctx->cb = cb;
	ctx->name = belle_sip_strdup(name);
	ctx->port = port;
	ctx->flags = flags;
	belle_sip_object_set_name((belle_sip_object_t*)ctx, ctx->name);
	/* Take a ref for the entire duration of the DNS procedure, it will be released when it is finished */
	belle_sip_object_ref(ctx);
	if (family == 0) family = AF_UNSPEC;
	ctx->family = family;
	ctx->type = (ctx->family == AF_INET6) ? DNS_T_AAAA : DNS_T_A;
#if defined(USE_GETADDRINFO_FALLBACK) && defined(_WIN32)
	ctx->ctlevent = (belle_sip_fd_t)-1;
#endif
	return (belle_sip_resolver_context_t*)resolver_start_query(ctx);
}

static uint8_t belle_sip_resolver_context_can_be_cancelled(belle_sip_resolver_context_t *obj) {
	return ((obj->cancelled == TRUE) || (obj->notified == TRUE)) ? FALSE : TRUE;
}

#define belle_sip_resolver_context_can_be_notified(obj) belle_sip_resolver_context_can_be_cancelled(obj)

static void dual_resolver_context_check_finished(belle_sip_dual_resolver_context_t *ctx) {
	if (belle_sip_resolver_context_can_be_notified(BELLE_SIP_RESOLVER_CONTEXT(ctx)) && (ctx->a_notified == TRUE) && (ctx->aaaa_notified == TRUE)) {
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(ctx));
	}
}

static void on_ipv4_results(void *data, const char *name, struct addrinfo *ai_list) {
	belle_sip_dual_resolver_context_t *ctx = BELLE_SIP_DUAL_RESOLVER_CONTEXT(data);
	ctx->a_results = ai_list;
	ctx->a_notified = TRUE;
	dual_resolver_context_check_finished(ctx);
}

static void on_ipv6_results(void *data, const char *name, struct addrinfo *ai_list) {
	belle_sip_dual_resolver_context_t *ctx = BELLE_SIP_DUAL_RESOLVER_CONTEXT(data);
	ctx->aaaa_results = ai_list;
	ctx->aaaa_notified = TRUE;
	dual_resolver_context_check_finished(ctx);
}

static belle_sip_resolver_context_t * belle_sip_stack_resolve_dual(belle_sip_stack_t *stack, const char *name, int port, belle_sip_resolver_callback_t cb , void *data){
	/* Then perform asynchronous DNS A or AAAA query */
	belle_sip_dual_resolver_context_t *ctx = belle_sip_object_new(belle_sip_dual_resolver_context_t);
	belle_sip_resolver_context_init((belle_sip_resolver_context_t*)ctx,stack);
	belle_sip_object_ref(ctx);/*we don't want the object to be destroyed until the end of this function*/
	ctx->cb_data = data;
	ctx->cb = cb;
	ctx->name = belle_sip_strdup(name);
	belle_sip_object_set_name((belle_sip_object_t*)ctx, ctx->name);
	/* Take a ref for the entire duration of the DNS procedure, it will be released when it is finished */
	belle_sip_object_ref(ctx);
	ctx->a_ctx=belle_sip_stack_resolve_single(stack,name,port,AF_INET, AI_V4MAPPED, on_ipv4_results,ctx);
	if (ctx->a_ctx) belle_sip_object_ref(ctx->a_ctx);
	ctx->aaaa_ctx=belle_sip_stack_resolve_single(stack, name, port, AF_INET6, 0, on_ipv6_results, ctx);
	if (ctx->aaaa_ctx) belle_sip_object_ref(ctx->aaaa_ctx);
	if (ctx->base.notified){
		/* All results were found synchronously */
		belle_sip_object_unref(ctx);
		ctx = NULL;
	} else belle_sip_object_unref(ctx);
	return BELLE_SIP_RESOLVER_CONTEXT(ctx);
}

belle_sip_resolver_context_t * belle_sip_stack_resolve_a(belle_sip_stack_t *stack, const char *name, int port, int family, belle_sip_resolver_callback_t cb , void *data) {
	struct addrinfo *res = bctbx_ip_address_to_addrinfo(family, SOCK_STREAM, name, port);
	if (res == NULL) {
		switch(family){
			case AF_UNSPEC:
				family=AF_INET6;
			case AF_INET6:
				return belle_sip_stack_resolve_dual(stack,name,port,cb,data);
				break;
			case AF_INET:
				return belle_sip_stack_resolve_single(stack,name,port,AF_INET,0,cb,data);
				break;
			default:
				belle_sip_error("belle_sip_stack_resolve_a(): unsupported address family [%i]",family);
		}
	} else {
		/* There is no resolve to be done */
		cb(data, name, res);
	}
	return NULL;
}

belle_sip_resolver_context_t * belle_sip_stack_resolve_srv(belle_sip_stack_t *stack, const char *service, const char *transport, const char *name, belle_sip_resolver_srv_callback_t cb, void *data) {
	belle_sip_simple_resolver_context_t *ctx = belle_sip_object_new(belle_sip_simple_resolver_context_t);
	char *srv_prefix = srv_prefix_from_service_and_transport(service, transport);
	belle_sip_resolver_context_init((belle_sip_resolver_context_t*)ctx,stack);
	ctx->srv_cb_data = data;
	ctx->srv_cb = cb;
	ctx->name = belle_sip_concat(srv_prefix, name, NULL);
	ctx->type = DNS_T_SRV;
	belle_sip_object_set_name((belle_sip_object_t*)ctx, ctx->name);
	/* Take a ref for the entire duration of the DNS procedure, it will be released when it is finished */
	belle_sip_object_ref(ctx);
	belle_sip_free(srv_prefix);
	return (belle_sip_resolver_context_t*)resolver_start_query(ctx);
}

void belle_sip_resolver_context_cancel(belle_sip_resolver_context_t *obj) {
	if (belle_sip_resolver_context_can_be_cancelled(obj)) {
		obj->cancelled = TRUE;
		BELLE_SIP_OBJECT_VPTR(obj, belle_sip_resolver_context_t)->cancel(obj);
		belle_sip_object_unref(obj);
	}
}

void belle_sip_resolver_context_notify(belle_sip_resolver_context_t *obj) {
	if (belle_sip_resolver_context_can_be_notified(obj)) {
		obj->notified = TRUE;
		BELLE_SIP_OBJECT_VPTR(obj, belle_sip_resolver_context_t)->notify(obj);
		belle_sip_object_unref(obj);
	}
}

/*
This function does the connect() method to get local ip address suitable to reach a given destination.
It works on all platform except for windows using ipv6 sockets. TODO: find a workaround for win32+ipv6 socket
*/
int belle_sip_get_src_addr_for(const struct sockaddr *dest, socklen_t destlen, struct sockaddr *src, socklen_t *srclen, int local_port){
	int af_type=dest->sa_family;
	int sock=(int)socket(af_type,SOCK_DGRAM,IPPROTO_UDP);
	int ret = 0;
	
	if (sock==(belle_sip_socket_t)-1){
		if (af_type == AF_INET){
			belle_sip_fatal("Could not create socket: %s",belle_sip_get_socket_error_string());
		}
		goto fail;
	}
	
	if (af_type==AF_INET6 && (IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6*)dest)->sin6_addr))){
		/*this is actually required only for windows, who is unable to provide an ipv4 mapped local address if the remote is ipv4 mapped,
		and unable to provide a correct local address if the remote address is true ipv6 address when in dual stack mode*/
		belle_sip_socket_enable_dual_stack(sock);
	}
	
	if (connect(sock,dest,destlen)==-1){
		ret = -get_socket_error();
		belle_sip_error("belle_sip_get_src_addr_for: connect() failed: %s",belle_sip_get_socket_error_string_from_code(-ret));
		goto fail;
	}
	if (getsockname(sock,src,srclen)==-1){
		ret = -get_socket_error();
		belle_sip_error("belle_sip_get_src_addr_for: getsockname() failed: %s",belle_sip_get_socket_error_string_from_code(-ret));
		goto fail;
	}
	
	if (af_type==AF_INET6){
		struct sockaddr_in6 *sin6=(struct sockaddr_in6*)src;
		sin6->sin6_port=htons(local_port);
	}else{
		struct sockaddr_in *sin=(struct sockaddr_in*)src;
		sin->sin_port=htons(local_port);
	}
	
	belle_sip_close_socket(sock);
	return ret;
fail:
	{
		struct addrinfo *res = bctbx_ip_address_to_addrinfo(af_type, SOCK_STREAM, af_type == AF_INET ? "127.0.0.1" : "::1", local_port);
		if (res != NULL) {
			memcpy(src,res->ai_addr,MIN((size_t)*srclen,res->ai_addrlen));
			*srclen=(socklen_t)res->ai_addrlen;
			bctbx_freeaddrinfo(res);
		} else {
			if (af_type == AF_INET) belle_sip_fatal("belle_sip_get_src_addr_for(): belle_sip_ip_address_to_addrinfo() failed");
		}
	}
	if (sock!=(belle_sip_socket_t)-1) belle_sip_close_socket(sock);
	return ret;
}
