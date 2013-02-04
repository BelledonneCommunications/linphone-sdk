/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010-2013  Belledonne Communications SARL

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

#include <stdio.h>
#include "CUnit/Basic.h"
#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"


#define IPV4_SIP_DOMAIN	"sip.linphone.org"
#define IPV4_SIP_IP	"37.59.129.73"
#define IPV6_SIP_DOMAIN	"videolan.org"
#define IPV6_SIP_IP	"2a01:e0d:1:3:58bf:fa02:0:1"
#define SIP_PORT	5060


typedef struct endpoint {
	belle_sip_stack_t* stack;
	long unsigned int resolver_id;
	int resolve_done;
	struct addrinfo *result;
} endpoint_t;

static unsigned int  wait_for(belle_sip_stack_t *stack, int *current_value, int expected_value, int timeout) {
	int retry = 0;
#define ITER 100
	while ((*current_value != expected_value) && (retry++ < (timeout / ITER))) {
		if (stack) belle_sip_stack_sleep(stack, ITER);
	}
	if (*current_value != expected_value) return FALSE;
	else return TRUE;
}

static endpoint_t* create_endpoint(void) {
	endpoint_t* endpoint;
	if (belle_sip_init_sockets() < 0) return NULL;
	endpoint = belle_sip_new0(endpoint_t);
	endpoint->stack = belle_sip_stack_new(NULL);
	return endpoint;
}

static void reset_endpoint(endpoint_t *endpoint) {
	endpoint->resolver_id = 0;
	endpoint->resolve_done = 0;
	if (endpoint->result) {
		freeaddrinfo(endpoint->result);
		endpoint->result = NULL;
	}
}

static void destroy_endpoint(endpoint_t *endpoint) {
	belle_sip_object_unref(endpoint->stack);
	belle_sip_free(endpoint);
	belle_sip_uninit_sockets();
}

static void resolve_done(void *data, const char *name, struct addrinfo *res) {
	endpoint_t *client = (endpoint_t *)data;
	client->resolve_done = 1;
	if (res) {
		client->result = res;
	}
}

static void resolve(void) {
	const char *peer_name;
	int peer_port = SIP_PORT;
	struct addrinfo *ai;
	int family;
	endpoint_t *client = create_endpoint();
	CU_ASSERT_PTR_NOT_NULL_FATAL(client);

	/* IPv4 A query */
	family = AF_INET;
	peer_name = IPV4_SIP_DOMAIN;
	client->resolver_id = belle_sip_resolve(client->stack, peer_name, peer_port, family, resolve_done, client, belle_sip_stack_get_main_loop(client->stack));
	CU_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 2000));
	CU_ASSERT_PTR_NOT_EQUAL(client->result, NULL);
	if (client->result) {
		struct sockaddr_in *sock_in = (struct sockaddr_in *)client->result->ai_addr;
		CU_ASSERT_EQUAL(ntohs(sock_in->sin_port), peer_port);
		ai = belle_sip_ip_address_to_addrinfo(IPV4_SIP_IP, peer_port);
		if (ai) {
			CU_ASSERT_EQUAL(sock_in->sin_addr.s_addr, ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr);
		}
	}

	/* IPv6 AAAA query */
	reset_endpoint(client);
	family = AF_INET6;
	peer_name = IPV6_SIP_DOMAIN;
	client->resolver_id = belle_sip_resolve(client->stack, peer_name, peer_port, family, resolve_done, client, belle_sip_stack_get_main_loop(client->stack));
	CU_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 2000));
	CU_ASSERT_PTR_NOT_EQUAL(client->result, NULL);
	if (client->result) {
		struct sockaddr_in6 *sock_in6 = (struct sockaddr_in6 *)client->result->ai_addr;
		CU_ASSERT_EQUAL(ntohs(sock_in6->sin6_port), peer_port);
		ai = belle_sip_ip_address_to_addrinfo(IPV6_SIP_IP, peer_port);
		if (ai) {
			struct in6_addr *ipv6_address = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
			CU_ASSERT_EQUAL(sock_in6->sin6_addr.s6_addr32[0], ipv6_address->s6_addr32[0]);
			CU_ASSERT_EQUAL(sock_in6->sin6_addr.s6_addr32[1], ipv6_address->s6_addr32[1]);
			CU_ASSERT_EQUAL(sock_in6->sin6_addr.s6_addr32[2], ipv6_address->s6_addr32[2]);
			CU_ASSERT_EQUAL(sock_in6->sin6_addr.s6_addr32[3], ipv6_address->s6_addr32[3]);
		}
	}

	destroy_endpoint(client);
}

int belle_sip_resolver_test_suite(){
	CU_pSuite pSuite = CU_add_suite("Resolver", NULL, NULL);

	if (NULL == CU_add_test(pSuite, "resolve", resolve)) {
		return CU_get_error();
	}
	return 0;
}
