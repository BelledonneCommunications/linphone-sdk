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


#define SIPDOMAIN "sip.linphone.org"
#define SIPPORT 5060


typedef struct endpoint {
	belle_sip_stack_t* stack;
	long unsigned int resolver_id;
	int resolve_done;
	int resolve_successful;
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
	endpoint_t* endpoint = belle_sip_new0(endpoint_t);
	endpoint->stack = belle_sip_stack_new(NULL);
	return endpoint;
}

static void destroy_endpoint(endpoint_t* endpoint) {
	belle_sip_object_unref(endpoint->stack);
	belle_sip_free(endpoint);
}

static void resolve_done(void *data, const char *name, struct addrinfo *res) {
	endpoint_t *client = (endpoint_t *)data;
	client->resolve_done = 1;
	if (res) {
		client->resolve_successful = 1;
	}
}

static void resolve(void) {
	const char* peer_name = SIPDOMAIN;
	int peer_port = SIPPORT;
	endpoint_t* client = create_endpoint();
	client->resolver_id = belle_sip_resolve(client->stack, peer_name, peer_port, 0, resolve_done, client, belle_sip_stack_get_main_loop(client->stack));
	CU_ASSERT_TRUE(wait_for(client->stack, &client->resolve_done, 1, 2000));
	CU_ASSERT_TRUE((client->resolve_successful == 1));
	destroy_endpoint(client);
}

int belle_sip_resolver_test_suite(){
	CU_pSuite pSuite = CU_add_suite("Resolver", NULL, NULL);

	if (NULL == CU_add_test(pSuite, "resolve", resolve)) {
		return CU_get_error();
	}
	return 0;
}
