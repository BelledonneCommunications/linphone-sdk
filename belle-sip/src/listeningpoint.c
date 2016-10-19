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


void belle_sip_listening_point_init(belle_sip_listening_point_t *lp, belle_sip_stack_t *s, const char *address, int port){
	char *tmp;
	belle_sip_init_sockets();
	lp->stack=s;
	lp->listening_uri=belle_sip_uri_create(NULL,address);
	belle_sip_object_ref(lp->listening_uri);
	belle_sip_uri_set_port(lp->listening_uri,port);
	belle_sip_uri_set_transport_param(lp->listening_uri,BELLE_SIP_OBJECT_VPTR(lp,belle_sip_listening_point_t)->transport);
	tmp=belle_sip_object_to_string((belle_sip_object_t*)BELLE_SIP_LISTENING_POINT(lp)->listening_uri);
	if (strchr(address,':')) {
		lp->ai_family=AF_INET6;
	} else {
		lp->ai_family=AF_INET;
	}
	belle_sip_message("Creating listening point [%p] on [%s]",lp, tmp);
	belle_sip_free(tmp);
}

static void belle_sip_listening_point_uninit(belle_sip_listening_point_t *lp){
	char *tmp=belle_sip_object_to_string((belle_sip_object_t*)BELLE_SIP_LISTENING_POINT(lp)->listening_uri);
	belle_sip_listening_point_clean_channels(lp);
	belle_sip_message("Listening point [%p] on [%s] destroyed",lp, tmp);
	belle_sip_object_unref(lp->listening_uri);
	belle_sip_free(tmp);
	lp->channel_listener=NULL; /*does not unref provider*/
	belle_sip_uninit_sockets();
	belle_sip_listening_point_set_keep_alive(lp,-1);
}


void belle_sip_listening_point_add_channel(belle_sip_listening_point_t *lp, belle_sip_channel_t *chan){
	chan->lp=lp;
	belle_sip_channel_add_listener(chan,lp->channel_listener); /*add channel listener*/
	/*channel is already owned, no ref needed - REVISIT: channel should be initally unowned probably.*/
	
	/* The channel with names must be treated with higher priority by the get_channel() method so queued on front.
	 * This is to prevent the UDP listening point to dispatch incoming messages to channels that were created by inbound connection
	 * where name cannot be determined. When this arrives, there can be 2 channels for the same destination IP and strange problems can occur
	 * where requests are sent through name qualified channel and response received through name unqualified channel.
	 */
	if (chan->has_name)
		lp->channels=belle_sip_list_prepend(lp->channels,chan);
	else
		lp->channels=belle_sip_list_append(lp->channels,chan);
}

belle_sip_channel_t *belle_sip_listening_point_create_channel(belle_sip_listening_point_t *obj, const belle_sip_hop_t *hop){
	belle_sip_channel_t *chan=BELLE_SIP_OBJECT_VPTR(obj,belle_sip_listening_point_t)->create_channel(obj,hop);
	if (chan){
		belle_sip_listening_point_add_channel(obj,chan);
	}
	return chan;
}


void belle_sip_listening_point_remove_channel(belle_sip_listening_point_t *lp, belle_sip_channel_t *chan){
	belle_sip_channel_remove_listener(chan,lp->channel_listener);
	lp->channels=belle_sip_list_remove(lp->channels,chan);
	belle_sip_object_unref(chan);
}


void belle_sip_listening_point_clean_channels(belle_sip_listening_point_t *lp){
	int existing_channels = belle_sip_listening_point_get_channel_count(lp);
	belle_sip_list_t* iterator;
	
	if (existing_channels > 0) {
		belle_sip_message("Listening point destroying [%i] channels",existing_channels);
	}
	for (iterator=lp->channels;iterator!=NULL;iterator=iterator->next) {
		belle_sip_channel_t *chan=(belle_sip_channel_t*)iterator->data;
		belle_sip_channel_force_close(chan);
	}
	lp->channels=belle_sip_list_free_with_data(lp->channels,(void (*)(void*))belle_sip_object_unref);
}

int belle_sip_listening_point_get_channel_count(const belle_sip_listening_point_t *lp){
	return (int)belle_sip_list_size(lp->channels);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_listening_point_t);
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_listening_point_t)
	{ 
		BELLE_SIP_VPTR_INIT(belle_sip_listening_point_t, belle_sip_object_t,FALSE),
		(belle_sip_object_destroy_t)belle_sip_listening_point_uninit,
		NULL,
		NULL,
		BELLE_SIP_DEFAULT_BUFSIZE_HINT
	},
	NULL,
	NULL
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

const char *belle_sip_listening_point_get_ip_address(const belle_sip_listening_point_t *lp){
	return belle_sip_uri_get_host(lp->listening_uri);
}

