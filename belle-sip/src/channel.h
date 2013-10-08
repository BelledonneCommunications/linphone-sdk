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
#ifndef BELLE_SIP_CHANNEL_H
#define BELLE_SIP_CHANNEL_H

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#else

#endif

#define belle_sip_network_buffer_size 65535

typedef enum belle_sip_channel_state{
	BELLE_SIP_CHANNEL_INIT,
	BELLE_SIP_CHANNEL_RES_IN_PROGRESS,
	BELLE_SIP_CHANNEL_RES_DONE,
	BELLE_SIP_CHANNEL_CONNECTING,
	BELLE_SIP_CHANNEL_RETRY,
	BELLE_SIP_CHANNEL_READY,
	BELLE_SIP_CHANNEL_ERROR,
	BELLE_SIP_CHANNEL_DISCONNECTED
}belle_sip_channel_state_t;

const char * belle_sip_channel_state_to_string(belle_sip_channel_state_t state);

/**
* belle_sip_channel_t is an object representing a single communication channel ( socket or file descriptor), 
* unlike the belle_sip_listening_point_t that can owns several channels for TCP or TLS (incoming server child sockets or 
* outgoing client sockets).
**/
typedef struct belle_sip_channel belle_sip_channel_t;

BELLE_SIP_DECLARE_INTERFACE_BEGIN(belle_sip_channel_listener_t)
void (*on_state_changed)(belle_sip_channel_listener_t *l, belle_sip_channel_t *, belle_sip_channel_state_t state);
int (*on_event)(belle_sip_channel_listener_t *l, belle_sip_channel_t *obj, unsigned revents);
void (*on_sending)(belle_sip_channel_listener_t *l, belle_sip_channel_t *obj, belle_sip_message_t *msg);
int (*on_auth_requested)(belle_sip_channel_listener_t *l, belle_sip_channel_t *obj, const char* distinghised_name);
BELLE_SIP_DECLARE_INTERFACE_END

#define BELLE_SIP_CHANNEL_LISTENER(obj) BELLE_SIP_INTERFACE_CAST(obj,belle_sip_channel_listener_t)
#define MAX_CHANNEL_BUFF_SIZE 64000 + 1500 + 1

typedef enum input_stream_state {
	WAITING_MESSAGE_START=0
	,MESSAGE_AQUISITION=1
	,BODY_AQUISITION=2
}input_stream_state_t;

typedef struct belle_sip_channel_input_stream{
	input_stream_state_t state;
	char buff[MAX_CHANNEL_BUFF_SIZE];
	char* read_ptr;
	char* write_ptr;
	belle_sip_message_t *msg;
}belle_sip_channel_input_stream_t;

typedef struct belle_sip_stream_channel belle_sip_stream_channel_t;
typedef struct belle_sip_tls_channel belle_sip_tls_channel_t;

struct belle_sip_channel{
	belle_sip_source_t base;
	belle_sip_listening_point_t *lp; /*the listening point that owns this channel*/
	belle_sip_stack_t *stack;
	belle_sip_channel_state_t state;
	belle_sip_list_t *listeners;
	char *peer_cname;
	char *peer_name;
	int peer_port;
	char *local_ip;
	int local_port;
	char *public_ip;
	int public_port;
	unsigned long resolver_id;
	struct addrinfo *peer_list;
	struct addrinfo *current_peer;
	belle_sip_list_t *outgoing_messages;
	belle_sip_list_t* incoming_messages;
	belle_sip_channel_input_stream_t input_stream;
	belle_sip_source_t *inactivity_timer;
	uint64_t last_recv_time;
	int simulated_recv_return; /* used to simulate network error. 0= no data (disconnected) >0= do nothing -1= network error*/
	unsigned char force_close; /* when channel is intentionnaly disconnected, in order to prevent looping notifications*/
	unsigned char learnt_ip_port;
	unsigned char has_name; /*set when the name of the peer is known, which is not the case of inboud connections*/
};

#define BELLE_SIP_CHANNEL(obj)		BELLE_SIP_CAST(obj,belle_sip_channel_t)


BELLE_SIP_BEGIN_DECLS

void belle_sip_channel_add_listener(belle_sip_channel_t *chan, belle_sip_channel_listener_t *l);

