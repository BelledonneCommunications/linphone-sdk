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

#include "stream_channel.h"
#include "belle-sip/mainloop.h"
#include "belle_sip_internal.h"

static void set_tcp_nodelay(belle_sip_socket_t sock) {
	int tmp = 1;
	int err = bctbx_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&tmp, sizeof(tmp));
	if (err == -1) {
		belle_sip_warning("Fail to set TCP_NODELAY: %s.", belle_sip_get_socket_error_string());
	}
}

/*************TCP********/

static int stream_channel_process_data(belle_sip_stream_channel_t *obj, unsigned int revents);

static void stream_channel_uninit(belle_sip_stream_channel_t *obj) {
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t *)obj);
	if (obj->socks5_proxy_resolver_ctx) belle_sip_object_unref(obj->socks5_proxy_resolver_ctx);
	if (sock != (belle_sip_socket_t)-1) stream_channel_close(obj);
}

int stream_channel_send(belle_sip_stream_channel_t *obj, const void *buf, size_t buflen) {
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t *)obj);
	int err = (int)bctbx_send(sock, buf, buflen, 0);
	if (err == (belle_sip_socket_t)-1) {
		int errnum = get_socket_error();
		if (!belle_sip_error_code_is_would_block(errnum)) {
			belle_sip_error("Could not send stream packet on channel [%p]: %s", obj,
			                belle_sip_get_socket_error_string_from_code(errnum));
		}
		return -errnum;
	}
	return err;
}

int stream_channel_recv(belle_sip_stream_channel_t *obj, void *buf, size_t buflen) {
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t *)obj);
	int err = (int)bctbx_recv(sock, buf, buflen, 0);

	if (err == (belle_sip_socket_t)-1) {
		int errnum = get_socket_error();
		if (errnum == BCTBX_ENOTCONN) { // Do NOT treat it as an error
			belle_sip_message("Socket is not connected because of IOS10 background policy");
			obj->base.closed_by_remote = TRUE;
			return 0;
		}

		if (!belle_sip_error_code_is_would_block(errnum)) {
			belle_sip_error("Could not receive stream packet: %s", belle_sip_get_socket_error_string_from_code(errnum));
		}
		return -errnum;
	}
	return err;
}

void stream_channel_close(belle_sip_stream_channel_t *obj) {
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t *)obj);
	if (sock != (belle_sip_socket_t)-1) {
#if TARGET_OS_IPHONE
		if (obj->read_stream != NULL) {
			CFReadStreamClose(obj->read_stream);
			CFRelease(obj->read_stream);
			obj->read_stream = NULL;
		}
		if (obj->write_stream != NULL) {
			CFWriteStreamClose(obj->write_stream);
			CFRelease(obj->write_stream);
			obj->write_stream = NULL;
		}
#endif
		belle_sip_close_socket(sock);
		belle_sip_source_reset((belle_sip_source_t *)obj);
	}
}

#if TARGET_OS_IPHONE
static void stream_channel_enable_ios_background_mode(belle_sip_stream_channel_t *obj) {
	int sock = belle_sip_source_get_socket((belle_sip_source_t *)obj);

	CFStreamCreatePairWithSocket(kCFAllocatorDefault, sock, &obj->read_stream, &obj->write_stream);
	if (obj->read_stream) {
		if (!CFReadStreamSetProperty(obj->read_stream, kCFStreamNetworkServiceType, kCFStreamNetworkServiceTypeVoIP)) {
			belle_sip_warning("CFReadStreamSetProperty() could not set VoIP service type on read stream.");
		}
	} else belle_sip_warning("CFStreamCreatePairWithSocket() could not create the read stream.");
	if (obj->write_stream) {
		if (!CFWriteStreamSetProperty(obj->write_stream, kCFStreamNetworkServiceType,
		                              kCFStreamNetworkServiceTypeVoIP)) {
			belle_sip_warning("CFReadStreamSetProperty() could not set VoIP service type on write stream.");
		}
	} else belle_sip_warning("CFStreamCreatePairWithSocket() could not create the write stream.");

	if (!CFReadStreamOpen(obj->read_stream)) {
		belle_sip_warning("CFReadStreamOpen() failed.");
	}

	if (!CFWriteStreamOpen(obj->write_stream)) {
		belle_sip_warning("CFWriteStreamOpen() failed.");
	}
}

#endif

int stream_channel_socks5_proxy_enabled(const belle_sip_stream_channel_t *obj) {
	return obj->base.stack->socks5_proxy_host && obj->base.stack->socks5_proxy_host[0] != '\0';
}