int belle_sip_listening_point_get_port(const belle_sip_listening_point_t *lp){
	return belle_sip_uri_get_listening_port(lp->listening_uri);
}

const char *belle_sip_listening_point_get_transport(const belle_sip_listening_point_t *lp){
	return belle_sip_uri_get_transport_param(lp->listening_uri);
}

const belle_sip_uri_t* belle_sip_listening_point_get_uri(const  belle_sip_listening_point_t *lp) {
	return lp->listening_uri;
}
int belle_sip_listening_point_get_well_known_port(const char *transport){
	if (strcasecmp(transport,"UDP")==0 || strcasecmp(transport,"TCP")==0 ) return 5060;
	if (strcasecmp(transport,"DTLS")==0 || strcasecmp(transport,"TLS")==0 ) return 5061;
	belle_sip_error("No well known port for transport %s", transport);
	return -1;
}

belle_sip_channel_t *_belle_sip_listening_point_get_channel(belle_sip_listening_point_t *lp, const belle_sip_hop_t *hop, const struct addrinfo *addr){
	return belle_sip_channel_find_from_list_with_addrinfo(lp->channels,hop,addr);
}

belle_sip_channel_t *belle_sip_listening_point_get_channel(belle_sip_listening_point_t *lp,const belle_sip_hop_t *hop){
	return belle_sip_channel_find_from_list(lp->channels,lp->ai_family,hop);
}

static int send_keep_alive(belle_sip_channel_t* obj) {
	/*keep alive*/
	const char* crlfcrlf = "\r\n\r\n";
	size_t size=strlen(crlfcrlf);
	int err=belle_sip_channel_send(obj,crlfcrlf,size);
	
	if (err<=0 && !belle_sip_error_code_is_would_block(-err) && err!=-EINTR){
		belle_sip_error("channel [%p]: could not send [%u] bytes of keep alive from [%s://%s:%i]  to [%s:%i]"	,obj
			,(unsigned int)size
			,belle_sip_channel_get_transport_name(obj)
			,obj->local_ip
			,obj->local_port
			,obj->peer_name
			,obj->peer_port);

		return -1;
	}else{
		belle_sip_message("channel [%p]: keep alive sent to [%s://%s:%i]"
							,obj
							,belle_sip_channel_get_transport_name(obj)
							,obj->peer_name
							,obj->peer_port);
		return 0;
	}
}
static int keep_alive_timer_func(void *user_data, unsigned int events) {
	belle_sip_listening_point_t* lp=(belle_sip_listening_point_t*)user_data;
	belle_sip_list_t* iterator;
	belle_sip_channel_t* channel;
	belle_sip_list_t *to_be_closed=NULL;

	for (iterator=lp->channels;iterator!=NULL;iterator=iterator->next) {
		channel=(belle_sip_channel_t*)iterator->data;
		if (channel->state == BELLE_SIP_CHANNEL_READY && send_keep_alive(channel)==-1) { /*only send keep alive if ready*/
			to_be_closed=belle_sip_list_append(to_be_closed,channel);
		}
	}
	for (iterator=to_be_closed;iterator!=NULL;iterator=iterator->next){
		channel=(belle_sip_channel_t*)iterator->data;
		channel_set_state(channel,BELLE_SIP_CHANNEL_ERROR);
		belle_sip_channel_close(channel);
	}
	belle_sip_list_free(to_be_closed);
	return BELLE_SIP_CONTINUE_WITHOUT_CATCHUP;
}

void belle_sip_listening_point_set_keep_alive(belle_sip_listening_point_t *lp,int ms) {
	if (ms <=0) {
		if (lp->keep_alive_timer) {
			belle_sip_main_loop_remove_source(lp->stack->ml,lp->keep_alive_timer);
			belle_sip_object_unref(lp->keep_alive_timer);
			lp->keep_alive_timer=NULL;
		}
		return;
	}

	if (!lp->keep_alive_timer) {
		lp->keep_alive_timer = belle_sip_main_loop_create_timeout(lp->stack->ml
			, keep_alive_timer_func
			, lp
			, ms
			,"keep alive") ;
	} else {
		belle_sip_source_set_timeout(lp->keep_alive_timer,ms);
	}
	return;
}

int belle_sip_listening_point_get_keep_alive(const belle_sip_listening_point_t *lp) {
	return lp->keep_alive_timer?(int)belle_sip_source_get_timeout(lp->keep_alive_timer):-1;
}

void belle_sip_listening_point_set_channel_listener(belle_sip_listening_point_t *lp,belle_sip_channel_listener_t* channel_listener) {
	lp->channel_listener=channel_listener;
}
