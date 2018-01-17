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

#include "belle_sip_internal.h"
#include <bctoolbox/defs.h>

#ifdef HAVE_MDNS
#include <dns_sd.h>

#ifndef _WIN32
#include <unistd.h>
#include <poll.h>

typedef struct pollfd belle_sip_pollfd_t;

static int belle_sip_poll(belle_sip_pollfd_t *pfd, int count, int duration) {
	int err = poll(pfd, count, duration);
	if (err == -1 && errno != EINTR)
		belle_sip_error("poll() error: %s",strerror(errno));

	return err;
}

#else
#include <winsock2.h>

typedef WSAPOLLFD belle_sip_pollfd_t;

static int belle_sip_poll(belle_sip_pollfd_t *pfd, int count, int duration) {
	int err = WSAPoll(pfd, count, duration);
	if (err == SOCKET_ERROR)
		belle_sip_error("WSAPoll() error: %d", WSAGetLastError());

	return err;
}

#endif

struct belle_sip_mdns_register {
	belle_sip_object_t base;
	DNSServiceRef service_ref;
	belle_sip_mdns_register_callback_t cb;
	void *data;
	bctbx_thread_t thread;
	volatile bool_t running;
};

belle_sip_mdns_register_t *belle_sip_mdns_register_create(belle_sip_mdns_register_callback_t cb, void *data) {
	belle_sip_mdns_register_t *obj = belle_sip_object_new(belle_sip_mdns_register_t);
	obj->cb = cb;
	obj->data = data;
	obj->running = FALSE;
	return obj;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_mdns_register_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_mdns_register_t, belle_sip_object_t, NULL, NULL, NULL, FALSE);

static char * srv_prefix_from_service_and_transport(const char *service, const char *transport) {
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

static void belle_sip_mdns_register_reply(DNSServiceRef service_ref
										, DNSServiceFlags flags
										, DNSServiceErrorType error_code
										, const char *name
										, const char *type
										, const char *domain
										, void *data) {
	belle_sip_mdns_register_t *reg = (belle_sip_mdns_register_t *)data;
	int error = 0;

	if (error_code != kDNSServiceErr_NoError) {
		belle_sip_error("%s error while registering %s [%s%s]: code %d", __FUNCTION__, name, type, domain, error_code);
		error = -1;
	}

	if (reg->cb) reg->cb(reg->data, error);
}

void *mdns_register_poll(void *data) {
	belle_sip_mdns_register_t *reg = (belle_sip_mdns_register_t *)data;
	belle_sip_pollfd_t pollfd = {0};

	pollfd.fd = DNSServiceRefSockFD(reg->service_ref);
	pollfd.events = POLLIN;

	reg->running = TRUE;
	while(reg->running) {
		int err = belle_sip_poll(&pollfd, 1, 1000);

		if (err > 0) {
			DNSServiceProcessResult(reg->service_ref);
		}
	}

	return NULL;
}
#endif

int belle_sip_mdns_register_available(void) {
#ifdef HAVE_MDNS
	return TRUE;
#else
	return FALSE;
#endif
}

belle_sip_mdns_register_t *belle_sip_mdns_register(const char *service, const char *transport, const char *domain, const char *name, int port, int prio, int weight, belle_sip_mdns_register_callback_t cb, void *data) {
#ifdef HAVE_MDNS
	belle_sip_mdns_register_t *reg = belle_sip_mdns_register_create(cb, data);
	DNSServiceErrorType error;
	TXTRecordRef txt_ref;
	char number[10];
	char *prefix;
	int n;

	TXTRecordCreate(&txt_ref, 0, NULL);

	n = snprintf(number, sizeof(number), "%d", prio);
	TXTRecordSetValue(&txt_ref, "prio\0", n, number);

	n = snprintf(number, sizeof(number), "%d", weight);
	TXTRecordSetValue(&txt_ref, "weight\0", n, number);

	prefix = srv_prefix_from_service_and_transport(service, transport);

	error = DNSServiceRegister(&reg->service_ref
							, 0
							, 0
							, name
							, prefix
							, domain
							, NULL
							, port
							, TXTRecordGetLength(&txt_ref)
							, TXTRecordGetBytesPtr(&txt_ref)
							, belle_sip_mdns_register_reply
							, reg);

	belle_sip_free(prefix);
	TXTRecordDeallocate(&txt_ref);

	if (error != kDNSServiceErr_NoError) {
		belle_sip_error("%s Register error [_%s._%s.%s]: code %d", __FUNCTION__, service, transport, domain, error);
		belle_sip_object_unref(reg);
		return NULL;
	} else {
		int errnum = bctbx_thread_create(&reg->thread, NULL, mdns_register_poll, reg);
		if (errnum != 0) {
			belle_sip_error("%s Error creating register thread: code %d", __FUNCTION__, errnum);
			belle_sip_object_unref(reg);
			return NULL;
		}
	}

	return reg;
#else
	return NULL;
#endif
}

void belle_sip_mdns_unregister(belle_sip_mdns_register_t *reg) {
#ifdef HAVE_MDNS
	if (!reg) return;

	if (reg->running) {
		reg->running = FALSE;
		bctbx_thread_join(reg->thread, NULL);
	}

	DNSServiceRefDeallocate(reg->service_ref);
	belle_sip_object_unref(reg);
#endif
}