static int socks5_proxy_port(const belle_sip_stack_t *stack) {
	return stack->socks5_proxy_port > 0 ? stack->socks5_proxy_port : 1080;
}

int stream_channel_start_socks5_connect(belle_sip_stream_channel_t *obj) {
	int ret;

	ret = stream_channel_send(obj, "\x05\x01\x00", 3);
	if (ret != 3) return -1;
	obj->socks5_proxy_waiting_for_connect_response = 1;
	obj->socks5_proxy_response_len = 0;
	return 0;
}

static int socks5_recv_until(belle_sip_stream_channel_t *obj, size_t expected) {
	int ret;

	if (obj->socks5_proxy_response_len >= expected) return 0;
	ret = stream_channel_recv(obj, obj->socks5_proxy_response + obj->socks5_proxy_response_len,
	                          sizeof(obj->socks5_proxy_response) - obj->socks5_proxy_response_len);
	if (ret <= 0) return -1;
	obj->socks5_proxy_response_len += (size_t)ret;
	return obj->socks5_proxy_response_len >= expected ? 0 : 1;
}

int stream_channel_process_socks5_response(belle_sip_stream_channel_t *obj) {
	unsigned char request[4 + 255 + 2];
	unsigned char atyp;
	size_t hostlen;
	size_t request_len;
	size_t expected_len;
	int ret;

	if (obj->socks5_proxy_waiting_for_connect_response == 1) {
		ret = socks5_recv_until(obj, 2);
		if (ret != 0) return ret;
		if (obj->socks5_proxy_response[0] != 0x05 || obj->socks5_proxy_response[1] != 0x00) {
			belle_sip_error("SOCKS5 proxy [%s:%i] rejected authentication method", obj->base.stack->socks5_proxy_host,
			                socks5_proxy_port(obj->base.stack));
			return -1;
		}
		hostlen = strlen(obj->base.peer_name);
		if (hostlen > 255) {
			belle_sip_error("SOCKS5 proxy connect failed: peer hostname [%s] is too long", obj->base.peer_name);
			return -1;
		}
		request[0] = 0x05; /* SOCKS version */
		request[1] = 0x01; /* CONNECT */
		request[2] = 0x00; /* reserved */
		request[3] = 0x03; /* domain name */
		request[4] = (unsigned char)hostlen;
		memcpy(request + 5, obj->base.peer_name, hostlen);
		request[5 + hostlen] = (unsigned char)((obj->base.peer_port >> 8) & 0xff);
		request[6 + hostlen] = (unsigned char)(obj->base.peer_port & 0xff);
		request_len = 7 + hostlen;
		ret = stream_channel_send(obj, request, request_len);
		if (ret != (int)request_len) return -1;
		obj->socks5_proxy_waiting_for_connect_response = 2;
		obj->socks5_proxy_response_len = 0;
		return 1;
	}
	ret = socks5_recv_until(obj, 5);
	if (ret != 0) return ret;
	atyp = obj->socks5_proxy_response[3];
	if (atyp == 0x01) expected_len = 10;
	else if (atyp == 0x03) expected_len = 5 + obj->socks5_proxy_response[4] + 2;
	else if (atyp == 0x04) expected_len = 22;
	else {
		belle_sip_error("SOCKS5 proxy [%s:%i] returned unsupported address type [%i]",
		                obj->base.stack->socks5_proxy_host, socks5_proxy_port(obj->base.stack), atyp);
		return -1;
	}
	ret = socks5_recv_until(obj, expected_len);
	if (ret != 0) return ret;
	if (obj->socks5_proxy_response[0] != 0x05 || obj->socks5_proxy_response[1] != 0x00) {
		belle_sip_error("SOCKS5 proxy [%s:%i] rejected CONNECT to [%s:%i] with status [%i]",
		                obj->base.stack->socks5_proxy_host, socks5_proxy_port(obj->base.stack), obj->base.peer_name,
		                obj->base.peer_port, obj->socks5_proxy_response[1]);
		return -1;
	}
	obj->socks5_proxy_connected = TRUE;
	obj->socks5_proxy_waiting_for_connect_response = 0;
	return 0;
}

