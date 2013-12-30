/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010-2013  Belledonne Communications SARL

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tunnel/client.hh>

using namespace belledonnecomm;

extern "C" void * tunnel_client_create_socket(void *tunnelclient, int minLocalPort, int maxLocalPort) {
	TunnelClient *tc = static_cast<TunnelClient *>(tunnelclient);
	return tc->createSocket(minLocalPort, maxLocalPort);
}

extern "C" void tunnel_client_close_socket(void *tunnelclient, void *tunnelsocket) {
	TunnelClient *tc = static_cast<TunnelClient *>(tunnelclient);
	TunnelSocket *ts = static_cast<TunnelSocket *>(tunnelsocket);
	tc->closeSocket(ts);
}

extern "C" int tunnel_socket_has_data(void *tunnelsocket) {
	TunnelSocket *ts = static_cast<TunnelSocket *>(tunnelsocket);
	return ts->hasData();
}

extern "C" int tunnel_socket_sendto(void *tunnelsocket, const void *buffer, size_t bufsize, const struct sockaddr *dest, socklen_t socklen) {
	TunnelSocket *ts = static_cast<TunnelSocket *>(tunnelsocket);
	return ts->sendto(buffer, bufsize, dest, socklen);
}

extern "C" int tunnel_socket_recvfrom(void *tunnelsocket, void *buffer, size_t bufsize, struct sockaddr *src, socklen_t socklen) {
	TunnelSocket *ts = static_cast<TunnelSocket *>(tunnelsocket);
	return ts->recvfrom(buffer, bufsize, src, socklen);
}
