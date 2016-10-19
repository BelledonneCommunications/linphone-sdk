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

/** 
 * INVITE server transaction implementation.
**/

#include "belle_sip_internal.h"

static void ist_on_terminate(belle_sip_ist_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	/*timer pointers are set to NULL because they can be released later*/
	if (obj->timer_G){
		belle_sip_transaction_stop_timer(base,obj->timer_G);
		belle_sip_object_unref(obj->timer_G);
		obj->timer_G=NULL;
	}
	if (obj->timer_H){
		belle_sip_transaction_stop_timer(base,obj->timer_H);
		belle_sip_object_unref(obj->timer_H);
		obj->timer_H=NULL;
	}
	if (obj->timer_I){
		belle_sip_transaction_stop_timer(base,obj->timer_I);
		belle_sip_object_unref(obj->timer_I);
		obj->timer_I=NULL;
	}
	if (obj->timer_L){
		belle_sip_transaction_stop_timer(base,obj->timer_L);
		belle_sip_object_unref(obj->timer_L);
		obj->timer_L=NULL;
	}
}

static void ist_destroy(belle_sip_ist_t *obj){
	ist_on_terminate(obj);
}

/* Timer G: INVITE response retransmit interval */
static int ist_on_timer_G(belle_sip_ist_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	if (base->state==BELLE_SIP_TRANSACTION_COMPLETED){
		const belle_sip_timer_config_t *cfg=belle_sip_transaction_get_timer_config(base);
		int interval=belle_sip_source_get_timeout(obj->timer_G);
	
		belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)base->last_response);
		belle_sip_source_set_timeout(obj->timer_G,MIN(2*interval,cfg->T2));
		return BELLE_SIP_CONTINUE;
	}
	return BELLE_SIP_STOP;
}

/* Timer H: Wait time for ACK receipt */
static int ist_on_timer_H(belle_sip_ist_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	if (base->state==BELLE_SIP_TRANSACTION_COMPLETED){
		belle_sip_transaction_terminate(base);
		/*FIXME: no ACK was received, should report the failure */
	}
	return BELLE_SIP_STOP;
}

/* Timer I: Wait time for ACK retransmits */
static int ist_on_timer_I(belle_sip_ist_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	belle_sip_transaction_terminate(base);
	return BELLE_SIP_STOP;
}

/* Timer L: Wait time for accepted INVITE request retransmits */
static int ist_on_timer_L(belle_sip_ist_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	belle_sip_transaction_terminate(base);
	return BELLE_SIP_STOP;
}

int belle_sip_ist_process_ack(belle_sip_ist_t *obj, belle_sip_message_t *ack){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	int ret=-1;
	switch(base->state){
		case BELLE_SIP_TRANSACTION_COMPLETED:
			/*clear timer G*/
			if (obj->timer_G){
				belle_sip_transaction_stop_timer(base,obj->timer_G);
				belle_sip_object_unref(obj->timer_G);
				obj->timer_G=NULL;
			}
			belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_CONFIRMED);
			if (!belle_sip_channel_is_reliable(base->channel)){
				const belle_sip_timer_config_t *cfg=belle_sip_transaction_get_timer_config(base);
				obj->timer_I=belle_sip_timeout_source_new((belle_sip_source_func_t)ist_on_timer_I,obj,cfg->T4);
				belle_sip_transaction_start_timer(base,obj->timer_I);
			}else ist_on_timer_I(obj);
		break;
		case BELLE_SIP_TRANSACTION_ACCEPTED:
			ret=0; /*let the ACK be reported to TU */
		break;
		default:
		break;
	}
	return ret;
}

static int ist_send_new_response(belle_sip_ist_t *obj, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	int code=belle_sip_response_get_status_code(resp);
	int ret=-1;
	switch(base->state){
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			{
				const belle_sip_timer_config_t *cfg=belle_sip_transaction_get_timer_config(base);
				ret=0;
				belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)resp);
				if (code>=200 && code<300){
					belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_ACCEPTED);
					obj->timer_L=belle_sip_timeout_source_new((belle_sip_source_func_t)ist_on_timer_L,obj,64*cfg->T1);
					belle_sip_transaction_start_timer(base,obj->timer_L);
				}else if (code>=300){
					belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_COMPLETED);
					if (!belle_sip_channel_is_reliable(base->channel)){
						obj->timer_G=belle_sip_timeout_source_new((belle_sip_source_func_t)ist_on_timer_G,obj,cfg->T1);
						belle_sip_transaction_start_timer(base,obj->timer_G);
					}
					obj->timer_H=belle_sip_timeout_source_new((belle_sip_source_func_t)ist_on_timer_H,obj,64*cfg->T1);
					belle_sip_transaction_start_timer(base,obj->timer_H);
				}
			}
		break;
		case BELLE_SIP_TRANSACTION_ACCEPTED:
			if (code>=200 && code<300){
				ret=0; /*let the response go to transport layer*/
			}
		default:
		break;
	}
	return ret;
}

static void ist_on_request_retransmission(belle_sip_nist_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	switch(base->state){
		case BELLE_SIP_TRANSACTION_PROCEEDING:
		case BELLE_SIP_TRANSACTION_COMPLETED:
			belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)base->last_response);
		break;
		default:
		break;
	}
}


BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_ist_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_ist_t)
	{
		{
			{
				BELLE_SIP_VPTR_INIT(belle_sip_ist_t,belle_sip_server_transaction_t,TRUE),
				(belle_sip_object_destroy_t)ist_destroy,
				NULL,
				NULL,
				BELLE_SIP_DEFAULT_BUFSIZE_HINT
			},
			(void (*)(belle_sip_transaction_t *))ist_on_terminate
		},
		(int (*)(belle_sip_server_transaction_t*, belle_sip_response_t *))ist_send_new_response,
		(void (*)(belle_sip_server_transaction_t*))ist_on_request_retransmission,
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END


belle_sip_ist_t *belle_sip_ist_new(belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_ist_t *obj=belle_sip_object_new(belle_sip_ist_t);
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;

	belle_sip_server_transaction_init((belle_sip_server_transaction_t*)obj,prov,req);
	belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_PROCEEDING);
	return obj;
}
