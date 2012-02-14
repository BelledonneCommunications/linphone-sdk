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

#ifndef AUTHENTICATION_HELPER_H_
#define AUTHENTICATION_HELPER_H_
#include "belle-sip/belle-sip.h"

/**
 * Create an authorization header from an www_authenticate header, all common parameters are copyed.
 * copy params: scheme, realm, nonce, algorithm, opaque
 * @param authentication source to be used as input
 * @return belle_sip_header_authorization_t*
 */
belle_sip_header_authorization_t* belle_sip_auth_helper_create_authorization(const belle_sip_header_www_authenticate_t* authentication);

/**
 * Create an proxy_authorization header from an www_authenticate header, all common parameters are copyed.
 * copy params: scheme, realm, nonce, algorithm, opaque
 * @param authentication source to be used as input
 * @return belle_sip_header_authorization_t*
 */
belle_sip_header_proxy_authorization_t* belle_sip_auth_helper_create_proxy_authorization(const belle_sip_header_proxy_authenticate_t* proxy_authentication);

/**
 * compute and set response value according to parameters
 * @return 0 if succeed
 */
int belle_sip_auth_helper_fill_authorization(belle_sip_header_authorization_t* authorization
												,const char* method
												,const char* username
												,const char* password);
/**
 * compute and set response value according to parameters
 * @return 0 if succeed
 */
int belle_sip_auth_helper_fill_proxy_authorization(belle_sip_header_proxy_authorization_t* proxy_authorization
												,const char* method
												,const char* username
												,const char* password);


#endif /* AUTHENTICATION_HELPER_H_ */