int stream_channel_connect_to(belle_sip_stream_channel_t *obj, const struct addrinfo *ai) {
	int err;
	int tmp;
	belle_sip_socket_t sock;
	belle_sip_stack_t *stack = obj->base.stack;
	tmp = 1;

	obj->base.ai_family = ai->ai_family;
	sock = bctbx_socket(ai->ai_family, SOCK_STREAM, IPPROTO_TCP);

	if (sock == (belle_sip_socket_t)-1) {
		belle_sip_error("Could not create socket: %s", belle_sip_get_socket_error_string());
		return -1;
	}
	tmp = 1;
	err = bctbx_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&tmp, sizeof(tmp));
	if (err != 0) {
		belle_sip_error("bctbx_setsockopt SO_REUSEADDR failed: [%s]", belle_sip_get_socket_error_string());
	}

	if (stack->test_bind_port != 0) {
		struct addrinfo *bind_ai = bctbx_ip_address_to_addrinfo(
		    ai->ai_family, SOCK_STREAM, ai->ai_family == AF_INET6 ? "::0" : "0.0.0.0", stack->test_bind_port);

		err = bctbx_bind(sock, bind_ai->ai_addr, (socklen_t)bind_ai->ai_addrlen);
		if (err != 0) {
			belle_sip_error("bctbx_bind failed: [%s]", belle_sip_get_socket_error_string());
			belle_sip_close_socket(sock);
			return -1;
		} else bctbx_message("bind() on port [%i] successful", stack->test_bind_port);
		bctbx_freeaddrinfo(bind_ai);
	}

	tmp = 1;
	err = bctbx_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&tmp, sizeof(tmp));
	if (err != 0) {
		belle_sip_error("bctbx_setsockopt TCP_NODELAY failed: [%s]", belle_sip_get_socket_error_string());
	}
	belle_sip_socket_set_nonblocking(sock);
	if (obj->base.stack->dscp && obj->base.lp) {
		/*apply dscp only to channel belonging to a SIP listening point*/
		belle_sip_message("DSCP value [%i] requested for this connection.", obj->base.stack->dscp);
		belle_sip_socket_set_dscp(sock, obj->base.ai_family, obj->base.stack->dscp);
	}
	if (ai->ai_family == AF_INET6 && stack->test_bind_port == 0) {
		belle_sip_socket_enable_dual_stack(sock);
	}

	err = bctbx_connect(sock, ai->ai_addr, (socklen_t)ai->ai_addrlen);
	if (err != 0 && get_socket_error() != BELLESIP_EINPROGRESS && get_socket_error() != BELLESIP_EWOULDBLOCK) {
		belle_sip_error("stream connect failed %s", belle_sip_get_socket_error_string());
		belle_sip_close_socket(sock);
		return -1;
	}
	belle_sip_channel_set_socket((belle_sip_channel_t *)obj, sock,
	                             (belle_sip_source_func_t)stream_channel_process_data);
	belle_sip_source_set_events((belle_sip_source_t *)obj,
	                            BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_WRITE | BELLE_SIP_EVENT_ERROR);
	belle_sip_source_set_timeout_int64((belle_sip_source_t *)obj,
	                                   belle_sip_stack_get_transport_timeout(obj->base.stack));
	belle_sip_main_loop_add_source(obj->base.stack->ml, (belle_sip_source_t *)obj);
	return 0;
}

static void socks5_proxy_res_done(void *data, belle_sip_resolver_results_t *results) {
	belle_sip_stream_channel_t *obj = (belle_sip_stream_channel_t *)data;
	const struct addrinfo *ai_list;
	if (obj->socks5_proxy_resolver_ctx) {
		belle_sip_object_unref(obj->socks5_proxy_resolver_ctx);
		obj->socks5_proxy_resolver_ctx = NULL;
	}
	ai_list = belle_sip_resolver_results_get_addrinfos(results);
	if (ai_list) {
		stream_channel_connect_to(obj, ai_list);
	} else {
		belle_sip_error("%s: DNS resolution failed for SOCKS5 proxy %s", __FUNCTION__,
		                belle_sip_resolver_results_get_name(results));
		channel_set_state((belle_sip_channel_t *)obj, BELLE_SIP_CHANNEL_ERROR);
	}
}

