/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2022  Belledonne Communications SARL

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
#import <UIKit/UIKit.h>

int belle_sip_get_ios_device_major_version() {
	NSArray *versionCompatibility = [[UIDevice currentDevice].systemVersion componentsSeparatedByString:@"."];

	return [[versionCompatibility objectAtIndex:0] intValue];
}
#endif
