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


#include "belle_sip_internal.h"


struct belle_http_provider{
	belle_sip_stack_t *stack;
	belle_sip_list_t *tcp_channels;
	belle_sip_list_t *tls_channels;
};

static int channel_on_event(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, unsigned int revents){
	if (revents & BELLE_SIP_EVENT_READ){
		belle_sip_message_t *msg;
		while ((msg=belle_sip_channel_pick_message(chan)))
			belle_sip_provider_dispatch_message(BELLE_SIP_PROVIDER(obj),msg);
	}
	return 0;
}

static int channel_on_auth_requested(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, const char* distinguished_name){
	if (BELLE_SIP_IS_INSTANCE_OF(chan,belle_sip_tls_channel_t)) {
		belle_http_provider_t *prov=BELLE_SIP_PROVIDER(obj);
		belle_sip_auth_event_t* auth_event = belle_sip_auth_event_create(NULL,NULL);
		belle_sip_tls_channel_t *tls_chan=BELLE_SIP_TLS_CHANNEL(chan);
		auth_event->mode=BELLE_SIP_AUTH_MODE_TLS;
		belle_sip_auth_event_set_distinguished_name(auth_event,distinguished_name);
		/*
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(prov->listeners,process_auth_requested,auth_event);
		belle_sip_tls_channel_set_client_certificates_chain(tls_chan,auth_event->cert);
		belle_sip_tls_channel_set_client_certificate_key(tls_chan,auth_event->key);
		belle_sip_auth_event_destroy(auth_event);
		*/
	}
	return 0;
}

static void channel_on_sending(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_message_t *msg){
}


BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_http_provider_t,belle_sip_channel_listener_t)
	channel_state_changed,
	channel_on_event,
	channel_on_sending,
	channel_on_auth_requested
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_http_provider_t,belle_sip_channel_listener_t);
	

static void http_provider_uninit(belle_http_provider_t *obj){
}

BELLE_SIP_INSTANCIATE_VPTR(belle_http_provider_t,belle_sip_object_t,http_provider_uninit,NULL,NULL,FALSE);

belle_sip_provider_t *belle_http_provider_new(belle_sip_stack_t *s){
	belle_sip_provider_t *p=belle_sip_object_new(belle_http_provider_t);
	p->stack=s;
	return p;
}
