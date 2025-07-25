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
#include <ctype.h>
#include <limits.h>
#include <wchar.h>

#ifdef __ANDROID__
#include "wakelock_internal.h"
#endif

#define BELLE_SIP_CHANNEL_INVOKE_MESSAGE_HEADERS_LISTENERS(channel, msg)                                               \
	BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(channel->full_listeners, belle_sip_channel_listener_t, on_message_headers,    \
	                                     channel, msg)

#define BELLE_SIP_CHANNEL_INVOKE_SENDING_LISTENERS(channel, msg)                                                       \
	BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(channel->full_listeners, belle_sip_channel_listener_t, on_sending, channel,   \
	                                     msg)

#define BELLE_SIP_CHANNEL_INVOKE_STATE_LISTENERS(channel, state)                                                       \
	BELLE_SIP_INVOKE_LISTENERS_REVERSE_ARG1_ARG2(channel->full_listeners, belle_sip_channel_listener_t,                \
	                                             on_state_changed, channel, state)                                     \
	BELLE_SIP_INVOKE_LISTENERS_REVERSE_ARG1_ARG2(channel->state_listeners, belle_sip_channel_listener_t,               \
	                                             on_state_changed, channel, state)

#define BELLE_SIP_CHANNEL_CLEAR_TIMER(obj, timer)                                                                      \
	do {                                                                                                               \
		if (obj->timer) {                                                                                              \
			belle_sip_main_loop_remove_source(obj->stack->ml, obj->timer);                                             \
			belle_sip_object_unref(obj->timer);                                                                        \
			obj->timer = NULL;                                                                                         \
		}                                                                                                              \
	} while (0)

static void channel_prepare_continue(belle_sip_channel_t *obj);
static void channel_process_queue(belle_sip_channel_t *obj);
static void channel_begin_send_background_task(belle_sip_channel_t *obj);
static void channel_end_send_background_task(belle_sip_channel_t *obj);
static void channel_begin_recv_background_task(belle_sip_channel_t *obj);
static void channel_end_recv_background_task(belle_sip_channel_t *obj);
static void channel_process_queue(belle_sip_channel_t *obj);
static char *make_logbuf(belle_sip_channel_t *obj,
                         belle_sip_log_level level,
                         const char *buffer,
                         size_t size,
                         belle_sip_direction_t direction);
static void channel_remove_listener(belle_sip_channel_t *obj, belle_sip_channel_listener_t *l);
static void free_ewouldblock_buffer(belle_sip_channel_t *obj);

const char *belle_sip_channel_state_to_string(belle_sip_channel_state_t state) {
	switch (state) {
		case BELLE_SIP_CHANNEL_INIT:
			return "INIT";
		case BELLE_SIP_CHANNEL_RES_IN_PROGRESS:
			return "RES_IN_PROGRESS";
		case BELLE_SIP_CHANNEL_RES_DONE:
			return "RES_DONE";
		case BELLE_SIP_CHANNEL_CONNECTING:
			return "CONNECTING";
		case BELLE_SIP_CHANNEL_RETRY:
			return "RETRY";
		case BELLE_SIP_CHANNEL_READY:
			return "READY";
		case BELLE_SIP_CHANNEL_ERROR:
			return "ERROR";
		case BELLE_SIP_CHANNEL_DISCONNECTED:
			return "DISCONNECTED";
	}
	return "BAD";
}

static belle_sip_list_t *
for_each_weak_unref_free(belle_sip_list_t *l, belle_sip_object_destroy_notify_t notify, void *ptr) {
	belle_sip_list_t *elem, *next;
	for (elem = l; elem != NULL; elem = next) {
		next = elem->next;
		belle_sip_object_weak_unref(elem->data, notify, ptr);
		belle_sip_free(elem);
	}
	return NULL;
}

static size_t belle_sip_channel_input_stream_get_readable_length(const belle_sip_channel_input_stream_t *input_stream) {
	return input_stream->write_ptr - input_stream->read_ptr;
}

static void belle_sip_channel_input_stream_rewind(belle_sip_channel_input_stream_t *input_stream) {
	int remaining;

	remaining = (int)(belle_sip_channel_input_stream_get_readable_length(input_stream));
	if (remaining > 0) {
		/* copy remaning bytes at top of buffer*/
		memmove(input_stream->buff, input_stream->read_ptr, remaining);
		input_stream->read_ptr = input_stream->buff;
		input_stream->write_ptr = input_stream->buff + remaining;
		*input_stream->write_ptr = '\0';
	} else {
		input_stream->read_ptr = input_stream->write_ptr = input_stream->buff;
	}
}

static void belle_sip_channel_input_stream_reset(belle_sip_channel_input_stream_t *input_stream) {
	belle_sip_channel_input_stream_rewind(input_stream);
	input_stream->state = WAITING_MESSAGE_START;
	if (input_stream->msg != NULL) belle_sip_object_unref(input_stream->msg);
	input_stream->msg = NULL;
	input_stream->chuncked_mode = FALSE;
	input_stream->content_length = -1;
}

static size_t belle_sip_channel_input_stream_get_free_length(belle_sip_channel_input_stream_t *input_stream) {
	return sizeof(input_stream->buff) - (input_stream->write_ptr - input_stream->buff);
}

static void belle_sip_channel_destroy(belle_sip_channel_t *obj) {
	belle_sip_channel_input_stream_reset(&obj->input_stream);
	if (obj->peer_cname) belle_sip_free(obj->peer_cname);
	belle_sip_free(obj->peer_name);
	if (obj->local_ip) belle_sip_free(obj->local_ip);
	obj->state_listeners =
	    for_each_weak_unref_free(obj->state_listeners, (belle_sip_object_destroy_notify_t)channel_remove_listener, obj);
	obj->full_listeners =
	    for_each_weak_unref_free(obj->full_listeners, (belle_sip_object_destroy_notify_t)channel_remove_listener, obj);

	if (obj->resolver_ctx != NULL) {
		belle_sip_resolver_context_cancel(obj->resolver_ctx);
		belle_sip_object_unref(obj->resolver_ctx);
	}
	SET_OBJECT_PROPERTY(obj, resolver_results, NULL);
	if (obj->static_peer_list) bctbx_freeaddrinfo(obj->static_peer_list);
	BELLE_SIP_CHANNEL_CLEAR_TIMER(obj, inactivity_timer);
	BELLE_SIP_CHANNEL_CLEAR_TIMER(obj, dns_ttl_timer);
	BELLE_SIP_CHANNEL_CLEAR_TIMER(obj, expect_pong_timer);
	if (obj->public_ip) belle_sip_free(obj->public_ip);
	if (obj->outgoing_messages) belle_sip_list_free_with_data(obj->outgoing_messages, belle_sip_object_unref);
	if (obj->incoming_messages) belle_sip_list_free_with_data(obj->incoming_messages, belle_sip_object_unref);
	free_ewouldblock_buffer(obj);
	if (obj->cur_out_message) {
		belle_sip_object_unref(obj->cur_out_message);
		obj->cur_out_message = NULL;
	}
	channel_end_send_background_task(obj);
	channel_end_recv_background_task(obj);
	if (obj->bank_identifier) bctbx_free(obj->bank_identifier);
	/*normally this should do nothing because it sould have been terminated already,
	    however leaving a background task open is so dangerous that we have to be paranoid*/
	belle_sip_message("Channel [%p] destroyed", obj);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_channel_t){
    BELLE_SIP_VPTR_INIT(belle_sip_channel_t, belle_sip_source_t, FALSE),
    (belle_sip_object_destroy_t)belle_sip_channel_destroy,
    NULL, /*clone*/
    NULL, /*marshal*/
    NULL, /*on_first_ref*/
    NULL, /*on_last_ref*/
    BELLE_SIP_DEFAULT_BUFSIZE_HINT},
    NULL, /* transport */
    0,    /* reliable */
    NULL, /* connect */
    NULL, /* channel_send */
    NULL, /* channel_recv */
    NULL  /* close */
    BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

    static void fix_incoming_via(belle_sip_request_t *msg, const struct addrinfo *origin) {
	char received[NI_MAXHOST];
	char rport[NI_MAXSERV];
	belle_sip_header_via_t *via;
	int err;
	struct sockaddr_storage saddr;
	socklen_t slen = sizeof(saddr);

	if (!origin) {
		belle_sip_warning("cannot fix via for message [%p], probably a test", msg);
		return;
	}
	bctbx_sockaddr_remove_v4_mapping(origin->ai_addr, (struct sockaddr *)&saddr, &slen);
	err = bctbx_getnameinfo((struct sockaddr *)&saddr, slen, received, sizeof(received), rport, sizeof(rport),
	                        NI_NUMERICHOST | NI_NUMERICSERV);
	if (err != 0) {
		belle_sip_error("fix_via: getnameinfo() failed: %s", gai_strerror(errno));
		return;
	}
	via = BELLE_SIP_HEADER_VIA(belle_sip_message_get_header((belle_sip_message_t *)msg, "via"));
	if (via) {
		const char *host = belle_sip_header_via_get_host(via);

		if (strcmp(host, received) != 0) belle_sip_header_via_set_received(via, received);

		if (belle_sip_parameters_has_parameter(BELLE_SIP_PARAMETERS(via), "rport")) {
			int port = belle_sip_header_via_get_listening_port(via);
			int rport_int = atoi(rport);
			if (rport_int != port) belle_sip_header_via_set_rport(via, atoi(rport));
		}
	}
}

// token =  1*(alphanum / "-" / "." / "!" / "%" / "*" / "_" / "+" / "`" / "'" / "~" )

static int is_token(const char *buff, size_t bufflen) {
	size_t i;
	for (i = 0; i < bufflen && buff[i] != '\0'; i++) {
		switch (buff[i]) {
			case '-':
			case '.':
			case '!':
			case '%':
			case '*':
			case '_':
			case '+':
			case '`':
			case '\'':
			case '~':
				break;
			default:
				if ((buff[i] >= '0' && buff[i] <= '9') || (buff[i] >= 'A' && buff[i] <= 'Z') ||
				    (buff[i] >= 'a' && buff[i] <= 'z') || (buff[i] == '\0'))
					continue;
				else return 0;
		}
	}
	return 1;
}

