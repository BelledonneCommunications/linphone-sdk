/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010-2013  Belledonne Communications SARL

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

#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"
#include "belle_sip_tester.h"


#define IPV4_SIP_DOMAIN		"sip.linphone.org"
#define IPV4_SIP_IP		"91.121.209.194"
#define IPV4_CNAME		"stun.linphone.org"
#define IPV4_CNAME_IP		"37.59.51.72"
#define IPV4_SIP_BAD_DOMAIN	"dummy.linphone.org"
#define IPV4_MULTIRES_DOMAIN	"yahoo.fr"

/* sip2.linphone.org has an IPv6 and an IPv4 IP*/
#define IPV6_SIP_DOMAIN		"sip2.linphone.org"
#define IPV6_SIP_IP		"2001:41d0:2:14b0::1"
#define IPV6_SIP_IPV4		"88.191.250.2"

#define SRV_DOMAIN		"linphone.org"
#define SIP_PORT		5060

typedef struct endpoint {
	belle_sip_stack_t* stack;
	belle_sip_resolver_context_t *resolver_ctx;
	int resolve_done;
	int resolve_ko;
	struct addrinfo *ai_list;
	belle_sip_list_t *srv_list; /**< List of struct dns_srv pointers */
} endpoint_t;

static unsigned int  wait_for(belle_sip_stack_t *stack, int *current_value, int expected_value, int timeout) {
#define ITER 100
	uint64_t begin, end;
	begin = belle_sip_time_ms();
	end = begin + timeout;
	while ((*current_value != expected_value) && (belle_sip_time_ms() < end)) {
		if (stack) belle_sip_stack_sleep(stack, ITER);
	}
	if (*current_value != expected_value) return FALSE;
	else return TRUE;
}

static endpoint_t* create_endpoint(void) {
	endpoint_t* endpoint;
	endpoint = belle_sip_new0(endpoint_t);
	endpoint->stack = belle_sip_stack_new(NULL);
	return endpoint;
}

static void reset_endpoint(endpoint_t *endpoint) {
	endpoint->resolver_ctx = 0;
	endpoint->resolve_done = 0;
	endpoint->resolve_ko = 0;
	if (endpoint->ai_list != NULL) {
		belle_sip_freeaddrinfo(endpoint->ai_list);
		endpoint->ai_list = NULL;
	}
	if (endpoint->srv_list != NULL) {
		belle_sip_list_free_with_data(endpoint->srv_list, belle_sip_object_unref);
		endpoint->srv_list = NULL;
	}
}

static void destroy_endpoint(endpoint_t *endpoint) {
	reset_endpoint(endpoint);
	belle_sip_object_unref(endpoint->stack);
	belle_sip_free(endpoint);
	belle_sip_uninit_sockets();
}

static void a_resolve_done(void *data, const char *name, struct addrinfo *ai_list) {
	endpoint_t *client = (endpoint_t *)data;
	BELLESIP_UNUSED(name);
	client->resolve_done = 1;
	if (ai_list) {
		client->ai_list = ai_list;
		client->resolve_done = 1;
	} else
		client->resolve_ko = 1;
}

static void srv_resolve_done(void *data, const char *name, belle_sip_list_t *srv_list) {
	endpoint_t *client = (endpoint_t *)data;
	BELLESIP_UNUSED(name);
	client->resolve_done = 1;
	if (srv_list) {
		client->srv_list = srv_list;
		client->resolve_done = 1;
	} else
		client->resolve_ko = 1;
}

/* Successful IPv4 A query */
static void ipv4_a_query(void) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = belle_sip_ip_address_to_addrinfo(AF_INET, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			belle_sip_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}

/* Successful IPv4 A query to a CNAME*/
/*This tests the recursion of dns.c*/
static void ipv4_cname_a_query(void) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, IPV4_CNAME, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = belle_sip_ip_address_to_addrinfo(AF_INET, IPV4_CNAME_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			belle_sip_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}

static void local_query(void) {
	int timeout;
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, "localhost", SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
	}
	destroy_endpoint(client);
}


/* Successful IPv4 A query with no result */
static void ipv4_a_query_no_result(void) {
	int timeout;
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, IPV4_SIP_BAD_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_EQUAL(client->ai_list, NULL);

	destroy_endpoint(client);
}

