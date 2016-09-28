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


#ifndef belle_sip_provider_h
#define belle_sip_provider_h

#define BELLE_SIP_BRANCH_MAGIC_COOKIE "z9hG4bK"

BELLE_SIP_BEGIN_DECLS

BELLESIP_EXPORT belle_sip_uri_t *belle_sip_provider_create_inbound_record_route(belle_sip_provider_t *p, belle_sip_request_t *req);

BELLESIP_EXPORT int belle_sip_provider_is_us(belle_sip_provider_t *p, belle_sip_uri_t*);

BELLESIP_EXPORT int belle_sip_provider_add_listening_point(belle_sip_provider_t *p, belle_sip_listening_point_t *lp);

BELLESIP_EXPORT void belle_sip_provider_remove_listening_point(belle_sip_provider_t *p, belle_sip_listening_point_t *lp);

BELLESIP_EXPORT belle_sip_listening_point_t *belle_sip_provider_get_listening_point(belle_sip_provider_t *p, const char *transport);

BELLESIP_EXPORT const belle_sip_list_t *belle_sip_provider_get_listening_points(belle_sip_provider_t *p);

BELLESIP_EXPORT void belle_sip_provider_add_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l);

BELLESIP_EXPORT void belle_sip_provider_remove_sip_listener(belle_sip_provider_t *p, belle_sip_listener_t *l);

BELLESIP_EXPORT belle_sip_header_call_id_t * belle_sip_provider_create_call_id(const belle_sip_provider_t *prov);

BELLESIP_EXPORT belle_sip_dialog_t *belle_sip_provider_create_dialog(belle_sip_provider_t *prov, belle_sip_transaction_t *t);

BELLESIP_EXPORT belle_sip_client_transaction_t *belle_sip_provider_create_client_transaction(belle_sip_provider_t *p, belle_sip_request_t *req);

BELLESIP_EXPORT belle_sip_server_transaction_t *belle_sip_provider_create_server_transaction(belle_sip_provider_t *p, belle_sip_request_t *req);

BELLESIP_EXPORT belle_sip_stack_t *belle_sip_provider_get_sip_stack(belle_sip_provider_t *p);

BELLESIP_EXPORT void belle_sip_provider_send_request(belle_sip_provider_t *p, belle_sip_request_t *req);

BELLESIP_EXPORT void belle_sip_provider_send_response(belle_sip_provider_t *p, belle_sip_response_t *resp);

BELLESIP_EXPORT void belle_sip_provider_clean_channels(belle_sip_provider_t *p);

/**
 * Add auth info to the request if found
 * @param p object
 * @param request to be updated
 * @param resp response to take authentication values from, might be NULL
 * @param from_uri optional - an uri to use instead of the from of the request, which can be anonymous.
 * @param auth_infos optional - A newly allocated belle_sip_auth_info_t object is added to this list. These object contains useful information like realm and username.
 * @param realm optional - If an outbound proxy realm is used, nounce can be reused from previous request to avoid re-authentication.
 * @returns 0 in case of success,
 *
 **/
BELLESIP_EXPORT int belle_sip_provider_add_authorization(belle_sip_provider_t *p, belle_sip_request_t* request,belle_sip_response_t *resp, belle_sip_uri_t *from_uri, belle_sip_list_t** auth_infos, const char* realm);

/**
 * Can be used to simulate network recv error, for tests.
 * @param prov
 * @param recv_error if <=0, will cause channel error to be reported
**/
BELLESIP_EXPORT void belle_sip_provider_set_recv_error(belle_sip_provider_t *prov, int recv_error);

/**
 * Can be used to unconditionally answer to incoming sip messages. By  default 480 is answered.
 * Can be enhanced by a new method belle_sip_provider_set_unconditional_answer to allows user to provide answer code
 * @param prov
 * @param enable 0 to disable
**/
BELLESIP_EXPORT void belle_sip_provider_enable_unconditional_answer(belle_sip_provider_t *prov, int enable);

/**
 * Can be used to choose unconditionally answer to incoming sip messages.
 * use belle_sip_provider_enable_unconditional_answer to enable/disable
 * @param prov
 * @param code 0 to sip response code
 **/
BELLESIP_EXPORT void belle_sip_provider_set_unconditional_answer(belle_sip_provider_t *prov, unsigned short code);

/**
 * Provides access to a specific dialog
 * @param prov object
 * @param call_if of the dialog
 * @param local_tag of the dialog
 * @param remote_tag of the dialog
 * @returns dialog corresponding to this parameter or NULL if not found
 *
 **/
BELLESIP_EXPORT belle_sip_dialog_t* belle_sip_provider_find_dialog(const belle_sip_provider_t *prov, const char* call_id,const char* local_tag,const char* remote_tag);

/**
 * Enable rport in via header. Enabled by default
 * @param prov
 * @return enable 0 to disable
**/
BELLESIP_EXPORT void belle_sip_provider_enable_rport(belle_sip_provider_t *prov, int enable);
/**
 * get Enable rport in via header. Enabled by default
 * @param prov
 * @param enable 0 to disable
**/
BELLESIP_EXPORT int belle_sip_provider_is_rport_enabled(belle_sip_provider_t *prov);


/**
 * Enable discovery of NAT's public address and port during SIP exchanges.
 * When activated, automatic contacts ( see belle_sip_header_contact_set_automatic() )
 * will use discovered public IP address and port (if any) instead of local ones.
 * NAT public address and port are discovered using received and rport parameters in via header of responses.
 * As a result, disabling rport ( see  belle_sip_provider_enable_rport() ) will also break this feature.
**/
BELLESIP_EXPORT void belle_sip_provider_enable_nat_helper(belle_sip_provider_t *prov, int enabled);

/**
 * Returns if nat helper behavior is enabled.
 * @see belle_sip_provider_enable_nat_helper()
**/
BELLESIP_EXPORT int belle_sip_provider_nat_helper_enabled(const belle_sip_provider_t *prov);

BELLE_SIP_END_DECLS

#define BELLE_SIP_PROVIDER(obj) BELLE_SIP_CAST(obj,belle_sip_provider_t)

#endif