int stream_channel_connect(belle_sip_stream_channel_t *obj, const struct addrinfo *ai) {
	belle_sip_stack_t *stack = obj->base.stack;

	if (stream_channel_socks5_proxy_enabled(obj)) {
		belle_sip_message("Resolving SOCKS5 proxy addr [%s] for channel [%p]", stack->socks5_proxy_host, obj);
		obj->socks5_proxy_resolver_ctx =
		    belle_sip_stack_resolve_a(stack, stack->socks5_proxy_host, socks5_proxy_port(stack), ai->ai_family,
		                              socks5_proxy_res_done, obj);
		if (obj->socks5_proxy_resolver_ctx) belle_sip_object_ref(obj->socks5_proxy_resolver_ctx);
		return 0;
	}
	return stream_channel_connect_to(obj, ai);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_stream_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_stream_channel_t){
    {BELLE_SIP_VPTR_INIT(belle_sip_stream_channel_t, belle_sip_channel_t, FALSE),
     (belle_sip_object_destroy_t)stream_channel_uninit, NULL, NULL, (belle_sip_object_on_first_ref_t)NULL,
     (belle_sip_object_on_last_ref_t)NULL, BELLE_SIP_DEFAULT_BUFSIZE_HINT},
    "TCP",
    1, /*is_reliable*/
    (int (*)(belle_sip_channel_t *, const struct addrinfo *))stream_channel_connect,
    (int (*)(belle_sip_channel_t *, const void *, size_t))stream_channel_send,
    (int (*)(belle_sip_channel_t *, void *, size_t))stream_channel_recv,
    (void (*)(belle_sip_channel_t *))stream_channel_close,
} BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

    int finalize_stream_connection(belle_sip_stream_channel_t *obj,
                                   unsigned int revents,
                                   struct sockaddr *addr,
                                   socklen_t *slen) {
	int err, errnum;
	socklen_t optlen = sizeof(errnum);
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t *)obj);

	if (revents == BELLE_SIP_EVENT_TIMEOUT) {
		belle_sip_warning("channel [%p]: user-defined transport timeout.", obj);
		return -1;
	}
	if (!(revents & BELLE_SIP_EVENT_WRITE) && !(revents & BELLE_SIP_EVENT_READ)) {
		belle_sip_warning("channel [%p]: getting unexpected event while connecting", obj);
		return -1;
	}

	err = bctbx_getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *)&errnum, &optlen);
	if (err != 0) {
		belle_sip_error("Failed to retrieve connection status for fd [%i]: cause [%s]", sock,
		                belle_sip_get_socket_error_string());
		return -1;
	} else {
		if (errnum == 0) {
			/*obtain bind address for client*/
			err = bctbx_getsockname(sock, addr, slen);
			if (err < 0) {
				belle_sip_error("Failed to retrieve sockname  for fd [%i]: cause [%s]", sock,
				                belle_sip_get_socket_error_string());
				return -1;
			}
#if TARGET_OS_IPHONE
			if (belle_sip_get_ios_device_major_version() < 16) { // Causes crash on app built for iOS16 with Xcode 14
				stream_channel_enable_ios_background_mode(obj);
			}
#endif
			set_tcp_nodelay(sock);
			return 0;
		} else {
			belle_sip_error("Connection failed  for fd [%i]: cause [%s]", sock,
			                belle_sip_get_socket_error_string_from_code(errnum));
			return -1;
		}
	}
}

