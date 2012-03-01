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

static void ict_destroy(belle_sip_ict_t *obj){
}

static void ict_on_response(belle_sip_ict_t *obj, belle_sip_response_t *resp){
}

static void ict_send_request(belle_sip_ict_t *obj){
	belle_sip_transaction_t *base=(belle_sip_transaction_t*)obj;
	base->state=BELLE_SIP_TRANSACTION_CALLING;
	
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_ict_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR(belle_sip_ict_t)={
	{
		{
			BELLE_SIP_VPTR_INIT(belle_sip_ict_t,belle_sip_client_transaction_t,FALSE),
			(belle_sip_object_destroy_t)ict_destroy,
			NULL,
			NULL
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