static int get_message_start_pos(char *buff, size_t bufflen) {
	/*FIXME still to optimize and better test, specially REQUEST PATH and error path*/
	int i;
	int res = 0;
	int status_code;
	char method[17] = {0};
	char saved_char1;
	char sip_version[10] = {0};
	size_t saved_char1_index;

	for (i = 0; i < (int)bufflen - 12; i++) { /*9=strlen( SIP/2.0\r\n)*/
		switch (buff[i]) {                    /*to avoid this character to be ignored by scanf*/
			case '\r':
			case '\n':
			case ' ':
			case '\t':
				continue;
			default:
				break;
		}
		saved_char1_index = bufflen - 1;
		saved_char1 = buff[saved_char1_index]; /*make sure buff is null terminated*/
		buff[saved_char1_index] = '\0';
		res = sscanf(buff + i, "SIP/2.0 %d ", &status_code);
		if (res != 1) res = sscanf(buff + i, "HTTP/1.%*i %d ", &status_code); /*might be HTTP ?*/
		if (res != 1) {
			res = sscanf(buff + i, "%16s %*s %9s\r\n", method, sip_version) == 2 && is_token(method, sizeof(method)) &&
			      (strcmp("SIP/2.0", sip_version) == 0 || strncmp("HTTP/1.", sip_version, strlen("HTTP/1.")) == 0);
		}
		buff[saved_char1_index] = saved_char1;
		if (res == 1) return i;
	}
	return -1;
}

void belle_sip_channel_set_public_ip_port(belle_sip_channel_t *obj, const char *public_ip, int port) {
	if (obj->public_ip) {
		int ip_changed = 0;
		int port_changed = 0;

		if (public_ip && strcmp(obj->public_ip, public_ip) != 0) {
			ip_changed = 1;
		}
		if (port != obj->public_port) {
			port_changed = 1;
		}
		if (ip_changed || port_changed) {
			belle_sip_warning("channel [%p]: public ip is changed from [%s:%i] to [%s:%i]", obj, obj->public_ip,
			                  obj->public_port, public_ip, port);
		}
		belle_sip_free(obj->public_ip);
		obj->public_ip = NULL;
	} else if (public_ip) {
		belle_sip_message("channel [%p]: discovered public ip and port are [%s:%i]", obj, public_ip, port);
	}
	if (public_ip) {
		obj->public_ip = belle_sip_strdup(public_ip);
	}
	obj->public_port = port;
}

static void belle_sip_channel_learn_public_ip_port(belle_sip_channel_t *obj, belle_sip_response_t *resp) {
	belle_sip_header_via_t *via = belle_sip_message_get_header_by_type(resp, belle_sip_header_via_t);
	const char *received;
	int rport;

	if (!via) {
		belle_sip_error("channel [%p]: no via in response.", obj);
		return;
	}

	if (!(received = belle_sip_header_via_get_received(via))) {
		/*use address from via*/;
		received = belle_sip_header_via_get_host(via);
	}

	rport = belle_sip_header_via_get_rport(via);
	if (rport <= 0) {
		/* no rport, the via port might be good then*/
		rport = belle_sip_header_via_get_listening_port(via);
	}
	belle_sip_channel_set_public_ip_port(obj, received, rport);

	obj->learnt_ip_port = TRUE;
}

static int pong_timeout(void *data, unsigned events) {
	belle_sip_channel_t *obj = (belle_sip_channel_t *)data;
	if (obj->state == BELLE_SIP_CHANNEL_READY && obj->pong_support_confirmed) {
		belle_sip_warning("channel[%p]: no pong received since ping, channel is going to be closed.", obj);
		channel_set_state(obj, BELLE_SIP_CHANNEL_ERROR);
		belle_sip_channel_close(obj);
	}
	return BELLE_SIP_STOP;
}

static void expect_pong(belle_sip_channel_t *obj) {
	BELLE_SIP_CHANNEL_CLEAR_TIMER(obj, expect_pong_timer);
	obj->expect_pong_timer = belle_sip_main_loop_create_timeout(
	    obj->stack->ml, pong_timeout, obj, belle_sip_stack_get_pong_timeout(obj->stack) * 1000, "pong timeout");
}

int belle_sip_channel_send_keep_alive(belle_sip_channel_t *obj, int doubled) {
	/*keep alive*/
	const char *crlfcrlf = "\r\n\r\n";
	size_t size = strlen(crlfcrlf);
	if (!doubled) size /= 2;
	int err = belle_sip_channel_send(obj, crlfcrlf, size);

	if (err <= 0 && !belle_sip_error_code_is_would_block(-err) && err != -EINTR) {
		belle_sip_error("channel [%p]: could not send [%u] bytes of keep alive from [%s://%s:%i]  to [%s:%i]", obj,
		                (unsigned int)size, belle_sip_channel_get_transport_name(obj), obj->local_ip, obj->local_port,
		                obj->peer_name, obj->peer_port);

		return -1;
	} else {
		belle_sip_message("channel [%p]: keep alive sent to [%s://%s:%i]", obj,
		                  belle_sip_channel_get_transport_name(obj), obj->peer_name, obj->peer_port);
		if (doubled && obj->ping_pong_enabled) expect_pong(obj);
		return 0;
	}
}

void belle_sip_channel_enable_ping_pong(belle_sip_channel_t *obj, int enabled) {
	obj->ping_pong_enabled = (unsigned char)enabled;
	if (!!!belle_sip_stack_ping_pong_verification_enabled(obj->stack)) {
		belle_sip_message("Channel[%p]: RFC5626 pong support is assumed to be supported by the server without "
		                  "performing any sanity checks.",
		                  obj);
		obj->pong_support_confirmed = TRUE;
	}
}

int belle_sip_channel_ping_pong_enabled(const belle_sip_channel_t *obj) {
	return obj->ping_pong_enabled;
}

static void uncompress_body_if_required(belle_sip_message_t *msg) {
	belle_sip_body_handler_t *bh = belle_sip_message_get_body_handler(msg);
	belle_sip_memory_body_handler_t *mbh = NULL;
	belle_sip_header_t *ceh = NULL;
	size_t body_len = 0;

	if (bh != NULL) {
		body_len = belle_sip_message_get_body_size(msg);
		ceh = belle_sip_message_get_header(msg, "Content-Encoding");
	}
	if ((body_len > 0) && (ceh != NULL)) {
		const char *content_encoding = belle_sip_header_get_unparsed_value(ceh);
		if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(bh, belle_sip_memory_body_handler_t)) {
			mbh = BELLE_SIP_MEMORY_BODY_HANDLER(bh);
			if (belle_sip_memory_body_handler_unapply_encoding(mbh, content_encoding) == 0) {
				belle_sip_header_content_type_t *content_type =
				    belle_sip_message_get_header_by_type(msg, belle_sip_header_content_type_t);
				belle_sip_header_content_length_t *content_length =
				    belle_sip_message_get_header_by_type(msg, belle_sip_header_content_length_t);
				belle_sip_header_content_length_set_content_length(
				    content_length, belle_sip_body_handler_get_size(BELLE_SIP_BODY_HANDLER(mbh)));
				belle_sip_header_t *xceh =
				    belle_sip_header_create("X-BelleSip-Removed-Content-Encoding", content_encoding);
				belle_sip_message_remove_header_from_ptr(msg, ceh);
				belle_sip_message_add_header(BELLE_SIP_MESSAGE(msg), xceh);
				if (content_type && (strcmp(belle_sip_header_content_type_get_type(content_type), "multipart") == 0)) {
					const char *boundary =
					    belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(content_type), "boundary");
					bh = (belle_sip_body_handler_t *)belle_sip_multipart_body_handler_new_from_buffer(
					    belle_sip_memory_body_handler_get_buffer(mbh),
					    belle_sip_body_handler_get_size((belle_sip_body_handler_t *)mbh), boundary);
					belle_sip_message_set_body_handler(msg, bh);
				}
			}
		} else {
			belle_sip_warning("message [%p] has Content-Encoding [%s] that cannot be unapplied", msg, content_encoding);
		}
	}
}

static void belle_sip_channel_message_ready(belle_sip_channel_t *obj) {
	belle_sip_message_t *msg = obj->input_stream.msg;
	belle_sip_body_handler_t *bh = belle_sip_message_get_body_handler(msg);
	if (bh) belle_sip_body_handler_end_transfer(bh);
	if (belle_sip_message_is_response(msg)) belle_sip_channel_learn_public_ip_port(obj, BELLE_SIP_RESPONSE(msg));
	uncompress_body_if_required(msg);
	belle_sip_message_set_channel_bank_identifier(msg, obj->bank_identifier);
	obj->incoming_messages = belle_sip_list_append(obj->incoming_messages, belle_sip_object_ref(msg));
	belle_sip_channel_input_stream_reset(&obj->input_stream);
	obj->inhibit_input_logging_buffer = 0;
}

static void feed_body(belle_sip_channel_t *obj, size_t len) {
	belle_sip_message_t *msg = obj->input_stream.msg;
	belle_sip_body_handler_t *bh = belle_sip_message_get_body_handler(msg);
	belle_sip_body_handler_recv_chunk(bh, msg, (uint8_t *)obj->input_stream.read_ptr, len);
	obj->input_stream.read_ptr += len;
	belle_sip_channel_input_stream_rewind(&obj->input_stream);
}