/* IPv4 A query send failure */
static void ipv4_a_query_send_failure(void) {
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	belle_sip_stack_set_resolver_send_error(client->stack, -1);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NULL(client->resolver_ctx);
	belle_sip_stack_set_resolver_send_error(client->stack, 0);

	destroy_endpoint(client);
}

/* IPv4 A query timeout */
static void ipv4_a_query_timeout(void) {

	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	belle_sip_stack_set_dns_timeout(client->stack, 0);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, "toto.com", SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 2000));
	BC_ASSERT_PTR_EQUAL(client->ai_list, NULL);
	BC_ASSERT_EQUAL(client->resolve_ko,1, int, "%d");
	destroy_endpoint(client);
}

/* Successful IPv4 A query with multiple results */
static void ipv4_a_query_multiple_results(void) {
	int timeout;
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, IPV4_MULTIRES_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		BC_ASSERT_PTR_NOT_NULL(client->ai_list->ai_next);
	}

	destroy_endpoint(client);
}

static void ipv4_a_query_with_v4mapped_results(void) {
	int timeout;
	endpoint_t *client;

	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}

	client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET6, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);

	destroy_endpoint(client);
}


/* Successful IPv6 AAAA query */
static void ipv6_aaaa_query(void) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client;

	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}

	client = create_endpoint();
	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, IPV6_SIP_DOMAIN, SIP_PORT, AF_INET6, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct addrinfo *next;
		struct sockaddr_in6 *sock_in6 = (struct sockaddr_in6 *)client->ai_list->ai_addr;
		int ntohsi = ntohs(sock_in6->sin6_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		/*the IPv6 address shall return first, and must be a real ipv6 address*/
		BC_ASSERT_TRUE(client->ai_list->ai_family==AF_INET6);
		BC_ASSERT_FALSE(IN6_IS_ADDR_V4MAPPED(&sock_in6->sin6_addr));
		ai = belle_sip_ip_address_to_addrinfo(AF_INET6, IPV6_SIP_IP, SIP_PORT);
		BC_ASSERT_PTR_NOT_NULL(ai);
		if (ai) {
			struct in6_addr *ipv6_address = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
			int i;
			for (i = 0; i < 8; i++) {
				BC_ASSERT_EQUAL(sock_in6->sin6_addr.s6_addr[i], ipv6_address->s6_addr[i], int, "%d");
			}
			belle_sip_freeaddrinfo(ai);
		}
		next=client->ai_list->ai_next;
		BC_ASSERT_PTR_NOT_NULL(next);
		if (next){
			int ntohsi = ntohs(sock_in6->sin6_port);
			sock_in6 = (struct sockaddr_in6 *)next->ai_addr;
			BC_ASSERT_TRUE(next->ai_family==AF_INET6);
			BC_ASSERT_TRUE(IN6_IS_ADDR_V4MAPPED(&sock_in6->sin6_addr));
			BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
			ai = belle_sip_ip_address_to_addrinfo(AF_INET6, IPV6_SIP_IPV4, SIP_PORT);
			BC_ASSERT_PTR_NOT_NULL(ai);
			if (ai) {
				struct in6_addr *ipv6_address = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
				int i;
				for (i = 0; i < 8; i++) {
					BC_ASSERT_EQUAL(sock_in6->sin6_addr.s6_addr[i], ipv6_address->s6_addr[i], int, "%d");
				}
				belle_sip_freeaddrinfo(ai);
			}
		}
	}
	destroy_endpoint(client);
}

/* Successful SRV query */
static void srv_query(void) {
	int timeout;
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve_srv(client->stack, "udp", SRV_DOMAIN, srv_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->srv_list);
	BC_ASSERT_NOT_EQUAL(belle_sip_list_size(client->srv_list), 0,int,"%d");
	if (client->srv_list && (belle_sip_list_size(client->srv_list) > 0)) {
		belle_sip_dns_srv_t *result_srv = belle_sip_list_nth_data(client->srv_list, 0);
		BC_ASSERT_EQUAL(belle_sip_dns_srv_get_port(result_srv), SIP_PORT, int, "%d");
	}

	destroy_endpoint(client);
}

