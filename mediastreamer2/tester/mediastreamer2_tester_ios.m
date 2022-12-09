/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TargetConditionals.h"
#if TARGET_OS_IPHONE

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CFRunLoop.h>
#include "mediastreamer2_tester.h"


int g_argc;
char** g_argv;
void stop_handler(int sig) {
    return;
}

static void* _apple_main(void* data) {
    NSString *bundlePath = [[[NSBundle mainBundle] bundlePath] retain];
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentPath = [[paths objectAtIndex:0] retain];
    NSLog(@"Bundle path: %@", bundlePath);
    NSLog(@"Document path: %@", documentPath);

    bc_tester_set_resource_dir_prefix([bundlePath UTF8String]);
    bc_tester_set_writable_dir_prefix([documentPath UTF8String]);

    apple_main(g_argc,g_argv);

    [bundlePath release];
    [documentPath release];
    return NULL;
}
int main(int argc, char * argv[]) {
    pthread_t main_thread;
    g_argc=argc;
    g_argv=argv;
    pthread_create(&main_thread,NULL,_apple_main,NULL);
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int value = UIApplicationMain(0, nil, nil, nil);
    [pool release];
    return value;
    pthread_join(main_thread,NULL);
    return 0;
}


#endif // target IPHONE