/*returns TRUE if a body is expected, and initialize a few things in the input stream context*/
static int check_body(belle_sip_channel_t *obj) {
	belle_sip_message_t *msg = obj->input_stream.msg;
	belle_sip_header_content_length_t *content_length_header =
	    belle_sip_message_get_header_by_type(msg, belle_sip_header_content_length_t);
	int expect_body = FALSE;

	obj->input_stream.content_length =
	    content_length_header ? belle_sip_header_content_length_get_content_length(content_length_header) : 0;

	expect_body = obj->input_stream.content_length > 0;

	if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(msg, belle_http_response_t) ||
	    BELLE_SIP_OBJECT_IS_INSTANCE_OF(msg, belle_http_request_t)) {
		/*http chunked mode handling*/
		if (belle_sip_message_get_header_by_type(msg, belle_sip_header_content_type_t) != NULL) {
			belle_sip_header_t *transfer_encoding = belle_sip_message_get_header(msg, "Transfer-Encoding");

			if (transfer_encoding) {
				const char *value = belle_sip_header_get_unparsed_value(transfer_encoding);
				if (strstr(value, "chunked") != 0) {
					obj->input_stream.chuncked_mode = 1;
					obj->input_stream.content_length = 0;
					obj->input_stream.chunk_size = -1;
					obj->input_stream.chunk_read_size = 0;
				}
			}
			expect_body = TRUE;
		}
	}
	if (expect_body) {
		belle_sip_body_handler_t *bh;
		// Should notify the listeners
		BELLE_SIP_CHANNEL_INVOKE_MESSAGE_HEADERS_LISTENERS(obj, msg);
		// Check if the listener has setup a body handler, otherwise create a default one
		bh = belle_sip_message_get_body_handler(msg);
		if (!bh) {
			belle_sip_header_t *content_encoding = belle_sip_message_get_header(msg, "Content-Encoding");
			belle_sip_header_content_type_t *content_type =
			    belle_sip_message_get_header_by_type(msg, belle_sip_header_content_type_t);
			if (content_encoding) {
				bh = (belle_sip_body_handler_t *)belle_sip_memory_body_handler_new(NULL, NULL);
				belle_sip_body_handler_add_header(bh, content_encoding);
			} else if (content_type &&
			           (strcmp(belle_sip_header_content_type_get_type(content_type), "multipart") == 0)) {
				const char *boundary =
				    belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(content_type), "boundary");
				bh = (belle_sip_body_handler_t *)belle_sip_multipart_body_handler_new(NULL, NULL, NULL, boundary);
			} else {
				bh = (belle_sip_body_handler_t *)belle_sip_memory_body_handler_new(NULL, NULL);
			}
			belle_sip_body_handler_set_size(bh, obj->input_stream.content_length);
			belle_sip_body_handler_add_header(bh, BELLE_SIP_HEADER(content_length_header));
			belle_sip_body_handler_add_header(bh, BELLE_SIP_HEADER(content_type));
			belle_sip_message_set_body_handler(msg, bh);
		}
		belle_sip_body_handler_begin_recv_transfer(bh);
	}
	return expect_body;
}

static int acquire_body_simple(belle_sip_channel_t *obj, int end_of_stream) {
	size_t content_length = obj->input_stream.content_length;
	size_t to_read = obj->input_stream.write_ptr - obj->input_stream.read_ptr;
	belle_sip_message_t *msg = obj->input_stream.msg;
	belle_sip_body_handler_t *bh = belle_sip_message_get_body_handler(msg);
	size_t cursize = belle_sip_body_handler_get_transfered_size(bh);

	if ((cursize == 0) && (to_read == 0)) {
		/**
		 * No data has been received yet, so do not call feed_body() with a size
		 * of 0 that is meaning that the transfer is finished.
		 */
	} else {
		to_read = MIN(content_length - cursize, to_read);
		feed_body(obj, to_read);
	}

	if (end_of_stream || belle_sip_body_handler_get_transfered_size(bh) >= content_length) {
		/*great body completed*/
		belle_sip_message("channel [%p] read [%i] bytes of body from [%s:%i]", obj, (int)content_length, obj->peer_name,
		                  obj->peer_port);
		belle_sip_channel_message_ready(obj);
		return BELLE_SIP_CONTINUE;
	}
	/*body is not finished, we need more data*/
	return BELLE_SIP_STOP;
}

static int acquire_chuncked_body(belle_sip_channel_t *obj) {
	belle_sip_channel_input_stream_t *st = &obj->input_stream;
	int readsize;
	do {
		if (st->chunk_size == -1) {
			char *tmp;
			/*belle_sip_message("seeing: %s",st->read_ptr);*/
			while ((tmp = strstr(st->read_ptr, "\r\n")) == st->read_ptr) { /*skip \r\n*/
				st->read_ptr += 2;
			}

			if (tmp != NULL) {
				/*the chunk length is there*/
				long chunksize = strtol(st->read_ptr, NULL, 16);
				if (chunksize >= 0 && chunksize != LONG_MAX) {
					if (chunksize == 0) {
						belle_sip_message("Got end of chunked body");
						st->read_ptr = tmp + 4; /*last chunk indicator finishes with two \r\n*/
						if (st->read_ptr > st->write_ptr) st->read_ptr = st->write_ptr;
						belle_sip_channel_message_ready(obj);
						return BELLE_SIP_CONTINUE;
					} else {
						belle_sip_message("Will get a chunk of %i bytes", (int)chunksize);
						st->chunk_size = chunksize;
						st->chunk_read_size = 0;
						st->read_ptr = tmp + 2;
					}
				} else {
					belle_sip_error("Chunk parse error");
					belle_sip_channel_input_stream_reset(st);
					obj->inhibit_input_logging_buffer = 0;
					return BELLE_SIP_CONTINUE;
				}
			} else {
				/*need more data*/
				return BELLE_SIP_STOP;
			}
		}
		readsize = MIN((int)(st->write_ptr - st->read_ptr), st->chunk_size - st->chunk_read_size);
		if (readsize > 0) {
			feed_body(obj, readsize);
			st->chunk_read_size += readsize;
		}
		if (st->chunk_size == st->chunk_read_size) {
			/*we have a chunk completed*/
			st->content_length += st->chunk_size;
			belle_sip_message("Chunk of [%i] bytes completed", st->chunk_size);
			st->chunk_size = -1; /*wait for next chunk indicator*/
		} else {
			/*need more data*/
			return BELLE_SIP_STOP;
		}
	} while (st->write_ptr - st->read_ptr > 0); /*no need to continue if nothing to read*/
	return BELLE_SIP_STOP;
}

static int acquire_body(belle_sip_channel_t *obj, int end_of_stream) {
	if (obj->input_stream.chuncked_mode) return acquire_chuncked_body(obj);
	else return acquire_body_simple(obj, end_of_stream);
}

static void notify_incoming_messages(belle_sip_channel_t *obj) {
	belle_sip_list_t *elem, *l_it;

	belle_sip_list_t *listeners =
	    belle_sip_list_copy_with_data(obj->full_listeners, (void *(*)(void *))belle_sip_object_ref);

	for (l_it = listeners; l_it != NULL; l_it = l_it->next) {
		belle_sip_channel_listener_t *listener = (belle_sip_channel_listener_t *)l_it->data;
		for (elem = obj->incoming_messages; elem != NULL; elem = elem->next) {
			belle_sip_message_t *msg = (belle_sip_message_t *)elem->data;
			BELLE_SIP_INTERFACE_METHODS_TYPE(belle_sip_channel_listener_t) * methods;
			methods = BELLE_SIP_INTERFACE_GET_METHODS(listener, belle_sip_channel_listener_t);
			if (methods->on_message) methods->on_message(listener, obj, msg);
		}
	}
	belle_sip_list_free_with_data(listeners, belle_sip_object_unref);
	belle_sip_list_free_with_data(obj->incoming_messages, belle_sip_object_unref);
	obj->incoming_messages = NULL;
}

void belle_sip_channel_parse_stream(belle_sip_channel_t *obj, int end_of_stream) {
	int offset;
	size_t read_size = 0;
	int num;

	while ((num = (int)belle_sip_channel_input_stream_get_readable_length(&obj->input_stream)) > 0) {

		if (obj->input_stream.state == WAITING_MESSAGE_START) {
			int i;
			/*first, make sure there is \r\n in the buffer, otherwise, micro parser cannot conclude, because we need a
			 * complete request or response line somewhere*/
			for (i = 0; i < num - 1; i++) {
				if ((obj->input_stream.read_ptr[i] == '\r' && obj->input_stream.read_ptr[i + 1] == '\n') ||
				    belle_sip_channel_input_stream_get_free_length(&obj->input_stream) <=
				        1 /*1 because null terminated*/ /*if buffer full try to parse in any case*/) {
					/*good, now we can start searching  for request/response*/
					if ((offset = get_message_start_pos(obj->input_stream.read_ptr, num)) >= 0) {
						/*message found !*/
						if (offset > 0) {
							belle_sip_warning("trashing [%i] bytes in front of sip message on channel [%p]", offset,
							                  obj);
							obj->input_stream.read_ptr += offset;
						}
						obj->input_stream.state = MESSAGE_AQUISITION;
					} else {
						belle_sip_debug("Unexpected [%s] received on channel [%p], trashing",
						                obj->input_stream.read_ptr, obj);
						obj->input_stream.read_ptr = obj->input_stream.write_ptr;
						belle_sip_channel_input_stream_reset(&obj->input_stream);
						obj->inhibit_input_logging_buffer = 0;
						continue;
					}
					break;
				}
			}

			if (i >= num - 1) {
				belle_sip_debug(
				    "[%s] received on channel [%p], cannot determine if expected or not, waiting for new data",
				    obj->input_stream.read_ptr, obj);
				break;
			}
		}

		if (obj->input_stream.state == MESSAGE_AQUISITION) {
			/*search for \r\n\r\n*/
			char *end_of_message = NULL;
			if ((end_of_message = strstr(obj->input_stream.read_ptr, "\r\n\r\n"))) {
				int bytes_to_parse;
				char tmp;
				/*end of message found*/
				end_of_message += 4; /*add \r\n\r\n*/
				bytes_to_parse = (int)(end_of_message - obj->input_stream.read_ptr);
				tmp = *end_of_message;
				*end_of_message = '\0'; /*this is in order for the following log to print the message only to its end.*/
				/*belle_sip_message("channel [%p] read message of [%i] bytes:\n%.40s...",obj, bytes_to_parse,
				 * obj->input_stream.read_ptr);*/
				obj->input_stream.msg =
				    belle_sip_message_parse_raw(obj->input_stream.read_ptr, bytes_to_parse, &read_size);
				*end_of_message = tmp;
				obj->input_stream.read_ptr += read_size;
				if (obj->input_stream.msg && read_size > 0) {
					belle_sip_message("channel [%p] [%i] bytes parsed", obj, (int)read_size);
					belle_sip_object_ref(obj->input_stream.msg);
					if (belle_sip_message_is_request(obj->input_stream.msg))
						fix_incoming_via(BELLE_SIP_REQUEST(obj->input_stream.msg), obj->current_peer);
					/*check for body*/

					if (check_body(obj)) {
						obj->input_stream.state = BODY_AQUISITION;
					} else {
						/*no body*/
						belle_sip_channel_message_ready(obj);
						continue;
					}
				} else {
					belle_sip_error("Could not parse [%s], on channel [%p] skipping to [%s]",
					                obj->input_stream.read_ptr, obj, end_of_message);
					if (obj->input_stream.msg) {
						/* despite read_size is zero, belle_sip_message_parse_raw() might return an empty
						 * belle_sip_message_t */
						belle_sip_object_unref(obj->input_stream.msg);
						obj->input_stream.msg = NULL;
					}
					obj->input_stream.read_ptr = end_of_message;
					obj->input_stream.state = WAITING_MESSAGE_START;
					obj->inhibit_input_logging_buffer = 0;
					continue;
				}
			} else break; /*The message isn't finished to be receive, we need more data*/
		}

		if (obj->input_stream.state == BODY_AQUISITION) {
			if (acquire_body(obj, end_of_stream) == BELLE_SIP_STOP) break;
		}
	}
}

