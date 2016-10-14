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


#ifndef BELLE_SIP_TRANSPORT_H
#define BELLE_SIP_TRANSPORT_H

BELLE_SIP_BEGIN_DECLS

/*This is passed as port information to listening point URIs and means that it shall use a random port.*/
#define BELLE_SIP_LISTENING_POINT_RANDOM_PORT (-1)
/*This is passed as port information to listening point URIs and means that it shall not bind.*/
#define BELLE_SIP_LISTENING_POINT_DONT_BIND (-2)

BELLESIP_EXPORT const char *belle_sip_listening_point_get_ip_address(const belle_sip_listening_point_t *lp);
BELLESIP_EXPORT int belle_sip_listening_point_get_port(const belle_sip_listening_point_t *lp);
BELLESIP_EXPORT const char *belle_sip_listening_point_get_transport(const belle_sip_listening_point_t *lp);
BELLESIP_EXPORT const char *belle_sip_listening_point_get_ip_address(const  belle_sip_listening_point_t *lp);
/*
 * set keep alive frequency in ms
 * @param lp object
 * @param ms keep alive period in ms. Values <=0 disable keep alive
 * */
BELLESIP_EXPORT void belle_sip_listening_point_set_keep_alive(belle_sip_listening_point_t *lp,int ms);

/*
 * get keep alive frequency in ms
 * @param lp object
 * @return  keep alive period in ms. Values <=0 disable keep alive
 * */
BELLESIP_EXPORT int belle_sip_listening_point_get_keep_alive(const belle_sip_listening_point_t *lp);



/**
 * get the listening information as an URI
 * @return IP/port/transport as an URI
 */
BELLESIP_EXPORT const belle_sip_uri_t* belle_sip_listening_point_get_uri(const  belle_sip_listening_point_t *ip);
BELLESIP_EXPORT int belle_sip_listening_point_is_reliable(const belle_sip_listening_point_t *lp);
/**
 * Clean (close) all channels (connection) managed by this listening point.
**/
BELLESIP_EXPORT void belle_sip_listening_point_clean_channels(belle_sip_listening_point_t *lp);

/**
 * Get the number of channels managed by this listening point.
**/
BELLESIP_EXPORT int belle_sip_listening_point_get_channel_count(const belle_sip_listening_point_t *lp);
BELLESIP_EXPORT int belle_sip_listening_point_get_well_known_port(const char *transport);

BELLESIP_DEPRECATED BELLESIP_EXPORT int belle_sip_tls_listening_point_set_root_ca(belle_sip_tls_listening_point_t *s, const char *path);

#define BELLE_SIP_TLS_LISTENING_POINT_BADCERT_CN_MISMATCH 	BELLE_TLS_VERIFY_CN_MISMATCH
#define BELLE_SIP_TLS_LISTENING_POINT_BADCERT_ANY_REASON 	BELLE_TLS_VERIFY_ANY_REASON
BELLESIP_DEPRECATED BELLESIP_EXPORT int belle_sip_tls_listening_point_set_verify_exceptions(belle_sip_tls_listening_point_t *s, int flags);
BELLESIP_DEPRECATED BELLESIP_EXPORT int belle_sip_tls_listening_point_set_verify_policy(belle_sip_tls_listening_point_t *s, belle_tls_verify_policy_t *pol);
BELLESIP_EXPORT int belle_sip_tls_listening_point_set_crypto_config(belle_sip_tls_listening_point_t *s, belle_tls_crypto_config_t *crypto_config);
BELLESIP_EXPORT belle_tls_crypto_config_t *belle_sip_tls_listening_point_get_crypto_config(belle_sip_tls_listening_point_t *s);
BELLESIP_EXPORT belle_sip_listening_point_t * belle_sip_tunnel_listening_point_new(belle_sip_stack_t *s, void *tunnelclient);


#define BELLE_SIP_UDP_LISTENING_POINT(obj) BELLE_SIP_CAST(obj,belle_sip_udp_listening_point_t)
#define BELLE_SIP_STREAM_LISTENING_POINT(obj) BELLE_SIP_CAST(obj,belle_sip_stream_listening_point_t)
#define BELLE_SIP_TLS_LISTENING_POINT(obj) BELLE_SIP_CAST(obj,belle_sip_tls_listening_point_t)

BELLE_SIP_END_DECLS


#endif

