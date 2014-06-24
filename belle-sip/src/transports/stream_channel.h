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

#ifndef STREAM_CHANNEL_H_
#define STREAM_CHANNEL_H_
#ifdef __APPLE_
#include "TargetConditionals.h"
#endif

#if TARGET_OS_IPHONE
#include <CoreFoundation/CFStream.h>
#include <CFNetwork/CFSocketStream.h>
#endif

#include "channel.h"


struct belle_sip_stream_channel{
	belle_sip_channel_t base;
#if TARGET_OS_IPHONE
	CFReadStreamRef read_stream;
	CFWriteStreamRef write_stream;
#endif
};

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_stream_channel_t,belle_sip_channel_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

void belle_sip_stream_channel_init_client(belle_sip_stream_channel_t *obj, belle_sip_stack_t *stack, const char *bindip, int localport,const char *peer_cname, const char *dest, int port);

BELLESIP_INTERNAL_EXPORT belle_sip_channel_t * belle_sip_stream_channel_new_client(belle_sip_stack_t *stack, const char *bindip, int localport, const char *peer_cname, const char *name, int port);
belle_sip_channel_t * belle_sip_stream_channel_new_child(belle_sip_stack_t *stack, belle_sip_socket_t sock, struct sockaddr *remote_addr, socklen_t slen);

void stream_channel_close(belle_sip_stream_channel_t *obj);
int stream_channel_connect(belle_sip_stream_channel_t *obj, const struct addrinfo *ai);
/*return 0 if succeed*/
int finalize_stream_connection(belle_sip_stream_channel_t *obj, unsigned int revents, struct sockaddr *addr, socklen_t* slen);
int stream_channel_send(belle_sip_stream_channel_t *obj, const void *buf, size_t buflen);
int stream_channel_recv(belle_sip_stream_channel_t *obj, void *buf, size_t buflen);


/*for testing purpose*/
BELLESIP_INTERNAL_EXPORT void belle_sip_channel_parse_stream(belle_sip_channel_t *obj, int end_of_stream);
#endif /* STREAM_CHANNEL_H_ */
