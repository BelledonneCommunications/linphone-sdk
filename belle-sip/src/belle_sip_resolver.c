/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "belle_sip_internal.h"
#include <bctoolbox/defs.h>

#ifdef HAVE_MDNS
#include <dns_sd.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#ifdef HAVE_DNS_SERVICE
/* When HAVE_DNS_SERVICE is on, the resolve is performed using Apple DNSService */
#include "TargetConditionals.h"
#include "bctoolbox/port.h" // mutex
#include "dns.h"
#include <dns_sd.h>
#include <dns_util.h>
#endif /* HAVE_DNS_SERVICE */
#include "dns/dns.h"

#define DNS_EAGAIN EAGAIN

/* Default value for a timeout to wait AAAA results after A result for the same host have been received.
 * This is to compensate buggy routers that don't really support IPv6 and never provide any answer to AAAA queries.
 */
static const int belle_sip_aaaa_timeout_after_a_received = 3000;

/* Default value for a timeout to wait for SRV results after a A/AAAA result for the same domain has been received.
 * This is to compensate buggy routers that don't really support SRV and never provide any answer to SRV queries.
 */
static const int belle_sip_srv_timeout_after_a_received = 3000;

typedef struct belle_sip_simple_resolver_context belle_sip_simple_resolver_context_t;
#define BELLE_SIP_SIMPLE_RESOLVER_CONTEXT(obj) BELLE_SIP_CAST(obj, belle_sip_simple_resolver_context_t)

typedef struct belle_sip_combined_resolver_context belle_sip_combined_resolver_context_t;
#define BELLE_SIP_COMBINED_RESOLVER_CONTEXT(obj) BELLE_SIP_CAST(obj, belle_sip_combined_resolver_context_t)

typedef struct belle_sip_dual_resolver_context belle_sip_dual_resolver_context_t;
#define BELLE_SIP_DUAL_RESOLVER_CONTEXT(obj) BELLE_SIP_CAST(obj, belle_sip_dual_resolver_context_t)

struct belle_sip_dns_srv {
	belle_sip_object_t base;
	unsigned short priority;
	unsigned short weight;
	unsigned short port;
	unsigned char a_done;
	unsigned char dont_free_a_results;
	int cumulative_weight; /*used only temporarily*/
	char *target;
	belle_sip_combined_resolver_context_t *root_resolver; /* used internally to combine SRV and A queries*/
	belle_sip_resolver_context_t *a_resolver;             /* used internally to combine SRV and A queries*/
	struct addrinfo *a_results;                           /* used internally to combine SRV and A queries*/
#ifdef HAVE_MDNS
	char *fullname; /* Used by mDNS */
#endif
};

static void belle_sip_dns_srv_destroy(belle_sip_dns_srv_t *obj) {
	if (obj->target) {
		belle_sip_free(obj->target);
		obj->target = NULL;
	}
	if (obj->a_resolver) {
		belle_sip_resolver_context_cancel(obj->a_resolver);
		belle_sip_object_unref(obj->a_resolver);
		obj->a_resolver = NULL;
	}
	if (obj->a_results && !obj->dont_free_a_results) {
		bctbx_freeaddrinfo(obj->a_results);
		obj->a_results = NULL;
	}
#ifdef HAVE_MDNS
	if (obj->fullname) {
		belle_sip_free(obj->fullname);
		obj->fullname = NULL;
	}
#endif
}

#ifdef HAVE_MDNS
belle_sip_dns_srv_t *belle_sip_mdns_srv_create(short unsigned int priority,
                                               short unsigned int weight,
                                               short unsigned int port,
                                               const char *target,
                                               const char *fullname) {
	belle_sip_dns_srv_t *obj = belle_sip_object_new(belle_sip_dns_srv_t);
	obj->priority = priority;
	obj->weight = weight;
	obj->port = port;
	obj->target = belle_sip_strdup(target);
	obj->fullname = belle_sip_strdup(fullname);
	return obj;
}
#endif

#ifdef HAVE_DNS_SERVICE
belle_sip_dns_srv_t *belle_sip_dns_srv_create_dns_service(dns_resource_record_t *rr) {
	belle_sip_dns_srv_t *obj = belle_sip_object_new(belle_sip_dns_srv_t);
	size_t end_pos;
	obj->priority = rr->data.SRV->priority;
	obj->weight = rr->data.SRV->weight;
	obj->port = rr->data.SRV->port;
	obj->target = belle_sip_strdup(rr->data.SRV->target);
	/*remove trailing '.' at the end*/
	end_pos = strlen(obj->target);
	if (end_pos > 0) {
		end_pos--;
		if (obj->target[end_pos] == '.') obj->target[end_pos] = '\0';
	}
	return obj;
}
#endif /* HAVE_DNS_SERVICE */

belle_sip_dns_srv_t *belle_sip_dns_srv_create(struct dns_srv *srv) {
	belle_sip_dns_srv_t *obj = belle_sip_object_new(belle_sip_dns_srv_t);
	size_t end_pos;
	obj->priority = srv->priority;
	obj->weight = srv->weight;
	obj->port = srv->port;
	obj->target = belle_sip_strdup(srv->target);
	/*remove trailing '.' at the end*/
	end_pos = strlen(obj->target);
	if (end_pos > 0) {
		end_pos--;
		if (obj->target[end_pos] == '.') obj->target[end_pos] = '\0';
	}
	return obj;
}

const char *belle_sip_dns_srv_get_target(const belle_sip_dns_srv_t *obj) {
	return obj->target;
}

unsigned short belle_sip_dns_srv_get_priority(const belle_sip_dns_srv_t *obj) {
	return obj->priority;
}

unsigned short belle_sip_dns_srv_get_weight(const belle_sip_dns_srv_t *obj) {
	return obj->weight;
}

unsigned short belle_sip_dns_srv_get_port(const belle_sip_dns_srv_t *obj) {
	return obj->port;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_dns_srv_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_dns_srv_t, belle_sip_object_t, belle_sip_dns_srv_destroy, NULL, NULL, TRUE);

struct belle_sip_resolver_results {
	belle_sip_object_t base;
	struct addrinfo *ai_list;
	bctbx_list_t *srv_list;
	char *name;
	uint32_t ttl;
};

static void belle_sip_resolver_results_destroy(belle_sip_resolver_results_t *obj) {
	if (obj->ai_list) bctbx_freeaddrinfo(obj->ai_list);
	bctbx_list_free_with_data(obj->srv_list, belle_sip_object_unref);
	if (obj->name) bctbx_free(obj->name);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_resolver_results_t);
BELLE_SIP_INSTANCIATE_VPTR(
    belle_sip_resolver_results_t, belle_sip_object_t, belle_sip_resolver_results_destroy, NULL, NULL, FALSE);

static int addrinfo_in_range(const struct addrinfo *ai, const struct addrinfo *begin, const struct addrinfo *end) {
	const struct addrinfo *it;
	for (it = begin; it != end; it = it->ai_next) {
		if (it == NULL) {
			belle_sip_error("addrinfo_in_range(): it == NULL, this should not happen, this is a bug !");
			break;
		}
		if (it == ai) {
			return 1;
		}
	}
	return 0;
}

static const belle_sip_dns_srv_t *_belle_sip_dns_srv_get_from_addrinfo(const belle_sip_list_t *srv_list,
                                                                       const struct addrinfo *ai) {
	/* We simply iterate on the addrinfo pointed by the srv objects in the srv list until we find "ai".
	 * This is not very efficient but there is not that much alternatives:
	 * - there is no user pointer in struct addrinfo
	 * - there might not be srv record for a domain, in which case the srv_list is empty...
	 */
	const bctbx_list_t *elem;

	for (elem = srv_list; elem != NULL; elem = elem->next) {
		const belle_sip_dns_srv_t *srv = (const belle_sip_dns_srv_t *)elem->data;
		const belle_sip_dns_srv_t *next_srv = elem->next ? (const belle_sip_dns_srv_t *)elem->next->data : NULL;

		if (addrinfo_in_range(ai, srv->a_results, next_srv ? next_srv->a_results : NULL)) {
			return srv;
		}
	}
	return NULL;
}

const belle_sip_dns_srv_t *belle_sip_resolver_results_get_srv_from_addrinfo(const belle_sip_resolver_results_t *obj,
                                                                            const struct addrinfo *ai) {
	return _belle_sip_dns_srv_get_from_addrinfo(obj->srv_list, ai);
}

const bctbx_list_t *belle_sip_resolver_results_get_srv_records(const belle_sip_resolver_results_t *obj) {
	return obj->srv_list;
}

const struct addrinfo *belle_sip_resolver_results_get_addrinfos(const belle_sip_resolver_results_t *obj) {
	return obj->ai_list;
}

int belle_sip_resolver_results_get_ttl(const belle_sip_resolver_results_t *obj) {
	return obj->ttl;
}

const char *belle_sip_resolver_results_get_name(const belle_sip_resolver_results_t *obj) {
	return obj->name;
}

belle_sip_resolver_results_t *
belle_sip_resolver_results_create(const char *name, struct addrinfo *ai_list, belle_sip_list_t *srv_list, int ttl) {
	belle_sip_resolver_results_t *obj = belle_sip_object_new(belle_sip_resolver_results_t);
	obj->ai_list = ai_list;
	obj->srv_list = srv_list;
	obj->ttl = ttl;
	obj->name = bctbx_strdup(name);
	return obj;
}

struct belle_sip_resolver_context {
	belle_sip_source_t source;
	belle_sip_stack_t *stack;
	uint32_t min_ttl;
	uint8_t notified;
	uint8_t cancelled;
#ifdef HAVE_DNS_SERVICE
	uint8_t use_dns_service;
	uint8_t pad[1];
#else  /* HAVE_DNS_SERVICE */
	uint8_t pad[2];
#endif /* else HAVE_DNS_SERVICE */
};

struct belle_sip_simple_resolver_context {
	belle_sip_resolver_context_t base;
	belle_sip_resolver_callback_t cb;
	belle_sip_resolver_srv_callback_t srv_cb;
	void *cb_data;
	void *srv_cb_data;
#ifdef HAVE_DNS_SERVICE
	bctbx_mutex_t
	    notify_mutex; // result is notified first on a specific thread, mutex to manage interactions with cancel
	DNSServiceErrorType
	    err; // used to pass the request dispatching status from the dns_service_queue to the main thread
	DNSServiceRef dns_service;
	dispatch_source_t dns_service_timer;
	uint16_t dns_service_type;
	dispatch_queue_t dns_service_queue; // this queue is created by the stack, but store a ref on it anyway
#endif                                  /*HAVE_DNS_SERVICE */
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
#ifdef HAVE_MDNS
	char *srv_prefix;
	char *srv_name;
	int resolving;
	bool_t browse_finished;
#endif
#if defined(USE_GETADDRINFO_FALLBACK) || defined(HAVE_MDNS)
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
	bool_t not_using_dns_socket;
};

