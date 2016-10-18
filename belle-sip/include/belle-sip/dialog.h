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

BELLESIP_EXPORT const char* belle_sip_dialog_state_to_string(const belle_sip_dialog_state_t state);

BELLESIP_EXPORT belle_sip_request_t *belle_sip_dialog_create_ack(belle_sip_dialog_t *dialog, unsigned int cseq);


/**
 * Create a request part of this dialog.
**/
BELLESIP_EXPORT belle_sip_request_t *belle_sip_dialog_create_request(belle_sip_dialog_t *dialog, const char *method);
/**
 * Create a request within a dialog keeping non system header from an initial request. This function is very useful to resend request after expiration or chalenge.
 * @param obj dialog associated to the request
 * @param initial_req, all headers + body are re-used from this request except: Via,From, To, Allows, CSeq, Call-ID, Max-Forwards
 *
 */
BELLESIP_EXPORT belle_sip_request_t * belle_sip_dialog_create_request_from(belle_sip_dialog_t *obj, const belle_sip_request_t *initial_req);

/**
 * Create a new request part of this dialog. If dialog is busy (pending transaction), the request can be created anyway and will be sent by the transaction
 * when the dialog becomes available.
**/
BELLESIP_EXPORT belle_sip_request_t * belle_sip_dialog_create_queued_request(belle_sip_dialog_t *obj, const char *method);

/**
 * Create a new request part of this dialog keeping non system header from an initial request. If dialog is busy (pending transaction), the request can be created anyway and will be sent by the transaction
 * when the dialog becomes available.
 * @param obj dialog associated to the request
 * @param initial_req, all headers + body are re-used from this request except: Via,From, To, Allows, CSeq, Call-ID, Max-Forwards
**/
BELLESIP_EXPORT belle_sip_request_t *belle_sip_dialog_create_queued_request_from(belle_sip_dialog_t *obj, const belle_sip_request_t *initial_req);

BELLESIP_EXPORT void belle_sip_dialog_delete(belle_sip_dialog_t *dialog);

BELLESIP_EXPORT void *belle_sip_dialog_get_application_data(const belle_sip_dialog_t *dialog);

BELLESIP_EXPORT void belle_sip_dialog_set_application_data(belle_sip_dialog_t *dialog, void *data);

BELLESIP_EXPORT const belle_sip_header_call_id_t *belle_sip_dialog_get_call_id(const belle_sip_dialog_t *dialog);

BELLESIP_EXPORT const belle_sip_header_address_t *belle_sip_dialog_get_local_party(const belle_sip_dialog_t *dialog);

BELLESIP_EXPORT const belle_sip_header_address_t *belle_sip_dialog_get_remote_party(const belle_sip_dialog_t *dialog);
/**
 * get the value of the last cseq used to issue a request
 * @return local cseq
 **/
BELLESIP_EXPORT unsigned int belle_sip_dialog_get_local_seq_number(const belle_sip_dialog_t *dialog);

unsigned int belle_sip_dialog_get_remote_seq_number(const belle_sip_dialog_t *dialog);

BELLESIP_EXPORT const char *belle_sip_dialog_get_local_tag(const belle_sip_dialog_t *dialog);

BELLESIP_EXPORT const char *belle_sip_dialog_get_remote_tag(const belle_sip_dialog_t *dialog);

BELLESIP_EXPORT const belle_sip_header_address_t *belle_sip_dialog_get_remote_target(belle_sip_dialog_t *dialog);

BELLESIP_EXPORT const belle_sip_list_t* belle_sip_dialog_get_route_set(belle_sip_dialog_t *dialog);

BELLESIP_EXPORT belle_sip_dialog_state_t belle_sip_dialog_get_state(const belle_sip_dialog_t *dialog);
/**
 * return the dialog state before last transition. Can be useful to detect early avorted dialogs
 * @param dialog
 * @returns state
 **/
BELLESIP_EXPORT belle_sip_dialog_state_t belle_sip_dialog_get_previous_state(const belle_sip_dialog_t *dialog);


BELLESIP_EXPORT int belle_sip_dialog_is_server(const belle_sip_dialog_t *dialog);

BELLESIP_EXPORT int belle_sip_dialog_is_secure(const belle_sip_dialog_t *dialog);

BELLESIP_EXPORT void belle_sip_dialog_send_ack(belle_sip_dialog_t *dialog, belle_sip_request_t *request);

BELLESIP_EXPORT void belle_sip_dialog_terminate_on_bye(belle_sip_dialog_t *dialog, int val);
/**
 * Give access to the last transaction processed by a dialog. Can be useful to get reason code for dialog terminated before reaching established state
 * @param dialog
 * @return last transaction
 */
BELLESIP_EXPORT belle_sip_transaction_t* belle_sip_dialog_get_last_transaction(const belle_sip_dialog_t *dialog);

BELLESIP_EXPORT int belle_sip_dialog_request_pending(const belle_sip_dialog_t *dialog);

/*for debugging purpose only, allow to disable checking for pending transaction*/
BELLESIP_EXPORT int belle_sip_dialog_pending_trans_checking_enabled( const belle_sip_dialog_t *dialog) ;
BELLESIP_EXPORT int belle_sip_dialog_enable_pending_trans_checking(belle_sip_dialog_t *dialog, int value) ;

BELLESIP_EXPORT int belle_sip_dialog_expired(const belle_sip_dialog_t *dialog);

BELLE_SIP_END_DECLS

#endif

