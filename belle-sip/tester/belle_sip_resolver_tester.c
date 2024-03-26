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

#include "bctoolbox/tester.h"
#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"
#include "belle_sip_tester.h"

#define IPV4_SIP_DOMAIN "sip.linphone.org"
#define IPV4_SIP_IP "51.89.71.129"
#define IPV4_CNAME "stun.linphone.org"
#define IPV4_CNAME_IP "51.89.71.129"
#define IPV4_SIP_BAD_DOMAIN "dummy.linphone.org"
#define IPV4_MULTIRES_DOMAIN "yahoo.fr"

/* sip2.linphone.org has an IPv6 and an IPv4 IP*/
#define IPV6_SIP_DOMAIN "sip2.linphone.org"
#define IPV6_SIP_IP "2001:41d0:2:14b0::1"
#define IPV6_SIP_IPV4 "88.191.250.2"

#define SRV_DOMAIN "linphone.org"
#define SIP_PORT 5060

typedef struct endpoint {
	belle_sip_stack_t *stack;
	belle_sip_resolver_context_t *resolver_ctx;
	int resolve_done;
	int resolve_ko;
	bctbx_list_t *srv_list;
	belle_sip_resolver_results_t *results;
	const struct addrinfo *ai_list;
} endpoint_t;

static unsigned int wait_for(belle_sip_stack_t *stack, int *current_value, int expected_value, int timeout) {
#define ITER 1
	uint64_t begin, end;
	begin = belle_sip_time_ms();
	end = begin + timeout;
	while ((*current_value != expected_value) && (belle_sip_time_ms() < end)) {
		if (stack) belle_sip_stack_sleep(stack, ITER);
	}
	if (*current_value != expected_value) return FALSE;
	else return TRUE;
}

static void destroy_endpoint_stack(endpoint_t *endpoint) {
	if (endpoint->stack != NULL) {
		belle_sip_stack_sleep(endpoint->stack, 1);
		belle_sip_object_unref(endpoint->stack);
		endpoint->stack = NULL;
	}
}

static void init_endpoint_stack(endpoint_t *endpoint) {
	destroy_endpoint_stack(endpoint);
	endpoint->stack = belle_sip_stack_new(NULL);
}

static endpoint_t *create_endpoint(void) {
	endpoint_t *endpoint;
	endpoint = belle_sip_new0(endpoint_t);
	init_endpoint_stack(endpoint);
	return endpoint;
}

static void reset_endpoint(endpoint_t *endpoint) {
	endpoint->resolver_ctx = 0;
	endpoint->resolve_done = 0;
	endpoint->resolve_ko = 0;
	if (endpoint->results) {
		belle_sip_object_unref(endpoint->results);
		endpoint->results = NULL;
	}
	endpoint->ai_list = NULL;
	if (endpoint->srv_list) {
		bctbx_list_free_with_data(endpoint->srv_list, belle_sip_object_unref);
		endpoint->srv_list = NULL;
	}
}

static void destroy_endpoint(endpoint_t *endpoint) {
	reset_endpoint(endpoint);
	destroy_endpoint_stack(endpoint);
	belle_sip_free(endpoint);
	belle_sip_uninit_sockets();
}

static void a_resolve_done(void *data, belle_sip_resolver_results_t *results) {
	endpoint_t *client = (endpoint_t *)data;

	client->resolve_done = 1;
	belle_sip_object_ref(results);
	client->results = results;
	if (belle_sip_resolver_results_get_addrinfos(results)) {
		client->ai_list = belle_sip_resolver_results_get_addrinfos(results);
		client->resolve_done = 1;
	} else {
		client->resolve_ko = 1;
	}
}

static void srv_resolve_done(void *data, const char *name, belle_sip_list_t *srv_list, uint32_t ttl) {
	endpoint_t *client = (endpoint_t *)data;
	BELLESIP_UNUSED(name);
	client->resolve_done = 1;
	if (srv_list) {
		client->srv_list = srv_list;
		client->resolve_done = 1;
	} else client->resolve_ko = 1;
}