struct belle_sip_combined_resolver_context {
	belle_sip_resolver_context_t base;
	belle_sip_resolver_callback_t cb;
	void *cb_data;
	char *name;
	int port;
	int family;
	struct addrinfo *final_results;
	struct addrinfo *a_fallback_results;
	belle_sip_list_t *srv_results;
	belle_sip_resolver_context_t *srv_ctx;
	belle_sip_resolver_context_t *a_fallback_ctx;
	int a_fallback_ttl;
	unsigned char srv_completed;
	unsigned char a_fallback_completed;
};

struct belle_sip_dual_resolver_context {
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

void belle_sip_resolver_context_init(belle_sip_resolver_context_t *obj, belle_sip_stack_t *stack) {
	obj->stack = stack;
	obj->min_ttl = UINT32_MAX;
#ifdef HAVE_DNS_SERVICE
	obj->use_dns_service =
	    obj->stack->use_dns_service; // the object might actually survive the stack, so save crucial information
#endif                               /* HAVE_DNS_SERVICE */
	belle_sip_init_sockets();        /* Need to be called for DNS resolution to work on Windows platform. */
}

static int dns_resconf_nameservers_from_list(struct dns_resolv_conf *resconf, const belle_sip_list_t *l) {
	int max_servers = sizeof(resconf->nameserver) / sizeof(struct sockaddr_storage);
	int i;
	const belle_sip_list_t *elem;

	for (i = 0, elem = l; i < max_servers && elem != NULL; elem = elem->next) {
		int error = dns_resconf_pton(&resconf->nameserver[i], (const char *)elem->data);
		if (error == 0) ++i;
	}

	return i > 0 ? 0 : -1;
}

static struct dns_resolv_conf *resconf(belle_sip_simple_resolver_context_t *ctx) {
	const char *path;
	const belle_sip_list_t *servers;
	int error;

	if (ctx->resconf) return ctx->resconf;

	if (!(ctx->resconf = dns_resconf_open(&error))) {
		belle_sip_error("%s dns_resconf_open error: %s", __FUNCTION__, dns_strerror(error));
		return NULL;
	}

	path = belle_sip_stack_get_dns_resolv_conf_file(ctx->base.stack);
	servers = ctx->base.stack->dns_servers;

	if (servers) {
		belle_sip_message("%s using application supplied dns server list.", __FUNCTION__);
		error = dns_resconf_nameservers_from_list(ctx->resconf, servers);
	} else if (!path) {
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
#elif defined(__ANDROID__)
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
	} else {
		error = dns_resconf_loadpath(ctx->resconf, path);
		if (error) {
			belle_sip_error("%s dns_resconf_loadpath() of custom file error [%s]: %s", __FUNCTION__, path,
			                dns_strerror(error));
			return NULL;
		}
	}

	if (error == 0) {
		char ip[64];
		char serv[10];
		int using_ipv6 = FALSE;
		size_t i;

		belle_sip_message("Resolver is using DNS server(s):");
		for (i = 0; i < sizeof(ctx->resconf->nameserver) / sizeof(ctx->resconf->nameserver[0]); ++i) {
			struct sockaddr *ns_addr = (struct sockaddr *)&ctx->resconf->nameserver[i];
			if (ns_addr->sa_family == AF_UNSPEC) break;
			bctbx_getnameinfo(ns_addr,
			                  ns_addr->sa_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr),
			                  ip, sizeof(ip), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV);
			belle_sip_message("\t%s", ip);
			if (ns_addr->sa_family == AF_INET6) using_ipv6 = TRUE;
		}
		ctx->resconf->iface.ss_family = using_ipv6 ? AF_INET6 : AF_INET;
		if (i == 0) {
			belle_sip_error("- no DNS servers available - resolution aborted.");
			return NULL;
		}
	} else {
		belle_sip_error("Error loading dns server addresses.");
		return NULL;
	}

	return ctx->resconf;
}

static struct dns_hosts *hosts(belle_sip_simple_resolver_context_t *ctx) {
	int error;

	if (ctx->hosts) return ctx->hosts;

	if (!(ctx->hosts = dns_hosts_local(&error))) {
		belle_sip_warning("%s dns_hosts_local error: %s", __FUNCTION__, dns_strerror(error));
		/*in case of failure, create an empty host object to make further processing happy, knowing that we can live
		 * without /etc/hosts.*/
		ctx->hosts = dns_hosts_open(&error);
	}

	if (ctx->base.stack->dns_user_hosts_file) {
		error = dns_hosts_loadpath(ctx->hosts, ctx->base.stack->dns_user_hosts_file);
		if (error) {
			belle_sip_error("%s dns_hosts_loadfile(\"%s\"): %s", __FUNCTION__, ctx->base.stack->dns_user_hosts_file,
			                dns_strerror(error));
		}
	}

	for (bctbx_list_t *it = ctx->base.stack->user_host_entries; it != NULL; it = it->next) {
		const char *ip = ((belle_sip_param_pair_t *)it->data)->name;
		const char *hostname = ((belle_sip_param_pair_t *)it->data)->value;
		error = dns_hosts_insert_v4_entry(ctx->hosts, ip, hostname);
		if (error) {
			belle_sip_error("%s cannot add host entry [%s:%s]: %s", __FUNCTION__, ip, hostname, dns_strerror(error));
		} else {
			belle_sip_message("host entry [%s:%s]  added", ip, hostname);
		}
	}

	return ctx->hosts;
}

struct dns_cache *cache(belle_sip_simple_resolver_context_t *ctx) {
	return NULL;
}

static struct addrinfo *ai_list_append(struct addrinfo *ai_list, struct addrinfo *ai_to_append) {
	struct addrinfo *ai_current = ai_list;
	if (ai_to_append == NULL) return ai_list;
	if (ai_list == NULL) return ai_to_append;
	while (ai_current->ai_next != NULL) {
		ai_current = ai_current->ai_next;
	}
	ai_current->ai_next = ai_to_append;
	return ai_list;
}

static int srv_compare_prio(const void *psrv1, const void *psrv2) {
	belle_sip_dns_srv_t *srv1 = (belle_sip_dns_srv_t *)psrv1;
	belle_sip_dns_srv_t *srv2 = (belle_sip_dns_srv_t *)psrv2;
	if (srv1->priority < srv2->priority) return -1;
	if (srv1->priority == srv2->priority) return 0;
	return 1;
}

#ifdef HAVE_MDNS
static int mdns_srv_compare_host_and_port(const void *psrv1, const void *psrv2) {
	belle_sip_dns_srv_t *srv1 = (belle_sip_dns_srv_t *)psrv1;
	belle_sip_dns_srv_t *srv2 = (belle_sip_dns_srv_t *)psrv2;
	int ret = strcmp(srv1->target, srv2->target);
	if (ret != 0) return ret;
	if (srv1->port < srv2->port) return -1;
	if (srv1->port == srv2->port) return 0;
	return 1;
}

static int mdns_srv_compare_fullname(const void *psrv1, const void *psrv2) {
	belle_sip_dns_srv_t *srv1 = (belle_sip_dns_srv_t *)psrv1;
	belle_sip_dns_srv_t *srv2 = (belle_sip_dns_srv_t *)psrv2;
	return strcmp(srv1->fullname, srv2->fullname);
}
#endif

/*
 * see https://www.ietf.org/rfc/rfc2782.txt
 * 0 weighted must just appear first.
 **/
static int srv_sort_weight(const void *psrv1, const void *psrv2) {
	belle_sip_dns_srv_t *srv1 = (belle_sip_dns_srv_t *)psrv1;
	if (srv1->weight == 0) return -1;
	return 1;
}

