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

#ifndef STREAM_CHANNEL_H_
#define STREAM_CHANNEL_H_
#include "channel.h"

int stream_channel_connect(belle_sip_channel_t *obj, const struct sockaddr *addr, socklen_t socklen);
/*return 0 if succeed*/
int finalize_stream_connection (belle_sip_fd_t fd, struct sockaddr *addr, socklen_t* slen);

#endif /* STREAM_CHANNEL_H_ */
