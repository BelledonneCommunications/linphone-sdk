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


#ifndef belle_sip_resolver_h
#define belle_sip_resolver_h

#include "belle_sip_internal.h"
#include "dns.h"


typedef struct belle_sip_resolver_context belle_sip_resolver_context_t;

#define BELLE_SIP_RESOLVER_CONTEXT(obj) BELLE_SIP_CAST(obj,belle_sip_resolver_context_t)

/**
 * Callback prototype for asynchronous DNS resolution. The result addrinfo must be taken and (possibly later) freed by 
 * the callee, using freeaddrinfo().
**/
typedef void (*belle_sip_resolver_callback_t)(void *data, const char *name, struct addrinfo *result);


struct belle_sip_resolver_context{
	belle_sip_source_t source;
	belle_sip_stack_t *stack;
	belle_sip_resolver_callback_t cb;
	void *cb_data;
	struct dns_resolv_conf *resconf;
	struct dns_hosts *hosts;
	struct dns_resolver *R;
	char *name;
	int port;
	struct addrinfo *ai;
	int family;
	uint8_t cancelled;
	uint8_t done;
};

int belle_sip_addrinfo_to_ip(const struct addrinfo *ai, char *ip, size_t ip_size, int *port);
BELLESIP_INTERNAL_EXPORT struct addrinfo * belle_sip_ip_address_to_addrinfo(int family, const char *ipaddress, int port);
BELLESIP_INTERNAL_EXPORT unsigned long belle_sip_resolve(belle_sip_stack_t *stack, const char *name, int port, int family, belle_sip_resolver_callback_t cb , void *data, belle_sip_main_loop_t *ml);
void belle_sip_resolve_cancel(belle_sip_main_loop_t *ml, unsigned long id);

/**
 * Lookups the source address from local interface that can be used to connect to a destination address.
 * local_port is only used to be assigned into the result source address.
**/
void belle_sip_get_src_addr_for(const struct sockaddr *dest, socklen_t destlen, struct sockaddr *src, socklen_t *srclen, int local_port);

/**
 * This function will transform a V4 to V6 mapped address to a pure V4 and write it into result, or will just copy it otherwise.
 * The memory for v6 and result may be the same, in which case processing is done in place or no copy is done.
 * The pointer to result must have sufficient storage, typically a struct sockaddr_storage.
**/ 
void belle_sip_address_remove_v4_mapping(const struct sockaddr *v6, struct sockaddr *result, socklen_t *result_len);

#endif