static belle_sip_dns_srv_t *srv_elect_one(belle_sip_list_t *srv_list) {
	int sum = 0;
	belle_sip_list_t *elem;
	belle_sip_dns_srv_t *srv;
	int rand_number;

	for (elem = srv_list; elem != NULL; elem = elem->next) {
		srv = (belle_sip_dns_srv_t *)elem->data;
		sum += srv->weight;
		srv->cumulative_weight = sum;
	}
	/*no weights given, return the first one*/
	if (sum == 0) return (belle_sip_dns_srv_t *)srv_list->data;
	rand_number = belle_sip_random() % sum; /*random number choosen in the range of the sum of weights*/
	for (elem = srv_list; elem != NULL; elem = elem->next) {
		srv = (belle_sip_dns_srv_t *)elem->data;
		if (rand_number <= srv->cumulative_weight) return srv;
	}
	return (belle_sip_dns_srv_t *)srv_list->data;
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
static belle_sip_list_t *srv_select_by_weight(belle_sip_list_t *srv_list) {
	belle_sip_list_t *same_prio = NULL;
	belle_sip_list_t *elem;
	belle_sip_dns_srv_t *prev_srv = NULL;
	belle_sip_list_t *result = NULL;

	for (elem = srv_list; elem != NULL; elem = elem->next) {
		belle_sip_dns_srv_t *srv = (belle_sip_dns_srv_t *)elem->data;
		if (prev_srv) {
			if (prev_srv->priority == srv->priority) {
				if (!same_prio) {
					same_prio = belle_sip_list_append(same_prio, prev_srv);
				}
				same_prio = belle_sip_list_insert_sorted(same_prio, srv, srv_sort_weight);
			} else {
				if (same_prio) {
					result = belle_sip_list_concat(result, srv_elect(&same_prio));
				}
			}
		}
		prev_srv = srv;
	}
	if (same_prio) {
		result = belle_sip_list_concat(result, srv_elect(&same_prio));
	}
	if (result) {
		belle_sip_list_free_with_data(srv_list, belle_sip_object_unref);
		return result;
	}
	return srv_list; /*no weight election was necessary, return original list*/
}

static void simple_resolver_context_notify(belle_sip_resolver_context_t *obj) {
	belle_sip_simple_resolver_context_t *ctx = BELLE_SIP_SIMPLE_RESOLVER_CONTEXT(obj);
	if ((ctx->type == DNS_T_A) || (ctx->type == DNS_T_AAAA)
#ifdef HAVE_DNS_SERVICE
	    || (ctx->dns_service_type == kDNSServiceType_A) || (ctx->dns_service_type == kDNSServiceType_AAAA)
#endif /* HAVE_DNS_SERVICE */
	) {
		struct addrinfo **ai_list = &ctx->ai_list;
		belle_sip_resolver_results_t *results;
#if defined(USE_GETADDRINFO_FALLBACK) || defined(HAVE_MDNS)
		if (ctx->getaddrinfo_ai_list != NULL) ai_list = &ctx->getaddrinfo_ai_list;
#endif
		results =
		    belle_sip_resolver_results_create(ctx->name, *ai_list, NULL, BELLE_SIP_RESOLVER_CONTEXT(obj)->min_ttl);
		ctx->cb(ctx->cb_data, results);
		*ai_list = NULL;
		belle_sip_object_unref(results);
	} else if (ctx->type == DNS_T_SRV
#ifdef HAVE_DNS_SERVICE
	           || (ctx->dns_service_type == kDNSServiceType_SRV)
#endif /* HAVE_DNS_SERVICE */
	) {
		ctx->srv_list = srv_select_by_weight(ctx->srv_list);
		ctx->srv_cb(ctx->srv_cb_data, ctx->name, ctx->srv_list, BELLE_SIP_RESOLVER_CONTEXT(obj)->min_ttl);
	}
}

static void dual_resolver_context_notify(belle_sip_resolver_context_t *obj) {
	belle_sip_dual_resolver_context_t *ctx = BELLE_SIP_DUAL_RESOLVER_CONTEXT(obj);
	struct addrinfo *results = ctx->aaaa_results;
	belle_sip_resolver_results_t *result_obj;
	if (obj->stack->ai_family_preference == AF_INET6) {
		results = ai_list_append(results, ctx->a_results);
	} else {
		results = ai_list_append(ctx->a_results, results);
	}
	ctx->a_results = NULL;
	ctx->aaaa_results = NULL;
	result_obj = belle_sip_resolver_results_create(ctx->name, results, NULL, BELLE_SIP_RESOLVER_CONTEXT(obj)->min_ttl);
	ctx->cb(ctx->cb_data, result_obj);
	belle_sip_object_unref(result_obj);
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
	belle_sip_resolver_results_t *results = belle_sip_resolver_results_create(
	    ctx->name, ctx->final_results, ctx->srv_results, BELLE_SIP_RESOLVER_CONTEXT(obj)->min_ttl);
	ctx->cb(ctx->cb_data, results);
	belle_sip_object_unref(results);
	ctx->final_results = NULL;
	ctx->srv_results = NULL;
	combined_resolver_context_cleanup(ctx);
}

static void append_dns_result(belle_sip_simple_resolver_context_t *ctx,
                              struct addrinfo **ai_list,
                              struct sockaddr *addr,
                              socklen_t addrlen) {
	char host[NI_MAXHOST + 1];
	int gai_err;
	int family = ctx->family;

	if ((gai_err = bctbx_getnameinfo(addr, addrlen, host, sizeof(host), NULL, 0, NI_NUMERICHOST)) != 0) {
		belle_sip_error("append_dns_result(): getnameinfo() failed: %s", gai_strerror(gai_err));
		return;
	}
	if (ctx->flags & AI_V4MAPPED) family = AF_INET6;
	*ai_list = ai_list_append(*ai_list, bctbx_ip_address_to_addrinfo(family, SOCK_STREAM, host, ctx->port));
	belle_sip_message("%s resolved to %s", ctx->name, host);
}

static int resolver_process_data(belle_sip_simple_resolver_context_t *ctx, unsigned int revents) {
	struct dns_packet *ans;
	struct dns_rr_i *I;
	struct dns_rr_i dns_rr_it;
	int error;
	unsigned char simulated_timeout = 0;
	int timeout = belle_sip_stack_get_dns_timeout(ctx->base.stack);
	unsigned char search_enabled = belle_sip_stack_dns_search_enabled(ctx->base.stack);

	/*Setting timeout to 0 can be used to simulate DNS timeout*/
	if ((revents != 0) && timeout == 0) {
		belle_sip_warning("Simulating DNS timeout");
		simulated_timeout = 1;
	}

	if (simulated_timeout ||
	    ((revents & BELLE_SIP_EVENT_TIMEOUT) && ((int)(belle_sip_time_ms() - ctx->start_time) >= timeout))) {
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
					append_dns_result(ctx, &ctx->ai_list, (struct sockaddr *)&sin6, sizeof(sin6));
					if (rr.ttl < BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl)
						BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl = rr.ttl;
				} else if ((ctx->type == DNS_T_A) && (rr.class == DNS_C_IN) && (rr.type == DNS_T_A)) {
					struct dns_a *a = &any.a;
					struct sockaddr_in sin;
					memset(&sin, 0, sizeof(sin));
					memcpy(&sin.sin_addr, &a->addr, sizeof(sin.sin_addr));
					sin.sin_family = AF_INET;
					sin.sin_port = ctx->port;
					append_dns_result(ctx, &ctx->ai_list, (struct sockaddr *)&sin, sizeof(sin));
					if (rr.ttl < BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl)
						BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl = rr.ttl;
				} else if ((ctx->type == DNS_T_SRV) && (rr.class == DNS_C_IN) && (rr.type == DNS_T_SRV)) {
					char host[NI_MAXHOST + 1];
					struct dns_srv *srv = &any.srv;
					belle_sip_dns_srv_t *b_srv = belle_sip_dns_srv_create(srv);
					snprintf(host, sizeof(host), "[target:%s port:%d prio:%d weight:%d]", srv->target, srv->port,
					         srv->priority, srv->weight);
					ctx->srv_list =
					    belle_sip_list_insert_sorted(ctx->srv_list, belle_sip_object_ref(b_srv), srv_compare_prio);
					belle_sip_message("SRV %s resolved to %s", ctx->name, host);
					if (rr.ttl < BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl)
						BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl = rr.ttl;
				}
			}
		}
		free(ans);
#if defined(USE_GETADDRINFO_FALLBACK) || defined(HAVE_MDNS)
		ctx->getaddrinfo_cancelled = TRUE;
