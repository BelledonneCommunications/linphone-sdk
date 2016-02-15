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


#ifndef belle_sip_http_provider_h
#define belle_sip_http_provider_h


BELLE_SIP_BEGIN_DECLS

#define BELLE_SIP_HTTP_PROVIDER(obj)	BELLE_SIP_CAST(obj,belle_http_provider_t)
/**
 * Set the certificate verify policy for the TLS connection
 * @return 0 on succes
 * @deprecated Use belle_http_provider_set_tls_crypto_config() instead
 */
BELLESIP_DEPRECATED BELLESIP_EXPORT int belle_http_provider_set_tls_verify_policy(belle_http_provider_t *obj, belle_tls_verify_policy_t *verify_ctx);

/**
 * Set the certificate crypto configuration used by this TLS connection
 * @return 0 on succes
 */
BELLESIP_EXPORT int belle_http_provider_set_tls_crypto_config(belle_http_provider_t *obj, belle_tls_crypto_config_t *crypto_config);

/**
 * Can be used to simulate network recv error, for tests.
 * @param obj
 * @param recv_error if <=0, will cause channel error to be reported
**/
BELLESIP_EXPORT void belle_http_provider_set_recv_error(belle_http_provider_t *obj, int recv_error);

BELLESIP_EXPORT int belle_http_provider_send_request(belle_http_provider_t *obj, belle_http_request_t *req, belle_http_request_listener_t *listener);

BELLESIP_EXPORT void belle_http_provider_cancel_request(belle_http_provider_t *obj, belle_http_request_t *req);

BELLESIP_EXPORT belle_sip_list_t** belle_http_provider_get_channels(belle_http_provider_t *obj, const char *transport_name);

BELLE_SIP_END_DECLS

#endif
