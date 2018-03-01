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
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "belle_sip_internal.h"

#if TARGET_OS_IPHONE

#include <UIKit/UIApplication.h>

unsigned long belle_sip_begin_background_task(const char *name, belle_sip_background_task_end_callback_t cb, void *data){
	UIApplication *app=[UIApplication sharedApplication];
	UIBackgroundTaskIdentifier bgid = UIBackgroundTaskInvalid;

	if (cb==NULL){
		belle_sip_error("belle_sip_begin_background_task(): the callback must not be NULL. Application must be aware that the background task needs to be terminated.");
		return UIBackgroundTaskInvalid;
	}

	void (^handler)() = ^{
		cb(data);
	};

	if([app respondsToSelector:@selector(beginBackgroundTaskWithName:expirationHandler:)]){
		bgid = [app beginBackgroundTaskWithName:[NSString stringWithUTF8String:name] expirationHandler:handler];
	} else {
		bgid = [app beginBackgroundTaskWithExpirationHandler:handler];
	}

	if (bgid==UIBackgroundTaskInvalid){
		belle_sip_error("Could not start background task %s.", name);
		return 0;
	}

	// backgroundTimeRemaining is properly set only when running background... but not immediately!
	if (app.applicationState != UIApplicationStateBackground || (app.backgroundTimeRemaining == DBL_MAX)) {
		belle_sip_message("Background task %s started. Unknown remaining time since application is not fully in background.", name);
	} else {
		belle_sip_message("Background task %s started. Remaining time %.1f secs", name, app.backgroundTimeRemaining);
	}
	return (unsigned long)bgid;
}

void belle_sip_end_background_task(unsigned long id){
	UIApplication *app=[UIApplication sharedApplication];
	if (id != UIBackgroundTaskInvalid){
		[app endBackgroundTask:(UIBackgroundTaskIdentifier)id];
	}
}

#else
/*mac*/
@import Foundation;

static unsigned long dummy_id=0;
static id activity_id=0;
static int activity_refcnt=0;



unsigned long belle_sip_begin_background_task(const char *name, belle_sip_background_task_end_callback_t cb, void *data){
	activity_refcnt++;
	if (activity_refcnt==1){
		NSProcessInfo *pinfo=[NSProcessInfo processInfo];
		if (pinfo && [pinfo respondsToSelector:@selector(beginActivityWithOptions:reason:)]){
			activity_id=[pinfo beginActivityWithOptions:NSActivityUserInitiatedAllowingIdleSystemSleep reason:@"Processing SIP signaling"];
			[activity_id retain];
			belle_sip_message("Activity started");
		}
	}
	return ++dummy_id;
}

void belle_sip_end_background_task(unsigned long id){
	activity_refcnt--;
	if (activity_refcnt==0){
		if (activity_id!=0){
			NSProcessInfo *pinfo=[NSProcessInfo processInfo];
			[pinfo endActivity:activity_id];
			[activity_id release];
			belle_sip_message("Activity ended");
			activity_id=0;
		}
	}
}

#endif

