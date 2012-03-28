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
 * INVITE server transaction implementation.
**/

#include "belle_sip_internal.h"

static void ist_destroy(belle_sip_ist_t *obj){
}


static void ist_on_terminate(belle_sip_ist_t *obj){
//	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
}

static int ist_send_new_response(belle_sip_ist_t *obj, belle_sip_response_t *resp){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	int code=belle_sip_response_get_status_code(resp);
	int ret=0;
	switch(base->state){
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			if (code==100)
				belle_sip_channel_queue_message(base->channel,(belle_sip_message_t*)resp);
		break;
		default:
		break;
	}
	return ret;
}

static void ist_on_request_retransmission(belle_sip_nist_t *obj){
}


BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_ist_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_ist_t)={
	{
		{
			{
				BELLE_SIP_VPTR_INIT(belle_sip_ist_t,belle_sip_server_transaction_t,TRUE),
				(belle_sip_object_destroy_t)ist_destroy,
				NULL,
				NULL
			},
			(void (*)(belle_sip_transaction_t *))ist_on_terminate
		},
		(int (*)(belle_sip_server_transaction_t*, belle_sip_response_t *))ist_send_new_response,
		(void (*)(belle_sip_server_transaction_t*))ist_on_request_retransmission,
	}
};


belle_sip_ist_t *belle_sip_ist_new(belle_sip_provider_t *prov, belle_sip_request_t *req){
	belle_sip_ist_t *obj=belle_sip_object_new(belle_sip_ist_t);
	belle_sip_server_transaction_init((belle_sip_server_transaction_t*)obj,prov,req);
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	belle_sip_response_t *resp;
	
	base->state=BELLE_SIP_TRANSACTION_PROCEEDING;
	resp=belle_sip_response_create_from_request(req,100);
	belle_sip_server_transaction_send_response((belle_sip_server_transaction_t*)obj,resp);
	belle_sip_object_unref(resp);
	return obj;
}
