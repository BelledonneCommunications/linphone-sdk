/*
 * Copyright (c) 2012-2021 Belledonne Communications SARL.
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

#include "belle_sip_internal.h"
#include "channel.h"

BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_udp_channel_t, belle_sip_channel_t)
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

struct belle_sip_udp_channel {
	belle_sip_channel_t base;
	belle_sip_socket_t shared_socket; /*the socket that belongs to the listening point. It is stored here because the
	                    channel parent class may erase its value in the belle_sip_source_t base class*/
};

typedef struct belle_sip_udp_channel belle_sip_udp_channel_t;

static void udp_channel_close(belle_sip_channel_t *obj) {
	belle_sip_udp_channel_t *chan = (belle_sip_udp_channel_t *)obj;
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t *)chan);
	if (chan->shared_socket == SOCKET_NOT_SET && sock != SOCKET_NOT_SET) {
		belle_sip_close_socket(belle_sip_source_get_socket((belle_sip_source_t *)obj));
	}
	belle_sip_source_reset((belle_sip_source_t *)obj);
}

static void udp_channel_uninit(belle_sip_udp_channel_t *obj) {
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t *)obj);
	if (obj->shared_socket == SOCKET_NOT_SET && sock != SOCKET_NOT_SET) {
		udp_channel_close((belle_sip_channel_t *)obj);
	} /* else no close of the socket, because it is owned by the listening point and shared between all channels */
}

static int udp_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen) {
	belle_sip_udp_channel_t *chan = (belle_sip_udp_channel_t *)obj;
	int err;
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t *)chan);
	if (sock) {
		if (chan->shared_socket != SOCKET_NOT_SET) {
			err = (int)bctbx_sendto(sock, buf, buflen, 0, obj->current_peer->ai_addr,
			                        (socklen_t)obj->current_peer->ai_addrlen);
		} else {
			/*There is no server socket, so we are in connected mode*/
			err = (int)bctbx_send(sock, buf, buflen, 0);
		}
		if (err == -1) {
			belle_sip_error("channel [%p]: could not send UDP packet because [%s]", obj,
			                belle_sip_get_socket_error_string());
			return -errno;
		}
	} else {
		belle_sip_error("channel [%p]: no socket are available to send UDP packet because [%s]", obj,
		                belle_sip_get_socket_error_string());
		err = -errno;
	}
	return err;
}

static int udp_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen) {
	belle_sip_udp_channel_t *chan = (belle_sip_udp_channel_t *)obj;
	int err;
	int errnum;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	belle_sip_socket_t sock = belle_sip_source_get_socket((belle_sip_source_t *)chan);

	err = (int)bctbx_recvfrom(sock, buf, buflen, 0, (struct sockaddr *)&addr, &addrlen);
	errnum = get_socket_error();
	if (err == -1 && errnum != BELLESIP_EWOULDBLOCK) {
		if (errnum == BCTBX_ENOTCONN) { // Do NOT treat it as an error
			belle_sip_message("Socket is not connected because of IOS10 background policy");
			obj->closed_by_remote = TRUE;
			return 0;
		}
		belle_sip_error("Could not receive UDP packet: %s", belle_sip_get_socket_error_string_from_code(errnum));
		return -errnum;
	}
	return err;
}

int udp_channel_connect(belle_sip_channel_t *obj, const struct addrinfo *ai) {
	belle_sip_udp_channel_t *chan = (belle_sip_udp_channel_t *)obj;
	struct sockaddr_storage laddr;
	int err;

	memset(&laddr, 0, sizeof(laddr));
	socklen_t lslen = sizeof(laddr);

	if (obj->local_port == BELLE_SIP_LISTENING_POINT_DONT_BIND) {
		// Creating socket for this channel here to be compatible with channel's retry mode
		int port = BELLE_SIP_LISTENING_POINT_RANDOM_PORT;
		int ai_family = obj->lp->ai_family;
		belle_sip_socket_t sock = udp_listening_point_create_udp_socket(
		    belle_sip_uri_get_host(((belle_sip_listening_point_t *)obj->lp)->listening_uri), &port, &ai_family);
		belle_sip_socket_set_nonblocking(sock);
		if (bctbx_connect(sock, ai->ai_addr, (socklen_t)ai->ai_addrlen) == -1) {
			err = -get_socket_error();
			belle_sip_error("bctbx_connect() failed for socket [%i]: cause [%s]", (int)sock,
			                belle_sip_get_socket_error_string_from_code(-err));
			return -1;
		}
		err = bctbx_getsockname(sock, (struct sockaddr *)&laddr, &lslen);
		if (err < 0) {
			belle_sip_error("Failed to retrieve sockname  for socket [%i]: cause [%s]", (int)sock,
			                belle_sip_get_socket_error_string());
			return -1;
		}
		belle_sip_channel_set_socket(BELLE_SIP_CHANNEL(chan), sock,
		                             (belle_sip_source_func_t)belle_sip_channel_process_data);
		belle_sip_source_set_events((belle_sip_source_t *)chan, BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_ERROR);
		belle_sip_main_loop_add_source(obj->lp->stack->ml, (belle_sip_source_t *)chan);
	} else {
		belle_sip_channel_set_socket(obj, chan->shared_socket, NULL);
		err = belle_sip_get_src_addr_for(ai->ai_addr, (socklen_t)ai->ai_addrlen, (struct sockaddr *)&laddr, &lslen,
		                                 obj->local_port);
		if (err == -BCTBX_ENETUNREACH || err == -BCTBX_EHOSTUNREACH) {
			return -1;
		}
	}

	belle_sip_channel_set_ready(obj, (struct sockaddr *)&laddr, lslen);
	return 0;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_udp_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_udp_channel_t){
    {BELLE_SIP_VPTR_INIT(belle_sip_udp_channel_t, belle_sip_channel_t, FALSE),
     (belle_sip_object_destroy_t)udp_channel_uninit, NULL, NULL, (belle_sip_object_on_first_ref_t)NULL,
     (belle_sip_object_on_last_ref_t)NULL, BELLE_SIP_DEFAULT_BUFSIZE_HINT},
    "UDP",
    0, /*is_reliable*/
    udp_channel_connect,
    udp_channel_send,
    udp_channel_recv,
    udp_channel_close} BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

    belle_sip_channel_t *belle_sip_channel_new_udp(
        belle_sip_stack_t *stack, int sock, const char *bindip, int localport, const char *dest, int port, int no_srv) {
	belle_sip_udp_channel_t *obj = belle_sip_object_new(belle_sip_udp_channel_t);
	belle_sip_channel_init((belle_sip_channel_t *)obj, stack, bindip, localport, NULL, dest, port, no_srv);
	obj->shared_socket = sock;
	return (belle_sip_channel_t *)obj;
}

belle_sip_channel_t *belle_sip_channel_new_udp_with_addr(
    belle_sip_stack_t *stack, int sock, const char *bindip, int localport, const struct addrinfo *peer) {
	belle_sip_udp_channel_t *obj = belle_sip_object_new(belle_sip_udp_channel_t);

	belle_sip_channel_init_with_addr((belle_sip_channel_t *)obj, stack, bindip, localport, peer->ai_addr,
	                                 (socklen_t)peer->ai_addrlen);
	obj->base.local_port = localport;
	obj->shared_socket = sock;
	/*this lookups the local address*/
	udp_channel_connect((belle_sip_channel_t *)obj, peer);
	return (belle_sip_channel_t *)obj;
}