static void belle_sip_channel_process_stream(belle_sip_channel_t *obj, int eos) {
	belle_sip_channel_parse_stream(obj, eos);
	if (obj->incoming_messages) {
		if (obj->simulated_recv_return == 1500) {
			belle_sip_list_t *elem;
			for (elem = obj->incoming_messages; elem != NULL; elem = elem->next) {
				belle_sip_message_t *msg = (belle_sip_message_t *)elem->data;
				char *dump = belle_sip_message_to_string(msg);
				belle_sip_message("Silently discarding incoming message [%.50s...] on channel [%p]", dump, obj);
				belle_sip_free(dump);
			}
			belle_sip_list_free_with_data(obj->incoming_messages, belle_sip_object_unref);
			obj->incoming_messages = NULL;
		} else {
			notify_incoming_messages(obj);
		}
	}
}

static int belle_sip_channel_process_read_data(belle_sip_channel_t *obj) {
	int read_bytes;

	/*prevent system to suspend the process until we have finish reading everything from the socket and notified the
	 * upper layer*/
	if (obj->input_stream.state == WAITING_MESSAGE_START) {
		channel_begin_recv_background_task(obj);
	}

	if (obj->simulated_recv_return > 0) {
		read_bytes = belle_sip_channel_recv(obj, obj->input_stream.write_ptr,
		                                    belle_sip_channel_input_stream_get_free_length(&obj->input_stream) - 1);
	} else {
		belle_sip_message("channel [%p]: simulating recv() returning %i", obj, obj->simulated_recv_return);
		read_bytes = obj->simulated_recv_return;
	}
	if (read_bytes > 0) {
		char *begin = obj->input_stream.write_ptr;
		obj->input_stream.write_ptr += read_bytes;
		/*first null terminate the read buff*/
		*obj->input_stream.write_ptr = '\0';
		if (read_bytes > 20 ||
		    obj->input_stream.state != WAITING_MESSAGE_START) /*to avoid tracing server based keep alives*/ {
			char *logbuf = make_logbuf(obj, BELLE_SIP_LOG_MESSAGE, begin, read_bytes, BELLE_SIP_DIRECTION_RECV);
			if (logbuf) {
				belle_sip_message("channel [%p]: received [%i] new bytes from [%s://%s:%i]:\n%s", obj, read_bytes,
				                  belle_sip_channel_get_transport_name(obj), obj->peer_name, obj->peer_port, logbuf);
				belle_sip_free(logbuf);
			}
		}
		belle_sip_channel_process_stream(obj, FALSE);
		if (obj->input_stream.state == WAITING_MESSAGE_START &&
		    belle_sip_channel_input_stream_get_readable_length(&obj->input_stream) == 0) {
			/* If no data is pending in the input stream and waiting for message start, release the background task. */
			channel_end_recv_background_task(obj);
		} /*if still in message acquisition state, keep the backgroud task*/
	} else if (read_bytes == 0) {
		/*before closing the channel, check if there was a pending message to receive, whose body acquisition is to be
		 * finished.*/
		belle_sip_channel_process_stream(obj, TRUE);
		obj->closed_by_remote = TRUE;
		channel_set_state(obj, BELLE_SIP_CHANNEL_DISCONNECTED);

	} else if (belle_sip_error_code_is_would_block(-read_bytes)) {
		// belle_sip_message("channel [%p]: recv() EWOULDBLOCK", obj);
	} else {
		belle_sip_error("Receive error on channel [%p]", obj);
		channel_set_state(obj, BELLE_SIP_CHANNEL_ERROR);
	}
	return read_bytes;
}

int belle_sip_channel_process_data(belle_sip_channel_t *obj, unsigned int revents) {
	int ret = BELLE_SIP_CONTINUE;
	belle_sip_object_ref(obj);
	if (revents & BELLE_SIP_EVENT_READ) {
		int read_bytes;
		int total_read_bytes = 0;
		do {
			read_bytes = belle_sip_channel_process_read_data(obj);
			/* While in body acquisition, attempt to read as much bytes as possible, until EWOULDBLOCK happens. */
			if (read_bytes > 0) total_read_bytes += read_bytes;
		} while (obj->state == BELLE_SIP_CHANNEL_READY && obj->input_stream.state == BODY_AQUISITION &&
		         read_bytes > 0 && total_read_bytes < belle_sip_max_network_data_size_per_iterate);
		if ((read_bytes < 0 && !belle_sip_error_code_is_would_block(-read_bytes)) || read_bytes == 0) {
			/* these are the error conditions (except EWOULDBLOCK) or normal disconnection. */
			ret = BELLE_SIP_STOP;
		}
	}
	if (revents & BELLE_SIP_EVENT_WRITE) {
		/*if we are here, this is because we had an EWOULDBLOCK while sending a message*/
		/*continue to send pending messages but before check the channel is still alive because
		it may have been closed by belle_sip_channel_process_read_data() above.*/
		if (obj->state == BELLE_SIP_CHANNEL_READY) {
			channel_process_queue(obj);
		}
	}
	belle_sip_object_unref(obj);
	return ret;
}

static int channel_inactive_timeout(void *data, unsigned int event) {
	belle_sip_channel_t *obj = (belle_sip_channel_t *)data;
	belle_sip_message("Channel [%p]: inactivity timeout reached.", obj);
	channel_set_state(obj, BELLE_SIP_CHANNEL_DISCONNECTED);
	return BELLE_SIP_STOP;
}

static void update_inactivity_timer(belle_sip_channel_t *obj, int from_recv) {
	int inactive_timeout;
	if (obj->lp) {
		/* We assume that if there is the listening_point back pointer, it is a SIP channel, otherwise it is http.*/
		inactive_timeout = belle_sip_stack_get_inactive_transport_timeout(obj->stack) * 1000;
	} else {
		inactive_timeout = belle_sip_stack_get_http_inactive_transport_timeout(obj->stack) * 1000;
	}
	if (inactive_timeout > 0) {
		if (!obj->inactivity_timer) {
			obj->inactivity_timer = belle_sip_main_loop_create_timeout(obj->stack->ml, channel_inactive_timeout, obj,
			                                                           inactive_timeout, "Channel inactivity timer");
		} else {
			/*restart the timer for new period*/
			belle_sip_source_set_timeout_int64(obj->inactivity_timer, inactive_timeout);
		}
	} else {
		if (obj->inactivity_timer) {
			belle_sip_main_loop_remove_source(obj->stack->ml, obj->inactivity_timer);
			belle_sip_object_unref(obj->inactivity_timer);
			obj->inactivity_timer = NULL;
		}
	}
	if (from_recv) obj->last_recv_time = belle_sip_time_ms();
}

/*constructor for channels creating an outgoing connection
 * bindip local ip address to bind on, typically 0.0.0.0 or ::0
 * locaport locaport to use for binding, can be set to 0 if port doesn't matter
 * peer_cname canonical name of remote host, used for TLS verification
 * peername peer's hostname, either ip address or DNS name
 * peer_port peer's port to connect to.
 */
void belle_sip_channel_init(belle_sip_channel_t *obj,
                            belle_sip_stack_t *stack,
                            const char *bindip,
                            int localport,
                            const char *peer_cname,
                            const char *peername,
                            int peer_port,
                            int no_srv) {
	/*to initialize our base class:*/
	belle_sip_channel_set_socket(obj, -1, NULL);
	belle_sip_message("belle_sip_channel_init(); peer_cname=%s", peer_cname);
	/*then initialize members*/
	obj->ai_family = AF_INET;
	obj->peer_cname = peer_cname ? belle_sip_strdup(peer_cname) : NULL;
	obj->peer_name = belle_sip_strdup(peername);
	obj->peer_port = peer_port;
	obj->no_srv = (unsigned char)no_srv;
	obj->stack = stack;
	if (bindip) {
		if (strcmp(bindip, "::0") != 0 && strcmp(bindip, "0.0.0.0") != 0) obj->local_ip = belle_sip_strdup(bindip);
		if (strchr(bindip, ':') != NULL) obj->ai_family = AF_INET6;
	}
	obj->local_port = localport;
	obj->simulated_recv_return = 1; /*not set*/
	if (peername) {
		/*check if we are given a real dns name or just an ip address*/
		struct addrinfo *ai = bctbx_ip_address_to_addrinfo(AF_UNSPEC, SOCK_STREAM, peername, peer_port);
		if (ai) bctbx_freeaddrinfo(ai);
		else obj->has_name = TRUE;
	}
	belle_sip_channel_input_stream_reset(&obj->input_stream);
	update_inactivity_timer(obj, FALSE);
}

/*constructor for channels created by incoming connections*/
void belle_sip_channel_init_with_addr(belle_sip_channel_t *obj,
                                      belle_sip_stack_t *stack,
                                      const char *bindip,
                                      int localport,
                                      const struct sockaddr *peer_addr,
                                      socklen_t addrlen) {
	char remoteip[64];
	struct addrinfo ai;
	int peer_port;

	memset(&ai, 0, sizeof(ai));
	ai.ai_family = peer_addr->sa_family;
	ai.ai_addr = (struct sockaddr *)peer_addr;
	ai.ai_addrlen = addrlen;
	bctbx_addrinfo_to_ip_address(&ai, remoteip, sizeof(remoteip), &peer_port);
	belle_sip_channel_init(obj, stack, bindip, localport, NULL, remoteip, peer_port, TRUE);
	obj->peer_list = obj->current_peer = obj->static_peer_list =
	    bctbx_ip_address_to_addrinfo(ai.ai_family, ai.ai_socktype, obj->peer_name, obj->peer_port);
	obj->ai_family = ai.ai_family;
}

