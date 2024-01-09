/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE

#include <memory>
#include <UIKit/UIApplication.h>
#include "ios_utils_app.hh"
#include "bctoolbox/logging.h"

namespace bctoolbox {

unsigned long IOSUtilsApp::beginBackgroundTask(const char *name, std::function<void()> cb) {
    __block UIBackgroundTaskIdentifier bgid = UIBackgroundTaskInvalid;

	UIApplication *app=[UIApplication sharedApplication];
	
	@try {
		if (cb==nullptr){
			bctbx_error("belle_sip_begin_background_task(): the callback must not be NULL. Application must be aware that the background task needs to be terminated.");
			bgid = UIBackgroundTaskInvalid;
			@throw([NSException exceptionWithName:@"LinphoneCoreException" reason:@"Background task has no callback" userInfo:nil]);
		}
		
		void (^handler)() = ^{
			cb();
		};
		
		if([app respondsToSelector:@selector(beginBackgroundTaskWithName:expirationHandler:)]){
			bgid = [app beginBackgroundTaskWithName:[NSString stringWithUTF8String:name] expirationHandler:handler];
		} else {
			bgid = [app beginBackgroundTaskWithExpirationHandler:handler];
		}

		if (bgid==UIBackgroundTaskInvalid){
			bctbx_error("Could not start background task %s.", name);
			bgid = 0;
			@throw([NSException exceptionWithName:@"LinphoneCoreException" reason:@"Could not start background task" userInfo:nil]);
		}
		
		// backgroundTimeRemaining is properly set only when running background... but not immediately!
		// removed app.applicationState check because not thread safe
		if (app.backgroundTimeRemaining == DBL_MAX) {
			bctbx_message("Background task %s started. Unknown remaining time since application is not fully in background.", name);
		} else {
			bctbx_message("Background task %s started. Remaining time %.1f secs", name, app.backgroundTimeRemaining);
		}
	}
	@catch (NSException*) {
		// do nothing
	}

    return (unsigned long)bgid;
}

void IOSUtilsApp::endBackgroundTask(unsigned long id) {
	UIApplication *app=[UIApplication sharedApplication];
	if (id != UIBackgroundTaskInvalid){
		[app endBackgroundTask:(UIBackgroundTaskIdentifier)id];
	}
}

bool IOSUtilsApp::isApplicationStateActive() {
    return ([UIApplication sharedApplication].applicationState == UIApplicationStateActive);
}
    
} //namespace bctoolbox

extern "C" {
    bctoolbox::IOSUtilsInterface *bctbx_create_ios_utils_app() {
        return new bctoolbox::IOSUtilsApp;
    }

    void bctbx_destroy_ios_utils_app(bctoolbox::IOSUtilsInterface* p) {
        delete p;
    }
}

#endif