/* Successful IPv4 A query */
static void ipv4_a_query_engine(unsigned char dns_engine) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = bctbx_ip_address_to_addrinfo(AF_INET, SOCK_STREAM, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			bctbx_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}
static void ipv4_a_query(void) {
	ipv4_a_query_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	ipv4_a_query_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

/* Cancel A query */
static void a_query_cancelled_engine(unsigned char dns_engine, bool_t destroy_stack) {
	int timeout;
	endpoint_t *client = create_endpoint();
	int i, dummy = 0;

	/* First run a successfull query, to the result is in the system cache */
	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);

	/* Then run 50 times a query and cancel it(just to try some race conditions) */
	for (i = 0; i < 50; i++) {
		reset_endpoint(client);
		client->resolver_ctx =
		    belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
		BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
		/* cancel the query (do it immediately otherwise the result might arrive before the cancel) */
		belle_sip_resolver_context_cancel(client->resolver_ctx);
		BC_ASSERT_PTR_NULL(client->ai_list);
		if (destroy_stack == TRUE) {
			destroy_endpoint_stack(client);
			bctbx_sleep_ms(50);
			init_endpoint_stack(client);
			belle_sip_stack_set_dns_engine(client->stack, dns_engine);
		} else {
			/* wait a little (query result is already in the system cache so it shall be fast) to give the time a to
			 * potential answer to reach cancelled request */
			wait_for(client->stack, &dummy, 1, 50);
		}

		BC_ASSERT_TRUE(client->resolve_done == 0);
		BC_ASSERT_PTR_NULL(client->ai_list);
	}

	destroy_endpoint(client);
}

static void a_query_cancelled(void) {
	a_query_cancelled_engine(BELLE_SIP_DNS_DNS_C, FALSE);
	a_query_cancelled_engine(BELLE_SIP_DNS_DNS_C, TRUE);
#ifdef HAVE_DNS_SERVICE
	belle_sip_object_inhibit_leak_detector(TRUE); // leak detector is not thread safe
	a_query_cancelled_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE, FALSE);
	a_query_cancelled_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE, TRUE);
	belle_sip_object_inhibit_leak_detector(FALSE);
#endif /* HAVE_DNS_SERVICE */
}

/* Cancel a SRV query */
static void srv_query_cancelled_engine(unsigned char dns_engine, bool_t destroy_stack) {
	int timeout;
	endpoint_t *client = create_endpoint();
	int i, dummy = 0;

	/* First run a successfull query, to the result is in the system cache */
	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_srv(client->stack, "sip", "udp", SRV_DOMAIN, srv_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->srv_list);
	BC_ASSERT_NOT_EQUAL((unsigned int)belle_sip_list_size(client->srv_list), 0, unsigned int, "%u");
	if (client->srv_list && (belle_sip_list_size(client->srv_list) > 0)) {
		belle_sip_dns_srv_t *result_srv = belle_sip_list_nth_data(client->srv_list, 0);
		BC_ASSERT_EQUAL(belle_sip_dns_srv_get_port(result_srv), SIP_PORT, int, "%d");
	}

	/* Then run 50 times a query and cancel it(just to try some race conditions) */
	for (i = 0; i < 50; i++) {
		reset_endpoint(client);
		client->resolver_ctx =
		    belle_sip_stack_resolve_srv(client->stack, "sip", "udp", SRV_DOMAIN, srv_resolve_done, client);
		BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
		/* cancel the query (do it immediately otherwise the result might arrive before the cancel) */
		belle_sip_resolver_context_cancel(client->resolver_ctx);
		BC_ASSERT_TRUE(client->resolve_done == 0);
		BC_ASSERT_PTR_NULL(client->srv_list);

		if (destroy_stack == TRUE) {
			destroy_endpoint_stack(client);
			bctbx_sleep_ms(50);
			init_endpoint_stack(client);
			belle_sip_stack_set_dns_engine(client->stack, dns_engine);
		} else {
			/* wait a little (query result is already in the system cache so it shall be fast) to give the time a to
			 * potential answer to reach cancelled request */
			wait_for(client->stack, &dummy, 1, 50);
		}
		BC_ASSERT_EQUAL((unsigned int)belle_sip_list_size(client->srv_list), 0, unsigned int, "%u");
	}

	destroy_endpoint(client);
}
static void srv_query_cancelled(void) {
	srv_query_cancelled_engine(BELLE_SIP_DNS_DNS_C, FALSE);
	srv_query_cancelled_engine(BELLE_SIP_DNS_DNS_C, TRUE);
#ifdef HAVE_DNS_SERVICE
	belle_sip_object_inhibit_leak_detector(TRUE); // leak detector is not thread safe
	srv_query_cancelled_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE, FALSE);
	srv_query_cancelled_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE, TRUE);
	belle_sip_object_inhibit_leak_detector(FALSE);
#endif /* HAVE_DNS_SERVICE */
}

