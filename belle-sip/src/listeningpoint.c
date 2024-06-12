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

#include "belle_sip_internal.h"
#include "channel_bank.hh"

void belle_sip_listening_point_init(belle_sip_listening_point_t *lp,
                                    belle_sip_stack_t *s,
                                    const char *address,
                                    int port) {
	char *tmp;
	belle_sip_init_sockets();
	lp->stack = s;
	lp->listening_uri = belle_sip_uri_create(NULL, address);
	belle_sip_object_ref(lp->listening_uri);
	belle_sip_uri_set_port(lp->listening_uri, port);
	belle_sip_uri_set_transport_param(lp->listening_uri,
	                                  BELLE_SIP_OBJECT_VPTR(lp, belle_sip_listening_point_t)->transport);
	tmp = belle_sip_object_to_string((belle_sip_object_t *)BELLE_SIP_LISTENING_POINT(lp)->listening_uri);
	if (strchr(address, ':')) {
		lp->ai_family = AF_INET6;
	} else {
		lp->ai_family = AF_INET;
	}
	belle_sip_message("Creating listening point [%p] on [%s]", lp, tmp);
	lp->channels = belle_sip_channel_bank_new();
	belle_sip_free(tmp);
}

static void belle_sip_listening_point_uninit(belle_sip_listening_point_t *lp) {
	char *tmp = belle_sip_object_to_string((belle_sip_object_t *)BELLE_SIP_LISTENING_POINT(lp)->listening_uri);
	belle_sip_listening_point_clean_channels(lp);
	belle_sip_object_unref(lp->channels);
	belle_sip_message("Listening point [%p] on [%s] destroyed", lp, tmp);
	belle_sip_object_unref(lp->listening_uri);
	belle_sip_free(tmp);
	lp->channel_listener = NULL; /*does not unref provider*/
	belle_sip_uninit_sockets();
	belle_sip_listening_point_set_keep_alive(lp, -1);
}

void belle_sip_listening_point_add_channel(belle_sip_listening_point_t *lp, belle_sip_channel_t *chan) {
	chan->lp = lp;
	belle_sip_channel_add_listener(chan, lp->channel_listener); /*add channel listener*/
	belle_sip_channel_bank_add_channel(lp->channels, chan);
	belle_sip_object_unref(chan);
}

belle_sip_channel_t *belle_sip_listening_point_create_channel(belle_sip_listening_point_t *obj,
                                                              const belle_sip_hop_t *hop) {
	belle_sip_channel_t *chan = BELLE_SIP_OBJECT_VPTR(obj, belle_sip_listening_point_t)->create_channel(obj, hop);
	if (chan) {
		if (hop->channel_bank_identifier)
			belle_sip_message("channel created within bank identifier %s", hop->channel_bank_identifier);
		belle_sip_channel_set_bank_identifier(chan, hop->channel_bank_identifier);
		belle_sip_listening_point_add_channel(obj, chan);
	}
	return chan;
}

void belle_sip_listening_point_remove_channel(belle_sip_listening_point_t *lp, belle_sip_channel_t *chan) {
	belle_sip_channel_remove_listener(chan, lp->channel_listener);
	belle_sip_channel_bank_remove_channel(lp->channels, chan);
}

void belle_sip_listening_point_clean_channels(belle_sip_listening_point_t *lp) {
	int existing_channels = belle_sip_listening_point_get_channel_count(lp);

	if (existing_channels > 0) {
		belle_sip_message("Listening point destroying [%i] channels", existing_channels);
	}
	belle_sip_channel_bank_clear_all(lp->channels);
}

static int remove_if_unreliable(belle_sip_channel_t *chan, void *user_data) {
	belle_sip_listening_point_t *lp = (belle_sip_listening_point_t *)user_data;
	uint64_t current_time = belle_sip_time_ms();
	if (chan->state == BELLE_SIP_CHANNEL_READY) {
		if (current_time - chan->last_recv_time > (uint64_t)(lp->stack->unreliable_transport_timeout * 1000)) {
			belle_sip_channel_force_close(chan);
			return 1;
		}
	}
	return 0;
}

void belle_sip_listening_point_clean_unreliable_channels(belle_sip_listening_point_t *lp) {
	size_t count = 0;

	if (lp->stack->unreliable_transport_timeout <= 0) return;
	count = belle_sip_channel_bank_remove_if(lp->channels, remove_if_unreliable, lp);
	if (count != 0) {
		belle_sip_message("belle_sip_listening_point_clean_unreliable_channels() has closed [%i] channels.",
		                  (int)count);
	}
}

int belle_sip_listening_point_get_channel_count(const belle_sip_listening_point_t *lp) {
	return (int)belle_sip_channel_bank_get_count(lp->channels);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_listening_point_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_listening_point_t){
    BELLE_SIP_VPTR_INIT(belle_sip_listening_point_t, belle_sip_object_t, FALSE),
    (belle_sip_object_destroy_t)belle_sip_listening_point_uninit,
    NULL,
    NULL,
    (belle_sip_object_on_first_ref_t)NULL,
    (belle_sip_object_on_last_ref_t)NULL,
    BELLE_SIP_DEFAULT_BUFSIZE_HINT},
    NULL,
    NULL BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

    const char *belle_sip_listening_point_get_ip_address(const belle_sip_listening_point_t *lp) {
	return belle_sip_uri_get_host(lp->listening_uri);
}

int belle_sip_listening_point_get_port(const belle_sip_listening_point_t *lp) {
	return belle_sip_uri_get_listening_port(lp->listening_uri);
}