static int stream_channel_process_data(belle_sip_stream_channel_t *obj, unsigned int revents) {
	struct sockaddr_storage ss;
	socklen_t addrlen = sizeof(ss);
	belle_sip_channel_state_t state = belle_sip_channel_get_state((belle_sip_channel_t *)obj);
	belle_sip_channel_t *base = (belle_sip_channel_t *)obj;

	/*belle_sip_message("TCP channel process_data");*/

	if (state == BELLE_SIP_CHANNEL_CONNECTING) {
		if (obj->socks5_proxy_waiting_for_connect_response) {
			int socks_ret = stream_channel_process_socks5_response(obj);
			if (socks_ret == -1) {
				channel_set_state(base, BELLE_SIP_CHANNEL_ERROR);
				return BELLE_SIP_STOP;
			}
			if (socks_ret == 1) return BELLE_SIP_CONTINUE;
			if (bctbx_getsockname(belle_sip_source_get_socket((belle_sip_source_t *)obj), (struct sockaddr *)&ss,
			                      &addrlen) == -1) {
				belle_sip_error("Failed to retrieve sockname for SOCKS5 proxied channel [%p]: %s", obj,
				                belle_sip_get_socket_error_string());
				channel_set_state(base, BELLE_SIP_CHANNEL_ERROR);
				return BELLE_SIP_STOP;
			}
			belle_sip_source_set_timeout_int64((belle_sip_source_t *)obj, -1);
			belle_sip_channel_set_ready(base, (struct sockaddr *)&ss, addrlen);
			return BELLE_SIP_CONTINUE;
		}
		if (finalize_stream_connection(obj, revents, (struct sockaddr *)&ss, &addrlen)) {
			belle_sip_error("Cannot connect to [%s://%s:%i]", belle_sip_channel_get_transport_name(base),
			                base->peer_name, base->peer_port);
			channel_set_state(base, BELLE_SIP_CHANNEL_ERROR);
			return BELLE_SIP_STOP;
		}
		belle_sip_source_set_events((belle_sip_source_t *)obj, BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_ERROR);
		if (stream_channel_socks5_proxy_enabled(obj) && !obj->socks5_proxy_connected) {
			if (stream_channel_start_socks5_connect(obj) == -1) {
				channel_set_state(base, BELLE_SIP_CHANNEL_ERROR);
				return BELLE_SIP_STOP;
			}
			return BELLE_SIP_CONTINUE;
		}
		belle_sip_source_set_timeout_int64((belle_sip_source_t *)obj, -1);
		belle_sip_channel_set_ready(base, (struct sockaddr *)&ss, addrlen);
		return BELLE_SIP_CONTINUE;
	} else if (state == BELLE_SIP_CHANNEL_READY || state == BELLE_SIP_CHANNEL_RES_IN_PROGRESS) {
		/* Because of DNS TTL timeout, the channel may enter the RES_IN_PROGRESS state temporarily while being
		 * connected.*/
		return belle_sip_channel_process_data(base, revents);
	} else {
		belle_sip_error("Unexpected event [%i], in state [%s] for channel [%p]", revents,
		                belle_sip_channel_state_to_string(state), obj);
		channel_set_state(base, BELLE_SIP_CHANNEL_ERROR);
		return BELLE_SIP_STOP;
	}
	return BELLE_SIP_CONTINUE;
}

void belle_sip_stream_channel_init_client(belle_sip_stream_channel_t *obj,
                                          belle_sip_stack_t *stack,
                                          const char *bindip,
                                          int localport,
                                          const char *peer_cname,
                                          const char *dest,
                                          int port,
                                          int no_srv) {
	belle_sip_channel_init((belle_sip_channel_t *)obj, stack, bindip, localport, peer_cname, dest, port, no_srv);
}

belle_sip_channel_t *belle_sip_stream_channel_new_client(belle_sip_stack_t *stack,
                                                         const char *bindip,
                                                         int localport,
                                                         const char *peer_cname,
                                                         const char *dest,
                                                         int port,
                                                         int no_srv) {
	belle_sip_stream_channel_t *obj = belle_sip_object_new(belle_sip_stream_channel_t);
	belle_sip_stream_channel_init_client(obj, stack, bindip, localport, peer_cname, dest, port, no_srv);
	return (belle_sip_channel_t *)obj;
}

/*child of server socket*/
belle_sip_channel_t *belle_sip_stream_channel_new_child(belle_sip_stack_t *stack,
                                                        belle_sip_socket_t sock,
                                                        struct sockaddr *remote_addr,
                                                        socklen_t slen) {
	struct sockaddr_storage localaddr;
	socklen_t local_len = sizeof(localaddr);
	belle_sip_stream_channel_t *obj;
	int err;
	int optval = 1;

	err = bctbx_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	if (err == -1) {
		belle_sip_warning("Fail to set SIP/TCP address reusable: %s.", belle_sip_get_socket_error_string());
	}

	set_tcp_nodelay(sock);

	if (bctbx_getsockname(sock, (struct sockaddr *)&localaddr, &local_len) == -1) {
		belle_sip_error("bctbx_getsockname() failed: %s", belle_sip_get_socket_error_string());
		return NULL;
	}

	obj = belle_sip_object_new(belle_sip_stream_channel_t);
	belle_sip_channel_init_with_addr((belle_sip_channel_t *)obj, stack, NULL, 0, remote_addr, slen);
	belle_sip_socket_set_nonblocking(sock);
	belle_sip_channel_set_socket((belle_sip_channel_t *)obj, sock,
	                             (belle_sip_source_func_t)stream_channel_process_data);
	belle_sip_source_set_events((belle_sip_source_t *)obj, BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_ERROR);
	belle_sip_channel_set_ready((belle_sip_channel_t *)obj, (struct sockaddr *)&localaddr, local_len);
	belle_sip_main_loop_add_source(stack->ml, (belle_sip_source_t *)obj);
	return (belle_sip_channel_t *)obj;
}
