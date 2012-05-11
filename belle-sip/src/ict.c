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

/** 
 * INVITE client transaction implementation.
**/

#include "belle_sip_internal.h"

static void ict_destroy(belle_sip_ict_t *obj){
}

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
	if (obj->ack){
		belle_sip_object_unref(obj->ack);
		obj->ack=NULL;
	}
}

static belle_sip_request_t *make_ack(belle_sip_ict_t *obj, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	if (obj->ack==NULL){
		obj->ack=belle_sip_request_new();
		belle_sip_request_set_method(obj->ack,"ACK");
		belle_sip_request_set_uri(obj->ack,belle_sip_request_get_uri(base->request));
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_VIA,FALSE);
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_CALL_ID,FALSE);
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_FROM,FALSE);
		belle_sip_util_copy_headers((belle_sip_message_t*)resp,(belle_sip_message_t*)obj->ack,BELLE_SIP_TO,FALSE);
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_CONTACT,TRUE);
		belle_sip_util_copy_headers((belle_sip_message_t*)base->request,(belle_sip_message_t*)obj->ack,BELLE_SIP_ROUTE,TRUE);
		belle_sip_message_add_header((belle_sip_message_t*)obj->ack,
		(belle_sip_header_t*)belle_sip_header_cseq_create(
			belle_sip_header_cseq_get_seq_number((belle_sip_header_cseq_t*)belle_sip_message_get_header((belle_sip_message_t*)base->request,BELLE_SIP_CSEQ)),
		    "CANCEL"));
	}

	return obj->ack;
}

static int ict_on_timer_D(belle_sip_ict_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	if (base->state==BELLE_SIP_TRANSACTION_COMPLETED){
		belle_sip_transaction_terminate(base);
	}
	return BELLE_SIP_STOP;
}

static void ict_on_response(belle_sip_ict_t *obj, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	int code=belle_sip_response_get_status_code(resp);

	switch (base->state){
		case BELLE_SIP_TRANSACTION_CALLING:
			base->state=BELLE_SIP_TRANSACTION_PROCEEDING;
			/* no break*/
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			if (code>=300){
				base->state=BELLE_SIP_TRANSACTION_COMPLETED;
				belle_sip_client_transaction_notify_response((belle_sip_client_transaction_t*)obj,resp);
				belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)make_ack(obj,resp));
				obj->timer_D=belle_sip_timeout_source_new((belle_sip_source_func_t)ict_on_timer_D,obj,32000);
				belle_sip_transaction_start_timer(base,obj->timer_D);
			}else if (code>=200){
				belle_sip_transaction_terminate(base);
				belle_sip_client_transaction_notify_response((belle_sip_client_transaction_t*)obj,resp);
			}else if (code>=100){
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

static int ict_on_timer_B(belle_sip_ict_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	switch (base->state){
		case BELLE_SIP_TRANSACTION_CALLING:
			belle_sip_transaction_notify_timeout(base);
			belle_sip_transaction_terminate(base);
		break;
		default:
		break;
	}
	return BELLE_SIP_STOP;
}


static void ict_send_request(belle_sip_ict_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	const belle_sip_timer_config_t *cfg=belle_sip_transaction_get_timer_config(base);

	base->state=BELLE_SIP_TRANSACTION_CALLING;
	belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)base->request);
	
	if (!belle_sip_channel_is_reliable(base->channel)){
		obj->timer_A=belle_sip_timeout_source_new((belle_sip_source_func_t)ict_on_timer_A,obj,cfg->T1);
		belle_sip_transaction_start_timer(base,obj->timer_A);
	}

	obj->timer_B=belle_sip_timeout_source_new((belle_sip_source_func_t)ict_on_timer_B,obj,cfg->T1*64);
	belle_sip_transaction_start_timer(base,obj->timer_B);
	
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_ict_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_ict_t)={
	{
		{
			{
				BELLE_SIP_VPTR_INIT(belle_sip_ict_t,belle_sip_client_transaction_t,TRUE),
				(belle_sip_object_destroy_t)ict_destroy,
				NULL,
				NULL
			},
			(void (*)(belle_sip_transaction_t*))on_ict_terminate
		},
		(void (*)(belle_sip_client_transaction_t*))ict_send_request,
		(void (*)(belle_sip_client_transaction_t*,belle_sip_response_t*))ict_on_response
	}
};


belle_sip_ict_t *belle_sip_ict_new(belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_ict_t *obj=belle_sip_object_new(belle_sip_ict_t);
	belle_sip_client_transaction_init((belle_sip_client_transaction_t*)obj,prov,req);
	return obj;
}