#endif
		if (dns_res_was_asymetric(ctx->R)) {
			belle_sip_message("DNS answer was not received from the DNS server IP address the request was sent to. "
			                  "This seems to be a known issue with NAT64 networks created by Apple computers.");
		}
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(ctx));
		return BELLE_SIP_STOP;
	}
	if (error != DNS_EAGAIN) {
		belle_sip_error("%s dns_res_check() error: %s (%d)", __FUNCTION__, dns_strerror(error), error);
#if defined(USE_GETADDRINFO_FALLBACK) || defined(HAVE_MDNS)
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
#if defined(USE_GETADDRINFO_FALLBACK) || defined(HAVE_MDNS)
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

#if defined(USE_GETADDRINFO_FALLBACK) || defined(HAVE_MDNS)
static void *_resolver_getaddrinfo_thread(void *ptr) {
	belle_sip_simple_resolver_context_t *ctx = (belle_sip_simple_resolver_context_t *)ptr;
	struct addrinfo *res = NULL;
	struct addrinfo hints = {0};
	char serv[10];
	int err;

	belle_sip_message("Resolver getaddrinfo thread started.");
	snprintf(serv, sizeof(serv), "%i", ctx->port);
	hints.ai_family = ctx->family;
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_protocol = strstr(ctx->name, "udp") ? IPPROTO_UDP : IPPROTO_TCP;
	err = getaddrinfo(ctx->name, serv, &hints, &res);
	if (err != 0) {
		belle_sip_error("getaddrinfo DNS resolution of %s failed: %s", ctx->name, gai_strerror(err));
	} else if (!ctx->getaddrinfo_cancelled) {
		struct addrinfo *res_it = res;
		do {
			append_dns_result(ctx, &ctx->getaddrinfo_ai_list, res_it->ai_addr, (socklen_t)res_it->ai_addrlen);
			res_it = res_it->ai_next;
		} while (res_it != NULL);
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
	bctbx_thread_create(&ctx->getaddrinfo_thread, NULL, _resolver_getaddrinfo_thread, ctx);
	ctx->getaddrinfo_source = belle_sip_fd_source_new((belle_sip_source_func_t)_resolver_getaddrinfo_callback, ctx, fd,
	                                                  BELLE_SIP_EVENT_READ, -1);
	belle_sip_main_loop_add_source(ctx->base.stack->ml, ctx->getaddrinfo_source);
}
#endif

#ifdef HAVE_MDNS
static int is_mdns_query(const char *name) {
	const char *suffix;
	char *tmp = NULL;
	int ret;
	int len = strlen(name);

	if (len > 0 && name[len - 1] == '.') {
		tmp = belle_sip_strdup(name);
		tmp[len - 1] = '\0';
		name = tmp;
	}

	/* Check if name ends with .local to determine if we'll use multicast DNS or not */
	suffix = strrchr(name, '.');
	ret = suffix && strcmp(suffix, ".local") == 0;
	if (tmp) belle_sip_free(tmp);
	return ret;
}
#endif

#ifdef HAVE_DNS_SERVICE
static void dns_service_deallocate(belle_sip_simple_resolver_context_t *ctx) {
	if (ctx->dns_service_timer) {
		dispatch_source_cancel(ctx->dns_service_timer);
		dispatch_release(ctx->dns_service_timer);
		ctx->dns_service_timer = NULL;
	}
	if (ctx->dns_service) {
		DNSServiceRefDeallocate(ctx->dns_service);
		ctx->dns_service = NULL;
	}
}

static void dns_service_query_record_cb(DNSServiceRef dns_service,
                                        DNSServiceFlags flags,
                                        uint32_t interfaceIndex,
                                        DNSServiceErrorType errorCode,
                                        const char *fullname,
                                        uint16_t rrtype,
                                        uint16_t rrclass,
                                        uint16_t rdlen,
                                        const void *rdata,
                                        uint32_t ttl,
                                        void *context) {
	belle_sip_simple_resolver_context_t *ctx = (belle_sip_simple_resolver_context_t *)context;
	bctbx_mutex_lock(&(ctx->notify_mutex));
	if (ctx->base.cancelled == TRUE) {
		dns_service_deallocate(ctx);
		// resolver was cancelled, we may not have the stack anymore, just unref ourselve
		bctbx_mutex_unlock(&(ctx->notify_mutex));
		belle_sip_object_unref(ctx);
		return;
	}

	if (errorCode != kDNSServiceErr_NoError) {
		if (errorCode == kDNSServiceErr_NoSuchRecord) {
			belle_sip_message("%s : resolving %s : no such record", __FUNCTION__, fullname);
		} else {
			belle_sip_error("%s : resolving %s got error %d", __FUNCTION__, fullname, errorCode);
		}
		dns_service_deallocate(ctx);
		belle_sip_main_loop_do_later(ctx->base.stack->ml, (belle_sip_callback_t)belle_sip_resolver_context_notify,
		                             BELLE_SIP_RESOLVER_CONTEXT(ctx));
		bctbx_mutex_unlock(&(ctx->notify_mutex));
		return;
	}

	if (rrclass != kDNSServiceClass_IN) {
		belle_sip_error("%s : resolving %s got class %d while IN(0x01) expected", __FUNCTION__, fullname, rrclass);
		dns_service_deallocate(ctx);
		belle_sip_main_loop_do_later(ctx->base.stack->ml, (belle_sip_callback_t)belle_sip_resolver_context_notify,
		                             BELLE_SIP_RESOLVER_CONTEXT(ctx));
		bctbx_mutex_unlock(&(ctx->notify_mutex));
		return;
	}

	bool_t got_valid_answer = FALSE;
	switch (rrtype) {
		case kDNSServiceType_A:
			if (rdlen == 4) {
				struct sockaddr_in sin;
				memset(&sin, 0, sizeof(sin));
				memcpy(&sin.sin_addr, rdata, sizeof(sin.sin_addr));
				sin.sin_family = AF_INET;
				sin.sin_port = ctx->port;
				append_dns_result(ctx, &ctx->ai_list, (struct sockaddr *)&sin, sizeof(sin));
				if (ttl < BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl) BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl = ttl;
				got_valid_answer = TRUE;
			}
			break;
		case kDNSServiceType_AAAA:
			if (rdlen == 16) {
				struct sockaddr_in6 sin;
				memset(&sin, 0, sizeof(sin));
				memcpy(&sin.sin6_addr, rdata, sizeof(sin.sin6_addr));
				sin.sin6_family = AF_INET6;
				sin.sin6_port = ctx->port;
				append_dns_result(ctx, &ctx->ai_list, (struct sockaddr *)&sin, sizeof(sin));
				if (ttl < BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl) BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl = ttl;
				got_valid_answer = TRUE;
			}
			break;

		case kDNSServiceType_SRV:
			if (rdlen > 6) {

				/* re-create a DNS packet to be able to use dns_util.h dns_parse_resource_record */
				/* Must add: name, type, class, ttl, rdlength before pasting the rdata to be parsed */
				char *dnsPacket = belle_sip_malloc(rdlen + 11);
				dnsPacket[0] = 0;                                 // name is empty: 0 length
				dnsPacket[1] = 0xFF & (kDNSServiceType_SRV >> 8); // type is SRV
				dnsPacket[2] = 0xFF & kDNSServiceType_SRV;
				dnsPacket[3] = 0xFF & (kDNSServiceClass_IN >> 8); // Class is IN
				dnsPacket[4] = 0xFF & kDNSServiceClass_IN;
				dnsPacket[5] = 0x01;                // ttl is not relevant, just put fixed value
				dnsPacket[6] = 0x01;                // ttl is not relevant, just put fixed value
				dnsPacket[7] = 0x01;                // ttl is not relevant, just put fixed value
				dnsPacket[8] = 0x01;                // ttl is not relevant, just put fixed value
				dnsPacket[9] = 0xFF & (rdlen >> 8); // use the given size
				dnsPacket[10] = 0xFF & rdlen;
				memcpy(dnsPacket + 11, rdata, rdlen);
				dns_resource_record_t *rr = dns_parse_resource_record(dnsPacket, rdlen + 11);
				belle_sip_free(dnsPacket);

				char host[NI_MAXHOST + 1];
				belle_sip_dns_srv_t *b_srv = belle_sip_dns_srv_create_dns_service(rr);
				dns_free_resource_record(rr);
				snprintf(host, sizeof(host), "[target:%s port:%d prio:%d weight:%d]", b_srv->target, b_srv->port,
				         b_srv->priority, b_srv->weight);

				ctx->srv_list =
				    belle_sip_list_insert_sorted(ctx->srv_list, belle_sip_object_ref(b_srv), srv_compare_prio);
				belle_sip_message("SRV %s resolved to %s", ctx->name, host);
				if (ttl < BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl) BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl = ttl;

				got_valid_answer = TRUE;
			}
			break;
		case kDNSServiceType_CNAME:
			belle_sip_message(
			    "%s : resolving %s got CNAME DNS answer. Ignore it, DNS Service will recurse it by itself",
			    __FUNCTION__, fullname);
			break;
		default:
			belle_sip_error("%s : resolving %s got DNS answer type %d - just ignore it", __FUNCTION__, fullname,
			                rrtype);
			bctbx_mutex_unlock(&(ctx->notify_mutex));
			return;
	}

	if (!(flags & kDNSServiceFlagsMoreComing) &&
	    (got_valid_answer == TRUE)) { // notify only when nothing more is coming and we had a valid answer
		dns_service_deallocate(ctx);
		belle_sip_main_loop_do_later(ctx->base.stack->ml, (belle_sip_callback_t)belle_sip_resolver_context_notify,
		                             BELLE_SIP_RESOLVER_CONTEXT(ctx));
	}
	bctbx_mutex_unlock(&(ctx->notify_mutex));
}
#endif /* HAVE_DNS_SERVICE */

static int _resolver_send_query(belle_sip_simple_resolver_context_t *ctx) {
	int error = 0;

#ifdef HAVE_MDNS
	if (is_mdns_query(ctx->name)) {
		ctx->not_using_dns_socket = TRUE;
		_resolver_getaddrinfo_start(ctx);
		return 0;
	}
#endif

	if (!ctx->base.stack->resolver_send_error) {
		error = dns_res_submit(ctx->R, ctx->name, ctx->type, DNS_C_IN);
		if (error) belle_sip_error("%s dns_res_submit error [%s]: %s", __FUNCTION__, ctx->name, dns_strerror(error));
	} else {
		/* Error simulation */
		error = ctx->base.stack->resolver_send_error;
		belle_sip_error("%s dns_res_submit error [%s]: simulated error %d", __FUNCTION__, ctx->name, error);
	}
	if (error < 0) {
		return -1;
	}

	if (resolver_process_data(ctx, 0) == BELLE_SIP_CONTINUE) {
		ctx->start_time = belle_sip_time_ms();
		belle_sip_message("DNS resolution awaiting response, queued to main loop");
		/*only init source if res inprogress*/
		/*the timeout set to the source is 1 s, this is to allow dns.c to send request retransmissions*/
		belle_sip_socket_source_init((belle_sip_source_t *)ctx, (belle_sip_source_func_t)resolver_process_data, ctx,
		                             dns_res_pollfd(ctx->R), BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_TIMEOUT, 1000);
#if defined(USE_GETADDRINFO_FALLBACK)
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
	int err = _resolver_send_query(ctx);
	if (err == 0) return BELLE_SIP_CONTINUE;

	return BELLE_SIP_STOP;
}

#ifdef HAVE_MDNS
typedef struct belle_sip_mdns_source belle_sip_mdns_source_t;
#define BELLE_SIP_MDNS_SOURCE(obj) BELLE_SIP_CAST(obj, belle_sip_mdns_source_t)

struct belle_sip_mdns_source {
	belle_sip_source_t base;
	DNSServiceRef service_ref;
	belle_sip_simple_resolver_context_t *ctx;
	bool_t resolve_finished;
};

belle_sip_mdns_source_t *belle_sip_mdns_source_new(DNSServiceRef service_ref,
                                                   belle_sip_simple_resolver_context_t *ctx,
                                                   belle_sip_source_func_t func,
                                                   int timeout) {
	belle_sip_mdns_source_t *obj = belle_sip_object_new(belle_sip_mdns_source_t);
	obj->service_ref = service_ref;
	obj->ctx = ctx;
	obj->resolve_finished = BELLE_SIP_CONTINUE;
	belle_sip_fd_source_init((belle_sip_source_t *)obj, func, obj, DNSServiceRefSockFD(service_ref),
	                         BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_TIMEOUT, timeout);
	return obj;
}

void belle_sip_mdns_source_set_service_ref(belle_sip_mdns_source_t *source, DNSServiceRef service_ref) {
	source->service_ref = service_ref;
	source->base.fd = DNSServiceRefSockFD(service_ref);
}

static void belle_sip_mdns_source_destroy(belle_sip_mdns_source_t *obj) {
	if (obj->service_ref) {
		DNSServiceRefDeallocate(obj->service_ref);
	}
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_mdns_source_t);
BELLE_SIP_INSTANCIATE_VPTR(
    belle_sip_mdns_source_t, belle_sip_source_t, belle_sip_mdns_source_destroy, NULL, NULL, TRUE);

static void resolver_process_mdns_resolve(DNSServiceRef service_ref,
                                          DNSServiceFlags flags,
                                          uint32_t interface,
                                          DNSServiceErrorType error_code,
                                          const char *fullname,
                                          const char *hosttarget,
                                          uint16_t port,
                                          uint16_t txt_len,
                                          const unsigned char *txt_record,
                                          belle_sip_mdns_source_t *source) {
	if (error_code != kDNSServiceErr_NoError) {
		belle_sip_error("%s error while resolving [%s]: code %d", __FUNCTION__, source->ctx->name, error_code);
	} else {
		uint8_t prio_size, weight_size, ttl_size;

		const char *prio_buf = TXTRecordGetValuePtr(txt_len, txt_record, "prio", &prio_size);
		const char *weight_buf = TXTRecordGetValuePtr(txt_len, txt_record, "weight", &weight_size);
		const char *ttl_buf = TXTRecordGetValuePtr(txt_len, txt_record, "ttl", &ttl_size);

		/* If the buffer is non-NULL then the key exist and if the result size is > 0 then the value is not empty */
		if (prio_buf && prio_size > 0 && weight_buf && weight_size > 0 && ttl_buf && ttl_size > 0) {
			short unsigned int prio, weight, ttl;

			/* Don't use the VLAs since it doesn't work on Windows */
			char *prio_value = belle_sip_malloc(prio_size + 1);
			char *weight_value = belle_sip_malloc(weight_size + 1);
			char *ttl_value = belle_sip_malloc(ttl_size + 1);

			memcpy(prio_value, prio_buf, prio_size);
			memcpy(weight_value, weight_buf, weight_size);
			memcpy(ttl_value, ttl_buf, ttl_size);

			prio_value[prio_size] = '\0';
			weight_value[weight_size] = '\0';
			ttl_value[ttl_size] = '\0';

			prio = atoi(prio_value);
			weight = atoi(weight_value);
			ttl = atoi(ttl_value);

			belle_sip_dns_srv_t *b_srv = belle_sip_mdns_srv_create(prio, weight, port, hosttarget, fullname);
			if (!belle_sip_list_find_custom(source->ctx->srv_list, mdns_srv_compare_host_and_port, b_srv)) {
				source->ctx->srv_list =
				    belle_sip_list_insert_sorted(source->ctx->srv_list, belle_sip_object_ref(b_srv), srv_compare_prio);
				if (ttl < BELLE_SIP_RESOLVER_CONTEXT(source->ctx)->min_ttl)
					BELLE_SIP_RESOLVER_CONTEXT(source->ctx)->min_ttl = ttl;

				belle_sip_message("mDNS %s resolved to [target:%s port:%d prio:%d weight:%d ttl:%d]", source->ctx->name,
				                  hosttarget, port, prio, weight, ttl);
			} else {
				belle_sip_object_unref(b_srv);
			}

			belle_sip_free(prio_value);
			belle_sip_free(weight_value);
			belle_sip_free(ttl_value);

			source->resolve_finished = BELLE_SIP_STOP;
		} else {
			belle_sip_warning("%s TXT record of %s does not contain a priority, weight or ttl key!", __FUNCTION__,
			                  hosttarget);
		}
	}

	source->ctx->resolving--;

	/* If this is the last resolve and the browse has already timed out then we notify */
	if (source->ctx->resolving == 0 && source->ctx->browse_finished)
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(source->ctx));
}

static int resolver_process_mdns_resolve_result(belle_sip_mdns_source_t *source, unsigned int revents) {
	if (revents & BELLE_SIP_EVENT_READ) {
		DNSServiceProcessResult(source->service_ref);
	}

	if (revents & BELLE_SIP_EVENT_TIMEOUT) return BELLE_SIP_STOP;

	return source->resolve_finished;
}

static void resolver_process_mdns_browse(DNSServiceRef service_ref,
                                         DNSServiceFlags flags,
                                         uint32_t interface,
                                         DNSServiceErrorType error_code,
                                         const char *name,
                                         const char *type,
                                         const char *domain,
                                         belle_sip_simple_resolver_context_t *ctx) {
	if (error_code != kDNSServiceErr_NoError) {
		belle_sip_error("%s error while browing [%s]: code %d", __FUNCTION__, ctx->name, error_code);
	} else {
		if (flags & kDNSServiceFlagsAdd) {
			DNSServiceRef resolve_ref;
			DNSServiceErrorType error;

			belle_sip_mdns_source_t *source = belle_sip_mdns_source_new(
			    NULL, ctx, (belle_sip_source_func_t)resolver_process_mdns_resolve_result, 1000);

			error = DNSServiceResolve(&resolve_ref, 0, interface, name, type, domain,
			                          (DNSServiceResolveReply)resolver_process_mdns_resolve, source);

			if (error == kDNSServiceErr_NoError) {
				ctx->resolving++;

				belle_sip_mdns_source_set_service_ref(source, resolve_ref);
				belle_sip_main_loop_add_source(ctx->base.stack->ml, (belle_sip_source_t *)source);
			} else {
				belle_sip_error("%s DNSServiceResolve error [%s]: code %d", __FUNCTION__, ctx->name, error);
				belle_sip_object_unref(source);
			}
		} else {
			belle_sip_list_t *elem;
			char fullname[512];

			/* If the browse service does not have the Add flags then it has to be removed */
			snprintf(fullname, sizeof(fullname), "%s.%s%s", name, type, domain);
			belle_sip_dns_srv_t *b_srv = belle_sip_mdns_srv_create(-1, -1, -1, NULL, fullname);

			elem = belle_sip_list_find_custom(ctx->srv_list, mdns_srv_compare_fullname, b_srv);
			if (elem) {
				belle_sip_object_unref((belle_sip_dns_srv_t *)elem->data);
				ctx->srv_list = belle_sip_list_delete_link(ctx->srv_list, elem);
			}

			belle_sip_object_unref(b_srv);
		}
	}
}

static int resolver_process_mdns_browse_result(belle_sip_mdns_source_t *source, unsigned int revents) {
	if (revents & BELLE_SIP_EVENT_READ) {
		DNSServiceProcessResult(source->service_ref);
	}

	if (revents & BELLE_SIP_EVENT_TIMEOUT) {
		if (source->ctx->resolving == 0) belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(source->ctx));

		source->ctx->browse_finished = TRUE;
		return BELLE_SIP_STOP;
	}
	return BELLE_SIP_CONTINUE;
}
#endif

static int _resolver_start_query(belle_sip_simple_resolver_context_t *ctx) {
	if (!ctx->name) return -1;
#ifdef HAVE_DNS_SERVICE
	if (ctx->base.use_dns_service == TRUE) {
		// Take a ref on the context so we are sure it will still exists when the notify is called
		// When returning an error, the notify is called anyway so do not unref, the matching unref
		// is always performed in the belle_sip_resolver_context_notify function or directly in the callback
		// when the resolve has been cancelled
		belle_sip_object_ref(ctx);

		/* Create the DNSServiceRef */
		DNSServiceErrorType err = kDNSServiceErr_NoError;
		err = DNSServiceQueryRecord(&(ctx->dns_service), kDNSServiceFlagsReturnIntermediates,
		                            0, // interfaceIndex
		                            ctx->name, ctx->dns_service_type, kDNSServiceClass_IN, dns_service_query_record_cb,
		                            (void *)ctx);

		if (err != kDNSServiceErr_NoError) {
			belle_sip_error("DNS query resolving %s, DNSServiceQueryRecord returned %d", ctx->name, err);
			return -1;
		}

		/* Create a timer */
		ctx->dns_service_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, ctx->dns_service_queue);
		int timeout = belle_sip_stack_get_dns_timeout(ctx->base.stack); // timeout is in ms
		dispatch_source_set_event_handler(ctx->dns_service_timer, ^{    // block captures ctx
		  bctbx_mutex_lock(&(ctx->notify_mutex));
		  dns_service_deallocate(ctx); /* disarm timer - we need it once and deallocate the query */
		  if (ctx->base.cancelled == TRUE) {
			  // resolver was cancelled, we may not have the stack anymore, just unref ourselve
			  bctbx_mutex_unlock(&(ctx->notify_mutex));
			  belle_sip_object_unref(ctx);
			  return;
		  }
		  belle_sip_message("DNS timer fired while resolving %s ", ctx->name);
		  belle_sip_main_loop_do_later(ctx->base.stack->ml, (belle_sip_callback_t)belle_sip_resolver_context_notify,
			                           BELLE_SIP_RESOLVER_CONTEXT(ctx));
		  bctbx_mutex_unlock(&(ctx->notify_mutex));
		});

		/* dispatch them */
		if (!ctx->base.stack->resolver_send_error) {
			dispatch_sync(
			    ctx->dns_service_queue,
			    ^{ // disptach them from the queue listening their events to avoid troubles, this block captures the ctx
				  bctbx_mutex_lock(&(ctx->notify_mutex));
				  dispatch_source_set_timer(ctx->dns_service_timer,
				                            dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_MSEC),
				                            timeout * NSEC_PER_MSEC, 0); // timeout given is in Nano seconds
				  dispatch_resume(ctx->dns_service_timer);
				  ctx->err = DNSServiceSetDispatchQueue(
				      ctx->dns_service,
				      ctx->dns_service_queue); // could return an error to be passed back to the main thread
				  if (ctx->err != kDNSServiceErr_NoError) {
					  dns_service_deallocate(
					      ctx); /* failed to dispatch, deallocate and disarm timer(which may already have fired) */
				  }
				  bctbx_mutex_unlock(&(ctx->notify_mutex));
			    });

			if (ctx->err != kDNSServiceErr_NoError) {
				belle_sip_message("DNS start_query resolving %s, DNSServiceSetDispatchQueue returned %d", ctx->name,
				                  err);
				return -1;
			}

		} else {
			/* Error simulation */
			belle_sip_error("%s DNSServiceRefSockFD error [%s]: simulated error %d", __FUNCTION__, ctx->name,
			                ctx->base.stack->resolver_send_error);
			return -1;
		}

		return 0;
	}