/* Cancel a SRV + A or AAAA query */
static void srv_a_query_cancelled_engine(unsigned char dns_engine, bool_t destroy_stack) {
	int timeout;
	endpoint_t *client = create_endpoint();
	int i, dummy = 0;

	/* First run a successfull query, to the result is in the system cache */
	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve(client->stack, "sip", "udp", SRV_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);

	/* Then run 50 times a query and cancel it(just to try some race conditions) */
	for (i = 0; i < 50; i++) {
		reset_endpoint(client);
		client->resolver_ctx =
		    belle_sip_stack_resolve(client->stack, "sip", "udp", SRV_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
		BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
		/* cancel the query (do it immediately otherwise the result might arrive before the cancel) */
		belle_sip_resolver_context_cancel(client->resolver_ctx);
		BC_ASSERT_TRUE(client->resolve_done == 0);
		BC_ASSERT_PTR_NULL(client->ai_list);

		/* wait a little (query result is already in the system cache so it shall be fast) to give the time a to
		 * potential answer to reach cancelled request */
		if (destroy_stack == TRUE) {
			destroy_endpoint_stack(client);
			bctbx_sleep_ms(50);
			init_endpoint_stack(client);
			belle_sip_stack_set_dns_engine(client->stack, dns_engine);
		} else {
			/* wait a little (query result is already in the system cache so it shall be fast) to give the time a to
			 * potential answer to reach cancelled request */
			wait_for(client->stack, &dummy, 1, 50);
		}
		BC_ASSERT_PTR_NULL(client->ai_list);
	}

	destroy_endpoint(client);
}
static void srv_a_query_cancelled(void) {
	srv_a_query_cancelled_engine(BELLE_SIP_DNS_DNS_C, FALSE);
	srv_a_query_cancelled_engine(BELLE_SIP_DNS_DNS_C, TRUE);
#ifdef HAVE_DNS_SERVICE
	belle_sip_object_inhibit_leak_detector(TRUE); // leak detector is not thread safe
	srv_a_query_cancelled_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE, FALSE);
	srv_a_query_cancelled_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE, TRUE);
	belle_sip_object_inhibit_leak_detector(FALSE);
#endif /* HAVE_DNS_SERVICE */
}

/* Cancel A+AAAA query */
static void aaaa_query_cancelled_engine(unsigned char dns_engine, bool_t destroy_stack) {
	int timeout;
	endpoint_t *client;
	int i, dummy = 0;

	if (!belle_sip_tester_ipv6_available()) {
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	client = create_endpoint();

	/* First run a successfull query, to the result is in the system cache */
	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV6_SIP_DOMAIN, SIP_PORT, AF_INET6, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);

	/* Then run 50 times a query and cancel it(just to try some race conditions) */
	for (i = 0; i < 50; i++) {
		reset_endpoint(client);
		client->resolver_ctx =
		    belle_sip_stack_resolve_a(client->stack, IPV6_SIP_DOMAIN, SIP_PORT, AF_INET6, a_resolve_done, client);
		BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
		/* cancel the query (do it immediately otherwise the result might arrive before the cancel) */
		belle_sip_resolver_context_cancel(client->resolver_ctx);
		BC_ASSERT_TRUE(client->resolve_done == 0);
		BC_ASSERT_PTR_NULL(client->ai_list);

		if (destroy_stack == TRUE) {
			destroy_endpoint_stack(client);
			bctbx_sleep_ms(50);
			init_endpoint_stack(client);
			belle_sip_stack_set_dns_engine(client->stack, dns_engine);
		} else {
			/* wait a little (query result is already in the system cache so it shall be fast) to give the time a to
			 * potential answer to reach cancelled request */
			wait_for(client->stack, &dummy, 1, 50);
		}
		BC_ASSERT_PTR_NULL(client->ai_list);
	}

	destroy_endpoint(client);
}
static void aaaa_query_cancelled(void) {
	aaaa_query_cancelled_engine(BELLE_SIP_DNS_DNS_C, FALSE);
	aaaa_query_cancelled_engine(BELLE_SIP_DNS_DNS_C, TRUE);
#ifdef HAVE_DNS_SERVICE
	belle_sip_object_inhibit_leak_detector(TRUE); // leak detector is not thread safe
	aaaa_query_cancelled_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE, FALSE);
	aaaa_query_cancelled_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE, TRUE);
	belle_sip_object_inhibit_leak_detector(FALSE);
#endif /* HAVE_DNS_SERVICE */
}

