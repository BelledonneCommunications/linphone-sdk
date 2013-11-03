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

static void channel_prepare_continue(belle_sip_channel_t *obj);
static void channel_process_queue(belle_sip_channel_t *obj);

const char *belle_sip_channel_state_to_string(belle_sip_channel_state_t state){
	switch(state){
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

static belle_sip_list_t * for_each_weak_unref_free(belle_sip_list_t *l, belle_sip_object_destroy_notify_t notify, void *ptr){
	belle_sip_list_t *elem,*next;
	for(elem=l;elem!=NULL;elem=next){
		next=elem->next;
		belle_sip_object_weak_unref(elem->data,notify,ptr);
		belle_sip_free(elem);
	}
	return NULL;
}

static void belle_sip_channel_destroy(belle_sip_channel_t *obj){
	if (obj->peer_list) freeaddrinfo(obj->peer_list);
	if (obj->peer_cname) belle_sip_free(obj->peer_cname);
	belle_sip_free(obj->peer_name);
	if (obj->local_ip) belle_sip_free(obj->local_ip);
	obj->listeners=for_each_weak_unref_free(obj->listeners,(belle_sip_object_destroy_notify_t)belle_sip_channel_remove_listener,obj);
	if (obj->resolver_id>0) belle_sip_stack_resolve_cancel(obj->stack,obj->resolver_id);
	if (obj->inactivity_timer){
		belle_sip_main_loop_remove_source(obj->stack->ml,obj->inactivity_timer);
		belle_sip_object_unref(obj->inactivity_timer);
	}
	if (obj->public_ip) belle_sip_free(obj->public_ip);
	belle_sip_message("Channel [%p] destroyed",obj);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_channel_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_channel_t)=
{
	{
		BELLE_SIP_VPTR_INIT(belle_sip_channel_t,belle_sip_source_t,FALSE),
		(belle_sip_object_destroy_t)belle_sip_channel_destroy,
		NULL, /*clone*/
		NULL, /*marshal*/
	}
};

static void fix_incoming_via(belle_sip_request_t *msg, const struct addrinfo* origin){
	char received[NI_MAXHOST];
	char rport[NI_MAXSERV];
	belle_sip_header_via_t *via;
	int err;
	if (!origin) {
		belle_sip_warning("cannot fix via for message [%p], probably a test",msg);
		return;
	}
	err=getnameinfo(origin->ai_addr,origin->ai_addrlen,received,sizeof(received),
	                rport,sizeof(rport),NI_NUMERICHOST|NI_NUMERICSERV);
	if (err!=0){
		belle_sip_error("fix_via: getnameinfo() failed: %s",gai_strerror(errno));
		return;
	}
	via=BELLE_SIP_HEADER_VIA(belle_sip_message_get_header((belle_sip_message_t*)msg,"via"));
	if (via){
		const char* host = belle_sip_header_via_get_host(via);
		
		if (strcmp(host,received)!=0)
				belle_sip_header_via_set_received(via,received);
			
		if (belle_sip_parameters_has_parameter(BELLE_SIP_PARAMETERS(via),"rport")){
			int port = belle_sip_header_via_get_listening_port(via);
			int rport_int=atoi(rport);
			if (rport_int!=port) belle_sip_header_via_set_rport(via,atoi(rport));
		}
	}
}

/*token       =  1*(alphanum / "-" / "." / "!" / "%" / "*"
                     / "_" / "+" / "`" / "'" / "~" )
 *
 * */
static int is_token(const char* buff,size_t bufflen ) {
	int i;
	for (i=0; i<bufflen && buff[i]!='\0';i++) {
		switch(buff[i]) {
			case '-' :
			case '.' :
			case '!' :
			case '%' :
			case '*' :
			case '_' :
			case '+' :
			case '`' :
			case '\'' :
			case '~' :
				break;
		default:
			if ((buff[i]>='0' && buff[i]<='9')
				|| (buff[i]>='A' && buff[i]<='Z')
				|| (buff[i]>='a' && buff[i]<='z')
				|| (buff[i]=='\0'))
				continue;
			else
				return 0;
		}
	}
	return 1;
}
static int get_message_start_pos(char *buff, size_t bufflen) {
	/*FIXME still to optimize an better tested, specially REQUEST PATH and error path*/
	int i;
	int res=0;
	int status_code;
	char method[17];
	char saved_char1;
	char sip_version[10];
	int saved_char1_index;

	for(i=0; i<(int)bufflen-12;i++) { /*9=strlen( SIP/2.0\r\n)*/
		switch (buff[i]) { /*to avoid this character to be ignored by scanf*/
			case '\r':
			case '\n':
				continue;
			default:
				break;
		}
		saved_char1_index=bufflen-1;
		saved_char1=buff[saved_char1_index]; /*make sure buff is null terminated*/
		buff[saved_char1_index]='\0';
		res=sscanf(buff+i,"SIP/2.0 %d ",&status_code);
		if (res!=1) {
			res= sscanf(buff+i,"%16s %*s %9s\r\n",method,sip_version)==2
					&& is_token(method,sizeof(method))
					&& strcmp("SIP/2.0",sip_version)==0 ;
		}
		buff[saved_char1_index]=saved_char1;
		if (res==1) return i;
	}
	return -1;
}

static void belle_sip_channel_input_stream_reset(belle_sip_channel_input_stream_t* input_stream) {
	int remaining;
	
	remaining=input_stream->write_ptr-input_stream->read_ptr;
	if (remaining>0){
		/* copy remaning bytes at top of buffer*/
		memmove(input_stream->buff,input_stream->read_ptr,remaining);
		input_stream->read_ptr=input_stream->buff;
		input_stream->write_ptr=input_stream->buff+remaining;
		*input_stream->write_ptr='\0';
	}else{
		input_stream->read_ptr=input_stream->write_ptr=input_stream->buff;
	}
	input_stream->state=WAITING_MESSAGE_START;
	input_stream->msg=NULL;
}

static size_t belle_sip_channel_input_stream_get_buff_length(belle_sip_channel_input_stream_t* input_stream) {
	return MAX_CHANNEL_BUFF_SIZE - (input_stream->write_ptr-input_stream->buff);
}

void belle_sip_channel_set_public_ip_port(belle_sip_channel_t *obj, const char *public_ip, int port){
	if (obj->public_ip){
		int ip_changed=0;
		int port_changed=0;
		
		if (public_ip && strcmp(obj->public_ip,public_ip)!=0){
			ip_changed=1;
		}
		if (port!=obj->public_port){
			port_changed=1;
		}
		if (ip_changed || port_changed){
			belle_sip_warning("channel [%p]: public ip is changed from [%s:%i] to [%s:%i]",obj,obj->public_ip,obj->public_port,public_ip,port);
		}
		belle_sip_free(obj->public_ip);
		obj->public_ip=NULL;
	}else if (public_ip){
		belle_sip_message("channel [%p]: discovered public ip and port are [%s:%i]",obj,public_ip,port);
	}
	if (public_ip){
		obj->public_ip=belle_sip_strdup(public_ip);
	}
	obj->public_port=port;
}

static void belle_sip_channel_learn_public_ip_port(belle_sip_channel_t *obj, belle_sip_response_t *resp){
	belle_sip_header_via_t *via=belle_sip_message_get_header_by_type(resp,belle_sip_header_via_t);
	const char *received;
	int rport;
	
	if (!via){
		belle_sip_error("channel [%p]: no via in response.",obj);
		return;
	}

	if (!(received=belle_sip_header_via_get_received(via))) {
		/*use address from via*/;
		received=belle_sip_header_via_get_host(via);
	}
	
	rport=belle_sip_header_via_get_rport(via);
	if (rport<=0){
		/* no rport, the via port might be good then*/
		rport=belle_sip_header_via_get_listening_port(via);
	}
	belle_sip_channel_set_public_ip_port(obj,received,rport);

	obj->learnt_ip_port=TRUE;
}

static void belle_sip_channel_message_ready(belle_sip_channel_t *obj){
	belle_sip_message_t *msg=obj->input_stream.msg;
	if (belle_sip_message_is_response(msg)) belle_sip_channel_learn_public_ip_port(obj,BELLE_SIP_RESPONSE(msg));
	obj->incoming_messages=belle_sip_list_append(obj->incoming_messages,msg);
	belle_sip_channel_input_stream_reset(&obj->input_stream);
}

void belle_sip_channel_parse_stream(belle_sip_channel_t *obj){
	int offset;
	size_t read_size=0;
	belle_sip_header_content_length_t* content_length_header;
	int content_length;
	int num;
	
	while ((num=(obj->input_stream.write_ptr-obj->input_stream.read_ptr))>0){
	
		if (obj->input_stream.state == WAITING_MESSAGE_START) {
			/*search for request*/
			if ((offset=get_message_start_pos(obj->input_stream.read_ptr,num)) >=0 ) {
				/*message found !*/
				if (offset>0) {
					belle_sip_warning("trashing [%i] bytes in front of sip message on channel [%p]",offset,obj);
					obj->input_stream.read_ptr+=offset;
				}
				obj->input_stream.state=MESSAGE_AQUISITION;
			} else {
				belle_sip_debug("Unexpected [%s] received on channel [%p], trashing",obj->input_stream.read_ptr,obj);
				obj->input_stream.read_ptr=obj->input_stream.write_ptr;
				belle_sip_channel_input_stream_reset(&obj->input_stream);
				continue;
			}
		}

		if (obj->input_stream.state==MESSAGE_AQUISITION) {
			/*search for \r\n\r\n*/
			char* end_of_message=NULL;
			if ((end_of_message=strstr(obj->input_stream.read_ptr,"\r\n\r\n"))){
				int bytes_to_parse;
				char tmp;
				/*end of message found*/
				end_of_message+=4;/*add \r\n\r\n*/
				bytes_to_parse=end_of_message-obj->input_stream.read_ptr;
				tmp=*end_of_message;
				*end_of_message='\0';/*this is in order for the following log to print the message only to its end.*/
				belle_sip_message("channel [%p] read message of [%i] bytes:\n%.40s...",obj, bytes_to_parse, obj->input_stream.read_ptr);
				obj->input_stream.msg=belle_sip_message_parse_raw(obj->input_stream.read_ptr
										,bytes_to_parse
										,&read_size);
				*end_of_message=tmp;
				obj->input_stream.read_ptr+=read_size;
				if (obj->input_stream.msg && read_size > 0){
					belle_sip_message("channel [%p] [%i] bytes parsed",obj,(int)read_size);
					belle_sip_object_ref(obj->input_stream.msg);
					if (belle_sip_message_is_request(obj->input_stream.msg)) fix_incoming_via(BELLE_SIP_REQUEST(obj->input_stream.msg),obj->current_peer);
					/*check for body*/
					if ((content_length_header = (belle_sip_header_content_length_t*)belle_sip_message_get_header(obj->input_stream.msg,BELLE_SIP_CONTENT_LENGTH)) != NULL
							&& belle_sip_header_content_length_get_content_length(content_length_header)>0) {

						obj->input_stream.state=BODY_AQUISITION;
					} else {
						/*no body*/
						belle_sip_channel_message_ready(obj);
						continue;
					}
				}else{
					belle_sip_error("Could not parse [%s], on channel [%p] skipping to [%s]",obj->input_stream.read_ptr
																							,obj
																							,end_of_message);
					obj->input_stream.read_ptr=end_of_message;
					obj->input_stream.state=WAITING_MESSAGE_START;
					continue;
				}
			}else break; /*The message isn't finished to be receive, we need more data*/
		}

		if (obj->input_stream.state==BODY_AQUISITION) {
			content_length=belle_sip_header_content_length_get_content_length((belle_sip_header_content_length_t*)belle_sip_message_get_header(obj->input_stream.msg,BELLE_SIP_CONTENT_LENGTH));
			if (content_length <= obj->input_stream.write_ptr-obj->input_stream.read_ptr) {
				/*great body completed*/
				belle_sip_message("channel [%p] read [%i] bytes of body from %s:%i\n%s"	,obj
					,content_length
					,obj->peer_name
					,obj->peer_port
					,obj->input_stream.read_ptr);
				belle_sip_message_set_body(obj->input_stream.msg,obj->input_stream.read_ptr,content_length);
				obj->input_stream.read_ptr+=content_length;
				belle_sip_channel_message_ready(obj);
			}else{
				/*body is not finished, we need more data*/
				break;
			}
		}
	}
}

int belle_sip_channel_process_data(belle_sip_channel_t *obj,unsigned int revents){
	int num;

	if (revents & BELLE_SIP_EVENT_READ) {
		if (obj->simulated_recv_return>0) {
			num=belle_sip_channel_recv(obj,obj->input_stream.write_ptr,belle_sip_channel_input_stream_get_buff_length(&obj->input_stream)-1);
		} else {
			belle_sip_message("channel [%p]: simulating recv() returning %i",obj,obj->simulated_recv_return);
			num=obj->simulated_recv_return;
		}
	} else {
		belle_sip_error("Unexpected event [%i] on channel [%p]",revents,obj);
		num=-1; /*to trigger an error*/
	}
	if (num>0){
		char *begin=obj->input_stream.write_ptr;
		obj->input_stream.write_ptr+=num;
		/*first null terminate the read buff*/
		*obj->input_stream.write_ptr='\0';
		if (num >50) /*to avoid tracing server based keep alives*/
			belle_sip_message("channel [%p]: received [%i] new bytes from [%s:%i]:\n%s",obj,num,obj->peer_name,obj->peer_port,begin);
		belle_sip_channel_parse_stream(obj);
		if (obj->incoming_messages)
			BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(obj->listeners,belle_sip_channel_listener_t,on_event,obj,BELLE_SIP_EVENT_READ/*always a read event*/);
		
	} else if (num == 0) {
		channel_set_state(obj,BELLE_SIP_CHANNEL_DISCONNECTED);
		return BELLE_SIP_STOP;
	} else {
		belle_sip_error("Receive error on channel [%p]",obj);
		channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
		return BELLE_SIP_STOP;
	}
	return BELLE_SIP_CONTINUE;
}

static int channel_inactive_timeout(void *data, unsigned int event){
	belle_sip_channel_t *obj=(belle_sip_channel_t *)data;
	belle_sip_message("Channel [%p]: inactivity timeout reached.",obj);
	channel_set_state(obj,BELLE_SIP_CHANNEL_DISCONNECTED);
	return BELLE_SIP_STOP;
}

static void update_inactivity_timer(belle_sip_channel_t *obj, int from_recv){
	int inactive_timeout=belle_sip_stack_get_inactive_transport_timeout(obj->stack)*1000;
	if (inactive_timeout>0){
		if (!obj->inactivity_timer ){
			obj->inactivity_timer=belle_sip_main_loop_create_timeout(obj->stack->ml,channel_inactive_timeout,obj,inactive_timeout,"Channel inactivity timer");
		}else{
			/*restart the timer for new period*/
			belle_sip_source_set_timeout(obj->inactivity_timer,inactive_timeout);
		}
	}else{
		if (obj->inactivity_timer){
			belle_sip_main_loop_remove_source(obj->stack->ml,obj->inactivity_timer);
			belle_sip_object_unref(obj->inactivity_timer);
			obj->inactivity_timer=NULL;
		}
	}
	if (from_recv)
		obj->last_recv_time=belle_sip_time_ms();
}

void belle_sip_channel_init(belle_sip_channel_t *obj, belle_sip_stack_t *stack,const char *bindip,int localport,const char *peer_cname, const char *peername, int peer_port){
	/*to initialize our base class:*/
	belle_sip_channel_set_socket(obj,-1,NULL);
	
	/*then initialize members*/
	obj->peer_cname=peer_cname ? belle_sip_strdup(peer_cname) : NULL;
	obj->peer_name=belle_sip_strdup(peername);
	obj->peer_port=peer_port;
	obj->stack=stack;
	if (bindip && strcmp(bindip,"::0")!=0 && strcmp(bindip,"0.0.0.0")!=0)
		obj->local_ip=belle_sip_strdup(bindip);
	obj->local_port=localport;
	obj->simulated_recv_return=1;/*not set*/
	if (peername){
		/*check if we are given a real dns name or just an ip address*/
		struct addrinfo *ai=belle_sip_ip_address_to_addrinfo(AF_UNSPEC,peername,peer_port);
		if (ai) freeaddrinfo(ai);
		else obj->has_name=TRUE;
	}
	belle_sip_channel_input_stream_reset(&obj->input_stream);
	update_inactivity_timer(obj,FALSE);
}

void belle_sip_channel_init_with_addr(belle_sip_channel_t *obj, belle_sip_stack_t *stack, const struct sockaddr *peer_addr, socklen_t addrlen){
	char remoteip[64];
	struct addrinfo ai;
	int peer_port;
	
	memset(&ai,0,sizeof(ai));
	ai.ai_family=peer_addr->sa_family;
	ai.ai_addr=(struct sockaddr*)peer_addr;
	ai.ai_addrlen=addrlen;
	belle_sip_addrinfo_to_ip(&ai,remoteip,sizeof(remoteip),&peer_port);
	belle_sip_channel_init(obj,stack,NULL,0,NULL,remoteip,peer_port);
	obj->peer_list=obj->current_peer=belle_sip_ip_address_to_addrinfo(ai.ai_family, obj->peer_name,obj->peer_port);
}

void belle_sip_channel_set_socket(belle_sip_channel_t *obj, belle_sip_socket_t sock, belle_sip_source_func_t datafunc){
	belle_sip_socket_source_init((belle_sip_source_t*)obj
									, datafunc
									, obj
									, sock
									, BELLE_SIP_EVENT_READ|BELLE_SIP_EVENT_WRITE
									, -1);
}

void belle_sip_channel_add_listener(belle_sip_channel_t *obj, belle_sip_channel_listener_t *l){
	obj->listeners=belle_sip_list_append(obj->listeners,
	                belle_sip_object_weak_ref(l,
	                (belle_sip_object_destroy_notify_t)belle_sip_channel_remove_listener,obj));
}

void belle_sip_channel_remove_listener(belle_sip_channel_t *obj, belle_sip_channel_listener_t *l){
	belle_sip_object_weak_unref(l,(belle_sip_object_destroy_notify_t)belle_sip_channel_remove_listener,obj);
	obj->listeners=belle_sip_list_remove(obj->listeners,l);
}

int belle_sip_channel_matches(const belle_sip_channel_t *obj, const belle_sip_hop_t *hop, const struct addrinfo *addr){
	if (hop && strcmp(hop->host,obj->peer_name)==0 && hop->port==obj->peer_port){
		if (hop->cname && obj->peer_cname && strcmp(hop->cname,obj->peer_cname)!=0)
			return 0; /*cname mismatch*/
		return 1;
	}
	if (addr && obj->current_peer)
		return addr->ai_addrlen==obj->current_peer->ai_addrlen && memcmp(addr->ai_addr,obj->current_peer->ai_addr,addr->ai_addrlen)==0;
	return 0;
}

const char *belle_sip_channel_get_local_address(belle_sip_channel_t *obj, int *port){
	if (port) *port=obj->local_port;
	return obj->local_ip;
}

int belle_sip_channel_is_reliable(const belle_sip_channel_t *obj){
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->reliable;
}

const char * belle_sip_channel_get_transport_name_lower_case(const belle_sip_channel_t *obj){
	const char* transport = belle_sip_channel_get_transport_name(obj);
	if (strcasecmp("udp",transport)==0) return "udp";
	else if (strcasecmp("tcp",transport)==0) return "tcp";
	else if (strcasecmp("tls",transport)==0) return "tls";
	else if (strcasecmp("dtls",transport)==0) return "dtls";
	else {
		belle_sip_message("Cannot convert [%s] to lower case",transport);
		return transport;
	}
}

const char * belle_sip_channel_get_transport_name(const belle_sip_channel_t *obj){
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->transport;
}

int belle_sip_channel_send(belle_sip_channel_t *obj, const void *buf, size_t buflen){
	update_inactivity_timer(obj,FALSE);
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->channel_send(obj,buf,buflen);
}

int belle_sip_channel_recv(belle_sip_channel_t *obj, void *buf, size_t buflen){
	update_inactivity_timer(obj,TRUE);
	return BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->channel_recv(obj,buf,buflen);
}

void belle_sip_channel_close(belle_sip_channel_t *obj){
	if (BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->close)
		BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->close(obj); /*udp channel doesn't have close function*/
	belle_sip_main_loop_remove_source(obj->stack->ml,(belle_sip_source_t*)obj);
	belle_sip_source_uninit((belle_sip_source_t*)obj);
}

const struct addrinfo * belle_sip_channel_get_peer(belle_sip_channel_t *obj){
	return obj->current_peer;
}

belle_sip_message_t* belle_sip_channel_pick_message(belle_sip_channel_t *obj) {
	belle_sip_message_t* result=NULL;
	belle_sip_list_t* front;
	if ((front=obj->incoming_messages)!=NULL) {
		result = (belle_sip_message_t*)obj->incoming_messages->data;
		obj->incoming_messages=belle_sip_list_remove_link(obj->incoming_messages,obj->incoming_messages);
		belle_sip_free(front);
	}
	return result;
}

static void channel_invoke_state_listener(belle_sip_channel_t *obj){
	if (obj->state==BELLE_SIP_CHANNEL_DISCONNECTED || obj->state==BELLE_SIP_CHANNEL_ERROR){
		belle_sip_channel_close(obj);
	}
	BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(obj->listeners,belle_sip_channel_listener_t,on_state_changed,obj,obj->state);
}

static void channel_invoke_state_listener_defered(belle_sip_channel_t *obj){
	channel_invoke_state_listener(obj);
	belle_sip_object_unref(obj);
}

static void belle_sip_channel_handle_error(belle_sip_channel_t *obj){
	if (obj->state!=BELLE_SIP_CHANNEL_READY){
		/* Previous connection attempts were failed (channel could not get ready).*/
		/* See if you can retry on an alternate ip address.*/
		if (obj->current_peer && obj->current_peer->ai_next){ /*obj->current_peer may be null in case of dns error*/
			obj->current_peer=obj->current_peer->ai_next;
			channel_set_state(obj,BELLE_SIP_CHANNEL_RETRY);
			belle_sip_channel_close(obj);
			belle_sip_channel_connect(obj);
			return;
		}/*else we have already tried all the ip addresses, so give up and notify the error*/
	}/*else the channel was previously working good with the current ip address but now fails, so let's notify the error*/
	
	obj->state=BELLE_SIP_CHANNEL_ERROR;
	/*Because error notification will in practice trigger the destruction of possible transactions and this channel,
	* it is safer to invoke the listener outside the current call stack.
	* Indeed the channel encounters network errors while being called for transmiting by a transaction.
	*/
	belle_sip_object_ref(obj);
	belle_sip_main_loop_do_later(obj->stack->ml,(belle_sip_callback_t)channel_invoke_state_listener_defered,obj);
}

int belle_sip_channel_notify_timeout(belle_sip_channel_t *obj){
	const int too_long=60;
	if (belle_sip_time_ms() - obj->last_recv_time>=(too_long * 1000)){
		belle_sip_message("A timeout related to this channel occured and no message received during last %i seconds. This channel is suspect, moving to error state",too_long);
		channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
		return TRUE;
	}
	return FALSE;
}

void channel_set_state(belle_sip_channel_t *obj, belle_sip_channel_state_t state) {
	belle_sip_message("channel %p: state %s",obj,belle_sip_channel_state_to_string(state));
	
	if (state==BELLE_SIP_CHANNEL_ERROR){
		belle_sip_channel_handle_error(obj);
	}else{
		obj->state=state;
		channel_invoke_state_listener(obj);
	}
}

static void _send_message(belle_sip_channel_t *obj, belle_sip_message_t *msg){
	char buffer[belle_sip_network_buffer_size];
	size_t len=0;
	int ret=0;
	belle_sip_error_code error=BELLE_SIP_OK;
	
	BELLE_SIP_INVOKE_LISTENERS_ARG1_ARG2(obj->listeners,belle_sip_channel_listener_t,on_sending,obj,msg);
	error=belle_sip_object_marshal((belle_sip_object_t*)msg,buffer,sizeof(buffer),&len);
	if ((error==BELLE_SIP_OK) && (len>0)){
		buffer[len]='\0';
		if (!obj->stack->send_error)
			ret=belle_sip_channel_send(obj,buffer,len);
		else
			/*debug case*/
			ret=obj->stack->send_error;

		if (ret<0){
			belle_sip_error("channel [%p]: could not send [%i] bytes from [%s://%s:%i]  to [%s:%i]"	,obj
				,(int)len
				,belle_sip_channel_get_transport_name(obj)
				,obj->local_ip
				,obj->local_port
				,obj->peer_name
				,obj->peer_port);
			channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
		}else if (len==ret){
			belle_sip_message("channel [%p]: message sent to [%s://%s:%i], size: [%i] bytes\n%s"
								,obj
								,belle_sip_channel_get_transport_name(obj)
								,obj->peer_name
								,obj->peer_port
								,ret
								,buffer);
		}else{
			belle_sip_error("channel [%p]: message partly sent to [%s://%s:%i], sent: [%i/%i] bytes:\n%s"
								,obj
								,belle_sip_channel_get_transport_name(obj)
								,obj->peer_name
								,obj->peer_port
								,ret
								,(int)len
								,buffer);
		}
	}
}

/* just to emulate network transmission delay */

typedef struct delayed_send{
	belle_sip_channel_t *chan;
	belle_sip_message_t *msg;
}delayed_send_t;

static int on_delayed_send_do(delayed_send_t *ds){
	belle_sip_message("on_delayed_send_do(): sending now");
	if (ds->chan->state==BELLE_SIP_CHANNEL_READY){
		_send_message(ds->chan,ds->msg);
	}
	belle_sip_object_unref(ds->chan);
	belle_sip_object_unref(ds->msg);
	belle_sip_free(ds);
	return FALSE;
}

static void send_message(belle_sip_channel_t *obj, belle_sip_message_t *msg){
	if (obj->stack->tx_delay>0){
		delayed_send_t *ds=belle_sip_new(delayed_send_t);
		ds->chan=(belle_sip_channel_t*)belle_sip_object_ref(obj);
		ds->msg=(belle_sip_message_t*)belle_sip_object_ref(msg);
		belle_sip_main_loop_add_timeout(obj->stack->ml,(belle_sip_source_func_t)on_delayed_send_do,ds,obj->stack->tx_delay);
		belle_sip_message("channel %p: message sending delayed by %i ms",obj,obj->stack->tx_delay);
	}else _send_message(obj,msg);
}

void belle_sip_channel_prepare(belle_sip_channel_t *obj){
	channel_prepare_continue(obj);
}

static void channel_push_outgoing(belle_sip_channel_t *obj, belle_sip_message_t *msg){
	obj->outgoing_messages=belle_sip_list_append(obj->outgoing_messages,msg);
}

static belle_sip_message_t *channel_pop_outgoing(belle_sip_channel_t *obj){
	belle_sip_message_t *msg=NULL;
	if (obj->outgoing_messages){
		msg=(belle_sip_message_t*)obj->outgoing_messages->data;
		obj->outgoing_messages=belle_sip_list_delete_link(obj->outgoing_messages,obj->outgoing_messages);
	}
	return msg;
}

static void channel_prepare_continue(belle_sip_channel_t *obj){
	switch(obj->state){
		case BELLE_SIP_CHANNEL_INIT:
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

static void channel_process_queue(belle_sip_channel_t *obj){
	belle_sip_message_t *msg;
	belle_sip_object_ref(obj);/* we need to ref ourself because code below may trigger our destruction*/

	while((msg=channel_pop_outgoing(obj))!=NULL && obj->state==BELLE_SIP_CHANNEL_READY) {
		send_message(obj, msg);
		belle_sip_object_unref(msg);
	}

	belle_sip_object_unref(obj);
}

void belle_sip_channel_set_ready(belle_sip_channel_t *obj, const struct sockaddr *addr, socklen_t slen){
	char name[NI_MAXHOST];
	char serv[NI_MAXSERV];

	if (obj->local_ip==NULL){
		int err=getnameinfo(addr,slen,name,sizeof(name),serv,sizeof(serv),NI_NUMERICHOST|NI_NUMERICSERV);
		if (err!=0){
			belle_sip_error("belle_sip_channel_set_ready(): getnameinfo() failed: %s",gai_strerror(err));
		}else{
			obj->local_ip=belle_sip_strdup(name);
			obj->local_port=atoi(serv);
			belle_sip_message("Channel has local address %s:%s",name,serv);
		}
	}
	channel_set_state(obj,BELLE_SIP_CHANNEL_READY);
	channel_process_queue(obj);
}

static void channel_res_done(void *data, const char *name, struct addrinfo *ai_list){
	belle_sip_channel_t *obj=(belle_sip_channel_t*)data;
	obj->resolver_id=0;
	if (ai_list){
		obj->peer_list=obj->current_peer=ai_list;
		channel_set_state(obj,BELLE_SIP_CHANNEL_RES_DONE);
		channel_prepare_continue(obj);
	}else{
		belle_sip_error("%s: Resolution done but no result", __FUNCTION__);
		channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
	}
}

void belle_sip_channel_resolve(belle_sip_channel_t *obj){
	channel_set_state(obj,BELLE_SIP_CHANNEL_RES_IN_PROGRESS);
	/*obj->resolver_id=belle_sip_stack_resolve(obj->stack, belle_sip_channel_get_transport_name_lower_case(obj), obj->peer_name, obj->peer_port, obj->lp->ai_family, channel_res_done, obj);*/
	obj->resolver_id=belle_sip_stack_resolve_a(obj->stack, obj->peer_name, obj->peer_port, obj->lp->ai_family, channel_res_done, obj);
	if (obj->resolver_id==-1){
		channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
	}
	return ;
}

void belle_sip_channel_connect(belle_sip_channel_t *obj){
	char ip[64];
	
	channel_set_state(obj,BELLE_SIP_CHANNEL_CONNECTING);
	belle_sip_addrinfo_to_ip(obj->current_peer,ip,sizeof(ip),NULL);
	belle_sip_message("Trying to connect to [%s://%s:%i]",belle_sip_channel_get_transport_name(obj),ip,obj->peer_port);
	
	if(BELLE_SIP_OBJECT_VPTR(obj,belle_sip_channel_t)->connect(obj,obj->current_peer)) {
		belle_sip_error("Cannot connect to [%s://%s:%i]",belle_sip_channel_get_transport_name(obj),obj->peer_name,obj->peer_port);
		channel_set_state(obj,BELLE_SIP_CHANNEL_ERROR);
	}
	return;
}

int belle_sip_channel_queue_message(belle_sip_channel_t *obj, belle_sip_message_t *msg){
	belle_sip_object_ref(msg);
	channel_push_outgoing(obj,msg);
	if (obj->state==BELLE_SIP_CHANNEL_INIT){
		belle_sip_channel_prepare(obj);
	}else if (obj->state==BELLE_SIP_CHANNEL_READY) {
		channel_process_queue(obj);
	}		
	return 0;
}

void belle_sip_channel_force_close(belle_sip_channel_t *obj){
	obj->force_close=1;
	channel_set_state(obj,BELLE_SIP_CHANNEL_DISCONNECTED);
}