#endif /* HAVE_DNS_SERVICE */
#ifdef HAVE_MDNS
	if (is_mdns_query(ctx->name) && ctx->type == DNS_T_SRV) {
		DNSServiceErrorType error;
		DNSServiceRef browse_ref;

		error = DNSServiceBrowse(&browse_ref, 0, 0, ctx->srv_prefix, ctx->srv_name,
		                         (DNSServiceBrowseReply)resolver_process_mdns_browse, ctx);

		if (error == kDNSServiceErr_NoError) {
			belle_sip_mdns_source_t *source = belle_sip_mdns_source_new(
			    browse_ref, ctx, (belle_sip_source_func_t)resolver_process_mdns_browse_result, 1000);
			belle_sip_main_loop_add_source(ctx->base.stack->ml, (belle_sip_source_t *)source);

			return 0;
		} else {
			belle_sip_error("%s DNSServiceBrowse error [%s]: code %d", __FUNCTION__, ctx->name, error);

			return -1;
		}
	} else {
#endif
		struct dns_options opts;
		int error;
		struct dns_resolv_conf *conf;

		conf = resconf(ctx);
		if (conf) {
			conf->options.recurse = 0;
			conf->options.timeout = 2;
			conf->options.attempts = 5;
		} else return -1;
		if (!hosts(ctx)) return -1;

		memset(&opts, 0, sizeof opts);

		/* When there are IPv6 nameservers, allow responses to arrive from an IP address that is not the IP address to
		 * which the request was sent originally. Mac' NAT64 network tend to do this sometimes.*/
		opts.udp_uses_connect = ctx->resconf->iface.ss_family != AF_INET6;
		if (!opts.udp_uses_connect) belle_sip_message("Resolver is not using connect().");

		if (!(ctx->R = dns_res_open(ctx->resconf, ctx->hosts, dns_hints_mortal(dns_hints_local(ctx->resconf, &error)),
		                            cache(ctx), &opts, &error))) {
			belle_sip_error("%s dns_res_open error [%s]: %s", __FUNCTION__, ctx->name, dns_strerror(error));
			return -1;
		}
		error = 0;
		if (ctx->base.stack->resolver_tx_delay > 0) {
			belle_sip_socket_source_init((belle_sip_source_t *)ctx,
			                             (belle_sip_source_func_t)resolver_process_data_delayed, ctx, -1,
			                             BELLE_SIP_EVENT_TIMEOUT, ctx->base.stack->resolver_tx_delay + 1000);
			belle_sip_message("%s DNS resolution delayed by %d ms", __FUNCTION__, ctx->base.stack->resolver_tx_delay);
		} else {
			error = _resolver_send_query(ctx);
		}
		if (error == 0 && !ctx->base.notified && !ctx->not_using_dns_socket)
			belle_sip_main_loop_add_source(ctx->base.stack->ml, (belle_sip_source_t *)ctx);
		return error;
#ifdef HAVE_MDNS
	}
