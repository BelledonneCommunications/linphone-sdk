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
 * INVITE client transaction implementation.
**/

#include "belle_sip_internal.h"


static void on_ict_terminate(belle_sip_ict_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	if (obj->timer_A){
		belle_sip_transaction_stop_timer(base,obj->timer_A);
		belle_sip_object_unref(obj->timer_A);
		obj->timer_A=NULL;
	}
	if (obj->timer_B){
		belle_sip_transaction_stop_timer(base,obj->timer_B);
		belle_sip_object_unref(obj->timer_B);
		obj->timer_B=NULL;
	}
	if (obj->timer_D){
		belle_sip_transaction_stop_timer(base,obj->timer_D);
		belle_sip_object_unref(obj->timer_D);
		obj->timer_D=NULL;
	}
	if (obj->timer_M){
		belle_sip_transaction_stop_timer(base,obj->timer_M);
		belle_sip_object_unref(obj->timer_M);
		obj->timer_M=NULL;
	}
	if (obj->ack){
		belle_sip_object_unref(obj->ack);
		obj->ack=NULL;
	}
}

static void ict_destroy(belle_sip_ict_t *obj){
	on_ict_terminate(obj);
}

static belle_sip_request_t *make_ack(belle_sip_ict_t *obj, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	if (obj->ack==NULL){
		obj->ack=belle_sip_request_new();
		belle_sip_object_ref(obj->ack);
		belle_sip_request_set_method(obj->ack,"ACK");
		belle_sip_request_set_uri(obj->ack,belle_sip_request_get_uri(base->request));
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_VIA,FALSE);
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_CALL_ID,FALSE);
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_FROM,FALSE);
		belle_sip_util_copy_headers((belle_sip_message_t*)resp,(belle_sip_message_t*)obj->ack,BELLE_SIP_TO,FALSE);
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_CONTACT,TRUE);
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_ROUTE,TRUE);
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_MAX_FORWARDS,FALSE);
		belle_sip_message_add_header((belle_sip_message_t*)obj->ack,
		(belle_sip_header_t*)belle_sip_header_cseq_create(
			belle_sip_header_cseq_get_seq_number((belle_sip_header_cseq_t*)belle_sip_message_get_header((belle_sip_message_t*)base->request,BELLE_SIP_CSEQ)),
		    "ACK"));
	}

	return obj->ack;
}

/* Timer D: Wait time for response retransmits */
static int ict_on_timer_D(belle_sip_ict_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	if (base->state==BELLE_SIP_TRANSACTION_COMPLETED){
		belle_sip_transaction_terminate(base);
	}
	return BELLE_SIP_STOP;
}

/* Timer M: Wait time for retransmission of 2xx to INVITE or additional 2xx from other branches of a forked INVITE */
static int ict_on_timer_M(belle_sip_ict_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	if (base->state==BELLE_SIP_TRANSACTION_ACCEPTED){
		belle_sip_transaction_terminate(base);
	}
	return BELLE_SIP_STOP;
}

static void ict_on_response(belle_sip_ict_t *obj, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	int code=belle_sip_response_get_status_code(resp);
	const belle_sip_timer_config_t *cfg=belle_sip_transaction_get_timer_config(base);

	switch (base->state){
		case BELLE_SIP_TRANSACTION_CALLING:
			belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_PROCEEDING);
			/* no break*/
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			if (code>=300){
				belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_COMPLETED);
				belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)make_ack(obj,resp));
				belle_sip_client_transaction_notify_response((belle_sip_client_transaction_t*)obj,resp);
				obj->timer_D=belle_sip_timeout_source_new((belle_sip_source_func_t)ict_on_timer_D,obj,cfg->T1*64);
				belle_sip_transaction_start_timer(base,obj->timer_D);
			}else if (code>=200){
				obj->timer_M=belle_sip_timeout_source_new((belle_sip_source_func_t)ict_on_timer_M,obj,cfg->T1*64);
				belle_sip_transaction_start_timer(base,obj->timer_M);
				belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_ACCEPTED);
				belle_sip_client_transaction_notify_response((belle_sip_client_transaction_t*)obj,resp);
			}else if (code>=100){
				belle_sip_client_transaction_notify_response((belle_sip_client_transaction_t*)obj,resp);
			}
		break;
		case BELLE_SIP_TRANSACTION_ACCEPTED:
			if (code>=200 && code<300){
				belle_sip_client_transaction_notify_response((belle_sip_client_transaction_t*)obj,resp);
			}
		break;
		case BELLE_SIP_TRANSACTION_COMPLETED:
			if (code>=300 && obj->ack){
				belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)obj->ack);
			}
		break;
		default:
		break;
	}
}

/* Timer A: INVITE request retransmit interval, for UDP only */
static int ict_on_timer_A(belle_sip_ict_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;

	switch(base->state){
		case BELLE_SIP_TRANSACTION_CALLING:
		{
			/*reset the timer to twice the previous value, and retransmit */
			unsigned int prev_timeout=belle_sip_source_get_timeout(obj->timer_A);
			belle_sip_source_set_timeout(obj->timer_A,2*prev_timeout);
			belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)base->request);
		}
		break;
		default:
		break;
	}
	
	return BELLE_SIP_CONTINUE;
}

/* Timer B: INVITE transaction timeout timer */
static int ict_on_timer_B(belle_sip_ict_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	switch (base->state){
		case BELLE_SIP_TRANSACTION_CALLING:
			belle_sip_transaction_notify_timeout(base);
		break;
		default:
		break;
	}
	return BELLE_SIP_STOP;
}


static void ict_send_request(belle_sip_ict_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	const belle_sip_timer_config_t *cfg=belle_sip_transaction_get_timer_config(base);

	belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_CALLING);
	
	if (!belle_sip_channel_is_reliable(base->channel)){
		obj->timer_A=belle_sip_timeout_source_new((belle_sip_source_func_t)ict_on_timer_A,obj,cfg->T1);
		belle_sip_transaction_start_timer(base,obj->timer_A);
	}

	obj->timer_B=belle_sip_timeout_source_new((belle_sip_source_func_t)ict_on_timer_B,obj,cfg->T1*64);
	belle_sip_transaction_start_timer(base,obj->timer_B);
	
	belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)base->request);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_ict_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_ict_t)
	{
		{
			{
				BELLE_SIP_VPTR_INIT(belle_sip_ict_t,belle_sip_client_transaction_t,TRUE),
				(belle_sip_object_destroy_t)ict_destroy,
				NULL,
				NULL,
				BELLE_SIP_DEFAULT_BUFSIZE_HINT
			},
			(void (*)(belle_sip_transaction_t*))on_ict_terminate
		},
		(void (*)(belle_sip_client_transaction_t*))ict_send_request,
		(void (*)(belle_sip_client_transaction_t*,belle_sip_response_t*))ict_on_response
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END


belle_sip_ict_t *belle_sip_ict_new(belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_ict_t *obj=belle_sip_object_new(belle_sip_ict_t);
	belle_sip_client_transaction_init((belle_sip_client_transaction_t*)obj,prov,req);
	return obj;
}

