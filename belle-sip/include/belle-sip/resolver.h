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


#ifndef belle_sip_resolver_h
#define belle_sip_resolver_h


typedef struct belle_sip_dns_srv belle_sip_dns_srv_t;
#define BELLE_SIP_DNS_SRV(obj) BELLE_SIP_CAST(obj,belle_sip_dns_srv_t)

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

BELLESIP_EXPORT const char *belle_sip_dns_srv_get_target(const belle_sip_dns_srv_t *obj);

BELLESIP_EXPORT unsigned short belle_sip_dns_srv_get_priority(const belle_sip_dns_srv_t *obj);

BELLESIP_EXPORT unsigned short belle_sip_dns_srv_get_weight(const belle_sip_dns_srv_t *obj);

BELLESIP_EXPORT unsigned short belle_sip_dns_srv_get_port(const belle_sip_dns_srv_t *obj);


/**
 * Asynchronously performs DNS SRV followed A/AAAA query. Automatically fallbacks to A/AAAA if SRV isn't found.
 * @param stack the belle_sip_stack_t, used to schedule asynchronous execution.
 * @param service the queried service ("sip", "stun", "turn"...)
 * @param transport the queried transport ("udp", "tcp", "tls")
 * @param name the SIP domain name
 * @param port a port that will be filled in the addrinfo list returned by the callback, for the case where no SRV records are found.
 * @param family address family expected in the addrinfo result list. AF_INET or AF_INET6. If AF_INET6 is used but no IPv6 records are found, the IPv4 addresses
 * will be returned as IPv6 with v4 mapping (AI_V4MAPPED).
 * @param cb a callback function that will be called to notify the results.
 * @param data a user pointer passed through the callback as first argument.
 * @return a #belle_sip_resolver_context_t that can be used to cancel the resolution if needed. The context must have been ref'd with belle_sip_object_ref().
**/
BELLESIP_EXPORT belle_sip_resolver_context_t * belle_sip_stack_resolve(belle_sip_stack_t *stack, const char *service, const char *transport, const char *name, int port, int family,
belle_sip_resolver_callback_t cb, void *data);

/**
 * Asynchronously performs DNS A or AAAA query.
 * @param stack the belle_sip_stack_t, used to schedule asynchronous execution.
 * @param name the SIP domain name
 * @param port a port that will be filled in the addrinfo list returned by the callback, for the case where no SRV records are found.
 * @param family address family expected in the addrinfo result list. AF_INET or AF_INET6. If AF_INET6 is used but no IPv4 records are found, the IPv4 addresses
 * will be returned as IPv6 with v4 mapping (AI_V4MAPPED).
 * @param cb a callback function that will be called to notify the results.
 * @param data a user pointer passed through the callback as first argument.
 * @return a #belle_sip_resolver_context_t that can be used to cancel the resolution if needed. The context must have been ref'd with belle_sip_object_ref().
**/
BELLESIP_EXPORT belle_sip_resolver_context_t * belle_sip_stack_resolve_a(belle_sip_stack_t *stack, const char *name, int port, int family, belle_sip_resolver_callback_t cb, void *data);

/**
 * Asynchronously performs DNS SRV query.
 * @return a #belle_sip_resolver_context_t that can be used to cancel the resolution if needed. The context must have been ref'd with belle_sip_object_ref().
**/
BELLESIP_EXPORT belle_sip_resolver_context_t * belle_sip_stack_resolve_srv(belle_sip_stack_t *stack, const char *service, const char *transport, const char *name, belle_sip_resolver_srv_callback_t cb, void *data);

/**
 * Cancel a pending asynchronous DNS query. The context is unref'd automatically, as a result the context shall no longer be used after this call.
 * If the query is already performed, cancellation has no effect, but context is unref'd anyway.
 **/
BELLESIP_EXPORT void belle_sip_resolver_context_cancel(belle_sip_resolver_context_t *ctx);

/**
 * Lookups the source address from local interface that can be used to connect to a destination address.
 * local_port is only used to be assigned into the result source address.
 * This function always puts something in src and srclen (the loopback address) even if anything fails.
 * The return code is 0 if successful, or -errno if an error was encoutered. Typical error is -ENETUNREACH when IPv6 network is not reachable. 
**/
BELLESIP_EXPORT int belle_sip_get_src_addr_for(const struct sockaddr *dest, socklen_t destlen, struct sockaddr *src, socklen_t *srclen, int local_port);


BELLE_SIP_END_DECLS


#endif
