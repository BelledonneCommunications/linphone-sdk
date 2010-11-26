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
#ifndef BELLE_SIP_TRANSACTION_H
#define BELLE_SIP_TRANSACTION_H

typedef struct belle_sip_transaction belle_sip_transaction_t;

typedef struct belle_sip_server_transaction belle_sip_server_transaction_t;

typedef struct belle_sip_client_transaction belle_sip_client_transaction_t;

typedef enum belle_sip_transaction_state{
	BELLE_SIP_TRANSACTION_CALLING,
	BELLE_SIP_TRANSACTION_COMPLETED,
	BELLE_SIP_TRANSACTION_CONFIRMED,
	BELLE_SIP_TRANSACTION_PROCEEDING,
	BELLE_SIP_TRANSACTION_TRYING,
	BELLE_SIP_TRANSACTION_TERMINATED
}belle_sip_transaction_state_t;


void *belle_sip_transaction_get_application_data(const belle_sip_transaction_t *t);
void belle_sip_transaction_set_application_data(belle_sip_transaction_t *t, void *data);
const char *belle_sip_transaction_get_branch_id(const belle_sip_transaction_t *t);
belle_sip_transaction_state_t belle_sip_transaction_get_state(const belle_sip_transaction_t *t);
void belle_sip_transaction_terminate(belle_sip_transaction_t *t);
belle_sip_request_t *belle_sip_transaction_get_request(belle_sip_transaction_t *t);

void belle_sip_server_transaction_send_response(belle_sip_server_transaction_t *t);

belle_sip_request_t * belle_sip_client_transaction_create_cancel(belle_sip_client_transaction_t *t);
void belle_sip_client_transaction_send_request(belle_sip_client_transaction_t *t);

BELLE_SIP_DECLARE_CAST(belle_sip_transaction_t);
BELLE_SIP_DECLARE_CAST(belle_sip_client_transaction_t);
BELLE_SIP_DECLARE_CAST(belle_sip_server_transaction_t);

#define BELLE_SIP_TRANSACTION(t) BELLE_SIP_CAST(t,belle_sip_transaction_t)
#define BELLE_SIP_SERVER_TRANSACTION(t) BELLE_SIP_CAST(t,belle_sip_server_transaction_t)
#define BELLE_SIP_CLIENT_TRANSACTION(t) BELLE_SIP_CAST(t,belle_sip_client_transaction_t)


/*to be moved out when belle_sip_provider_t is implemented:*/

belle_sip_client_transaction_t * belle_sip_client_transaction_new(belle_sip_request_t *req);

belle_sip_server_transaction_t * belle_sip_server_transaction_new(belle_sip_request_t *req);

#endif

