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


#ifndef belle_sip_dialog_h
#define belle_sip_dialog_h

enum belle_sip_dialog_state{
	BELLE_SIP_DIALOG_NULL,
	BELLE_SIP_DIALOG_EARLY,
	BELLE_SIP_DIALOG_CONFIRMED,
	BELLE_SIP_DIALOG_TERMINATED
};

typedef enum belle_sip_dialog_state belle_sip_dialog_state_t;

BELLE_SIP_BEGIN_DECLS

belle_sip_request_t *belle_sip_dialog_create_ack(belle_sip_dialog_t *dialog, unsigned int cseq);

belle_sip_request_t *belle_sip_dialog_create_request(belle_sip_dialog_t *dialog, const char *method);

void belle_sip_dialog_delete(belle_sip_dialog_t *dialog);

void *belle_sip_get_application_data(const belle_sip_dialog_t *dialog);

void belle_sip_set_application_data(belle_sip_dialog_t *dialog, void *data);

const char *belle_sip_dialog_get_dialog_id(const belle_sip_dialog_t *dialog);

const belle_sip_header_call_id_t *belle_sip_dialog_get_call_id(const belle_sip_dialog_t *dialog);

const belle_sip_header_address_t *belle_sip_get_local_party(const belle_sip_dialog_t *dialog);

const belle_sip_header_address_t *belle_sip_get_remote_party(const belle_sip_dialog_t *dialog);

unsigned int belle_sip_dialog_get_local_seq_number(const belle_sip_dialog_t *dialog);

unsigned int belle_sip_dialog_get_remote_seq_number(const belle_sip_dialog_t *dialog);

const char *belle_sip_dialog_get_local_tag(const belle_sip_dialog_t *dialog);

const char *belle_sip_dialog_get_remote_tag(const belle_sip_dialog_t *dialog);

const belle_sip_header_address_t *belle_sip_dialog_get_remote_target(belle_sip_dialog_t *dialog);

const belle_sip_list_t* belle_sip_dialog_get_route_set(belle_sip_dialog_t *dialog);

belle_sip_dialog_state_t belle_sip_dialog_get_state(const belle_sip_dialog_t *dialog);

int belle_sip_dialog_is_server(const belle_sip_dialog_t *dialog);

int belle_sip_dialog_is_secure(const belle_sip_dialog_t *dialog);

void belle_sip_dialog_send_ack(belle_sip_dialog_t *dialog, belle_sip_request_t *request);

void belle_sip_dialog_terminate_on_bye(belle_sip_dialog_t *dialog, int val);

BELLE_SIP_END_DECLS

#endif

