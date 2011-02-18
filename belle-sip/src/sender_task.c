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

static void belle_sip_sender_task_uninit(belle_sip_sender_task_t *t){
	belle_sip_stack_t *stack=belle_sip_provider_get_sip_stack(t->provider);
	belle_sip_object_unref(t->message);
	if (t->channel) belle_sip_object_unref(t->channel);
	if (t->source) {
		belle_sip_main_loop_cancel_source(stack->ml,belle_sip_source_get_id(t->source));
		belle_sip_object_unref(t->source);
	}
	if (t->resolver_id>0) belle_sip_main_loop_cancel_source (stack->ml,t->resolver_id);
}

belle_sip_sender_task_t * belle_sip_sender_task_new(belle_sip_provider_t *provider,  belle_sip_sender_task_callback_t cb, void *data){
	belle_sip_sender_task_t *t=belle_sip_object_new(belle_sip_sender_task_t,belle_sip_sender_task_uninit);
	t->provider=provider;
	t->message=NULL;
	t->cb=cb;
	t->cb_data=data;
	return t;
}

static int do_send(belle_sip_sender_task_t *t){
	int err=belle_sip_channel_send(t->channel,t->buf,strlen(t->buf));
	if (err>0){
		t->cb(t,t->cb_data,0);
	}else if (err!=-EWOULDBLOCK){
		t->cb(t,t->cb_data,-1);
	}
	return err;
}

static int retry_send(belle_sip_sender_task_t *t, unsigned int revents){
	if (revents & BELLE_SIP_EVENT_WRITE){
		do_send(t);
		return 0;
	}
	/*timeout : notify the failure*/
	t->cb(t,t->cb_data,-1);
	return 0;
}

static void sender_task_send(belle_sip_sender_task_t *t){
	int err;
	if (t->buf==NULL)
		t->buf=belle_sip_message_to_string(t->message);
	err=do_send(t);
	if (err==-EWOULDBLOCK){
		belle_sip_stack_t *stack=belle_sip_provider_get_sip_stack(t->provider);
		/*need to retry later*/
		if (t->source==NULL){
			t->source=belle_sip_channel_create_source(t->channel,BELLE_SIP_EVENT_WRITE,BELLE_SIP_SOCKET_TIMEOUT,
			                                         (belle_sip_source_func_t)retry_send,t);
		}
		belle_sip_main_loop_add_source (stack->ml,t->source);
	}
}

static void sender_task_find_channel_and_send(belle_sip_sender_task_t *t){
	belle_sip_listening_point_t *lp=belle_sip_provider_get_listening_point(t->provider,t->hop.transport);
	if (lp==NULL){
		belle_sip_error("No listening point available for transport %s",t->hop.transport);
		goto error;
	}else{
		belle_sip_channel_t *chan=belle_sip_listening_point_find_output_channel (lp,t->dest);
		if (chan==NULL) goto error;
		t->channel=(belle_sip_channel_t*)belle_sip_object_ref(chan);
		
		sender_task_send(t);
	}
	return;
	error:
		t->cb(t,t->cb_data,-1);
}

static void sender_task_res_done(void *data, const char *name, struct addrinfo *res){
	belle_sip_sender_task_t *t=(belle_sip_sender_task_t*)data;
	t->resolver_id=0;
	if (res){
		t->dest=res;
		sender_task_find_channel_and_send(t);
	}else{
		belle_sip_io_error_event_t ev;
		ev.transport=t->hop.transport;
		ev.source=t->provider;
		ev.port=t->hop.port;
		ev.host=t->hop.host;
		BELLE_SIP_PROVIDER_INVOKE_LISTENERS(t->provider,process_io_error,&ev);
		t->cb(t,t->cb_data,-1);
	}
}

void belle_sip_sender_task_send(belle_sip_sender_task_t *t, belle_sip_message_t *msg){
	belle_sip_stack_t *stack=belle_sip_provider_get_sip_stack(t->provider);

	if (t->message!=NULL && t->message!=msg && msg!=NULL){
		belle_sip_object_unref(t->message);
		t->message=NULL;
		belle_sip_free(t->buf);
		t->buf=NULL;
	}
	if (t->message==NULL){
	    t->message=(belle_sip_message_t*)belle_sip_object_ref(msg);
	}
	if (t->channel){
		/*retransmission or new response to be sent, everything already done for setting up the transport*/
		sender_task_send(t);
		return;
	}
	if (belle_sip_message_is_request(t->message)){
		belle_sip_stack_get_next_hop(stack,BELLE_SIP_REQUEST(t->message),&t->hop);
		t->resolver_id=belle_sip_resolve(t->hop.host,t->hop.port,0,sender_task_res_done,t,stack->ml);
	}else{
		belle_sip_response_get_return_hop(BELLE_SIP_RESPONSE(t->message),&t->hop);
		sender_task_find_channel_and_send(t);
	}
}

int belle_sip_sender_task_is_reliable(belle_sip_sender_task_t *task){
	if (task->channel==NULL) belle_sip_fatal("The transport isn't known yet");
	return belle_sip_listening_point_is_reliable(task->channel->lp);
}