void belle_sip_channel_set_socket(belle_sip_channel_t *obj, belle_sip_socket_t sock, belle_sip_source_func_t datafunc) {
	belle_sip_socket_source_init((belle_sip_source_t *)obj, datafunc, obj, sock,
	                             BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_WRITE, -1);
}

static bool_t is_state_only_listener(const belle_sip_channel_listener_t *listener) {
	BELLE_SIP_INTERFACE_METHODS_TYPE(belle_sip_channel_listener_t) * methods;
	methods = BELLE_SIP_INTERFACE_GET_METHODS(listener, belle_sip_channel_listener_t);
	return methods->on_state_changed &&
	       !(methods->on_message_headers || methods->on_message || methods->on_sending || methods->on_auth_requested);
}
static void channel_remove_listener(belle_sip_channel_t *obj, belle_sip_channel_listener_t *l) {
	if (is_state_only_listener(l)) obj->state_listeners = belle_sip_list_remove(obj->state_listeners, l);
	else obj->full_listeners = belle_sip_list_remove(obj->full_listeners, l);
}

void belle_sip_channel_add_listener(belle_sip_channel_t *obj, belle_sip_channel_listener_t *l) {

	if (is_state_only_listener(l)) {
		obj->state_listeners = belle_sip_list_prepend(
		    obj->state_listeners,
		    belle_sip_object_weak_ref(l, (belle_sip_object_destroy_notify_t)channel_remove_listener, obj));
	} else {
		obj->full_listeners = belle_sip_list_prepend(
		    obj->full_listeners,
		    belle_sip_object_weak_ref(l, (belle_sip_object_destroy_notify_t)channel_remove_listener, obj));
	}
}

void belle_sip_channel_remove_listener(belle_sip_channel_t *obj, belle_sip_channel_listener_t *l) {
	belle_sip_object_weak_unref(l, (belle_sip_object_destroy_notify_t)channel_remove_listener, obj);
	channel_remove_listener(obj, l);
}

int belle_sip_channel_matches(const belle_sip_channel_t *obj, const belle_sip_hop_t *hop, const struct addrinfo *addr) {
	if (hop) {
		if (obj->current_peer_cname && strcasecmp(hop->host, obj->current_peer_cname) == 0 &&
		    hop->port == obj->peer_port) {
			/*We are matching a specific node of a SRV record set. */
			if (hop->cname && obj->peer_cname && strcasecmp(hop->cname, obj->current_peer_cname) != 0)
				return 0; /*cname mismatch*/
			return 1;
		}
		if (strcmp(hop->host, obj->peer_name) == 0 && (hop->port == obj->peer_port || obj->srv_overrides_port)) {
			/*We may be matching the general name of the service, in that case the port doesn't matter.*/
			if (hop->cname && obj->peer_cname && strcasecmp(hop->cname, obj->peer_cname) != 0) {
				return 0; /*cname mismatch*/
			}
			return 1;
		}
	}
	if (addr && obj->current_peer) return bctbx_sockaddr_equals(addr->ai_addr, obj->current_peer->ai_addr);
	return 0;
}

const char *belle_sip_channel_get_local_address(belle_sip_channel_t *obj, int *port) {
	if (port) *port = obj->local_port;
	return obj->local_ip;
}

const char *belle_sip_channel_get_public_address(belle_sip_channel_t *obj, int *port) {
	const char *ret = obj->public_ip ? obj->public_ip : obj->local_ip;
	if (*port) *port = obj->public_port;
	return ret;
}

belle_sip_uri_t *belle_sip_channel_create_routable_uri(belle_sip_channel_t *chan) {
	const char *transport = belle_sip_channel_get_transport_name_lower_case(chan);
	belle_sip_uri_t *uri = belle_sip_uri_new();
	unsigned char natted = chan->public_ip && strcmp(chan->public_ip, chan->local_ip) != 0;

	if (natted) {
		belle_sip_uri_set_host(uri, chan->public_ip);
		belle_sip_uri_set_port(uri, chan->public_port);
	} else {

		belle_sip_uri_set_host(uri, chan->local_ip);
		// With streamed protocols listening port is what we want
		if (chan->lp) belle_sip_uri_set_port(uri, belle_sip_uri_get_port(chan->lp->listening_uri));
		else belle_sip_uri_set_port(uri, chan->local_port);
	}

	belle_sip_uri_set_transport_param(uri, transport);
	belle_sip_uri_set_lr_param(uri, TRUE);
	return uri;
}

int belle_sip_channel_is_reliable(const belle_sip_channel_t *obj) {
	return BELLE_SIP_OBJECT_VPTR(obj, belle_sip_channel_t)->reliable;
}

const char *belle_sip_channel_get_transport_name_lower_case(const belle_sip_channel_t *obj) {
	const char *transport = belle_sip_channel_get_transport_name(obj);
	if (strcasecmp("udp", transport) == 0) return "udp";
	else if (strcasecmp("tcp", transport) == 0) return "tcp";
	else if (strcasecmp("tls", transport) == 0) return "tls";
	else if (strcasecmp("dtls", transport) == 0) return "dtls";
	else {
		belle_sip_message("Cannot convert [%s] to lower case", transport);
		return transport;
	}
}

const char *belle_sip_channel_get_transport_name(const belle_sip_channel_t *obj) {
	return BELLE_SIP_OBJECT_VPTR(obj, belle_sip_channel_t)->transport;
}

int belle_sip_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen) {
	update_inactivity_timer(obj, FALSE);
	return BELLE_SIP_OBJECT_VPTR(obj, belle_sip_channel_t)->channel_send(obj, buf, buflen);
}

static bool_t is_pong(const char *buf, size_t buflen) {
	return buflen == 2 && strncmp(buf, "\r\n", 2) == 0;
}

int belle_sip_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen) {
	int ret;

	ret = BELLE_SIP_OBJECT_VPTR(obj, belle_sip_channel_t)->channel_recv(obj, buf, buflen);
	update_inactivity_timer(obj, TRUE);
	if (obj->expect_pong_timer && !obj->pong_support_confirmed && !obj->expect_pong_timer->expired &&
	    is_pong((const char *)buf, ret)) {
		belle_sip_message("channel[%p]: RFC5626 pong support is confirmed.", obj);
		obj->pong_support_confirmed = TRUE;
	}
	/* in any case since we received something, clear the expect_pong_timer */
	BELLE_SIP_CHANNEL_CLEAR_TIMER(obj, expect_pong_timer);
	return ret;
}

void belle_sip_channel_close(belle_sip_channel_t *obj) {
	if (BELLE_SIP_OBJECT_VPTR(obj, belle_sip_channel_t)->close)
		BELLE_SIP_OBJECT_VPTR(obj, belle_sip_channel_t)->close(obj); /*udp channel doesn't have close function*/
	belle_sip_main_loop_remove_source(obj->stack->ml, (belle_sip_source_t *)obj);
}

const struct addrinfo *belle_sip_channel_get_peer(belle_sip_channel_t *obj) {
	return obj->current_peer;
}

static void channel_on_send_background_task_ended(belle_sip_channel_t *obj) {
	belle_sip_warning("channel [%p]: send background task has to be ended now, but work isn't finished.", obj);
	channel_end_send_background_task(obj);
}

static void channel_begin_send_background_task(belle_sip_channel_t *obj) {
	if (obj->bg_task_id == 0) {
		obj->bg_task_id = belle_sip_begin_background_task("belle-sip send channel",
		                                                  (void (*)(void *))channel_on_send_background_task_ended, obj);
		if (obj->bg_task_id)
			belle_sip_message("channel [%p]: starting send background task with id=[%lx].", obj, obj->bg_task_id);
	}
}

static void channel_end_send_background_task(belle_sip_channel_t *obj) {
	if (obj->bg_task_id) {
		belle_sip_message("channel [%p]: ending send background task with id=[%lx].", obj, obj->bg_task_id);
		belle_sip_end_background_task(obj->bg_task_id);
		obj->bg_task_id = 0;
	}
}

static void channel_on_recv_background_task_ended(belle_sip_channel_t *obj) {
	belle_sip_warning("channel [%p]: recv background task has to be ended now, but work isn't finished.", obj);
	channel_end_recv_background_task(obj);
}

static void channel_begin_recv_background_task(belle_sip_channel_t *obj) {
	if (obj->recv_bg_task_id == 0) {
		obj->recv_bg_task_id = belle_sip_begin_background_task(
		    "belle-sip recv channel", (void (*)(void *))channel_on_recv_background_task_ended, obj);
		if (obj->recv_bg_task_id)
			belle_sip_message("channel [%p]: starting recv background task with id=[%lx].", obj, obj->recv_bg_task_id);
	}
}

static void channel_end_recv_background_task(belle_sip_channel_t *obj) {
	if (obj->recv_bg_task_id) {
		belle_sip_message("channel [%p]: ending recv background task with id=[%lx].", obj, obj->recv_bg_task_id);
		belle_sip_end_background_task(obj->recv_bg_task_id);
		obj->recv_bg_task_id = 0;
	}
}

static void channel_invoke_state_listener(belle_sip_channel_t *obj) {
	int close = FALSE;
	switch (obj->state) {
		case BELLE_SIP_CHANNEL_DISCONNECTED:
		case BELLE_SIP_CHANNEL_ERROR:
			/*the background tasks must be released "after" notifying the app of the disconnected or error state
			 By "after" it is means not before the main loop iteration that will notify the app.
			 This is the reason why these calls are done here rather than in the channel_set_state() function.*/
			channel_end_send_background_task(obj);
			channel_end_recv_background_task(obj);
			close = TRUE;
			break;
		default:
			break;
	}
	/*Channel listeners may drop the last reference of the channel, so protect by ref/unref until we finish.*/
	belle_sip_object_ref(obj);
	BELLE_SIP_CHANNEL_INVOKE_STATE_LISTENERS(obj, obj->state);
	if (close) belle_sip_channel_close(obj);
	belle_sip_object_unref(obj);
}

static void channel_notify_error_to_listeners(belle_sip_channel_t *obj) {
	/* The channel may have been passed to DISCONNECTED state due to _force_close() method.
	 * Do not notify the error in this case, since the channel is already closed.
	 */
	if (obj->state == BELLE_SIP_CHANNEL_ERROR) {
		channel_invoke_state_listener(obj);
	}
	belle_sip_object_unref(obj);
}

