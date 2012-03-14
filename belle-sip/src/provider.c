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
#include "listeningpoint_internal.h"


static void belle_sip_provider_uninit(belle_sip_provider_t *p){
	belle_sip_list_free(p->listeners);
	belle_sip_list_free(p->lps);
}

static void channel_state_changed(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_channel_state_t state){
}

static void belle_sip_provider_dispatch_message(belle_sip_provider_t *prov, belle_sip_message_t *msg){
	/*should find existing transaction*/

	if (belle_sip_message_is_request(msg)){
		belle_sip_request_event_t event;
		event.source=prov;
		event.server_transaction=NULL;
		event.request=(belle_sip_request_t*)msg;
		event.dialog=NULL;
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(prov,process_request_event,&event);
	}else{
		belle_sip_response_event_t event;
		event.source=prov;
		event.client_transaction=NULL;
		event.dialog=NULL;
		event.response=(belle_sip_response_t*)msg;
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(prov,process_response_event,&event);
	}
}



static void fix_outgoing_via(belle_sip_provider_t *p, belle_sip_channel_t *chan, belle_sip_message_t *msg){
	belle_sip_header_via_t *via=BELLE_SIP_HEADER_VIA(belle_sip_message_get_header(msg,"via"));
	belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(via),"rport",NULL);
	if (belle_sip_header_via_get_host(via)==NULL){
		const char *local_ip;
		int local_port;
		local_ip=belle_sip_channel_get_local_address(chan,&local_port);
		belle_sip_header_via_set_host(via,local_ip);
		belle_sip_header_via_set_port(via,local_port);
		belle_sip_header_via_set_protocol(via,"SIP/2.0");
		belle_sip_header_via_set_transport(via,belle_sip_channel_get_transport_name(chan));
	}
	if (belle_sip_header_via_get_branch(via)==NULL){
		char *branchid=belle_sip_strdup_printf(BELLE_SIP_BRANCH_MAGIC_COOKIE "%x",belle_sip_random());
		belle_sip_header_via_set_branch(via,branchid);
		belle_sip_free(branchid);
	}
}
/*
static void belle_sip_provider_read_message(belle_sip_provider_t *prov, belle_sip_channel_t *chan){
	char buffer[belle_sip_network_buffer_size];
	int err;
	err=belle_sip_channel_recv(chan,buffer,sizeof(buffer));
	if (err>0){
		belle_sip_message_t *msg;
		buffer[err]='\0';
		belle_sip_message("provider %p read message from %s:%i\n%s",prov,chan->peer_name,chan->peer_port,buffer);
		msg=belle_sip_message_parse(buffer);
		if (msg){
			if (belle_sip_message_is_request(msg)) fix_incoming_via(BELLE_SIP_REQUEST(msg),chan->peer);
			belle_sip_provider_dispatch_message(prov,msg);
		}else{
			belle_sip_error("Could not parse this message.");
		}
	}
}
*/
static int channel_on_event(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, unsigned int revents){
	if (revents & BELLE_SIP_EVENT_READ){
		belle_sip_provider_dispatch_message(BELLE_SIP_PROVIDER(obj),belle_sip_channel_pick_message(chan));
	}
	return 0;
}

static void channel_on_sending(belle_sip_channel_listener_t *obj, belle_sip_channel_t *chan, belle_sip_message_t *msg){
	belle_sip_header_contact_t* contact = (belle_sip_header_contact_t*)belle_sip_message_get_header(msg,"Contact");
	belle_sip_uri_t* contact_uri;
	/*probably better to be in channel*/
	fix_outgoing_via((belle_sip_provider_t*)obj,chan,msg);
	/*fill contact if needeed*/
	if (!contact) {
		contact = belle_sip_header_contact_new();
		belle_sip_message_add_header(msg,(belle_sip_header_t*)contact);
	}
	if (!(contact_uri =belle_sip_header_address_get_uri((belle_sip_header_address_t*)contact))) {
		contact_uri = belle_sip_uri_new();
		belle_sip_header_address_set_uri((belle_sip_header_address_t*)contact,contact_uri);
	}
	if (!belle_sip_uri_get_host(contact_uri)) {
		belle_sip_uri_set_host(contact_uri,chan->local_ip);
	}
	if (belle_sip_uri_get_transport_param(contact_uri) == NULL && strcasecmp("udp",belle_sip_channel_get_transport_name(chan))!=0) {
		belle_sip_uri_set_transport_param(contact_uri,belle_sip_channel_get_transport_name_lower_case(chan));
	}
	if (belle_sip_uri_get_port(contact_uri) == 0 && chan->local_port!=5060) {
		belle_sip_uri_set_port(contact_uri,chan->local_port);
	}
}

BELLE_SIP_IMPLEMENT_INTERFACE_BEGIN(belle_sip_provider_t,belle_sip_channel_listener_t)
	channel_state_changed,
	channel_on_event,
	channel_on_sending
BELLE_SIP_IMPLEMENT_INTERFACE_END

