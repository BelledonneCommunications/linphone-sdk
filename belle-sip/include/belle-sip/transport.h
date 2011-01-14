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
#ifndef BELLE_SIP_TRANSPORT_H
#define BELLE_SIP_TRANSPORT_H


typedef struct belle_sip_transport belle_sip_transport_t;

struct belle_sip_transport_class{
	char *name; /*udp,tcp*/
	int is_reliable;
	int (*connect)(belle_sip_transport_t *t, const struct sockaddr *addr, socklen_t addrlen);
	int (*recvfrom)(belle_sip_transport_t *t, void *buf, size_t buflen, struct sockaddr *addr, socklen_t *addrlen);
	int (*sendto)(belle_sip_transport_t *t, const void *buf, size_t buflen, const struct sockaddr *addr, socklen_t addrlen);
	/**
	 * Method to know if this transport is suitable for supplied transport name and address.
	 * Should return 1 if suitable, 0 otherwise.
	 **/
	int (*matches)(belle_sip_transport_t *, const char *name, const struct sockaddr *addr, socklen_t addrlen);
	void (*close)(belle_sip_transport_t *);
};

typedef struct belle_sip_transport_class belle_sip_transport_class_t;


struct belle_sip_transport{
	int magic;
	belle_sip_transport_class_t *klass;
	int refcnt;
};

const char *belle_sip_transport_get_name(const belle_sip_transport_t *t);
int belle_sip_transport_is_reliable(const belle_sip_transport_t *t);
int belle_sip_transport_matches(belle_sip_transport_t *t, const char *name, const struct sockaddr *addr, socklen_t addrlen);
int belle_sip_transport_sendto(belle_sip_transport_t *t, const void *buf, size_t buflen, const struct sockaddr *addr, socklen_t addrlen);
int belle_sip_transport_recvfrom(belle_sip_transport_t *t, void *buf, size_t buflen, struct sockaddr *addr, socklen_t *addrlen);
void belle_sip_transport_ref(belle_sip_transport_t *t);
void belle_sip_transport_unref(belle_sip_transport_t *t);

BELLE_SIP_DECLARE_CAST(belle_sip_transport_t);

#define BELLE_SIP_TRANSPORT(obj) BELLE_SIP_CAST(obj,belle_sip_transport_t)

#endif
