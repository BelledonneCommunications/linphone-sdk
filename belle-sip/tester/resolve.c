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

#include <stdio.h>

#include "belle-sip/belle-sip.h"

static belle_sip_stack_t *stack;

static void resolver_callback(void *data, belle_sip_resolver_results_t *results) {
	int err;
	const struct addrinfo *ai_it;
	char name[NI_MAXHOST];
	char port[NI_MAXSERV];
	const struct addrinfo *ai_list = belle_sip_resolver_results_get_addrinfos(results);

	for (ai_it = ai_list; ai_it != NULL; ai_it = ai_it->ai_next) {
		err = bctbx_getnameinfo(ai_it->ai_addr, ai_list->ai_addrlen, name, sizeof(name), port, sizeof(port),
		                        NI_NUMERICSERV | NI_NUMERICHOST);
		if (err != 0) {
			fprintf(stderr, "getnameinfo error: %s", gai_strerror(err));
		} else {
			printf("\t%s %s  (ttl:%u)\n", name, port, belle_sip_resolver_results_get_ttl(results));
		}
	}
	if (ai_list == NULL) {
		printf("\tno results.\n");
	}
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(stack));
}

int main(int argc, char *argv[]) {
	int i;
	const char *domain = NULL;
	const char *transport = "udp";
	int use_srv = 1;
	const char *dns_server = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage:\n%s <domain name> [@dns-server-ip] [transport] [--no-srv] [--debug]\n", argv[0]);
		return -1;
	}
	domain = argv[1];
	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--debug") == 0) {
			belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
		} else if (strcmp(argv[i], "--no-srv") == 0) {
			use_srv = 0;
		} else if (*argv[i] == '@') {
			dns_server = argv[i] + 1;
		} else if (strstr(argv[i], "--") != argv[i]) transport = argv[i];
	}
	stack = belle_sip_stack_new(NULL);
	if (dns_server) {
		printf("Using DNS server %s\n", dns_server);
		bctbx_list_t *l = bctbx_list_append(NULL, (void *)dns_server);
		belle_sip_stack_set_dns_servers(stack, l);
		bctbx_list_free(l);
	}
	if (use_srv) {
		printf("Trying to resolve domain '%s', with transport hint '%s'\n", domain, transport);
		belle_sip_stack_resolve(stack, "sip", transport, domain, 5060, AF_INET6, resolver_callback, NULL);
	} else {
		printf("Trying to resolve domain '%s' without SRV...\n", domain);
		belle_sip_stack_resolve_a(stack, domain, 5060, AF_INET6, resolver_callback, NULL);
	}
	belle_sip_stack_main(stack);
	belle_sip_object_unref(stack);
	return 0;
}
