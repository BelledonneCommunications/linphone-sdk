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

const char *belle_sip_listening_point_get_ip_address(const belle_sip_listening_point_t *lp);
int belle_sip_listening_point_get_port(const belle_sip_listening_point_t *lp);
const char *belle_sip_listening_point_get_transport(const belle_sip_listening_point_t *ip);
const char *belle_sip_listening_point_get_ip_address(const  belle_sip_listening_point_t *ip);
int belle_sip_listening_point_is_reliable(const belle_sip_listening_point_t *lp);


BELLE_SIP_END_DECLS

#endif

