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

#ifndef sender_task_h
#define sender_task_h

struct belle_sip_sender_task;



typedef void (*belle_sip_sender_task_callback_t)(struct belle_sip_sender_task* , void *data, int retcode);

struct belle_sip_sender_task{
	belle_sip_object_t base;
	belle_sip_provider_t *provider;
	belle_sip_message_t *message;
	belle_sip_source_t *source;
	belle_sip_channel_t *channel;
	belle_sip_hop_t hop;
	struct addrinfo *dest;
	unsigned long resolver_id;
	char *buf;
	belle_sip_sender_task_callback_t cb;
	void *cb_data;
};

typedef struct belle_sip_sender_task belle_sip_sender_task_t;



belle_sip_sender_task_t * belle_sip_sender_task_new(belle_sip_provider_t *provider, belle_sip_sender_task_callback_t cb, void *data);

void belle_sip_sender_task_send(belle_sip_sender_task_t *task, belle_sip_message_t *msg);

/*you can only call that after send has been called once */
int belle_sip_sender_task_is_reliable(belle_sip_sender_task_t *task);


#endif