BELLE_SIP_DECLARE_IMPLEMENTED_INTERFACES_1(belle_sip_provider_t,belle_sip_channel_listener_t);
	
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_provider_t,belle_sip_object_t,belle_sip_provider_uninit,NULL,NULL,FALSE);

belle_sip_provider_t *belle_sip_provider_new(belle_sip_stack_t *s, belle_sip_listening_point_t *lp){
	belle_sip_provider_t *p=belle_sip_object_new(belle_sip_provider_t);
	p->stack=s;
	belle_sip_provider_add_listening_point(p,lp);
	return p;
}

int belle_sip_provider_add_listening_point(belle_sip_provider_t *p, belle_sip_listening_point_t *lp){
	p->lps=belle_sip_list_append(p->lps,lp);
	return 0;
}

belle_sip_listening_point_t *belle_sip_provider_get_listening_point(belle_sip_provider_t *p, const char *transport){
	belle_sip_list_t *l;
	for(l=p->lps;l!=NULL;l=l->next){
		belle_sip_listening_point_t *lp=(belle_sip_listening_point_t*)l->data;
		if (strcasecmp(belle_sip_listening_point_get_transport(lp),transport)==0)
			return lp;
	}
	return NULL;
}

const belle_sip_list_t *belle_sip_provider_get_listening_points(belle_sip_provider_t *p){
	return p->lps;
}

void belle_sip_provider_add_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l){
	p->listeners=belle_sip_list_append(p->listeners,l);
}

void belle_sip_provider_remove_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l){
	p->listeners=belle_sip_list_remove(p->listeners,l);
}

belle_sip_header_call_id_t * belle_sip_provider_create_call_id(belle_sip_provider_t *prov){
	belle_sip_header_call_id_t *cid=belle_sip_header_call_id_new();
	char tmp[32];
	snprintf(tmp,sizeof(tmp),"%u",belle_sip_random());
	belle_sip_header_call_id_set_call_id(cid,tmp);
	return cid;
}

belle_sip_client_transaction_t *belle_sip_provider_create_client_transaction(belle_sip_provider_t *prov, belle_sip_request_t *req){
	if (strcmp(belle_sip_request_get_method(req),"INVITE")==0)
		return (belle_sip_client_transaction_t*)belle_sip_ict_new(prov,req);
	else 
		return (belle_sip_client_transaction_t*)belle_sip_nict_new(prov,req);
}

belle_sip_server_transaction_t *belle_sip_provider_create_server_transaction(belle_sip_provider_t *prov, belle_sip_request_t *req){
	if (strcmp(belle_sip_request_get_method(req),"INVITE")==0)
		return (belle_sip_server_transaction_t*)belle_sip_ist_new(prov,req);
	else 
		return (belle_sip_server_transaction_t*)belle_sip_nist_new(prov,req);
}

belle_sip_stack_t *belle_sip_provider_get_sip_stack(belle_sip_provider_t *p){
	return p->stack;
}

belle_sip_channel_t * belle_sip_provider_get_channel(belle_sip_provider_t *p, const char *name, int port, const char *transport){
	belle_sip_list_t *l;
	belle_sip_listening_point_t *candidate=NULL,*lp;
	belle_sip_channel_t *chan;
	for(l=p->lps;l!=NULL;l=l->next){
		lp=(belle_sip_listening_point_t*)l->data;
		if (strcasecmp(belle_sip_listening_point_get_transport(lp),transport)==0){
			chan=belle_sip_listening_point_get_channel(lp,name,port);
			if (chan) return chan;
			candidate=lp;
		}
	}
	if (candidate){
		chan=belle_sip_listening_point_create_channel(candidate,name,port);
		if (chan==NULL) belle_sip_error("Could not create channel to %s:%s:%i",transport,name,port);
		else belle_sip_channel_add_listener(chan,BELLE_SIP_CHANNEL_LISTENER(p));
		return chan;
	}
	belle_sip_error("No listening point matching for transport %s",transport);
	return NULL;
}

void belle_sip_provider_send_request(belle_sip_provider_t *p, belle_sip_request_t *req){
	belle_sip_hop_t hop={0};
	belle_sip_channel_t *chan;
	belle_sip_stack_get_next_hop(p->stack,req,&hop);
	chan=belle_sip_provider_get_channel(p,hop.host, hop.port, hop.transport);
	if (chan) {
		belle_sip_channel_queue_message(chan,BELLE_SIP_MESSAGE(req));
	}
}

void belle_sip_provider_send_response(belle_sip_provider_t *p, belle_sip_response_t *resp){
	belle_sip_hop_t hop;
	belle_sip_channel_t *chan;
	belle_sip_response_get_return_hop(resp,&hop);
	chan=belle_sip_provider_get_channel(p,hop.host, hop.port, hop.transport);
	if (chan) belle_sip_channel_queue_message(chan,BELLE_SIP_MESSAGE(resp));
}

belle_sip_response_t* belle_sip_response_event_get_response(const belle_sip_response_event_t* event) {
	return event->response;
}
/*private provider API*/

void belle_sip_provider_set_transaction_terminated(belle_sip_provider_t *p, belle_sip_transaction_t *t){
	
}