#endif
}

static belle_sip_simple_resolver_context_t *resolver_start_query(belle_sip_simple_resolver_context_t *ctx) {
	int error;

	/* Take a ref for this part of code because _resolver_start_query() can notify the results and free the ctx if this
	 * is not the case. */
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

static void belle_sip_combined_resolver_context_destroy(belle_sip_combined_resolver_context_t *obj) {
	if (obj->name != NULL) {
		belle_sip_free(obj->name);
		obj->name = NULL;
	}
	if (obj->srv_ctx) {
		belle_sip_object_unref(obj->srv_ctx);
		obj->srv_ctx = NULL;
	}
	if (obj->a_fallback_ctx) {
		belle_sip_object_unref(obj->a_fallback_ctx);
		obj->a_fallback_ctx = NULL;
	}
	if (obj->a_fallback_results) {
		/* we don't a/aaaa results since SRV provided results*/
		bctbx_freeaddrinfo(obj->a_fallback_results);
		obj->a_fallback_results = NULL;
	}
}

static void belle_sip_simple_resolver_context_destroy(belle_sip_simple_resolver_context_t *ctx) {
	/* Do not free elements of ctx->ai_list with bctbx_freeaddrinfo(). Let the caller do it, otherwise
	   it will not be able to use them after the resolver has been destroyed. */
#if defined(USE_GETADDRINFO_FALLBACK) || defined(HAVE_MDNS)
	if (ctx->getaddrinfo_thread != 0) {
		bctbx_thread_join(ctx->getaddrinfo_thread, NULL);
#ifdef _WIN32
		if (ctx->ctlevent != (belle_sip_fd_t)-1) CloseHandle(ctx->ctlevent);
#else
		close(ctx->ctlpipe[0]);
		close(ctx->ctlpipe[1]);
#endif
	}
	if (ctx->getaddrinfo_source) belle_sip_object_unref(ctx->getaddrinfo_source);

#endif
	if (ctx->ai_list != NULL) {
		bctbx_freeaddrinfo(ctx->ai_list);
		ctx->ai_list = NULL;
	}
#if defined(USE_GETADDRINFO_FALLBACK) || defined(HAVE_MDNS)
	if (ctx->getaddrinfo_ai_list != NULL) {
		bctbx_freeaddrinfo(ctx->getaddrinfo_ai_list);
		ctx->getaddrinfo_ai_list = NULL;
	}
#endif
	if (ctx->name != NULL) {
		belle_sip_free(ctx->name);
		ctx->name = NULL;
	}
#ifdef HAVE_DNS_SERVICE
	if (ctx->base.use_dns_service == TRUE) {
		// deallocate the query and disarm timer on the dns service thread
		if (ctx->dns_service != NULL ||
		    ctx->dns_service_timer != NULL) { // make sure we do not call this if we are in the dns thread
			dispatch_sync(ctx->dns_service_queue, ^{
			  dns_service_deallocate(ctx);
			});
		}
		// Cancel might not deallocate results when they arrive after the cancel, make sure they are
		if (ctx->base.cancelled == TRUE) {
			if (ctx->srv_list != NULL) {
				bctbx_list_free_with_data(ctx->srv_list, belle_sip_object_unref);
				ctx->srv_list = NULL;
			}
		}
		bctbx_mutex_destroy(&ctx->notify_mutex);
		dispatch_release(ctx->dns_service_queue);
	}
#endif /* HAVE_DNS_SERVICE */
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
#ifdef HAVE_MDNS
	if (ctx->srv_prefix) {
		belle_sip_free(ctx->srv_prefix);
		ctx->srv_prefix = NULL;
	}
	if (ctx->srv_name) {
		belle_sip_free(ctx->srv_name);
		ctx->srv_name = NULL;
	}
#endif
}

static void belle_sip_dual_resolver_context_destroy(belle_sip_dual_resolver_context_t *obj) {
	if (obj->a_ctx) {
		belle_sip_object_unref(obj->a_ctx);
		obj->a_ctx = NULL;
	}
	if (obj->aaaa_ctx) {
		belle_sip_object_unref(obj->aaaa_ctx);
		obj->aaaa_ctx = NULL;
	}
	if (obj->a_results) {
		bctbx_freeaddrinfo(obj->a_results);
		obj->a_results = NULL;
	}
	if (obj->aaaa_results) {
		bctbx_freeaddrinfo(obj->aaaa_results);
		obj->aaaa_results = NULL;
	}
	if (obj->name) {
		belle_sip_free(obj->name);
		obj->name = NULL;
	}
}

static void simple_resolver_context_cancel(belle_sip_resolver_context_t *obj) {
	belle_sip_main_loop_remove_source(obj->stack->ml, (belle_sip_source_t *)obj);
}

static void combined_resolver_context_cancel(belle_sip_resolver_context_t *obj) {
	belle_sip_combined_resolver_context_t *ctx = BELLE_SIP_COMBINED_RESOLVER_CONTEXT(obj);
	bctbx_list_t *elem;

	for (elem = ctx->srv_results; elem != NULL; elem = elem->next) {
		belle_sip_dns_srv_t *srv = (belle_sip_dns_srv_t *)elem->data;
		if (srv->a_resolver) {
			belle_sip_resolver_context_cancel(srv->a_resolver);
			belle_sip_object_unref(srv->a_resolver);
			srv->a_resolver = NULL;
		}
	}
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
	belle_sip_dual_resolver_context_t *ctx = BELLE_SIP_DUAL_RESOLVER_CONTEXT(obj);
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
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_resolver_context_t){
    BELLE_SIP_VPTR_INIT(belle_sip_resolver_context_t, belle_sip_source_t, TRUE), (belle_sip_object_destroy_t)NULL, NULL,
    NULL, BELLE_SIP_DEFAULT_BUFSIZE_HINT},
    NULL BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

    BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_simple_resolver_context_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_simple_resolver_context_t){
    {BELLE_SIP_VPTR_INIT(belle_sip_simple_resolver_context_t, belle_sip_resolver_context_t, TRUE),
     (belle_sip_object_destroy_t)belle_sip_simple_resolver_context_destroy, NULL, NULL, BELLE_SIP_DEFAULT_BUFSIZE_HINT},
    simple_resolver_context_cancel,
    simple_resolver_context_notify} BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

    BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_dual_resolver_context_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_dual_resolver_context_t){
    {BELLE_SIP_VPTR_INIT(belle_sip_dual_resolver_context_t, belle_sip_resolver_context_t, TRUE),
     (belle_sip_object_destroy_t)belle_sip_dual_resolver_context_destroy, NULL, NULL, BELLE_SIP_DEFAULT_BUFSIZE_HINT},
    dual_resolver_context_cancel,
    dual_resolver_context_notify} BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

    BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_combined_resolver_context_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_combined_resolver_context_t){
    {BELLE_SIP_VPTR_INIT(belle_sip_combined_resolver_context_t, belle_sip_resolver_context_t, TRUE),
     (belle_sip_object_destroy_t)belle_sip_combined_resolver_context_destroy, NULL, NULL,
     BELLE_SIP_DEFAULT_BUFSIZE_HINT},
    combined_resolver_context_cancel,
    combined_resolver_context_notify} BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

    static char *srv_prefix_from_service_and_transport(const char *service, const char *transport) {
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

static void combined_resolver_context_check_finished(belle_sip_combined_resolver_context_t *obj, uint32_t ttl) {
	belle_sip_list_t *elem;
	struct addrinfo *final = NULL;

	if (ttl < BELLE_SIP_RESOLVER_CONTEXT(obj)->min_ttl) BELLE_SIP_RESOLVER_CONTEXT(obj)->min_ttl = ttl;

	if (!obj->srv_completed && obj->srv_results) {
		unsigned char finished = TRUE;
		for (elem = obj->srv_results; elem != NULL; elem = elem->next) {
			belle_sip_dns_srv_t *srv = (belle_sip_dns_srv_t *)elem->data;
			if (!srv->a_done) {
				finished = FALSE;
				break;
			}
		}
		if (finished) {
			belle_sip_message("All A/AAAA results for combined resolution have arrived.");
			obj->srv_completed = TRUE;
			for (elem = obj->srv_results; elem != NULL; elem = elem->next) {
				belle_sip_dns_srv_t *srv = (belle_sip_dns_srv_t *)elem->data;
				final = ai_list_append(final, srv->a_results);
				srv->dont_free_a_results = TRUE;
			}
			obj->final_results = final;
		}
	}
	if (obj->srv_completed && obj->a_fallback_completed) {
		if (obj->final_results == NULL) {
			/* No SRV results, use a/aaaa fallback */
			obj->final_results = obj->a_fallback_results;
			obj->base.min_ttl = obj->a_fallback_ttl;
			obj->a_fallback_results = NULL;
		}
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(obj));
	}
}

static int combined_resolver_srv_timeout(belle_sip_combined_resolver_context_t *ctx) {
	belle_sip_message("No SRV results while A/AAAA fallback resulted arrived a while ago. Giving up SRV.");
	if (ctx->srv_ctx) {
		belle_sip_resolver_context_cancel(ctx->srv_ctx);
		belle_sip_object_unref(ctx->srv_ctx);
		ctx->srv_ctx = NULL;
	}
	ctx->srv_completed = TRUE;
	combined_resolver_context_check_finished(ctx, BELLE_SIP_RESOLVER_CONTEXT(ctx)->min_ttl);
	return BELLE_SIP_STOP;
}

static void process_a_fallback_result(void *data, belle_sip_resolver_results_t *results) {
	belle_sip_combined_resolver_context_t *ctx = (belle_sip_combined_resolver_context_t *)data;
	ctx->a_fallback_results = results->ai_list;
	results->ai_list = NULL;
	ctx->a_fallback_ttl = results->ttl;
	ctx->a_fallback_completed = TRUE;

	/*
	 * Start a global timer in order to workaround buggy home routers that don't respond to SRV requests.
	 * If A fallback response arrived, SRV shall not take a lot longer.
	 */
	belle_sip_message("resolver[%p]: starting SRV timer since A/AAAA fallback response is received.", ctx);
	belle_sip_socket_source_init((belle_sip_source_t *)ctx, (belle_sip_source_func_t)combined_resolver_srv_timeout, ctx,
	                             -1, BELLE_SIP_EVENT_TIMEOUT, belle_sip_srv_timeout_after_a_received);
	belle_sip_main_loop_add_source(ctx->base.stack->ml, (belle_sip_source_t *)ctx);

	combined_resolver_context_check_finished(ctx, ctx->base.min_ttl);
}