/* Cancel A query during timeout*/
static void timeout_query_cancelled_engine(unsigned char dns_engine, bool_t destroy_stack) {
	endpoint_t *client = create_endpoint();
	int i, dummy = 0;

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	belle_sip_stack_set_dns_timeout(client->stack, 0);
	/* Then run 50 times a query and cancel it(just to try some race conditions) */
	for (i = 0; i < 50; i++) {
		client->resolver_ctx =
		    belle_sip_stack_resolve_a(client->stack, "toto.com", SIP_PORT, AF_INET, a_resolve_done, client);
		BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
		/* cancel the query (do it immediately otherwise the result might arrive before the cancel) */
		belle_sip_resolver_context_cancel(client->resolver_ctx);
		BC_ASSERT_TRUE(client->resolve_done == 0);
		BC_ASSERT_PTR_NULL(client->ai_list);

		if (destroy_stack == TRUE) {
			destroy_endpoint_stack(client);
			bctbx_sleep_ms(50);
			init_endpoint_stack(client);
			belle_sip_stack_set_dns_engine(client->stack, dns_engine);
		} else {
			/* wait a little (query result is already in the system cache so it shall be fast) to give the time a to
			 * potential answer to reach cancelled request */
			wait_for(client->stack, &dummy, 1, 50);
		}
		BC_ASSERT_PTR_NULL(client->ai_list);
		reset_endpoint(client);
	}

	destroy_endpoint(client);
}
static void timeout_query_cancelled(void) {
	timeout_query_cancelled_engine(BELLE_SIP_DNS_DNS_C, FALSE);
	timeout_query_cancelled_engine(BELLE_SIP_DNS_DNS_C, TRUE);
#ifdef HAVE_DNS_SERVICE
	belle_sip_object_inhibit_leak_detector(TRUE); // leak detector is not thread safe
	timeout_query_cancelled_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE, FALSE);
	timeout_query_cancelled_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE, TRUE);
	belle_sip_object_inhibit_leak_detector(FALSE);
#endif /* HAVE_DNS_SERVICE */
}

/* Successful IPv4 A query to a CNAME*/
/*This tests the recursion of dns.c*/
static void ipv4_cname_a_query_engine(unsigned char dns_engine) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV4_CNAME, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = bctbx_ip_address_to_addrinfo(AF_INET, SOCK_STREAM, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			bctbx_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}

static void ipv4_cname_a_query(void) {
	ipv4_cname_a_query_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	ipv4_cname_a_query_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

static void local_query_engine(unsigned char dns_engine) {
	int timeout;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, "localhost", SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
	}
	destroy_endpoint(client);
}

