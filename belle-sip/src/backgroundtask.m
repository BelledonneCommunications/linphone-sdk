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

#include "belle_sip_internal.h"

#include <UIKit/UIApplication.h>

unsigned int belle_sip_begin_background_task(const char *name, belle_sip_background_task_end_callback_t cb, void *data){
	UIApplication *app=[UIApplication sharedApplication];
	UIBackgroundTaskIdentifier bgid=[app beginBackgroundTaskWithExpirationHandler:^{
		cb(data);
	}];
	if (bgid==UIBackgroundTaskInvalid){
		belle_sip_error("Could not start background task.");
		return 0;
	}
	return (unsigned int)bgid;
}

void belle_sip_end_background_task(unsigned int id){
	UIApplication *app=[UIApplication sharedApplication];
	[app endBackgroundTask:(UIBackgroundTaskIdentifier)id];
}

