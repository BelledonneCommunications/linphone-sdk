/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Belledonne Communications

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#if !defined(WIN32) && !defined(_WIN32_WCE)
#ifdef __APPLE__
#include <sys/types.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#endif

#include "mediastreamer2/msticker.h"
#include "mediastreamer2/ice.h"


static void ice_check_list_init(IceCheckList *cl)
{
	cl->state = ICL_Running;
}

IceCheckList * ice_check_list_new(void)
{
	IceCheckList *cl = ms_new0(IceCheckList, 1);
	if (cl == NULL) {
		ms_error("ice_check_list_new: Memory allocation failed");
		return NULL;
	}
	ice_check_list_init(cl);
	return cl;
}

void ice_check_list_destroy(IceCheckList *cl)
{
	ms_free(cl);
}

IceCheckListState ice_check_list_state(IceCheckList *cl)
{
	return cl->state;
}

void ice_handle_stun_packet(IceCheckList *cl, RtpSession* session, mblk_t* m)
{
	//TODO
}