static void local_query(void) {
	local_query_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	local_query_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

/* Successful IPv4 A query with no result */
static void ipv4_a_query_no_result_engine(unsigned char dns_engine) {
	int timeout;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV4_SIP_BAD_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_EQUAL(client->ai_list, NULL);

	destroy_endpoint(client);
}
static void ipv4_a_query_no_result(void) {
	ipv4_a_query_no_result_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	ipv4_a_query_no_result_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

/* IPv4 A query send failure */
static void ipv4_a_query_send_failure_engine(unsigned char dns_engine) {
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	belle_sip_stack_set_resolver_send_error(client->stack, -1);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NULL(client->resolver_ctx);
	belle_sip_stack_set_resolver_send_error(client->stack, 0);

	destroy_endpoint(client);
}
static void ipv4_a_query_send_failure(void) {
	ipv4_a_query_send_failure_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	ipv4_a_query_send_failure_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

/* IPv4 A query timeout */
static void ipv4_a_query_timeout_engine(unsigned char dns_engine) {

	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	belle_sip_stack_set_dns_timeout(client->stack, 0);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, "toto.com", SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 2000));
	BC_ASSERT_PTR_EQUAL(client->ai_list, NULL);
	BC_ASSERT_EQUAL(client->resolve_ko, 1, int, "%d");
	destroy_endpoint(client);
}
static void ipv4_a_query_timeout(void) {
	ipv4_a_query_timeout_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	ipv4_a_query_timeout_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

/* Successful IPv4 A query with multiple results */
static void ipv4_a_query_multiple_results_engine(unsigned char dns_engine) {
	int timeout;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV4_MULTIRES_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		BC_ASSERT_PTR_NOT_NULL(client->ai_list->ai_next);
	}

	destroy_endpoint(client);
}
static void ipv4_a_query_multiple_results(void) {
	ipv4_a_query_multiple_results_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	ipv4_a_query_multiple_results_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

static void ipv4_a_query_with_v4mapped_results_engine(unsigned char dns_engine) {
	int timeout;
	endpoint_t *client;

	if (!belle_sip_tester_ipv6_available()) {
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}

	client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET6, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);

	destroy_endpoint(client);
}
static void ipv4_a_query_with_v4mapped_results(void) {
	ipv4_a_query_with_v4mapped_results_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	ipv4_a_query_with_v4mapped_results_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

/* Successful IPv6 AAAA query */
static void ipv6_aaaa_query_engine(unsigned char dns_engine) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client;

	if (!belle_sip_tester_ipv6_available()) {
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}

	client = create_endpoint();
	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV6_SIP_DOMAIN, SIP_PORT, AF_INET6, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct addrinfo *next;
		struct sockaddr_in6 *sock_in6 = (struct sockaddr_in6 *)client->ai_list->ai_addr;
		int ntohsi = ntohs(sock_in6->sin6_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		/*the IPv6 address shall return first, and must be a real ipv6 address*/
		BC_ASSERT_EQUAL(client->ai_list->ai_family, AF_INET6, int, "%d");
		BC_ASSERT_FALSE(IN6_IS_ADDR_V4MAPPED(&sock_in6->sin6_addr));
		ai = bctbx_ip_address_to_addrinfo(AF_INET6, SOCK_STREAM, IPV6_SIP_IP, SIP_PORT);
		BC_ASSERT_PTR_NOT_NULL(ai);
		if (ai) {
			struct in6_addr *ipv6_address = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
			int i;
			for (i = 0; i < 8; i++) {
				BC_ASSERT_EQUAL(sock_in6->sin6_addr.s6_addr[i], ipv6_address->s6_addr[i], int, "%d");
			}
			bctbx_freeaddrinfo(ai);
		}
		next = client->ai_list->ai_next;
		BC_ASSERT_PTR_NOT_NULL(next);
		if (next) {
			int ntohsi = ntohs(sock_in6->sin6_port);
			sock_in6 = (struct sockaddr_in6 *)next->ai_addr;
			BC_ASSERT_EQUAL(next->ai_family, AF_INET6, int, "%d");
			BC_ASSERT_TRUE(IN6_IS_ADDR_V4MAPPED(&sock_in6->sin6_addr));
			BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
			ai = bctbx_ip_address_to_addrinfo(AF_INET6, SOCK_STREAM, IPV6_SIP_IPV4, SIP_PORT);
			BC_ASSERT_PTR_NOT_NULL(ai);
			if (ai) {
				struct in6_addr *ipv6_address = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
				int i;
				for (i = 0; i < 8; i++) {
					BC_ASSERT_EQUAL(sock_in6->sin6_addr.s6_addr[i], ipv6_address->s6_addr[i], int, "%d");
				}
				bctbx_freeaddrinfo(ai);
			}
		}
	}
	destroy_endpoint(client);
}
static void ipv6_aaaa_query(void) {
	ipv6_aaaa_query_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	ipv6_aaaa_query_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

/* Successful SRV query */
static void srv_query_engine(unsigned char dns_engine) {
	int timeout;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve_srv(client->stack, "sip", "udp", SRV_DOMAIN, srv_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->srv_list);
	BC_ASSERT_NOT_EQUAL((unsigned int)belle_sip_list_size(client->srv_list), 0, unsigned int, "%u");
	if (client->srv_list && (belle_sip_list_size(client->srv_list) > 0)) {
		belle_sip_dns_srv_t *result_srv = belle_sip_list_nth_data(client->srv_list, 0);
		BC_ASSERT_EQUAL(belle_sip_dns_srv_get_port(result_srv), SIP_PORT, int, "%d");
	}

	destroy_endpoint(client);
}
static void srv_query(void) {
	srv_query_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	srv_query_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

/* Successful SRV + A or AAAA queries */
static void srv_a_query_engine(unsigned char dns_engine) {
	int timeout;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve(client->stack, "sip", "udp", SRV_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);

	destroy_endpoint(client);
}
static void srv_a_query(void) {
	srv_a_query_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	srv_a_query_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

/* Successful SRV query with no result + A query */
static void srv_a_query_no_srv_result_engine(unsigned char dns_engine) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve(client->stack, "sip", "udp", IPV4_CNAME, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = bctbx_ip_address_to_addrinfo(AF_INET, SOCK_STREAM, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			bctbx_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}
static void srv_a_query_no_srv_result(void) {
	srv_a_query_no_srv_result_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	srv_a_query_no_srv_result_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

static void non_working_srv_a_query_engine(unsigned char dns_engine) {
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	belle_sip_stack_simulate_non_working_srv(client->stack, TRUE);
	client->resolver_ctx =
	    belle_sip_stack_resolve(client->stack, "sip", "udp", SRV_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	/* the results should arrive before the normal timeout (10s).*/
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 5000));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);

	destroy_endpoint(client);
}

static void non_working_srv_a_query(void) {
	non_working_srv_a_query_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	non_working_srv_a_query_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

static void local_full_query_engine(unsigned char dns_engine) {
	int timeout;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	client->resolver_ctx =
	    belle_sip_stack_resolve(client->stack, "sip", "tcp", "localhost", SIP_PORT, AF_INET, a_resolve_done, client);
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
static void local_full_query(void) {
	local_full_query_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	local_full_query_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

/* No query needed because already resolved */
static void no_query_needed_engine(unsigned char dns_engine) {
	struct addrinfo *ai;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	client->resolver_ctx =
	    belle_sip_stack_resolve(client->stack, "sip", "udp", IPV4_SIP_IP, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(client->resolve_done);
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = bctbx_ip_address_to_addrinfo(AF_INET, SOCK_STREAM, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			bctbx_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}
static void no_query_needed(void) {
	no_query_needed_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	no_query_needed_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

static void set_custom_resolv_conf(belle_sip_stack_t *stack, const char *ns[]) {
	char *resolv_file = bc_tester_file("tmp_resolv.conf");
	FILE *f = fopen(resolv_file, "w");
	BC_ASSERT_PTR_NOT_NULL(f);
	if (f) {
		int i;
		for (i = 0; i < 3; ++i) {
			if (ns[i] != NULL) {
				fprintf(f, "nameserver %s\n", ns[i]);
			} else break;
		}
		fclose(f);
	}
	belle_sip_stack_set_dns_resolv_conf_file(stack, resolv_file);
	free(resolv_file);
}

static void _dns_fallback(const char *nameservers[], unsigned char dns_engine) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client = create_endpoint();

	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	set_custom_resolv_conf(client->stack, nameservers);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = bctbx_ip_address_to_addrinfo(AF_INET, SOCK_STREAM, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			bctbx_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}

static void dns_fallback(void) {
	const char *nameservers[] = {"94.23.19.176", /*linphone.org ; this is not a name server, it will not respond*/
	                             "8.8.8.8",      /* public nameserver, should work*/
	                             NULL};
	_dns_fallback(nameservers, BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	_dns_fallback(nameservers, BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

static void dns_fallback_because_of_server_refused(void) {
	const char *nameservers[] = {"212.245.255.2", "8.8.8.8", /* public nameserver, should work*/
	                             NULL};
	_dns_fallback(nameservers, BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	_dns_fallback(nameservers, BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

static void dns_fallback_because_of_scope_link_ipv6(void) {
	const char *nameservers[] = {
	    "fe80::fdc5:99ef:ac05:5c55%enp0s25", /* Scope link IPv6 name server that will not respond */
	    "8.8.8.8",                           /* public nameserver, should work*/
	    NULL};
	_dns_fallback(nameservers, BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	_dns_fallback(nameservers, BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

static void dns_fallback_because_of_invalid_ipv6(void) {
	const char *nameservers[] = {"fe80::ba26:6cff:feb9:145c", /* Invalid IPv6 name server */
	                             "8.8.8.8",                   /* public nameserver, should work*/
	                             NULL};
	_dns_fallback(nameservers, BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	_dns_fallback(nameservers, BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

static void ipv6_dns_server_engine(unsigned char dns_engine) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client;
	const char *nameservers[] = {"2001:4860:4860::8888", NULL};

	if (!belle_sip_tester_ipv6_available()) {
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}

	client = create_endpoint();
	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	set_custom_resolv_conf(client->stack, nameservers);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = bctbx_ip_address_to_addrinfo(AF_INET, SOCK_STREAM, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			bctbx_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}
static void ipv6_dns_server(void) {
	ipv6_dns_server_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	ipv6_dns_server_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

static void ipv4_and_ipv6_dns_server_engine(unsigned char dns_engine) {
	struct addrinfo *ai;
	int timeout;
	endpoint_t *client;
	const char *nameservers[] = {"8.8.8.8", "2a01:e00::2", NULL};

	if (!belle_sip_tester_ipv6_available()) {
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}
	client = create_endpoint();
	if (!BC_ASSERT_PTR_NOT_NULL(client)) return;
	belle_sip_stack_set_dns_engine(client->stack, dns_engine);
	timeout = belle_sip_stack_get_dns_timeout(client->stack);
	set_custom_resolv_conf(client->stack, nameservers);
	client->resolver_ctx =
	    belle_sip_stack_resolve_a(client->stack, IPV4_SIP_DOMAIN, SIP_PORT, AF_INET, a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, timeout));
	BC_ASSERT_PTR_NOT_EQUAL(client->ai_list, NULL);
	if (client->ai_list) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
		int ntohsi = (int)ntohs(sock_in->sin_port);
		BC_ASSERT_EQUAL(ntohsi, SIP_PORT, int, "%d");
		ai = bctbx_ip_address_to_addrinfo(AF_INET, SOCK_STREAM, IPV4_SIP_IP, SIP_PORT);
		if (ai) {
			BC_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr, int, "%d");
			bctbx_freeaddrinfo(ai);
		}
	}

	destroy_endpoint(client);
}
static void ipv4_and_ipv6_dns_server(void) {
	ipv4_and_ipv6_dns_server_engine(BELLE_SIP_DNS_DNS_C);
#ifdef HAVE_DNS_SERVICE
	ipv4_and_ipv6_dns_server_engine(BELLE_SIP_DNS_APPLE_DNS_SERVICE);
#endif /* HAVE_DNS_SERVICE */
}

#ifdef HAVE_MDNS
static void mdns_register_callback(void *data, int error) {
	int *register_error = (int *)data;
	*register_error = error;
}

static void mdns_query_ipv4_or_ipv6(int family) {
	belle_sip_mdns_register_t *reg;
	endpoint_t *client;
	int register_error = -1;

	client = create_endpoint();

	reg = belle_sip_mdns_register("sip", "tcp", "test.linphone.local", NULL, 5060, 10, 100, 3600,
	                              mdns_register_callback, &register_error);
	BC_ASSERT_PTR_NOT_NULL(reg);
	BC_ASSERT_TRUE(wait_for(client->stack, &register_error, 0, 5000));

	client->resolver_ctx = belle_sip_stack_resolve(client->stack, "sip", "tcp", "test.linphone.local", 5060, family,
	                                               a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 10000));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);

	belle_sip_mdns_unregister(reg);
	destroy_endpoint(client);
}

static void mdns_query(void) {
	mdns_query_ipv4_or_ipv6(AF_INET);
}

static void mdns_query_ipv6(void) {
	if (!belle_sip_tester_ipv6_available()) {
		belle_sip_warning("Test skipped, IPv6 connectivity not available.");
		return;
	}

	mdns_query_ipv4_or_ipv6(AF_INET6);
}

static void mdns_query_no_result(void) {
	endpoint_t *client;

	client = create_endpoint();

	/* Wait some time to be sure that the services from last test are stopped */
	wait_for(client->stack, &client->resolve_done, 1, 1000);

	client->resolver_ctx = belle_sip_stack_resolve(client->stack, "sip", "tcp", "test.linphone.local", 5060, AF_INET,
	                                               a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 10000));
	BC_ASSERT_PTR_NULL(client->ai_list);

	destroy_endpoint(client);
}

static void mdns_query_multiple_result(void) {
	belle_sip_mdns_register_t *reg1, *reg2;
	endpoint_t *client;
	int register_error = -1;

	client = create_endpoint();

	reg1 = belle_sip_mdns_register("sip", "tcp", "test.linphone.local", "Register1", 5060, 20, 100, 3600,
	                               mdns_register_callback, &register_error);
	BC_ASSERT_PTR_NOT_NULL(reg1);
	BC_ASSERT_TRUE(wait_for(client->stack, &register_error, 0, 5000));

	register_error = -1;
	reg2 = belle_sip_mdns_register("sip", "tcp", "test.linphone.local", "Register2", 5070, 10, 100, 3600,
	                               mdns_register_callback, &register_error);
	BC_ASSERT_PTR_NOT_NULL(reg2);
	BC_ASSERT_TRUE(wait_for(client->stack, &register_error, 0, 5000));

	client->resolver_ctx = belle_sip_stack_resolve(client->stack, "sip", "tcp", "test.linphone.local", 5060, AF_INET,
	                                               a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 10000));

	BC_ASSERT_PTR_NOT_NULL(client->ai_list);
	if (client->ai_list) {
		/* We need to know if we have two results, if ai_list is != NULL then we have one so we check ai_next */
		BC_ASSERT_PTR_NOT_NULL(client->ai_list->ai_next);

		/* The first adress should be the one registered on port 5070 since it has higher priority */
		if (client->ai_list->ai_addr->sa_family == AF_INET) {
			struct sockaddr_in *sock_in = (struct sockaddr_in *)client->ai_list->ai_addr;
			int ntohsi = (int)ntohs(sock_in->sin_port);
			BC_ASSERT_EQUAL(ntohsi, 5070, int, "%d");
		} else {
			struct sockaddr_in6 *sock_in = (struct sockaddr_in6 *)client->ai_list->ai_addr;
			int ntohsi = (int)ntohs(sock_in->sin6_port);
			BC_ASSERT_EQUAL(ntohsi, 5070, int, "%d");
		}
	}

	belle_sip_mdns_unregister(reg1);
	belle_sip_mdns_unregister(reg2);
	destroy_endpoint(client);
}

static void mdns_queries(void) {
	belle_sip_mdns_register_t *reg;
	endpoint_t *client;
	int register_error = -1;

	client = create_endpoint();

	/* Wait some time to be sure that the services from last test are stopped */
	wait_for(client->stack, &client->resolve_done, 1, 1000);

	reg = belle_sip_mdns_register("sip", "tcp", "test.linphone.local", NULL, 5060, 10, 100, 3600,
	                              mdns_register_callback, &register_error);
	BC_ASSERT_PTR_NOT_NULL(reg);
	BC_ASSERT_TRUE(wait_for(client->stack, &register_error, 0, 5000));

	client->resolver_ctx = belle_sip_stack_resolve(client->stack, "sip", "tcp", "test.linphone.local", 5060, AF_INET,
	                                               a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 10000));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);

	reset_endpoint(client);
	wait_for(client->stack, &client->resolve_done, 1, 5000); // Wait 5 seconds

	client->resolver_ctx = belle_sip_stack_resolve(client->stack, "sip", "tcp", "test.linphone.local", 5060, AF_INET,
	                                               a_resolve_done, client);
	BC_ASSERT_PTR_NOT_NULL(client->resolver_ctx);
	BC_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 10000));
	BC_ASSERT_PTR_NOT_NULL(client->ai_list);

	belle_sip_mdns_unregister(reg);
	destroy_endpoint(client);
}
#endif

static test_t resolver_tests[] = {
    TEST_NO_TAG("A query (IPv4)", ipv4_a_query),
    TEST_NO_TAG("A query (IPv4) with CNAME", ipv4_cname_a_query),
    TEST_NO_TAG("A query (IPv4) with no result", ipv4_a_query_no_result),
    TEST_NO_TAG("A query (IPv4) with send failure", ipv4_a_query_send_failure),
    TEST_NO_TAG("A query (IPv4) with timeout", ipv4_a_query_timeout),
    TEST_NO_TAG("A query (IPv4) with multiple results", ipv4_a_query_multiple_results),
    TEST_NO_TAG("A query (IPv4) with AI_V4MAPPED results", ipv4_a_query_with_v4mapped_results),
    TEST_NO_TAG("Local query", local_query),
    TEST_NO_TAG("AAAA query (IPv6)", ipv6_aaaa_query),
    TEST_NO_TAG("SRV query", srv_query),
    TEST_NO_TAG("SRV + A query", srv_a_query),
    TEST_NO_TAG("SRV + A query with no SRV result", srv_a_query_no_srv_result),
    TEST_NO_TAG("Non working SRV + A query", non_working_srv_a_query),
    TEST_NO_TAG("Local SRV+A query", local_full_query),
    TEST_NO_TAG("No query needed", no_query_needed),
    TEST_NO_TAG("DNS fallback", dns_fallback),
    TEST_NO_TAG("DNS fallback because of server refused", dns_fallback_because_of_server_refused),
    TEST_NO_TAG("DNS fallback because of scope link IPv6", dns_fallback_because_of_scope_link_ipv6),
    TEST_NO_TAG("DNS fallback because of invalid IPv6", dns_fallback_because_of_invalid_ipv6),
    TEST_NO_TAG("IPv6 DNS server", ipv6_dns_server),
    TEST_NO_TAG("IPv4 and v6 DNS servers", ipv4_and_ipv6_dns_server),
    TEST_NO_TAG("A query (IPv4) cancelled", a_query_cancelled),
    TEST_NO_TAG("SRV query cancelled", srv_query_cancelled),
    TEST_NO_TAG("SRV + A query cancelled", srv_a_query_cancelled),
    TEST_NO_TAG("AAAA query cancelled", aaaa_query_cancelled),
    TEST_NO_TAG("A query in time out cancelled", timeout_query_cancelled),
#ifdef HAVE_MDNS
    TEST_NO_TAG("MDNS query", mdns_query),
    TEST_NO_TAG("MDNS query with ipv6", mdns_query_ipv6),
    TEST_NO_TAG("MDNS query with no result", mdns_query_no_result),
    TEST_NO_TAG("MDNS query with multiple result", mdns_query_multiple_result),
    TEST_NO_TAG("MDNS multiple queries", mdns_queries)
#endif
};

test_suite_t resolver_test_suite = {"Resolver",
                                    NULL,
                                    NULL,
                                    belle_sip_tester_before_each,
                                    belle_sip_tester_after_each,
                                    sizeof(resolver_tests) / sizeof(resolver_tests[0]),
                                    resolver_tests,
                                    0,
                                    0};