static void process_a_from_srv(void *data, belle_sip_resolver_results_t *results) {
	belle_sip_dns_srv_t *srv = (belle_sip_dns_srv_t *)data;
	srv->a_results = results->ai_list;
	results->ai_list = NULL;
	srv->a_done = TRUE;
	belle_sip_message("A query finished for srv result [%s]", srv->target);
	if (results->ttl < BELLE_SIP_RESOLVER_CONTEXT(srv->root_resolver)->min_ttl)
		BELLE_SIP_RESOLVER_CONTEXT(srv->root_resolver)->min_ttl = results->ttl;
	combined_resolver_context_check_finished(srv->root_resolver, results->ttl);
}

static void srv_resolve_a(belle_sip_combined_resolver_context_t *obj, belle_sip_dns_srv_t *srv) {
	belle_sip_message("Starting A/AAAA query for srv result [%s]", srv->target);
	srv->root_resolver = obj;
	/* take a ref of the srv object because the A resolution may terminate synchronously and destroy the srv
	 object before to store the returned value of belle_sip_stack_resolve_a(). That would lead to an invalid write */
	belle_sip_object_ref(srv);
	srv->a_resolver =
	    belle_sip_stack_resolve_a(obj->base.stack, srv->target, srv->port, obj->family, process_a_from_srv, srv);
	if (srv->a_resolver) {
		belle_sip_object_ref(srv->a_resolver);
	}
	belle_sip_object_unref(srv);
}

static void process_srv_results(void *data, const char *name, belle_sip_list_t *srv_results, uint32_t ttl) {
	belle_sip_combined_resolver_context_t *ctx = (belle_sip_combined_resolver_context_t *)data;
	/*take a ref here, because the A resolution might succeed synchronously and terminate the context before exiting
	 * this function*/

	if (ctx->base.stack->simulate_non_working_srv) {
		belle_sip_list_free_with_data(srv_results, belle_sip_object_unref);
		belle_sip_message("SRV results ignored for testing.");
		return;
	}

	belle_sip_object_ref(ctx);
	if (ttl < BELLE_SIP_RESOLVER_CONTEXT(data)->min_ttl) BELLE_SIP_RESOLVER_CONTEXT(data)->min_ttl = ttl;
	if (srv_results) {
		belle_sip_list_t *elem;

		if (ctx->a_fallback_ctx) {
			ctx->a_fallback_completed = TRUE; /* we don't need a/aaaa fallback anymore.*/
			belle_sip_resolver_context_cancel(ctx->a_fallback_ctx);
			belle_sip_object_unref(ctx->a_fallback_ctx);
			ctx->a_fallback_ctx = NULL;
		}
		/* take a ref of each srv_results because the last A resolution may terminate synchronously
		 and destroy the list before the loop terminate */
		ctx->srv_results = belle_sip_list_copy(srv_results);

		belle_sip_list_for_each(srv_results, (void (*)(void *))belle_sip_object_ref);

		for (elem = srv_results; elem != NULL; elem = elem->next) {
			belle_sip_dns_srv_t *srv = (belle_sip_dns_srv_t *)elem->data;
			srv_resolve_a(ctx, srv);
		}
		srv_results = belle_sip_list_free_with_data(srv_results, belle_sip_object_unref);
		/* Since we have SRV results, we can cancel the srv timeout, and cancel the fallback a/aaaa resolution */
		belle_sip_source_cancel((belle_sip_source_t *)ctx);
	} else {
		/* No SRV result. Possibly notify the a/aaaa fallback if already arrived*/
		ctx->srv_completed = TRUE;
		combined_resolver_context_check_finished(ctx, ctx->base.min_ttl);
	}
	belle_sip_object_unref(ctx);
}

/**
 * Perform combined SRV + A / AAAA resolution.
 **/
belle_sip_resolver_context_t *belle_sip_stack_resolve(belle_sip_stack_t *stack,
                                                      const char *service,
                                                      const char *transport,
                                                      const char *name,
                                                      int port,
                                                      int family,
                                                      belle_sip_resolver_callback_t cb,
                                                      void *data) {
	struct addrinfo *res = bctbx_ip_address_to_addrinfo(family, SOCK_STREAM, name, port);
	if (res == NULL) {
		/* First perform asynchronous DNS SRV query */
		belle_sip_combined_resolver_context_t *ctx = belle_sip_object_new(belle_sip_combined_resolver_context_t);
		belle_sip_resolver_context_init((belle_sip_resolver_context_t *)ctx, stack);
		belle_sip_object_ref(ctx); /*we don't want the object to be destroyed until the end of this function*/
		ctx->cb = cb;
		ctx->cb_data = data;
		ctx->name = belle_sip_strdup(name);
		ctx->port = port;
		belle_sip_object_set_name((belle_sip_object_t *)ctx, ctx->name);
		if (family == 0) family = AF_UNSPEC;
		ctx->family = family;
		/* Take a ref for the entire duration of the DNS procedure, it will be released when it is finished */
		belle_sip_object_ref(ctx);
		ctx->srv_ctx = belle_sip_stack_resolve_srv(stack, service, transport, name, process_srv_results, ctx);
		if (ctx->srv_ctx) {
			belle_sip_object_ref(ctx->srv_ctx);
		}
		/*In parallel and in case of no SRV result, perform A query */
		ctx->a_fallback_ctx = belle_sip_stack_resolve_a(ctx->base.stack, ctx->name, ctx->port, ctx->family,
		                                                process_a_fallback_result, ctx);
		if (ctx->a_fallback_ctx) belle_sip_object_ref(ctx->a_fallback_ctx);
		if (ctx->base.notified) {
			belle_sip_object_unref(ctx);
			return NULL;
		}
		belle_sip_object_unref(ctx);
		return BELLE_SIP_RESOLVER_CONTEXT(ctx);
	} else {
		/* There is no resolution to be done */
		belle_sip_resolver_results_t *results = belle_sip_resolver_results_create(name, res, NULL, UINT32_MAX);
		cb(data, results);
		belle_sip_object_unref(results);
		return NULL;
	}
}

static belle_sip_resolver_context_t *belle_sip_stack_resolve_single(belle_sip_stack_t *stack,
                                                                    const char *name,
                                                                    int port,
                                                                    int family,
                                                                    int flags,
                                                                    belle_sip_resolver_callback_t cb,
                                                                    void *data) {
	/* Then perform asynchronous DNS A or AAAA query */
	belle_sip_simple_resolver_context_t *ctx = belle_sip_object_new(belle_sip_simple_resolver_context_t);
	belle_sip_resolver_context_init((belle_sip_resolver_context_t *)ctx, stack);
	ctx->cb_data = data;
	ctx->cb = cb;
	ctx->name = belle_sip_strdup(name);
	ctx->port = port;
	ctx->flags = flags;
	belle_sip_object_set_name((belle_sip_object_t *)ctx, ctx->name);
	/* Take a ref for the entire duration of the DNS procedure, it will be released when it is finished */
	belle_sip_object_ref(ctx);
	if (family == 0) family = AF_UNSPEC;
	ctx->family = family;
#ifdef HAVE_DNS_SERVICE
	ctx->dns_service_queue = stack->dns_service_queue;
	dispatch_retain(ctx->dns_service_queue); // take a ref on the dispatch queue
	ctx->dns_service_type = (ctx->family == AF_INET6) ? kDNSServiceType_AAAA : kDNSServiceType_A;
	if (stack->use_dns_service == TRUE) {
		bctbx_mutex_init(&ctx->notify_mutex, NULL);
	}
#endif /* HAVE_DNS_SERVICE */
	ctx->type = (ctx->family == AF_INET6) ? DNS_T_AAAA : DNS_T_A;
#if (defined(USE_GETADDRINFO_FALLBACK) || defined(HAVE_MDNS)) && defined(_WIN32)
	ctx->ctlevent = (belle_sip_fd_t)-1;
#endif
	return (belle_sip_resolver_context_t *)resolver_start_query(ctx);
}

static uint8_t belle_sip_resolver_context_can_be_cancelled(belle_sip_resolver_context_t *obj) {
	return ((obj->cancelled == TRUE) || (obj->notified == TRUE)) ? FALSE : TRUE;
}

#define belle_sip_resolver_context_can_be_notified(obj) belle_sip_resolver_context_can_be_cancelled(obj)

static void dual_resolver_context_check_finished(belle_sip_dual_resolver_context_t *ctx) {
	if (belle_sip_resolver_context_can_be_notified(BELLE_SIP_RESOLVER_CONTEXT(ctx)) && (ctx->a_notified == TRUE) &&
	    (ctx->aaaa_notified == TRUE)) {
		belle_sip_resolver_context_notify(BELLE_SIP_RESOLVER_CONTEXT(ctx));
	}
}

static int dual_resolver_aaaa_timeout(void *data, unsigned int event) {
	belle_sip_dual_resolver_context_t *ctx = BELLE_SIP_DUAL_RESOLVER_CONTEXT(data);

	/*
	 * It is too late to receive the AAAA query, so give up, and notify the A result we have already.
	 */
	if (ctx->aaaa_ctx) {
		belle_sip_resolver_context_cancel(ctx->aaaa_ctx);
		belle_sip_object_unref(ctx->aaaa_ctx);
		ctx->aaaa_ctx = NULL;
	}
	ctx->aaaa_notified = TRUE;
	dual_resolver_context_check_finished(ctx);
	return BELLE_SIP_STOP;
}

static void on_ipv4_results(void *data, belle_sip_resolver_results_t *results) {
	belle_sip_dual_resolver_context_t *ctx = BELLE_SIP_DUAL_RESOLVER_CONTEXT(data);

	ctx->a_results = results->ai_list;
	results->ai_list = NULL;
	ctx->a_notified = TRUE;

	if (!ctx->aaaa_notified && ctx->a_results && belle_sip_aaaa_timeout_after_a_received > 0) {
		/*
		 * Start a global timer in order to workaround buggy home routers that don't respond to AAAA requests when there
		 * is no corresponding AAAA record for the queried domain. It is only started if we have a A result.
		 */
		belle_sip_message("resolver[%p]: starting aaaa timeout since A response is received.", ctx);
		belle_sip_socket_source_init((belle_sip_source_t *)ctx, (belle_sip_source_func_t)dual_resolver_aaaa_timeout,
		                             ctx, -1, BELLE_SIP_EVENT_TIMEOUT, belle_sip_aaaa_timeout_after_a_received);
		belle_sip_main_loop_add_source(ctx->base.stack->ml, (belle_sip_source_t *)ctx);
	}
	dual_resolver_context_check_finished(ctx);
}

