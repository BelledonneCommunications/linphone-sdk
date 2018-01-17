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

#ifndef BELLE_SIP_MDNS_REGISTER_H
#define BELLE_SIP_MDNS_REGISTER_H

typedef struct belle_sip_mdns_register belle_sip_mdns_register_t;
#define BELLE_SIP_MDNS_REGISTER(obj) BELLE_SIP_CAST(obj,belle_sip_mdns_register_t)

/**
 * Callback prototype for asynchronous multicast DNS registration (advertisement).
 */
typedef void (*belle_sip_mdns_register_callback_t)(void *data, int error);

BELLE_SIP_BEGIN_DECLS

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Tells if multicast DNS is available.
 * @return true if it is available, false otherwise.
**/
BELLESIP_EXPORT int belle_sip_mdns_register_available(void);

/**
 * Asynchronously performs the mdns registration (advertisement).
 * @param service the queried service ("sip", "stun", "turn"...).
 * @param transport the queried transport ("udp", "tcp", "tls").
 * @param domain the local domain name in which the service will be registered.
 * @param name the name of the mdns service, if NULL it is the computer's name. Only useful for multiple registrations to avoid conflicts.
 * @param port the port of the service.
 * @param priority the priority of the service, lower value means more preferred.
 * @param weight a relative weight for services within the same local domain that have the same priority, higher value means more preferred.
 * @param cb a callback function that will be called to notify the results.
 * @param data a user pointer passed through the callback as first argument.
 * @return a #belle_sip_register_t that can be used to cancel the registration if needed. The context must have been ref'd with belle_sip_object_ref().
**/
BELLESIP_EXPORT belle_sip_mdns_register_t *belle_sip_mdns_register(const char *service, const char *transport, const char *domain, const char* name, int port, int prio, int weight, belle_sip_mdns_register_callback_t cb, void *data);

/**
 * Cancels the mdns registration.
 * @param context the belle_sip_mdns_register_t used to register the service.
**/
BELLESIP_EXPORT void belle_sip_mdns_unregister(belle_sip_mdns_register_t *context);

#ifdef __cplusplus
} /* end extern "C" */
#endif

BELLE_SIP_END_DECLS

#endif /* BELLE_SIP_MDNS_REGISTER_H */
