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


#ifndef BELLE_SIP_TRANSPORT_H
#define BELLE_SIP_TRANSPORT_H

BELLE_SIP_BEGIN_DECLS

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

BELLE_SIP_END_DECLS


#endif