const char *belle_sip_listening_point_get_transport(const belle_sip_listening_point_t *lp) {
	return belle_sip_uri_get_transport_param(lp->listening_uri);
}

const belle_sip_uri_t *belle_sip_listening_point_get_uri(const belle_sip_listening_point_t *lp) {
	return lp->listening_uri;
}
int belle_sip_listening_point_get_well_known_port(const char *transport) {
	int well_known_port = belle_sip_stack_get_well_known_port();
	int tls_well_known_port = belle_sip_stack_get_well_known_port_tls();
	if (strcasecmp(transport, "UDP") == 0 || strcasecmp(transport, "TCP") == 0) {
		return well_known_port;

	} else if (strcasecmp(transport, "DTLS") == 0 || strcasecmp(transport, "TLS") == 0) {
		return tls_well_known_port;
	} else {
		belle_sip_error("belle_sip_listening_point_get_well_known_port() : Not valid transport value : %s", transport);
		return -1;
	}
}

belle_sip_channel_t *_belle_sip_listening_point_get_channel_by_addrinfo(belle_sip_listening_point_t *lp,
                                                                        const struct addrinfo *addr) {
	return belle_sip_channel_bank_find_by_addrinfo(lp->channels, addr);
}

belle_sip_channel_t *belle_sip_listening_point_get_channel(belle_sip_listening_point_t *lp,
                                                           const belle_sip_hop_t *hop) {
	return belle_sip_channel_bank_find(lp->channels, lp->ai_family, hop);
}

belle_sip_channel_t *belle_sip_listening_point_find_channel_by_local_uri(belle_sip_listening_point_t *lp,
                                                                         const belle_sip_uri_t *uri) {
	const char *transport = belle_sip_uri_is_secure(uri) ? "TLS" : belle_sip_uri_get_transport_param(uri);
	if (transport == NULL) transport = "udp";
	if (strcasecmp(belle_sip_listening_point_get_transport(lp), transport) != 0) return NULL;
	return belle_sip_channel_bank_find_by_local_uri(lp->channels, uri);
}

struct keep_alive_context {
	belle_sip_list_t *to_be_closed;
	int doubled;
};

static void send_keep_alive(belle_sip_channel_t *chan, void *user_data) {
	struct keep_alive_context *ctx = (struct keep_alive_context *)user_data;
	if (chan->state == BELLE_SIP_CHANNEL_READY && chan->out_state == OUTPUT_STREAM_IDLE &&
	    belle_sip_channel_send_keep_alive(chan, ctx->doubled) == -1) { /*only send keep alive if ready*/
		ctx->to_be_closed = belle_sip_list_prepend(ctx->to_be_closed, chan);
	}
}

static void set_error_and_close(belle_sip_channel_t *channel) {
	channel_set_state(channel, BELLE_SIP_CHANNEL_ERROR);
	belle_sip_channel_close(channel);
}

static void _belle_sip_listening_point_send_keep_alive(belle_sip_listening_point_t *lp, int doubled) {
	struct keep_alive_context ctx = {NULL, doubled};

	belle_sip_channel_bank_for_each(lp->channels, send_keep_alive, &ctx);
	bctbx_list_free_with_data(ctx.to_be_closed, (bctbx_list_free_func)set_error_and_close);
}

static void assign_simulated_recv_return(belle_sip_channel_t *chan, void *user_data) {
	belle_sip_channel_set_simulated_recv_return(chan, *(int *)user_data);
}

void belle_sip_listening_point_set_simulated_recv_return(belle_sip_listening_point_t *lp, int recv_error) {
	belle_sip_channel_bank_for_each(lp->channels, assign_simulated_recv_return, &recv_error);
}

void belle_sip_listening_point_send_keep_alive(belle_sip_listening_point_t *lp) {
	_belle_sip_listening_point_send_keep_alive(lp, TRUE);
}

void belle_sip_listening_point_send_pong(belle_sip_listening_point_t *lp) {
	_belle_sip_listening_point_send_keep_alive(lp, FALSE);
}

static int keep_alive_timer_func(void *user_data, unsigned int events) {
	belle_sip_listening_point_t *lp = (belle_sip_listening_point_t *)user_data;
	belle_sip_listening_point_send_keep_alive(lp);
	return BELLE_SIP_CONTINUE_WITHOUT_CATCHUP;
}

void belle_sip_listening_point_set_keep_alive(belle_sip_listening_point_t *lp, int ms) {
	if (ms <= 0) {
		if (lp->keep_alive_timer) {
			belle_sip_main_loop_remove_source(lp->stack->ml, lp->keep_alive_timer);
			belle_sip_object_unref(lp->keep_alive_timer);
			lp->keep_alive_timer = NULL;
		}
		return;
	}

	if (!lp->keep_alive_timer) {
		lp->keep_alive_timer =
		    belle_sip_main_loop_create_timeout(lp->stack->ml, keep_alive_timer_func, lp, ms, "keep alive");
	} else {
		belle_sip_source_set_timeout_int64(lp->keep_alive_timer, ms);
	}
	return;
}

int belle_sip_listening_point_get_keep_alive(const belle_sip_listening_point_t *lp) {
	return lp->keep_alive_timer ? (int)belle_sip_source_get_timeout_int64(lp->keep_alive_timer) : -1;
}

void belle_sip_listening_point_set_channel_listener(belle_sip_listening_point_t *lp,
                                                    belle_sip_channel_listener_t *channel_listener) {
	lp->channel_listener = channel_listener;
}
