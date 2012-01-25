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


#ifndef belle_sip_provider_h
#define belle_sip_provider_h


BELLE_SIP_BEGIN_DECLS

int belle_sip_provider_add_listening_point(belle_sip_provider_t *p, belle_sip_listening_point_t *lp);

void belle_sip_provider_remove_listening_point(belle_sip_provider_t *p, belle_sip_listening_point_t *lp);

belle_sip_listening_point_t *belle_sip_provider_get_listening_point(belle_sip_provider_t *p, const char *transport);

const belle_sip_list_t *belle_sip_provider_get_listening_points(belle_sip_provider_t *p);

void belle_sip_provider_add_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l);

void belle_sip_provider_remove_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l);

belle_sip_header_call_id_t * belle_sip_provider_create_call_id(belle_sip_provider_t *prov);

belle_sip_client_transaction_t *belle_sip_provider_create_client_transaction(belle_sip_provider_t *p, belle_sip_request_t *req);

belle_sip_server_transaction_t *belle_sip_provider_create_server_transaction(belle_sip_provider_t *p, belle_sip_request_t *req);

belle_sip_stack_t *belle_sip_provider_get_sip_stack(belle_sip_provider_t *p);

void belle_sip_provider_send_request(belle_sip_provider_t *p, belle_sip_request_t *req);

void belle_sip_provider_send_response(belle_sip_provider_t *p, belle_sip_response_t *resp);

BELLE_SIP_END_DECLS

#define BELLE_SIP_PROVIDER(obj) BELLE_SIP_CAST(obj,belle_sip_provider_t)

#endif
