/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2012  Belledonne Communications SARL, Grenoble, France

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

#ifndef REFRESHER_HELPER_H_
#define REFRESHER_HELPER_H_
#define BELLE_SIP_REFRESHER_REUSE_EXPIRES -1

typedef struct belle_sip_refresher belle_sip_refresher_t;
/**
 * Refresher listener invoked every time a refresh action is performed
 * @param refresher corresponding refresher object.
 * @param user_pointer user pointer
 * @param status_code status code for the last refresh action
 * @param reason_phrase
 * */
typedef void (*belle_sip_refresher_listener_t) ( const belle_sip_refresher_t* refresher
												,void* user_pointer
												,unsigned int status_code
												,const char* reason_phrase);

/**
 * add a refresher listener
 */
BELLESIP_EXPORT void belle_sip_refresher_set_listener(belle_sip_refresher_t* refresher, belle_sip_refresher_listener_t listener,void* user_pointer);

/**
 * start the refresher
 */
int belle_sip_refresher_start(belle_sip_refresher_t* refresher);
/**
 * stop refresher
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
 * get current client transaction
 * @param refresher object
 * @return transaction
 */
BELLESIP_EXPORT const belle_sip_client_transaction_t* belle_sip_refresher_get_transaction(const belle_sip_refresher_t* refresher);

/**
 * get current list of auth info if any. Contains the list of filled #belle_sip_auth_event_t in case of a 401 or 407 is repported to the #belle_sip_refresher_listener_t  ;
 * @param refresher object
 * @return list of #belle_sip_auth_info_t
 */
BELLESIP_EXPORT const belle_sip_list_t* belle_sip_refresher_get_auth_events(const belle_sip_refresher_t* refresher);

/**
 * get current public address as reported by received/rport in case of NAT.
 * @param refresher object
 * @return nated contact header or NULL if not determined
 */
BELLESIP_EXPORT const belle_sip_header_contact_t* belle_sip_refresher_get_nated_contact(const belle_sip_refresher_t* refresher);
/**
 * Activate contact rewriting based on received/rport
 * @param refresher object
 * @param enable 0 to disable
 *
 * */
BELLESIP_EXPORT void belle_sip_refresher_enable_nat_helper(belle_sip_refresher_t* refresher,int enable);
/**
 * Contact rewriting statebased on received/rport
 * @param refresher object
 * @return  0 to disable
 *
 * */
BELLESIP_EXPORT int belle_sip_refresher_is_nat_helper_enabled(const belle_sip_refresher_t* refresher);


#endif /* REFRESHER_HELPER_H_ */