static void channel_connect_next(belle_sip_channel_t *obj) {
	if (obj->state == BELLE_SIP_CHANNEL_RETRY) {
		belle_sip_channel_connect(obj);
	}
	belle_sip_object_unref(obj);
}

static void channel_set_current_peer(belle_sip_channel_t *obj, const struct addrinfo *ai) {
	if (obj->resolver_results) {
		const belle_sip_dns_srv_t *srv = belle_sip_resolver_results_get_srv_from_addrinfo(obj->resolver_results, ai);
		obj->current_peer_cname = srv ? belle_sip_dns_srv_get_target(srv) : NULL;
		if (obj->current_peer_cname) {
			belle_sip_message("channel[%p]: current peer hostname is [%s] on port [%i].", obj, obj->current_peer_cname,
			                  belle_sip_dns_srv_get_port(srv));
		}
	} else {
		obj->current_peer_cname = NULL;
	}
	obj->current_peer = ai;
}

static void belle_sip_channel_handle_error(belle_sip_channel_t *obj) {
	if (obj->state != BELLE_SIP_CHANNEL_READY || obj->soft_error) {
		/* Previous connection attempts were failed (channel could not get ready) OR soft error reported*/
		obj->soft_error = FALSE;
		/* See if you can retry on an alternate ip address.*/
		if (obj->current_peer && obj->current_peer->ai_next) { /*obj->current_peer may be null in case of dns error*/
			channel_set_current_peer(obj, obj->current_peer->ai_next);
			channel_set_state(obj, BELLE_SIP_CHANNEL_RETRY);
			belle_sip_channel_close(obj);
			belle_sip_main_loop_do_later(obj->stack->ml, (belle_sip_callback_t)channel_connect_next,
			                             belle_sip_object_ref(obj));
			return;
		} /*else we have already tried all the ip addresses, so give up and notify the error*/
	}     /*else the channel was previously working good with the current ip address but now fails, so let's notify the
	         error*/

	obj->state = BELLE_SIP_CHANNEL_ERROR;
	/*Because error notification will in practice trigger the destruction of possible transactions and this channel,
	 * it is safer to invoke the listener outside the current call stack.
	 * Indeed the channel encounters network errors while being called for transmiting by a transaction.
	 */
	belle_sip_main_loop_do_later(obj->stack->ml, (belle_sip_callback_t)channel_notify_error_to_listeners,
	                             belle_sip_object_ref(obj));
}

int belle_sip_channel_notify_timeout(belle_sip_channel_t *obj) {
	const uint64_t too_long = 60;

	if (obj->state != BELLE_SIP_CHANNEL_READY) {
		/*no need to notify the timeout if the channel is already in error or retry state*/
		return FALSE;
	}

	if ((belle_sip_time_ms() - obj->last_recv_time) >= too_long * 1000) {
		belle_sip_message("A timeout related to this channel occured and no message received during last %i seconds. "
		                  "This channel is suspect, moving to error state",
		                  (int)too_long);
		obj->soft_error = TRUE;
		channel_set_state(obj, BELLE_SIP_CHANNEL_ERROR);
		return TRUE;
	}
	return FALSE;
}

void belle_sip_channel_notify_server_error(belle_sip_channel_t *obj) {
	belle_sip_message("channel[%p]: this server is encountering internal errors, moving to error state to eventually "
	                  "connect to another IP.",
	                  obj);
	obj->soft_error = TRUE;
	channel_set_state(obj, BELLE_SIP_CHANNEL_ERROR);
}

void channel_set_state(belle_sip_channel_t *obj, belle_sip_channel_state_t state) {
	belle_sip_message("channel[%p]: entering state %s", obj, belle_sip_channel_state_to_string(state));

	if (obj->state == state) {
		belle_sip_error("channel_set_state() called twice with the same state. This is a programming mistake.");
		return;
	}
	if (state == BELLE_SIP_CHANNEL_ERROR) {
		belle_sip_channel_handle_error(obj);
	} else {
		obj->state = state;
		channel_invoke_state_listener(obj);
	}
}

static void free_ewouldblock_buffer(belle_sip_channel_t *obj) {
	if (obj->ewouldblock_buffer) {
		belle_sip_free(obj->ewouldblock_buffer);
		obj->ewouldblock_buffer = NULL;
		obj->ewouldblock_size = 0;
		obj->ewouldblock_offset = 0;
	}
}

static void handle_ewouldblock(belle_sip_channel_t *obj, const char *buffer, size_t size) {
	belle_sip_source_set_events((belle_sip_source_t *)obj,
	                            BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_WRITE | BELLE_SIP_EVENT_ERROR);
	free_ewouldblock_buffer(obj);
	obj->ewouldblock_buffer = belle_sip_malloc(size);
	obj->ewouldblock_size = size;
	memcpy(obj->ewouldblock_buffer, buffer, size);
}

static size_t find_non_printable(const char *buffer, size_t size) {
#if 0
	size_t i;
	for(i=0;i<size;++i){
		/*we must check that character is printable, not just ascii
		(31 'US' char is not printable for instance)*/
#ifdef _MSC_VER
		if ((buffer[i] < 0) || (buffer[i] > 255)) return i;
#endif
		if (!isprint(buffer[i]) && !isspace(buffer[i])) return i;
	}
	return size;
#else
	size_t i = 0;
	mbstate_t mbs;
	memset(&mbs, 0, sizeof(mbs));
	do {
		size_t valid_multibyte_len = mbrlen(buffer + i, size - i, &mbs);
		if (valid_multibyte_len == (size_t)-1 || valid_multibyte_len == (size_t)-2 || valid_multibyte_len == 0) break;
		i += valid_multibyte_len;
	} while (1);
	return i;
#endif
}
/*
 * this function is to avoid logging too much or non-ascii data received.
 */
static char *make_logbuf(belle_sip_channel_t *obj,
                         belle_sip_log_level level,
                         const char *buffer,
                         size_t size,
                         belle_sip_direction_t direction) {
	char *logbuf;
	char truncate_msg[128] = {0};
	size_t limit = 14000; /*typically a big INVITE with many streams and ice candidates*/

	if (!belle_sip_log_level_enabled(level)) {
		return NULL;
	}
	if ((direction == BELLE_SIP_DIRECTION_RECV && obj->inhibit_input_logging_buffer == 1) ||
	    (direction == BELLE_SIP_DIRECTION_SEND && obj->inhibit_output_logging_buffer == 1)) {
		return NULL;
	}
	size = MIN(size, limit);
	limit = find_non_printable(buffer, size);
	if (limit < size) {
		belle_sip_message("channel [%p]: found binary data in buffer, will stop logging it now.", obj);
		if (direction == BELLE_SIP_DIRECTION_RECV) obj->inhibit_input_logging_buffer = 1;
		else obj->inhibit_output_logging_buffer = 1;
		if (limit == 0) {
			snprintf(truncate_msg, sizeof(truncate_msg) - 1, "... (binary data)");
		} else {
			snprintf(truncate_msg, sizeof(truncate_msg) - 1, "... (first %u bytes shown)", (unsigned int)limit);
		}
	}
	size = limit;

	size += strlen(truncate_msg);

	logbuf = belle_sip_malloc(size + 1);
	strncpy(logbuf, buffer, size);
	if (truncate_msg[0] != 0) {
		strcpy(logbuf + limit, truncate_msg);
	}
	logbuf[size] = '\0';
	return logbuf;
}

static int send_buffer(belle_sip_channel_t *obj, const char *buffer, size_t size) {
	int ret = 0;
	char *logbuf = NULL;

	if (obj->stack->send_error == 0) {
		ret = belle_sip_channel_send(obj, buffer, size);
	} else if (obj->stack->send_error < 0) {
		/*for testing purpose only */
		belle_sip_message("channel[%p]: simulating socket error [%i].", obj, (int)obj->stack->send_error);
		ret = obj->stack->send_error;
	} else {
		ret = (int)size; /*to silently discard message*/
		belle_sip_message("channel[%p]: %i bytes are silently discarded, to simulate loss of data.", obj, (int)size);
	}

	if (ret < 0) {
		if (!belle_sip_error_code_is_would_block(-ret)) {
			belle_sip_error("channel [%p]: could not send [%i] bytes from [%s://%s:%i] to [%s:%i]", obj, (int)size,
			                belle_sip_channel_get_transport_name(obj), obj->local_ip, obj->local_port, obj->peer_name,
			                obj->peer_port);
			channel_set_state(obj, BELLE_SIP_CHANNEL_ERROR);
		} /*ewouldblock error has to be handled by caller*/
	} else if (size == (size_t)ret) {
		logbuf = make_logbuf(obj, BELLE_SIP_LOG_MESSAGE, buffer, size, BELLE_SIP_DIRECTION_SEND);
		if (logbuf) {
			belle_sip_message("channel [%p]: message %s to [%s://%s:%i], size: [%i] bytes\n%s", obj,
			                  obj->stack->send_error == 0 ? "sent" : "silently discarded",
			                  belle_sip_channel_get_transport_name(obj), obj->peer_name, obj->peer_port, ret, logbuf);
		}
	} else {
		logbuf = make_logbuf(obj, BELLE_SIP_LOG_MESSAGE, buffer, ret, BELLE_SIP_DIRECTION_SEND);
		if (logbuf) {
			belle_sip_message("channel [%p]: message partly sent to [%s://%s:%i], sent: [%i/%i] bytes:\n%s", obj,
			                  belle_sip_channel_get_transport_name(obj), obj->peer_name, obj->peer_port, ret, (int)size,
			                  logbuf);
		}
	}
	if (logbuf) belle_sip_free(logbuf);
	return ret;
}

static void check_content_length(belle_sip_message_t *msg, size_t body_len) {
	belle_sip_header_content_length_t *ctlen =
	    belle_sip_message_get_header_by_type(msg, belle_sip_header_content_length_t);
	size_t value = ctlen ? belle_sip_header_content_length_get_content_length(ctlen) : 0;
	if (body_len) {
		if (ctlen == NULL) {
			belle_sip_message("message [%p] has body of size [" FORMAT_SIZE_T "] but no Content-Length, adding it.",
			                  msg, body_len);
			belle_sip_message_add_header(msg, (belle_sip_header_t *)belle_sip_header_content_length_create(body_len));
		} else {
			if (value != body_len) {
				belle_sip_warning("message [%p] has Content-Length [" FORMAT_SIZE_T "] and body size [" FORMAT_SIZE_T
				                  "] which are inconsistent, fixing it.",
				                  msg, value, body_len);
				belle_sip_header_content_length_set_content_length(ctlen, body_len);
			}
		}
	} else {
		/*no body, or undetermined size body*/
		if (ctlen && value != 0) {
			belle_sip_error("message [%p] has Content-Length [" FORMAT_SIZE_T
			                "], but without body or body with undetermined size. Fix your app.",
			                msg, value);
		}
	}
}

