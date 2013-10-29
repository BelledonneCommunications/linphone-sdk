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


typedef struct belle_sip_resolver_context belle_sip_resolver_context_t;

#define BELLE_SIP_RESOLVER_CONTEXT(obj) BELLE_SIP_CAST(obj,belle_sip_resolver_context_t)


/**
 * Callback prototype for asynchronous DNS SRV resolution.
 * The srv_list contains struct dns_srv elements that must be taken and (possibly later) freed by the callee, using belle_sip_free().
 */
typedef void (*belle_sip_resolver_srv_callback_t)(void *data, const char *name, belle_sip_list_t *srv_list);

/**
 * Callback prototype for asynchronous DNS A and AAAA resolution.
 * The ai_list contains addrinfo elements that must be taken and (possibly later) freed by the callee, using freeaddrinfo().
 * These elements are linked by their ai_next field.
**/
typedef void (*belle_sip_resolver_callback_t)(void *data, const char *name, struct addrinfo *ai_list);

BELLE_SIP_BEGIN_DECLS

int belle_sip_addrinfo_to_ip(const struct addrinfo *ai, char *ip, size_t ip_size, int *port);
BELLESIP_EXPORT struct addrinfo * belle_sip_ip_address_to_addrinfo(int family, const char *ipaddress, int port);
BELLESIP_EXPORT unsigned long belle_sip_stack_resolve(belle_sip_stack_t *stack, const char *transport, const char *name, int port, int family, belle_sip_resolver_callback_t cb, void *data);
BELLESIP_EXPORT unsigned long belle_sip_stack_resolve_a(belle_sip_stack_t *stack, const char *name, int port, int family, belle_sip_resolver_callback_t cb, void *data);
BELLESIP_EXPORT unsigned long belle_sip_stack_resolve_srv(belle_sip_stack_t *stack, const char *name, const char *transport, belle_sip_resolver_srv_callback_t cb, void *data);
BELLESIP_EXPORT void belle_sip_stack_resolve_cancel(belle_sip_stack_t *stack, unsigned long id);

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

BELLE_SIP_END_DECLS


#endif