static void on_ipv6_results(void *data, belle_sip_resolver_results_t *results) {
	belle_sip_dual_resolver_context_t *ctx = BELLE_SIP_DUAL_RESOLVER_CONTEXT(data);
	ctx->aaaa_results = results->ai_list;
	results->ai_list = NULL;
	ctx->aaaa_notified = TRUE;

	if (ctx->a_notified) {
		/* Cancel the aaaa timeout.*/
		belle_sip_source_cancel((belle_sip_source_t *)ctx);
	}
	dual_resolver_context_check_finished(ctx);
}

static belle_sip_resolver_context_t *belle_sip_stack_resolve_dual(
    belle_sip_stack_t *stack, const char *name, int port, belle_sip_resolver_callback_t cb, void *data) {
	/* Then perform asynchronous DNS A or AAAA query */
	belle_sip_dual_resolver_context_t *ctx = belle_sip_object_new(belle_sip_dual_resolver_context_t);
	belle_sip_resolver_context_init((belle_sip_resolver_context_t *)ctx, stack);
	belle_sip_object_ref(ctx); /*we don't want the object to be destroyed until the end of this function*/
	ctx->cb_data = data;
	ctx->cb = cb;
	ctx->name = belle_sip_strdup(name);
	belle_sip_object_set_name((belle_sip_object_t *)ctx, ctx->name);
	/* Take a ref for the entire duration of the DNS procedure, it will be released when it is finished */
	belle_sip_object_ref(ctx);
	ctx->a_ctx = belle_sip_stack_resolve_single(stack, name, port, AF_INET, AI_V4MAPPED, on_ipv4_results, ctx);
	if (ctx->a_ctx) belle_sip_object_ref(ctx->a_ctx);
	ctx->aaaa_ctx = belle_sip_stack_resolve_single(stack, name, port, AF_INET6, 0, on_ipv6_results, ctx);
	if (ctx->aaaa_ctx) belle_sip_object_ref(ctx->aaaa_ctx);
	if (ctx->base.notified) {
		/* All results were found synchronously */
		belle_sip_object_unref(ctx);
		ctx = NULL;
	} else belle_sip_object_unref(ctx);
	return BELLE_SIP_RESOLVER_CONTEXT(ctx);
}

belle_sip_resolver_context_t *belle_sip_stack_resolve_a(
    belle_sip_stack_t *stack, const char *name, int port, int family, belle_sip_resolver_callback_t cb, void *data) {
	struct addrinfo *res = bctbx_ip_address_to_addrinfo(family, SOCK_STREAM, name, port);
	if (res == NULL) {
		switch (family) {
			case AF_UNSPEC:
				family = AF_INET6;
				BCTBX_NO_BREAK; /*intentionally no break*/
			case AF_INET6:
				return belle_sip_stack_resolve_dual(stack, name, port, cb, data);
				break;
			case AF_INET:
				return belle_sip_stack_resolve_single(stack, name, port, AF_INET, 0, cb, data);
				break;
			default:
				belle_sip_error("belle_sip_stack_resolve_a(): unsupported address family [%i]", family);
		}
	} else {
		/* There is no resolution to be done */
		belle_sip_resolver_results_t *results = belle_sip_resolver_results_create(name, res, NULL, UINT32_MAX);
		cb(data, results);
		belle_sip_object_unref(results);
	}
	return NULL;
}

belle_sip_resolver_context_t *belle_sip_stack_resolve_srv(belle_sip_stack_t *stack,
                                                          const char *service,
                                                          const char *transport,
                                                          const char *name,
                                                          belle_sip_resolver_srv_callback_t cb,
                                                          void *data) {
	belle_sip_simple_resolver_context_t *ctx = belle_sip_object_new(belle_sip_simple_resolver_context_t);
	char *srv_prefix = srv_prefix_from_service_and_transport(service, transport);
	belle_sip_resolver_context_init((belle_sip_resolver_context_t *)ctx, stack);
	ctx->srv_cb_data = data;
	ctx->srv_cb = cb;
#ifdef HAVE_MDNS
	ctx->srv_prefix = belle_sip_strdup(srv_prefix);
	ctx->srv_name = belle_sip_strdup(name);
	ctx->resolving = 0;
	ctx->browse_finished = FALSE;
#endif
	ctx->name = belle_sip_concat(srv_prefix, name, NULL);
#ifdef HAVE_DNS_SERVICE
	ctx->dns_service_queue = stack->dns_service_queue;
	dispatch_retain(ctx->dns_service_queue); // take a ref on the dispatch queue
	ctx->dns_service_type = kDNSServiceType_SRV;
	if (stack->use_dns_service == TRUE) {
		bctbx_mutex_init(&ctx->notify_mutex, NULL);
	}
#endif /* HAVE_DNS_SERVICE */
	ctx->type = DNS_T_SRV;
	belle_sip_object_set_name((belle_sip_object_t *)ctx, ctx->name);
	/* Take a ref for the entire duration of the DNS procedure, it will be released when it is finished */
	belle_sip_object_ref(ctx);
	belle_sip_free(srv_prefix);
	return (belle_sip_resolver_context_t *)resolver_start_query(ctx);
}

int belle_sip_resolver_context_cancel(belle_sip_resolver_context_t *obj) {
	int ret = -1;
#ifdef HAVE_DNS_SERVICE
	bctbx_mutex_t *notify_mutex = NULL;
	if ((obj->use_dns_service == TRUE) &&
	    (BELLE_SIP_OBJECT_IS_INSTANCE_OF(obj, belle_sip_simple_resolver_context_t) == TRUE)) {
		notify_mutex = &(BELLE_SIP_SIMPLE_RESOLVER_CONTEXT(obj)->notify_mutex);
		bctbx_mutex_lock(notify_mutex);
	}
#endif /* HAVE_DNS_SERVICE */
	if (belle_sip_resolver_context_can_be_cancelled(obj)) {
		obj->cancelled = TRUE;
		BELLE_SIP_OBJECT_VPTR(obj, belle_sip_resolver_context_t)->cancel(obj);
#ifdef HAVE_DNS_SERVICE
		if (notify_mutex != NULL) {
			bctbx_mutex_unlock(notify_mutex);
		}
#endif /* HAVE_DNS_SERVICE */
		belle_sip_object_unref(obj);
		ret = 0;
	}
#ifdef HAVE_DNS_SERVICE
	else {
		if (notify_mutex != NULL) {
			bctbx_mutex_unlock(notify_mutex);
		}
	}
#endif /* HAVE_DNS_SERVICE */
	return ret;
}

void belle_sip_resolver_context_notify(belle_sip_resolver_context_t *obj) {
#ifdef HAVE_DNS_SERVICE
	/* When using DNS Service, a ref is taken in simple resolver to ensure
	 * the context is always present if a notify rises after a cancel
	 * Check before performing the actual notify if we must unref it
	 * as the notify may destroy the object (when the resolver is not simple
	 * and unref after the notify if needed */
	bool_t unref_me = ((obj->use_dns_service == TRUE) &&
	                   (BELLE_SIP_OBJECT_IS_INSTANCE_OF(obj, belle_sip_simple_resolver_context_t) == TRUE));
#endif /* HAVE_DNS_SERVICE */
	if (belle_sip_resolver_context_can_be_notified(obj)) {
		obj->notified = TRUE;
		BELLE_SIP_OBJECT_VPTR(obj, belle_sip_resolver_context_t)->notify(obj);
		belle_sip_object_unref(obj);
	}
#ifdef HAVE_DNS_SERVICE
	if (unref_me == TRUE) {
		belle_sip_object_unref(obj);
	}
#endif /* HAVE_DNS_SERVICE */
}

/*
This function does the connect() method to get local ip address suitable to reach a given destination.
It works on all platform except for windows using ipv6 sockets. TODO: find a workaround for win32+ipv6 socket
*/
int belle_sip_get_src_addr_for(
    const struct sockaddr *dest, socklen_t destlen, struct sockaddr *src, socklen_t *srclen, int local_port) {
	int af_type = dest->sa_family;
	int sock = (int)bctbx_socket(af_type, SOCK_DGRAM, IPPROTO_UDP);
	int ret = 0;

	if (sock == (belle_sip_socket_t)-1) {
		if (af_type == AF_INET) {
			belle_sip_fatal("Could not create socket: %s", belle_sip_get_socket_error_string());
		}
		goto fail;
	}

	if (af_type == AF_INET6 && (IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)dest)->sin6_addr))) {
		/*this is actually required only for windows, who is unable to provide an ipv4 mapped local address if the
		remote is ipv4 mapped, and unable to provide a correct local address if the remote address is true ipv6 address
		when in dual stack mode*/
		belle_sip_socket_enable_dual_stack(sock);
	}
	if (bctbx_connect(sock, dest, destlen) == -1) {
		// if (connect(sock,dest,destlen)==-1){
		ret = -get_socket_error();
		belle_sip_error("belle_sip_get_src_addr_for: bctbx_connect() failed: %s",
		                belle_sip_get_socket_error_string_from_code(-ret));
		goto fail;
	}
	if (bctbx_getsockname(sock, src, srclen) == -1) {
		ret = -get_socket_error();
		belle_sip_error("belle_sip_get_src_addr_for: bctbx_getsockname() failed: %s",
		                belle_sip_get_socket_error_string_from_code(-ret));
		goto fail;
	}

	if (af_type == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)src;
		sin6->sin6_port = htons(local_port);
	} else {
		struct sockaddr_in *sin = (struct sockaddr_in *)src;
		sin->sin_port = htons(local_port);
	}

	belle_sip_close_socket(sock);
	return ret;
fail : {
	struct addrinfo *res =
	    bctbx_ip_address_to_addrinfo(af_type, SOCK_STREAM, af_type == AF_INET ? "127.0.0.1" : "::1", local_port);
	if (res != NULL) {
		memcpy(src, res->ai_addr, MIN((size_t)*srclen, res->ai_addrlen));
		*srclen = (socklen_t)res->ai_addrlen;
		bctbx_freeaddrinfo(res);
	} else {
		if (af_type == AF_INET)
			belle_sip_fatal("belle_sip_get_src_addr_for(): belle_sip_ip_address_to_addrinfo() failed");
	}
}
	if (sock != (belle_sip_socket_t)-1) belle_sip_close_socket(sock);
	return ret;
}
