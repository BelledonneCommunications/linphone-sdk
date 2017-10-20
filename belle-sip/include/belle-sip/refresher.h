/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2012  Belledonne Communications SARL, Grenoble, France

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

#ifndef REFRESHER_HELPER_H_
#define REFRESHER_HELPER_H_

#define BELLE_SIP_REFRESHER_REUSE_EXPIRES -1

BELLE_SIP_BEGIN_DECLS

typedef struct belle_sip_refresher belle_sip_refresher_t;
/**
 * Refresher listener invoked every time a refresh action is performed
 * @param refresher corresponding refresher object.
 * @param user_pointer user pointer
 * @param status_code status code for the last refresh action
 * @param reason_phrase
 * @param will_retry a boolean indicating wether the refresher is going to retry the request automatically.
 * */
typedef void (*belle_sip_refresher_listener_t) (belle_sip_refresher_t* refresher
								,void* user_pointer
								,unsigned int status_code
								,const char* reason_phrase, int will_retry);

/**
 * add a refresher listener
 */
BELLESIP_EXPORT void belle_sip_refresher_set_listener(belle_sip_refresher_t* refresher, belle_sip_refresher_listener_t listener,void* user_pointer);

/**
 * start the refresher
 */
int belle_sip_refresher_start(belle_sip_refresher_t* refresher);
/**
 * stop refresher.
 * If a transaction is pending, it will be terminated.
 */
BELLESIP_EXPORT void belle_sip_refresher_stop(belle_sip_refresher_t* refresher);

/**
 * Manually initiate a new transaction .
 * @param refresher object
 * @param expires #BELLE_SIP_REFRESHER_REUSE_EXPIRES means value extracted from the transaction
 * @return 0 if succeed
 */
BELLESIP_EXPORT int belle_sip_refresher_refresh(belle_sip_refresher_t* refresher,int expires);
/**
 * returns current expires value;
 */
BELLESIP_EXPORT int belle_sip_refresher_get_expires(const belle_sip_refresher_t* refresher);

/**
 * returns  delay in ms after which the refresher will retry in case of recoverable error (I.E 408, 480, 503, 504, io error);
 */
BELLESIP_EXPORT int belle_sip_refresher_get_retry_after(const belle_sip_refresher_t* refresher);

/**
 * Delay in ms after which the refresher will retry in case of recoverable error (I.E 408, 480, 503, 504, io error);
 */
BELLESIP_EXPORT void belle_sip_refresher_set_retry_after(belle_sip_refresher_t* refresher, int delay_ms);

/**
 * returns  realm of the outbound proxy used for authentication, if any
 */
BELLESIP_EXPORT const char* belle_sip_refresher_get_realm(const belle_sip_refresher_t* refresher);

/**
 * Realm of the outbound proxy used for authentication, if any
 */
BELLESIP_EXPORT void belle_sip_refresher_set_realm(belle_sip_refresher_t* refresher, const char* realm);

/**
 * get current client transaction
 * @param refresher object
 * @return transaction
 */
BELLESIP_EXPORT const belle_sip_client_transaction_t* belle_sip_refresher_get_transaction(const belle_sip_refresher_t* refresher);

/**
 * Get current list of auth info if any. Contains the list of filled #belle_sip_auth_event_t in case of a 401 or 407 is repported to the #belle_sip_refresher_listener_t  ;
 * @param refresher object
 * @return list of #belle_sip_auth_info_t
 */
BELLESIP_EXPORT const belle_sip_list_t* belle_sip_refresher_get_auth_events(const belle_sip_refresher_t* refresher);

/**
 * Enable manual mode: only belle_sip_refresher_refresh() called by application will cause requests to be resubmitted.
**/
BELLESIP_EXPORT void belle_sip_refresher_enable_manual_mode(belle_sip_refresher_t *refresher, int enabled);

/**
 * Retrieve current local address used by the underlying refresher's channel.
**/
BELLESIP_EXPORT const char * belle_sip_refresher_get_local_address(belle_sip_refresher_t* refresher, int *port);

/**
 * Retrieve current public address used by the underlying refresher's channel.
**/
BELLESIP_EXPORT const char * belle_sip_refresher_get_public_address(belle_sip_refresher_t* refresher, int *port);
/**
 * Retrieve last know contact header if known. Only available after a successful registration.
**/
BELLESIP_EXPORT belle_sip_header_contact_t* belle_sip_refresher_get_contact(const belle_sip_refresher_t* refresher);

BELLE_SIP_END_DECLS

#endif /* REFRESHER_HELPER_H_ */
