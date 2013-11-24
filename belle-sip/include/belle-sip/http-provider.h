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


#ifndef belle_sip_http_provider_h
#define belle_sip_http_provider_h


BELLE_SIP_BEGIN_DECLS

BELLESIP_EXPORT void belle_http_provider_send_request(belle_http_provider_t *obj, belle_http_request_t *req, belle_http_request_listener_t *listener);

BELLE_SIP_END_DECLS

#endif
