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
 * non-INVITE server transaction implementation.
**/

#include "belle_sip_internal.h"

static void nist_on_terminate(belle_sip_nist_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	if (obj->timer_J){
		belle_sip_transaction_stop_timer(base,obj->timer_J);
		belle_sip_object_unref(obj->timer_J);
		obj->timer_J=NULL;
	}
}

static void nist_destroy(belle_sip_nist_t *obj){
	nist_on_terminate(obj);
}

static int nist_on_timer_J(belle_sip_nist_t *obj){
	belle_sip_transaction_terminate((belle_sip_transaction_t *)obj);
	return BELLE_SIP_STOP;
}

static void nist_set_completed(belle_sip_nist_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	const belle_sip_timer_config_t *cfg=belle_sip_transaction_get_timer_config(base);
	int tval;
	if (!belle_sip_channel_is_reliable(base->channel))
		tval=cfg->T1*64;
	else tval=0;
	obj->timer_J=belle_sip_timeout_source_new((belle_sip_source_func_t)nist_on_timer_J,obj,tval);
	belle_sip_transaction_start_timer(base,obj->timer_J);
	belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_COMPLETED);
}

static int nist_send_new_response(belle_sip_nist_t *obj, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	int code=belle_sip_response_get_status_code(resp);
	int ret=0;
	switch(base->state){
		case BELLE_SIP_TRANSACTION_TRYING:
			if (code<200){
				belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_PROCEEDING);
				belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)resp);
				break;
			}
			/* no break nist can directly pass from TRYING to PROCEEDING*/
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			if (code>=200){
				nist_set_completed(obj);
			}
			belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)resp);
		break;
		case BELLE_SIP_TRANSACTION_COMPLETED:
			belle_sip_warning("nist_send_new_response(): not allowed to send a response while transaction is completed.");
			ret=-1; /*not allowed to send a response at this time*/
		break;
		default:
			//ignore
		break;
	}
	return ret;
}

static void nist_on_request_retransmission(belle_sip_nist_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	switch(base->state){
		case BELLE_SIP_TRANSACTION_PROCEEDING:
		case BELLE_SIP_TRANSACTION_COMPLETED:
			belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)base->last_response);
		break;
		default:
			//ignore
		break;
	}
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_nist_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_nist_t)
	{
		{
			{
				BELLE_SIP_VPTR_INIT(belle_sip_nist_t,belle_sip_server_transaction_t,TRUE),
				(belle_sip_object_destroy_t)nist_destroy,
				NULL,
				NULL,
				BELLE_SIP_DEFAULT_BUFSIZE_HINT
			},
			(void (*)(belle_sip_transaction_t *))nist_on_terminate
		},
		(int (*)(belle_sip_server_transaction_t*, belle_sip_response_t *))nist_send_new_response,
		(void (*)(belle_sip_server_transaction_t*))nist_on_request_retransmission,
	}
BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END


belle_sip_nist_t *belle_sip_nist_new(belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_nist_t *obj=belle_sip_object_new(belle_sip_nist_t);
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	belle_sip_server_transaction_init((belle_sip_server_transaction_t*)obj,prov,req);
	belle_sip_transaction_set_state(base,BELLE_SIP_TRANSACTION_TRYING);
	return obj;
}