static void compress_body_if_required(belle_sip_message_t *msg) {
	belle_sip_body_handler_t *bh = belle_sip_message_get_body_handler(msg);
	belle_sip_memory_body_handler_t *mbh = NULL;
	belle_sip_header_t *ceh = NULL;
	size_t body_len = 0;

	if (bh != NULL) {
		body_len = belle_sip_message_get_body_size(msg);
		ceh = belle_sip_message_get_header(msg, "Content-Encoding");
	}
	if ((body_len > 0) && (ceh != NULL)) {
		const char *content_encoding = belle_sip_header_get_unparsed_value(ceh);
		if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(bh, belle_sip_multipart_body_handler_t)) {
			char *marshalled_content = belle_sip_object_to_string(BELLE_SIP_OBJECT(bh));
			mbh = belle_sip_memory_body_handler_new_from_buffer(marshalled_content, strlen(marshalled_content), NULL,
			                                                    NULL);
			bh = BELLE_SIP_BODY_HANDLER(mbh);
			belle_sip_message_set_body_handler(msg, bh);
		}
		if (BELLE_SIP_OBJECT_IS_INSTANCE_OF(bh, belle_sip_memory_body_handler_t)) {
			mbh = BELLE_SIP_MEMORY_BODY_HANDLER(bh);
			int ret = belle_sip_memory_body_handler_apply_encoding(mbh, content_encoding);
			/* Remove Content-Encoding header if it could not be applied */
			if (ret < 0) belle_sip_message_remove_header_from_ptr(msg, ceh);
		} else {
			belle_sip_warning("message [%p] has Content-Encoding [%s] that cannot be applied", msg, content_encoding);
		}
	}
}

static void _send_message(belle_sip_channel_t *obj) {
	char buffer[belle_sip_send_network_buffer_size];
	size_t len = 0;
	belle_sip_error_code error = BELLE_SIP_OK;
	belle_sip_message_t *msg = obj->cur_out_message;
	belle_sip_body_handler_t *bh = belle_sip_message_get_body_handler(msg);
	size_t body_len = bh ? belle_sip_body_handler_get_size(bh) : 0;
	int sendret;
	size_t off;
	int ret;

	while (obj->ewouldblock_buffer) {
		sendret = send_buffer(obj, (const char *)obj->ewouldblock_buffer + obj->ewouldblock_offset,
		                      obj->ewouldblock_size - obj->ewouldblock_offset);
		if (sendret > 0) {
			obj->ewouldblock_offset += sendret;
			if (obj->ewouldblock_offset == obj->ewouldblock_size) {
				free_ewouldblock_buffer(obj);
				if (obj->out_state == OUTPUT_STREAM_SENDING_HEADERS) goto done; // to avoid message to be sent twice
				                                                                /*else continue body sending*/
			}
			/* continue to expedite the ewouldblock error until we it is completed or get a new ewouldblock*/
		} else if (belle_sip_error_code_is_would_block(-sendret)) {
			/*we got an ewouldblock again. Nothing to do, we'll be called later in order to retry*/
			return;
		} else { /*error or disconnection case*/
			goto done;
		}
	}

	if (obj->out_state == OUTPUT_STREAM_SENDING_HEADERS) {
		BELLE_SIP_CHANNEL_INVOKE_SENDING_LISTENERS(obj, msg);
		check_content_length(msg, body_len);
		error = belle_sip_object_marshal((belle_sip_object_t *)msg, buffer, sizeof(buffer) - 1, &len);
		if (error != BELLE_SIP_OK) {
			belle_sip_error("channel [%p] _send_message: marshaling failed.", obj);
			goto done;
		}
		/*send the headers and eventually the body if it fits in our buffer*/
		if (bh) {
			size_t max_body_len = sizeof(buffer) - 1 - len;

			if (body_len > 0 &&
			    body_len <= max_body_len) { /*if size is known and fits into our buffer, send together with headers*/
				belle_sip_body_handler_begin_send_transfer(bh);
				do {
					max_body_len = sizeof(buffer) - 1 - len;
					ret = belle_sip_body_handler_send_chunk(bh, msg, (uint8_t *)buffer + len, &max_body_len);
					if (max_body_len == 0)
						belle_sip_warning("belle_sip_body_handler_send_chunk on channel [%p], 0 bytes read", obj);
					len += max_body_len;
				} while (ret == BELLE_SIP_CONTINUE);
				belle_sip_body_handler_end_transfer(bh);
			} else {
				if (body_len == 0) {
					belle_sip_fatal("Sending bodies whose size is not known must be done in chunked mode, which is not "
					                "supported yet.");
				}
				belle_sip_body_handler_begin_send_transfer(bh);
				obj->out_state = OUTPUT_STREAM_SENDING_BODY;
			}
		}
		off = 0;
		do {
			sendret = send_buffer(obj, buffer + off, len - off);
			if (sendret > 0) {
				off += sendret;
				if (off == len) {
					break;
				}
			} else if (belle_sip_error_code_is_would_block(-sendret)) {
				handle_ewouldblock(obj, buffer + off, len - off);
				return;
			} else { /*error or disconnection case*/
				goto done;
			}
		} while (1);
	}
	if (obj->out_state == OUTPUT_STREAM_SENDING_BODY) {
		do {
			size_t chunk_len = sizeof(buffer) - 1;
			ret = belle_sip_body_handler_send_chunk(bh, msg, (uint8_t *)buffer, &chunk_len);
			if (chunk_len != 0) {
				off = 0;
				do {
					sendret = send_buffer(obj, buffer + off, chunk_len - off);
					if (sendret > 0) {
						off += sendret;
						if (off == chunk_len) {
							break;
						}
					} else if (belle_sip_error_code_is_would_block(-sendret)) {
						handle_ewouldblock(obj, buffer + off, chunk_len - off);
						return;
					} else { /*error or disconnection case*/
						goto done;
					}
				} while (1);
			}
		} while (ret == BELLE_SIP_CONTINUE);
		belle_sip_body_handler_end_transfer(bh);
	}
done:
	/*we get ready to send another message*/
	belle_sip_source_set_events((belle_sip_source_t *)obj, BELLE_SIP_EVENT_READ | BELLE_SIP_EVENT_ERROR);
	free_ewouldblock_buffer(obj);
	obj->out_state = OUTPUT_STREAM_IDLE;
	obj->inhibit_output_logging_buffer = 0;
	belle_sip_object_unref(obj->cur_out_message);
	obj->cur_out_message = NULL;
}

static void send_message(belle_sip_channel_t *obj, belle_sip_message_t *msg) {
	obj->cur_out_message = (belle_sip_message_t *)belle_sip_object_ref(msg);
	obj->out_state = OUTPUT_STREAM_SENDING_HEADERS;
	compress_body_if_required(obj->cur_out_message);
	_send_message(obj);
}

void belle_sip_channel_prepare(belle_sip_channel_t *obj) {
	channel_prepare_continue(obj);
}

static void channel_push_outgoing(belle_sip_channel_t *obj, belle_sip_message_t *msg) {
	obj->outgoing_messages = belle_sip_list_append(obj->outgoing_messages, msg);
}

static belle_sip_message_t *channel_pop_outgoing(belle_sip_channel_t *obj) {
	belle_sip_message_t *msg = NULL;
	if (obj->outgoing_messages) {
		msg = (belle_sip_message_t *)obj->outgoing_messages->data;
		obj->outgoing_messages = belle_sip_list_delete_link(obj->outgoing_messages, obj->outgoing_messages);
	}
	return msg;
}

static void channel_prepare_continue(belle_sip_channel_t *obj) {
	switch (obj->state) {
		case BELLE_SIP_CHANNEL_INIT:
			channel_begin_send_background_task(obj);
			belle_sip_channel_resolve(obj);
			break;
		case BELLE_SIP_CHANNEL_RES_DONE:
			belle_sip_channel_connect(obj);
			break;
		case BELLE_SIP_CHANNEL_READY:
			channel_process_queue(obj);
			break;
		default:
			break;
	}
}

static void channel_process_queue(belle_sip_channel_t *obj) {
	belle_sip_message_t *msg;
	belle_sip_object_ref(obj); /* we need to ref ourself because code below may trigger our destruction*/

	if (obj->out_state != OUTPUT_STREAM_IDLE) {
		_send_message(obj);
	}

	while (obj->state == BELLE_SIP_CHANNEL_READY && obj->out_state == OUTPUT_STREAM_IDLE &&
	       (msg = channel_pop_outgoing(obj)) != NULL) {
		send_message(obj, msg);
		belle_sip_object_unref(msg);
	}
	if (obj->state == BELLE_SIP_CHANNEL_READY && obj->out_state == OUTPUT_STREAM_IDLE) {
		channel_end_send_background_task(obj);
	}

	belle_sip_object_unref(obj);
}

void belle_sip_channel_set_ready(belle_sip_channel_t *obj, const struct sockaddr *addr, socklen_t slen) {
	char name[NI_MAXHOST];
	char serv[NI_MAXSERV];

	if (obj->local_ip == NULL || obj->local_port <= 0) {
		struct sockaddr_storage saddr;
		socklen_t slen2 = sizeof(saddr);
		int err;

		bctbx_sockaddr_remove_v4_mapping(addr, (struct sockaddr *)&saddr, &slen2);

		err = bctbx_getnameinfo((struct sockaddr *)&saddr, slen2, name, sizeof(name), serv, sizeof(serv),
		                        NI_NUMERICHOST | NI_NUMERICSERV);
		if (err != 0) {
			belle_sip_error("belle_sip_channel_set_ready(): getnameinfo() failed: %s", gai_strerror(err));
		} else {
			if (obj->local_ip == NULL) obj->local_ip = belle_sip_strdup(name);
			obj->local_port = atoi(serv);
			belle_sip_message("Channel has local address %s:%s", name, serv);
		}
	}
	channel_set_state(obj, BELLE_SIP_CHANNEL_READY);
	channel_process_queue(obj);
}

