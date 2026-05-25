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

#ifndef STREAM_CHANNEL_H_
#define STREAM_CHANNEL_H_
#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#if TARGET_OS_IPHONE
#include <CFNetwork/CFSocketStream.h>
#include <CoreFoundation/CFStream.h>
#endif

#include "belle_sip_internal.h"

struct belle_sip_stream_channel {
	belle_sip_channel_t base;
	belle_sip_resolver_context_t *socks5_proxy_resolver_ctx;
	int socks5_proxy_connected;
	int socks5_proxy_waiting_for_connect_response;
	unsigned char socks5_proxy_response[262];
	size_t socks5_proxy_response_len;
#if TARGET_OS_IPHONE
	CFReadStreamRef read_stream;
	CFWriteStreamRef write_stream;
#endif
};

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_stream_channel_t, belle_sip_channel_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

void belle_sip_stream_channel_init_client(belle_sip_stream_channel_t *obj,
                                          belle_sip_stack_t *stack,
                                          const char *bindip,
                                          int localport,
                                          const char *peer_cname,
                                          const char *dest,
                                          int port,
                                          int no_srv);

BELLESIP_EXPORT belle_sip_channel_t *belle_sip_stream_channel_new_client(belle_sip_stack_t *stack,
                                                                         const char *bindip,
                                                                         int localport,
                                                                         const char *peer_cname,
                                                                         const char *name,
                                                                         int port,
                                                                         int no_srv);
belle_sip_channel_t *belle_sip_stream_channel_new_child(belle_sip_stack_t *stack,
                                                        belle_sip_socket_t sock,
                                                        struct sockaddr *remote_addr,
                                                        socklen_t slen);

void stream_channel_close(belle_sip_stream_channel_t *obj);
int stream_channel_connect(belle_sip_stream_channel_t *obj, const struct addrinfo *ai);
int stream_channel_connect_to(belle_sip_stream_channel_t *obj, const struct addrinfo *ai);
/*return 0 if succeed*/
int finalize_stream_connection(belle_sip_stream_channel_t *obj,
                               unsigned int revents,
                               struct sockaddr *addr,
                               socklen_t *slen);
int stream_channel_socks5_proxy_enabled(const belle_sip_stream_channel_t *obj);
int stream_channel_start_socks5_connect(belle_sip_stream_channel_t *obj);
int stream_channel_process_socks5_response(belle_sip_stream_channel_t *obj);
int stream_channel_send(belle_sip_stream_channel_t *obj, const void *buf, size_t buflen);
int stream_channel_recv(belle_sip_stream_channel_t *obj, void *buf, size_t buflen);

/*for testing purpose*/
BELLESIP_EXPORT void belle_sip_channel_parse_stream(belle_sip_channel_t *obj, int end_of_stream);
#endif /* STREAM_CHANNEL_H_ */