void belle_sip_channel_remove_listener(belle_sip_channel_t *obj, belle_sip_channel_listener_t *l);

int belle_sip_channel_matches(const belle_sip_channel_t *obj, const belle_sip_hop_t *hop, const struct addrinfo *addr);

void belle_sip_channel_resolve(belle_sip_channel_t *obj);

void belle_sip_channel_connect(belle_sip_channel_t *obj);

void belle_sip_channel_prepare(belle_sip_channel_t *obj);

void belle_sip_channel_close(belle_sip_channel_t *obj);
/**
 *
 * returns number of send byte or <0 in case of error
 */
int belle_sip_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen);

int belle_sip_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen);
/*only used by channels implementation*/
void belle_sip_channel_set_ready(belle_sip_channel_t *obj, const struct sockaddr *addr, socklen_t slen);
void belle_sip_channel_init(belle_sip_channel_t *obj, belle_sip_stack_t *stack, const char *bindip,int localport, const char *peer_cname, const char *peername, int peer_port);
void belle_sip_channel_init_with_addr(belle_sip_channel_t *obj, belle_sip_stack_t *stack, const struct sockaddr *peer_addr, socklen_t addrlen);
void belle_sip_channel_set_socket(belle_sip_channel_t *obj, belle_sip_socket_t sock, belle_sip_source_func_t datafunc);
/*end of channel implementations*/
/**
 * pickup last received message. This method take the ownership of the message.
 */
belle_sip_message_t* belle_sip_channel_pick_message(belle_sip_channel_t *obj);

int belle_sip_channel_queue_message(belle_sip_channel_t *obj, belle_sip_message_t *msg);

int belle_sip_channel_is_reliable(const belle_sip_channel_t *obj);

const char * belle_sip_channel_get_transport_name(const belle_sip_channel_t *obj);
const char * belle_sip_channel_get_transport_name_lower_case(const belle_sip_channel_t *obj);

const struct addrinfo * belle_sip_channel_get_peer(belle_sip_channel_t *obj);

const char *belle_sip_channel_get_local_address(belle_sip_channel_t *obj, int *port);

#define belle_sip_channel_get_state(chan) ((chan)->state)

void channel_set_state(belle_sip_channel_t *obj, belle_sip_channel_state_t state);

/*just invokes the listeners to process data*/
int belle_sip_channel_process_data(belle_sip_channel_t *obj,unsigned int revents);

/*this function is to be used only in belle_sip_listening_point_clean_channels()*/
void belle_sip_channel_force_close(belle_sip_channel_t *obj);

/*this function is for transactions to report that a channel seems non working because a timeout occured for example.
 It results in the channel possibly entering error state, so that it gets cleaned. Next transactions will re-open a new one and
 get a better chance of receiving an answer.
 Returns TRUE if the channel enters error state, 0 otherwise (channel is given a second chance) */
int belle_sip_channel_notify_timeout(belle_sip_channel_t *obj);

BELLE_SIP_END_DECLS


BELLE_SIP_DECLARE_CUSTOM_VPTR_BEGIN(belle_sip_channel_t,belle_sip_source_t)
	const char *transport;
	int reliable;
	int (*connect)(belle_sip_channel_t *obj, const struct addrinfo *ai);
	int (*channel_send)(belle_sip_channel_t *obj, const void *buf, size_t buflen);
	int (*channel_recv)(belle_sip_channel_t *obj, void *buf, size_t buflen);
	void (*close)(belle_sip_channel_t *obj);
BELLE_SIP_DECLARE_CUSTOM_VPTR_END

/*
 * tls client certificate authentication. might be relevant for both tls and dtls channels.Only implemented in tls channel for now
 */
void belle_sip_tls_channel_set_client_certificates_chain(belle_sip_tls_channel_t *obj, belle_sip_certificates_chain_t* cert_chain);
void belle_sip_tls_channel_set_client_certificate_key(belle_sip_tls_channel_t *obj, belle_sip_signing_key_t* key);

#define BELLE_SIP_TLS_CHANNEL(obj)		BELLE_SIP_CAST(obj,belle_sip_tls_channel_t)

#endif