/* Successful SRV + A or AAAA queries */
static void srv_a_query(void) {
	int timeout;
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve(client->stack, "udp", SRV_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);

	destroy_endpoint(client);
}

/* Successful SRV query with no result + A query */
static void srv_a_query_no_srv_result(void) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve(client->stack, "udp", IPV4_CNAME, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = belle_sip_ip_address_to_addrinfo(AF_INET, IPV4_CNAME_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			belle_sip_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}

static void local_full_query(void) {
	int timeout;
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx = belle_sip_stack_resolve(client->stack, "tcp", "localhost", SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
	}
	destroy_endpoint(client);
}

/* No query needed because already resolved */
static void no_query_needed(void) {
	struct addrinfo *ai;
	endpoint_t *client = create_endpoint();

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	client->resolver_ctx = belle_sip_stack_resolve(client->stack, "udp", IPV4_SIP_IP, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(client->resolve_done);
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = belle_sip_ip_address_to_addrinfo(AF_INET, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			belle_sip_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}

static void set_custom_resolv_conf(belle_sip_stack_t *stack, const char *ns[3]){
	char *resolv_file = bc_tester_file("tmp_resolv.conf");
	FILE *f=fopen(resolv_file,"w");
	BC_ASSERT_PTR_NOT_NULL(f);
	if (f){
		int i;
		for (i=0; i<3; ++i){
			if (ns[i]!=NULL){
				fprintf(f,"nameserver %s\n",ns[i]);
			}else break;
		}
		fclose(f);
	}
	belle_sip_stack_set_dns_resolv_conf_file(stack, resolv_file);
	free(resolv_file);
}

static void dns_fallback(void) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client = create_endpoint();
	const char *nameservers[]={
		"94.23.19.176", /*linphone.org ; this is not a name server, it will not respond*/
		"8.8.8.8", /* public nameserver, should work*/
		NULL
	};

	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	set_custom_resolv_conf(client->stack,nameservers);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = belle_sip_ip_address_to_addrinfo(AF_INET, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			belle_sip_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}

static void ipv6_dns_server(void) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client;
	const char *nameservers[]={
		"2001:4860:4860::8888",
		NULL
	};

	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}

	client = create_endpoint();
	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	set_custom_resolv_conf(client->stack,nameservers);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = belle_sip_ip_address_to_addrinfo(AF_INET, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			belle_sip_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}

static void ipv4_and_ipv6_dns_server(void) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client;
	const char *nameservers[]={
		"8.8.8.8",
		"2a01:e00::2",
		NULL
	};

	if (!belle_sip_tester_ipv6_available()){
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	client = create_endpoint();
	BC_ASSERT_PTR_NOT_NULL_FATAL(client);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	set_custom_resolv_conf(client->stack,nameservers);
	client->resolver_ctx = belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = belle_sip_ip_address_to_addrinfo(AF_INET, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			belle_sip_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}

test_t resolver_tests[] = {
	{ "A query (IPv4)", ipv4_a_query },
	{ "A query (IPv4) with CNAME", ipv4_cname_a_query },
	{ "A query (IPv4) with no result", ipv4_a_query_no_result },
	{ "A query (IPv4) with send failure", ipv4_a_query_send_failure },
	{ "A query (IPv4) with timeout", ipv4_a_query_timeout },
	{ "A query (IPv4) with multiple results", ipv4_a_query_multiple_results },
	{ "A query (IPv4) with AI_V4MAPPED results", ipv4_a_query_with_v4mapped_results },
	{ "Local query", local_query },
	{ "AAAA query (IPv6)", ipv6_aaaa_query },
	{ "SRV query", srv_query },
	{ "SRV + A query", srv_a_query },
	{ "SRV + A query with no SRV result", srv_a_query_no_srv_result },
	{ "Local SRV+A query", local_full_query },
	{ "No query needed", no_query_needed },
	{ "DNS fallback", dns_fallback },
	{ "IPv6 DNS server", ipv6_dns_server },
	{ "IPv4 and v6 DNS servers", ipv4_and_ipv6_dns_server }
};

test_suite_t resolver_test_suite = {"Resolver", NULL, NULL, belle_sip_tester_before_each, belle_sip_tester_after_each,
									sizeof(resolver_tests) / sizeof(resolver_tests[0]), resolver_tests};