static int channel_dns_ttl_timeout(void *data, unsigned int event) {
	belle_sip_channel_t *obj = (belle_sip_channel_t *)data;
	belle_sip_message("Channel [%p]: DNS TTL timeout reached.", obj);
	obj->dns_ttl_timedout = TRUE;
	return BELLE_SIP_STOP;
}

/* returns the addrinfo from list that matches 'ai' in terms of content*/
static const struct addrinfo *addrinfo_in_list(const struct addrinfo *ai, const struct addrinfo *list) {
	const struct addrinfo *item = list;
	while (item != NULL) {
		if ((ai->ai_family == item->ai_family) && (bctbx_sockaddr_equals(ai->ai_addr, item->ai_addr))) {
			return item;
		}
		item = item->ai_next;
	}
	return NULL;
}

/* returns the first addrinfo from list, if it is matching 'ai' in terms content*/
static const struct addrinfo *addrinfo_is_first(const struct addrinfo *ai, const struct addrinfo *list) {
	if (list != NULL && (ai->ai_family == list->ai_family) && (bctbx_sockaddr_equals(ai->ai_addr, list->ai_addr)))
		return list;
	return NULL;
}

static void channel_res_done(void *data, belle_sip_resolver_results_t *results) {
	belle_sip_channel_t *obj = (belle_sip_channel_t *)data;
	const struct addrinfo *ai_list = NULL;
	const char *name = NULL;
	belle_sip_resolver_results_t *prev_dns_results = obj->resolver_results;

	if (obj->resolver_ctx) {
		belle_sip_object_unref(obj->resolver_ctx);
		obj->resolver_ctx = NULL;
	}
	if (results) {
		/*update current resolver_results, peer_list. current_peer still points to an element of the old list, if it
		 * exists*/
		obj->peer_list = ai_list = belle_sip_resolver_results_get_addrinfos(results);
		belle_sip_object_ref(results);
		name = belle_sip_resolver_results_get_name(results);
	}
	obj->resolver_results = results;

	if (ai_list) {
		int ttl = belle_sip_resolver_results_get_ttl(results);
		/* setup now our current_peer. It can be initialized for the first time, kept as before, or changed*/
		if (!obj->current_peer) {
			/*first time we connect*/
			channel_set_current_peer(obj, ai_list);
			channel_set_state(obj, BELLE_SIP_CHANNEL_RES_DONE);
		} else {
			const struct addrinfo *existing_peer;

			if (belle_sip_stack_reconnect_to_primary_asap_enabled(obj->stack)) {
				existing_peer = addrinfo_is_first(obj->current_peer, ai_list);
			} else {
				/*find if the current_peer we had is still existing in the new list. If yes, we will keep it*/
				existing_peer = addrinfo_in_list(obj->current_peer, ai_list);
			}

			if (existing_peer) {
				belle_sip_message("channel[%p]: DNS resolution returned the currently used address, continue using it",
				                  obj);
				channel_set_current_peer(obj, existing_peer);
				channel_set_state(obj, BELLE_SIP_CHANNEL_READY);
			} else {
				belle_sip_message("channel[%p]: DNS resolution returned an address different than the one being used, "
				                  "reconnect to the new address",
				                  obj);
				channel_set_current_peer(obj, ai_list);
				belle_sip_channel_close(obj);
				belle_sip_main_loop_do_later(obj->stack->ml, (belle_sip_callback_t)channel_connect_next,
				                             belle_sip_object_ref(obj));
				channel_set_state(obj, BELLE_SIP_CHANNEL_RETRY);
			}
		}
		channel_prepare_continue(obj);
		if (ttl > 0) {
			if (!obj->dns_ttl_timer) {
				obj->dns_ttl_timer = belle_sip_main_loop_create_timeout(obj->stack->ml, channel_dns_ttl_timeout, obj,
				                                                        ttl * 1000, "Channel DNS TTL timer");
			} else {
				/* Restart the timer for new period. */
				belle_sip_source_set_timeout_int64(obj->dns_ttl_timer, ttl * 1000LL);
				belle_sip_main_loop_add_source(obj->stack->ml, obj->dns_ttl_timer);
			}
		}
	} else {
		channel_set_current_peer(obj, NULL);
		belle_sip_error("%s: DNS resolution failed for %s", __FUNCTION__, name);
		channel_set_state(obj, BELLE_SIP_CHANNEL_ERROR);
	}
	/*now free the old resolver results if any, since we don't need the old current_peer*/
	if (prev_dns_results) belle_sip_object_unref(prev_dns_results);
}

void belle_sip_channel_resolve(belle_sip_channel_t *obj) {
	belle_sip_message("channel [%p]: starting resolution of %s", obj, obj->peer_name);
	channel_set_state(obj, BELLE_SIP_CHANNEL_RES_IN_PROGRESS);
	if (belle_sip_stack_dns_srv_enabled(obj->stack) && obj->lp != NULL && obj->no_srv == 0)
		obj->resolver_ctx =
		    belle_sip_stack_resolve(obj->stack, "sip", belle_sip_channel_get_transport_name_lower_case(obj),
		                            obj->peer_name, obj->peer_port, obj->ai_family, channel_res_done, obj);
	else
		obj->resolver_ctx = belle_sip_stack_resolve_a(obj->stack, obj->peer_name, obj->peer_port, obj->ai_family,
		                                              channel_res_done, obj);
	if (obj->resolver_ctx) {
		belle_sip_object_ref(obj->resolver_ctx);
	}
	return;
}

void belle_sip_channel_connect(belle_sip_channel_t *obj) {
	char ip[64];
	int port = obj->peer_port;

	BELLE_SIP_CHANNEL_CLEAR_TIMER(obj, expect_pong_timer);
	channel_set_state(obj, BELLE_SIP_CHANNEL_CONNECTING);
	bctbx_addrinfo_to_ip_address(obj->current_peer, ip, sizeof(ip), &port);
	/* update peer_port as it may have been overriden by SRV resolution*/
	if (port != obj->peer_port) {
		/*the SRV resolution provided a port number that must be used*/
		obj->srv_overrides_port = TRUE;
		obj->peer_port = port;
	}
	belle_sip_message("Trying to connect to [%s://%s:%i]", belle_sip_channel_get_transport_name(obj), ip,
	                  obj->peer_port);

	if (BELLE_SIP_OBJECT_VPTR(obj, belle_sip_channel_t)->connect(obj, obj->current_peer)) {
		belle_sip_error("Cannot connect to [%s://%s:%i]", belle_sip_channel_get_transport_name(obj), obj->peer_name,
		                obj->peer_port);
		channel_set_state(obj, BELLE_SIP_CHANNEL_ERROR);
	}
	return;
}

static void queue_message(belle_sip_channel_t *obj, belle_sip_message_t *msg) {
	belle_sip_object_ref(msg);
	channel_push_outgoing(obj, msg);
	if (obj->state == BELLE_SIP_CHANNEL_INIT) {
		belle_sip_channel_prepare(obj);
	} else if (obj->state == BELLE_SIP_CHANNEL_READY) {
		channel_process_queue(obj);
	}
}

typedef struct delay_send {
	belle_sip_channel_t *chan;
	belle_sip_message_t *msg;
} delay_send_t;

/* just to emulate network transmission delay */
static int on_delayed_send_do(delay_send_t *ctx) {
	belle_sip_message("on_delayed_send_do(): sending now");
	if (ctx->chan->state != BELLE_SIP_CHANNEL_ERROR && ctx->chan->state != BELLE_SIP_CHANNEL_DISCONNECTED) {
		queue_message(ctx->chan, ctx->msg);
	}
	belle_sip_object_unref(ctx->chan);
	belle_sip_object_unref(ctx->msg);
	belle_sip_free(ctx);
	return FALSE;
}

static void queue_message_delayed(belle_sip_channel_t *obj, belle_sip_message_t *msg) {
	delay_send_t *ctx = belle_sip_malloc(sizeof(delay_send_t));
	ctx->chan = (belle_sip_channel_t *)belle_sip_object_ref(obj);
	ctx->msg = (belle_sip_message_t *)belle_sip_object_ref(msg);

	belle_sip_main_loop_add_timeout(obj->stack->ml, (belle_sip_source_func_t)on_delayed_send_do, ctx,
	                                obj->stack->tx_delay);
	belle_sip_message("channel %p: message sending delayed by %i ms", obj, obj->stack->tx_delay);
}

int belle_sip_channel_queue_message(belle_sip_channel_t *obj, belle_sip_message_t *msg) {
	if (obj->stack->tx_delay > 0) {
		queue_message_delayed(obj, msg);
	} else queue_message(obj, msg);
	return 0;
}

void belle_sip_channel_force_close(belle_sip_channel_t *obj) {
	obj->force_close = 1;
	channel_set_state(obj, BELLE_SIP_CHANNEL_DISCONNECTED);
}

void belle_sip_channel_check_dns_reusability(belle_sip_channel_t *obj) {
	if (obj->dns_ttl_timedout) {
		obj->dns_ttl_timedout = FALSE;
		belle_sip_channel_resolve(obj);
	}
}

void belle_sip_channel_set_simulated_recv_return(belle_sip_channel_t *obj, int recv_error) {
	obj->simulated_recv_return = recv_error;
	obj->base.notify_required = (recv_error <= 0);
}

const char *belle_sip_channel_get_bank_identifier(const belle_sip_channel_t *obj) {
	return obj->bank_identifier;
}

void belle_sip_channel_set_bank_identifier(belle_sip_channel_t *obj, const char *identifier) {
	if (obj->bank_identifier) {
		belle_sip_error("A channel cannot have its bank identifier changed.");
		return;
	}
	obj->bank_identifier = bctbx_strdup(identifier);
}

#ifdef __ANDROID__

unsigned long
belle_sip_begin_background_task(const char *name, belle_sip_background_task_end_callback_t cb, void *data) {
	return wake_lock_acquire(name);
}

void belle_sip_end_background_task(unsigned long id) {
	wake_lock_release(id);
}

#elif !TARGET_OS_IPHONE && !defined(__APPLE__)

/*defines stubs*/
unsigned long
belle_sip_begin_background_task(const char *name, belle_sip_background_task_end_callback_t cb, void *data) {
	return 0;
}

void belle_sip_end_background_task(unsigned long id) {
	return;
}

#endif
