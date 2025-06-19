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
#include "bctoolbox/ios_utils.hh"

using namespace bctoolbox;

unsigned long belle_sip_begin_background_task(const char *name, belle_sip_background_task_end_callback_t cb, void *data){
    auto &iOSUtils = IOSUtils::getUtils();

    std::function<void()> callback;
    if (cb) {
        callback = std::bind(cb, data);
    }

    return iOSUtils.beginBackgroundTask(name, callback);
}

void belle_sip_end_background_task(unsigned long id){
    auto &iOSUtils = IOSUtils::getUtils();
    iOSUtils.endBackgroundTask(id);
}

#else
/*mac*/
#import <Foundation/Foundation.h>

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
